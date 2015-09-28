/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParam_h_
#define AudioParam_h_

#include "AudioParamTimeline.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "AudioNode.h"
#include "mozilla/dom/TypedArray.h"
#include "WebAudioUtils.h"
#include "js/TypeDecls.h"

namespace mozilla {

namespace dom {

class AudioParam final : public nsWrapperCache,
                         public AudioParamTimeline
{
  virtual ~AudioParam();

public:
  typedef void (*CallbackType)(AudioNode* aNode, const AudioTimelineEvent&);

  AudioParam(AudioNode* aNode,
             CallbackType aCallback,
             float aDefaultValue,
             const char* aName);

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioParam)

  AudioContext* GetParentObject() const
  {
    return mNode->Context();
  }

  double DOMTimeToStreamTime(double aTime) const
  {
    return mNode->Context()->DOMTimeToStreamTime(aTime);
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // We override SetValueCurveAtTime to convert the Float32Array to the wrapper
  // object.
  void SetValueCurveAtTime(const Float32Array& aValues, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    aValues.ComputeLengthAndData();

    EventInsertionHelper(aRv, AudioTimelineEvent::SetValueCurve,
                         aStartTime, 0.0f, 0.0f, aDuration, aValues.Data(),
                         aValues.Length());
  }

  void SetValue(float aValue)
  {
    AudioTimelineEvent event(AudioTimelineEvent::SetValue, 0.0f, aValue);

    ErrorResult rv;
    if (!ValidateEvent(event, rv)) {
      MOZ_ASSERT(false, "This should not happen, "
                        "setting the value should always work");
      return;
    }

    AudioParamTimeline::SetValue(aValue);

    mCallback(mNode, event);
  }

  void SetValueAtTime(float aValue, double aStartTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    EventInsertionHelper(aRv, AudioTimelineEvent::SetValueAtTime,
                         aStartTime, aValue);
  }

  void LinearRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aEndTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    EventInsertionHelper(aRv, AudioTimelineEvent::LinearRamp, aEndTime, aValue);
  }

  void ExponentialRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aEndTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    EventInsertionHelper(aRv, AudioTimelineEvent::ExponentialRamp,
                         aEndTime, aValue);
  }

  void SetTargetAtTime(float aTarget, double aStartTime,
                       double aTimeConstant, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime) ||
        !WebAudioUtils::IsTimeValid(aTimeConstant)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    EventInsertionHelper(aRv, AudioTimelineEvent::SetTarget,
                         aStartTime, aTarget,
                         aTimeConstant);
  }

  void CancelScheduledValues(double aStartTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }

    double streamTime = DOMTimeToStreamTime(aStartTime);

    // Remove some events on the main thread copy.
    AudioEventTimeline::CancelScheduledValues(streamTime);

    AudioTimelineEvent event(AudioTimelineEvent::Cancel,
                             streamTime, 0.0f);

    mCallback(mNode, event);
  }

  uint32_t ParentNodeId()
  {
    return mNode->Id();
  }

  void GetName(nsAString& aName)
  {
    aName.AssignASCII(mName);
  }

  float DefaultValue() const
  {
    return mDefaultValue;
  }

  AudioNode* Node() const
  {
    return mNode;
  }

  const nsTArray<AudioNode::InputNode>& InputNodes() const
  {
    return mInputNodes;
  }

  void RemoveInputNode(uint32_t aIndex)
  {
    mInputNodes.RemoveElementAt(aIndex);
  }

  AudioNode::InputNode* AppendInputNode()
  {
    return mInputNodes.AppendElement();
  }

  void DisconnectFromGraphAndDestroyStream();

  // May create the stream if it doesn't exist
  MediaStream* Stream();

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioParamTimeline::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mNode

    // Just count the array, actual nodes are counted in mNode.
    amount += mInputNodes.ShallowSizeOfExcludingThis(aMallocSizeOf);

    if (mNodeStreamPort) {
      amount += mNodeStreamPort->SizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

private:
  void EventInsertionHelper(ErrorResult& aRv,
                            AudioTimelineEvent::Type aType,
                            double aTime, float aValue,
                            double aTimeConstant = 0.0,
                            float aDuration = 0.0,
                            const float* aCurve = nullptr,
                            uint32_t aCurveLength = 0)
  {
    AudioTimelineEvent event(aType,
                             DOMTimeToStreamTime(aTime), aValue,
                             aTimeConstant, aDuration, aCurve, aCurveLength);

    if (!ValidateEvent(event, aRv)) {
      return;
    }

    AudioEventTimeline::InsertEvent<double>(event);

    mCallback(mNode, event);
  }

  nsRefPtr<AudioNode> mNode;
  // For every InputNode, there is a corresponding entry in mOutputParams of the
  // InputNode's mInputNode.
  nsTArray<AudioNode::InputNode> mInputNodes;
  CallbackType mCallback;
  const float mDefaultValue;
  const char* mName;
  // The input port used to connect the AudioParam's stream to its node's stream
  nsRefPtr<MediaInputPort> mNodeStreamPort;
};

} // namespace dom
} // namespace mozilla

#endif

