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
 * Interaction with nsBuiltinDecoderStateMachine, nsHTMLMediaElement,
 * ChannelMediaResource and sub-decoders (nsWebMDecoder).
 *
 *
 *        nsBuiltinDecoderStateMachine      nsHTMLMediaElement
 *               1 /           \ 1           / 1
 *                /             \           /
 *             1 /               \ 1       / 1
 * nsDASHReader ------ nsDASHDecoder ------------ ChannelMediaResource
 *          |1          1      1     |1      \1             (for MPD Manifest)
 *          |                        |        ------------
 *          |*                       |*                   \*
 *     nsWebMReader ------- nsDASHRepDecoder ------- ChannelMediaResource
 *                 1       1                      1       1 (for media streams)
 *
 * One decoder and state-machine, as with current, non-DASH decoders.
 *
 * DASH adds multiple readers, decoders and resources, in order to manage
 * download and decode of the MPD manifest and individual media streams.
 *
 * Rep/|Representation| is for an individual media stream, e.g. audio
 * nsDASHRepDecoder is the decoder for a rep/|Representation|.
 *
 * FLOW
 *
 * 1 - Download and parse the MPD (DASH XML-based manifest).
 *
 * Media element creates new |nsDASHDecoder| object:
 *   member var initialization to default values, including a/v sub-decoders.
 *   nsBuiltinDecoder and nsMediaDecoder constructors are called.
 *   nsBuiltinDecoder::Init() is called.
 *
 * Media element creates new |ChannelMediaResource|:
 *   used to download MPD manifest.
 *
 * Media element calls |nsDASHDecoder|->Load() to download the MPD file:
 *   creates an |nsDASHReader| object to forward calls to multiple
 *     nsWebMReaders (corresponding to MPD |Representation|s i.e. streams).
 *     Note: 1 |nsDASHReader| per DASH/WebM MPD.
 *
 *   also calls |ChannelMediaResource|::Open().
 *     uses nsHttpChannel to download MPD; notifies nsDASHDecoder.
 *
 * Meanwhile, back in |nsDASHDecoder|->Load():
 *   nsBuiltinDecoderStateMachine is created.
 *     has ref to |nsDASHReader| object.
 *     state machine is scheduled.
 *
 * Media element finishes decoder setup:
 *   element added to media URI table etc.
 *
 * -- At this point, objects are waiting on HTTP returning MPD data.
 *
 * MPD Download (Async |ChannelMediaResource| channel callbacks):
 *   calls nsDASHDecoder::|NotifyDownloadEnded|().
 *     nsDASHDecoder parses MPD XML to DOM to MPD classes.
 *       gets |Segment| URLs from MPD for audio and video streams.
 *       creates |nsIChannel|s, |ChannelMediaResource|s.
 *         stores resources as member vars (to forward function calls later).
 *       creates |nsWebMReader|s and |nsDASHRepDecoder|s.
 *         DASHreader creates |nsWebMReader|s.
 *         |Representation| decoders are connected to the |ChannelMediaResource|s.
 *
 *     |nsDASHDecoder|->|LoadRepresentations|() starts download and decode.
 *
 *
 * 2 - Media Stream, Byte Range downloads.
 *
 * -- At this point the Segment media stream downloads are managed by
 *    individual |ChannelMediaResource|s and |nsWebMReader|s.
 *    A single |nsDASHDecoder| and |nsBuiltinDecoderStateMachine| manage them
 *    and communicate to |nsHTMLMediaElement|.
 *
 * Each |nsDASHRepDecoder| gets init range and index range from its MPD
 * |Representation|. |nsDASHRepDecoder| uses ChannelMediaResource to start the
 * byte range downloads, calling |OpenByteRange| with a |MediaByteRange|
 * object.
 * Once the init and index segments have been downloaded and |ReadMetadata| has
 * completed, each |nsWebMReader| notifies it's peer |nsDASHRepDecoder|.
 * Note: the decoder must wait until index data is parsed because it needs to
 *       get the offsets of the subsegments (WebM clusters) from the media file
 *       itself.
 * Since byte ranges for subsegments are obtained, |nsDASHRepdecoder| continues
 * downloading the files in byte range chunks.
 *
 * XXX Note that this implementation of nsDASHRepDecoder is focused on DASH
 *     WebM On Demand profile: on the todo list is an action item to make this
 *     more abstract.
 *
 * Note on |Seek|: Currently, |nsMediaCache| requires that seeking start at the
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
#include "nsBuiltinDecoderStateMachine.h"
#include "nsWebMDecoder.h"
#include "nsWebMReader.h"
#include "nsDASHReader.h"
#include "nsDASHMPDParser.h"
#include "nsDASHRepDecoder.h"
#include "nsDASHDecoder.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(msg, ...) PR_LOG(gBuiltinDecoderLog, PR_LOG_DEBUG, \
                             ("%p [nsDASHDecoder] " msg, this, __VA_ARGS__))
#define LOG1(msg) PR_LOG(gBuiltinDecoderLog, PR_LOG_DEBUG, \
                         ("%p [nsDASHDecoder] " msg, this))
#else
#define LOG(msg, ...)
#define LOG1(msg)
#endif

nsDASHDecoder::nsDASHDecoder() :
  nsBuiltinDecoder(),
  mNotifiedLoadAborted(false),
  mBuffer(nullptr),
  mBufferLength(0),
  mMPDReaderThread(nullptr),
  mPrincipal(nullptr),
  mDASHReader(nullptr),
  mAudioRepDecoder(nullptr),
  mVideoRepDecoder(nullptr)
{
  MOZ_COUNT_CTOR(nsDASHDecoder);
}

nsDASHDecoder::~nsDASHDecoder()
{
  MOZ_COUNT_DTOR(nsDASHDecoder);
}

nsDecoderStateMachine*
nsDASHDecoder::CreateStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return new nsBuiltinDecoderStateMachine(this, mDASHReader);
}

void
nsDASHDecoder::ReleaseStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  // Since state machine owns mDASHReader, remove reference to it.
  mDASHReader = nullptr;

  nsBuiltinDecoder::ReleaseStateMachine();
  for (uint i = 0; i < mAudioRepDecoders.Length(); i++) {
    mAudioRepDecoders[i]->ReleaseStateMachine();
  }
  for (uint i = 0; i < mVideoRepDecoders.Length(); i++) {
    mVideoRepDecoders[i]->ReleaseStateMachine();
  }
}

nsresult
nsDASHDecoder::Load(MediaResource* aResource,
                    nsIStreamListener** aStreamListener,
                    nsMediaDecoder* aCloneDonor)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  mDASHReader = new nsDASHReader(this);

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
nsDASHDecoder::NotifyDownloadEnded(nsresult aStatus)
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
      NS_NewRunnableMethod(this, &nsDASHDecoder::ReadMPDBuffer);
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
    if (mElement) {
      mElement->LoadAborted();
    }
    return;
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    LOG("Network error trying to download MPD: aStatus [%x].", aStatus);
    NetworkError();
  }
}

void
nsDASHDecoder::ReadMPDBuffer()
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
    NS_NewRunnableMethod(this, &nsDASHDecoder::OnReadMPDBufferCompleted);
  rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    LOG("Error dispatching parse event to main thread: rv[%x]", rv);
    DecodeError();
    return;
  }
}

void
nsDASHDecoder::OnReadMPDBufferCompleted()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (mShuttingDown) {
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
nsDASHDecoder::ParseMPDBuffer()
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
nsDASHDecoder::CreateRepDecoders()
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

      // Create |nsDASHRepDecoder| objects for each representation.
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

  NS_ENSURE_TRUE(mVideoRepDecoder, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mAudioRepDecoder, NS_ERROR_NOT_INITIALIZED);

  return NS_OK;
}

nsresult
nsDASHDecoder::CreateAudioRepDecoder(nsIURI* aUrl,
                                     mozilla::net::Representation const * aRep)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_ARG(aUrl);
  NS_ENSURE_ARG(aRep);
  NS_ENSURE_TRUE(mElement, NS_ERROR_NOT_INITIALIZED);

  // Create subdecoder and init with media element.
  nsDASHRepDecoder* audioDecoder = new nsDASHRepDecoder(this);
  NS_ENSURE_TRUE(audioDecoder->Init(mElement), NS_ERROR_NOT_INITIALIZED);

  if (!mAudioRepDecoder) {
    mAudioRepDecoder = audioDecoder;
  }
  mAudioRepDecoders.AppendElement(audioDecoder);

  // Create sub-reader; attach to DASH reader and sub-decoder.
  nsWebMReader* audioReader = new nsWebMReader(audioDecoder);
  if (mDASHReader) {
    mDASHReader->AddAudioReader(audioReader);
  }
  audioDecoder->SetReader(audioReader);

  // Create media resource with URL and connect to sub-decoder.
  MediaResource* audioResource
    = CreateAudioSubResource(aUrl, static_cast<nsMediaDecoder*>(audioDecoder));
  NS_ENSURE_TRUE(audioResource, NS_ERROR_NOT_INITIALIZED);

  audioDecoder->SetResource(audioResource);
  audioDecoder->SetMPDRepresentation(aRep);

  return NS_OK;
}

nsresult
nsDASHDecoder::CreateVideoRepDecoder(nsIURI* aUrl,
                                     mozilla::net::Representation const * aRep)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_ARG(aUrl);
  NS_ENSURE_ARG(aRep);
  NS_ENSURE_TRUE(mElement, NS_ERROR_NOT_INITIALIZED);

  // Create subdecoder and init with media element.
  nsDASHRepDecoder* videoDecoder = new nsDASHRepDecoder(this);
  NS_ENSURE_TRUE(videoDecoder->Init(mElement), NS_ERROR_NOT_INITIALIZED);

  if (!mVideoRepDecoder) {
    mVideoRepDecoder = videoDecoder;
  }
  mVideoRepDecoders.AppendElement(videoDecoder);

  // Create sub-reader; attach to DASH reader and sub-decoder.
  nsWebMReader* videoReader = new nsWebMReader(videoDecoder);
  if (mDASHReader) {
    mDASHReader->AddVideoReader(videoReader);
  }
  videoDecoder->SetReader(videoReader);

  // Create media resource with URL and connect to sub-decoder.
  MediaResource* videoResource
    = CreateVideoSubResource(aUrl, static_cast<nsMediaDecoder*>(videoDecoder));
  NS_ENSURE_TRUE(videoResource, NS_ERROR_NOT_INITIALIZED);

  videoDecoder->SetResource(videoResource);
  videoDecoder->SetMPDRepresentation(aRep);

  return NS_OK;
}

mozilla::MediaResource*
nsDASHDecoder::CreateAudioSubResource(nsIURI* aUrl,
                                      nsMediaDecoder* aAudioDecoder)
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

  return audioResource;
}

mozilla::MediaResource*
nsDASHDecoder::CreateVideoSubResource(nsIURI* aUrl,
                                      nsMediaDecoder* aVideoDecoder)
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

  return videoResource;
}

nsresult
nsDASHDecoder::CreateSubChannel(nsIURI* aUrl, nsIChannel** aChannel)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_ARG(aUrl);

  nsCOMPtr<nsILoadGroup> loadGroup = mElement->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, NS_ERROR_NULL_POINTER);

  // Check for a Content Security Policy to pass down to the channel
  // created to load the media content.
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = mElement->NodePrincipal()->GetCsp(getter_AddRefs(csp));
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
nsDASHDecoder::LoadRepresentations()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  nsresult rv;
  {
    // Hold the lock while we do this to set proper lock ordering
    // expectations for dynamic deadlock detectors: decoder lock(s)
    // should be grabbed before the cache lock.
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

    // Load the decoders for each |Representation|'s media streams.
    if (mAudioRepDecoder) {
      rv = mAudioRepDecoder->Load();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (mVideoRepDecoder) {
      rv = mVideoRepDecoder->Load();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (NS_FAILED(rv)) {
      LOG("Failed to open stream! rv [%x].", rv);
      return rv;
    }
  }

  if (mAudioRepDecoder) {
    mAudioRepDecoder->SetStateMachine(mDecoderStateMachine);
  }
  if (mVideoRepDecoder) {
    mVideoRepDecoder->SetStateMachine(mDecoderStateMachine);
  }

  // Now that subreaders are init'd, it's ok to init state machine.
  return InitializeStateMachine(nullptr);
}

void
nsDASHDecoder::NotifyDownloadEnded(nsDASHRepDecoder* aRepDecoder,
                                   nsresult aStatus,
                                   MediaByteRange &aRange)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

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
    // Return error if |aRepDecoder| does not match current audio/video decoder.
    if (aRepDecoder != mAudioRepDecoder && aRepDecoder != mVideoRepDecoder) {
      LOG("Error! Decoder [%p] does not match current sub-decoders!",
          aRepDecoder);
      DecodeError();
      return;
    }
    LOG("Byte range downloaded: decoder [%p] range requested [%d - %d]",
        aRepDecoder, aRange.mStart, aRange.mEnd);

    // XXX Do Stream Switching here before loading next bytes, e.g.
    // decoder = PossiblySwitchDecoder(aRepDecoder);
    // decoder->LoadNextByteRange();
    aRepDecoder->LoadNextByteRange();
    return;
  } else if (aStatus == NS_BINDING_ABORTED) {
    LOG("MPD download has been cancelled by the user: aStatus [%x].", aStatus);
    if (mElement) {
      mElement->LoadAborted();
    }
    return;
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    LOG("Network error trying to download MPD: aStatus [%x].", aStatus);
    NetworkError();
  }
}

void
nsDASHDecoder::LoadAborted()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (!mNotifiedLoadAborted && mElement) {
    mElement->LoadAborted();
    mNotifiedLoadAborted = true;
    LOG1("Load Aborted! Notifying media element.");
  }
}

void
nsDASHDecoder::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Notify reader of shutdown first.
  if (mDASHReader) {
    mDASHReader->NotifyDecoderShuttingDown();
  }

  // Call parent class shutdown.
  nsBuiltinDecoder::Shutdown();
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
nsDASHDecoder::DecodeError()
{
  if (NS_IsMainThread()) {
    nsBuiltinDecoder::DecodeError();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsBuiltinDecoder::DecodeError);
    nsresult rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      LOG("Error dispatching DecodeError event to main thread: rv[%x]", rv);
    }
  }
}
