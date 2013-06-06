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

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(OscillatorNode, AudioNode,
                                     mPeriodicWave, mFrequency, mDetune)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(OscillatorNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(OscillatorNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(OscillatorNode, AudioNode)

class OscillatorNodeEngine : public AudioNodeEngine
{
public:
  OscillatorNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    , mStart(0)
    , mStop(TRACK_TICKS_MAX)
    // Keep the default value in sync with the default value in OscillatorNode::OscillatorNode.
    , mFrequency(440.f)
    , mDetune(0.f)
    , mType(OscillatorType::Sine)
    , mPhase(0.)
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
    case TYPE: mType = static_cast<OscillatorType>(aParam); break;
    default:
      NS_ERROR("Bad OscillatorNodeEngine Int32Parameter");
    }
  }

  double ComputeFrequency(TrackTicks ticks, size_t count)
  {
    double frequency, detune;
    if (mFrequency.HasSimpleValue()) {
      frequency = mFrequency.GetValue();
    } else {
      frequency = mFrequency.GetValueAtTime(ticks, count);
    }
    if (mDetune.HasSimpleValue()) {
      detune = mDetune.GetValue();
    } else {
      detune = mDetune.GetValueAtTime(ticks, count);
    }
    return frequency * pow(2., detune / 1200.);
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

  void ComputeSine(AudioChunk *aOutput)
  {
    AllocateAudioBlock(1, aOutput);
    float* output = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[0]));

    TrackTicks ticks = mSource->GetCurrentPosition();
    uint32_t start, end;
    FillBounds(output, ticks, start, end);

    double rate = 2.*M_PI / mSource->SampleRate();
    double phase = mPhase;
    for (uint32_t i = start; i < end; ++i) {
      phase += ComputeFrequency(ticks, i) * rate;
      output[i] = sin(phase);
    }
    mPhase = phase;
    while (mPhase > 2.0*M_PI) {
      // Rescale to avoid precision reductions on long runs.
      mPhase -= 2.0*M_PI;
    }
  }

  void ComputeSquare(AudioChunk *aOutput)
  {
    AllocateAudioBlock(1, aOutput);
    float* output = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[0]));

    TrackTicks ticks = mSource->GetCurrentPosition();
    uint32_t start, end;
    FillBounds(output, ticks, start, end);

    double rate = 1.0 / mSource->SampleRate();
    double phase = mPhase;
    for (uint32_t i = start; i < end; ++i) {
      phase += ComputeFrequency(ticks, i) * rate;
      if (phase > 1.0) {
        phase -= 1.0;
      }
      output[i] = phase < 0.5 ? 1.0 : -1.0;
    }
    mPhase = phase;
  }

  void ComputeSawtooth(AudioChunk *aOutput)
  {
    AllocateAudioBlock(1, aOutput);
    float* output = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[0]));

    TrackTicks ticks = mSource->GetCurrentPosition();
    uint32_t start, end;
    FillBounds(output, ticks, start, end);

    double rate = 1.0 / mSource->SampleRate();
    double phase = mPhase;
    for (uint32_t i = start; i < end; ++i) {
      phase += ComputeFrequency(ticks, i) * rate;
      if (phase > 1.0) {
        phase -= 1.0;
      }
      output[i] = phase < 0.5 ? 2.0*phase : 2.0*(phase - 1.0);
    }
    mPhase = phase;
  }

  void ComputeTriangle(AudioChunk *aOutput)
  {
    AllocateAudioBlock(1, aOutput);
    float* output = static_cast<float*>(const_cast<void*>(aOutput->mChannelData[0]));

    TrackTicks ticks = mSource->GetCurrentPosition();
    uint32_t start, end;
    FillBounds(output, ticks, start, end);

    double rate = 1.0 / mSource->SampleRate();
    double phase = mPhase;
    for (uint32_t i = start; i < end; ++i) {
      phase += ComputeFrequency(ticks, i) * rate;
      if (phase > 1.0) {
        phase -= 1.0;
      }
      if (phase < 0.25) {
        output[i] = 4.0*phase;
      } else if (phase < 0.75) {
        output[i] = 1.0 - 4.0*(phase - 0.25);
      } else {
        output[i] = 4.0*(phase - 0.75) - 1.0;
      }
    }
    mPhase = phase;
  }

  void ComputeSilence(AudioChunk *aOutput)
  {
    aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    TrackTicks ticks = aStream->GetCurrentPosition();
    if (ticks + WEBAUDIO_BLOCK_SIZE < mStart) {
      // We're not playing yet.
      ComputeSilence(aOutput);
      return;
    }
    if (ticks >= mStop) {
      // We've finished playing.
      ComputeSilence(aOutput);
      *aFinished = true;
      return;
    }
    // Synthesize the correct waveform.
    switch (mType) {
      case OscillatorType::Sine:
        ComputeSine(aOutput);
        break;
      case OscillatorType::Square:
        ComputeSquare(aOutput);
        break;
      case OscillatorType::Sawtooth:
        ComputeSawtooth(aOutput);
        break;
      case OscillatorType::Triangle:
        ComputeTriangle(aOutput);
        break;
      default:
        ComputeSilence(aOutput);
    }
  }

  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  TrackTicks mStart;
  TrackTicks mStop;
  AudioParamTimeline mFrequency;
  AudioParamTimeline mDetune;
  OscillatorType mType;
  double mPhase;
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
}

JSObject*
OscillatorNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return OscillatorNodeBinding::Wrap(aCx, aScope, this);
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
  SendInt32ParameterToStream(OscillatorNodeEngine::TYPE, static_cast<int32_t>(mType));
  if (mType == OscillatorType::Custom) {
    // TODO: Send the custom wave table somehow
  }
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
                             Context()->DestinationStream(),
                             aWhen);

  MOZ_ASSERT(!mPlayingRef, "We can only accept a successful start() call once");
  mPlayingRef.Take(this);
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

  mPlayingRef.Drop(this);

  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  if (!ns || !Context()) {
    // We've already stopped and had our stream shut down
    return;
  }

  // TODO: Perhaps we need to do more here.
  ns->SetStreamTimeParameter(OscillatorNodeEngine::STOP,
                             Context()->DestinationStream(),
                             std::max(0.0, aWhen));
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
    mPlayingRef.Drop(this);
  }
}

}
}

