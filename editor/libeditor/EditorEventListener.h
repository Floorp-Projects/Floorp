/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditorEventListener_h
#define EditorEventListener_h

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIDOMEventListener.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsCaret;
class nsIContent;
class nsIDOMDragEvent;
class nsIDOMEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIPresShell;
class nsPresContext;

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

#ifdef XP_WIN
// On Windows, we support switching the text direction by pressing Ctrl+Shift
#define HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
#endif

namespace mozilla {

class EditorBase;

class EditorEventListener : public nsIDOMEventListener
{
public:
  EditorEventListener();

  virtual nsresult Connect(EditorBase* aEditorBase);

  void Disconnect();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void SpellCheckIfNeeded();

protected:
  virtual ~EditorEventListener();

  nsresult InstallToEditor();
  void UninstallFromEditor();

#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  nsresult KeyDown(nsIDOMKeyEvent* aKeyEvent);
  nsresult KeyUp(nsIDOMKeyEvent* aKeyEvent);
#endif
  nsresult KeyPress(nsIDOMKeyEvent* aKeyEvent);
  nsresult HandleText(nsIDOMEvent* aTextEvent);
  nsresult HandleStartComposition(nsIDOMEvent* aCompositionEvent);
  void HandleEndComposition(nsIDOMEvent* aCompositionEvent);
  virtual nsresult MouseDown(nsIDOMMouseEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMMouseEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseClick(nsIDOMMouseEvent* aMouseEvent);
  nsresult Focus(nsIDOMEvent* aEvent);
  nsresult Blur(nsIDOMEvent* aEvent);
  nsresult DragEnter(nsIDOMDragEvent* aDragEvent);
  nsresult DragOver(nsIDOMDragEvent* aDragEvent);
  nsresult DragExit(nsIDOMDragEvent* aDragEvent);
  nsresult Drop(nsIDOMDragEvent* aDragEvent);

  bool CanDrop(nsIDOMDragEvent* aEvent);
  void CleanupDragDropCaret();
  already_AddRefed<nsIPresShell> GetPresShell();
  nsPresContext* GetPresContext();
  nsIContent* GetFocusedRootContent();
  // Returns true if IME consumes the mouse event.
  bool NotifyIMEOfMouseButtonEvent(nsIDOMMouseEvent* aMouseEvent);
  bool EditorHasFocus();
  bool IsFileControlTextBox();
  bool ShouldHandleNativeKeyBindings(nsIDOMKeyEvent* aKeyEvent);
  nsresult HandleMiddleClickPaste(nsIDOMMouseEvent* aMouseEvent);

  EditorBase* mEditorBase; // weak
  RefPtr<nsCaret> mCaret;
  bool mCommitText;
  bool mInTransaction;
  bool mMouseDownOrUpConsumedByIME;
#ifdef HANDLE_NATIVE_TEXT_DIRECTION_SWITCH
  bool mHaveBidiKeyboards;
  bool mShouldSwitchTextDirection;
  bool mSwitchToRTL;
#endif
};

} // namespace mozilla

#endif // #ifndef EditorEventListener_h
