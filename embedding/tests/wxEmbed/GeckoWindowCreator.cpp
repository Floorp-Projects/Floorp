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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
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

#include "GeckoWindowCreator.h"
#include "GeckoContainer.h"

#include "nsIWebBrowserChrome.h"
#include "nsString.h"

GeckoWindowCreator::GeckoWindowCreator()
{
}

GeckoWindowCreator::~GeckoWindowCreator()
{
}

NS_IMPL_ISUPPORTS1(GeckoWindowCreator, nsIWindowCreator)

NS_IMETHODIMP
GeckoWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *parent,
                                  PRUint32 chromeFlags,
                                  nsIWebBrowserChrome **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nsnull;

    if (!parent)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIGeckoContainer> geckoContainer = do_QueryInterface(parent);
    if (!geckoContainer)
        return NS_ERROR_FAILURE;

    nsCAutoString role;
    geckoContainer->GetRole(role);
    if (stricmp(role.get(), "browser") == 0)
    {
        GeckoContainerUI *pUI = NULL;
        geckoContainer->GetContainerUI(&pUI);
        if (pUI)
        {
            return pUI->CreateBrowserWindow(chromeFlags, parent, _retval);
        }
    }

    return NS_ERROR_FAILURE;
}
