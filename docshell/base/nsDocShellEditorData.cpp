/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellEditorData.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIEditor.h"
#include "nsIEditingSession.h"
#include "nsIDocShell.h"

nsDocShellEditorData::nsDocShellEditorData(nsIDocShell* aOwningDocShell)
  : mDocShell(aOwningDocShell)
  , mMakeEditable(false)
  , mIsDetached(false)
  , mDetachedMakeEditable(false)
  , mDetachedEditingState(nsIHTMLDocument::eOff)
{
  NS_ASSERTION(mDocShell, "Where is my docShell?");
}

nsDocShellEditorData::~nsDocShellEditorData()
{
  TearDownEditor();
}

void
nsDocShellEditorData::TearDownEditor()
{
  if (mEditor) {
    mEditor->PreDestroy(false);
    mEditor = nullptr;
  }
  mEditingSession = nullptr;
  mIsDetached = false;
}

nsresult
nsDocShellEditorData::MakeEditable(bool aInWaitForUriLoad)
{
  if (mMakeEditable) {
    return NS_OK;
  }

  // if we are already editable, and are getting turned off,
  // nuke the editor.
  if (mEditor) {
    NS_WARNING("Destroying existing editor on frame");

    mEditor->PreDestroy(false);
    mEditor = nullptr;
  }

  if (aInWaitForUriLoad) {
    mMakeEditable = true;
  }
  return NS_OK;
}

bool
nsDocShellEditorData::GetEditable()
{
  return mMakeEditable || (mEditor != nullptr);
}

nsresult
nsDocShellEditorData::CreateEditor()
{
  nsCOMPtr<nsIEditingSession> editingSession;
  nsresult rv = GetEditingSession(getter_AddRefs(editingSession));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMWindow> domWindow =
    mDocShell ? mDocShell->GetWindow() : nullptr;
  rv = editingSession->SetupEditorOnWindow(domWindow);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult
nsDocShellEditorData::GetEditingSession(nsIEditingSession** aResult)
{
  nsresult rv = EnsureEditingSession();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = mEditingSession);

  return NS_OK;
}

nsresult
nsDocShellEditorData::GetEditor(nsIEditor** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_IF_ADDREF(*aResult = mEditor);
  return NS_OK;
}

nsresult
nsDocShellEditorData::SetEditor(nsIEditor* aEditor)
{
  // destroy any editor that we have. Checks for equality are
  // necessary to ensure that assigment into the nsCOMPtr does
  // not temporarily reduce the refCount of the editor to zero
  if (mEditor.get() != aEditor) {
    if (mEditor) {
      mEditor->PreDestroy(false);
      mEditor = nullptr;
    }

    mEditor = aEditor;  // owning addref
    if (!mEditor) {
      mMakeEditable = false;
    }
  }

  return NS_OK;
}

// This creates the editing session on the content docShell that owns 'this'.
nsresult
nsDocShellEditorData::EnsureEditingSession()
{
  NS_ASSERTION(mDocShell, "Should have docShell here");
  NS_ASSERTION(!mIsDetached, "This will stomp editing session!");

  nsresult rv = NS_OK;

  if (!mEditingSession) {
    mEditingSession =
      do_CreateInstance("@mozilla.org/editor/editingsession;1", &rv);
  }

  return rv;
}

nsresult
nsDocShellEditorData::DetachFromWindow()
{
  NS_ASSERTION(mEditingSession,
               "Can't detach when we don't have a session to detach!");

  nsCOMPtr<nsIDOMWindow> domWindow =
    mDocShell ? mDocShell->GetWindow() : nullptr;
  nsresult rv = mEditingSession->DetachFromWindow(domWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsDetached = true;
  mDetachedMakeEditable = mMakeEditable;
  mMakeEditable = false;

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  if (htmlDoc) {
    mDetachedEditingState = htmlDoc->GetEditingState();
  }

  mDocShell = nullptr;

  return NS_OK;
}

nsresult
nsDocShellEditorData::ReattachToWindow(nsIDocShell* aDocShell)
{
  mDocShell = aDocShell;

  nsCOMPtr<nsIDOMWindow> domWindow =
    mDocShell ? mDocShell->GetWindow() : nullptr;
  nsresult rv = mEditingSession->ReattachToWindow(domWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsDetached = false;
  mMakeEditable = mDetachedMakeEditable;

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  if (htmlDoc) {
    htmlDoc->SetEditingState(mDetachedEditingState);
  }

  return NS_OK;
}
