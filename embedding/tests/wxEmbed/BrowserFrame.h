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