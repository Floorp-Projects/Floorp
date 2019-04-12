/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

bool nsPIDOMWindowOuter::IsLoading() const {
  auto* win = GetCurrentInnerWindow();

  if (!win) {
    NS_ERROR("No current inner window available!");

    return false;
  }

  return win->IsLoading();
}

bool nsPIDOMWindowInner::IsLoading() const {
  if (!mOuterWindow) {
    NS_ERROR("IsLoading() called on orphan inner window!");

    return false;
  }

  return !mIsDocumentLoaded;
}

bool nsPIDOMWindowOuter::IsHandlingResizeEvent() const {
  auto* win = GetCurrentInnerWindow();

  if (!win) {
    NS_ERROR("No current inner window available!");

    return false;
  }

  return win->IsHandlingResizeEvent();
}

bool nsPIDOMWindowInner::IsHandlingResizeEvent() const {
  if (!mOuterWindow) {
    NS_ERROR("IsHandlingResizeEvent() called on orphan inner window!");

    return false;
  }

  return mIsHandlingResizeEvent;
}

bool nsPIDOMWindowInner::IsCurrentInnerWindow() const {
  return mOuterWindow && mOuterWindow->GetCurrentInnerWindow() == this;
}

bool nsPIDOMWindowInner::HasActiveDocument() {
  return IsCurrentInnerWindow() ||
         (mOuterWindow && mOuterWindow->GetCurrentInnerWindow() &&
          mOuterWindow->GetCurrentInnerWindow()->GetDoc() == mDoc);
}

bool nsPIDOMWindowInner::IsTopInnerWindow() const {
  return mTopInnerWindow == this;
}

nsIDocShell* nsPIDOMWindowOuter::GetDocShell() const { return mDocShell; }

nsIDocShell* nsPIDOMWindowInner::GetDocShell() const {
  return mOuterWindow ? mOuterWindow->GetDocShell() : nullptr;
}

mozilla::dom::BrowsingContext* nsPIDOMWindowOuter::GetBrowsingContext() const {
  return mBrowsingContext;
}

mozilla::dom::Element* nsPIDOMWindowOuter::GetFocusedElement() const {
  return mInnerWindow ? mInnerWindow->GetFocusedElement() : nullptr;
}

mozilla::dom::Element* nsPIDOMWindowInner::GetFocusedElement() const {
  return mFocusedElement;
}
