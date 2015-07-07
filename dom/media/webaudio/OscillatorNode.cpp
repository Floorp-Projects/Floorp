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

class OscillatorNodeEngine final : public AudioNodeEngine
{
public:
  OscillatorNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(aDestination->Stream())
    , mStart(-1)
    , mStop(STREAM_TIME_MAX)
    // Keep the default values in sync with OscillatorNode::OscillatorNode.
    , mFrequency(440.f)
    , mDetune(0.f)
    , mType(OscillatorType::Sine)
    , mPhase(0.)
    , mRecomputeParameters(true)
    , mCustomLength(0)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mBasicWaveFormCache = aDestination->Context()->GetBasicWaveFormCache();
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
                            TrackRate aSampleRate) override
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

  virtual void SetStreamTimeParameter(uint32_t aIndex, StreamTime aParam) override
  {
    switch (aIndex) {
    case START: mStart = aParam; break;
    case STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad OscillatorNodeEngine StreamTimeParameter");
    }
  }

  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override
  {
    switch (aIndex) {
      case TYPE:
        // Set the new type.
        mType = static_cast<OscillatorType>(aParam);
        if (mType == OscillatorType::Sine) {
          // Forget any previous custom data.
          mCustomLength = 0;
          mCustom = nullptr;
          mPeriodicWave = nullptr;
          mRecomputeParameters = true;
        }
        switch (mType) {
          case OscillatorType::Sine:
            mPhase = 0.0;
            break;
          case OscillatorType::Square:
          case OscillatorType::Triangle:
          case OscillatorType::Sawtooth:
            mPeriodicWave = mBasicWaveFormCache->GetBasicWaveForm(mType);
            break;
          case OscillatorType::Custom:
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

  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer) override
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
    const float twoPiFloat = float(2 * M_PI);
    mPhase += mPhaseIncrement;
    if (mPhase > twoPiFloat) {
      mPhase -= twoPiFloat;
    } else if (mPhase < -twoPiFloat) {
      mPhase += twoPiFloat;
    }
  }

  void UpdateParametersIfNeeded(StreamTime ticks, size_t count)
  {
    double frequency, detune;

    // Shortcut if frequency-related AudioParam are not automated, and we
    // already have computed the frequency information and related parameters.
    if (!ParametersMayNeedUpdate()) {
      return;
    }

    bool simpleFrequency = mFrequency.HasSimpleValue();
    bool simpleDetune = mDetune.HasSimpleValue();

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
    float signalPeriod = mSource->SampleRate() / mFinalFrequency;
    mRecomputeParameters = false;

    mPhaseIncrement = 2 * M_PI / signalPeriod;
  }

  void FillBounds(float* output, StreamTime ticks,
                  uint32_t& start, uint32_t& end)
  {
    MOZ_ASSERT(output);
    static_assert(StreamTime(WEBAUDIO_BLOCK_SIZE) < UINT_MAX,
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

  void ComputeSine(float * aOutput, StreamTime ticks, uint32_t aStart, uint32_t aEnd)
  {
    for (uint32_t i = aStart; i < aEnd; ++i) {
      UpdateParametersIfNeeded(ticks, i);

      aOutput[i] = sin(mPhase);

      IncrementPhase();
    }
  }

  bool ParametersMayNeedUpdate()
  {
    return !mDetune.HasSimpleValue() ||
           !mFrequency.HasSimpleValue() ||
           mRecomputeParameters;
  }

  void ComputeCustom(float* aOutput,
                     StreamTime ticks,
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
    float basePhaseIncrement = mPeriodicWave->rateScale();

    UpdateParametersIfNeeded(ticks, aStart);

    bool parametersMayNeedUpdate = ParametersMayNeedUpdate();
    mPeriodicWave->waveDataForFundamentalFrequency(mFinalFrequency,
                                                   lowerWaveData,
                                                   higherWaveData,
                                                   tableInterpolationFactor);

    for (uint32_t i = aStart; i < aEnd; ++i) {
      if (parametersMayNeedUpdate) {
        mPeriodicWave->waveDataForFundamentalFrequency(mFinalFrequency,
                                                       lowerWaveData,
                                                       higherWaveData,
                                                       tableInterpolationFactor);
        UpdateParametersIfNeeded(ticks, i);
      }
      // Bilinear interpolation between adjacent samples in each table.
      float floorPhase = floorf(mPhase);
      int j1Signed = static_cast<int>(floorPhase);
      uint32_t j1 = j1Signed & indexMask;
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
                            bool* aFinished) override
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    StreamTime ticks = aStream->GetCurrentPosition();
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
    if (ticks + WEBAUDIO_BLOCK_SIZE <= mStart) {
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
      case OscillatorType::Triangle:
      case OscillatorType::Sawtooth:
      case OscillatorType::Custom:
        ComputeCustom(output, ticks, start, end);
        break;
      default:
        ComputeSilence(aOutput);
    };

  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
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

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  StreamTime mStart;
  StreamTime mStop;
  AudioParamTimeline mFrequency;
  AudioParamTimeline mDetune;
  OscillatorType mType;
  float mPhase;
  float mFinalFrequency;
  float mPhaseIncrement;
  bool mRecomputeParameters;
  nsRefPtr<ThreadSharedFloatArrayBufferList> mCustom;
  nsRefPtr<BasicWaveFormCache> mBasicWaveFormCache;
  uint32_t mCustomLength;
  nsRefPtr<WebCore::PeriodicWave> mPeriodicWave;
};

OscillatorNode::OscillatorNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mType(OscillatorType::Sine)
  , mFrequency(new AudioParam(this, SendFrequencyToStream, 440.0f, "frequency"))
  , mDetune(new AudioParam(this, SendDetuneToStream, 0.0f, "detune"))
  , mStartCalled(false)
{
  OscillatorNodeEngine* engine = new OscillatorNodeEngine(this, aContext->Destination());
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::SOURCE_STREAM);
  engine->SetSourceStream(mStream);
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
OscillatorNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return OscillatorNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
OscillatorNode::DestroyMediaStream()
{
  if (mStream) {
    mStream->RemoveMainThreadListener(this);
  }
  AudioNode::DestroyMediaStream();
}

void
OscillatorNode::SendFrequencyToStream(AudioNode* aNode)
{
  OscillatorNode* This = static_cast<OscillatorNode*>(aNode);
  if (!This->mStream) {
    return;
  }
  SendTimelineParameterToStream(This, OscillatorNodeEngine::FREQUENCY, *This->mFrequency);
}

void
OscillatorNode::SendDetuneToStream(AudioNode* aNode)
{
  OscillatorNode* This = static_cast<OscillatorNode*>(aNode);
  if (!This->mStream) {
    return;
  }
  SendTimelineParameterToStream(This, OscillatorNodeEngine::DETUNE, *This->mDetune);
}

void
OscillatorNode::SendTypeToStream()
{
  if (!mStream) {
    return;
  }
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
  MOZ_ASSERT(mStream, "Missing node stream.");
  MOZ_ASSERT(mPeriodicWave, "Send called without PeriodicWave object.");
  SendInt32ParameterToStream(OscillatorNodeEngine::PERIODICWAVE,
                             mPeriodicWave->DataLength());
  nsRefPtr<ThreadSharedFloatArrayBufferList> data =
    mPeriodicWave->GetThreadSharedBuffer();
  mStream->SetBuffer(data.forget());
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

  if (!mStream) {
    // Nothing to play, or we're already dead for some reason
    return;
  }

  // TODO: Perhaps we need to do more here.
  mStream->SetStreamTimeParameter(OscillatorNodeEngine::START,
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

  if (!mStream || !Context()) {
    // We've already stopped and had our stream shut down
    return;
  }

  // TODO: Perhaps we need to do more here.
  mStream->SetStreamTimeParameter(OscillatorNodeEngine::STOP,
                                  Context(), std::max(0.0, aWhen));
}

void
OscillatorNode::NotifyMainThreadStreamFinished()
{
  MOZ_ASSERT(mStream->IsFinished());

  class EndedEventDispatcher final : public nsRunnable
  {
  public:
    explicit EndedEventDispatcher(OscillatorNode* aNode)
      : mNode(aNode) {}
    NS_IMETHOD Run() override
    {
      // If it's not safe to run scripts right now, schedule this to run later
      if (!nsContentUtils::IsSafeToRunScript()) {
        nsContentUtils::AddScriptRunner(this);
        return NS_OK;
      }

      mNode->DispatchTrustedEvent(NS_LITERAL_STRING("ended"));
      // Release stream resources.
      mNode->DestroyMediaStream();
      return NS_OK;
    }
  private:
    nsRefPtr<OscillatorNode> mNode;
  };

  NS_DispatchToMainThread(new EndedEventDispatcher(this));

  // Drop the playing reference
  // Warning: The below line might delete this.
  MarkInactive();
}

}
}
