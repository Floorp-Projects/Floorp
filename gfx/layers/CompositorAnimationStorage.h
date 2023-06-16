/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorAnimationStorage_h
#define mozilla_layers_CompositorAnimationStorage_h

#include "mozilla/layers/AnimationStorageData.h"
#include "mozilla/layers/LayersMessages.h"  // for TransformData, etc
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/Variant.h"
#include "nsClassHashtable.h"
#include "X11UndefineNone.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace mozilla {
namespace layers {
class APZSampler;
class Animation;
class CompositorBridgeParent;
class OMTAController;

typedef nsTArray<layers::Animation> AnimationArray;

struct AnimationTransform {
  /*
   * This transform is calculated from sampleanimation in device pixel
   * and used for layers (i.e. non WebRender)
   */
  gfx::Matrix4x4 mTransformInDevSpace;
  /*
   * This transform is calculated from frame used for WebRender and used by
   * getOMTAStyle() for OMTA testing.
   */
  gfx::Matrix4x4 mFrameTransform;
  TransformData mData;
};

struct AnimatedValue final {
  typedef Variant<AnimationTransform, float, nscolor> AnimatedValueType;

  const AnimatedValueType& Value() const { return mValue; }
  const AnimationTransform& Transform() const {
    return mValue.as<AnimationTransform>();
  }
  const float& Opacity() const { return mValue.as<float>(); }
  const nscolor& Color() const { return mValue.as<nscolor>(); }
  template <typename T>
  bool Is() const {
    return mValue.is<T>();
  }

  AnimatedValue(const gfx::Matrix4x4& aTransformInDevSpace,
                const gfx::Matrix4x4& aFrameTransform,
                const TransformData& aData)
      : mValue(AsVariant(AnimationTransform{aTransformInDevSpace,
                                            aFrameTransform, aData})) {}

  explicit AnimatedValue(const float& aValue) : mValue(AsVariant(aValue)) {}

  explicit AnimatedValue(nscolor aValue) : mValue(AsVariant(aValue)) {}

  void SetTransform(const gfx::Matrix4x4& aFrameTransform,
                    const TransformData& aData) {
    MOZ_ASSERT(mValue.is<AnimationTransform>());
    AnimationTransform& previous = mValue.as<AnimationTransform>();
    previous.mFrameTransform = aFrameTransform;
    if (previous.mData != aData) {
      previous.mData = aData;
    }
  }
  void SetOpacity(float aOpacity) {
    MOZ_ASSERT(mValue.is<float>());
    mValue.as<float>() = aOpacity;
  }
  void SetColor(nscolor aColor) {
    MOZ_ASSERT(mValue.is<nscolor>());
    mValue.as<nscolor>() = aColor;
  }

 private:
  AnimatedValueType mValue;
};

struct WrAnimations {
  nsTArray<wr::WrOpacityProperty> mOpacityArrays;
  nsTArray<wr::WrTransformProperty> mTransformArrays;
  nsTArray<wr::WrColorProperty> mColorArrays;
};

// CompositorAnimationStorage stores the animations and animated values
// keyed by a CompositorAnimationsId. The "animations" are a representation of
// an entire animation over time, while the "animated values" are values sampled
// from the animations at a particular point in time.
//
// There is one CompositorAnimationStorage per CompositorBridgeParent (i.e.
// one per browser window), and the CompositorAnimationsId key is unique within
// a particular CompositorAnimationStorage instance.
//
// Each layer which has animations gets a CompositorAnimationsId key, and reuses
// that key during its lifetime. Likewise, in layers-free webrender, a display
// item that is animated (e.g. nsDisplayTransform) gets a CompositorAnimationsId
// key and reuses that key (it persists the key via the frame user-data
// mechanism).
class CompositorAnimationStorage final {
  typedef nsClassHashtable<nsUint64HashKey, AnimatedValue> AnimatedValueTable;
  typedef std::unordered_map<uint64_t, std::unique_ptr<AnimationStorageData>>
      AnimationsTable;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorAnimationStorage)
 public:
  explicit CompositorAnimationStorage(CompositorBridgeParent* aCompositorBridge)
      : mLock("CompositorAnimationStorage::mLock"),
        mCompositorBridge(aCompositorBridge) {}

  OMTAValue GetOMTAValue(const uint64_t& aId) const;

  /**
   * Collect all animations in this class as WebRender type properties.
   */
  WrAnimations CollectWebRenderAnimations() const;

  /**
   * Set the animations based on the unique id
   */
  void SetAnimations(uint64_t aId, const LayersId& aLayersId,
                     const AnimationArray& aAnimations);

  /**
   * Sample animation based the given timestamps and store them in this
   * CompositorAnimationStorage. The animated values after sampling will be
   * stored in CompositorAnimationStorage as well.
   *
   * Returns true if there is any animation.
   * Note that even if there are only in-delay phase animations (i.e. not
   * visually effective), this function returns true to ensure we composite
   * again on the next tick.
   *
   * Note: This is called only by WebRender.
   */
  bool SampleAnimations(const OMTAController* aOMTAController,
                        TimeStamp aPreviousFrameTime,
                        TimeStamp aCurrentFrameTime);

  bool HasAnimations() const;

  /**
   * Clear AnimatedValues and Animations data
   */
  void ClearById(const uint64_t& aId);

 private:
  ~CompositorAnimationStorage() = default;

  /**
   * Return the animated value if a given id can map to its animated value
   */
  AnimatedValue* GetAnimatedValue(const uint64_t& aId) const;

  /**
   * Set the animation transform based on the unique id and also
   * set up |aFrameTransform| and |aData| for OMTA testing.
   * If |aPreviousValue| is not null, the animation transform replaces the value
   * in the |aPreviousValue|.
   * NOTE: |aPreviousValue| should be the value for the |aId|.
   */
  void SetAnimatedValue(uint64_t aId, AnimatedValue* aPreviousValue,
                        const gfx::Matrix4x4& aFrameTransform,
                        const TransformData& aData);

  /**
   * Similar to above but for opacity.
   */
  void SetAnimatedValue(uint64_t aId, AnimatedValue* aPreviousValue,
                        float aOpacity);

  /**
   * Similar to above but for color.
   */
  void SetAnimatedValue(uint64_t aId, AnimatedValue* aPreviousValue,
                        nscolor aColor);

  using JankedAnimationMap =
      std::unordered_map<LayersId, nsTArray<uint64_t>, LayersId::HashFn>;

  /*
   * Store the animated values from |aAnimationValues|.
   */
  void StoreAnimatedValue(
      nsCSSPropertyID aProperty, uint64_t aId,
      const std::unique_ptr<AnimationStorageData>& aAnimationStorageData,
      const AutoTArray<RefPtr<StyleAnimationValue>, 1>& aAnimationValues,
      const MutexAutoLock& aProofOfMapLock,
      const RefPtr<APZSampler>& aApzSampler, AnimatedValue* aAnimatedValueEntry,
      JankedAnimationMap& aJankedAnimationMap);

 private:
  AnimatedValueTable mAnimatedValues;
  AnimationsTable mAnimations;
  std::unordered_set<uint64_t> mNewAnimations;
  mutable Mutex mLock MOZ_UNANNOTATED;
  // CompositorBridgeParent owns this CompositorAnimationStorage instance.
  CompositorBridgeParent* MOZ_NON_OWNING_REF mCompositorBridge;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorAnimationStorage_h
