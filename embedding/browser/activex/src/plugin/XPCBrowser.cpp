/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "StdAfx.h"

#include "npapi.h"

#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManagerUtils.h"
#include "nsString.h"
#include "nsIPref.h"


#include "XPCBrowser.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

IEBrowser::IEBrowser()
{
}

IEBrowser::~IEBrowser()
{
}

HRESULT IEBrowser::Init(PluginInstanceData *pData)
{
    mData = pData;
    // Get the location URL
    NPN_GetValue(mData->pPluginInstance, NPNVDOMWindow, (void *) &mDOMWindow);
    if (mDOMWindow)
    {
        mWebNavigation  = do_GetInterface(mDOMWindow);
    }
    return S_OK;
}


nsresult IEBrowser::GetWebNavigation(nsIWebNavigation **aWebNav)
{
    NS_ENSURE_ARG_POINTER(aWebNav);
    *aWebNav = mWebNavigation;
    NS_IF_ADDREF(*aWebNav);
    return (*aWebNav) ? NS_OK : NS_ERROR_FAILURE;
}

// Return the nsIDOMWindow object
nsresult IEBrowser::GetDOMWindow(nsIDOMWindow **aDOMWindow)
{
    NS_ENSURE_ARG_POINTER(aDOMWindow);
    *aDOMWindow = mDOMWindow;
    NS_IF_ADDREF(*aDOMWindow);
    return (*aDOMWindow) ? NS_OK : NS_ERROR_FAILURE;
}

// Return the nsIPref object
nsresult IEBrowser::GetPrefs(nsIPref **aPrefs)
{
    NS_ENSURE_ARG_POINTER(aPrefs);
    nsresult rv;
    nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID, &rv);
    *aPrefs = prefs;
    NS_IF_ADDREF(*aPrefs);
    return rv;
}

// Return the valid state of the browser
PRBool IEBrowser::BrowserIsValid()
{
    return mWebNavigation ? PR_TRUE : PR_FALSE;
}

