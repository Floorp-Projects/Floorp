/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIAnimationObserver_h___
#define nsIAnimationObserver_h___

#include "nsIMutationObserver.h"

namespace mozilla {
namespace dom {
class Animation;
} // namespace dom
} // namespace mozilla

#define NS_IANIMATION_OBSERVER_IID \
{ 0xed025fc7, 0xdeda, 0x48b9, \
  { 0x9c, 0x35, 0xf2, 0xb6, 0x1e, 0xeb, 0xd0, 0x8d } }

class nsIAnimationObserver : public nsIMutationObserver
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IANIMATION_OBSERVER_IID)

  virtual void AnimationAdded(mozilla::dom::Animation* aAnimation) = 0;
  virtual void AnimationChanged(mozilla::dom::Animation* aAnimation) = 0;
  virtual void AnimationRemoved(mozilla::dom::Animation* aAnimation) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIAnimationObserver, NS_IANIMATION_OBSERVER_IID)

#define NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONADDED                          \
    virtual void AnimationAdded(mozilla::dom::Animation* aAnimation)         \
      override;

#define NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONCHANGED                        \
    virtual void AnimationChanged(mozilla::dom::Animation* aAnimation)       \
      override;

#define NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONREMOVED                        \
    virtual void AnimationRemoved(mozilla::dom::Animation* aAnimation)       \
      override;

#define NS_IMPL_NSIANIMATIONOBSERVER_STUB(class_)                            \
void                                                                         \
class_::AnimationAdded(mozilla::dom::Animation* aAnimation)                  \
{                                                                            \
}                                                                            \
void                                                                         \
class_::AnimationChanged(mozilla::dom::Animation* aAnimation)                \
{                                                                            \
}                                                                            \
void                                                                         \
class_::AnimationRemoved(mozilla::dom::Animation* aAnimation)                \
{                                                                            \
}                                                                            \
NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(class_)                                \
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(class_)

#define NS_DECL_NSIANIMATIONOBSERVER                                         \
    NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONADDED                              \
    NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONCHANGED                            \
    NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONREMOVED                            \
    NS_DECL_NSIMUTATIONOBSERVER

#endif // nsIAnimationObserver_h___
