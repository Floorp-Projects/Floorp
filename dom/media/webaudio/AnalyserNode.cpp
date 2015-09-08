/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnalyserNode.h"
#include "mozilla/dom/AnalyserNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"

namespace mozilla {

static const uint32_t MAX_FFT_SIZE = 32768;
static const size_t CHUNK_COUNT = MAX_FFT_SIZE >> WEBAUDIO_BLOCK_SIZE_BITS;
static_assert(MAX_FFT_SIZE == CHUNK_COUNT * WEBAUDIO_BLOCK_SIZE,
              "MAX_FFT_SIZE must be a multiple of WEBAUDIO_BLOCK_SIZE");
static_assert((CHUNK_COUNT & (CHUNK_COUNT - 1)) == 0,
              "CHUNK_COUNT must be power of 2 for remainder behavior");

namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(AnalyserNode, AudioNode)

class AnalyserNodeEngine final : public AudioNodeEngine
{
  class TransferBuffer final : public nsRunnable
  {
  public:
    TransferBuffer(AudioNodeStream* aStream,
                   const AudioChunk& aChunk)
      : mStream(aStream)
      , mChunk(aChunk)
    {
    }

    NS_IMETHOD Run()
    {
      nsRefPtr<AnalyserNode> node =
        static_cast<AnalyserNode*>(mStream->Engine()->NodeMainThread());
      if (node) {
        node->AppendChunk(mChunk);
      }
      return NS_OK;
    }

  private:
    nsRefPtr<AudioNodeStream> mStream;
    AudioChunk mChunk;
  };

public:
  explicit AnalyserNodeEngine(AnalyserNode* aNode)
    : AudioNodeEngine(aNode)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioBlock& aInput,
                            AudioBlock* aOutput,
                            bool* aFinished) override
  {
    *aOutput = aInput;

    if (aInput.IsNull()) {
      // If AnalyserNode::mChunks has only null chunks, then there is no need
      // to send further null chunks.
      if (mChunksToProcess == 0) {
        return;
      }

      --mChunksToProcess;
    } else {
      // This many null chunks will be required to empty AnalyserNode::mChunks.
      mChunksToProcess = CHUNK_COUNT;
    }

    nsRefPtr<TransferBuffer> transfer =
      new TransferBuffer(aStream, aInput.AsAudioChunk());
    NS_DispatchToMainThread(transfer);
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t mChunksToProcess = 0;
};

AnalyserNode::AnalyserNode(AudioContext* aContext)
  : AudioNode(aContext,
              1,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mAnalysisBlock(2048)
  , mMinDecibels(-100.)
  , mMaxDecibels(-30.)
  , mSmoothingTimeConstant(.8)
{
  mStream = AudioNodeStream::Create(aContext,
                                    new AnalyserNodeEngine(this),
                                    AudioNodeStream::NO_STREAM_FLAGS);

  // Enough chunks must be recorded to handle the case of fftSize being
  // increased to maximum immediately before getFloatTimeDomainData() is
  // called, for example.
  unused << mChunks.SetLength(CHUNK_COUNT, fallible);

  AllocateBuffer();
}

size_t
AnalyserNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mAnalysisBlock.SizeOfExcludingThis(aMallocSizeOf);
  amount += mChunks.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mOutputBuffer.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t
AnalyserNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
AnalyserNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AnalyserNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
AnalyserNode::SetFftSize(uint32_t aValue, ErrorResult& aRv)
{
  // Disallow values that are not a power of 2 and outside the [32,32768] range
  if (aValue < 32 ||
      aValue > MAX_FFT_SIZE ||
      (aValue & (aValue - 1)) != 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  if (FftSize() != aValue) {
    mAnalysisBlock.SetFFTSize(aValue);
    AllocateBuffer();
  }
}

void
AnalyserNode::SetMinDecibels(double aValue, ErrorResult& aRv)
{
  if (aValue >= mMaxDecibels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mMinDecibels = aValue;
}

void
AnalyserNode::SetMaxDecibels(double aValue, ErrorResult& aRv)
{
  if (aValue <= mMinDecibels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mMaxDecibels = aValue;
}

void
AnalyserNode::SetSmoothingTimeConstant(double aValue, ErrorResult& aRv)
{
  if (aValue < 0 || aValue > 1) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mSmoothingTimeConstant = aValue;
}

void
AnalyserNode::GetFloatFrequencyData(const Float32Array& aArray)
{
  if (!FFTAnalysis()) {
    // Might fail to allocate memory
    return;
  }

  aArray.ComputeLengthAndData();

  float* buffer = aArray.Data();
  size_t length = std::min(size_t(aArray.Length()), mOutputBuffer.Length());

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = WebAudioUtils::ConvertLinearToDecibels(mOutputBuffer[i], mMinDecibels);
  }
}

void
AnalyserNode::GetByteFrequencyData(const Uint8Array& aArray)
{
  if (!FFTAnalysis()) {
    // Might fail to allocate memory
    return;
  }

  const double rangeScaleFactor = 1.0 / (mMaxDecibels - mMinDecibels);

  aArray.ComputeLengthAndData();

  unsigned char* buffer = aArray.Data();
  size_t length = std::min(size_t(aArray.Length()), mOutputBuffer.Length());

  for (size_t i = 0; i < length; ++i) {
    const double decibels = WebAudioUtils::ConvertLinearToDecibels(mOutputBuffer[i], mMinDecibels);
    // scale down the value to the range of [0, UCHAR_MAX]
    const double scaled = std::max(0.0, std::min(double(UCHAR_MAX),
                                                 UCHAR_MAX * (decibels - mMinDecibels) * rangeScaleFactor));
    buffer[i] = static_cast<unsigned char>(scaled);
  }
}

void
AnalyserNode::GetFloatTimeDomainData(const Float32Array& aArray)
{
  aArray.ComputeLengthAndData();

  float* buffer = aArray.Data();
  size_t length = std::min(aArray.Length(), FftSize());

  GetTimeDomainData(buffer, length);
}

void
AnalyserNode::GetByteTimeDomainData(const Uint8Array& aArray)
{
  aArray.ComputeLengthAndData();

  size_t length = std::min(aArray.Length(), FftSize());

  AlignedTArray<float> tmpBuffer;
  if (!tmpBuffer.SetLength(length, fallible)) {
    return;
  }

  GetTimeDomainData(tmpBuffer.Elements(), length);

  unsigned char* buffer = aArray.Data();
  for (size_t i = 0; i < length; ++i) {
    const float value = tmpBuffer[i];
    // scale the value to the range of [0, UCHAR_MAX]
    const float scaled = std::max(0.0f, std::min(float(UCHAR_MAX),
                                                 128.0f * (value + 1.0f)));
    buffer[i] = static_cast<unsigned char>(scaled);
  }
}

bool
AnalyserNode::FFTAnalysis()
{
  AlignedTArray<float> tmpBuffer;
  size_t fftSize = FftSize();
  if (!tmpBuffer.SetLength(fftSize, fallible)) {
    return false;
  }

  float* inputBuffer = tmpBuffer.Elements();
  GetTimeDomainData(inputBuffer, fftSize);
  ApplyBlackmanWindow(inputBuffer, fftSize);
  mAnalysisBlock.PerformFFT(inputBuffer);

  // Normalize so than an input sine wave at 0dBfs registers as 0dBfs (undo FFT scaling factor).
  const double magnitudeScale = 1.0 / fftSize;

  for (uint32_t i = 0; i < mOutputBuffer.Length(); ++i) {
    double scalarMagnitude = NS_hypot(mAnalysisBlock.RealData(i),
                                      mAnalysisBlock.ImagData(i)) *
                             magnitudeScale;
    mOutputBuffer[i] = mSmoothingTimeConstant * mOutputBuffer[i] +
                       (1.0 - mSmoothingTimeConstant) * scalarMagnitude;
  }

  return true;
}

void
AnalyserNode::ApplyBlackmanWindow(float* aBuffer, uint32_t aSize)
{
  double alpha = 0.16;
  double a0 = 0.5 * (1.0 - alpha);
  double a1 = 0.5;
  double a2 = 0.5 * alpha;

  for (uint32_t i = 0; i < aSize; ++i) {
    double x = double(i) / aSize;
    double window = a0 - a1 * cos(2 * M_PI * x) + a2 * cos(4 * M_PI * x);
    aBuffer[i] *= window;
  }
}

bool
AnalyserNode::AllocateBuffer()
{
  bool result = true;
  if (mOutputBuffer.Length() != FrequencyBinCount()) {
    if (!mOutputBuffer.SetLength(FrequencyBinCount(), fallible)) {
      return false;
    }
    memset(mOutputBuffer.Elements(), 0, sizeof(float) * FrequencyBinCount());
  }
  return result;
}

void
AnalyserNode::AppendChunk(const AudioChunk& aChunk)
{
  if (mChunks.Length() == 0) {
    return;
  }

  ++mCurrentChunk;
  mChunks[mCurrentChunk & (CHUNK_COUNT - 1)] = aChunk;
}

// Reads into aData the oldest aLength samples of the fftSize most recent
// samples.
void
AnalyserNode::GetTimeDomainData(float* aData, size_t aLength)
{
  size_t fftSize = FftSize();
  MOZ_ASSERT(aLength <= fftSize);

  if (mChunks.Length() == 0) {
    PodZero(aData, aLength);
    return;
  }

  size_t readChunk =
    mCurrentChunk - ((fftSize - 1) >> WEBAUDIO_BLOCK_SIZE_BITS);
  size_t readIndex = (0 - fftSize) & (WEBAUDIO_BLOCK_SIZE - 1);
  MOZ_ASSERT(readIndex == 0 || readIndex + fftSize == WEBAUDIO_BLOCK_SIZE);

  for (size_t writeIndex = 0; writeIndex < aLength; ) {
    const AudioChunk& chunk = mChunks[readChunk & (CHUNK_COUNT - 1)];
    const size_t channelCount = chunk.ChannelCount();
    size_t copyLength =
      std::min<size_t>(aLength - writeIndex, WEBAUDIO_BLOCK_SIZE);
    float* dataOut = &aData[writeIndex];

    if (channelCount == 0) {
      PodZero(dataOut, copyLength);
    } else {
      float scale = chunk.mVolume / channelCount;
      { // channel 0
        auto channelData =
          static_cast<const float*>(chunk.mChannelData[0]) + readIndex;
        AudioBufferCopyWithScale(channelData, scale, dataOut, copyLength);
      }
      for (uint32_t i = 1; i < channelCount; ++i) {
        auto channelData =
          static_cast<const float*>(chunk.mChannelData[i]) + readIndex;
        AudioBufferAddWithScale(channelData, scale, dataOut, copyLength);
      }
    }

    readChunk++;
    writeIndex += copyLength;
  }
}

} // namespace dom
} // namespace mozilla

