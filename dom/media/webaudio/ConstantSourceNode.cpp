/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConstantSourceNode.h"

#include "AudioDestinationNode.h"
#include "nsContentUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ConstantSourceNode, AudioScheduledSourceNode,
                                   mOffset)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ConstantSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioScheduledSourceNode)

NS_IMPL_ADDREF_INHERITED(ConstantSourceNode, AudioScheduledSourceNode)
NS_IMPL_RELEASE_INHERITED(ConstantSourceNode, AudioScheduledSourceNode)

class ConstantSourceNodeEngine final : public AudioNodeEngine
{
public:
  ConstantSourceNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(aDestination->Stream())
    , mStart(-1)
    , mStop(STREAM_TIME_MAX)
    // Keep the default values in sync with ConstantSourceNode::ConstantSourceNode.
    , mOffset(1.0f)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameters {
    OFFSET,
    START,
    STOP,
  };
  void RecvTimelineEvent(uint32_t aIndex,
                         AudioTimelineEvent& aEvent) override
  {
    MOZ_ASSERT(mDestination);

    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent,
                                                    mDestination);

    switch (aIndex) {
    case OFFSET:
      mOffset.InsertEvent<int64_t>(aEvent);
      break;
    default:
      NS_ERROR("Bad ConstantSourceNodeEngine TimelineParameter");
    }
  }

  void SetStreamTimeParameter(uint32_t aIndex, StreamTime aParam) override
  {
    switch (aIndex) {
    case START:
      mStart = aParam;
      mSource->SetActive();
      break;
    case STOP: mStop = aParam; break;
    default:
      NS_ERROR("Bad ConstantSourceNodeEngine StreamTimeParameter");
    }
  }

  void ProcessBlock(AudioNodeStream* aStream,
                    GraphTime aFrom,
                    const AudioBlock& aInput,
                    AudioBlock* aOutput,
                    bool* aFinished) override
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    StreamTime ticks = mDestination->GraphTimeToStreamTime(aFrom);
    if (mStart == -1) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    if (ticks + WEBAUDIO_BLOCK_SIZE <= mStart || ticks >= mStop) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    } else {
      aOutput->AllocateChannels(1);
      float* output = aOutput->ChannelFloatsForWrite(0);

      if (mOffset.HasSimpleValue()) {
        for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
          output[i] = mOffset.GetValueAtTime(aFrom, 0);
        }
      } else {
        mOffset.GetValuesAtTime(ticks, output, WEBAUDIO_BLOCK_SIZE);
      }
    }

    if (ticks + WEBAUDIO_BLOCK_SIZE >= mStop) {
      // We've finished playing.
      *aFinished = true;
    }
  }

  bool IsActive() const override
  {
    // start() has been called.
    return mStart != -1;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);

    // Not owned:
    // - mSource
    // - mDestination
    // - mOffset (internal ref owned by node)

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // mSource deletes the engine in its destructor.
  AudioNodeStream* MOZ_NON_OWNING_REF mSource;
  RefPtr<AudioNodeStream> mDestination;
  StreamTime mStart;
  StreamTime mStop;
  AudioParamTimeline mOffset;
};

ConstantSourceNode::ConstantSourceNode(AudioContext* aContext)
  : AudioScheduledSourceNode(aContext,
                             2,
                             ChannelCountMode::Max,
                             ChannelInterpretation::Speakers)
  , mOffset(new AudioParam(this, ConstantSourceNodeEngine::OFFSET,
                           "offset", 1.0f))
  , mStartCalled(false)
{
  ConstantSourceNodeEngine* engine = new ConstantSourceNodeEngine(this, aContext->Destination());
  mStream = AudioNodeStream::Create(aContext, engine,
                                    AudioNodeStream::NEED_MAIN_THREAD_FINISHED,
                                    aContext->Graph());
  engine->SetSourceStream(mStream);
  mStream->AddMainThreadListener(this);
}

ConstantSourceNode::~ConstantSourceNode()
{
}

size_t
ConstantSourceNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);

  amount += mOffset->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
ConstantSourceNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
ConstantSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ConstantSourceNode_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<ConstantSourceNode>
ConstantSourceNode::Constructor(const GlobalObject& aGlobal,
                                AudioContext& aContext,
                                const ConstantSourceOptions& aOptions,
                                ErrorResult& aRv)
{
  RefPtr<ConstantSourceNode> object = new ConstantSourceNode(&aContext);
  object->mOffset->SetValue(aOptions.mOffset);
  return object.forget();
}

void
ConstantSourceNode::DestroyMediaStream()
{
  if (mStream) {
    mStream->RemoveMainThreadListener(this);
  }
  AudioNode::DestroyMediaStream();
}

void
ConstantSourceNode::Start(double aWhen, ErrorResult& aRv)
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
    return;
  }

  mStream->SetStreamTimeParameter(ConstantSourceNodeEngine::START,
                                  Context(), aWhen);

  MarkActive();
}

void
ConstantSourceNode::Stop(double aWhen, ErrorResult& aRv)
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
    return;
  }

  mStream->SetStreamTimeParameter(ConstantSourceNodeEngine::STOP,
                                  Context(), std::max(0.0, aWhen));
}

void
ConstantSourceNode::NotifyMainThreadStreamFinished()
{
  MOZ_ASSERT(mStream->IsFinished());

  class EndedEventDispatcher final : public Runnable
  {
  public:
    explicit EndedEventDispatcher(ConstantSourceNode* aNode)
      : mozilla::Runnable("EndedEventDispatcher")
      , mNode(aNode)
    {
    }
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
    RefPtr<ConstantSourceNode> mNode;
  };

  Context()->Dispatch(do_AddRef(new EndedEventDispatcher(this)));

  // Drop the playing reference
  // Warning: The below line might delete this.
  MarkInactive();
}

} // namespace dom
} // namespace mozilla
