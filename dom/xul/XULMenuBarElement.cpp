/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULMenuBarElement.h"
#include "MenuBarListener.h"
#include "XULButtonElement.h"
#include "nsXULPopupManager.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Try.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XULMenuBarElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULMenuBarElement,
                                                XULMenuParentElement)
  if (tmp->mListener) {
    tmp->mListener->Detach();
    tmp->mListener = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULMenuBarElement,
                                                  XULMenuParentElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XULMenuBarElement,
                                               XULMenuParentElement)

XULMenuBarElement::XULMenuBarElement(
    already_AddRefed<class NodeInfo>&& aNodeInfo)
    : XULMenuParentElement(std::move(aNodeInfo)) {}

XULMenuBarElement::~XULMenuBarElement() { MOZ_DIAGNOSTIC_ASSERT(!mListener); }

void XULMenuBarElement::SetActive(bool aActiveFlag) {
  // If the activity is not changed, there is nothing to do.
  if (mIsActive == aActiveFlag) {
    return;
  }

  // We can't activate a menubar outside of the document.
  if (!IsInComposedDoc()) {
    MOZ_ASSERT(!mIsActive, "How?");
    return;
  }

  if (!aActiveFlag) {
    // If there is a request to deactivate the menu bar, check to see whether
    // there is a menu popup open for the menu bar. In this case, don't
    // deactivate the menu bar.
    if (auto* activeChild = GetActiveMenuChild()) {
      if (activeChild->IsMenuPopupOpen()) {
        return;
      }
    }
  }

  mIsActive = aActiveFlag;
  if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
    pm->SetActiveMenuBar(this, aActiveFlag);
  }
  if (!aActiveFlag) {
    mActiveByKeyboard = false;
    SetActiveMenuChild(nullptr);
  }

  RefPtr dispatcher = new AsyncEventDispatcher(
      this, aActiveFlag ? u"DOMMenuBarActive"_ns : u"DOMMenuBarInactive"_ns,
      CanBubble::eYes, ChromeOnlyDispatch::eNo);
  DebugOnly<nsresult> rv = dispatcher->PostDOMEvent();
  NS_ASSERTION(NS_SUCCEEDED(rv), "AsyncEventDispatcher failed to dispatch");
}

nsresult XULMenuBarElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  MOZ_TRY(XULMenuParentElement::BindToTree(aContext, aParent));
  MOZ_DIAGNOSTIC_ASSERT(!mListener);
  if (aContext.InComposedDoc()) {
    mListener = new MenuBarListener(*this);
  }
  return NS_OK;
}

void XULMenuBarElement::UnbindFromTree(bool aNullParent) {
  if (mListener) {
    mListener->Detach();
    mListener = nullptr;
  }
  if (NS_WARN_IF(mIsActive)) {
    // Clean up silently when getting removed from the document while active.
    mIsActive = false;
    if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
      pm->SetActiveMenuBar(this, false);
    }
  }
  return XULMenuParentElement::UnbindFromTree(aNullParent);
}

}  // namespace mozilla::dom
