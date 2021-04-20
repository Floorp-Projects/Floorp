/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULPopupElement_h__
#define XULPopupElement_h__

#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsXULElement.h"

struct JSContext;

namespace mozilla {
class ErrorResult;

namespace dom {

class DOMRect;
class Element;
class Event;
class StringOrOpenPopupOptions;
struct ActivateMenuItemOptions;

nsXULElement* NS_NewXULPopupElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class XULPopupElement : public nsXULElement {
 private:
  nsIFrame* GetFrame(bool aFlushLayout);

 public:
  explicit XULPopupElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsXULElement(std::move(aNodeInfo)) {}

  void GetLabel(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::label, aValue);
  }
  void SetLabel(const nsAString& aValue, ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::label, aValue, rv);
  }

  void GetPosition(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::position, aValue);
  }
  void SetPosition(const nsAString& aValue, ErrorResult& rv) {
    SetXULAttr(nsGkAtoms::position, aValue, rv);
  }

  bool AutoPosition();

  void SetAutoPosition(bool aShouldAutoPosition);

  void OpenPopup(Element* aAnchorElement,
                 const StringOrOpenPopupOptions& aOptions, int32_t aXPos,
                 int32_t aYPos, bool aIsContextMenu, bool aAttributesOverride,
                 Event* aTriggerEvent);

  void OpenPopupAtScreen(int32_t aXPos, int32_t aYPos, bool aIsContextMenu,
                         Event* aTriggerEvent);

  void OpenPopupAtScreenRect(const nsAString& aPosition, int32_t aXPos,
                             int32_t aYPos, int32_t aWidth, int32_t aHeight,
                             bool aIsContextMenu, bool aAttributesOverride,
                             Event* aTriggerEvent);

  void HidePopup(bool aCancel);

  void ActivateItem(Element& aItemElement,
                    const ActivateMenuItemOptions& aOptions, ErrorResult& aRv);

  void GetState(nsString& aState);

  nsINode* GetTriggerNode() const;

  bool IsAnchored() const;

  Element* GetAnchorNode() const;

  already_AddRefed<DOMRect> GetOuterScreenRect();

  void MoveTo(int32_t aLeft, int32_t aTop);

  void MoveToAnchor(Element* aAnchorElement, const nsAString& aPosition,
                    int32_t aXPos, int32_t aYPos, bool aAttributesOverride);

  void SizeTo(int32_t aWidth, int32_t aHeight);

  void SetConstraintRect(DOMRectReadOnly& aRect);

 protected:
  virtual ~XULPopupElement() = default;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // XULPopupElement_h
