/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULResizerElement_h
#define mozilla_dom_XULResizerElement_h

#include "nsXULElement.h"
#include "Units.h"

namespace mozilla::dom {

nsXULElement* NS_NewXULResizerElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class XULResizerElement final : public nsXULElement {
 public:
  explicit XULResizerElement(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
      : nsXULElement(std::move(aNodeInfo)) {}

  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(EventChainPostVisitor&) override;

 private:
  virtual ~XULResizerElement() = default;
  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  MOZ_CAN_RUN_SCRIPT
  void PostHandleEventInternal(EventChainPostVisitor&);

  struct Direction {
    int8_t mHorizontal;
    int8_t mVertical;
  };
  Direction GetDirection();

  nsIContent* GetContentToResize() const;

  /**
   * Adjust the window position and size in a direction according to the mouse
   * movement and the resizer direction. The minimum and maximum size is used
   * to constrain the size.
   *
   * @param aPos left or top position
   * @param aSize width or height
   * @param aMovement the amount the mouse was moved
   * @param aResizerDirection resizer direction returned by GetDirection
   */
  static void AdjustDimensions(int32_t* aPos, int32_t* aSize, int32_t aMovement,
                               int8_t aResizerDirection);

  struct SizeInfo {
    nsCString width, height;
  };
  static void SizeInfoDtorFunc(void* aObject, nsAtom* aPropertyName,
                               void* aPropertyValue, void* aData);
  static void ResizeContent(nsIContent* aContent, const Direction& aDirection,
                            const SizeInfo& aSizeInfo,
                            SizeInfo* aOriginalSizeInfo);
  static void MaybePersistOriginalSize(nsIContent* aContent,
                                       const SizeInfo& aSizeInfo);
  static void RestoreOriginalSize(nsIContent* aContent);

  LayoutDeviceIntRect mMouseDownRect;
  LayoutDeviceIntPoint mMouseDownPoint;
  bool mTrackingMouseMove = false;
};

}  // namespace mozilla::dom

#endif  // XULResizerElement_h
