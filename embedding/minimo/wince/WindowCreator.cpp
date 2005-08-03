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
 * The Original Code is Minimo.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "MinimoPrivate.h"

NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

WindowCreator::WindowCreator(nsIAppShell* aAppShell)
{
    mAppShell = aAppShell;
}

WindowCreator::~WindowCreator()
{
}

NS_IMPL_ISUPPORTS2(WindowCreator, nsIWindowCreator, nsIWindowCreator2)
NS_IMETHODIMP
WindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                  PRUint32 aChromeFlags,
                                  nsIWebBrowserChrome **_retval)
{
    PRBool cancel;
    return CreateChromeWindow2(aParent, aChromeFlags, 0, 0, &cancel, _retval);
}

NS_IMETHODIMP
WindowCreator::CreateChromeWindow2(nsIWebBrowserChrome *aParent,
                                   PRUint32 aChromeFlags,
                                   PRUint32 aContextFlags,
                                   nsIURI *aURI,
                                   PRBool *aCancel,
                                   nsIWebBrowserChrome **aNewWindow)
{
    *aCancel = PR_FALSE;

	if (!mAppShell)
        return NS_ERROR_NOT_INITIALIZED;
    
    nsCOMPtr<nsIXULWindow> newWindow;
    
    nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    if (!appShell)
      return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIXULWindow> xulParent(do_GetInterface(aParent));
    
    unsigned long x, y;
    GetScreenSize(&x, &y);
    
    appShell->CreateTopLevelWindow(xulParent, 
                                   0, 
                                   aChromeFlags,
                                   x,
                                   y,
                                   mAppShell, 
                                   getter_AddRefs(newWindow));
    
    // if anybody gave us anything to work with, use it
    if (newWindow) {
      newWindow->SetContextFlags(aContextFlags);
      nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(newWindow));
      if (thing)
        CallGetInterface(thing.get(), aNewWindow);
    }
    
    return *aNewWindow ? NS_OK : NS_ERROR_FAILURE;
}
