/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsIPICS_h__
#define nsIPICS_h__

#include "nsISupports.h"
#include "nsIWebShellServices.h"


// {DFB4E4D2-AFC3-11d2-8286-00805F2AB103}
#define NS_IPICS_IID \
{0xdfb4e4d2, 0xafc3, 0x11d2, {0x82, 0x86, 0x0, 0x80, 0x5f, 0x2a, 0xb1, 0x3}}


// {DFB4E4D3-AFC3-11d2-8286-00805F2AB103}
#define NS_PICS_CID \
{0xdfb4e4d3, 0xafc3, 0x11d2,{0x82, 0x86, 0x0, 0x80, 0x5f, 0x2a, 0xb1, 0x3}}


class nsIPICS : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPICS_IID)

  NS_IMETHOD ProcessPICSLabel(char *label) = 0;
  NS_IMETHOD GetWebShell(PRUint32 key, nsIWebShellServices*& aResult) = 0;
  NS_IMETHOD SetNotified(nsIWebShellServices* aResult, nsIURI* aURL, PRBool notified) = 0;
};

extern NS_EXPORT nsresult NS_NewPICS(nsIPICS** aPICS);

/* ContractID prefixes for PICS DLL registration. */
#define NS_PICS_CONTRACTID "@mozilla.org/pics;1"
#define NS_PICS_CLASSNAME "PICS Service"


#endif /* nsIPICS_h__ */
