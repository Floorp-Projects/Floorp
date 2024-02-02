/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULMenuParentElement_h__
#define XULMenuParentElement_h__

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include "nsXULElement.h"

namespace mozilla::dom {

class KeyboardEvent;
class XULButtonElement;

nsXULElement* NS_NewXULMenuParentElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class XULMenuParentElement : public nsXULElement {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULMenuParentElement, nsXULElement)

  explicit XULMenuParentElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  bool IsMenuBar() const { return NodeInfo()->Equals(nsGkAtoms::menubar); }
  bool IsMenu() const { return !IsMenuBar(); }

  bool IsLocked() const { return mLocked; }

  void LockMenuUntilClosed(bool aLock);

  bool IsInMenuList() const {
    return GetParent() && GetParent()->IsXULElement(nsGkAtoms::menulist);
  }

  XULButtonElement* FindMenuWithShortcut(KeyboardEvent&) const;
  XULButtonElement* FindMenuWithShortcut(const nsAString& aString,
                                         bool& aDoAction) const;
  MOZ_CAN_RUN_SCRIPT void HandleEnterKeyPress(WidgetEvent&);

  NS_IMPL_FROMNODE_HELPER(XULMenuParentElement,
                          IsAnyOfXULElements(nsGkAtoms::menupopup,
                                             nsGkAtoms::panel,
                                             nsGkAtoms::tooltip,
                                             nsGkAtoms::menubar));

  XULButtonElement* GetActiveMenuChild() const { return mActiveItem.get(); }
  enum class ByKey : bool { No, Yes };
  MOZ_CAN_RUN_SCRIPT void SetActiveMenuChild(XULButtonElement*,
                                             ByKey = ByKey::No);

  XULButtonElement* GetFirstMenuItem() const;
  XULButtonElement* GetLastMenuItem() const;

  XULButtonElement* GetNextMenuItemFrom(const XULButtonElement&) const;
  XULButtonElement* GetPrevMenuItemFrom(const XULButtonElement&) const;

  enum class Wrap : bool { No, Yes };
  XULButtonElement* GetNextMenuItem(Wrap = Wrap::Yes) const;
  XULButtonElement* GetPrevMenuItem(Wrap = Wrap::Yes) const;

  XULButtonElement* GetContainingMenu() const;

  MOZ_CAN_RUN_SCRIPT void SelectFirstItem();

 protected:
  RefPtr<XULButtonElement> mActiveItem;
  bool mLocked = false;

  ~XULMenuParentElement() override;
};

}  // namespace mozilla::dom

#endif  // XULMenuParentElement_h
