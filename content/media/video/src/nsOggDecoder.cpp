/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "prlog.h"
#include "prmem.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsAudioStream.h"
#include "nsChannelReader.h"
#include "nsHTMLVideoElement.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsAutoLock.h"
#include "nsIRenderingContext.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsPresContext.h"
#include "nsOggDecoder.h"

/* 
   The maximum height and width of the video. Used for
   sanitizing the memory allocation of the RGB buffer
*/
#define MAX_VIDEO_WIDTH  2000
#define MAX_VIDEO_HEIGHT 2000

// The number of entries in oggplay buffer list. This value
// is the one used by the oggplay examples.
#define OGGPLAY_BUFFER_SIZE 20

// The number of frames to read before audio callback is called.
// This value is the one used by the oggplay examples.
#define OGGPLAY_FRAMES_PER_CALLBACK 2048

// Offset into Ogg buffer containing audio information. This value
// is the one used by the oggplay examples.
#define OGGPLAY_AUDIO_OFFSET 250L

// Maximum mumber of milliseconds to pause while buffering video data
// on a slow connection. The DATA_WAIT is used to compute the maximum
// amount of data to wait for based on the current bytes/second
// download rate. The TIME_WAIT is the total maximum time to wait in
// case the bytes/second rate slows down.
#define MAX_BUFFERING_DATA_WAIT_MS 15000
#define MAX_BUFFERING_TIME_WAIT_MS 30000

// An Event that uses an nsOggDecoder object. It provides a
// way to inform the event that the decoder object is no longer
// valid, so queued events can safely ignore it.
class nsDecoderEvent : public nsRunnable {
public:
  nsDecoderEvent(nsOggDecoder* decoder) : 
    mLock(nsnull), 
    mDecoder(decoder)
  {
  }

  PRBool Init()
  {
    mLock = PR_NewLock();
    return mLock != nsnull;
  }

  virtual ~nsDecoderEvent()
  {
    if (mLock) {
      PR_DestroyLock(mLock);
      mLock = nsnull;
    }
  }

  void Revoke()
  {
    nsAutoLock lock(mLock);
    mDecoder = nsnull;
  }

  void Lock() 
  {
    PR_Lock(mLock);
  }

  void Unlock() 
  {
    PR_Unlock(mLock);
  }

  NS_IMETHOD Run() {
    nsAutoLock lock(mLock);
    return RunWithLock();
  }

protected:
  virtual nsresult RunWithLock() = 0;

  PRLock* mLock;
  nsOggDecoder* mDecoder;
};

// This handles the Ogg decoding loop. It blocks waiting for
// ogg data, decodes that data, and then goes back to blocking.
// It is synchronised with the nsVideoPresentationEvent internally
// by OggPlay so that it doesn't go to far ahead or behind the
// display of the video frame data.
class nsVideoDecodeEvent : public nsDecoderEvent
{
public:
  nsVideoDecodeEvent(nsOggDecoder* decoder) :
    nsDecoderEvent(decoder)
  {
  }

protected:
  nsresult RunWithLock() 
  {
    if (mDecoder && mDecoder->StepDecoding()) {
      NS_GetCurrentThread()->Dispatch(this, NS_DISPATCH_NORMAL);
    }
    else {
      LOG(PR_LOG_DEBUG, ("Decoding thread completed"));
    }
    return NS_OK;
  }

private:
  // PR_TRUE if we are actively decoding
  PRPackedBool mDecoding;
};

class nsVideoPresentationEvent : public nsDecoderEvent
{
public:
  nsVideoPresentationEvent(nsOggDecoder* decoder) :
    nsDecoderEvent(decoder)
  {
  }

  // Stop the invalidation timer. When we aren't decoding
  // video frames we stop the timer since it takes a fair
  // amount of CPU on some platforms.
  void StopInvalidating()
  {
    if (mDecoder) {
      // Stop the invalidation timer
      nsCOMPtr<nsIRunnable> event = 
        NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, StopInvalidating); 
      
      if (event) {
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }
    }
  }
  
  nsresult RunWithLock() {
    if (mDecoder && !mDecoder->IsPaused() && mDecoder->StepDisplay()) {
      NS_GetCurrentThread()->Dispatch(this, NS_DISPATCH_NORMAL);
    }
    else {
      LOG(PR_LOG_DEBUG, ("Presentation thread completed"));
      StopInvalidating();
    }
    return NS_OK;
  }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsOggDecoder, nsIObserver)

void nsOggDecoder::Pause() 
{
  if (!mPresentationThread)
    return;

  mPaused = PR_TRUE;
  nsCOMPtr<nsIRunnable> event = 
    NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, DoPause); 
  if (event)
    mPresentationThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

void nsOggDecoder::DoPause() 
{
  mPaused = PR_TRUE;
  if (mAudioStream) {
    mAudioStream->Pause();
  }
  mSystemSyncSeconds = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
}

float nsOggDecoder::GetVolume()
{
  float volume = 0.0;
  if (mAudioStream) {
    mAudioStream->GetVolume(&volume);
  }
  else {
    volume = mInitialVolume;
  }

  return volume;
}

void nsOggDecoder::SetVolume(float volume)
{
  if (mAudioStream) {
    mAudioStream->SetVolume(volume);
  }
  else {
    mInitialVolume = volume;
  }
}

float nsOggDecoder::GetDuration()
{
  // Currently not implemented. Video Spec says to return
  // NaN if unknown.
  // TODO: return NaN
  return 0.0;
}

nsOggDecoder::nsOggDecoder() :
  nsVideoDecoder(),
  mBytesDownloaded(0),
  mVideoNextFrameTime(0.0),
  mLoadInProgress(PR_FALSE),
  mPlayAfterLoad(PR_FALSE),
  mNotifyOnShutdown(PR_FALSE),
  mVideoCurrentFrameTime(0.0),
  mInitialVolume(1.0),
  mAudioRate(0),
  mAudioChannels(0),
  mAudioTrack(-1),
  mVideoTrack(-1),
  mPlayer(0),
  mReader(0),
  mPaused(PR_TRUE),
  mFirstFrameLoaded(PR_FALSE),
  mFirstFrameCondVar(nsnull),
  mFirstFrameLock(nsnull),
  mSystemSyncSeconds(0.0),
  mResourceLoaded(PR_FALSE),
  mMetadataLoaded(PR_FALSE)
{
}

PRBool nsOggDecoder::Init() 
{
  mFirstFrameLock = PR_NewLock();  
  mFirstFrameCondVar = mFirstFrameLock ? PR_NewCondVar(mFirstFrameLock) : nsnull ;

  mDecodeEvent = new nsVideoDecodeEvent(this);
  mPresentationEvent = new nsVideoPresentationEvent(this);

  return mFirstFrameLock &&
    mFirstFrameCondVar &&
    mDecodeEvent && mDecodeEvent->Init() &&
    mPresentationEvent && mPresentationEvent->Init() &&
    nsVideoDecoder::Init();
}

void nsOggDecoder::Shutdown() 
{
  if (mDecodeEvent) {
    // Must prepare oggplay for close to stop the decode event from
    // waiting on oggplay's semaphore. Without this, revoke deadlocks.
    if (mPlayer) {
      oggplay_prepare_for_close(mPlayer);
    }

    mDecodeEvent->Revoke();
    mDecodeEvent = nsnull;
  }
  if (mPresentationEvent) {
    mPresentationEvent->Revoke();
    mPresentationEvent = nsnull;
  }

  Stop();
  nsVideoDecoder::Shutdown();
}

nsOggDecoder::~nsOggDecoder()
{
  Shutdown();
  if (mFirstFrameCondVar) {
    PR_DestroyCondVar(mFirstFrameCondVar);
    mFirstFrameCondVar = nsnull;
  }
  if (mFirstFrameLock) {
    PR_DestroyLock(mFirstFrameLock);
    mFirstFrameLock = nsnull;
  }
}

nsIntSize nsOggDecoder::GetVideoSize(nsIntSize defaultSize)
{
  return (mRGBWidth == -1 || mRGBHeight == -1) ? defaultSize : nsIntSize(mRGBWidth, mRGBHeight);
}

double nsOggDecoder::GetVideoFramerate() {
  return mFramerate;
}

PRBool nsOggDecoder::IsPaused()
{
  return mPaused;
}

PRBool nsOggDecoder::StepDecoding()
{
  PRBool stop = PR_TRUE;
  if (mPlayer && mDecodeThread) {
    OggPlayErrorCode r = oggplay_step_decoding(mPlayer);
    if (r != E_OGGPLAY_CONTINUE && 
        r != E_OGGPLAY_USER_INTERRUPT &&
        r != E_OGGPLAY_TIMEOUT) {
      stop = PR_TRUE;
      // Inform the element that we've ended the video
      nsCOMPtr<nsIRunnable> event = 
        NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, PlaybackCompleted); 
      
      if (event) {
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }
    }
    else {
      stop = PR_FALSE;

      if (r == E_OGGPLAY_CONTINUE)
        BufferData();
    }
  }
  return !stop;
}

void nsOggDecoder::BufferData()
{
  if (!mPaused && mReader && mMetadataLoaded && !mResourceLoaded) {    
    double bps = mReader->BytesPerSecond();
    PRUint32 bytes = static_cast<PRUint32>((bps * MAX_BUFFERING_DATA_WAIT_MS)/1000.0);

    // Buffer if it looks like we'll starve for data during
    // MAX_BUFFERING_DATA_WAIT_MS time period.
    if (mReader->Available() < bytes) {
      PRIntervalTime start = PR_IntervalNow();
      if (mElement) {
        nsCOMPtr<nsIRunnable> event = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, BufferingStarted);
        if (event) 
          // The pause event must be processed before the
          // buffering loop occurs. Otherwise the loop exits
          // immediately as it checks to see if the user chose
          // to unpause. 
          NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
      }
      
      // Sleep to allow more data to be downloaded. Note that we are
      // sleeping the decode thread, not the main thread. We stop
      // sleeping when the resource is completely loaded, or the user
      // explicitly chooses to continue playing, or an arbitary time
      // period has elapsed or we've downloaded enough bytes to
      // continue playing at the current download rate.
      while (!mResourceLoaded && 
             mPaused && 
             (PR_IntervalToMilliseconds(PR_IntervalNow() - start) < MAX_BUFFERING_TIME_WAIT_MS) &&
             mReader->Available() < bytes) {
        LOG(PR_LOG_DEBUG, 
            ("Buffering data until %d bytes available or %d milliseconds", 
             (long)(bytes - mReader->Available()),
             MAX_BUFFERING_TIME_WAIT_MS - (PR_IntervalToMilliseconds(PR_IntervalNow() - start))));
        
        PR_Sleep(PR_MillisecondsToInterval(1000));
        
        bps = mReader->BytesPerSecond();       
        bytes = static_cast<PRUint32>((bps * (MAX_BUFFERING_DATA_WAIT_MS))/1000.0);
      }

      if (mElement) {
        nsCOMPtr<nsIRunnable> event = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, BufferingStopped);
        if (event) 
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }
    }
  }
}

nsresult nsOggDecoder::Load(nsIURI* aURI) 
{
  nsresult rv;
  Stop();
  mURI = aURI;

  mReader = new nsChannelReader();
  NS_ENSURE_TRUE(mReader, NS_ERROR_OUT_OF_MEMORY);

  rv = mReader->Init(this, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewThread(getter_AddRefs(mDecodeThread));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewThread(getter_AddRefs(mPresentationThread));
  NS_ENSURE_SUCCESS(rv, rv);

  mLoadInProgress = PR_TRUE;
  nsCOMPtr<nsIRunnable> event = 
    NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, LoadOggHeaders); 
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  rv = mDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  StartProgress();

  return NS_OK;
}

nsresult nsOggDecoder::Play()
{
  mPaused = PR_FALSE;
  if (mLoadInProgress) {
    mPlayAfterLoad = PR_TRUE;
    return NS_OK;
  }
  else if (!mPlayer) {
    Load(mURI);
  }
  else {
    StartPlaybackThreads();
  }

  if (!mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = 
        NS_SUCCEEDED(observerService->AddObserver(this, 
                                                  NS_XPCOM_SHUTDOWN_OBSERVER_ID, 
                                                  PR_FALSE));
    }
    else {
      NS_WARNING("Could not get an observer service. Video decoding events may not shutdown cleanly.");
    }
  }

  return NS_OK;
} 

nsresult nsOggDecoder::Seek(float time)
{
  return NS_ERROR_NOT_IMPLEMENTED;  
}

nsresult nsOggDecoder::PlaybackRateChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsOggDecoder::Stop()
{
  mLoadInProgress = PR_FALSE;
  StopInvalidating();
  StopProgress();
  if (mDecodeThread) {
    if (mPlayer) {
      oggplay_prepare_for_close(mPlayer);
    }
    mDecodeThread->Shutdown();
    mDecodeThread = nsnull;
  }
  if (mPresentationThread) {
    if (!mFirstFrameLoaded) {
      nsAutoLock lock(mFirstFrameLock);
      mFirstFrameLoaded = PR_TRUE;
      PR_NotifyAllCondVar(mFirstFrameCondVar);
    }

    mPresentationThread->Shutdown();
    mPresentationThread = nsnull;
  }
  CloseAudioStream();
  if (mPlayer){
    oggplay_close(mPlayer);
    mPlayer = nsnull;
  }
  mPaused = PR_TRUE;
  mVideoCurrentFrameTime = 0.0;

  if (mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = PR_FALSE;
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  }
}

void nsOggDecoder::HandleVideoData(int track_num, OggPlayVideoData* video_data) {
  int y_width;
  int y_height;
  oggplay_get_video_y_size(mPlayer, track_num, &y_width, &y_height);

  int uv_width;
  int uv_height;
  oggplay_get_video_uv_size(mPlayer, track_num, &uv_width, &uv_height);

  if (y_width >= MAX_VIDEO_WIDTH || y_height >= MAX_VIDEO_HEIGHT)
    return;

  {
    nsAutoLock lock(mVideoUpdateLock);

    SetRGBData(y_width, y_height, mFramerate, nsnull);

    // If there is not enough memory to allocate the RGB buffer,
    // don't display it, but continue without error. When enough
    // memory is available the display will start working again.
    if (mRGB) {
      OggPlayYUVChannels yuv;
      OggPlayRGBChannels rgb;
      
      yuv.ptry = video_data->y;
      yuv.ptru = video_data->u;
      yuv.ptrv = video_data->v;
      yuv.uv_width = uv_width;
      yuv.uv_height = uv_height;
      yuv.y_width = y_width;
      yuv.y_height = y_height;
      
      rgb.ptro = mRGB.get();
      rgb.rgb_width = mRGBWidth;
      rgb.rgb_height = mRGBHeight;

      oggplay_yuv2bgr(&yuv, &rgb);
    }
  }
}

void nsOggDecoder::HandleAudioData(OggPlayAudioData* audio_data, int size) {
  if (mAudioStream) {
    // 'size' is number of samples. Multiply by number of channels
    // to get the actual number of floats being sent.
    nsresult rv = mAudioStream->Write(reinterpret_cast<float*>(audio_data), size * mAudioChannels);
    if (!NS_SUCCEEDED(rv)) {
      LOG(PR_LOG_ERROR, ("Could not write audio data to pipe"));
    }
  }
}

double nsOggDecoder::GetSyncTime() 
{
  double time = 0.0;
  if (mAudioStream && mAudioTrack != -1) {
    mAudioStream->GetTime(&time);
  }
  else {
    // No audio stream, or audio track. Sync to system clock.
    time = 
      (mSystemSyncSeconds == 0.0) ?
      0.0 :
      double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0 - mSystemSyncSeconds;
  }

  return time;
}

void nsOggDecoder::OpenAudioStream()
{
  mAudioStream = new nsAudioStream();
  if (!mAudioStream) {
    LOG(PR_LOG_ERROR, ("Could not create audio stream"));
  }
  else {
    mAudioStream->Init(mAudioChannels, mAudioRate);
    mAudioStream->SetVolume(mInitialVolume);
  }
}

void nsOggDecoder::CloseAudioStream()
{
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }
}

void nsOggDecoder::LoadOggHeaders() 
{
  LOG(PR_LOG_DEBUG, ("Loading Ogg Headers"));
  mPlayer = oggplay_open_with_reader((OggPlayReader*)mReader);
  if (mPlayer) {
    LOG(PR_LOG_DEBUG, ("There are %d tracks", oggplay_get_num_tracks(mPlayer)));

    for (int i = 0; i < oggplay_get_num_tracks(mPlayer); ++i) {
      LOG(PR_LOG_DEBUG, ("Tracks %d: %s", i, oggplay_get_track_typename(mPlayer, i)));
      if (mVideoTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_THEORA) {
        oggplay_set_callback_num_frames(mPlayer, i, 1);
        mVideoTrack = i;
        int fpsd, fpsn;
        oggplay_get_video_fps(mPlayer, i, &fpsd, &fpsn);
        mFramerate = fpsd == 0 ? 0.0 : double(fpsn)/double(fpsd);
        LOG(PR_LOG_DEBUG, ("Frame rate: %f", mFramerate));
      }
      else if (mAudioTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_VORBIS) {
        mAudioTrack = i;
        oggplay_set_offset(mPlayer, i, OGGPLAY_AUDIO_OFFSET);
        oggplay_get_audio_samplerate(mPlayer, i, &mAudioRate);
        oggplay_get_audio_channels(mPlayer, i, &mAudioChannels);
        LOG(PR_LOG_DEBUG, ("samplerate: %d, channels: %d", mAudioRate, mAudioChannels));
      }
      
      if (oggplay_set_track_active(mPlayer, i) < 0) 
        LOG(PR_LOG_ERROR, ("Could not set track %d active", i));
    }
    
    if (mVideoTrack == -1) {
      oggplay_set_callback_num_frames(mPlayer, mAudioTrack, OGGPLAY_FRAMES_PER_CALLBACK);
    }

    oggplay_use_buffer(mPlayer, OGGPLAY_BUFFER_SIZE);

    // Inform the element that we've loaded the Ogg metadata
    nsCOMPtr<nsIRunnable> metadataLoadedEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, MetadataLoaded); 
    
    if (metadataLoadedEvent) {
      NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);
    }

    // Load the first frame of data 
    nsCOMPtr<nsIRunnable> firstFrameEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, LoadFirstFrame); 
    
    if (firstFrameEvent) {
      NS_GetCurrentThread()->Dispatch(firstFrameEvent, NS_DISPATCH_NORMAL);
    }
  }
}

// This is always run in the decoder thread
void nsOggDecoder::LoadFirstFrame() 
{
  if (StepDecoding()) {
    // Inform the element that we've loaded the first frame of data
    nsCOMPtr<nsIRunnable> frameLoadedEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, FirstFrameLoaded); 

    if (frameLoadedEvent) {
      NS_DispatchToMainThread(frameLoadedEvent, NS_DISPATCH_NORMAL);
    }
    nsCOMPtr<nsIRunnable> displayEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, DisplayFirstFrame); 
    if (displayEvent) {
      mPresentationThread->Dispatch(displayEvent, NS_DISPATCH_NORMAL);
    }

    // Notify waiting presentation thread that it can start playing
    {
      nsAutoLock lock(mFirstFrameLock);

      mFirstFrameLoaded = PR_TRUE;
      PR_NotifyAllCondVar(mFirstFrameCondVar);
    }
    mDecodeThread->Dispatch(mDecodeEvent, NS_DISPATCH_NORMAL);
  }
}

void nsOggDecoder::StartPresentationThread() 
{
  {
    nsAutoLock lock(mFirstFrameLock);

    while (!mFirstFrameLoaded)
      PR_WaitCondVar(mFirstFrameCondVar, PR_INTERVAL_NO_TIMEOUT);
  }

  if (mAudioStream) {
    mAudioStream->Resume();
  }
  else {
    OpenAudioStream();
  }
  
  mSystemSyncSeconds = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
  mPresentationThread->Dispatch(mPresentationEvent, NS_DISPATCH_NORMAL);
}
 
float nsOggDecoder::GetCurrentTime()
{
  return mVideoCurrentFrameTime;
}

void nsOggDecoder::GetCurrentURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
}


void nsOggDecoder::DisplayFirstFrame()
{
  // Step through the decoded frames, allowing display of the first frame
  // of video.
  StepDisplay();
}

void nsOggDecoder::ProcessTrack(int aTrackNumber, OggPlayCallbackInfo* aTrackInfo)
{
  OggPlayDataType type = oggplay_callback_info_get_type(aTrackInfo);
  OggPlayDataHeader ** headers = oggplay_callback_info_get_headers(aTrackInfo);
  switch(type) {
  case OGGPLAY_INACTIVE:
    {
      break;
    }
    
  case OGGPLAY_YUV_VIDEO:
    {
      double video_time = ((double)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
      mVideoCurrentFrameTime = video_time;
          
      OggPlayVideoData* video_data = oggplay_callback_info_get_video_data(headers[0]);
      HandleVideoData(aTrackNumber, video_data);
    }
    break;
  case OGGPLAY_FLOATS_AUDIO:
    {
      int required = oggplay_callback_info_get_required(aTrackInfo);
      for (int j = 0; j < required; ++j) {
        int size = oggplay_callback_info_get_record_size(headers[j]);
        OggPlayAudioData* audio_data = oggplay_callback_info_get_audio_data(headers[j]);
        HandleAudioData(audio_data, size);
      }
      break;
    }
  case OGGPLAY_CMML:
    {
      if (oggplay_callback_info_get_required(aTrackInfo) > 0)
        LOG(PR_LOG_DEBUG, ("CMML: %s", oggplay_callback_info_get_text_data(headers[0])));
      break;
    }
  default:
    break;
  }
}

PRBool nsOggDecoder::StepDisplay()
{
  if (!mPlayer/* || !mDecodeThread */) {
    return PR_FALSE;
  }

  int num_tracks = oggplay_get_num_tracks(mPlayer);
  OggPlayCallbackInfo  ** track_info = oggplay_buffer_retrieve_next(mPlayer);

  if (track_info) {
   double audio_time = GetSyncTime();
    PRInt32 millis = PRInt32((mVideoNextFrameTime-audio_time) * 1000.0);      
    // LOG(PR_LOG_DEBUG, ("Sleep: %d %f %f", millis, audio_time, mVideoNextFrameTime));
    mVideoNextFrameTime += 1.0/mFramerate;

    if (millis > 0) {
      PR_Sleep(PR_MillisecondsToInterval(millis));
    }

   if (mVideoTrack != -1 && num_tracks > mVideoTrack)  {
      ProcessTrack(mVideoTrack, track_info[mVideoTrack]);
   }

   if (mAudioTrack != -1 && num_tracks > mAudioTrack) {
      ProcessTrack(mAudioTrack, track_info[mAudioTrack]);
   }

   oggplay_buffer_release(mPlayer, track_info);
  }
  else {
    PR_Sleep(PR_MillisecondsToInterval(10));
  }

  return PR_TRUE;
}

void nsOggDecoder::StartPlaybackThreads()
{
  StartInvalidating(mFramerate);

  nsCOMPtr<nsIRunnable> event = 
    NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, StartPresentationThread); 
  if (event)
    mPresentationThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

void nsOggDecoder::MetadataLoaded()
{
  mMetadataLoaded = PR_TRUE;
  if (mElement) {
    mElement->MetadataLoaded();
  }

  if (mPlayAfterLoad) {
    mPlayAfterLoad = PR_FALSE;
    StartPlaybackThreads();
  }
  mLoadInProgress = PR_FALSE;
}

void nsOggDecoder::FirstFrameLoaded()
{
  StartInvalidating(mFramerate);
  if (mElement) {
    mElement->FirstFrameLoaded();
  }
}

void nsOggDecoder::ResourceLoaded()
{
  mResourceLoaded = PR_TRUE;
  if (mElement) {
    mElement->ResourceLoaded();
  }
  StopProgress();
}

void nsOggDecoder::PlaybackCompleted()
{
  if (mElement) {
    mElement->PlaybackCompleted();
  }
}

NS_IMETHODIMP nsOggDecoder::Observe(nsISupports *aSubjet,
                                      const char *aTopic,
                                      const PRUnichar *someData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }

  return NS_OK;
}

PRUint32 nsOggDecoder::GetBytesLoaded()
{
  return mBytesDownloaded;
}

PRUint32 nsOggDecoder::GetTotalBytes()
{
  // TODO: Need to compute this for ogg files
  return 0;
}

void nsOggDecoder::UpdateBytesDownloaded(PRUint32 aBytes)
{
  mBytesDownloaded = aBytes;
}

void nsOggDecoder::BufferingStopped()
{
  if (mElement) {
    mElement->ChangeReadyState(nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME);
    mElement->Play();
  }
}

void nsOggDecoder::BufferingStarted()
{
  if (mElement) {
    mElement->Pause();
    mElement->ChangeReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE);
  }
}
