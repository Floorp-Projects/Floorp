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
#include "QtPromptService.h"

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
#include <nsXULAppAPI.h>
#include <nsWidgetsCID.h>
#include <nsIDOMUIEvent.h>

#include <nsIInterfaceRequestor.h>
#include <nsIComponentManager.h>
#include <nsIFocusController.h>
#include <nsProfileDirServiceProvider.h>
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>
#include <nsVoidArray.h>
#include <nsIDOMBarProp.h>
#include <nsIDOMWindow.h>
#include <nsIDOMEvent.h>
#include <nsPIDOMEventTarget.h>

char        *QGeckoGlobals::sPath        = nsnull;
PRUint32     QGeckoGlobals::sWidgetCount = 0;
char        *QGeckoGlobals::sCompPath    = nsnull;
nsILocalFile *QGeckoGlobals::sProfileDir  = nsnull;
nsISupports  *QGeckoGlobals::sProfileLock = nsnull;
nsVoidArray *QGeckoGlobals::sWindowList  = nsnull;
nsIDirectoryServiceProvider *QGeckoGlobals::sAppFileLocProvider = nsnull;

class QTEmbedDirectoryProvider : public nsIDirectoryServiceProvider2
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
};

static const QTEmbedDirectoryProvider kDirectoryProvider;

NS_IMPL_QUERY_INTERFACE2(QTEmbedDirectoryProvider,
                         nsIDirectoryServiceProvider,
                         nsIDirectoryServiceProvider2)

NS_IMETHODIMP_(nsrefcnt)
QTEmbedDirectoryProvider::AddRef()
{
    return 1;
}

NS_IMETHODIMP_(nsrefcnt)
QTEmbedDirectoryProvider::Release()
{
    return 1;
}

NS_IMETHODIMP
QTEmbedDirectoryProvider::GetFile(const char *aKey, PRBool *aPersist,
                                   nsIFile* *aResult)
{
    if (QGeckoGlobals::sAppFileLocProvider) {
        nsresult rv = QGeckoGlobals::sAppFileLocProvider->GetFile(aKey, aPersist,
                      aResult);
        if (NS_SUCCEEDED(rv))
            return rv;
    }

    if (QGeckoGlobals::sProfileDir && !strcmp(aKey, NS_APP_USER_PROFILE_50_DIR)) {
        *aPersist = PR_TRUE;
        return QGeckoGlobals::sProfileDir->Clone(aResult);
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
QTEmbedDirectoryProvider::GetFiles(const char *aKey,
                                    nsISimpleEnumerator* *aResult)
{
    nsCOMPtr<nsIDirectoryServiceProvider2>
    dp2(do_QueryInterface(QGeckoGlobals::sAppFileLocProvider));

    if (!dp2)
        return NS_ERROR_FAILURE;

    return dp2->GetFiles(aKey, aResult);
}

#ifndef _NO_PROMPT_UI
#define NS_PROMPTSERVICE_CID \
 {0x95611356, 0xf583, 0x46f5, {0x81, 0xff, 0x4b, 0x3e, 0x01, 0x62, 0xc6, 0x19}}

NS_GENERIC_FACTORY_CONSTRUCTOR(QtPromptService)

static const nsModuleComponentInfo defaultAppComps[] = {
  {
    "Prompt Service",
    NS_PROMPTSERVICE_CID,
    "@mozilla.org/embedcomp/prompt-service;1",
    QtPromptServiceConstructor
  }
};
#else
static const nsModuleComponentInfo defaultAppComps[] = {};
#endif

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

        const char *grePath = sPath;

        if (!grePath)
            grePath = getenv("MOZILLA_FIVE_HOME");

        if (!grePath)
            return;

        nsCOMPtr<nsILocalFile> greDir;
        rv = NS_NewNativeLocalFile(nsDependentCString(grePath), PR_TRUE,
                                   getter_AddRefs(greDir));
        if (NS_FAILED(rv))
            return;

        if (sProfileDir && !sProfileLock) {
            rv = XRE_LockProfileDirectory(sProfileDir,
                                          &sProfileLock);
            if (NS_FAILED(rv)) return;
        }

        rv = XRE_InitEmbedding(greDir, binDir,
                               const_cast<QTEmbedDirectoryProvider*>
                                             (&kDirectoryProvider),
                               nsnull, nsnull);

        if (NS_FAILED(rv))
            return;

        if (sProfileDir)
          XRE_NotifyProfile();

        rv = registerAppComponents();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Warning: Failed to register app components.\n");
    }
}

void
QGeckoGlobals::popStartup()
{
    sWidgetCount--;
    if (sWidgetCount == 0) {

        // we no longer need a reference to the DirectoryServiceProvider
        if (sAppFileLocProvider) {
            NS_RELEASE(sAppFileLocProvider);
            sAppFileLocProvider = nsnull;
        }

        // shut down XPCOM/Embedding
        XRE_TermEmbedding();

        NS_IF_RELEASE(sProfileLock);
        NS_IF_RELEASE(sProfileDir);
    }
}

void
QGeckoGlobals::setPath(const char *aPath)
{
    if (sPath)
        free(sPath);
    if (aPath)
        sPath = strdup(aPath);
    else
        sPath = nsnull;
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
        if (sWidgetCount) {
            NS_ERROR("Cannot change profile directory during run.");
            return;
        }

        NS_RELEASE(sProfileDir);
        NS_RELEASE(sProfileLock);
    }

    nsresult rv =
        NS_NewNativeLocalFile(nsDependentCString(aDir), PR_TRUE, &sProfileDir);

    if (NS_SUCCEEDED(rv) && aName)
        rv = sProfileDir->AppendNative(nsDependentCString(aName));

    if (NS_SUCCEEDED(rv)) {
        PRBool exists = PR_FALSE;
        rv = sProfileDir->Exists(&exists);
        if (!exists)
            rv = sProfileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
        rv = XRE_LockProfileDirectory(sProfileDir, &sProfileLock);
    }

    if (NS_SUCCEEDED(rv)) {
        if (sWidgetCount)
            XRE_NotifyProfile();

        return;
    }

    NS_WARNING("Failed to lock profile.");

    // Failed
    NS_IF_RELEASE(sProfileDir);
    NS_IF_RELEASE(sProfileLock);
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
QGeckoGlobals::registerAppComponents()
{
  nsCOMPtr<nsIComponentRegistrar> cr;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
  NS_ENSURE_SUCCESS(rv, rv);

  int numAppComps = sizeof(defaultAppComps) / sizeof(nsModuleComponentInfo);
  for (int i = 0; i < numAppComps; ++i) {
    nsCOMPtr<nsIGenericFactory> componentFactory;
    rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
                              &(defaultAppComps[i]));
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to create factory for component");
      continue;  // don't abort registering other components
    }

    rv = cr->RegisterFactory(defaultAppComps[i].mCID, defaultAppComps[i].mDescription,
                             defaultAppComps[i].mContractID, componentFactory);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to register factory for component");
  }

  return rv;
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
        QGeckoEmbed *tmpPrivate = static_cast<QGeckoEmbed *>
                                                (sWindowList->ElementAt(i));
        // get the browser object for that window
        nsIWebBrowserChrome *chrome = static_cast<nsIWebBrowserChrome *>
                                                     (tmpPrivate->window());
        if (chrome == aBrowser)
            return tmpPrivate;
    }

    return nsnull;
}
