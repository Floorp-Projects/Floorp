/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellEditorData.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIEditor.h"
#include "nsEditingSession.h"
#include "nsIDocShell.h"

using namespace mozilla;

nsDocShellEditorData::nsDocShellEditorData(nsIDocShell* aOwningDocShell)
    : mDocShell(aOwningDocShell),
      mDetachedEditingState(nsIHTMLDocument::eOff),
      mMakeEditable(false),
      mIsDetached(false),
      mDetachedMakeEditable(false) {
  NS_ASSERTION(mDocShell, "Where is my docShell?");
}

nsDocShellEditorData::~nsDocShellEditorData() { TearDownEditor(); }

void nsDocShellEditorData::TearDownEditor() {
  if (mHTMLEditor) {
    RefPtr<HTMLEditor> htmlEditor = mHTMLEditor.forget();
    htmlEditor->PreDestroy(false);
  }
  mEditingSession = nullptr;
  mIsDetached = false;
}

nsresult nsDocShellEditorData::MakeEditable(bool aInWaitForUriLoad) {
  if (mMakeEditable) {
    return NS_OK;
  }

  // if we are already editable, and are getting turned off,
  // nuke the editor.
  if (mHTMLEditor) {
    NS_WARNING("Destroying existing editor on frame");

    RefPtr<HTMLEditor> htmlEditor = mHTMLEditor.forget();
    htmlEditor->PreDestroy(false);
  }

  if (aInWaitForUriLoad) {
    mMakeEditable = true;
  }
  return NS_OK;
}

bool nsDocShellEditorData::GetEditable() {
  return mMakeEditable || (mHTMLEditor != nullptr);
}

nsEditingSession* nsDocShellEditorData::GetEditingSession() {
  EnsureEditingSession();

  return mEditingSession.get();
}

nsresult nsDocShellEditorData::SetHTMLEditor(HTMLEditor* aHTMLEditor) {
  // destroy any editor that we have. Checks for equality are
  // necessary to ensure that assigment into the nsCOMPtr does
  // not temporarily reduce the refCount of the editor to zero
  if (mHTMLEditor == aHTMLEditor) {
    return NS_OK;
  }

  if (mHTMLEditor) {
    RefPtr<HTMLEditor> htmlEditor = mHTMLEditor.forget();
    htmlEditor->PreDestroy(false);
    MOZ_ASSERT(!mHTMLEditor,
               "Nested call of nsDocShellEditorData::SetHTMLEditor() detected");
  }

  mHTMLEditor = aHTMLEditor;  // owning addref
  if (!mHTMLEditor) {
    mMakeEditable = false;
  }

  return NS_OK;
}

// This creates the editing session on the content docShell that owns 'this'.
void nsDocShellEditorData::EnsureEditingSession() {
  NS_ASSERTION(mDocShell, "Should have docShell here");
  NS_ASSERTION(!mIsDetached, "This will stomp editing session!");

  if (!mEditingSession) {
    mEditingSession = new nsEditingSession();
  }
}

nsresult nsDocShellEditorData::DetachFromWindow() {
  NS_ASSERTION(mEditingSession,
               "Can't detach when we don't have a session to detach!");

  nsCOMPtr<nsPIDOMWindowOuter> domWindow =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  nsresult rv = mEditingSession->DetachFromWindow(domWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsDetached = true;
  mDetachedMakeEditable = mMakeEditable;
  mMakeEditable = false;

  nsCOMPtr<dom::Document> doc = domWindow->GetDoc();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
  if (htmlDoc) {
    mDetachedEditingState = htmlDoc->GetEditingState();
  }

  mDocShell = nullptr;

  return NS_OK;
}

nsresult nsDocShellEditorData::ReattachToWindow(nsIDocShell* aDocShell) {
  mDocShell = aDocShell;

  nsCOMPtr<nsPIDOMWindowOuter> domWindow =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  nsresult rv = mEditingSession->ReattachToWindow(domWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsDetached = false;
  mMakeEditable = mDetachedMakeEditable;

  RefPtr<dom::Document> doc = domWindow->GetDoc();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
  if (htmlDoc) {
    htmlDoc->SetEditingState(mDetachedEditingState);
  }

  return NS_OK;
}
