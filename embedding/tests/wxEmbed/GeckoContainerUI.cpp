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

#include "global.h"

#include "GeckoContainer.h"


nsresult GeckoContainerUI::CreateBrowserWindow(PRUint32 aChromeFlags,
         nsIWebBrowserChrome *aParent, nsIWebBrowserChrome **aNewWindow)
{
    return NS_ERROR_FAILURE;
}

void GeckoContainerUI::Destroy()
{
}

void GeckoContainerUI::Destroyed()
{
}

void GeckoContainerUI::SetFocus()
{
}

void GeckoContainerUI::KillFocus()
{
}

void GeckoContainerUI::UpdateStatusBarText(const PRUnichar* aStatusText)
{
}

void GeckoContainerUI::UpdateCurrentURI()
{
}

void GeckoContainerUI::UpdateBusyState(PRBool aBusy)
{
    mBusy = aBusy;
}

void GeckoContainerUI::UpdateProgress(PRInt32 aCurrent, PRInt32 aMax)
{
}

void GeckoContainerUI::GetResourceStringById(PRInt32 aID, char ** aReturn)
{
}

void GeckoContainerUI::ShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aContextMenuInfo)
{
}

void GeckoContainerUI::ShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
}

void GeckoContainerUI::HideTooltip()
{
}

void GeckoContainerUI::ShowWindow(PRBool aShow)
{
}

void GeckoContainerUI::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
}

void GeckoContainerUI::EnableChromeWindow(PRBool aEnabled)
{
}

PRUint32 GeckoContainerUI::RunEventLoop(PRBool &aRunCondition)
{
    return 0;
}
