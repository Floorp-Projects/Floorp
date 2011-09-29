/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIMEStateManager_h__
#define nsIMEStateManager_h__

#include "nscore.h"

class nsIContent;
class nsPIDOMWindow;
class nsPresContext;
class nsIWidget;
class nsTextStateManager;
class nsISelection;

/*
 * IME state manager
 */

class nsIMEStateManager
{
public:
  static nsresult OnDestroyPresContext(nsPresContext* aPresContext);
  static nsresult OnRemoveContent(nsPresContext* aPresContext,
                                  nsIContent* aContent);
  static nsresult OnChangeFocus(nsPresContext* aPresContext,
                                nsIContent* aContent,
                                PRUint32 aReason);
  static void OnInstalledMenuKeyboardListener(bool aInstalling);

  // These two methods manage focus and selection/text observers.
  // They are separate from OnChangeFocus above because this offers finer
  // control compared to having the two methods incorporated into OnChangeFocus

  // OnTextStateBlur should be called *before* NS_BLUR_CONTENT fires
  // aPresContext is the nsPresContext receiving focus (not lost focus)
  // aContent is the nsIContent receiving focus (not lost focus)
  // aPresContext and/or aContent may be null
  static nsresult OnTextStateBlur(nsPresContext* aPresContext,
                                  nsIContent* aContent);
  // OnTextStateFocus should be called *after* NS_FOCUS_CONTENT fires
  // aPresContext is the nsPresContext receiving focus
  // aContent is the nsIContent receiving focus
  static nsresult OnTextStateFocus(nsPresContext* aPresContext,
                                   nsIContent* aContent);
  // Get the focused editor's selection and root
  static nsresult GetFocusSelectionAndRoot(nsISelection** aSel,
                                           nsIContent** aRoot);
  // This method updates the current IME state.  However, if the enabled state
  // isn't changed by the new state, this method does nothing.
  // Note that this method changes the IME state of the active element in the
  // widget.  So, the caller must have focus.
  // aNewIMEState must have an enabled state of nsIContent::IME_STATUS_*.
  // And optionally, it can have an open state of nsIContent::IME_STATUS_*.
  static void UpdateIMEState(PRUint32 aNewIMEState, nsIContent* aContent);

protected:
  static void SetIMEState(PRUint32 aState, nsIContent* aContent,
                          nsIWidget* aWidget, PRUint32 aReason);
  static PRUint32 GetNewIMEState(nsPresContext* aPresContext,
                                 nsIContent* aContent);

  static nsIWidget* GetWidget(nsPresContext* aPresContext);

  static nsIContent*    sContent;
  static nsPresContext* sPresContext;
  static bool           sInstalledMenuKeyboardListener;
  static bool           sInSecureInputMode;

  static nsTextStateManager* sTextStateObserver;
};

#endif // nsIMEStateManager_h__
