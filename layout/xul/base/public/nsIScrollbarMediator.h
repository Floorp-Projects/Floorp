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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *    Nate Nielsen (nielsen@memberwebs.com)
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

#ifndef nsIScrollbarMediator_h___
#define nsIScrollbarMediator_h___

// {b589027f-271b-4c68-91df-04f139885e9a}
#define NS_ISCROLLBARMEDIATOR_IID \
{ 0xb589027f, 0x271b, 0x4c68, { 0x91, 0xdf, 0x04, 0xf1, 0x39, 0x88, 0x5e, 0x9a } }

static NS_DEFINE_IID(kIScrollbarMediatorIID, NS_ISCROLLBARMEDIATOR_IID);

class nsIScrollbarMediator : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCROLLBARMEDIATOR_IID)

  // The nsISupports aScrollbar argument below denotes the
  // scrollbar that's firing the notification. It should be
  // where the same object as where nsIScrollbarFrame is implemented

  NS_IMETHOD PositionChanged(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32& aNewIndex) = 0;
  NS_IMETHOD ScrollbarButtonPressed(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32 aNewIndex) = 0;

  NS_IMETHOD VisibilityChanged(nsISupports* aScrollbar, PRBool aVisible) = 0;
};

#endif

