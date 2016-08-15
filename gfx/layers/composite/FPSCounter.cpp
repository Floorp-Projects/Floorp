/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>                     // for size_t
#include "Units.h"                      // for ScreenIntRect
#include "gfxRect.h"                    // for gfxRect
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color, SurfaceFormat
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsIFile.h"                    // for nsIFile
#include "nsDirectoryServiceDefs.h"     // for NS_OS_TMP_DIR
#include "mozilla/Sprintf.h"
#include "FPSCounter.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

FPSCounter::FPSCounter(const char* aName)
  : mWriteIndex(0)
  , mIteratorIndex(-1)
  , mFPSName(aName)
{
  Init();
}

FPSCounter::~FPSCounter() { }

void
FPSCounter::Init()
{
  for (int i = 0; i < kMaxFrames; i++) {
    mFrameTimestamps.AppendElement(TimeStamp());
  }
  mLastInterval = TimeStamp::Now();
}

// Returns true if we captured a full interval of data
bool
FPSCounter::CapturedFullInterval(TimeStamp aTimestamp) {
  TimeDuration duration = aTimestamp - mLastInterval;
  return duration.ToSeconds() >= kFpsDumpInterval;
}

void
FPSCounter::AddFrame(TimeStamp aTimestamp) {
  NS_ASSERTION(mWriteIndex < kMaxFrames, "We probably have a bug with the circular buffer");
  NS_ASSERTION(mWriteIndex >= 0, "Circular Buffer index should never be negative");

  int index = mWriteIndex++;
  if (mWriteIndex == kMaxFrames) {
    mWriteIndex = 0;
  }

  mFrameTimestamps[index] = aTimestamp;

  if (CapturedFullInterval(aTimestamp)) {
    PrintFPS();
    WriteFrameTimeStamps();
    mLastInterval = aTimestamp;
  }
}

double
FPSCounter::AddFrameAndGetFps(TimeStamp aTimestamp) {
  AddFrame(aTimestamp);
  return GetFPS(aTimestamp);
}

int
FPSCounter::GetLatestReadIndex()
{
  if (mWriteIndex == 0) {
    return kMaxFrames - 1;
  }

  return mWriteIndex - 1;
}

TimeStamp
FPSCounter::GetLatestTimeStamp()
{
  TimeStamp timestamp = mFrameTimestamps[GetLatestReadIndex()];
  MOZ_ASSERT(!timestamp.IsNull(), "Cannot use null timestamps");
  return timestamp;
}

// Returns true if we iterated over a full interval of data
bool
FPSCounter::IteratedFullInterval(TimeStamp aTimestamp, double aDuration) {
  MOZ_ASSERT(mIteratorIndex >= 0, "Cannot be negative");
  MOZ_ASSERT(mIteratorIndex < kMaxFrames, "Iterator index cannot be greater than kMaxFrames");

  TimeStamp currentStamp = mFrameTimestamps[mIteratorIndex];
  TimeDuration duration = aTimestamp - currentStamp;
  return duration.ToSeconds() >= aDuration;
}

void
FPSCounter::ResetReverseIterator()
{
  mIteratorIndex = GetLatestReadIndex();
}

/***
 * Returns true if we have another timestamp that is valid and
 * is within the given duration that we're interested in.
 * Duration is in seconds
 */
bool FPSCounter::HasNext(TimeStamp aTimestamp, double aDuration)
{
  // Order of evaluation here has to stay the same
  // otherwise IteratedFullInterval reads from mFrameTimestamps which cannot
  // be null
  return (mIteratorIndex != mWriteIndex) // Didn't loop around the buffer
          && !mFrameTimestamps[mIteratorIndex].IsNull() // valid data
          && !IteratedFullInterval(aTimestamp, aDuration);
}

TimeStamp
FPSCounter::GetNextTimeStamp()
{
  TimeStamp timestamp = mFrameTimestamps[mIteratorIndex--];
  MOZ_ASSERT(!timestamp.IsNull(), "Reading Invalid Timestamp Data");

  if (mIteratorIndex == -1) {
    mIteratorIndex = kMaxFrames - 1;
  }
  return timestamp;
}

/**
 * GetFPS calculates how many frames we've already composited from the current
 * frame timestamp and we iterate from the latest timestamp we recorded,
 * going back in time. When we hit a frame that is longer than the 1 second
 * from the current composited frame, we return how many frames we've counted.
 * Just a visualization:
 *
 *                                 aTimestamp
 * Frames: 1 2 3 4 5 6 7 8 9 10 11 12
 * Time   -------------------------->
 *
 * GetFPS iterates from aTimestamp, which is the current frame.
 * Then starting at frame 12, going back to frame 11, 10, etc, we calculate
 * the duration of the recorded frame timestamp from aTimestamp.
 * Once duration is greater than 1 second, we return how many frames
 * we composited.
 */
double
FPSCounter::GetFPS(TimeStamp aTimestamp)
{
  int frameCount = 0;
  int duration = 1.0; // Only care about the last 1s of data

  ResetReverseIterator();
  while (HasNext(aTimestamp, duration)) {
    GetNextTimeStamp();
    frameCount++;
  }

  return frameCount;
}

// Iterate the same way we do in GetFPS()
int
FPSCounter::BuildHistogram(std::map<int, int>& aFpsData)
{
  TimeStamp currentIntervalStart = GetLatestTimeStamp();
  TimeStamp currentTimeStamp = GetLatestTimeStamp();
  TimeStamp startTimeStamp = GetLatestTimeStamp();

  int frameCount = 0;
  int totalFrameCount = 0;

  ResetReverseIterator();
  while (HasNext(startTimeStamp)) {
    currentTimeStamp = GetNextTimeStamp();
    TimeDuration interval = currentIntervalStart - currentTimeStamp;

    if (interval.ToSeconds() >= 1.0 ) {
      currentIntervalStart = currentTimeStamp;
      aFpsData[frameCount]++;
      frameCount = 0;
    }

    frameCount++;
    totalFrameCount++;
  }

  TimeDuration totalTime = currentIntervalStart - currentTimeStamp;
  printf_stderr("Discarded %d frames over %f ms in histogram for %s\n",
    frameCount, totalTime.ToMilliseconds(), mFPSName);
  return totalFrameCount;
}

// Iterate the same way we do in GetFPS()
void
FPSCounter::WriteFrameTimeStamps(PRFileDesc* fd)
{
  const int bufferSize = 256;
  char buffer[bufferSize];
  int writtenCount = SprintfLiteral(buffer, "FPS Data for: %s\n", mFPSName);
  MOZ_ASSERT(writtenCount >= 0);
  PR_Write(fd, buffer, writtenCount);

  ResetReverseIterator();
  TimeStamp startTimeStamp = GetLatestTimeStamp();

  MOZ_ASSERT(HasNext(startTimeStamp));
  TimeStamp previousSample = GetNextTimeStamp();

  MOZ_ASSERT(HasNext(startTimeStamp));
  TimeStamp nextTimeStamp = GetNextTimeStamp();

  while (HasNext(startTimeStamp)) {
    TimeDuration duration = previousSample - nextTimeStamp;
    writtenCount = SprintfLiteral(buffer, "%f,\n", duration.ToMilliseconds());

    MOZ_ASSERT(writtenCount >= 0);
    PR_Write(fd, buffer, writtenCount);

    previousSample = nextTimeStamp;
    nextTimeStamp = GetNextTimeStamp();
  }
}

double
FPSCounter::GetMean(std::map<int, int> aHistogram)
{
  double average = 0.0;
  double samples = 0.0;

  for (std::map<int, int>::iterator iter = aHistogram.begin();
    iter != aHistogram.end(); ++iter)
  {
    int fps = iter->first;
    int count = iter->second;

    average += fps * count;
    samples += count;
  }

  return average / samples;
}

double
FPSCounter::GetStdDev(std::map<int, int> aHistogram)
{
  double sumOfDifferences = 0;
  double average = GetMean(aHistogram);
  double samples = 0.0;

  for (std::map<int, int>::iterator iter = aHistogram.begin();
    iter != aHistogram.end(); ++iter)
  {
    int fps = iter->first;
    int count = iter->second;

    double diff = ((double) fps) - average;
    diff *= diff;

    for (int i = 0; i < count; i++) {
      sumOfDifferences += diff;
    }
    samples += count;
  }

  double stdDev = sumOfDifferences / samples;
  return sqrt(stdDev);
}

void
FPSCounter::PrintFPS()
{
  if (!gfxPrefs::FPSPrintHistogram()) {
    return;
  }

  std::map<int, int> histogram;
  int totalFrames = BuildHistogram(histogram);

  TimeDuration measurementInterval = mFrameTimestamps[GetLatestReadIndex()] - mLastInterval;
  printf_stderr("FPS for %s. Total Frames: %d Time Interval: %f seconds\n",
                mFPSName, totalFrames, measurementInterval.ToSecondsSigDigits());

  PrintHistogram(histogram);
}

void
FPSCounter::PrintHistogram(std::map<int, int>& aHistogram)
{
  int length = 0;
  const int kBufferLength = 512;
  char buffer[kBufferLength];

  for (std::map<int, int>::iterator iter = aHistogram.begin();
    iter != aHistogram.end(); iter++)
  {
    int fps = iter->first;
    int count = iter->second;

    length += snprintf(buffer + length, kBufferLength - length,
                       "FPS: %d = %d. ", fps, count);
    NS_ASSERTION(length >= kBufferLength, "Buffer overrun while printing FPS histogram.");
  }

  printf_stderr("%s\n", buffer);
  printf_stderr("Mean: %f , std dev %f\n", GetMean(aHistogram), GetStdDev(aHistogram));
}

// Write FPS timestamp data to a file only if
// draw-fps.write-to-file is true
nsresult
FPSCounter::WriteFrameTimeStamps()
{
  if (!gfxPrefs::WriteFPSToFile()) {
    return NS_OK;
  }

  MOZ_ASSERT(mWriteIndex == 0);

  nsCOMPtr<nsIFile> resultFile;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(resultFile));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!strncmp(mFPSName, "Compositor", strlen(mFPSName))) {
    resultFile->Append(NS_LITERAL_STRING("fps.txt"));
  } else {
    resultFile->Append(NS_LITERAL_STRING("txn.txt"));
  }

  PRFileDesc* fd = nullptr;
  int mode = 644;
  int openFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
  rv = resultFile->OpenNSPRFileDesc(openFlags, mode, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  WriteFrameTimeStamps(fd);
  PR_Close(fd);

  nsAutoCString path;
  rv = resultFile->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  printf_stderr("Wrote FPS data to file: %s\n", path.get());
  return NS_OK;
}

FPSState::FPSState()
  : mCompositionFps("Compositor")
  , mTransactionFps("LayerTransactions")
{
}

// Size of the builtin font.
static const float FontHeight = 7.f;
static const float FontWidth = 4.f;

// Scale the font when drawing it to the viewport for better readability.
static const float FontScaleX = 2.f;
static const float FontScaleY = 3.f;

static void DrawDigits(unsigned int aValue,
                       int aOffsetX, int aOffsetY,
                       Compositor* aCompositor,
                       EffectChain& aEffectChain)
{
  if (aValue > 999) {
    aValue = 999;
  }

  unsigned int divisor = 100;
  float textureWidth = FontWidth * 10;
  gfx::Float opacity = 1;
  gfx::Matrix4x4 transform;
  transform.PreScale(FontScaleX, FontScaleY, 1);

  for (size_t n = 0; n < 3; ++n) {
    unsigned int digit = aValue % (divisor * 10) / divisor;
    divisor /= 10;

    RefPtr<TexturedEffect> texturedEffect = static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
    texturedEffect->mTextureCoords = Rect(float(digit * FontWidth) / textureWidth, 0, FontWidth / textureWidth, 1.0f);

    Rect drawRect = Rect(aOffsetX + n * FontWidth, aOffsetY, FontWidth, FontHeight);
    IntRect clipRect = IntRect(0, 0, 300, 100);
    aCompositor->DrawQuad(drawRect, clipRect,
  aEffectChain, opacity, transform);
  }
}

void FPSState::DrawFPS(TimeStamp aNow,
                       int aOffsetX, int aOffsetY,
                       unsigned int aFillRatio,
                       Compositor* aCompositor)
{
  if (!mFPSTextureSource) {
    const char *text =
      "                                        "
      " XXX XX  XXX XXX X X XXX XXX XXX XXX XXX"
      " X X  X    X   X X X X   X     X X X X X"
      " X X  X  XXX XXX XXX XXX XXX   X XXX XXX"
      " X X  X  X     X   X   X X X   X X X   X"
      " XXX XXX XXX XXX   X XXX XXX   X XXX   X"
      "                                        ";

    // Convert the text encoding above to RGBA.
    int w = FontWidth * 10;
    int h = FontHeight;
    uint32_t* buf = (uint32_t *) malloc(w * h * sizeof(uint32_t));
    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j++) {
        uint32_t purple = 0xfff000ff;
        uint32_t white  = 0xffffffff;
        buf[i * w + j] = (text[i * w + j] == ' ') ? purple : white;
      }
    }

   int bytesPerPixel = 4;
    RefPtr<DataSourceSurface> fpsSurface = Factory::CreateWrappingDataSourceSurface(
      reinterpret_cast<uint8_t*>(buf), w * bytesPerPixel, IntSize(w, h), SurfaceFormat::B8G8R8A8);
    mFPSTextureSource = aCompositor->CreateDataTextureSource();
    mFPSTextureSource->Update(fpsSurface);
  }

  EffectChain effectChain;
  effectChain.mPrimaryEffect = CreateTexturedEffect(SurfaceFormat::B8G8R8A8,
                                                    mFPSTextureSource,
                                                    SamplingFilter::POINT,
                                                    true);

  unsigned int fps = unsigned(mCompositionFps.AddFrameAndGetFps(aNow));
  unsigned int txnFps = unsigned(mTransactionFps.GetFPS(aNow));

  DrawDigits(fps, aOffsetX + 0, aOffsetY, aCompositor, effectChain);
  DrawDigits(txnFps, aOffsetX + FontWidth * 4, aOffsetY, aCompositor, effectChain);
  DrawDigits(aFillRatio, aOffsetX + FontWidth * 8, aOffsetY, aCompositor, effectChain);
}

} // end namespace layers
} // end namespace mozilla
