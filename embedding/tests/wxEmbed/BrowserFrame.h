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

#ifndef BROWSERFRAME_H
#define BROWSERFRAME_H

#include "GeckoFrame.h"

class BrowserFrame :
    public GeckoFrame
{
protected:
    DECLARE_EVENT_TABLE()

    nsAutoString mContextLinkUrl;

    void OnBrowserUrl(wxCommandEvent &event);
    void OnBrowserGo(wxCommandEvent &event);
    void OnBrowserHome(wxCommandEvent &event);
    void OnBrowserOpenLinkInNewWindow(wxCommandEvent & event);

    void OnBrowserBack(wxCommandEvent &event);
    void OnUpdateBrowserBack(wxUpdateUIEvent &event);

    void OnBrowserForward(wxCommandEvent &event);
    void OnUpdateBrowserForward(wxUpdateUIEvent &event);

    void OnBrowserReload(wxCommandEvent &event);

    void OnBrowserStop(wxCommandEvent &event);
    void OnUpdateBrowserStop(wxUpdateUIEvent &event);

    void OnFileSave(wxCommandEvent &event);
    void OnFilePrint(wxCommandEvent &event);

    void OnViewPageSource(wxCommandEvent &event);
    void OnUpdateViewPageSource(wxUpdateUIEvent &event);

public :
    BrowserFrame(wxWindow* aParent);

    nsresult LoadURI(const char *aURI);
    nsresult LoadURI(const wchar_t *aURI);

    // GeckoContainerUI overrides
    virtual nsresult CreateBrowserWindow(PRUint32 aChromeFlags,
         nsIWebBrowserChrome *aParent, nsIWebBrowserChrome **aNewWindow);
    virtual void UpdateStatusBarText(const PRUnichar* aStatusText);
    virtual void UpdateCurrentURI();
    virtual void ShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aContextMenuInfo);
};

#endif