/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PopupBoxObject_h
#define mozilla_dom_PopupBoxObject_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BoxObject.h"
#include "nsString.h"

struct JSContext;
class nsPopupSetFrame;

namespace mozilla {
namespace dom {

class DOMRect;
class Element;
class Event;

class PopupBoxObject final : public BoxObject
{
public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(PopupBoxObject, BoxObject)

  PopupBoxObject();

  nsIContent* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void ShowPopup(Element* aAnchorElement,
                 Element& aPopupElement,
                 int32_t aXPos,
                 int32_t aYPos,
                 const nsAString& aPopupType,
                 const nsAString& aAnchorAlignment,
                 const nsAString& aPopupAlignment);

  void HidePopup(bool aCancel);

  bool AutoPosition();

  void SetAutoPosition(bool aShouldAutoPosition);

  void SizeTo(int32_t aWidth, int32_t aHeight);

  void MoveTo(int32_t aLeft, int32_t aTop);

  void OpenPopup(Element* aAnchorElement,
                 const nsAString& aPosition,
                 int32_t aXPos,
                 int32_t aYPos,
                 bool aIsContextMenu, bool aAttributesOverride,
                 Event* aTriggerEvent);

  void OpenPopupAtScreen(int32_t aXPos,
                         int32_t aYPos,
                         bool aIsContextMenu,
                         Event* aTriggerEvent);

  void OpenPopupAtScreenRect(const nsAString& aPosition,
                             int32_t aXPos, int32_t aYPos,
                             int32_t aWidth, int32_t aHeight,
                             bool aIsContextMenu,
                             bool aAttributesOverride,
                             Event* aTriggerEvent);

  void GetPopupState(nsString& aState);

  nsINode* GetTriggerNode() const;

  Element* GetAnchorNode() const;

  already_AddRefed<DOMRect> GetOuterScreenRect();

  void MoveToAnchor(Element* aAnchorElement,
                    const nsAString& aPosition,
                    int32_t aXPos,
                    int32_t aYPos,
                    bool aAttributesOverride);

  void GetAlignmentPosition(nsString& positionStr);

  int32_t AlignmentOffset();

  void SetConstraintRect(dom::DOMRectReadOnly& aRect);

private:
  ~PopupBoxObject();

protected:
  nsPopupSetFrame* GetPopupSetFrame();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PopupBoxObject_h
