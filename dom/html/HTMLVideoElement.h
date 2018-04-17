/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLVideoElement_h
#define mozilla_dom_HTMLVideoElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/StaticPrefs.h"

namespace mozilla {

class FrameStatistics;

namespace dom {

class WakeLock;
class VideoPlaybackQuality;

class HTMLVideoElement final : public HTMLMediaElement
{
public:
  typedef mozilla::dom::NodeInfo NodeInfo;

  explicit HTMLVideoElement(already_AddRefed<NodeInfo>& aNodeInfo);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLVideoElement, video)

  using HTMLMediaElement::GetPaused;

  virtual bool IsVideo() const override {
    return true;
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  static void Init();

  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  virtual nsresult Clone(NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // Set size with the current video frame's height and width.
  // If there is no video frame, returns NS_ERROR_FAILURE.
  nsresult GetVideoSize(nsIntSize* size);

  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel) override;

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  // WebIDL

  uint32_t Width() const
  {
    return GetIntAttr(nsGkAtoms::width, 0);
  }

  void SetWidth(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::width, aValue, 0, aRv);
  }

  uint32_t Height() const
  {
    return GetIntAttr(nsGkAtoms::height, 0);
  }

  void SetHeight(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::height, aValue, 0, aRv);
  }

  uint32_t VideoWidth() const
  {
    if (mMediaInfo.HasVideo()) {
      if (mMediaInfo.mVideo.mRotation == VideoInfo::Rotation::kDegree_90 ||
          mMediaInfo.mVideo.mRotation == VideoInfo::Rotation::kDegree_270) {
        return mMediaInfo.mVideo.mDisplay.height;
      }
      return mMediaInfo.mVideo.mDisplay.width;
    }
    return 0;
  }

  uint32_t VideoHeight() const
  {
    if (mMediaInfo.HasVideo()) {
      if (mMediaInfo.mVideo.mRotation == VideoInfo::Rotation::kDegree_90 ||
          mMediaInfo.mVideo.mRotation == VideoInfo::Rotation::kDegree_270) {
        return mMediaInfo.mVideo.mDisplay.width;
      }
      return mMediaInfo.mVideo.mDisplay.height;
    }
    return 0;
  }

  VideoInfo::Rotation RotationDegrees() const
  {
    return mMediaInfo.mVideo.mRotation;
  }

  void GetPoster(nsAString& aValue)
  {
    GetURIAttr(nsGkAtoms::poster, nullptr, aValue);
  }
  void SetPoster(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::poster, aValue, aRv);
  }

  uint32_t MozParsedFrames() const;

  uint32_t MozDecodedFrames() const;

  uint32_t MozPresentedFrames() const;

  uint32_t MozPaintedFrames();

  double MozFrameDelay();

  bool MozHasAudio() const;

  // Gives access to the decoder's frame statistics, if present.
  FrameStatistics* GetFrameStatistics();

  already_AddRefed<VideoPlaybackQuality> GetVideoPlaybackQuality();


  bool MozOrientationLockEnabled() const
  {
    return StaticPrefs::MediaVideocontrolsLockVideoOrientation();
  }

  bool MozIsOrientationLocked() const
  {
    return mIsOrientationLocked;
  }

  void SetMozIsOrientationLocked(bool aLock)
  {
    mIsOrientationLocked = aLock;
  }

protected:
  virtual ~HTMLVideoElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void WakeLockCreate() override;
  virtual void WakeLockRelease() override;
  void UpdateScreenWakeLock();

  RefPtr<WakeLock> mScreenWakeLock;

  bool mIsOrientationLocked;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);

  static bool IsVideoStatsEnabled();
  double TotalPlayTime() const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLVideoElement_h
