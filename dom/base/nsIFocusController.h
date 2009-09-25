/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=3 sw=2 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Dan Rosen <dr@netscape.com>
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

#ifndef nsIFocusController_h__
#define nsIFocusController_h__

#include "nsISupports.h"
#include "nsCOMPtr.h"

class nsPIDOMWindow;
class nsIDOMNode;
class nsIController;
class nsIControllers;

// 6D733829-8AE4-43BD-BEEE-35420FE3E932
#define NS_IFOCUSCONTROLLER_IID \
{ 0x6d733829, 0x8ae4, 0x43bd, \
  { 0xbe, 0xee, 0x35, 0x42, 0x0f, 0xe3, 0xe9, 0x32 } }

class nsIFocusController : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOCUSCONTROLLER_IID)

  NS_IMETHOD GetPopupNode(nsIDOMNode** aNode)=0;
  NS_IMETHOD SetPopupNode(nsIDOMNode* aNode)=0;

  NS_IMETHOD GetControllerForCommand(nsPIDOMWindow* aContextWindow,
                                     const char * aCommand,
                                     nsIController** aResult)=0;
  NS_IMETHOD GetControllers(nsPIDOMWindow* aContextWindow, nsIControllers** aResult)=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFocusController, NS_IFOCUSCONTROLLER_IID)

#endif // nsIFocusController_h__
