/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULMenuBarElement_h__
#define XULMenuBarElement_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "nsINode.h"
#include "nsISupports.h"
#include "XULMenuParentElement.h"

namespace mozilla::dom {

class KeyboardEvent;
class XULButtonElement;
class MenuBarListener;

nsXULElement* NS_NewXULMenuBarElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class XULMenuBarElement final : public XULMenuParentElement {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULMenuBarElement,
                                           XULMenuParentElement)
  NS_IMPL_FROMNODE_WITH_TAG(XULMenuBarElement, kNameSpaceID_XUL, menubar)

  explicit XULMenuBarElement(already_AddRefed<class NodeInfo>&&);

  MOZ_CAN_RUN_SCRIPT void SetActive(bool);
  bool IsActive() const { return mIsActive; }

  void SetActiveByKeyboard() { mActiveByKeyboard = true; }
  bool IsActiveByKeyboard() const { return mActiveByKeyboard; }

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(UnbindContext&) override;

 protected:
  ~XULMenuBarElement() override;

  // Whether or not the menu bar is active (a menu item is highlighted or
  // shown).
  bool mIsActive = false;

  // Whether the menubar was made active via the keyboard.
  bool mActiveByKeyboard = false;

  // The event listener that listens to document key presses and so on.
  RefPtr<MenuBarListener> mListener;
};

}  // namespace mozilla::dom

#endif  // XULMenuBarElement_h
