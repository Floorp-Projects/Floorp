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

#include "GeckoWindow.h"
#include "nsIWebBrowserFocus.h"

IMPLEMENT_DYNAMIC_CLASS(GeckoWindow, wxPanel)

GeckoWindow::GeckoWindow(void) :
    mGeckoContainer(NULL)
{
}

GeckoWindow::~GeckoWindow()
{
    SetGeckoContainer(NULL);
}

void GeckoWindow::SetGeckoContainer(GeckoContainer *aGeckoContainer)
{
    if (aGeckoContainer != mGeckoContainer)
    {
        NS_IF_RELEASE(mGeckoContainer);
        mGeckoContainer = aGeckoContainer;
        NS_IF_ADDREF(mGeckoContainer);
    }
}

BEGIN_EVENT_TABLE(GeckoWindow, wxPanel)
    EVT_SIZE(      GeckoWindow::OnSize)
    EVT_SET_FOCUS( GeckoWindow::OnSetFocus)
    EVT_KILL_FOCUS(GeckoWindow::OnKillFocus)
END_EVENT_TABLE()

void GeckoWindow::OnSize(wxSizeEvent &event)
{
    if (!mGeckoContainer)
    {
        return;
    }
    // Make sure the browser is visible and sized 
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mGeckoContainer->GetWebBrowser(getter_AddRefs(webBrowser));
    nsCOMPtr<nsIBaseWindow> webBrowserAsWin = do_QueryInterface(webBrowser);
    if (webBrowserAsWin)
    {
        wxSize size = GetClientSize();
        webBrowserAsWin->SetPositionAndSize(
            0, 0, size.GetWidth(), size.GetHeight(), PR_TRUE);
        webBrowserAsWin->SetVisibility(PR_TRUE);
    }
}

void GeckoWindow::OnSetFocus(wxFocusEvent &event)
{
}

void GeckoWindow::OnKillFocus(wxFocusEvent &event)
{
}
