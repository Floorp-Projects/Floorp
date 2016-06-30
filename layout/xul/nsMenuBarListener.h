/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsMenuBarListener_h__
#define nsMenuBarListener_h__

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMEventListener.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

class nsMenuBarFrame;
class nsIDOMKeyEvent;

/** editor Implementation of the DragListener interface
 */
class nsMenuBarListener final : public nsIDOMEventListener
{
public:
  /** default constructor
   */
  explicit nsMenuBarListener(nsMenuBarFrame* aMenuBar);

  static void InitializeStatics();
   
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;
  
  nsresult KeyUp(nsIDOMEvent* aMouseEvent);
  nsresult KeyDown(nsIDOMEvent* aMouseEvent);
  nsresult KeyPress(nsIDOMEvent* aMouseEvent);
  nsresult Blur(nsIDOMEvent* aEvent);
  nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  nsresult Fullscreen(nsIDOMEvent* aEvent);

  static nsresult GetMenuAccessKey(int32_t* aAccessKey);
  
  NS_DECL_ISUPPORTS

  static bool IsAccessKeyPressed(nsIDOMKeyEvent* event);

  void OnDestroyMenuBarFrame();

protected:
  /** default destructor
   */
  virtual ~nsMenuBarListener();

  static void InitAccessKey();

  static mozilla::Modifiers GetModifiersForAccessKey(nsIDOMKeyEvent* event);

  // This should only be called by the nsMenuBarListener during event dispatch,
  // thus ensuring that this doesn't get destroyed during the process.
  void ToggleMenuActiveState();

  bool Destroyed() const { return !mMenuBarFrame; }

  nsMenuBarFrame* mMenuBarFrame; // The menu bar object.
  // Whether or not the ALT key is currently down.
  bool mAccessKeyDown;
  // Whether or not the ALT key down is canceled by other action.
  bool mAccessKeyDownCanceled;
  static bool mAccessKeyFocuses; // Does the access key by itself focus the menubar?
  static int32_t mAccessKey;     // See nsIDOMKeyEvent.h for sample values
  static mozilla::Modifiers mAccessKeyMask;// Modifier mask for the access key
};


#endif
