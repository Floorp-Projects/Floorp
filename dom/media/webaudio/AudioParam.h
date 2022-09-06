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
#include "AudioNode.h"
#include "mozilla/dom/TypedArray.h"
#include "WebAudioUtils.h"
#include "js/TypeDecls.h"

namespace mozilla::dom {

class AudioParam final : public nsWrapperCache, public AudioParamTimeline {
  virtual ~AudioParam();

 public:
  AudioParam(AudioNode* aNode, uint32_t aIndex, const nsAString& aName,
             float aDefaultValue,
             float aMinValue = std::numeric_limits<float>::lowest(),
             float aMaxValue = std::numeric_limits<float>::max());

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(AudioParam)

  AudioContext* GetParentObject() const { return mNode->Context(); }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  float Value() {
    return AudioParamTimeline::GetValueAtTime<double>(
        GetParentObject()->CurrentTime());
  }

  // We override SetValueCurveAtTime to convert the Float32Array to the wrapper
  // object.
  AudioParam* SetValueCurveAtTime(const nsTArray<float>& aValues,
                                  double aStartTime, double aDuration,
                                  ErrorResult& aRv) {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_START_TIME_ERROR>();
      return this;
    }
    aStartTime = std::max(aStartTime, GetParentObject()->CurrentTime());
    EventInsertionHelper(aRv, AudioTimelineEvent::SetValueCurve, aStartTime,
                         0.0f, 0.0f, aDuration, aValues.Elements(),
                         aValues.Length());

    return this;
  }

  // Intended for use in AudioNode creation, when the setter should not throw.
  void SetInitialValue(float aValue) {
    MOZ_ASSERT(HasSimpleValue(), "Existing events unexpected");
    AudioTimelineEvent event(AudioTimelineEvent::SetValue, 0.0f, aValue);

    DebugOnly<ErrorResult> rv;
    MOZ_ASSERT(ValidateEvent(event, rv), "This event should be valid");

    AudioParamTimeline::SetValue(aValue);

    SendEventToEngine(event);
  }

  void SetValue(float aValue, ErrorResult& aRv) {
    AudioTimelineEvent event(AudioTimelineEvent::SetValue, 0.0f, aValue);

    if (!ValidateEvent(event, aRv)) {
      return;
    }

    AudioParamTimeline::SetValue(aValue);

    SendEventToEngine(event);
  }

  AudioParam* SetValueAtTime(float aValue, double aStartTime,
                             ErrorResult& aRv) {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_START_TIME_ERROR>();
      return this;
    }
    aStartTime = std::max(aStartTime, GetParentObject()->CurrentTime());
    EventInsertionHelper(aRv, AudioTimelineEvent::SetValueAtTime, aStartTime,
                         aValue);

    return this;
  }

  AudioParam* LinearRampToValueAtTime(float aValue, double aEndTime,
                                      ErrorResult& aRv) {
    if (!WebAudioUtils::IsTimeValid(aEndTime)) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_END_TIME_ERROR>();
      return this;
    }
    aEndTime = std::max(aEndTime, GetParentObject()->CurrentTime());
    EventInsertionHelper(aRv, AudioTimelineEvent::LinearRamp, aEndTime, aValue);
    return this;
  }

  AudioParam* ExponentialRampToValueAtTime(float aValue, double aEndTime,
                                           ErrorResult& aRv) {
    if (!WebAudioUtils::IsTimeValid(aEndTime)) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_END_TIME_ERROR>();
      return this;
    }
    aEndTime = std::max(aEndTime, GetParentObject()->CurrentTime());
    EventInsertionHelper(aRv, AudioTimelineEvent::ExponentialRamp, aEndTime,
                         aValue);
    return this;
  }

  AudioParam* SetTargetAtTime(float aTarget, double aStartTime,
                              double aTimeConstant, ErrorResult& aRv) {
    if (!WebAudioUtils::IsTimeValid(aStartTime) ||
        !WebAudioUtils::IsTimeValid(aTimeConstant)) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_START_TIME_ERROR>();
      return this;
    }
    aStartTime = std::max(aStartTime, GetParentObject()->CurrentTime());
    EventInsertionHelper(aRv, AudioTimelineEvent::SetTarget, aStartTime,
                         aTarget, aTimeConstant);

    return this;
  }

  AudioParam* CancelScheduledValues(double aStartTime, ErrorResult& aRv) {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_START_TIME_ERROR>();
      return this;
    }

    aStartTime = std::max(aStartTime, GetParentObject()->CurrentTime());

    // Remove some events on the main thread copy.
    AudioEventTimeline::CancelScheduledValues(aStartTime);

    AudioTimelineEvent event(AudioTimelineEvent::Cancel, aStartTime, 0.0f);

    SendEventToEngine(event);

    return this;
  }

  uint32_t ParentNodeId() { return mNode->Id(); }

  void GetName(nsAString& aName) { aName.Assign(mName); }

  float DefaultValue() const { return mDefaultValue; }

  float MinValue() const { return mMinValue; }

  float MaxValue() const { return mMaxValue; }

  bool IsTrackSuspended() const {
    return mTrack ? mTrack->IsSuspended() : false;
  }

  const nsTArray<AudioNode::InputNode>& InputNodes() const {
    return mInputNodes;
  }

  void RemoveInputNode(uint32_t aIndex) { mInputNodes.RemoveElementAt(aIndex); }

  AudioNode::InputNode* AppendInputNode() {
    return mInputNodes.AppendElement();
  }

  // May create the track if it doesn't exist
  mozilla::MediaTrack* Track();

  // Return nullptr if track doesn't exist.
  mozilla::MediaTrack* GetTrack() const;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = AudioParamTimeline::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mNode

    // Just count the array, actual nodes are counted in mNode.
    amount += mInputNodes.ShallowSizeOfExcludingThis(aMallocSizeOf);

    if (mNodeTrackPort) {
      amount += mNodeTrackPort->SizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  void EventInsertionHelper(ErrorResult& aRv, AudioTimelineEvent::Type aType,
                            double aTime, float aValue,
                            double aTimeConstant = 0.0, double aDuration = 0.0,
                            const float* aCurve = nullptr,
                            uint32_t aCurveLength = 0) {
    AudioTimelineEvent event(aType, aTime, aValue, aTimeConstant, aDuration,
                             aCurve, aCurveLength);

    if (!ValidateEvent(event, aRv)) {
      return;
    }

    AudioEventTimeline::InsertEvent<double>(event);

    SendEventToEngine(event);

    CleanupOldEvents();
  }

  void CleanupOldEvents();

  void SendEventToEngine(const AudioTimelineEvent& aEvent);

  void DisconnectFromGraphAndDestroyTrack();

  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
  RefPtr<AudioNode> mNode;
  // For every InputNode, there is a corresponding entry in mOutputParams of the
  // InputNode's mInputNode.
  nsTArray<AudioNode::InputNode> mInputNodes;
  const nsString mName;
  // The input port used to connect the AudioParam's track to its node's track
  RefPtr<MediaInputPort> mNodeTrackPort;
  const uint32_t mIndex;
  const float mDefaultValue;
  const float mMinValue;
  const float mMaxValue;
};

}  // namespace mozilla::dom

#endif
