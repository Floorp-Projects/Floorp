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
  typedef nsAutoTArray<nsAutoArrayPtr<float>, 2> InputChannels;

  ScriptProcessorNodeEngine(ScriptProcessorNode* aNode,
                            AudioDestinationNode* aDestination,
                            uint32_t aBufferSize,
                            uint32_t aNumberOfInputChannels)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(aDestination->Stream())
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
    mSharedBuffers = new SharedBuffers(mSource->SampleRate());
  }

  SharedBuffers* GetSharedBuffers() const
  {
    return mSharedBuffers;
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) override
  {
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

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mSource (probably)
    // - mDestination (probably)
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mSharedBuffers->SizeOfIncludingThis(aMallocSizeOf);
    amount += mInputChannels.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mInputChannels.Length(); i++) {
      amount += mInputChannels[i].SizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
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
    StreamTime playbackTick = mSource->GetCurrentPosition();
    // Add the duration of the current sample
    playbackTick += WEBAUDIO_BLOCK_SIZE;
    // Add the delay caused by the main thread
    playbackTick += mSharedBuffers->DelaySoFar();
    // Compute the playback time in the coordinate system of the destination
    double playbackTime =
      mSource->DestinationTimeFromTicks(mDestination, playbackTick);

    class Command final : public nsRunnable
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

      NS_IMETHOD Run() override
      {
        nsRefPtr<ScriptProcessorNode> node = static_cast<ScriptProcessorNode*>
          (mStream->Engine()->NodeMainThread());
        if (!node) {
          return NS_OK;
        }
        AudioContext* context = node->Context();
        if (!context) {
          return NS_OK;
        }

        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(node->GetOwner()))) {
          return NS_OK;
        }
        JSContext* cx = jsapi.cx();

        // Create the input buffer
        nsRefPtr<AudioBuffer> inputBuffer;
        if (!mNullInput) {
          ErrorResult rv;
          inputBuffer =
            AudioBuffer::Create(context, mInputChannels.Length(),
                                node->BufferSize(),
                                context->SampleRate(), cx, rv);
          if (rv.Failed()) {
            return NS_OK;
          }
          // Put the channel data inside it
          for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
            inputBuffer->SetRawChannelContents(i, mInputChannels[i]);
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
                         context->StreamTimeToDOMTime(mPlaybackTime));
        node->DispatchTrustedEvent(event);

        // Steal the output buffers if they have been set.
        // Don't create a buffer if it hasn't been used to return output;
        // FinishProducingOutputBuffer() will optimize output = null.
        // GetThreadSharedChannelsForRate() may also return null after OOM.
        nsRefPtr<ThreadSharedFloatArrayBufferList> output;
        if (event->HasOutputBuffer()) {
          ErrorResult rv;
          AudioBuffer* buffer = event->GetOutputBuffer(rv);
          // HasOutputBuffer() returning true means that GetOutputBuffer()
          // will not fail.
          MOZ_ASSERT(!rv.Failed());
          output = buffer->GetThreadSharedChannelsForRate(cx);
        }

        // Append it to our output buffer queue
        auto engine =
          static_cast<ScriptProcessorNodeEngine*>(mStream->Engine());
        engine->GetSharedBuffers()->
          FinishProducingOutputBuffer(output, node->BufferSize());

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

  nsAutoPtr<SharedBuffers> mSharedBuffers;
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
  engine->SetSourceStream(mStream);
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

JSObject*
ScriptProcessorNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ScriptProcessorNodeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

