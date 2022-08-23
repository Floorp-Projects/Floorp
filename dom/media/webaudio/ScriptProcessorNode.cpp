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
#include "AudioNodeTrack.h"
#include "AudioProcessingEvent.h"
#include "WebAudioUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"
#include <deque>
#include "Tracing.h"

namespace mozilla::dom {

// The maximum latency, in seconds, that we can live with before dropping
// buffers.
static const float MAX_LATENCY_S = 0.5;

// This class manages a queue of output buffers shared between
// the main thread and the Media Track Graph thread.
class SharedBuffers final {
 private:
  class OutputQueue final {
   public:
    explicit OutputQueue(const char* aName) : mMutex(aName) {}

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
        MOZ_REQUIRES(mMutex) {
      mMutex.AssertCurrentThreadOwns();

      size_t amount = 0;
      for (size_t i = 0; i < mBufferList.size(); i++) {
        amount += mBufferList[i].SizeOfExcludingThis(aMallocSizeOf, false);
      }

      return amount;
    }

    Mutex& Lock() const MOZ_RETURN_CAPABILITY(mMutex) {
      return const_cast<OutputQueue*>(this)->mMutex;
    }

    size_t ReadyToConsume() const MOZ_REQUIRES(mMutex) {
      // Accessed on both main thread and media graph thread.
      mMutex.AssertCurrentThreadOwns();
      return mBufferList.size();
    }

    // Produce one buffer
    AudioChunk& Produce() MOZ_REQUIRES(mMutex) {
      mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(NS_IsMainThread());
      mBufferList.push_back(AudioChunk());
      return mBufferList.back();
    }

    // Consumes one buffer.
    AudioChunk Consume() MOZ_REQUIRES(mMutex) {
      mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(ReadyToConsume() > 0);
      AudioChunk front = mBufferList.front();
      mBufferList.pop_front();
      return front;
    }

    // Empties the buffer queue.
    void Clear() MOZ_REQUIRES(mMutex) {
      mMutex.AssertCurrentThreadOwns();
      mBufferList.clear();
    }

   private:
    typedef std::deque<AudioChunk> BufferList;

    // Synchronizes access to mBufferList.  Note that it's the responsibility
    // of the callers to perform the required locking, and we assert that every
    // time we access mBufferList.
    Mutex mMutex MOZ_UNANNOTATED;
    // The list representing the queue.
    BufferList mBufferList;
  };

 public:
  explicit SharedBuffers(float aSampleRate)
      : mOutputQueue("SharedBuffers::outputQueue"),
        mDelaySoFar(TRACK_TIME_MAX),
        mSampleRate(aSampleRate),
        mLatency(0.0),
        mDroppingBuffers(false) {}

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t amount = aMallocSizeOf(this);

    {
      MutexAutoLock lock(mOutputQueue.Lock());
      amount += mOutputQueue.SizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  // main thread

  // NotifyNodeIsConnected() may be called even when the state has not
  // changed.
  void NotifyNodeIsConnected(bool aIsConnected) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!aIsConnected) {
      // Reset main thread state for FinishProducingOutputBuffer().
      mLatency = 0.0f;
      mLastEventTime = TimeStamp();
      mDroppingBuffers = false;
      // Don't flush the output buffer here because the graph thread may be
      // using it now.  The graph thread will flush when it knows it is
      // disconnected.
    }
    mNodeIsConnected = aIsConnected;
  }

  void FinishProducingOutputBuffer(const AudioChunk& aBuffer) {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mNodeIsConnected) {
      // The output buffer is not used, and mLastEventTime will not be
      // initialized until the node is re-connected.
      return;
    }

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
      float bufferDuration = aBuffer.mDuration / mSampleRate;
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

    for (uint32_t offset = 0; offset < aBuffer.mDuration;
         offset += WEBAUDIO_BLOCK_SIZE) {
      AudioChunk& chunk = mOutputQueue.Produce();
      chunk = aBuffer;
      chunk.SliceTo(offset, offset + WEBAUDIO_BLOCK_SIZE);
    }
  }

  // graph thread

  AudioChunk GetOutputBuffer() {
    MOZ_ASSERT(!NS_IsMainThread());
    AudioChunk buffer;

    {
      MutexAutoLock lock(mOutputQueue.Lock());
      if (mOutputQueue.ReadyToConsume() > 0) {
        if (mDelaySoFar == TRACK_TIME_MAX) {
          mDelaySoFar = 0;
        }
        buffer = mOutputQueue.Consume();
      } else {
        // If we're out of buffers to consume, just output silence
        buffer.SetNull(WEBAUDIO_BLOCK_SIZE);
        if (mDelaySoFar != TRACK_TIME_MAX) {
          // Remember the delay that we just hit
          mDelaySoFar += WEBAUDIO_BLOCK_SIZE;
        }
      }
    }

    return buffer;
  }

  TrackTime DelaySoFar() const {
    MOZ_ASSERT(!NS_IsMainThread());
    return mDelaySoFar == TRACK_TIME_MAX ? 0 : mDelaySoFar;
  }

  void Flush() {
    MOZ_ASSERT(!NS_IsMainThread());
    mDelaySoFar = TRACK_TIME_MAX;
    {
      MutexAutoLock lock(mOutputQueue.Lock());
      mOutputQueue.Clear();
    }
  }

 private:
  OutputQueue mOutputQueue;
  // How much delay we've seen so far.  This measures the amount of delay
  // caused by the main thread lagging behind in producing output buffers.
  // TRACK_TIME_MAX means that we have not received our first buffer yet.
  // Graph thread only.
  TrackTime mDelaySoFar;
  // The samplerate of the context.
  const float mSampleRate;
  // The remaining members are main thread only.
  // This is the latency caused by the buffering. If this grows too high, we
  // will drop buffers until it is acceptable.
  float mLatency;
  // This is the time at which we last produced a buffer, to detect if the main
  // thread has been blocked.
  TimeStamp mLastEventTime;
  // True if we should be dropping buffers.
  bool mDroppingBuffers;
  // True iff the AudioNode has at least one input or output connected.
  bool mNodeIsConnected;
};

class ScriptProcessorNodeEngine final : public AudioNodeEngine {
 public:
  ScriptProcessorNodeEngine(ScriptProcessorNode* aNode,
                            AudioDestinationNode* aDestination,
                            uint32_t aBufferSize,
                            uint32_t aNumberOfInputChannels)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Track()),
        mSharedBuffers(new SharedBuffers(mDestination->mSampleRate)),
        mBufferSize(aBufferSize),
        mInputChannelCount(aNumberOfInputChannels),
        mInputWriteIndex(0) {}

  SharedBuffers* GetSharedBuffers() const { return mSharedBuffers.get(); }

  enum {
    IS_CONNECTED,
  };

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override {
    switch (aIndex) {
      case IS_CONNECTED:
        mIsConnected = aParam;
        break;
      default:
        NS_ERROR("Bad Int32Parameter");
    }  // End index switch.
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    TRACE("ScriptProcessorNodeEngine::ProcessBlock");

    // This node is not connected to anything. Per spec, we don't fire the
    // onaudioprocess event. We also want to clear out the input and output
    // buffer queue, and output a null buffer.
    if (!mIsConnected) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      mSharedBuffers->Flush();
      mInputWriteIndex = 0;
      return;
    }

    // The input buffer is allocated lazily when non-null input is received.
    if (!aInput.IsNull() && !mInputBuffer) {
      mInputBuffer = ThreadSharedFloatArrayBufferList::Create(
          mInputChannelCount, mBufferSize, fallible);
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
        AudioBlockCopyChannelWithScale(
            static_cast<const float*>(aInput.mChannelData[i]), aInput.mVolume,
            writeData);
      }
    }
    mInputWriteIndex += aInput.GetDuration();

    // Now, see if we have data to output
    // Note that we need to do this before sending the buffer to the main
    // thread so that our delay time is updated.
    *aOutput = mSharedBuffers->GetOutputBuffer();

    if (mInputWriteIndex >= mBufferSize) {
      SendBuffersToMainThread(aTrack, aFrom);
      mInputWriteIndex -= mBufferSize;
    }
  }

  bool IsActive() const override {
    // Could return false when !mIsConnected after all output chunks produced
    // by main thread events calling
    // SharedBuffers::FinishProducingOutputBuffer() have been processed.
    return true;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    // Not owned:
    // - mDestination (probably)
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mSharedBuffers->SizeOfIncludingThis(aMallocSizeOf);
    if (mInputBuffer) {
      amount += mInputBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  void SendBuffersToMainThread(AudioNodeTrack* aTrack, GraphTime aFrom) {
    MOZ_ASSERT(!NS_IsMainThread());

    // we now have a full input buffer ready to be sent to the main thread.
    TrackTime playbackTick = mDestination->GraphTimeToTrackTime(aFrom);
    // Add the duration of the current sample
    playbackTick += WEBAUDIO_BLOCK_SIZE;
    // Add the delay caused by the main thread
    playbackTick += mSharedBuffers->DelaySoFar();
    // Compute the playback time in the coordinate system of the destination
    double playbackTime = mDestination->TrackTimeToSeconds(playbackTick);

    class Command final : public Runnable {
     public:
      Command(AudioNodeTrack* aTrack,
              already_AddRefed<ThreadSharedFloatArrayBufferList> aInputBuffer,
              double aPlaybackTime)
          : mozilla::Runnable("Command"),
            mTrack(aTrack),
            mInputBuffer(aInputBuffer),
            mPlaybackTime(aPlaybackTime) {}

      NS_IMETHOD Run() override {
        auto engine = static_cast<ScriptProcessorNodeEngine*>(mTrack->Engine());
        AudioChunk output;
        output.SetNull(engine->mBufferSize);
        {
          auto node =
              static_cast<ScriptProcessorNode*>(engine->NodeMainThread());
          if (!node) {
            return NS_OK;
          }

          if (node->HasListenersFor(nsGkAtoms::onaudioprocess)) {
            DispatchAudioProcessEvent(node, &output);
          }
          // The node may have been destroyed during event dispatch.
        }

        // Append it to our output buffer queue
        engine->GetSharedBuffers()->FinishProducingOutputBuffer(output);

        return NS_OK;
      }

      // Sets up |output| iff buffers are set in event handlers.
      void DispatchAudioProcessEvent(ScriptProcessorNode* aNode,
                                     AudioChunk* aOutput) {
        AudioContext* context = aNode->Context();
        if (!context) {
          return;
        }

        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(aNode->GetOwner()))) {
          return;
        }
        JSContext* cx = jsapi.cx();
        uint32_t inputChannelCount = aNode->ChannelCount();

        // Create the input buffer
        RefPtr<AudioBuffer> inputBuffer;
        if (mInputBuffer) {
          ErrorResult rv;
          inputBuffer = AudioBuffer::Create(
              context->GetOwner(), inputChannelCount, aNode->BufferSize(),
              context->SampleRate(), mInputBuffer.forget(), rv);
          if (rv.Failed()) {
            rv.SuppressException();
            return;
          }
        }

        // Ask content to produce data in the output buffer
        // Note that we always avoid creating the output buffer here, and we try
        // to avoid creating the input buffer as well.  The AudioProcessingEvent
        // class knows how to lazily create them if needed once the script tries
        // to access them.  Otherwise, we may be able to get away without
        // creating them!
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
          *aOutput = buffer->GetThreadSharedChannelsForRate(cx);
          MOZ_ASSERT(aOutput->IsNull() ||
                         aOutput->mBufferFormat == AUDIO_FORMAT_FLOAT32,
                     "AudioBuffers initialized from JS have float data");
        }
      }

     private:
      RefPtr<AudioNodeTrack> mTrack;
      RefPtr<ThreadSharedFloatArrayBufferList> mInputBuffer;
      double mPlaybackTime;
    };

    RefPtr<Command> command =
        new Command(aTrack, mInputBuffer.forget(), playbackTime);
    mAbstractMainThread->Dispatch(command.forget());
  }

  friend class ScriptProcessorNode;

  RefPtr<AudioNodeTrack> mDestination;
  UniquePtr<SharedBuffers> mSharedBuffers;
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
    : AudioNode(aContext, aNumberOfInputChannels,
                mozilla::dom::ChannelCountMode::Explicit,
                mozilla::dom::ChannelInterpretation::Speakers),
      mBufferSize(aBufferSize ? aBufferSize
                              :  // respect what the web developer requested
                      4096)      // choose our own buffer size -- 4KB for now
      ,
      mNumberOfOutputChannels(aNumberOfOutputChannels) {
  MOZ_ASSERT(BufferSize() % WEBAUDIO_BLOCK_SIZE == 0, "Invalid buffer size");
  ScriptProcessorNodeEngine* engine = new ScriptProcessorNodeEngine(
      this, aContext->Destination(), BufferSize(), aNumberOfInputChannels);
  mTrack = AudioNodeTrack::Create(
      aContext, engine, AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

ScriptProcessorNode::~ScriptProcessorNode() = default;

size_t ScriptProcessorNode::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t ScriptProcessorNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void ScriptProcessorNode::EventListenerAdded(nsAtom* aType) {
  AudioNode::EventListenerAdded(aType);
  if (aType == nsGkAtoms::onaudioprocess) {
    UpdateConnectedStatus();
  }
}

void ScriptProcessorNode::EventListenerRemoved(nsAtom* aType) {
  AudioNode::EventListenerRemoved(aType);
  if (aType == nsGkAtoms::onaudioprocess && mTrack) {
    UpdateConnectedStatus();
  }
}

JSObject* ScriptProcessorNode::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return ScriptProcessorNode_Binding::Wrap(aCx, this, aGivenProto);
}

void ScriptProcessorNode::UpdateConnectedStatus() {
  bool isConnected =
      mHasPhantomInput || !(OutputNodes().IsEmpty() &&
                            OutputParams().IsEmpty() && InputNodes().IsEmpty());

  // Events are queued even when there is no listener because a listener
  // may be added while events are in the queue.
  SendInt32ParameterToTrack(ScriptProcessorNodeEngine::IS_CONNECTED,
                            isConnected);

  if (isConnected && HasListenersFor(nsGkAtoms::onaudioprocess)) {
    MarkActive();
  } else {
    MarkInactive();
  }

  // MarkInactive above might have released this node, check if it has a track.
  if (!mTrack) {
    return;
  }

  auto engine = static_cast<ScriptProcessorNodeEngine*>(mTrack->Engine());
  engine->GetSharedBuffers()->NotifyNodeIsConnected(isConnected);
}

}  // namespace mozilla::dom
