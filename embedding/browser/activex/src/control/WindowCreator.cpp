/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Adam Lock <adamlock@netscape.com>
 *
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

#include "stdafx.h"

#include "WindowCreator.h"

NS_IMPL_ISUPPORTS1(CWindowCreator, nsIWindowCreator)

CWindowCreator::CWindowCreator(void)
{
}

CWindowCreator::~CWindowCreator()
{
}

NS_IMETHODIMP
CWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent, PRUint32 aChromeFlags, nsIWebBrowserChrome **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;

    // NOTE:
    //
    // The nsIWindowCreator::CreateChromeWindow is REQUIRED to handle a nsnull
    // parent window but this implementation must have one in order to fire
    // events to the ActiveX container. Therefore we treat a nsnull aParent
    // as an error and return.

    if (aParent == nsnull)
    {
        return NS_ERROR_FAILURE;
    }

    CWebBrowserContainer *pContainer = NS_STATIC_CAST(CWebBrowserContainer *, aParent);
    
    CComQIPtr<IDispatch> dispNew;
    VARIANT_BOOL bCancel = VARIANT_FALSE;

    // Test if the event sink can give us a new window to navigate into
    if (pContainer->mEvents2)
    {
        pContainer->mEvents2->Fire_NewWindow2(&dispNew, &bCancel);
        if ((bCancel == VARIANT_FALSE) && dispNew)
        {
            CComQIPtr<IMozControlBridge> cpBridge = dispNew;
            if (cpBridge)
            {
                nsIWebBrowser *browser = nsnull;
                cpBridge->GetWebBrowser((void **) &browser);
                if (browser)
                {
                    nsresult rv = browser->GetContainerWindow(_retval);
                    NS_RELEASE(browser);
                    return rv;
                }
            }
        }
    }

    return NS_ERROR_FAILURE;
}

