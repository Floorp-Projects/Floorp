/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
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
 * The Original Code is layout debugging code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

#include "nsLayoutDebugCLH.h"
#include "nsString.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"

#ifdef MOZ_XUL_APP
#include "nsICommandLine.h"
#endif

nsLayoutDebugCLH::nsLayoutDebugCLH()
{
}

nsLayoutDebugCLH::~nsLayoutDebugCLH()
{
}

NS_IMPL_ISUPPORTS1(nsLayoutDebugCLH, ICOMMANDLINEHANDLER)

#ifdef MOZ_XUL_APP

NS_IMETHODIMP
nsLayoutDebugCLH::Handle(nsICommandLine* aCmdLine)
{
    nsresult rv;
    PRBool found;

    PRInt32 idx;
    rv = aCmdLine->FindFlag(NS_LITERAL_STRING("layoutdebug"), PR_FALSE, &idx);
    NS_ENSURE_SUCCESS(rv, rv);
    if (idx < 0)
      return NS_OK;

    PRInt32 length;
    aCmdLine->GetLength(&length);

    nsAutoString url;
    if (idx + 1 < length) {
        rv = aCmdLine->GetArgument(idx + 1, url);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!url.IsEmpty() && url.CharAt(0) == '-')
            url.Truncate();
    }

    aCmdLine->RemoveArguments(idx, idx + !url.IsEmpty());

    nsCOMPtr<nsISupportsArray> argsArray =
        do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!url.IsEmpty())
    {
        nsCOMPtr<nsISupportsString> scriptableURL =
            do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
        NS_ENSURE_TRUE(scriptableURL, NS_ERROR_FAILURE);
  
        scriptableURL->SetData(url);
        argsArray->AppendElement(scriptableURL);
    }

    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMWindow> opened;
    wwatch->OpenWindow(nsnull, "chrome://layoutdebug/content/",
                       "_blank", "chrome,dialog=no,all", argsArray,
                       getter_AddRefs(opened));
    aCmdLine->SetPreventDefault(PR_TRUE);
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebugCLH::GetHelpInfo(nsACString& aResult)
{
    aResult.Assign(NS_LITERAL_CSTRING("  -layoutdebug [<url>] Start with Layout Debugger\n"));
    return NS_OK;
}

#else // !MOZ_XUL_APP

CMDLINEHANDLER_IMPL(nsLayoutDebugCLH, "-layoutdebug",
                    "general.startup.layoutdebug",
                    "chrome://layoutdebug/content/",
                    "Start with Layout Debugger",
                    "@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug",
                    "LayoutDebug Startup Handler",
                    PR_TRUE,
                    "",
                    PR_TRUE)

#endif
