/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OscillatorNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "blink/PeriodicWave.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(OscillatorNode, AudioNode,
                                   mPeriodicWave, mFrequency, mDetune)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(OscillatorNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(OscillatorNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(OscillatorNode, AudioNode)

static const float sLeakTriangle = 0.995f;
static const float sLeak = 0.999f;

class DCBlocker
{
public:
  // These are sane defauts when the initial mPhase is zero
  explicit DCBlocker(float aLastInput = 0.0f,
            float aLastOutput = 0.0f,
            float aPole = 0.995)
    :mLastInput(aLastInput),
     mLastOutput(aLastOutput),
     mPole(aPole)
  {
    MOZ_ASSERT(aPole > 0);
  }

  inline float Process(float aInput)
  {
    float out;

    out = mLastOutput * mPole + aInput - mLastInput;
    mLastOutput = out;
    mLastInput = aInput;

    return out;
  }
private:
  float mLastInput;
  float mLastOutput;
  float mPole;
};


class OscillatorNodeEngine : public AudioNodeEngine
{
public:
  OscillatorNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    , mStart(-1)
    , mStop(TRACK_TICKS_MAX)
    // Keep the default values in sync with OscillatorNode::OscillatorNode.
    , mFrequency(440.f)
    , mDetune(0.f)
    , mType(OscillatorType::Sine)
    , mPhase(0.)
    // mSquare, mTriangle, and mSaw are not used for default type "sine".
    // They are initialized if and when switching to the OscillatorTypes that
    // use them.
    // mFinalFrequency, mNumberOfHarmonics, mSignalPeriod, mAmplitudeAtZero,
    // mPhaseIncrement, and mPhaseWrap are initialized in
    // UpdateParametersIfNeeded() when mRecomputeParameters is set.
    , mRecomputeParameters(true)
    , mCustomLength(0)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameters {
    FREQUENCY,
    DETUNE,
    TYPE,
    PERIODICWAVE,
    START,
    STOP,
  };
  void SetTimelineParameter(uint32_t aIndex,
                            const AudioParamTimeline& aValue,
                            TrackRate aSampleRate) MOZ_OVERRIDE
  {
    mRecomputeParameters = true;
    switch (aIndex) {
    case FREQUENCY:
      MOZ_ASSERT(mSource && mDestination);
      mFrequency = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mFrequency, mSource, mDestination);
      break;
    case DETUNE:
      MOZ_ASSERT(mSource && mDestination);
      mDetune = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mDetune, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad OscillatorNodeEngine TimelineParameter");
    }
  }

  virtual void SetStreamTimeParameter(uint32_t aIndex, TrackTicks aParam)
  {
    switch (aIndex) {
    case START: mStart = aParam; break;
    case STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad OscillatorNodeEngine StreamTimeParameter");
    }
  }

  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam)
  {
    switch (aIndex) {
      case TYPE:
        // Set the new type.
        mType = static_cast<OscillatorType>(aParam);
        if (mType != OscillatorType::Custom) {
          // Forget any previous custom data.
          mCustomLength = 0;
          mCustom = nullptr;
          mPeriodicWave = nullptr;
          mRecomputeParameters = true;
        }
        // Update BLIT integrators with the new initial conditions.
        switch (mType) {
          case OscillatorType::Sine:
            mPhase = 0.0;
            break;
          case OscillatorType::Square:
            mPhase = 0.0;
            // Initial integration condition is -0.5, because our
            // square has 50% duty cycle.
            mSquare = -0.5;
            break;
          case OscillatorType::Triangle:
            // Initial mPhase and related integration condition so the
            // triangle is in the middle of the first upward slope.
            // XXX actually do the maths and put the right number here.
            mPhase = (float)(M_PI / 2);
            mSquare = 0.5;
            mTriangle = 0.0;
            break;
          case OscillatorType::Sawtooth:
            // Initial mPhase so the oscillator starts at the
            // middle of the ramp, per spec.
            mPhase = (float)(M_PI / 2);
            // mSaw = 0 when mPhase = pi/2.
            mSaw = 0.0;
            break;
          case OscillatorType::Custom:
            // Custom waveforms don't use BLIT.
            break;
          default:
            NS_ERROR("Bad OscillatorNodeEngine type parameter.");
        }
        // End type switch.
        break;
      case PERIODICWAVE:
        MOZ_ASSERT(aParam >= 0, "negative custom array length");
        mCustomLength = static_cast<uint32_t>(aParam);
        break;
      default:
        NS_ERROR("Bad OscillatorNodeEngine Int32Parameter.");
    }
    // End index switch.
  }

  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    MOZ_ASSERT(mCustomLength, "Custom buffer sent before length");
    mCustom = aBuffer;
    MOZ_ASSERT(mCustom->GetChannels() == 2,
               "PeriodicWave should have sent two channels");
    mPeriodicWave = WebCore::PeriodicWave::create(mSource->SampleRate(),
    mCustom->GetData(0), mCustom->GetData(1), mCustomLength);
  }

  void IncrementPhase()
  {
    mPhase += mPhaseIncrement;
    if (mPhase > mPhaseWrap) {
      mPhase -= mPhaseWrap;
    }
  }

  // Square and triangle are using a bipolar band-limited impulse train, saw is
  // using a normal band-limited impulse train.
  bool UsesBipolarBLIT() {
    return mType == OscillatorType::Square || mType == OscillatorType::Triangle;
  }

  void UpdateParametersIfNeeded(TrackTicks ticks, size_t count)
  {
    double frequency, detune;

    bool simpleFrequency = mFrequency.HasSimpleValue();
    bool simpleDetune = mDetune.HasSimpleValue();

    // Shortcut if frequency-related AudioParam are not automated, and we
    // already have computed the frequency information and related parameters.
    if (simpleFrequency && simpleDetune && !mRecomputeParameters) {
      return;
    }

    if (simpleFrequency) {
      frequency = mFrequency.GetValue();
    } else {
      frequency = mFrequency.GetValueAtTime(ticks, count);
    }
    if (simpleDetune) {
      detune = mDetune.GetValue();
    } else {
      detune = mDetune.GetValueAtTime(ticks, count);
    }

    mFinalFrequency = frequency * pow(2., detune / 1200.);
    mRecomputeParameters = false;

    // When using bipolar BLIT, we divide the signal period by two, because we
    // are using two BLIT out of phase.
    mSignalPeriod = UsesBipolarBLIT() ? 0.5 * mSource->SampleRate() / mFinalFrequency
                                      : mSource->SampleRate() / mFinalFrequency;
    // Wrap the phase accordingly:
    mPhaseWrap = UsesBipolarBLIT() || mType == OscillatorType::Sine ? 2 * M_PI
                                   : M_PI;
    // Even number of harmonics for bipolar blit, odd otherwise.
    mNumberOfHarmonics = UsesBipolarBLIT() ? 2 * floor(0.5 * mSignalPeriod)
                                           : 2 * floor(0.5 * mSignalPeriod) + 1;
    mPhaseIncrement = mType == OscillatorType::Sine ? 2 * M_PI / mSignalPeriod
                                                    : M_PI / mSignalPeriod;
    mAmplitudeAtZero = mNumberOfHarmonics / mSignalPeriod;
  }

  void FillBounds(float* output, TrackTicks ticks,
                  uint32_t& start, uint32_t& end)
  {
    MOZ_ASSERT(output);
    static_assert(TrackTicks(WEBAUDIO_BLOCK_SIZE) < UINT_MAX,
        "WEBAUDIO_BLOCK_SIZE overflows interator bounds.");
    start = 0;
    if (ticks < mStart) {
      start = mStart - ticks;
      for (uint32_t i = 0; i < start; ++i) {
        output[i] = 0.0;
      }
    }
    end = WEBAUDIO_BLOCK_SIZE;
    if (ticks + end > mStop) {
      end = mStop - ticks;
      for (uint32_t i = end; i < WEBAUDIO_BLOCK_SIZE; ++i) {
        output[i] = 0.0;
      }
    }
  }

  float BipolarBLIT()
  {
    float blit;
    float denom = sin(mPhase);

    if (fabs(denom) < std::numeric_limits<float>::epsilon()) {
      if (mPhase < 0.1f || mPhase > 2 * M_PI - 0.1f) {
        blit = mAmplitudeAtZero;
      } else {
        blit = -mAmplitudeAtZero;
      }
    } else {
      blit = sin(mNumberOfHarmonics * mPhase);
      blit /= mSignalPeriod * denom;
    }
    return blit;
  }

  float UnipolarBLIT()
  {
    float blit;
    float denom = sin(mPhase);

    if (fabs(denom) <= std::numeric_limits<float>::epsilon()) {
      blit = mAmplitudeAtZero;
    } else {
      blit = sin(mNumberOfHarmonics * mPhase);
      blit /= mSignalPeriod * denom;
    }

    return blit;
  }

  void ComputeSine(float * aOutput, TrackTicks ticks, uint32_t aStart, uint32_t aEnd)
  {
    for (uint32_t i = aStart; i < aEnd; ++i) {
      UpdateParametersIfNeeded(ticks, i);

      aOutput[i] = sin(mPhase);

      IncrementPhase();
    }
  }

  void ComputeSquare(float * aOutput, TrackTicks ticks, uint32_t aStart, uint32_t aEnd)
  {
    for (uint32_t i = aStart; i < aEnd; ++i) {
      UpdateParametersIfNeeded(ticks, i);
      // Integration to get us a square. It turns out we can have a
      // pure integrator here.
      mSquare = mSquare * sLeak + BipolarBLIT();
      aOutput[i] = mSquare;
      // maybe we want to apply a gain, the wg has not decided yet
      aOutput[i] *= 1.5;
      IncrementPhase();
    }
  }

  void ComputeSawtooth(float * aOutput, TrackTicks ticks, uint32_t aStart, uint32_t aEnd)
  {
    float dcoffset;
    for (uint32_t i = aStart; i < aEnd; ++i) {
      UpdateParametersIfNeeded(ticks, i);
      // DC offset so the Saw does not ramp up to infinity when integrating.
      dcoffset = mFinalFrequency / mSource->SampleRate();
      // Integrate and offset so we get mAmplitudeAtZero sawtooth. We have a
      // very low frequency component somewhere here, but I'm not sure where.
      mSaw = mSaw * sLeak + (UnipolarBLIT() - dcoffset);
      // reverse the saw so we are spec compliant
      aOutput[i] = -mSaw * 1.5;

      IncrementPhase();
    }
  }

  void ComputeTriangle(float * aOutput, TrackTicks ticks, uint32_t aStart, uint32_t aEnd)
  {
    for (uint32_t i = aStart; i < aEnd; ++i) {
      UpdateParametersIfNeeded(ticks, i);
      // Integrate to get a square
      mSquare += BipolarBLIT();
      // Leaky integrate to get a triangle. We get too much dc offset if we don't
      // leaky integrate here.
      // C6 = k0 / period
      // (period is samplingrate / frequency, k0 = (PI/2)/(2*PI)) = 0.25
      float C6 = 0.25 / (mSource->SampleRate() / mFinalFrequency);
      mTriangle = mTriangle * sLeakTriangle + mSquare + C6;
      // DC Block, and scale back to [-1.0; 1.0]
      aOutput[i] = mDCBlocker.Process(mTriangle) / (mSignalPeriod/2) * 1.5;

      IncrementPhase();
    }
  }

  void ComputeCustom(float* aOutput,
                     TrackTicks ticks,
                     uint32_t aStart,
                     uint32_t aEnd)
  {
    MOZ_ASSERT(mPeriodicWave, "No custom waveform data");

    uint32_t periodicWaveSize = mPeriodicWave->periodicWaveSize();
    // Mask to wrap wave data indices into the range [0,periodicWaveSize).
    uint32_t indexMask = periodicWaveSize - 1;
    MOZ_ASSERT(periodicWaveSize && (periodicWaveSize & indexMask) == 0,
               "periodicWaveSize must be power of 2");
    float* higherWaveData = nullptr;
    float* lowerWaveData = nullptr;
    float tableInterpolationFactor;
    // Phase increment at frequency of 1 Hz.
    // mPhase runs [0,periodicWaveSize) here instead of [0,2*M_PI).
    float basePhaseIncrement =
      static_cast<float>(periodicWaveSize) / mSource->SampleRate();

    for (uint32_t i = aStart; i < aEnd; ++i) {
      UpdateParametersIfNeeded(ticks, i);
      mPeriodicWave->waveDataForFundamentalFrequency(mFinalFrequency,
                                                     lowerWaveData,
                                                     higherWaveData,
                                                     tableInterpolationFactor);
      // Bilinear interpolation between adjacent samples in each table.
      float floorPhase = floorf(mPhase);
      uint32_t j1 = floorPhase;
      j1 &= indexMask;
      uint32_t j2 = j1 + 1;
      j2 &= indexMask;

      float sampleInterpolationFactor = mPhase - floorPhase;

      float lower = (1.0f - sampleInterpolationFactor) * lowerWaveData[j1] +
                    sampleInterpolationFactor * lowerWaveData[j2];
      float higher = (1.0f - sampleInterpolationFactor) * higherWaveData[j1] +
                    sampleInterpolationFactor * higherWaveData[j2];
      aOutput[i] = (1.0f - tableInterpolationFactor) * lower +
                   tableInterpolationFactor * higher;

      // Calculate next phase position from wrapped value j1 to avoid loss of
      // precision at large values.
      mPhase =
        j1 + sampleInterpolationFactor + basePhaseIncrement * mFinalFrequency;
    }
  }

  void ComputeSilence(AudioChunk *aOutput)
  {
    aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    TrackTicks ticks = aStream->GetCurrentPosition();
    if (mStart == -1) {
      ComputeSilence(aOutput);
      return;
    }

    if (ticks >= mStop) {
      // We've finished playing.
      ComputeSilence(aOutput);
      *aFinished = true;
      return;
    }
    if (ticks + WEBAUDIO_BLOCK_SIZE < mStart) {
      // We're not playing yet.
      ComputeSilence(aOutput);
      return;
    }

    AllocateAudioBlock(1, aOutput);
    float* output = static_cast<float*>(
        const_cast<void*>(aOutput->mChannelData[0]));

    uint32_t start, end;
    FillBounds(output, ticks, start, end);

    // Synthesize the correct waveform.
    switch(mType) {
      case OscillatorType::Sine:
        ComputeSine(output, ticks, start, end);
        break;
      case OscillatorType::Square:
        ComputeSquare(output, ticks, start, end);
        break;
      case OscillatorType::Triangle:
        ComputeTriangle(output, ticks, start, end);
        break;
      case OscillatorType::Sawtooth:
        ComputeSawtooth(output, ticks, start, end);
        break;
      case OscillatorType::Custom:
        ComputeCustom(output, ticks, start, end);
        break;
      default:
        ComputeSilence(aOutput);
    };

  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);

    // Not owned:
    // - mSource
    // - mDestination
    // - mFrequency (internal ref owned by node)
    // - mDetune (internal ref owned by node)

    if (mCustom) {
      amount += mCustom->SizeOfIncludingThis(aMallocSizeOf);
    }

    if (mPeriodicWave) {
      amount += mPeriodicWave->sizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  DCBlocker mDCBlocker;
  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  TrackTicks mStart;
  TrackTicks mStop;
  AudioParamTimeline mFrequency;
  AudioParamTimeline mDetune;
  OscillatorType mType;
  float mPhase;
  float mFinalFrequency;
  uint32_t mNumberOfHarmonics;
  float mSignalPeriod;
  float mAmplitudeAtZero;
  float mPhaseIncrement;
  float mSquare;
  float mTriangle;
  float mSaw;
  float mPhaseWrap;
  bool mRecomputeParameters;
  nsRefPtr<ThreadSharedFloatArrayBufferList> mCustom;
  uint32_t mCustomLength;
  nsAutoPtr<WebCore::PeriodicWave> mPeriodicWave;
};

OscillatorNode::OscillatorNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mType(OscillatorType::Sine)
  , mFrequency(new AudioParam(MOZ_THIS_IN_INITIALIZER_LIST(),
               SendFrequencyToStream, 440.0f))
  , mDetune(new AudioParam(MOZ_THIS_IN_INITIALIZER_LIST(),
            SendDetuneToStream, 0.0f))
  , mStartCalled(false)
  , mStopped(false)
{
  OscillatorNodeEngine* engine = new OscillatorNodeEngine(this, aContext->Destination());
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::SOURCE_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
  mStream->AddMainThreadListener(this);
}

OscillatorNode::~OscillatorNode()
{
}

size_t
OscillatorNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);

  // For now only report if we know for sure that it's not shared.
  if (mPeriodicWave) {
    amount += mPeriodicWave->SizeOfIncludingThisIfNotShared(aMallocSizeOf);
  }
  amount += mFrequency->SizeOfIncludingThis(aMallocSizeOf);
  amount += mDetune->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
OscillatorNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
OscillatorNode::WrapObject(JSContext* aCx)
{
  return OscillatorNodeBinding::Wrap(aCx, this);
}

void
OscillatorNode::SendFrequencyToStream(AudioNode* aNode)
{
  OscillatorNode* This = static_cast<OscillatorNode*>(aNode);
  SendTimelineParameterToStream(This, OscillatorNodeEngine::FREQUENCY, *This->mFrequency);
}

void
OscillatorNode::SendDetuneToStream(AudioNode* aNode)
{
  OscillatorNode* This = static_cast<OscillatorNode*>(aNode);
  SendTimelineParameterToStream(This, OscillatorNodeEngine::DETUNE, *This->mDetune);
}

void
OscillatorNode::SendTypeToStream()
{
  if (mType == OscillatorType::Custom) {
    // The engine assumes we'll send the custom data before updating the type.
    SendPeriodicWaveToStream();
  }
  SendInt32ParameterToStream(OscillatorNodeEngine::TYPE, static_cast<int32_t>(mType));
}

void OscillatorNode::SendPeriodicWaveToStream()
{
  NS_ASSERTION(mType == OscillatorType::Custom,
               "Sending custom waveform to engine thread with non-custom type");
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "Missing node stream.");
  MOZ_ASSERT(mPeriodicWave, "Send called without PeriodicWave object.");
  SendInt32ParameterToStream(OscillatorNodeEngine::PERIODICWAVE,
                             mPeriodicWave->DataLength());
  nsRefPtr<ThreadSharedFloatArrayBufferList> data =
    mPeriodicWave->GetThreadSharedBuffer();
  ns->SetBuffer(data.forget());
}

void
OscillatorNode::Start(double aWhen, ErrorResult& aRv)
{
  if (!WebAudioUtils::IsTimeValid(aWhen)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  if (mStartCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mStartCalled = true;

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!ns) {
    // Nothing to play, or we're already dead for some reason
    return;
  }

  // TODO: Perhaps we need to do more here.
  ns->SetStreamTimeParameter(OscillatorNodeEngine::START,
                             Context(), aWhen);

  MarkActive();
}

void
OscillatorNode::Stop(double aWhen, ErrorResult& aRv)
{
  if (!WebAudioUtils::IsTimeValid(aWhen)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  if (!mStartCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!ns || !Context()) {
    // We've already stopped and had our stream shut down
    return;
  }

  // TODO: Perhaps we need to do more here.
  ns->SetStreamTimeParameter(OscillatorNodeEngine::STOP,
                             Context(), std::max(0.0, aWhen));
}

void
OscillatorNode::NotifyMainThreadStateChanged()
{
  if (mStream->IsFinished()) {
    class EndedEventDispatcher : public nsRunnable
    {
    public:
      explicit EndedEventDispatcher(OscillatorNode* aNode)
        : mNode(aNode) {}
      NS_IMETHODIMP Run()
      {
        // If it's not safe to run scripts right now, schedule this to run later
        if (!nsContentUtils::IsSafeToRunScript()) {
          nsContentUtils::AddScriptRunner(this);
          return NS_OK;
        }

        mNode->DispatchTrustedEvent(NS_LITERAL_STRING("ended"));
        return NS_OK;
      }
    private:
      nsRefPtr<OscillatorNode> mNode;
    };
    if (!mStopped) {
      // Only dispatch the ended event once
      NS_DispatchToMainThread(new EndedEventDispatcher(this));
      mStopped = true;
    }

    // Drop the playing reference
    // Warning: The below line might delete this.
    MarkInactive();
  }
}

}
}

