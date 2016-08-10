/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoUtils_h
#define VideoUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"

#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsSize.h"
#include "nsRect.h"

#include "nsThreadUtils.h"
#include "prtime.h"
#include "AudioSampleFormat.h"
#include "TimeUnits.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"

using mozilla::CheckedInt64;
using mozilla::CheckedUint64;
using mozilla::CheckedInt32;
using mozilla::CheckedUint32;

// This file contains stuff we'd rather put elsewhere, but which is
// dependent on other changes which we don't want to wait for. We plan to
// remove this file in the near future.


// This belongs in xpcom/monitor/Monitor.h, once we've made
// mozilla::Monitor non-reentrant.
namespace mozilla {

// EME Key System String.
static const char* const kEMEKeySystemClearkey = "org.w3.clearkey";
static const char* const kEMEKeySystemWidevine = "com.widevine.alpha";
static const char* const kEMEKeySystemPrimetime = "com.adobe.primetime";

/**
 * ReentrantMonitorConditionallyEnter
 *
 * Enters the supplied monitor only if the conditional value |aEnter| is true.
 * E.g. Used to allow unmonitored read access on the decode thread,
 * and monitored access on all other threads.
 */
class MOZ_STACK_CLASS ReentrantMonitorConditionallyEnter
{
public:
  ReentrantMonitorConditionallyEnter(bool aEnter,
                                     ReentrantMonitor &aReentrantMonitor) :
    mReentrantMonitor(nullptr)
  {
    MOZ_COUNT_CTOR(ReentrantMonitorConditionallyEnter);
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
    MOZ_COUNT_DTOR(ReentrantMonitorConditionallyEnter);
  }
private:
  // Restrict to constructor and destructor defined above.
  ReentrantMonitorConditionallyEnter();
  ReentrantMonitorConditionallyEnter(const ReentrantMonitorConditionallyEnter&);
  ReentrantMonitorConditionallyEnter& operator =(const ReentrantMonitorConditionallyEnter&);
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);

  ReentrantMonitor* mReentrantMonitor;
};

// Shuts down a thread asynchronously.
class ShutdownThreadEvent : public Runnable
{
public:
  explicit ShutdownThreadEvent(nsIThread* aThread) : mThread(aThread) {}
  ~ShutdownThreadEvent() {}
  NS_IMETHOD Run() override {
    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }
private:
  nsCOMPtr<nsIThread> mThread;
};

template<class T>
class DeleteObjectTask: public Runnable {
public:
  explicit DeleteObjectTask(nsAutoPtr<T>& aObject)
    : mObject(aObject)
  {
  }
  NS_IMETHOD Run() override {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    mObject = nullptr;
    return NS_OK;
  }
private:
  nsAutoPtr<T> mObject;
};

template<class T>
void DeleteOnMainThread(nsAutoPtr<T>& aObject) {
  NS_DispatchToMainThread(new DeleteObjectTask<T>(aObject));
}

class MediaResource;

// Estimates the buffered ranges of a MediaResource using a simple
// (byteOffset/length)*duration method. Probably inaccurate, but won't
// do file I/O, and can be used when we don't have detailed knowledge
// of the byte->time mapping of a resource. aDurationUsecs is the duration
// of the media in microseconds. Estimated buffered ranges are stored in
// aOutBuffered. Ranges are 0-normalized, i.e. in the range of (0,duration].
media::TimeIntervals GetEstimatedBufferedTimeRanges(mozilla::MediaResource* aStream,
                                                    int64_t aDurationUsecs);

// Converts from number of audio frames (aFrames) to microseconds, given
// the specified audio rate (aRate).
CheckedInt64 FramesToUsecs(int64_t aFrames, uint32_t aRate);
// Converts from number of audio frames (aFrames) TimeUnit, given
// the specified audio rate (aRate).
media::TimeUnit FramesToTimeUnit(int64_t aFrames, uint32_t aRate);
// Perform aValue * aMul / aDiv, reducing the possibility of overflow due to
// aValue * aMul overflowing.
CheckedInt64 SaferMultDiv(int64_t aValue, uint32_t aMul, uint32_t aDiv);

// Converts from microseconds (aUsecs) to number of audio frames, given the
// specified audio rate (aRate). Stores the result in aOutFrames. Returns
// true if the operation succeeded, or false if there was an integer
// overflow while calulating the conversion.
CheckedInt64 UsecsToFrames(int64_t aUsecs, uint32_t aRate);

// Format TimeUnit as number of frames at given rate.
CheckedInt64 TimeUnitToFrames(const media::TimeUnit& aTime, uint32_t aRate);

// Converts milliseconds to seconds.
#define MS_TO_SECONDS(ms) ((double)(ms) / (PR_MSEC_PER_SEC))

// Converts seconds to milliseconds.
#define SECONDS_TO_MS(s) ((int)((s) * (PR_MSEC_PER_SEC)))

// Converts from seconds to microseconds. Returns failure if the resulting
// integer is too big to fit in an int64_t.
nsresult SecondsToUsecs(double aSeconds, int64_t& aOutUsecs);

// The maximum height and width of the video. Used for
// sanitizing the memory allocation of the RGB buffer.
// The maximum resolution we anticipate encountering in the
// wild is 2160p (UHD "4K") or 4320p - 7680x4320 pixels for VR.
static const int32_t MAX_VIDEO_WIDTH = 8192;
static const int32_t MAX_VIDEO_HEIGHT = 4608;

// Scales the display rect aDisplay by aspect ratio aAspectRatio.
// Note that aDisplay must be validated by IsValidVideoRegion()
// before being used!
void ScaleDisplayByAspectRatio(nsIntSize& aDisplay, float aAspectRatio);

// Downmix Stereo audio samples to Mono.
// Input are the buffer contains stereo data and the number of frames.
void DownmixStereoToMono(mozilla::AudioDataValue* aBuffer,
                         uint32_t aFrames);

bool IsVideoContentType(const nsCString& aContentType);

// Returns true if it's safe to use aPicture as the picture to be
// extracted inside a frame of size aFrame, and scaled up to and displayed
// at a size of aDisplay. You should validate the frame, picture, and
// display regions before using them to display video frames.
bool IsValidVideoRegion(const nsIntSize& aFrame, const nsIntRect& aPicture,
                        const nsIntSize& aDisplay);

// Template to automatically set a variable to a value on scope exit.
// Useful for unsetting flags, etc.
template<typename T>
class AutoSetOnScopeExit {
public:
  AutoSetOnScopeExit(T& aVar, T aValue)
    : mVar(aVar)
    , mValue(aValue)
  {}
  ~AutoSetOnScopeExit() {
    mVar = mValue;
  }
private:
  T& mVar;
  const T mValue;
};

class SharedThreadPool;

// The MediaDataDecoder API blocks, with implementations waiting on platform
// decoder tasks.  These platform decoder tasks are queued on a separate
// thread pool to ensure they can run when the MediaDataDecoder clients'
// thread pool is blocked.  Tasks on the PLATFORM_DECODER thread pool must not
// wait on tasks in the PLAYBACK thread pool.
//
// No new dependencies on this mechanism should be added, as methods are being
// made async supported by MozPromise, making this unnecessary and
// permitting unifying the pool.
enum class MediaThreadType {
  PLAYBACK, // MediaDecoderStateMachine and MediaDecoderReader
  PLATFORM_DECODER
};
// Returns the thread pool that is shared amongst all decoder state machines
// for decoding streams.
already_AddRefed<SharedThreadPool> GetMediaThreadPool(MediaThreadType aType);

enum H264_PROFILE {
  H264_PROFILE_UNKNOWN                     = 0,
  H264_PROFILE_BASE                        = 0x42,
  H264_PROFILE_MAIN                        = 0x4D,
  H264_PROFILE_EXTENDED                    = 0x58,
  H264_PROFILE_HIGH                        = 0x64,
};

enum H264_LEVEL {
    H264_LEVEL_1         = 10,
    H264_LEVEL_1_b       = 11,
    H264_LEVEL_1_1       = 11,
    H264_LEVEL_1_2       = 12,
    H264_LEVEL_1_3       = 13,
    H264_LEVEL_2         = 20,
    H264_LEVEL_2_1       = 21,
    H264_LEVEL_2_2       = 22,
    H264_LEVEL_3         = 30,
    H264_LEVEL_3_1       = 31,
    H264_LEVEL_3_2       = 32,
    H264_LEVEL_4         = 40,
    H264_LEVEL_4_1       = 41,
    H264_LEVEL_4_2       = 42,
    H264_LEVEL_5         = 50,
    H264_LEVEL_5_1       = 51,
    H264_LEVEL_5_2       = 52
};

// Extracts the H.264/AVC profile and level from an H.264 codecs string.
// H.264 codecs parameters have a type defined as avc1.PPCCLL, where
// PP = profile_idc, CC = constraint_set flags, LL = level_idc.
// See http://blog.pearce.org.nz/2013/11/what-does-h264avc1-codecs-parameters.html
// for more details.
// Returns false on failure.
bool
ExtractH264CodecDetails(const nsAString& aCodecs,
                        int16_t& aProfile,
                        int16_t& aLevel);

// Use a cryptographic quality PRNG to generate raw random bytes
// and convert that to a base64 string.
nsresult
GenerateRandomName(nsCString& aOutSalt, uint32_t aLength);

// This version returns a string suitable for use as a file or URL
// path. This is based on code from nsExternalAppHandler::SetUpTempFile.
nsresult
GenerateRandomPathName(nsCString& aOutSalt, uint32_t aLength);

already_AddRefed<TaskQueue>
CreateMediaDecodeTaskQueue();

// Iteratively invokes aWork until aCondition returns true, or aWork returns false.
// Use this rather than a while loop to avoid bogarting the task queue.
template<class Work, class Condition>
RefPtr<GenericPromise> InvokeUntil(Work aWork, Condition aCondition) {
  RefPtr<GenericPromise::Private> p = new GenericPromise::Private(__func__);

  if (aCondition()) {
    p->Resolve(true, __func__);
  }

  struct Helper {
    static void Iteration(RefPtr<GenericPromise::Private> aPromise, Work aLocalWork, Condition aLocalCondition) {
      if (!aLocalWork()) {
        aPromise->Reject(NS_ERROR_FAILURE, __func__);
      } else if (aLocalCondition()) {
        aPromise->Resolve(true, __func__);
      } else {
        nsCOMPtr<nsIRunnable> r =
          NS_NewRunnableFunction([aPromise, aLocalWork, aLocalCondition] () { Iteration(aPromise, aLocalWork, aLocalCondition); });
        AbstractThread::GetCurrent()->Dispatch(r.forget());
      }
    }
  };

  Helper::Iteration(p, aWork, aCondition);
  return p.forget();
}

// Simple timer to run a runnable after a timeout.
class SimpleTimer : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  // Create a new timer to run aTask after aTimeoutMs milliseconds
  // on thread aTarget. If aTarget is null, task is run on the main thread.
  static already_AddRefed<SimpleTimer> Create(nsIRunnable* aTask,
                                              uint32_t aTimeoutMs,
                                              nsIThread* aTarget = nullptr);
  void Cancel();

  NS_IMETHOD Notify(nsITimer *timer) override;

private:
  virtual ~SimpleTimer() {}
  nsresult Init(nsIRunnable* aTask, uint32_t aTimeoutMs, nsIThread* aTarget);

  RefPtr<nsIRunnable> mTask;
  nsCOMPtr<nsITimer> mTimer;
};

void
LogToBrowserConsole(const nsAString& aMsg);

bool
ParseMIMETypeString(const nsAString& aMIMEType,
                    nsString& aOutContainerType,
                    nsTArray<nsString>& aOutCodecs);

bool
ParseCodecsString(const nsAString& aCodecs, nsTArray<nsString>& aOutCodecs);

bool
IsH264CodecString(const nsAString& aCodec);

bool
IsAACCodecString(const nsAString& aCodec);

bool
IsVP8CodecString(const nsAString& aCodec);

bool
IsVP9CodecString(const nsAString& aCodec);

template <typename String>
class StringListRange
{
  typedef typename String::char_type CharType;
  typedef const CharType* Pointer;

public:
  // Iterator into range, trims items and skips empty items.
  class Iterator
  {
  public:
    bool operator!=(const Iterator& a) const
    {
      return mStart != a.mStart || mEnd != a.mEnd;
    }
    Iterator& operator++()
    {
      SearchItemAt(mComma + 1);
      return *this;
    }
    typedef decltype(Substring(Pointer(), Pointer())) DereferencedType;
    DereferencedType operator*()
    {
      return Substring(mStart, mEnd);
    }
  private:
    friend class StringListRange;
    Iterator(const CharType* aRangeStart, uint32_t aLength)
      : mRangeEnd(aRangeStart + aLength)
    {
      SearchItemAt(aRangeStart);
    }
    void SearchItemAt(Pointer start)
    {
      // First, skip leading whitespace.
      for (Pointer p = start; ; ++p) {
        if (p >= mRangeEnd) {
          mStart = mEnd = mComma = mRangeEnd;
          return;
        }
        auto c = *p;
        if (c == CharType(',')) {
          // Comma -> Empty item -> Skip.
        } else if (c != CharType(' ')) {
          mStart = p;
          break;
        }
      }
      // Find comma, recording start of trailing space.
      Pointer trailingWhitespace = nullptr;
      for (Pointer p = mStart + 1; ; ++p) {
        if (p >= mRangeEnd) {
          mEnd = trailingWhitespace ? trailingWhitespace : p;
          mComma = p;
          return;
        }
        auto c = *p;
        if (c == CharType(',')) {
          mEnd = trailingWhitespace ? trailingWhitespace : p;
          mComma = p;
          return;
        }
        if (c == CharType(' ')) {
          // Found a whitespace -> Record as trailing if not first one.
          if (!trailingWhitespace) {
            trailingWhitespace = p;
          }
        } else {
          // Found a non-whitespace -> Reset trailing whitespace if needed.
          if (trailingWhitespace) {
            trailingWhitespace = nullptr;
          }
        }
      }
    }
    const Pointer mRangeEnd;
    Pointer mStart;
    Pointer mEnd;
    Pointer mComma;
  };

  explicit StringListRange(const String& aList) : mList(aList) {}
  Iterator begin()
  {
    return Iterator(mList.Data(), mList.Length());
  }
  Iterator end()
  {
    return Iterator(mList.Data() + mList.Length(), 0);
  }
private:
  const String& mList;
};

template <typename String>
StringListRange<String>
MakeStringListRange(const String& aList)
{
  return StringListRange<String>(aList);
}

template <typename ListString, typename ItemString>
static bool
StringListContains(const ListString& aList, const ItemString& aItem)
{
  for (const auto& listItem : MakeStringListRange(aList)) {
    if (listItem.Equals(aItem)) {
      return true;
    }
  }
  return false;
}

} // end namespace mozilla

#endif
