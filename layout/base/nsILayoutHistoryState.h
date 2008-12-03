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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/*
 * interface for container for information saved in session history when
 * the document is not
 */

#ifndef _nsILayoutHistoryState_h
#define _nsILayoutHistoryState_h

#include "nsISupports.h"
#include "nsStringFwd.h"

class nsPresState;

#define NS_ILAYOUTHISTORYSTATE_IID \
{ 0x003919e2, 0x5e6b, 0x4d76, \
 { 0xa9, 0x4f, 0xbc, 0x5d, 0x15, 0x5b, 0x1c, 0x67 } }

class nsILayoutHistoryState : public nsISupports {
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILAYOUTHISTORYSTATE_IID)

  /**
   * Set |aState| as the state object for |aKey|.
   * This _transfers_ownership_ of |aState| to the LayoutHistoryState.
   * It will be freed when RemoveState() is called or when the
   * LayoutHistoryState is destroyed.
   */
  NS_IMETHOD AddState(const nsCString& aKey, nsPresState* aState) = 0;

  /**
   * Look up the state object for |aKey|.
   */
  NS_IMETHOD GetState(const nsCString& aKey, nsPresState** aState) = 0;

  /**
   * Remove the state object for |aKey|.
   */
  NS_IMETHOD RemoveState(const nsCString& aKey) = 0;

  /**
   * Check whether this history has any states in it
   */
  NS_IMETHOD_(PRBool) HasStates() const = 0;

  /**
   * Sets whether this history can contain only scroll position history
   * or all possible history
   */
   NS_IMETHOD SetScrollPositionOnly(const PRBool aFlag) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILayoutHistoryState,
                              NS_ILAYOUTHISTORYSTATE_IID)

nsresult
NS_NewLayoutHistoryState(nsILayoutHistoryState** aState);

#endif /* _nsILayoutHistoryState_h */

