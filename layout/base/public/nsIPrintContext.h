/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 * 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 */
#ifndef nsIPrintContext_h___
#define nsIPrintContext_h___

#include "nsISupports.h"
#include "nscore.h"

class nsIPrintSettings;

#define NS_IPRINTCONTEXT_IID   \
{ 0xa05e0e40, 0xe1bf, 0x11d4, \
  { 0xa8, 0x5d, 0x0, 0x10, 0x5a, 0x18, 0x34, 0x19 } }


// An interface for presentation printing contexts 
class nsIPrintContext : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRINTCONTEXT_IID)

  // Set and Get nsIPrintSettings
  NS_IMETHOD SetPrintSettings(nsIPrintSettings* aPS) = 0;
  NS_IMETHOD GetPrintSettings(nsIPrintSettings** aPS) = 0;

};


// Factory method to create a "paginated" presentation context for
// printing
nsresult
NS_NewPrintContext(nsIPrintContext** aInstancePtrResult);

#endif
