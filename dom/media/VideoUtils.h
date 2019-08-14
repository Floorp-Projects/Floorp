/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoUtils_h
#define VideoUtils_h

#include "AudioSampleFormat.h"
#include "MediaInfo.h"
#include "TimeUnits.h"
#include "VideoLimits.h"
#include "mozilla/gfx/Point.h"  // for gfx::IntSize
#include "mozilla/gfx/Types.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsINamed.h"
#include "nsIThread.h"
#include "nsITimer.h"

#include "nsThreadUtils.h"
#include "prtime.h"

using mozilla::CheckedInt32;
using mozilla::CheckedInt64;
using mozilla::CheckedUint32;
using mozilla::CheckedUint64;

// This file contains stuff we'd rather put elsewhere, but which is
// dependent on other changes which we don't want to wait for. We plan to
// remove this file in the near future.

// This belongs in xpcom/monitor/Monitor.h, once we've made
// mozilla::Monitor non-reentrant.
namespace mozilla {

class MediaContainerType;

// EME Key System String.
#define EME_KEY_SYSTEM_CLEARKEY "org.w3.clearkey"
#define EME_KEY_SYSTEM_WIDEVINE "com.widevine.alpha"

/**
 * ReentrantMonitorConditionallyEnter
 *
 * Enters the supplied monitor only if the conditional value |aEnter| is true.
 * E.g. Used to allow unmonitored read access on the decode thread,
 * and monitored access on all other threads.
 */
class MOZ_STACK_CLASS ReentrantMonitorConditionallyEnter {
 public:
  ReentrantMonitorConditionallyEnter(bool aEnter,
                                     ReentrantMonitor& aReentrantMonitor)
      : mReentrantMonitor(nullptr) {
    MOZ_COUNT_CTOR(ReentrantMonitorConditionallyEnter);
    if (aEnter) {
      mReentrantMonitor = &aReentrantMonitor;
      NS_ASSERTION(mReentrantMonitor, "null monitor");
      mReentrantMonitor->Enter();
    }
  }
  ~ReentrantMonitorConditionallyEnter(void) {
    if (mReentrantMonitor) {
      mReentrantMonitor->Exit();
    }
    MOZ_COUNT_DTOR(ReentrantMonitorConditionallyEnter);
  }

 private:
  // Restrict to constructor and destructor defined above.
  ReentrantMonitorConditionallyEnter();
  ReentrantMonitorConditionallyEnter(const ReentrantMonitorConditionallyEnter&);
  ReentrantMonitorConditionallyEnter& operator=(
      const ReentrantMonitorConditionallyEnter&);
  static void* operator new(size_t) noexcept(true);
  static void operator delete(void*);

  ReentrantMonitor* mReentrantMonitor;
};

// Shuts down a thread asynchronously.
class ShutdownThreadEvent : public Runnable {
 public:
  explicit ShutdownThreadEvent(nsIThread* aThread)
      : Runnable("ShutdownThreadEvent"), mThread(aThread) {}
  ~ShutdownThreadEvent() {}
  NS_IMETHOD Run() override {
    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIThread> mThread;
};

class MediaResource;

// Estimates the buffered ranges of a MediaResource using a simple
// (byteOffset/length)*duration method. Probably inaccurate, but won't
// do file I/O, and can be used when we don't have detailed knowledge
// of the byte->time mapping of a resource. aDurationUsecs is the duration
// of the media in microseconds. Estimated buffered ranges are stored in
// aOutBuffered. Ranges are 0-normalized, i.e. in the range of (0,duration].
media::TimeIntervals GetEstimatedBufferedTimeRanges(
    mozilla::MediaResource* aStream, int64_t aDurationUsecs);

// Converts from number of audio frames (aFrames) to microseconds, given
// the specified audio rate (aRate).
CheckedInt64 FramesToUsecs(int64_t aFrames, uint32_t aRate);
// Converts from number of audio frames (aFrames) TimeUnit, given
// the specified audio rate (aRate).
media::TimeUnit FramesToTimeUnit(int64_t aFrames, uint32_t aRate);
// Perform aValue * aMul / aDiv, reducing the possibility of overflow due to
// aValue * aMul overflowing.
CheckedInt64 SaferMultDiv(int64_t aValue, uint64_t aMul, uint64_t aDiv);

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

// Scales the display rect aDisplay by aspect ratio aAspectRatio.
// Note that aDisplay must be validated by IsValidVideoRegion()
// before being used!
void ScaleDisplayByAspectRatio(gfx::IntSize& aDisplay, float aAspectRatio);

// Downmix Stereo audio samples to Mono.
// Input are the buffer contains stereo data and the number of frames.
void DownmixStereoToMono(mozilla::AudioDataValue* aBuffer, uint32_t aFrames);

// Decide the number of playback channels according to the
// given AudioInfo and the prefs that are being set.
uint32_t DecideAudioPlaybackChannels(const AudioInfo& info);

bool IsDefaultPlaybackDeviceMono();

bool IsVideoContentType(const nsCString& aContentType);

// Returns true if it's safe to use aPicture as the picture to be
// extracted inside a frame of size aFrame, and scaled up to and displayed
// at a size of aDisplay. You should validate the frame, picture, and
// display regions before using them to display video frames.
bool IsValidVideoRegion(const gfx::IntSize& aFrame,
                        const gfx::IntRect& aPicture,
                        const gfx::IntSize& aDisplay);

// Template to automatically set a variable to a value on scope exit.
// Useful for unsetting flags, etc.
template <typename T>
class AutoSetOnScopeExit {
 public:
  AutoSetOnScopeExit(T& aVar, T aValue) : mVar(aVar), mValue(aValue) {}
  ~AutoSetOnScopeExit() { mVar = mValue; }

 private:
  T& mVar;
  const T mValue;
};

enum class MediaThreadType {
  PLAYBACK,          // MediaDecoderStateMachine and MediaFormatReader
  PLATFORM_DECODER,  // MediaDataDecoder
  MSG_CONTROL,
  WEBRTC_DECODER,
  MDSM,
};
// Returns the thread pool that is shared amongst all decoder state machines
// for decoding streams.
already_AddRefed<SharedThreadPool> GetMediaThreadPool(MediaThreadType aType);

enum H264_PROFILE {
  H264_PROFILE_UNKNOWN = 0,
  H264_PROFILE_BASE = 0x42,
  H264_PROFILE_MAIN = 0x4D,
  H264_PROFILE_EXTENDED = 0x58,
  H264_PROFILE_HIGH = 0x64,
};

enum H264_LEVEL {
  H264_LEVEL_1 = 10,
  H264_LEVEL_1_b = 11,
  H264_LEVEL_1_1 = 11,
  H264_LEVEL_1_2 = 12,
  H264_LEVEL_1_3 = 13,
  H264_LEVEL_2 = 20,
  H264_LEVEL_2_1 = 21,
  H264_LEVEL_2_2 = 22,
  H264_LEVEL_3 = 30,
  H264_LEVEL_3_1 = 31,
  H264_LEVEL_3_2 = 32,
  H264_LEVEL_4 = 40,
  H264_LEVEL_4_1 = 41,
  H264_LEVEL_4_2 = 42,
  H264_LEVEL_5 = 50,
  H264_LEVEL_5_1 = 51,
  H264_LEVEL_5_2 = 52
};

// Extracts the H.264/AVC profile and level from an H.264 codecs string.
// H.264 codecs parameters have a type defined as avc1.PPCCLL, where
// PP = profile_idc, CC = constraint_set flags, LL = level_idc.
// See
// http://blog.pearce.org.nz/2013/11/what-does-h264avc1-codecs-parameters.html
// for more details.
// Returns false on failure.
bool ExtractH264CodecDetails(const nsAString& aCodecs, uint8_t& aProfile,
                             uint8_t& aConstraint, uint8_t& aLevel);

struct VideoColorSpace {
  // TODO: Define the value type as strong type enum
  // to better know the exact meaning corresponding to ISO/IEC 23001-8:2016.
  // Default value is listed
  // https://www.webmproject.org/vp9/mp4/#optional-fields
  uint8_t mPrimaryId = 1;   // Table 2
  uint8_t mTransferId = 1;  // Table 3
  uint8_t mMatrixId = 1;    // Table 4
  uint8_t mRangeId = 0;
};

// Extracts the VPX codecs parameter string.
// See https://www.webmproject.org/vp9/mp4/#codecs-parameter-string
// for more details.
// Returns false on failure.
bool ExtractVPXCodecDetails(const nsAString& aCodec, uint8_t& aProfile,
                            uint8_t& aLevel, uint8_t& aBitDepth);
bool ExtractVPXCodecDetails(const nsAString& aCodec, uint8_t& aProfile,
                            uint8_t& aLevel, uint8_t& aBitDepth,
                            uint8_t& aChromaSubsampling,
                            VideoColorSpace& aColorSpace);

// Use a cryptographic quality PRNG to generate raw random bytes
// and convert that to a base64 string.
nsresult GenerateRandomName(nsCString& aOutSalt, uint32_t aLength);

// This version returns a string suitable for use as a file or URL
// path. This is based on code from nsExternalAppHandler::SetUpTempFile.
nsresult GenerateRandomPathName(nsCString& aOutSalt, uint32_t aLength);

already_AddRefed<TaskQueue> CreateMediaDecodeTaskQueue(const char* aName);

// Iteratively invokes aWork until aCondition returns true, or aWork returns
// false. Use this rather than a while loop to avoid bogarting the task queue.
template <class Work, class Condition>
RefPtr<GenericPromise> InvokeUntil(Work aWork, Condition aCondition) {
  RefPtr<GenericPromise::Private> p = new GenericPromise::Private(__func__);

  if (aCondition()) {
    p->Resolve(true, __func__);
  }

  struct Helper {
    static void Iteration(const RefPtr<GenericPromise::Private>& aPromise,
                          Work aLocalWork, Condition aLocalCondition) {
      if (!aLocalWork()) {
        aPromise->Reject(NS_ERROR_FAILURE, __func__);
      } else if (aLocalCondition()) {
        aPromise->Resolve(true, __func__);
      } else {
        nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
            "InvokeUntil::Helper::Iteration",
            [aPromise, aLocalWork, aLocalCondition]() {
              Iteration(aPromise, aLocalWork, aLocalCondition);
            });
        AbstractThread::GetCurrent()->Dispatch(r.forget());
      }
    }
  };

  Helper::Iteration(p, aWork, aCondition);
  return p.forget();
}

// Simple timer to run a runnable after a timeout.
class SimpleTimer : public nsITimerCallback, public nsINamed {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINAMED

  // Create a new timer to run aTask after aTimeoutMs milliseconds
  // on thread aTarget. If aTarget is null, task is run on the main thread.
  static already_AddRefed<SimpleTimer> Create(
      nsIRunnable* aTask, uint32_t aTimeoutMs,
      nsIEventTarget* aTarget = nullptr);
  void Cancel();

  NS_IMETHOD Notify(nsITimer* timer) override;

 private:
  virtual ~SimpleTimer() {}
  nsresult Init(nsIRunnable* aTask, uint32_t aTimeoutMs,
                nsIEventTarget* aTarget);

  RefPtr<nsIRunnable> mTask;
  nsCOMPtr<nsITimer> mTimer;
};

void LogToBrowserConsole(const nsAString& aMsg);

bool ParseMIMETypeString(const nsAString& aMIMEType,
                         nsString& aOutContainerType,
                         nsTArray<nsString>& aOutCodecs);

bool ParseCodecsString(const nsAString& aCodecs,
                       nsTArray<nsString>& aOutCodecs);

bool IsH264CodecString(const nsAString& aCodec);

bool IsAACCodecString(const nsAString& aCodec);

bool IsVP8CodecString(const nsAString& aCodec);

bool IsVP9CodecString(const nsAString& aCodec);

bool IsAV1CodecString(const nsAString& aCodec);

// Try and create a TrackInfo with a given codec MIME type.
UniquePtr<TrackInfo> CreateTrackInfoWithMIMEType(
    const nsACString& aCodecMIMEType);

// Try and create a TrackInfo with a given codec MIME type, and optional extra
// parameters from a container type (its MIME type and codecs are ignored).
UniquePtr<TrackInfo> CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
    const nsACString& aCodecMIMEType, const MediaContainerType& aContainerType);

namespace detail {

// aString should start with aMajor + '/'.
constexpr bool StartsWithMIMETypeMajor(const char* aString, const char* aMajor,
                                       size_t aMajorRemaining) {
  return (aMajorRemaining == 0 && *aString == '/') ||
         (*aString == *aMajor &&
          StartsWithMIMETypeMajor(aString + 1, aMajor + 1,
                                  aMajorRemaining - 1));
}

// aString should only contain [a-z0-9\-\.] and a final '\0'.
constexpr bool EndsWithMIMESubtype(const char* aString, size_t aRemaining) {
  return aRemaining == 0 || (((*aString >= 'a' && *aString <= 'z') ||
                              (*aString >= '0' && *aString <= '9') ||
                              *aString == '-' || *aString == '.') &&
                             EndsWithMIMESubtype(aString + 1, aRemaining - 1));
}

// Simple MIME-type literal string checker with a given (major) type.
// Only accepts "{aMajor}/[a-z0-9\-\.]+".
template <size_t MajorLengthPlus1>
constexpr bool IsMIMETypeWithMajor(const char* aString, size_t aLength,
                                   const char (&aMajor)[MajorLengthPlus1]) {
  return aLength > MajorLengthPlus1 &&  // Major + '/' + at least 1 char
         StartsWithMIMETypeMajor(aString, aMajor, MajorLengthPlus1 - 1) &&
         EndsWithMIMESubtype(aString + MajorLengthPlus1,
                             aLength - MajorLengthPlus1);
}

}  // namespace detail

// Simple MIME-type string checker.
// Only accepts lowercase "{application,audio,video}/[a-z0-9\-\.]+".
// Add more if necessary.
constexpr bool IsMediaMIMEType(const char* aString, size_t aLength) {
  return detail::IsMIMETypeWithMajor(aString, aLength, "application") ||
         detail::IsMIMETypeWithMajor(aString, aLength, "audio") ||
         detail::IsMIMETypeWithMajor(aString, aLength, "video");
}

// Simple MIME-type string literal checker.
// Only accepts lowercase "{application,audio,video}/[a-z0-9\-\.]+".
// Add more if necessary.
template <size_t LengthPlus1>
constexpr bool IsMediaMIMEType(const char (&aString)[LengthPlus1]) {
  return IsMediaMIMEType(aString, LengthPlus1 - 1);
}

// Simple MIME-type string checker.
// Only accepts lowercase "{application,audio,video}/[a-z0-9\-\.]+".
// Add more if necessary.
inline bool IsMediaMIMEType(const nsACString& aString) {
  return IsMediaMIMEType(aString.Data(), aString.Length());
}

enum class StringListRangeEmptyItems {
  // Skip all empty items (empty string will process nothing)
  // E.g.: "a,,b" -> ["a", "b"], "" -> nothing
  Skip,
  // Process all, except if string is empty
  // E.g.: "a,,b" -> ["a", "", "b"], "" -> nothing
  ProcessEmptyItems,
  // Process all, including 1 empty item in an empty string
  // E.g.: "a,,b" -> ["a", "", "b"], "" -> [""]
  ProcessAll
};

template <typename String,
          StringListRangeEmptyItems empties = StringListRangeEmptyItems::Skip>
class StringListRange {
  typedef typename String::char_type CharType;
  typedef const CharType* Pointer;

 public:
  // Iterator into range, trims items and optionally skips empty items.
  class Iterator {
   public:
    bool operator!=(const Iterator& a) const {
      return mStart != a.mStart || mEnd != a.mEnd;
    }
    Iterator& operator++() {
      SearchItemAt(mComma + 1);
      return *this;
    }
    // DereferencedType should be 'const nsDependent[C]String' pointing into
    // mList (which is 'const ns[C]String&').
    typedef decltype(Substring(Pointer(), Pointer())) DereferencedType;
    DereferencedType operator*() { return Substring(mStart, mEnd); }

   private:
    friend class StringListRange;
    Iterator(const CharType* aRangeStart, uint32_t aLength)
        : mRangeEnd(aRangeStart + aLength),
          mStart(nullptr),
          mEnd(nullptr),
          mComma(nullptr) {
      SearchItemAt(aRangeStart);
    }
    void SearchItemAt(Pointer start) {
      // First, skip leading whitespace.
      for (Pointer p = start;; ++p) {
        if (p >= mRangeEnd) {
          if (p > mRangeEnd +
                      (empties != StringListRangeEmptyItems::Skip ? 1 : 0)) {
            p = mRangeEnd +
                (empties != StringListRangeEmptyItems::Skip ? 1 : 0);
          }
          mStart = mEnd = mComma = p;
          return;
        }
        auto c = *p;
        if (c == CharType(',')) {
          // Comma -> Empty item -> Skip or process?
          if (empties != StringListRangeEmptyItems::Skip) {
            mStart = mEnd = mComma = p;
            return;
          }
        } else if (c != CharType(' ')) {
          mStart = p;
          break;
        }
      }
      // Find comma, recording start of trailing space.
      Pointer trailingWhitespace = nullptr;
      for (Pointer p = mStart + 1;; ++p) {
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
  Iterator begin() const {
    return Iterator(
        mList.Data() +
            ((empties == StringListRangeEmptyItems::ProcessEmptyItems &&
              mList.Length() == 0)
                 ? 1
                 : 0),
        mList.Length());
  }
  Iterator end() const {
    return Iterator(mList.Data() + mList.Length() +
                        (empties != StringListRangeEmptyItems::Skip ? 1 : 0),
                    0);
  }

 private:
  const String& mList;
};

template <StringListRangeEmptyItems empties = StringListRangeEmptyItems::Skip,
          typename String>
StringListRange<String, empties> MakeStringListRange(const String& aList) {
  return StringListRange<String, empties>(aList);
}

template <StringListRangeEmptyItems empties = StringListRangeEmptyItems::Skip,
          typename ListString, typename ItemString>
static bool StringListContains(const ListString& aList,
                               const ItemString& aItem) {
  for (const auto& listItem : MakeStringListRange<empties>(aList)) {
    if (listItem.Equals(aItem)) {
      return true;
    }
  }
  return false;
}

inline void AppendStringIfNotEmpty(nsACString& aDest, nsACString&& aSrc) {
  if (!aSrc.IsEmpty()) {
    aDest.Append(NS_LITERAL_CSTRING("\n"));
    aDest.Append(aSrc);
  }
}

// Returns true if we're running on a cellular connection; 2G, 3G, etc.
// Main thread only.
bool OnCellularConnection();

inline gfx::YUVColorSpace DefaultColorSpace(const gfx::IntSize& aSize) {
  return aSize.height < 720 ? gfx::YUVColorSpace::BT601
                            : gfx::YUVColorSpace::BT709;
}

}  // end namespace mozilla

#endif
