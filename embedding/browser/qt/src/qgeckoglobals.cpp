/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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
#include "qgeckoglobals.h"

#include "qgeckoembed.h"
#include "EmbedWindow.h"

#include "nsIAppShell.h"
#include <nsIDocShell.h>
#include <nsIWebProgress.h>
#include <nsIWebNavigation.h>
#include <nsIWebBrowser.h>
#include <nsISHistory.h>
#include <nsIWebBrowserChrome.h>
#include "nsIWidget.h"
#include "nsCRT.h"
#include <nsIWindowWatcher.h>
#include <nsILocalFile.h>
#include <nsEmbedAPI.h>
#include <nsWidgetsCID.h>
#include <nsIDOMUIEvent.h>

#include <nsIInterfaceRequestor.h>
#include <nsIComponentManager.h>
#include <nsIFocusController.h>
#include <nsProfileDirServiceProvider.h>
#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>
#include <nsIPref.h>
#include <nsVoidArray.h>
#include <nsIDOMBarProp.h>
#include <nsIDOMWindow.h>
#include <nsIDOMEventReceiver.h>

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

PRUint32     QGeckoGlobals::sWidgetCount = 0;
char        *QGeckoGlobals::sCompPath    = nsnull;
nsIAppShell *QGeckoGlobals::sAppShell    = nsnull;
char        *QGeckoGlobals::sProfileDir  = nsnull;
char        *QGeckoGlobals::sProfileName = nsnull;
nsIPref     *QGeckoGlobals::sPrefs       = nsnull;
nsVoidArray *QGeckoGlobals::sWindowList  = nsnull;
nsIDirectoryServiceProvider *QGeckoGlobals::sAppFileLocProvider = nsnull;
nsProfileDirServiceProvider *QGeckoGlobals::sProfileDirServiceProvider = nsnull;

void
QGeckoGlobals::pushStartup()
{
    // increment the number of widgets
    sWidgetCount++;

    // if this is the first widget, fire up xpcom
    if (sWidgetCount == 1) {
        nsresult rv;
        nsCOMPtr<nsILocalFile> binDir;

        if (sCompPath) {
            rv = NS_NewNativeLocalFile(nsDependentCString(sCompPath), 1, getter_AddRefs(binDir));
            if (NS_FAILED(rv))
                return;
        }

        rv = NS_InitEmbedding(binDir, sAppFileLocProvider);
        if (NS_FAILED(rv))
            return;

        // we no longer need a reference to the DirectoryServiceProvider
        if (sAppFileLocProvider) {
            NS_RELEASE(sAppFileLocProvider);
            sAppFileLocProvider = nsnull;
        }

        rv = startupProfile();
        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Warning: Failed to start up profiles.\n");

        // XXX startup appshell service?

        nsCOMPtr<nsIAppShell> appShell;
        appShell = do_CreateInstance(kAppShellCID);
        if (!appShell) {
            NS_WARNING("Failed to create appshell in QGeckoGlobals::pushStartup!\n");
            return;
        }
        sAppShell = appShell.get();
        NS_ADDREF(sAppShell);
        sAppShell->Create(0, nsnull);
        sAppShell->Spinup();
    }
}

void
QGeckoGlobals::popStartup()
{
    sWidgetCount--;
    if (sWidgetCount == 0) {
        // shut down the profiles
        shutdownProfile();

        if (sAppShell) {
            // Shutdown the appshell service.
            sAppShell->Spindown();
            NS_RELEASE(sAppShell);
            sAppShell = 0;
        }

        // shut down XPCOM/Embedding
        NS_TermEmbedding();
    }
}

void
QGeckoGlobals::setCompPath(const char *aPath)
{
    if (sCompPath)
        free(sCompPath);
    if (aPath)
        sCompPath = strdup(aPath);
    else
        sCompPath = nsnull;
}

void
QGeckoGlobals::setAppComponents(const nsModuleComponentInfo *,
                             int)
{
}

void
QGeckoGlobals::setProfilePath(const char *aDir, const char *aName)
{
    if (sProfileDir) {
        nsMemory::Free(sProfileDir);
        sProfileDir = nsnull;
    }

    if (sProfileName) {
        nsMemory::Free(sProfileName);
        sProfileName = nsnull;
    }

    if (aDir)
        sProfileDir = (char *)nsMemory::Clone(aDir, strlen(aDir) + 1);

    if (aName)
        sProfileName = (char *)nsMemory::Clone(aName, strlen(aDir) + 1);
}

void
QGeckoGlobals::setDirectoryServiceProvider(nsIDirectoryServiceProvider
                                        *appFileLocProvider)
{
    if (sAppFileLocProvider)
        NS_RELEASE(sAppFileLocProvider);

    if (appFileLocProvider) {
        sAppFileLocProvider = appFileLocProvider;
        NS_ADDREF(sAppFileLocProvider);
    }
}


/* static */
int
QGeckoGlobals::startupProfile(void)
{
    // initialize profiles
    if (sProfileDir && sProfileName) {
        nsresult rv;
        nsCOMPtr<nsILocalFile> profileDir;
        NS_NewNativeLocalFile(nsDependentCString(sProfileDir), PR_TRUE,
                              getter_AddRefs(profileDir));
        if (!profileDir)
            return NS_ERROR_FAILURE;
        rv = profileDir->AppendNative(nsDependentCString(sProfileName));
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsProfileDirServiceProvider> locProvider;
        NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(locProvider));
        if (!locProvider)
            return NS_ERROR_FAILURE;
        rv = locProvider->Register();
        if (NS_FAILED(rv))
            return rv;
        rv = locProvider->SetProfileDir(profileDir);
        if (NS_FAILED(rv))
            return rv;
        // Keep a ref so we can shut it down.
        NS_ADDREF(sProfileDirServiceProvider = locProvider);

        // get prefs
        nsCOMPtr<nsIPref> pref;
        pref = do_GetService(NS_PREF_CONTRACTID);
        if (!pref)
            return NS_ERROR_FAILURE;
        sPrefs = pref.get();
        NS_ADDREF(sPrefs);
    }
    return NS_OK;
}

/* static */
void
QGeckoGlobals::shutdownProfile(void)
{
    if (sProfileDirServiceProvider) {
        sProfileDirServiceProvider->Shutdown();
        NS_RELEASE(sProfileDirServiceProvider);
        sProfileDirServiceProvider = 0;
    }
    if (sPrefs) {
        NS_RELEASE(sPrefs);
        sPrefs = 0;
    }
}

void QGeckoGlobals::initializeGlobalObjects()
{
    if (!sWindowList) {
        sWindowList = new nsVoidArray();
    }
}

void QGeckoGlobals::addEngine(QGeckoEmbed *embed)
{
    sWindowList->AppendElement(embed);
}

void QGeckoGlobals::removeEngine(QGeckoEmbed *embed)
{
    sWindowList->RemoveElement(embed);
}

QGeckoEmbed *QGeckoGlobals::findPrivateForBrowser(nsIWebBrowserChrome *aBrowser)
{
    if (!sWindowList)
        return nsnull;

    // Get the number of browser windows.
    PRInt32 count = sWindowList->Count();
    // This function doesn't get called very often at all ( only when
    // creating a new window ) so it's OK to walk the list of open
    // windows.
    for (int i = 0; i < count; i++) {
        QGeckoEmbed *tmpPrivate = NS_STATIC_CAST(QGeckoEmbed *,
                                                sWindowList->ElementAt(i));
        // get the browser object for that window
        nsIWebBrowserChrome *chrome = NS_STATIC_CAST(nsIWebBrowserChrome *,
                                                     tmpPrivate->window());
        if (chrome == aBrowser)
            return tmpPrivate;
    }

    return nsnull;
}
