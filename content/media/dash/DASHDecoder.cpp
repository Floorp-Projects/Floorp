/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */

/* This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DASH - Dynamic Adaptive Streaming over HTTP.
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * Interaction with MediaDecoderStateMachine, nsHTMLMediaElement,
 * ChannelMediaResource and sub-decoders (WebMDecoder).
 *
 *
 *        MediaDecoderStateMachine      nsHTMLMediaElement
 *               1 /           \ 1           / 1
 *                /             \           /
 *             1 /               \ 1       / 1
 * DASHReader ------ DASHDecoder ------------ ChannelMediaResource
 *          |1          1      1     |1      \1             (for MPD Manifest)
 *          |                        |        ------------
 *          |*                       |*                   \*
 *     WebMReader ------- DASHRepDecoder ------- ChannelMediaResource
 *                 1       1                      1       1 (for media streams)
 *
 * One decoder and state-machine, as with current, non-DASH decoders.
 *
 * DASH adds multiple readers, decoders and resources, in order to manage
 * download and decode of the MPD manifest and individual media streams.
 *
 * Rep/|Representation| is for an individual media stream, e.g. audio
 * DASHRepDecoder is the decoder for a rep/|Representation|.
 *
 * FLOW
 *
 * 1 - Download and parse the MPD (DASH XML-based manifest).
 *
 * Media element creates new |DASHDecoder| object:
 *   member var initialization to default values, including a/v sub-decoders.
 *   MediaDecoder and MediaDecoder constructors are called.
 *   MediaDecoder::Init() is called.
 *
 * Media element creates new |ChannelMediaResource|:
 *   used to download MPD manifest.
 *
 * Media element calls |DASHDecoder|->Load() to download the MPD file:
 *   creates an |DASHReader| object to forward calls to multiple
 *     WebMReaders (corresponding to MPD |Representation|s i.e. streams).
 *     Note: 1 |DASHReader| per DASH/WebM MPD.
 *
 *   also calls |ChannelMediaResource|::Open().
 *     uses nsHttpChannel to download MPD; notifies DASHDecoder.
 *
 * Meanwhile, back in |DASHDecoder|->Load():
 *   MediaDecoderStateMachine is created.
 *     has ref to |DASHReader| object.
 *     state machine is scheduled.
 *
 * Media element finishes decoder setup:
 *   element added to media URI table etc.
 *
 * -- At this point, objects are waiting on HTTP returning MPD data.
 *
 * MPD Download (Async |ChannelMediaResource| channel callbacks):
 *   calls DASHDecoder::|NotifyDownloadEnded|().
 *     DASHDecoder parses MPD XML to DOM to MPD classes.
 *       gets |Segment| URLs from MPD for audio and video streams.
 *       creates |nsIChannel|s, |ChannelMediaResource|s.
 *         stores resources as member vars (to forward function calls later).
 *       creates |WebMReader|s and |DASHRepDecoder|s.
 *         DASHreader creates |WebMReader|s.
 *         |Representation| decoders are connected to the |ChannelMediaResource|s.
 *
 *     |DASHDecoder|->|LoadRepresentations|() starts download and decode.
 *
 *
 * 2 - Media Stream, Byte Range downloads.
 *
 * -- At this point the Segment media stream downloads are managed by
 *    individual |ChannelMediaResource|s and |WebMReader|s.
 *    A single |DASHDecoder| and |MediaDecoderStateMachine| manage them
 *    and communicate to |nsHTMLMediaElement|.
 *
 * Each |DASHRepDecoder| gets init range and index range from its MPD
 * |Representation|. |DASHRepDecoder| uses ChannelMediaResource to start the
 * byte range downloads, calling |OpenByteRange| with a |MediaByteRange|
 * object.
 * Once the init and index segments have been downloaded and |ReadMetadata| has
 * completed, each |WebMReader| notifies it's peer |DASHRepDecoder|.
 * Note: the decoder must wait until index data is parsed because it needs to
 *       get the offsets of the subsegments (WebM clusters) from the media file
 *       itself.
 * Since byte ranges for subsegments are obtained, |nsDASHRepdecoder| continues
 * downloading the files in byte range chunks.
 *
 * XXX Note that this implementation of DASHRepDecoder is focused on DASH
 *     WebM On Demand profile: on the todo list is an action item to make this
 *     more abstract.
 *
 * Note on |Seek|: Currently, |MediaCache| requires that seeking start at the
 *                 beginning of the block in which the desired offset would be
 *                 found. As such, when |ChannelMediaResource| does a seek
 *                 using DASH WebM subsegments (clusters), it requests a start
 *                 offset that corresponds to the beginning of the block, not
 *                 the start offset of the cluster. For DASH Webm, which has
 *                 media encoded in single files, this is fine. Future work on
 *                 other profiles will require this to be re-examined.
 */

#include <limits>
#include <prdtoa.h>
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "VideoUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsICachingChannel.h"
#include "MediaDecoderStateMachine.h"
#include "WebMDecoder.h"
#include "WebMReader.h"
#include "DASHReader.h"
#include "nsDASHMPDParser.h"
#include "DASHRepDecoder.h"
#include "DASHDecoder.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(msg, ...) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, \
                             ("%p [DASHDecoder] " msg, this, __VA_ARGS__))
#define LOG1(msg) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, \
                         ("%p [DASHDecoder] " msg, this))
#else
#define LOG(msg, ...)
#define LOG1(msg)
#endif

DASHDecoder::DASHDecoder() :
  MediaDecoder(),
  mNotifiedLoadAborted(false),
  mBuffer(nullptr),
  mBufferLength(0),
  mMPDReaderThread(nullptr),
  mPrincipal(nullptr),
  mDASHReader(nullptr),
  mVideoAdaptSetIdx(-1),
  mAudioRepDecoderIdx(-1),
  mVideoRepDecoderIdx(-1),
  mAudioSubsegmentIdx(0),
  mVideoSubsegmentIdx(0),
  mAudioMetadataReadCount(0),
  mVideoMetadataReadCount(0),
  mSeeking(false),
  mStatisticsLock("DASHDecoder.mStatisticsLock")
{
  MOZ_COUNT_CTOR(DASHDecoder);
  mAudioStatistics = new MediaChannelStatistics();
  mVideoStatistics = new MediaChannelStatistics();
}

DASHDecoder::~DASHDecoder()
{
  MOZ_COUNT_DTOR(DASHDecoder);
}

MediaDecoderStateMachine*
DASHDecoder::CreateStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return new MediaDecoderStateMachine(this, mDASHReader);
}

void
DASHDecoder::ReleaseStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  // Since state machine owns mDASHReader, remove reference to it.
  mDASHReader = nullptr;

  MediaDecoder::ReleaseStateMachine();
  for (uint i = 0; i < mAudioRepDecoders.Length(); i++) {
    mAudioRepDecoders[i]->ReleaseStateMachine();
  }
  for (uint i = 0; i < mVideoRepDecoders.Length(); i++) {
    mVideoRepDecoders[i]->ReleaseStateMachine();
  }
}

nsresult
DASHDecoder::Load(MediaResource* aResource,
                  nsIStreamListener** aStreamListener,
                  MediaDecoder* aCloneDonor)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  mDASHReader = new DASHReader(this);

  nsresult rv = OpenResource(aResource, aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  mDecoderStateMachine = CreateStateMachine();
  if (!mDecoderStateMachine) {
    LOG1("Failed to create state machine!");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
DASHDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Should be no download ended notification if MPD Manager exists.
  if (mMPDManager) {
    LOG("Network Error! Repeated MPD download notification but MPD Manager "
        "[%p] already exists!", mMPDManager.get());
    NetworkError();
    return;
  }

  if (NS_SUCCEEDED(aStatus)) {
    LOG1("MPD downloaded.");

    // mPrincipal must be set on main thread before dispatch to parser thread.
    mPrincipal = GetCurrentPrincipal();

    // Create reader thread for |ChannelMediaResource|::|Read|.
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &DASHDecoder::ReadMPDBuffer);
    NS_ENSURE_TRUE(event, );

    nsresult rv = NS_NewNamedThread("DASH MPD Reader",
                                    getter_AddRefs(mMPDReaderThread),
                                    event,
                                    MEDIA_THREAD_STACK_SIZE);
    if (NS_FAILED(rv) || !mMPDReaderThread) {
      LOG("Error creating MPD reader thread: rv[%x] thread [%p].",
          rv, mMPDReaderThread.get());
      DecodeError();
      return;
    }
  } else if (aStatus == NS_BINDING_ABORTED) {
    LOG("MPD download has been cancelled by the user: aStatus [%x].", aStatus);
    if (mOwner) {
      mOwner->LoadAborted();
    }
    return;
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    LOG("Network error trying to download MPD: aStatus [%x].", aStatus);
    NetworkError();
  }
}

void
DASHDecoder::ReadMPDBuffer()
{
  NS_ASSERTION(!NS_IsMainThread(), "Should not be on main thread.");

  LOG1("Started reading from the MPD buffer.");

  int64_t length = mResource->GetLength();
  if (length <= 0 || length > DASH_MAX_MPD_SIZE) {
    LOG("MPD is larger than [%d]MB.", DASH_MAX_MPD_SIZE/(1024*1024));
    DecodeError();
    return;
  }

  mBuffer = new char[length];

  uint32_t count = 0;
  nsresult rv = mResource->Read(mBuffer, length, &count);
  // By this point, all bytes should be available for reading.
  if (NS_FAILED(rv) || count != length) {
    LOG("Error reading MPD buffer: rv [%x] count [%d] length [%d].",
        rv, count, length);
    DecodeError();
    return;
  }
  // Store buffer length for processing on main thread.
  mBufferLength = static_cast<uint32_t>(length);

  LOG1("Finished reading MPD buffer; back to main thread for parsing.");

  // Dispatch event to Main thread to parse MPD buffer.
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &DASHDecoder::OnReadMPDBufferCompleted);
  rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    LOG("Error dispatching parse event to main thread: rv[%x]", rv);
    DecodeError();
    return;
  }
}

void
DASHDecoder::OnReadMPDBufferCompleted()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (mShuttingDown) {
    LOG1("Shutting down! Ignoring OnReadMPDBufferCompleted().");
    return;
  }

  // Shutdown the thread.
  if (!mMPDReaderThread) {
    LOG1("Error: MPD reader thread does not exist!");
    DecodeError();
    return;
  }
  nsresult rv = mMPDReaderThread->Shutdown();
  if (NS_FAILED(rv)) {
    LOG("MPD reader thread did not shutdown correctly! rv [%x]", rv);
    DecodeError();
    return;
  }
  mMPDReaderThread = nullptr;

  // Close the MPD resource.
  rv = mResource ? mResource->Close() : NS_ERROR_NULL_POINTER;
  if (NS_FAILED(rv)) {
    LOG("Media Resource did not close correctly! rv [%x]", rv);
    NetworkError();
    return;
  }

  // Start parsing the MPD data and loading the media.
  rv = ParseMPDBuffer();
  if (NS_FAILED(rv)) {
    LOG("Error parsing MPD buffer! rv [%x]", rv);
    DecodeError();
    return;
  }
  rv = CreateRepDecoders();
  if (NS_FAILED(rv)) {
    LOG("Error creating decoders for Representations! rv [%x]", rv);
    DecodeError();
    return;
  }

  rv = LoadRepresentations();
  if (NS_FAILED(rv)) {
    LOG("Error loading Representations! rv [%x]", rv);
    NetworkError();
    return;
  }

  // Notify reader that it can start reading metadata. Sub-readers will still
  // block until sub-resources have downloaded data into the media cache.
  mDASHReader->ReadyToReadMetadata();
}

nsresult
DASHDecoder::ParseMPDBuffer()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_NULL_POINTER);

  LOG1("Started parsing the MPD buffer.");

  // Parse MPD buffer and get root DOM element.
  nsAutoPtr<nsDASHMPDParser> parser;
  parser = new nsDASHMPDParser(mBuffer.forget(), mBufferLength, mPrincipal,
                               mResource->URI());
  mozilla::net::DASHMPDProfile profile;
  parser->Parse(getter_Transfers(mMPDManager), &profile);
  mBuffer = nullptr;
  NS_ENSURE_TRUE(mMPDManager, NS_ERROR_NULL_POINTER);

  LOG("Finished parsing the MPD buffer. Profile is [%d].", profile);

  return NS_OK;
}

nsresult
DASHDecoder::CreateRepDecoders()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_TRUE(mMPDManager, NS_ERROR_NULL_POINTER);

  // Global settings for the presentation.
  int64_t startTime = mMPDManager->GetStartTime();
  mDuration = mMPDManager->GetDuration();
  NS_ENSURE_TRUE(startTime >= 0 && mDuration > 0, NS_ERROR_ILLEGAL_VALUE);

  // For each audio/video stream, create a |ChannelMediaResource| object.

  for (int i = 0; i < mMPDManager->GetNumAdaptationSets(); i++) {
    IMPDManager::AdaptationSetType asType = mMPDManager->GetAdaptationSetType(i);
    if (asType == IMPDManager::DASH_VIDEO_STREAM) {
      mVideoAdaptSetIdx = i;
    }
    for (int j = 0; j < mMPDManager->GetNumRepresentations(i); j++) {
      // Get URL string.
      nsAutoString segmentUrl;
      nsresult rv = mMPDManager->GetFirstSegmentUrl(i, j, segmentUrl);
      NS_ENSURE_SUCCESS(rv, rv);

      // Get segment |nsIURI|; use MPD's base URI in case of relative paths.
      nsCOMPtr<nsIURI> url;
      rv = NS_NewURI(getter_AddRefs(url), segmentUrl, nullptr, mResource->URI());
      NS_ENSURE_SUCCESS(rv, rv);
#ifdef PR_LOGGING
      nsAutoCString newUrl;
      rv = url->GetSpec(newUrl);
      NS_ENSURE_SUCCESS(rv, rv);
      LOG("Using URL=\"%s\" for AdaptationSet [%d] Representation [%d]",
          newUrl.get(), i, j);
#endif

      // 'file://' URLs are not supported.
      nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(url);
      NS_ENSURE_FALSE(fileURL, NS_ERROR_ILLEGAL_VALUE);

      // Create |DASHRepDecoder| objects for each representation.
      if (asType == IMPDManager::DASH_VIDEO_STREAM) {
        Representation const * rep = mMPDManager->GetRepresentation(i, j);
        NS_ENSURE_TRUE(rep, NS_ERROR_NULL_POINTER);
        rv = CreateVideoRepDecoder(url, rep);
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (asType == IMPDManager::DASH_AUDIO_STREAM) {
        Representation const * rep = mMPDManager->GetRepresentation(i, j);
        NS_ENSURE_TRUE(rep, NS_ERROR_NULL_POINTER);
        rv = CreateAudioRepDecoder(url, rep);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  NS_ENSURE_TRUE(VideoRepDecoder(), NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(AudioRepDecoder(), NS_ERROR_NOT_INITIALIZED);

  return NS_OK;
}

nsresult
DASHDecoder::CreateAudioRepDecoder(nsIURI* aUrl,
                                   mozilla::net::Representation const * aRep)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_ARG(aUrl);
  NS_ENSURE_ARG(aRep);
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NOT_INITIALIZED);

  // Create subdecoder and init with media element.
  DASHRepDecoder* audioDecoder = new DASHRepDecoder(this);
  NS_ENSURE_TRUE(audioDecoder->Init(mOwner), NS_ERROR_NOT_INITIALIZED);

  // Set current decoder to the first one created.
  if (mAudioRepDecoderIdx == -1) {
    mAudioRepDecoderIdx = 0;
  }
  mAudioRepDecoders.AppendElement(audioDecoder);

  // Create sub-reader; attach to DASH reader and sub-decoder.
  WebMReader* audioReader = new WebMReader(audioDecoder);
  if (mDASHReader) {
    audioReader->SetMainReader(mDASHReader);
    mDASHReader->AddAudioReader(audioReader);
  }
  audioDecoder->SetReader(audioReader);

  // Create media resource with URL and connect to sub-decoder.
  MediaResource* audioResource
    = CreateAudioSubResource(aUrl, static_cast<MediaDecoder*>(audioDecoder));
  NS_ENSURE_TRUE(audioResource, NS_ERROR_NOT_INITIALIZED);

  audioDecoder->SetResource(audioResource);
  audioDecoder->SetMPDRepresentation(aRep);

  LOG("Created audio DASHRepDecoder [%p]", audioDecoder);

  return NS_OK;
}

nsresult
DASHDecoder::CreateVideoRepDecoder(nsIURI* aUrl,
                                   mozilla::net::Representation const * aRep)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_ARG(aUrl);
  NS_ENSURE_ARG(aRep);
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NOT_INITIALIZED);

  // Create subdecoder and init with media element.
  DASHRepDecoder* videoDecoder = new DASHRepDecoder(this);
  NS_ENSURE_TRUE(videoDecoder->Init(mOwner), NS_ERROR_NOT_INITIALIZED);

  // Set current decoder to the first one created.
  if (mVideoRepDecoderIdx == -1) {
    mVideoRepDecoderIdx = 0;
  }
  mVideoRepDecoders.AppendElement(videoDecoder);

  // Create sub-reader; attach to DASH reader and sub-decoder.
  WebMReader* videoReader = new WebMReader(videoDecoder);
  if (mDASHReader) {
    videoReader->SetMainReader(mDASHReader);
    mDASHReader->AddVideoReader(videoReader);
  }
  videoDecoder->SetReader(videoReader);

  // Create media resource with URL and connect to sub-decoder.
  MediaResource* videoResource
    = CreateVideoSubResource(aUrl, static_cast<MediaDecoder*>(videoDecoder));
  NS_ENSURE_TRUE(videoResource, NS_ERROR_NOT_INITIALIZED);

  videoDecoder->SetResource(videoResource);
  videoDecoder->SetMPDRepresentation(aRep);

  LOG("Created video DASHRepDecoder [%p]", videoDecoder);

  return NS_OK;
}

MediaResource*
DASHDecoder::CreateAudioSubResource(nsIURI* aUrl,
                                    MediaDecoder* aAudioDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_TRUE(aUrl, nullptr);
  NS_ENSURE_TRUE(aAudioDecoder, nullptr);

  // Create channel for representation.
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = CreateSubChannel(aUrl, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, nullptr);

  // Create resource for representation.
  MediaResource* audioResource
    = MediaResource::Create(aAudioDecoder, channel);
  NS_ENSURE_TRUE(audioResource, nullptr);

  audioResource->RecordStatisticsTo(mAudioStatistics);
  return audioResource;
}

MediaResource*
DASHDecoder::CreateVideoSubResource(nsIURI* aUrl,
                                    MediaDecoder* aVideoDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_TRUE(aUrl, nullptr);
  NS_ENSURE_TRUE(aVideoDecoder, nullptr);

  // Create channel for representation.
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = CreateSubChannel(aUrl, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, nullptr);

  // Create resource for representation.
  MediaResource* videoResource
    = MediaResource::Create(aVideoDecoder, channel);
  NS_ENSURE_TRUE(videoResource, nullptr);

  videoResource->RecordStatisticsTo(mVideoStatistics);
  return videoResource;
}

nsresult
DASHDecoder::CreateSubChannel(nsIURI* aUrl, nsIChannel** aChannel)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_ARG(aUrl);

  NS_ENSURE_TRUE(mOwner, NS_ERROR_NULL_POINTER);
  nsHTMLMediaElement* element = mOwner->GetMediaElement();
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsILoadGroup> loadGroup =
    element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, NS_ERROR_NULL_POINTER);

  // Check for a Content Security Policy to pass down to the channel
  // created to load the media content.
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = element->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv,rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_MEDIA);
  }
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aUrl,
                     nullptr,
                     loadGroup,
                     nullptr,
                     nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY,
                     channelPolicy);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ENSURE_TRUE(channel, NS_ERROR_NULL_POINTER);

  NS_ADDREF(*aChannel = channel);
  return NS_OK;
}

nsresult
DASHDecoder::LoadRepresentations()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  nsresult rv;
  {
    // Hold the lock while we do this to set proper lock ordering
    // expectations for dynamic deadlock detectors: decoder lock(s)
    // should be grabbed before the cache lock.
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

    // Load the decoders for each |Representation|'s media streams.
    // XXX Prob ok to load all audio decoders, since there should only be one
    //     created, but need to review the rest of the file.
    if (AudioRepDecoder()) {
      rv = AudioRepDecoder()->Load();
      NS_ENSURE_SUCCESS(rv, rv);
      mAudioMetadataReadCount++;
    }
    // Load all video decoders.
    for (uint32_t i = 0; i < mVideoRepDecoders.Length(); i++) {
      rv = mVideoRepDecoders[i]->Load();
      NS_ENSURE_SUCCESS(rv, rv);
      mVideoMetadataReadCount++;
    }
    if (AudioRepDecoder()) {
      AudioRepDecoder()->SetStateMachine(mDecoderStateMachine);
    }
    for (uint32_t i = 0; i < mVideoRepDecoders.Length(); i++) {
      mVideoRepDecoders[i]->SetStateMachine(mDecoderStateMachine);
    }
  }
  // Now that subreaders are init'd, it's ok to init state machine.
  return InitializeStateMachine(nullptr);
}

void
DASHDecoder::NotifyDownloadEnded(DASHRepDecoder* aRepDecoder,
                                 nsresult aStatus,
                                 int32_t const aSubsegmentIdx)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (mShuttingDown) {
    LOG1("Shutting down! Ignoring NotifyDownloadEnded().");
    return;
  }

  // MPD Manager must exist, indicating MPD has been downloaded and parsed.
  if (!mMPDManager) {
    LOG1("Network Error! MPD Manager must exist, indicating MPD has been "
        "downloaded and parsed");
    NetworkError();
    return;
  }

  // Decoder for the media |Representation| must not be null.
  if (!aRepDecoder) {
    LOG1("Decoder for Representation is reported as null.");
    DecodeError();
    return;
  }

  if (NS_SUCCEEDED(aStatus)) {
    LOG("Byte range downloaded: decoder [%p] subsegmentIdx [%d]",
        aRepDecoder, aSubsegmentIdx);

    if (aSubsegmentIdx < 0) {
      LOG("Last subsegment for decoder [%p] was downloaded",
          aRepDecoder);
      return;
    }

    nsRefPtr<DASHRepDecoder> decoder = aRepDecoder;
    {
      ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

      if (!IsDecoderAllowedToDownloadSubsegment(aRepDecoder,
                                                aSubsegmentIdx)) {
        NS_WARNING("Decoder downloaded subsegment but it is not allowed!");
        LOG("Error! Decoder [%p] downloaded subsegment [%d] but it is not "
            "allowed!", aRepDecoder, aSubsegmentIdx);
        return;
      }

      if (aRepDecoder == VideoRepDecoder() &&
          mVideoSubsegmentIdx == aSubsegmentIdx) {
        IncrementSubsegmentIndex(aRepDecoder);
      } else if (aRepDecoder == AudioRepDecoder() &&
          mAudioSubsegmentIdx == aSubsegmentIdx) {
        IncrementSubsegmentIndex(aRepDecoder);
      } else {
        return;
      }

      // Do Stream Switching here before loading next bytes.
      // Audio stream switching not supported.
      if (aRepDecoder == VideoRepDecoder() &&
          mVideoSubsegmentIdx < VideoRepDecoder()->GetNumDataByteRanges()) {
        nsresult rv = PossiblySwitchDecoder(aRepDecoder);
        if (NS_FAILED(rv)) {
          LOG("Failed possibly switching decoder rv[0x%x]", rv);
          DecodeError();
          return;
        }
        decoder = VideoRepDecoder();
      }
    }

    // Before loading, note the index of the decoder which will downloaded the
    // next video subsegment.
    if (decoder == VideoRepDecoder()) {
      if (mVideoSubsegmentLoads.IsEmpty() ||
          (uint32_t)mVideoSubsegmentIdx >= mVideoSubsegmentLoads.Length()) {
        LOG("Appending decoder [%d] [%p] to mVideoSubsegmentLoads at index "
            "[%d] before load; mVideoSubsegmentIdx[%d].",
            mVideoRepDecoderIdx, VideoRepDecoder(),
            mVideoSubsegmentLoads.Length(), mVideoSubsegmentIdx);
        mVideoSubsegmentLoads.AppendElement(mVideoRepDecoderIdx);
      } else {
        // Change an existing load, and keep subsequent entries to help
        // determine if subsegments are cached already.
        LOG("Setting decoder [%d] [%p] in mVideoSubsegmentLoads at index "
            "[%d] before load; mVideoSubsegmentIdx[%d].",
            mVideoRepDecoderIdx, VideoRepDecoder(),
            mVideoSubsegmentIdx, mVideoSubsegmentIdx);
        mVideoSubsegmentLoads[mVideoSubsegmentIdx] = mVideoRepDecoderIdx;
      }
    }

    // Load the next range of data bytes. If the range is already cached,
    // this function will be called again to adaptively download the next
    // subsegment.
#ifdef PR_LOGGING
    if (decoder.get() == AudioRepDecoder()) {
      LOG("Requesting load for audio decoder [%p] subsegment [%d].",
        decoder.get(), mAudioSubsegmentIdx);
    } else if (decoder.get() == VideoRepDecoder()) {
      LOG("Requesting load for video decoder [%p] subsegment [%d].",
        decoder.get(), mVideoSubsegmentIdx);
    }
#endif
    if (!decoder || (decoder != AudioRepDecoder() &&
                     decoder != VideoRepDecoder())) {
      LOG("Invalid decoder [%p]: video idx [%d] audio idx [%d]",
          decoder.get(), AudioRepDecoder(), VideoRepDecoder());
      DecodeError();
      return;
    }
    decoder->LoadNextByteRange();
  } else if (aStatus == NS_BINDING_ABORTED) {
    LOG("MPD download has been cancelled by the user: aStatus [%x].", aStatus);
    if (mOwner) {
      mOwner->LoadAborted();
    }
    return;
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    LOG("Network error trying to download MPD: aStatus [%x].", aStatus);
    NetworkError();
  }
}

void
DASHDecoder::LoadAborted()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (!mNotifiedLoadAborted && mOwner) {
    mOwner->LoadAborted();
    mNotifiedLoadAborted = true;
    LOG1("Load Aborted! Notifying media element.");
  }
}

void
DASHDecoder::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Notify reader of shutdown first.
  if (mDASHReader) {
    mDASHReader->NotifyDecoderShuttingDown();
  }

  // Call parent class shutdown.
  MediaDecoder::Shutdown();
  NS_ENSURE_TRUE(mShuttingDown, );

  // Shutdown reader thread if not already done.
  if (mMPDReaderThread) {
    nsresult rv = mMPDReaderThread->Shutdown();
    NS_ENSURE_SUCCESS(rv, );
    mMPDReaderThread = nullptr;
  }

  // Forward to sub-decoders.
  for (uint i = 0; i < mAudioRepDecoders.Length(); i++) {
    if (mAudioRepDecoders[i]) {
      mAudioRepDecoders[i]->Shutdown();
    }
  }
  for (uint i = 0; i < mVideoRepDecoders.Length(); i++) {
    if (mVideoRepDecoders[i]) {
      mVideoRepDecoders[i]->Shutdown();
    }
  }
}

void
DASHDecoder::DecodeError()
{
  if (NS_IsMainThread()) {
    MediaDecoder::DecodeError();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &MediaDecoder::DecodeError);
    nsresult rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      LOG("Error dispatching DecodeError event to main thread: rv[%x]", rv);
    }
  }
}

void
DASHDecoder::OnReadMetadataCompleted(DASHRepDecoder* aRepDecoder)
{
  if (mShuttingDown) {
    LOG1("Shutting down! Ignoring OnReadMetadataCompleted().");
    return;
  }

  NS_ASSERTION(aRepDecoder, "aRepDecoder is null!");
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  LOG("Metadata loaded for decoder[%p]", aRepDecoder);

  // Decrement audio|video metadata read counter and get ref to active decoder.
  nsRefPtr<DASHRepDecoder> activeDecoder;
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    for (uint32_t i = 0; i < mAudioRepDecoders.Length(); i++) {
      if (aRepDecoder == mAudioRepDecoders[i]) {
        --mAudioMetadataReadCount;
        break;
      }
    }
    for (uint32_t i = 0; i < mVideoRepDecoders.Length(); i++) {
      if (aRepDecoder == mVideoRepDecoders[i]) {
        --mVideoMetadataReadCount;
        break;
      }
    }
  }

  // Once all metadata is downloaded for audio|video decoders, start loading
  // data for the active decoder.
  if (mAudioMetadataReadCount == 0 && mVideoMetadataReadCount == 0) {
    if (AudioRepDecoder()) {
      LOG("Dispatching load event for audio decoder [%p]", AudioRepDecoder());
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(AudioRepDecoder(), &DASHRepDecoder::LoadNextByteRange);
      nsresult rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        LOG("Error dispatching audio decoder [%p] load event to main thread: "
            "rv[%x]", AudioRepDecoder(), rv);
        DecodeError();
        return;
      }
    }
    if (VideoRepDecoder()) {
      LOG("Dispatching load event for video decoder [%p]", VideoRepDecoder());
      // Add decoder to subsegment load history.
      NS_ASSERTION(mVideoSubsegmentLoads.IsEmpty(),
                   "No subsegment loads should be recorded at this stage!");
      NS_ASSERTION(mVideoSubsegmentIdx == 0,
                   "Current subsegment should be 0 at this stage!");
      LOG("Appending decoder [%d] [%p] to mVideoSubsegmentLoads at index "
          "[%d] before load; mVideoSubsegmentIdx[%d].",
          mVideoRepDecoderIdx, VideoRepDecoder(),
          (uint32_t)mVideoSubsegmentLoads.Length(), mVideoSubsegmentIdx);
      mVideoSubsegmentLoads.AppendElement(mVideoRepDecoderIdx);

      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(VideoRepDecoder(), &DASHRepDecoder::LoadNextByteRange);
      nsresult rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        LOG("Error dispatching video decoder [%p] load event to main thread: "
            "rv[%x]", VideoRepDecoder(), rv);
        DecodeError();
        return;
      }
    }
  }
}

nsresult
DASHDecoder::PossiblySwitchDecoder(DASHRepDecoder* aRepDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_FALSE(mShuttingDown, NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(aRepDecoder == VideoRepDecoder(), NS_ERROR_ILLEGAL_VALUE);
  NS_ASSERTION((uint32_t)mVideoRepDecoderIdx < mVideoRepDecoders.Length(),
               "Index for video decoder is out of bounds!");
  NS_ASSERTION((uint32_t)mVideoSubsegmentIdx < VideoRepDecoder()->GetNumDataByteRanges(),
               "Can't switch to a byte range out of bounds.");
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  // Now, determine if and which decoder to switch to.
  // XXX This download rate is averaged over time, and only refers to the bytes
  // downloaded for the video decoder. A running average would be better, and
  // something that includes all downloads. But this will do for now.
  NS_ASSERTION(VideoRepDecoder(), "Video decoder should not be null.");
  NS_ASSERTION(VideoRepDecoder()->GetResource(),
               "Video resource should not be null");
  bool reliable = false;
  double downloadRate = 0;
  {
    MutexAutoLock lock(mStatisticsLock);
    downloadRate = mVideoStatistics->GetRate(&reliable);
  }
  uint32_t bestRepIdx = UINT32_MAX;
  bool noRepAvailable = !mMPDManager->GetBestRepForBandwidth(mVideoAdaptSetIdx,
                                                             downloadRate,
                                                             bestRepIdx);
  LOG("downloadRate [%0.2f kbps] reliable [%s] bestRepIdx [%d] noRepAvailable [%s]",
      downloadRate/1000.0, (reliable ? "yes" : "no"), bestRepIdx,
      (noRepAvailable ? "yes" : "no"));

  // If there is a higher bitrate stream that can be downloaded with the
  // current estimated bandwidth, step up to the next stream, for a graceful
  // increase in quality.
  uint32_t toDecoderIdx = mVideoRepDecoderIdx;
  if (bestRepIdx > toDecoderIdx) {
    toDecoderIdx = NS_MIN(toDecoderIdx+1, mVideoRepDecoders.Length()-1);
  } else if (toDecoderIdx < bestRepIdx) {
    // If the bitrate is too much for the current bandwidth, just use that
    // stream directly.
    toDecoderIdx = bestRepIdx;
  }

  // Upgrade |toDecoderIdx| if a better subsegment was previously downloaded and
  // is still cached.
  if (mVideoSubsegmentIdx < mVideoSubsegmentLoads.Length() &&
      toDecoderIdx < mVideoSubsegmentLoads[mVideoSubsegmentIdx]) {
    // Check if the subsegment is cached.
    uint32_t betterRepIdx = mVideoSubsegmentLoads[mVideoSubsegmentIdx];
    if (mVideoRepDecoders[betterRepIdx]->IsSubsegmentCached(mVideoSubsegmentIdx)) {
      toDecoderIdx = betterRepIdx;
    }
  }

  NS_ENSURE_TRUE(toDecoderIdx < mVideoRepDecoders.Length(),
                 NS_ERROR_ILLEGAL_VALUE);

  // Notify reader and sub decoders and do the switch.
  if (toDecoderIdx != mVideoRepDecoderIdx) {
    LOG("*** Switching video decoder from [%d] [%p] to [%d] [%p] at "
        "subsegment [%d]", mVideoRepDecoderIdx, VideoRepDecoder(),
        toDecoderIdx, mVideoRepDecoders[toDecoderIdx].get(),
        mVideoSubsegmentIdx);

    // Tell main reader to switch subreaders at |subsegmentIdx| - equates to
    // switching data source for reading.
    mDASHReader->RequestVideoReaderSwitch(mVideoRepDecoderIdx, toDecoderIdx,
                                          mVideoSubsegmentIdx);
    // Notify decoder it is about to be switched.
    mVideoRepDecoders[mVideoRepDecoderIdx]->PrepareForSwitch();
    // Switch video decoders - equates to switching download source.
    mVideoRepDecoderIdx = toDecoderIdx;
  }

  return NS_OK;
}

nsresult
DASHDecoder::Seek(double aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_FALSE(mShuttingDown, NS_ERROR_UNEXPECTED);

  LOG("Seeking to [%.2fs]", aTime);

  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    // We want to stop the current series of downloads and restart later with
    // the appropriate subsegment.

    // 1 - Set the seeking flag, so that when current subsegments download (if
    // any), the next subsegment will not be downloaded.
    mSeeking = true;

    // 2 - Cancel all current downloads to reset for seeking.
    for (uint32_t i = 0; i < mAudioRepDecoders.Length(); i++) {
      mAudioRepDecoders[i]->CancelByteRangeLoad();
    }
    for (uint32_t i = 0; i < mVideoRepDecoders.Length(); i++) {
      mVideoRepDecoders[i]->CancelByteRangeLoad();
    }
  }

  return MediaDecoder::Seek(aTime);
}

void
DASHDecoder::NotifySeekInVideoSubsegment(int32_t aRepDecoderIdx,
                                         int32_t aSubsegmentIdx)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  NS_ASSERTION(0 <= aRepDecoderIdx &&
               aRepDecoderIdx < mVideoRepDecoders.Length(),
               "Video decoder index is out of bounds");

  // Reset current subsegment to match the one being seeked.
  mVideoSubsegmentIdx = aSubsegmentIdx;
  // Reset current decoder to match the one returned by
  // |GetRepIdxForVideoSubsegmentLoad|.
  mVideoRepDecoderIdx = aRepDecoderIdx;

  mSeeking = false;

  LOG("Dispatching load for video decoder [%d] [%p]: seek in subsegment [%d]",
      mVideoRepDecoderIdx, VideoRepDecoder(), aSubsegmentIdx);

  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(VideoRepDecoder(),
                         &DASHRepDecoder::LoadNextByteRange);
  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv)) {
    LOG("Error dispatching video byte range load: rv[0x%x].",
        rv);
    NetworkError();
    return;
  }
}

void
DASHDecoder::NotifySeekInAudioSubsegment(int32_t aSubsegmentIdx)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  // Reset current subsegment to match the one being seeked.
  mAudioSubsegmentIdx = aSubsegmentIdx;

  LOG("Dispatching seeking load for audio decoder [%d] [%p]: subsegment [%d]",
     mAudioRepDecoderIdx, AudioRepDecoder(), aSubsegmentIdx);

  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(AudioRepDecoder(),
                         &DASHRepDecoder::LoadNextByteRange);
  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv)) {
    LOG("Error dispatching audio byte range load: rv[0x%x].",
        rv);
    NetworkError();
    return;
  }
}

bool
DASHDecoder::IsDecoderAllowedToDownloadData(DASHRepDecoder* aRepDecoder)
{
  NS_ASSERTION(aRepDecoder, "DASHRepDecoder pointer is null.");

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  // Only return true if |aRepDecoder| is active and metadata for all
  // representations has been downloaded.
  return ((aRepDecoder == AudioRepDecoder() && mAudioMetadataReadCount == 0) ||
          (aRepDecoder == VideoRepDecoder() && mVideoMetadataReadCount == 0));
}

bool
DASHDecoder::IsDecoderAllowedToDownloadSubsegment(DASHRepDecoder* aRepDecoder,
                                                  int32_t const aSubsegmentIdx)
{
  NS_ASSERTION(aRepDecoder, "DASHRepDecoder pointer is null.");

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  // Forbid any downloads until we've been told what subsegment to seek to.
  if (mSeeking) {
    return false;
  }
  // Return false if there is still metadata to be downloaded.
  if (mAudioMetadataReadCount != 0 || mVideoMetadataReadCount != 0) {
    return false;
  }
  // No audio switching; allow the audio decoder to download all subsegments.
  if (aRepDecoder == AudioRepDecoder()) {
    return true;
  }

  int32_t videoDecoderIdx = GetRepIdxForVideoSubsegmentLoad(aSubsegmentIdx);
  if (aRepDecoder == mVideoRepDecoders[videoDecoderIdx]) {
    return true;
  }
  return false;
}

void
DASHDecoder::SetSubsegmentIndex(DASHRepDecoder* aRepDecoder,
                                int32_t aSubsegmentIdx)
{
  NS_ASSERTION(0 <= aSubsegmentIdx,
               "Subsegment index should not be negative!");
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (aRepDecoder == AudioRepDecoder()) {
    mAudioSubsegmentIdx = aSubsegmentIdx;
  } else if (aRepDecoder == VideoRepDecoder()) {
    // If this is called in the context of a Seek, we need to cancel downloads
    // from other rep decoders, or all rep decoders if we're not seeking in the
    // current subsegment.
    // Note: NotifySeekInSubsegment called from DASHReader will already have
    // set the current decoder.
    mVideoSubsegmentIdx = aSubsegmentIdx;
  }
}

double
DASHDecoder::ComputePlaybackRate(bool* aReliable)
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  MOZ_ASSERT(NS_IsMainThread() || OnStateMachineThread());
  NS_ASSERTION(aReliable, "Bool pointer aRelible should not be null!");

  // While downloading the MPD, return 0; do not count manifest as media data.
  if (mResource && !mMPDManager) {
    return 0;
  }

  // Once MPD is downloaded, use the rate from the video decoder.
  // XXX Not ideal, but since playback rate is used to estimate if we have
  // enough data to continue playing, this should be sufficient.
  double videoRate = 0;
  if (VideoRepDecoder()) {
    videoRate = VideoRepDecoder()->ComputePlaybackRate(aReliable);
  }
  return videoRate;
}

void
DASHDecoder::UpdatePlaybackRate()
{
  MOZ_ASSERT(NS_IsMainThread() || OnStateMachineThread());
  GetReentrantMonitor().AssertCurrentThreadIn();
  // While downloading the MPD, return silently; playback rate has no meaning
  // for the manifest.
  if (mResource && !mMPDManager) {
    return;
  }
  // Once MPD is downloaded and audio/video decoder(s) are loading, forward to
  // active rep decoders.
  if (AudioRepDecoder()) {
    AudioRepDecoder()->UpdatePlaybackRate();
  }
  if (VideoRepDecoder()) {
    VideoRepDecoder()->UpdatePlaybackRate();
  }
}

void
DASHDecoder::NotifyPlaybackStarted()
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  // While downloading the MPD, return silently; playback rate has no meaning
  // for the manifest.
  if (mResource && !mMPDManager) {
    return;
  }
  // Once MPD is downloaded and audio/video decoder(s) are loading, forward to
  // active rep decoders.
  if (AudioRepDecoder()) {
    AudioRepDecoder()->NotifyPlaybackStarted();
  }
  if (VideoRepDecoder()) {
    VideoRepDecoder()->NotifyPlaybackStarted();
  }
}

void
DASHDecoder::NotifyPlaybackStopped()
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  // While downloading the MPD, return silently; playback rate has no meaning
  // for the manifest.
  if (mResource && !mMPDManager) {
    return;
  }
  // Once  // Once MPD is downloaded and audio/video decoder(s) are loading, forward to
  // active rep decoders.
  if (AudioRepDecoder()) {
    AudioRepDecoder()->NotifyPlaybackStopped();
  }
  if (VideoRepDecoder()) {
    VideoRepDecoder()->NotifyPlaybackStopped();
  }
}

MediaDecoder::Statistics
DASHDecoder::GetStatistics()
{
  MOZ_ASSERT(NS_IsMainThread() || OnStateMachineThread());
  Statistics result;

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (mResource && !mMPDManager) {
    return MediaDecoder::GetStatistics();
  }

  // XXX Use video decoder and its media resource to get stats.
  // This assumes that the following getter functions are getting relevant
  // video data only.
  if (VideoRepDecoder() && VideoRepDecoder()->GetResource()) {
    MediaResource *resource = VideoRepDecoder()->GetResource();
    // Note: this rate reflects the rate observed for all video downloads.
    result.mDownloadRate =
      resource->GetDownloadRate(&result.mDownloadRateReliable);
    result.mDownloadPosition =
      resource->GetCachedDataEnd(VideoRepDecoder()->mDecoderPosition);
    result.mTotalBytes = resource->GetLength();
    result.mPlaybackRate = ComputePlaybackRate(&result.mPlaybackRateReliable);
    result.mDecoderPosition = VideoRepDecoder()->mDecoderPosition;
    result.mPlaybackPosition = VideoRepDecoder()->mPlaybackPosition;
  }
  else {
    result.mDownloadRate = 0;
    result.mDownloadRateReliable = true;
    result.mPlaybackRate = 0;
    result.mPlaybackRateReliable = true;
    result.mDecoderPosition = 0;
    result.mPlaybackPosition = 0;
    result.mDownloadPosition = 0;
    result.mTotalBytes = 0;
  }

  return result;
}

} // namespace mozilla
