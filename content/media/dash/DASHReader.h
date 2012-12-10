/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */

/* This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DASH - Dynamic Adaptive Streaming over HTTP
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * see DASHDecoder.cpp for comments on DASH object interaction
 */

#if !defined(DASHReader_h_)
#define DASHReader_h_

#include "VideoUtils.h"
#include "MediaDecoderReader.h"
#include "DASHRepReader.h"

namespace mozilla {

class DASHRepReader;

class DASHReader : public MediaDecoderReader
{
public:
  DASHReader(AbstractMediaDecoder* aDecoder);
  ~DASHReader();

  // Adds a pointer to a audio/video reader for a media |Representation|.
  // Called on the main thread only.
  void AddAudioReader(DASHRepReader* aAudioReader);
  void AddVideoReader(DASHRepReader* aVideoReader);

  // Waits for metadata bytes to be downloaded, then reads and parses them.
  // Called on the decode thread only.
  nsresult ReadMetadata(VideoInfo* aInfo,
                        MetadataTags** aTags);

  // Waits for |ReadyToReadMetadata| or |NotifyDecoderShuttingDown|
  // notification, whichever comes first. Ensures no attempt to read metadata
  // during |DASHDecoder|::|Shutdown|. Called on decode thread only.
  nsresult WaitForMetadata() {
    NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
    ReentrantMonitorAutoEnter mon(mReadMetadataMonitor);
    while (true) {
      // Abort if the decoder has started shutting down.
      if (mDecoderIsShuttingDown) {
        return NS_ERROR_ABORT;
      } else if (mReadyToReadMetadata) {
        break;
      }
      mon.Wait();
    }
    return NS_OK;
  }

  // Called on the main thread by |DASHDecoder| to notify that metadata bytes
  // have been downloaded.
  void ReadyToReadMetadata() {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
    ReentrantMonitorAutoEnter mon(mReadMetadataMonitor);
    mReadyToReadMetadata = true;
    mon.NotifyAll();
  }

  // Called on the main thread by |DASHDecoder| when it starts Shutdown. Will
  // wake metadata monitor if waiting for a silent return from |ReadMetadata|.
  void NotifyDecoderShuttingDown() {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
    ReentrantMonitorAutoEnter metadataMon(mReadMetadataMonitor);
    mDecoderIsShuttingDown = true;
    // Notify |ReadMetadata| of the shutdown if it's waiting.
    metadataMon.NotifyAll();
  }

  // Audio/video status are dependent on the presence of audio/video readers.
  // Call on decode thread only.
  bool HasAudio();
  bool HasVideo();

  // Returns references to the audio/video queues of sub-readers. Called on
  // decode, state machine and audio threads.
  MediaQueue<AudioData>& AudioQueue() MOZ_OVERRIDE;
  MediaQueue<VideoData>& VideoQueue() MOZ_OVERRIDE;

  // Called from MediaDecoderStateMachine on the main thread.
  nsresult Init(MediaDecoderReader* aCloneDonor);

  // Used by |MediaMemoryReporter|.
  int64_t VideoQueueMemoryInUse();
  int64_t AudioQueueMemoryInUse();

  // Called on the decode thread, at the start of the decode loop, before
  // |DecodeVideoFrame|.  Carries out video reader switch if previously
  // requested, and tells sub-readers to |PrepareToDecode|.
  void PrepareToDecode() MOZ_OVERRIDE;

  // Called on the decode thread.
  bool DecodeVideoFrame(bool &aKeyframeSkip, int64_t aTimeThreshold);
  bool DecodeAudioData();

  // Converts seek time to byte offset. Called on the decode thread only.
  nsresult Seek(int64_t aTime,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime);

  // Called by state machine on multiple threads.
  nsresult GetBuffered(nsTimeRanges* aBuffered, int64_t aStartTime);

  // Called on the state machine or decode threads.
  VideoData* FindStartTime(int64_t& aOutStartTime);

  // Prepares for an upcoming switch of video readers. Called by
  // |DASHDecoder| when it has switched download streams. Sets the index of
  // the reader to switch TO and the index of the subsegment to switch AT
  // (start offset). (Note: Subsegment boundaries are switch access points for
  // DASH-WebM). Called on the main thread. Must be in the decode monitor.
  void RequestVideoReaderSwitch(uint32_t aFromReaderIdx,
                                uint32_t aToReaderIdx,
                                uint32_t aSubsegmentIdx);

private:
  // Switches video subreaders if a stream-switch flag has been set, and the
  // current reader has read up to the switching subsegment (start offset).
  // Called on the decode thread only.
  void PossiblySwitchVideoReaders();

  // Monitor and booleans used to wait for metadata bytes to be downloaded, and
  // skip reading metadata if |DASHDecoder|'s shutdown is in progress.
  ReentrantMonitor mReadMetadataMonitor;
  bool mReadyToReadMetadata;
  bool mDecoderIsShuttingDown;

  // Wrapper class protecting accesses to sub-readers. Asserts that the
  // decoder monitor has been entered for write access on all threads and read
  // access on all threads that are not the decode thread. Read access on the
  // decode thread does not need to be protected.
  class MonitoredSubReader
  {
  public:
    // Main constructor takes a pointer to the owning |DASHReader| to verify
    // correct entry into the decoder's |ReentrantMonitor|.
    MonitoredSubReader(DASHReader* aReader) :
      mReader(aReader),
      mSubReader(nullptr)
    {
      MOZ_COUNT_CTOR(DASHReader::MonitoredSubReader);
      NS_ASSERTION(mReader, "Reader is null!");
    }
    // Note: |mSubReader|'s refcount will be decremented in this destructor.
    ~MonitoredSubReader()
    {
      MOZ_COUNT_DTOR(DASHReader::MonitoredSubReader);
    }

    // Override '=' to always assert thread is "in monitor" for writes/changes
    // to |mSubReader|.
    MonitoredSubReader& operator=(DASHRepReader* rhs)
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      mSubReader = rhs;
      return *this;
    }

    // Override '*' to assert threads other than the decode thread are "in
    // monitor" for ptr reads.
    operator DASHRepReader*() const
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      if (!mReader->GetDecoder()->OnDecodeThread()) {
        mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      }
      return mSubReader;
    }

    // Override '->' to assert threads other than the decode thread are "in
    // monitor" for |mSubReader| function calls.
    DASHRepReader* operator->() const
    {
      return *this;
    }
  private:
    // Pointer to |DASHReader| object which owns this |MonitoredSubReader|.
    DASHReader* mReader;
    // Ref ptr to the sub reader.
    nsRefPtr<DASHRepReader> mSubReader;
  };

  // Wrapped ref ptrs to current sub-readers of individual media
  // |Representation|s. Decoder monitor must be entered for write access on all
  // threads and read access on all threads that are not the decode thread.
  // Read access on the decode thread does not need to be protected.
  // Note: |MonitoredSubReader| class will assert correct monitor use.
  MonitoredSubReader mAudioReader;
  MonitoredSubReader mVideoReader;

  // Wrapper class protecting accesses to sub-reader list. Asserts that the
  // decoder monitor has been entered for write access on all threads and read
  // access on all threads that are not the decode thread. Read access on the
  // decode thread does not need to be protected.
  // Note: Elems accessed via operator[] are not protected with monitor
  // assertion checks once obtained.
  class MonitoredSubReaderList
  {
  public:
    // Main constructor takes a pointer to the owning |DASHReader| to verify
    // correct entry into the decoder's |ReentrantMonitor|.
    MonitoredSubReaderList(DASHReader* aReader) :
      mReader(aReader)
    {
      MOZ_COUNT_CTOR(DASHReader::MonitoredSubReaderList);
      NS_ASSERTION(mReader, "Reader is null!");
    }
    // Note: Elements in |mSubReaderList| will have their refcounts decremented
    // in this destructor.
    ~MonitoredSubReaderList()
    {
      MOZ_COUNT_DTOR(DASHReader::MonitoredSubReaderList);
    }

    // Returns Length of |mSubReaderList| array. Will assert threads other than
    // the decode thread are "in monitor".
    uint32_t Length() const
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      if (!mReader->GetDecoder()->OnDecodeThread()) {
        mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      }
      return mSubReaderList.Length();
    }

    // Override '[]' to assert threads other than the decode thread are "in
    // monitor" for accessing individual elems. Note: elems returned do not
    // have monitor assertions builtin like |MonitoredSubReader| objects.
    nsRefPtr<DASHRepReader>& operator[](uint32_t i)
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      if (!mReader->GetDecoder()->OnDecodeThread()) {
        mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      }
      return mSubReaderList[i];
    }

    // Appends a reader to the end of |mSubReaderList|. Will always assert that
    // the thread is "in monitor".
    void
    AppendElement(DASHRepReader* aReader)
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      mSubReaderList.AppendElement(aReader);
    }
  private:
    // Pointer to |DASHReader| object which owns this |MonitoredSubReader|.
    DASHReader* mReader;
    // Ref ptrs to the sub readers.
    nsTArray<nsRefPtr<DASHRepReader> > mSubReaderList;
  };

  // Ref ptrs to all sub-readers of individual media |Representation|s.
  // Decoder monitor must be entered for write access on all threads and read
  // access on all threads that are not the decode thread. Read acces on the
  // decode thread does not need to be protected.
  MonitoredSubReaderList mAudioReaders;
  MonitoredSubReaderList mVideoReaders;

  // When true, indicates that we should switch reader. Must be in the monitor
  // for write access and read access off the decode thread.
  bool mSwitchVideoReaders;

  // Indicates the subsegment index at which the reader should switch. Must be
  // in the monitor for write access and read access off the decode thread.
  nsTArray<uint32_t> mSwitchToVideoSubsegmentIndexes;

  // Counts the number of switches that have taken place. Must be in the
  // monitor for write access and read access off the decode thread.
  int32_t mSwitchCount;
};

} // namespace mozilla

#endif
