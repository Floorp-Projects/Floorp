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
 * see nsDASHDecoder.cpp for comments on DASH object interaction
 */

#if !defined(nsDASHReader_h_)
#define nsDASHReader_h_

#include "nsBuiltinDecoderReader.h"

class nsDASHReader : public nsBuiltinDecoderReader
{
public:
  typedef mozilla::MediaResource MediaResource;

  nsDASHReader(nsBuiltinDecoder* aDecoder) :
    nsBuiltinDecoderReader(aDecoder),
    mReadMetadataMonitor("media.dashreader.readmetadata"),
    mReadyToReadMetadata(false),
    mDecoderIsShuttingDown(false),
    mAudioReader(this),
    mVideoReader(this),
    mAudioReaders(this),
    mVideoReaders(this)
  {
    MOZ_COUNT_CTOR(nsDASHReader);
  }
  ~nsDASHReader()
  {
    MOZ_COUNT_DTOR(nsDASHReader);
  }

  // Adds a pointer to a audio/video reader for a media |Representation|.
  // Called on the main thread only.
  void AddAudioReader(nsBuiltinDecoderReader* aAudioReader);
  void AddVideoReader(nsBuiltinDecoderReader* aVideoReader);

  // Waits for metadata bytes to be downloaded, then reads and parses them.
  // Called on the decode thread only.
  nsresult ReadMetadata(nsVideoInfo* aInfo,
                        nsHTMLMediaElement::MetadataTags** aTags);

  // Waits for |ReadyToReadMetadata| or |NotifyDecoderShuttingDown|
  // notification, whichever comes first. Ensures no attempt to read metadata
  // during |nsDASHDecoder|::|Shutdown|. Called on decode thread only.
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

  // Called on the main thread by |nsDASHDecoder| to notify that metadata bytes
  // have been downloaded.
  void ReadyToReadMetadata() {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
    ReentrantMonitorAutoEnter mon(mReadMetadataMonitor);
    mReadyToReadMetadata = true;
    mon.NotifyAll();
  }

  // Called on the main thread by |nsDASHDecoder| when it starts Shutdown. Will
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
  bool HasAudio() {
    NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
    return mAudioReader ? mAudioReader->HasAudio() : false;
  }
  bool HasVideo() {
    NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
    return mVideoReader ? mVideoReader->HasVideo() : false;
  }

  // Returns references to the audio/video queues of sub-readers. Called on
  // decode, state machine and audio threads.
  MediaQueue<AudioData>& AudioQueue();
  MediaQueue<VideoData>& VideoQueue();

  // Called from nsBuiltinDecoderStateMachine on the main thread.
  nsresult Init(nsBuiltinDecoderReader* aCloneDonor);

  // Used by |MediaMemoryReporter|.
  int64_t VideoQueueMemoryInUse();
  int64_t AudioQueueMemoryInUse();

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

  // Call by state machine on multiple threads.
  bool IsSeekableInBufferedRanges();

private:
  // Similar to |ReentrantMonitorAutoEnter|, this class enters the supplied
  // monitor in its constructor, but only if the conditional value |aEnter| is
  // true. Used here to allow read access on the sub-readers' owning thread,
  // i.e. the decode thread, while locking write accesses from all threads,
  // and read accesses from non-decode threads.
  class ReentrantMonitorConditionallyEnter
  {
  public:
    ReentrantMonitorConditionallyEnter(bool aEnter,
                                       ReentrantMonitor &aReentrantMonitor) :
      mReentrantMonitor(nullptr)
    {
      MOZ_COUNT_CTOR(nsDASHReader::ReentrantMonitorConditionallyEnter);
      if (aEnter) {
        mReentrantMonitor = &aReentrantMonitor;
        NS_ASSERTION(mReentrantMonitor, "null monitor");
        mReentrantMonitor->Enter();
      }
    }
    ~ReentrantMonitorConditionallyEnter(void)
    {
      if (mReentrantMonitor) {
        mReentrantMonitor->Exit();
      }
      MOZ_COUNT_DTOR(nsDASHReader::ReentrantMonitorConditionallyEnter);
    }
  private:
    // Restrict to constructor and destructor defined above.
    ReentrantMonitorConditionallyEnter();
    ReentrantMonitorConditionallyEnter(const ReentrantMonitorConditionallyEnter&);
    ReentrantMonitorConditionallyEnter& operator =(const ReentrantMonitorConditionallyEnter&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    // Ptr to the |ReentrantMonitor| object. Null if |aEnter| in constructor
    // was false.
    ReentrantMonitor* mReentrantMonitor;
  };

  // Monitor and booleans used to wait for metadata bytes to be downloaded, and
  // skip reading metadata if |nsDASHDecoder|'s shutdown is in progress.
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
    // Main constructor takes a pointer to the owning |nsDASHReader| to verify
    // correct entry into the decoder's |ReentrantMonitor|.
    MonitoredSubReader(nsDASHReader* aReader) :
      mReader(aReader),
      mSubReader(nullptr)
    {
      MOZ_COUNT_CTOR(nsDASHReader::MonitoredSubReader);
      NS_ASSERTION(mReader, "Reader is null!");
    }
    // Note: |mSubReader|'s refcount will be decremented in this destructor.
    ~MonitoredSubReader()
    {
      MOZ_COUNT_DTOR(nsDASHReader::MonitoredSubReader);
    }

    // Override '=' to always assert thread is "in monitor" for writes/changes
    // to |mSubReader|.
    MonitoredSubReader& operator=(nsBuiltinDecoderReader* rhs)
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      mSubReader = rhs;
      return *this;
    }

    // Override '*' to assert threads other than the decode thread are "in
    // monitor" for ptr reads.
    operator nsBuiltinDecoderReader*() const
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      if (!mReader->GetDecoder()->OnDecodeThread()) {
        mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      }
      return mSubReader;
    }

    // Override '->' to assert threads other than the decode thread are "in
    // monitor" for |mSubReader| function calls.
    nsBuiltinDecoderReader* operator->() const
    {
      return *this;
    }
  private:
    // Pointer to |nsDASHReader| object which owns this |MonitoredSubReader|.
    nsDASHReader* mReader;
    // Ref ptr to the sub reader.
    nsRefPtr<nsBuiltinDecoderReader> mSubReader;
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
    // Main constructor takes a pointer to the owning |nsDASHReader| to verify
    // correct entry into the decoder's |ReentrantMonitor|.
    MonitoredSubReaderList(nsDASHReader* aReader) :
      mReader(aReader)
    {
      MOZ_COUNT_CTOR(nsDASHReader::MonitoredSubReaderList);
      NS_ASSERTION(mReader, "Reader is null!");
    }
    // Note: Elements in |mSubReaderList| will have their refcounts decremented
    // in this destructor.
    ~MonitoredSubReaderList()
    {
      MOZ_COUNT_DTOR(nsDASHReader::MonitoredSubReaderList);
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
    nsRefPtr<nsBuiltinDecoderReader>& operator[](uint32_t i)
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
    AppendElement(nsBuiltinDecoderReader* aReader)
    {
      NS_ASSERTION(mReader->GetDecoder(), "Decoder is null!");
      mReader->GetDecoder()->GetReentrantMonitor().AssertCurrentThreadIn();
      mSubReaderList.AppendElement(aReader);
    }
  private:
    // Pointer to |nsDASHReader| object which owns this |MonitoredSubReader|.
    nsDASHReader* mReader;
    // Ref ptrs to the sub readers.
    nsTArray<nsRefPtr<nsBuiltinDecoderReader> > mSubReaderList;
  };

  // Ref ptrs to all sub-readers of individual media |Representation|s.
  // Decoder monitor must be entered for write access on all threads and read
  // access on all threads that are not the decode thread. Read acces on the
  // decode thread does not need to be protected.
  MonitoredSubReaderList mAudioReaders;
  MonitoredSubReaderList mVideoReaders;
};


#endif
