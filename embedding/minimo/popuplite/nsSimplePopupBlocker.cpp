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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIPopupWindowManager.h"
#include "nsIServiceManagerUtils.h"

#include "nsIModule.h"
#include "nsIGenericFactory.h"

class nsIURI;

class nsSimplePopupBlocker : public nsIPopupWindowManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOPUPWINDOWMANAGER

  nsSimplePopupBlocker();
  virtual ~nsSimplePopupBlocker();

private:
  PRUint32 mPolicy;
};

nsSimplePopupBlocker::nsSimplePopupBlocker() 
  : mPolicy(DENY_POPUP) 
{
}

nsSimplePopupBlocker::~nsSimplePopupBlocker(void) 
{
}

NS_IMPL_ISUPPORTS1(nsSimplePopupBlocker, nsIPopupWindowManager)

NS_IMETHODIMP
nsSimplePopupBlocker::GetDefaultPermission(PRUint32 *aDefaultPermission)
{
  NS_ENSURE_ARG_POINTER(aDefaultPermission);
  *aDefaultPermission = mPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsSimplePopupBlocker::SetDefaultPermission(PRUint32 aDefaultPermission)
{
  mPolicy = aDefaultPermission;
  return NS_OK;
}


NS_IMETHODIMP
nsSimplePopupBlocker::TestPermission(nsIURI *aURI, PRUint32 *aPermission)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aPermission);
  
  // For now, I don't care if we should display the popup.  Eventually, 
  // we need to do some bookkeeping.

  *aPermission = DENY_POPUP;
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimplePopupBlocker)

#define NS_POPUPWINDOWMANAGER_CID \
 {0x4275d3f4, 0x752a, 0x427a, {0xb4, 0x32, 0x14, 0xd5, 0xdd, 0xa1, 0xc2, 0x0b}}

static const nsModuleComponentInfo components[] = {
    { "Simple Popup Blocker",
      NS_POPUPWINDOWMANAGER_CID,
      NS_POPUPWINDOWMANAGER_CONTRACTID,
      nsSimplePopupBlockerConstructor
    },
};

NS_IMPL_NSGETMODULE(popuplite, components)

