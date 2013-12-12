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
#include "nsCxPusher.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"
#include <deque>

namespace mozilla {
namespace dom {

// The maximum latency, in seconds, that we can live with before dropping
// buffers.
static const float MAX_LATENCY_S = 0.5;

NS_IMPL_ISUPPORTS_INHERITED0(ScriptProcessorNode, AudioNode)

// This class manages a queue of output buffers shared between
// the main thread and the Media Stream Graph thread.
class SharedBuffers
{
private:
  class OutputQueue
  {
  public:
    explicit OutputQueue(const char* aName)
      : mMutex(aName)
    {}

    Mutex& Lock() { return mMutex; }

    size_t ReadyToConsume() const
    {
      mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!NS_IsMainThread());
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
  SharedBuffers(float aSampleRate)
    : mOutputQueue("SharedBuffers::outputQueue")
    , mDelaySoFar(TRACK_TICKS_MAX)
    , mSampleRate(aSampleRate)
    , mLatency(0.0)
    , mDroppingBuffers(false)
  {
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
      // When the main thread is blocked, and all the event are processed in a
      // burst after the main thread unblocks, the |(now - mLastEventTime)|
      // interval will be very short. |latency - bufferDuration| will be
      // negative, effectively moving back mLatency to a smaller and smaller
      // value, until it crosses zero, at which point we stop dropping buffers
      // and resume normal operation.
      float latency = (now - mLastEventTime).ToSeconds();
      float bufferDuration = aBufferSize / mSampleRate;
      mLatency += latency - bufferDuration;
      mLastEventTime = now;
      if (mLatency > MAX_LATENCY_S || (mDroppingBuffers && mLatency > 0.0)) {
        mDroppingBuffers = true;
        return;
      } else {
        mDroppingBuffers = false;
      }
    }

    MutexAutoLock lock(mOutputQueue.Lock());
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
        if (mDelaySoFar == TRACK_TICKS_MAX) {
          mDelaySoFar = 0;
        }
        buffer = mOutputQueue.Consume();
      } else {
        // If we're out of buffers to consume, just output silence
        buffer.SetNull(WEBAUDIO_BLOCK_SIZE);
        if (mDelaySoFar != TRACK_TICKS_MAX) {
          // Remember the delay that we just hit
          mDelaySoFar += WEBAUDIO_BLOCK_SIZE;
        }
      }
    }

    return buffer;
  }

  TrackTicks DelaySoFar() const
  {
    MOZ_ASSERT(!NS_IsMainThread());
    return mDelaySoFar == TRACK_TICKS_MAX ? 0 : mDelaySoFar;
  }

  void Reset()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    mDelaySoFar = TRACK_TICKS_MAX;
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
  // TRACK_TICKS_MAX means that we have not received our first buffer yet.
  TrackTicks mDelaySoFar;
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

class ScriptProcessorNodeEngine : public AudioNodeEngine
{
public:
  typedef nsAutoTArray<nsAutoArrayPtr<float>, 2> InputChannels;

  ScriptProcessorNodeEngine(ScriptProcessorNode* aNode,
                            AudioDestinationNode* aDestination,
                            uint32_t aBufferSize,
                            uint32_t aNumberOfInputChannels)
    : AudioNodeEngine(aNode)
    , mSharedBuffers(aNode->GetSharedBuffers())
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    , mBufferSize(aBufferSize)
    , mInputWriteIndex(0)
    , mSeenNonSilenceInput(false)
  {
    mInputChannels.SetLength(aNumberOfInputChannels);
    AllocateInputBlock();
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished) MOZ_OVERRIDE
  {
    MutexAutoLock lock(NodeMutex());

    // If our node is dead, just output silence.
    if (!Node()) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    // This node is not connected to anything. Per spec, we don't fire the
    // onaudioprocess event. We also want to clear out the input and output
    // buffer queue, and output a null buffer.
    if (!(aStream->ConsumerCount() ||
          aStream->AsProcessedStream()->InputPortCount())) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      mSharedBuffers->Reset();
      mSeenNonSilenceInput = false;
      mInputWriteIndex = 0;
      return;
    }

    // First, record our input buffer
    for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
      if (aInput.IsNull()) {
        PodZero(mInputChannels[i] + mInputWriteIndex,
                aInput.GetDuration());
      } else {
        mSeenNonSilenceInput = true;
        MOZ_ASSERT(aInput.GetDuration() == WEBAUDIO_BLOCK_SIZE, "sanity check");
        MOZ_ASSERT(aInput.mChannelData.Length() == mInputChannels.Length());
        AudioBlockCopyChannelWithScale(static_cast<const float*>(aInput.mChannelData[i]),
                                       aInput.mVolume,
                                       mInputChannels[i] + mInputWriteIndex);
      }
    }
    mInputWriteIndex += aInput.GetDuration();

    // Now, see if we have data to output
    // Note that we need to do this before sending the buffer to the main
    // thread so that our delay time is updated.
    *aOutput = mSharedBuffers->GetOutputBuffer();

    if (mInputWriteIndex >= mBufferSize) {
      SendBuffersToMainThread(aStream);
      mInputWriteIndex -= mBufferSize;
      mSeenNonSilenceInput = false;
      AllocateInputBlock();
    }
  }

private:
  void AllocateInputBlock()
  {
    for (unsigned i = 0; i < mInputChannels.Length(); ++i) {
      if (!mInputChannels[i]) {
        mInputChannels[i] = new float[mBufferSize];
      }
    }
  }

  void SendBuffersToMainThread(AudioNodeStream* aStream)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // we now have a full input buffer ready to be sent to the main thread.
    TrackTicks playbackTick = mSource->GetCurrentPosition();
    // Add the duration of the current sample
    playbackTick += WEBAUDIO_BLOCK_SIZE;
    // Add the delay caused by the main thread
    playbackTick += mSharedBuffers->DelaySoFar();
    // Compute the playback time in the coordinate system of the destination
    double playbackTime =
      WebAudioUtils::StreamPositionToDestinationTime(playbackTick,
                                                     mSource,
                                                     mDestination);

    class Command : public nsRunnable
    {
    public:
      Command(AudioNodeStream* aStream,
              InputChannels& aInputChannels,
              double aPlaybackTime,
              bool aNullInput)
        : mStream(aStream)
        , mPlaybackTime(aPlaybackTime)
        , mNullInput(aNullInput)
      {
        mInputChannels.SetLength(aInputChannels.Length());
        if (!aNullInput) {
          for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
            mInputChannels[i] = aInputChannels[i].forget();
          }
        }
      }

      NS_IMETHODIMP Run()
      {
        // If it's not safe to run scripts right now, schedule this to run later
        if (!nsContentUtils::IsSafeToRunScript()) {
          nsContentUtils::AddScriptRunner(this);
          return NS_OK;
        }

        nsRefPtr<ScriptProcessorNode> node;
        {
          // No need to keep holding the lock for the whole duration of this
          // function, since we're holding a strong reference to it, so if
          // we can obtain the reference, we will hold the node alive in
          // this function.
          MutexAutoLock lock(mStream->Engine()->NodeMutex());
          node = static_cast<ScriptProcessorNode*>(mStream->Engine()->Node());
        }
        if (!node || !node->Context()) {
          return NS_OK;
        }

        AutoPushJSContext cx(node->Context()->GetJSContext());
        if (cx) {

          // Create the input buffer
          nsRefPtr<AudioBuffer> inputBuffer;
          if (!mNullInput) {
            inputBuffer = new AudioBuffer(node->Context(),
                                          node->BufferSize(),
                                          node->Context()->SampleRate());
            if (!inputBuffer->InitializeBuffers(mInputChannels.Length(), cx)) {
              return NS_OK;
            }
            // Put the channel data inside it
            for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
              inputBuffer->SetRawChannelContents(cx, i, mInputChannels[i]);
            }
          }

          // Ask content to produce data in the output buffer
          // Note that we always avoid creating the output buffer here, and we try to
          // avoid creating the input buffer as well.  The AudioProcessingEvent class
          // knows how to lazily create them if needed once the script tries to access
          // them.  Otherwise, we may be able to get away without creating them!
          nsRefPtr<AudioProcessingEvent> event = new AudioProcessingEvent(node, nullptr, nullptr);
          event->InitEvent(inputBuffer,
                           mInputChannels.Length(),
                           mPlaybackTime);
          node->DispatchTrustedEvent(event);

          // Steal the output buffers
          nsRefPtr<ThreadSharedFloatArrayBufferList> output;
          if (event->HasOutputBuffer()) {
            output = event->OutputBuffer()->GetThreadSharedChannelsForRate(cx);
          }

          // Append it to our output buffer queue
          node->GetSharedBuffers()->FinishProducingOutputBuffer(output, node->BufferSize());
        }
        return NS_OK;
      }
    private:
      nsRefPtr<AudioNodeStream> mStream;
      InputChannels mInputChannels;
      double mPlaybackTime;
      bool mNullInput;
    };

    NS_DispatchToMainThread(new Command(aStream, mInputChannels,
                                        playbackTime,
                                        !mSeenNonSilenceInput));
  }

  friend class ScriptProcessorNode;

  SharedBuffers* mSharedBuffers;
  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  InputChannels mInputChannels;
  const uint32_t mBufferSize;
  // The write index into the current input buffer
  uint32_t mInputWriteIndex;
  bool mSeenNonSilenceInput;
};

ScriptProcessorNode::ScriptProcessorNode(AudioContext* aContext,
                                         uint32_t aBufferSize,
                                         uint32_t aNumberOfInputChannels,
                                         uint32_t aNumberOfOutputChannels)
  : AudioNode(aContext,
              aNumberOfInputChannels,
              mozilla::dom::ChannelCountMode::Explicit,
              mozilla::dom::ChannelInterpretation::Speakers)
  , mSharedBuffers(new SharedBuffers(aContext->SampleRate()))
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
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
}

ScriptProcessorNode::~ScriptProcessorNode()
{
}

JSObject*
ScriptProcessorNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ScriptProcessorNodeBinding::Wrap(aCx, aScope, this);
}

}
}

