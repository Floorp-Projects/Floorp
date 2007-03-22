/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
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
