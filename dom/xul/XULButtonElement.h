/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_xul_XULButtonElement_h__
#define dom_xul_XULButtonElement_h__

#include "mozilla/Attributes.h"
#include "nsINode.h"
#include "nsXULElement.h"

class nsMenuBarFrame;
class nsMenuPopupFrame;
class nsXULMenuCommandEvent;

namespace mozilla::dom {

class KeyboardEvent;
class XULPopupElement;
class XULMenuBarElement;
class XULMenuParentElement;

class XULButtonElement : public nsXULElement {
 public:
  explicit XULButtonElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  ~XULButtonElement() override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool MouseClicked(WidgetGUIEvent&);
  MOZ_CAN_RUN_SCRIPT nsresult PostHandleEvent(EventChainPostVisitor&) override;
  MOZ_CAN_RUN_SCRIPT void PostHandleEventForMenus(EventChainPostVisitor&);
  MOZ_CAN_RUN_SCRIPT void HandleEnterKeyPress(WidgetEvent&);

  void PopupOpened();
  MOZ_CAN_RUN_SCRIPT void PopupClosed(bool aDeselectMenu);

  XULPopupElement* GetContainingPopupElement() const;
  nsMenuPopupFrame* GetContainingPopupWithoutFlushing() const;
  MOZ_CAN_RUN_SCRIPT void ToggleMenuState();
  bool IsMenuPopupOpen();

  bool IsMenuItem() const { return NodeInfo()->Equals(nsGkAtoms::menuitem); }
  bool IsMenuList() const { return NodeInfo()->Equals(nsGkAtoms::menulist); }
  bool IsMenuActive() const;
  MOZ_CAN_RUN_SCRIPT void OpenMenuPopup(bool aSelectFirstItem);
  void CloseMenuPopup(bool aDeselectMenu);

  bool IsOnMenu() const;
  bool IsOnMenuList() const;
  bool IsOnMenuBar() const;
  bool IsOnContextMenu() const;

  XULMenuParentElement* GetMenuParent() const;

  void UnbindFromTree(bool aNullParent) override;

  MOZ_CAN_RUN_SCRIPT bool HandleKeyPress(KeyboardEvent& keyEvent);
  bool OpenedWithKey() const;
  // Called to execute our command handler.
  MOZ_CAN_RUN_SCRIPT void ExecuteMenu(WidgetEvent&);
  MOZ_CAN_RUN_SCRIPT void ExecuteMenu(Modifiers, int16_t aButton,
                                      bool aIsTrusted);

  // Whether we are a menu/menulist/menuitem element.
  bool IsAlwaysMenu() const { return mIsAlwaysMenu; }
  // Whether we should behave like a menu. This is the above plus buttons with
  // type=menu attribute.
  bool IsMenu() const;

  nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                      int32_t aModType) const override;
  void AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  NS_IMPL_FROMNODE_HELPER(XULButtonElement,
                          IsAnyOfXULElements(nsGkAtoms::checkbox,
                                             nsGkAtoms::radio, nsGkAtoms::thumb,
                                             nsGkAtoms::button, nsGkAtoms::menu,
                                             nsGkAtoms::menulist,
                                             nsGkAtoms::menuitem,
                                             nsGkAtoms::toolbarbutton,
                                             nsGkAtoms::toolbarpaletteitem,
                                             nsGkAtoms::scrollbarbutton))

  nsMenuPopupFrame* GetMenuPopup(FlushType aFlushType);
  nsMenuPopupFrame* GetMenuPopupWithoutFlushing() const;
  XULPopupElement* GetMenuPopupContent() const;
  int32_t MenuOpenCloseDelay() const;

  bool IsDisabled() const { return GetXULBoolAttr(nsGkAtoms::disabled); }

 private:
  XULMenuBarElement* GetMenuBar() const;
  void Blurred();
  enum class MenuType {
    Checkbox,
    Radio,
    Normal,
  };
  Maybe<MenuType> GetMenuType() const;

  void UncheckRadioSiblings();
  void StopBlinking();
  MOZ_CAN_RUN_SCRIPT void StartBlinking();
  void KillMenuOpenTimer();
  MOZ_CAN_RUN_SCRIPT void PassMenuCommandEventToPopupManager();

  bool mIsHandlingKeyEvent = false;

  // Whether this is a XULMenuElement.
  const bool mIsAlwaysMenu;
  RefPtr<nsXULMenuCommandEvent> mDelayedMenuCommandEvent;
  nsCOMPtr<nsITimer> mMenuOpenTimer;
  nsCOMPtr<nsITimer> mMenuBlinkTimer;
};

}  // namespace mozilla::dom

#endif
