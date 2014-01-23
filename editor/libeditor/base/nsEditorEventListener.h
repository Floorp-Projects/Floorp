/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEditorEventListener_h__
#define nsEditorEventListener_h__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIDOMEventListener.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsCaret;
class nsIDOMEvent;
class nsIPresShell;

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

#ifdef XP_WIN
// On Windows, we support switching the text direction by pressing Ctrl+Shift
#define HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
#endif

class nsEditor;
class nsIDOMDragEvent;

class nsEditorEventListener : public nsIDOMEventListener
{
public:
  nsEditorEventListener();
  virtual ~nsEditorEventListener();

  virtual nsresult Connect(nsEditor* aEditor);

  void Disconnect();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
#endif
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD HandleText(nsIDOMEvent* aTextEvent);
  NS_IMETHOD HandleStartComposition(nsIDOMEvent* aCompositionEvent);
  void       HandleEndComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);

  void SpellCheckIfNeeded();

  static NS_HIDDEN_(void) ShutDown();

protected:
  nsresult InstallToEditor();
  void UninstallFromEditor();

  bool CanDrop(nsIDOMDragEvent* aEvent);
  nsresult DragEnter(nsIDOMDragEvent* aDragEvent);
  nsresult DragOver(nsIDOMDragEvent* aDragEvent);
  nsresult DragExit(nsIDOMDragEvent* aDragEvent);
  nsresult Drop(nsIDOMDragEvent* aDragEvent);
  nsresult DragGesture(nsIDOMDragEvent* aDragEvent);
  void CleanupDragDropCaret();
  already_AddRefed<nsIPresShell> GetPresShell();
  bool IsFileControlTextBox();
  bool ShouldHandleNativeKeyBindings(nsIDOMEvent* aKeyEvent);

protected:
  nsEditor* mEditor; // weak
  nsRefPtr<nsCaret> mCaret;
  bool mCommitText;
  bool mInTransaction;
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  bool mHaveBidiKeyboards;
  bool mShouldSwitchTextDirection;
  bool mSwitchToRTL;
#endif
};

#endif // nsEditorEventListener_h__
