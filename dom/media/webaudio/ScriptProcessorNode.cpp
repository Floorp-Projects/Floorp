/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptProcessorNode.h"
#include "mozilla/dom/ScriptProcessorNodeBinding.h"
#include "AudioBuffer.h"
#include "AudioDestinationNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioProcessingEvent.h"
#include "WebAudioUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"
#include "nsAutoPtr.h"
#include <deque>

namespace mozilla {
namespace dom {

// The maximum latency, in seconds, that we can live with before dropping
// buffers.
static const float MAX_LATENCY_S = 0.5;

NS_IMPL_ISUPPORTS_INHERITED0(ScriptProcessorNode, AudioNode)

// This class manages a queue of output buffers shared between
// the main thread and the Media Stream Graph thread.
class SharedBuffers final
{
private:
  class OutputQueue final
  {
  public:
    explicit OutputQueue(const char* aName)
      : mMutex(aName)
    {}

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
    {
      mMutex.AssertCurrentThreadOwns();

      size_t amount = 0;
      for (size_t i = 0; i < mBufferList.size(); i++) {
        amount += mBufferList[i].SizeOfExcludingThis(aMallocSizeOf, false);
      }

      return amount;
    }

    Mutex& Lock() const { return const_cast<OutputQueue*>(this)->mMutex; }

    size_t ReadyToConsume() const
    {
      // Accessed on both main thread and media graph thread.
      mMutex.AssertCurrentThreadOwns();
      return mBufferList.size();
    }

    // Produce one buffer
    AudioChunk& Produce()
    {
      mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(NS_IsMainThread());
      mBufferList.push_back(AudioChunk());
      return mBufferList.back();
    }

    // Consumes one buffer.
    AudioChunk Consume()
    {
      mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(ReadyToConsume() > 0);
      AudioChunk front = mBufferList.front();
      mBufferList.pop_front();
      return front;
    }

    // Empties the buffer queue.
    void Clear()
    {
      mMutex.AssertCurrentThreadOwns();
      mBufferList.clear();
    }

  private:
    typedef std::deque<AudioChunk> BufferList;

    // Synchronizes access to mBufferList.  Note that it's the responsibility
    // of the callers to perform the required locking, and we assert that every
    // time we access mBufferList.
    Mutex mMutex;
    // The list representing the queue.
    BufferList mBufferList;
  };

public:
  explicit SharedBuffers(float aSampleRate)
    : mOutputQueue("SharedBuffers::outputQueue")
    , mDelaySoFar(STREAM_TIME_MAX)
    , mSampleRate(aSampleRate)
    , mLatency(0.0)
    , mDroppingBuffers(false)
  {
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = aMallocSizeOf(this);

    {
      MutexAutoLock lock(mOutputQueue.Lock());
      amount += mOutputQueue.SizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  // main thread
  void FinishProducingOutputBuffer(ThreadSharedFloatArrayBufferList* aBuffer,
                                   uint32_t aBufferSize)
  {
    MOZ_ASSERT(NS_IsMainThread());

    TimeStamp now = TimeStamp::Now();

    if (mLastEventTime.IsNull()) {
      mLastEventTime = now;
    } else {
      // When main thread blocking has built up enough so
      // |mLatency > MAX_LATENCY_S|, frame dropping starts. It continues until
      // the output buffer is completely empty, at which point the accumulated
      // latency is also reset to 0.
      // It could happen that the output queue becomes empty before the input
      // node has fully caught up. In this case there will be events where
      // |(now - mLastEventTime)| is very short, making mLatency negative.
      // As this happens and the size of |mLatency| becomes greater than
      // MAX_LATENCY_S, frame dropping starts again to maintain an as short
      // output queue as possible.
      float latency = (now - mLastEventTime).ToSeconds();
      float bufferDuration = aBufferSize / mSampleRate;
      mLatency += latency - bufferDuration;
      mLastEventTime = now;
      if (fabs(mLatency) > MAX_LATENCY_S) {
        mDroppingBuffers = true;
      }
    }

    MutexAutoLock lock(mOutputQueue.Lock());
    if (mDroppingBuffers) {
      if (mOutputQueue.ReadyToConsume()) {
        return;
      }
      mDroppingBuffers = false;
      mLatency = 0;
    }

    for (uint32_t offset = 0; offset < aBufferSize; offset += WEBAUDIO_BLOCK_SIZE) {
      AudioChunk& chunk = mOutputQueue.Produce();
      if (aBuffer) {
        chunk.mDuration = WEBAUDIO_BLOCK_SIZE;
        chunk.mBuffer = aBuffer;
        chunk.mChannelData.SetLength(aBuffer->GetChannels());
        for (uint32_t i = 0; i < aBuffer->GetChannels(); ++i) {
          chunk.mChannelData[i] = aBuffer->GetData(i) + offset;
        }
        chunk.mVolume = 1.0f;
        chunk.mBufferFormat = AUDIO_FORMAT_FLOAT32;
      } else {
        chunk.SetNull(WEBAUDIO_BLOCK_SIZE);
      }
    }
  }

  // graph thread
  AudioChunk GetOutputBuffer()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    AudioChunk buffer;

    {
      MutexAutoLock lock(mOutputQueue.Lock());
      if (mOutputQueue.ReadyToConsume() > 0) {
        if (mDelaySoFar == STREAM_TIME_MAX) {
          mDelaySoFar = 0;
        }
        buffer = mOutputQueue.Consume();
      } else {
        // If we're out of buffers to consume, just output silence
        buffer.SetNull(WEBAUDIO_BLOCK_SIZE);
        if (mDelaySoFar != STREAM_TIME_MAX) {
          // Remember the delay that we just hit
          mDelaySoFar += WEBAUDIO_BLOCK_SIZE;
        }
      }
    }

    return buffer;
  }

  StreamTime DelaySoFar() const
  {
    MOZ_ASSERT(!NS_IsMainThread());
    return mDelaySoFar == STREAM_TIME_MAX ? 0 : mDelaySoFar;
  }

  void Reset()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    mDelaySoFar = STREAM_TIME_MAX;
    mLatency = 0.0f;
    {
      MutexAutoLock lock(mOutputQueue.Lock());
      mOutputQueue.Clear();
    }
    mLastEventTime = TimeStamp();
  }

private:
  OutputQueue mOutputQueue;
  // How much delay we've seen so far.  This measures the amount of delay
  // caused by the main thread lagging behind in producing output buffers.
  // STREAM_TIME_MAX means that we have not received our first buffer yet.
  StreamTime mDelaySoFar;
  // The samplerate of the context.
  float mSampleRate;
  // This is the latency caused by the buffering. If this grows too high, we
  // will drop buffers until it is acceptable.
  float mLatency;
  // This is the time at which we last produced a buffer, to detect if the main
  // thread has been blocked.
  TimeStamp mLastEventTime;
  // True if we should be dropping buffers.
  bool mDroppingBuffers;
};

class ScriptProcessorNodeEngine final : public AudioNodeEngine
{
public:
  ScriptProcessorNodeEngine(ScriptProcessorNode* aNode,
                            AudioDestinationNode* aDestination,
                            uint32_t aBufferSize,
                            uint32_t aNumberOfInputChannels)
    : AudioNodeEngine(aNode)
    , mDestination(aDestination->Stream())
    , mSharedBuffers(new SharedBuffers(mDestination->SampleRate()))
    , mBufferSize(aBufferSize)
    , mInputChannelCount(aNumberOfInputChannels)
    , mInputWriteIndex(0)
  {
  }

  SharedBuffers* GetSharedBuffers() const
  {
    return mSharedBuffers;
  }

  enum {
    IS_CONNECTED,
  };

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override
  {
    switch (aIndex) {
      case IS_CONNECTED:
        mIsConnected = aParam;
        break;
      default:
        NS_ERROR("Bad Int32Parameter");
    } // End index switch.
  }

  void ProcessBlock(AudioNodeStream* aStream,
                    GraphTime aFrom,
                    const AudioBlock& aInput,
                    AudioBlock* aOutput,
                    bool* aFinished) override
  {
    // This node is not connected to anything. Per spec, we don't fire the
    // onaudioprocess event. We also want to clear out the input and output
    // buffer queue, and output a null buffer.
    if (!mIsConnected) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      mSharedBuffers->Reset();
      mInputWriteIndex = 0;
      return;
    }

    // The input buffer is allocated lazily when non-null input is received.
    if (!aInput.IsNull() && !mInputBuffer) {
      mInputBuffer = ThreadSharedFloatArrayBufferList::
        Create(mInputChannelCount, mBufferSize, fallible);
      if (mInputBuffer && mInputWriteIndex) {
        // Zero leading for null chunks that were skipped.
        for (uint32_t i = 0; i < mInputChannelCount; ++i) {
          float* channelData = mInputBuffer->GetDataForWrite(i);
          PodZero(channelData, mInputWriteIndex);
        }
      }
    }

    // First, record our input buffer, if its allocation succeeded.
    uint32_t inputChannelCount = mInputBuffer ? mInputBuffer->GetChannels() : 0;
    for (uint32_t i = 0; i < inputChannelCount; ++i) {
      float* writeData = mInputBuffer->GetDataForWrite(i) + mInputWriteIndex;
      if (aInput.IsNull()) {
        PodZero(writeData, aInput.GetDuration());
      } else {
        MOZ_ASSERT(aInput.GetDuration() == WEBAUDIO_BLOCK_SIZE, "sanity check");
        MOZ_ASSERT(aInput.ChannelCount() == inputChannelCount);
        AudioBlockCopyChannelWithScale(static_cast<const float*>(aInput.mChannelData[i]),
                                       aInput.mVolume, writeData);
      }
    }
    mInputWriteIndex += aInput.GetDuration();

    // Now, see if we have data to output
    // Note that we need to do this before sending the buffer to the main
    // thread so that our delay time is updated.
    *aOutput = mSharedBuffers->GetOutputBuffer();

    if (mInputWriteIndex >= mBufferSize) {
      SendBuffersToMainThread(aStream, aFrom);
      mInputWriteIndex -= mBufferSize;
    }
  }

  bool IsActive() const override
  {
    // Could return false when !mIsConnected after all output chunks produced
    // by main thread events calling
    // SharedBuffers::FinishProducingOutputBuffer() have been processed.
    return true;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mDestination (probably)
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mSharedBuffers->SizeOfIncludingThis(aMallocSizeOf);
    if (mInputBuffer) {
      amount += mInputBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  void SendBuffersToMainThread(AudioNodeStream* aStream, GraphTime aFrom)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // we now have a full input buffer ready to be sent to the main thread.
    StreamTime playbackTick = mDestination->GraphTimeToStreamTime(aFrom);
    // Add the duration of the current sample
    playbackTick += WEBAUDIO_BLOCK_SIZE;
    // Add the delay caused by the main thread
    playbackTick += mSharedBuffers->DelaySoFar();
    // Compute the playback time in the coordinate system of the destination
    double playbackTime = mDestination->StreamTimeToSeconds(playbackTick);

    class Command final : public Runnable
    {
    public:
      Command(AudioNodeStream* aStream,
              already_AddRefed<ThreadSharedFloatArrayBufferList> aInputBuffer,
              double aPlaybackTime)
        : mStream(aStream)
        , mInputBuffer(aInputBuffer)
        , mPlaybackTime(aPlaybackTime)
      {
      }

      NS_IMETHOD Run() override
      {
        RefPtr<ThreadSharedFloatArrayBufferList> output;

        auto engine =
          static_cast<ScriptProcessorNodeEngine*>(mStream->Engine());
        {
          auto node = static_cast<ScriptProcessorNode*>
            (engine->NodeMainThread());
          if (!node) {
            return NS_OK;
          }

          if (node->HasListenersFor(nsGkAtoms::onaudioprocess)) {
            output = DispatchAudioProcessEvent(node);
          }
          // The node may have been destroyed during event dispatch.
        }

        // Append it to our output buffer queue
        engine->GetSharedBuffers()->
          FinishProducingOutputBuffer(output, engine->mBufferSize);

        return NS_OK;
      }

      // Returns the output buffers if set in event handlers.
      ThreadSharedFloatArrayBufferList*
        DispatchAudioProcessEvent(ScriptProcessorNode* aNode)
      {
        AudioContext* context = aNode->Context();
        if (!context) {
          return nullptr;
        }

        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(aNode->GetOwner()))) {
          return nullptr;
        }
        JSContext* cx = jsapi.cx();
        uint32_t inputChannelCount = aNode->ChannelCount();

        // Create the input buffer
        RefPtr<AudioBuffer> inputBuffer;
        if (mInputBuffer) {
          ErrorResult rv;
          inputBuffer =
            AudioBuffer::Create(context->GetOwner(), inputChannelCount,
                                aNode->BufferSize(), context->SampleRate(),
                                mInputBuffer.forget(), rv);
          if (rv.Failed()) {
            rv.SuppressException();
            return nullptr;
          }
        }

        // Ask content to produce data in the output buffer
        // Note that we always avoid creating the output buffer here, and we try to
        // avoid creating the input buffer as well.  The AudioProcessingEvent class
        // knows how to lazily create them if needed once the script tries to access
        // them.  Otherwise, we may be able to get away without creating them!
        RefPtr<AudioProcessingEvent> event =
          new AudioProcessingEvent(aNode, nullptr, nullptr);
        event->InitEvent(inputBuffer, inputChannelCount, mPlaybackTime);
        aNode->DispatchTrustedEvent(event);

        // Steal the output buffers if they have been set.
        // Don't create a buffer if it hasn't been used to return output;
        // FinishProducingOutputBuffer() will optimize output = null.
        // GetThreadSharedChannelsForRate() may also return null after OOM.
        if (event->HasOutputBuffer()) {
          ErrorResult rv;
          AudioBuffer* buffer = event->GetOutputBuffer(rv);
          // HasOutputBuffer() returning true means that GetOutputBuffer()
          // will not fail.
          MOZ_ASSERT(!rv.Failed());
          return buffer->GetThreadSharedChannelsForRate(cx);
        }

        return nullptr;
      }
    private:
      RefPtr<AudioNodeStream> mStream;
      RefPtr<ThreadSharedFloatArrayBufferList> mInputBuffer;
      double mPlaybackTime;
    };

    RefPtr<Command> command = new Command(aStream, mInputBuffer.forget(),
                                          playbackTime);
    mAbstractMainThread->Dispatch(command.forget());
  }

  friend class ScriptProcessorNode;

  AudioNodeStream* mDestination;
  nsAutoPtr<SharedBuffers> mSharedBuffers;
  RefPtr<ThreadSharedFloatArrayBufferList> mInputBuffer;
  const uint32_t mBufferSize;
  const uint32_t mInputChannelCount;
  // The write index into the current input buffer
  uint32_t mInputWriteIndex;
  bool mIsConnected = false;
};

ScriptProcessorNode::ScriptProcessorNode(AudioContext* aContext,
                                         uint32_t aBufferSize,
                                         uint32_t aNumberOfInputChannels,
                                         uint32_t aNumberOfOutputChannels)
  : AudioNode(aContext,
              aNumberOfInputChannels,
              mozilla::dom::ChannelCountMode::Explicit,
              mozilla::dom::ChannelInterpretation::Speakers)
  , mBufferSize(aBufferSize ?
                  aBufferSize : // respect what the web developer requested
                  4096)         // choose our own buffer size -- 4KB for now
  , mNumberOfOutputChannels(aNumberOfOutputChannels)
{
  MOZ_ASSERT(BufferSize() % WEBAUDIO_BLOCK_SIZE == 0, "Invalid buffer size");
  ScriptProcessorNodeEngine* engine =
    new ScriptProcessorNodeEngine(this,
                                  aContext->Destination(),
                                  BufferSize(),
                                  aNumberOfInputChannels);
  mStream = AudioNodeStream::Create(aContext, engine,
                                    AudioNodeStream::NO_STREAM_FLAGS,
                                    aContext->Graph());
}

ScriptProcessorNode::~ScriptProcessorNode()
{
}

size_t
ScriptProcessorNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t
ScriptProcessorNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
ScriptProcessorNode::EventListenerAdded(nsIAtom* aType)
{
  AudioNode::EventListenerAdded(aType);
  if (aType == nsGkAtoms::onaudioprocess) {
    UpdateConnectedStatus();
  }
}

void
ScriptProcessorNode::EventListenerRemoved(nsIAtom* aType)
{
  AudioNode::EventListenerRemoved(aType);
  if (aType == nsGkAtoms::onaudioprocess) {
    UpdateConnectedStatus();
  }
}

JSObject*
ScriptProcessorNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ScriptProcessorNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
ScriptProcessorNode::UpdateConnectedStatus()
{
  bool isConnected = mHasPhantomInput ||
    !(OutputNodes().IsEmpty() && OutputParams().IsEmpty()
      && InputNodes().IsEmpty());

  // Events are queued even when there is no listener because a listener
  // may be added while events are in the queue.
  SendInt32ParameterToStream(ScriptProcessorNodeEngine::IS_CONNECTED,
                             isConnected);

  if (isConnected && HasListenersFor(nsGkAtoms::onaudioprocess)) {
    MarkActive();
  } else {
    MarkInactive();
  }
}

} // namespace dom
} // namespace mozilla

