/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
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

#include "nsWebNavigationInfo.h"
#include "nsIWebNavigation.h"
#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIPluginManager.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

NS_IMPL_ISUPPORTS1(nsWebNavigationInfo, nsIWebNavigationInfo)

#define CONTENT_DLF_CONTRACT "@mozilla.org/content/document-loader-factory;1"
#define PLUGIN_DLF_CONTRACT \
    "@mozilla.org/content/plugin/document-loader-factory;1"

nsresult
nsWebNavigationInfo::Init()
{
  nsresult rv;
  mCategoryManager = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mImgLoader = do_GetService("@mozilla.org/image/loader;1", &rv);

  return rv;
}

NS_IMETHODIMP
nsWebNavigationInfo::IsTypeSupported(const nsACString& aType,
                                     nsIWebNavigation* aWebNav,
                                     PRUint32* aIsTypeSupported)
{
  NS_PRECONDITION(aIsTypeSupported, "null out param?");

  // Note to self: aWebNav could be an nsWebBrowser or an nsDocShell here (or
  // an nsSHistory, but not much we can do with that).  So if we start using
  // it here, we need to be careful to get to the docshell correctly.
  
  // For now just report what the Gecko-Content-Viewers category has
  // to say for itself.
  *aIsTypeSupported = nsIWebNavigationInfo::UNSUPPORTED;

  const nsCString& flatType = PromiseFlatCString(aType);
  nsresult rv = IsTypeSupportedInternal(flatType, aIsTypeSupported);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*aIsTypeSupported) {
    return rv;
  }
  
  // Try reloading plugins in case they've changed.
  nsCOMPtr<nsIPluginManager> pluginManager =
    do_GetService("@mozilla.org/plugin/manager;1");
  if (pluginManager) {
    // PR_FALSE will ensure that currently running plugins will not
    // be shut down
    rv = pluginManager->ReloadPlugins(PR_FALSE);
    if (NS_SUCCEEDED(rv)) {
      // OK, we reloaded plugins and there were new ones
      // (otherwise NS_ERROR_PLUGINS_PLUGINSNOTCHANGED would have
      // been returned).  Try checking whether we can handle the
      // content now.
      return IsTypeSupportedInternal(flatType, aIsTypeSupported);
    }
  }

  return NS_OK;
}

nsresult
nsWebNavigationInfo::IsTypeSupportedInternal(const nsCString& aType,
                                             PRUint32* aIsSupported)
{
  NS_PRECONDITION(mCategoryManager, "Must have category manager");
  NS_PRECONDITION(aIsSupported, "Null out param?");

  nsXPIDLCString value;
  nsresult rv = mCategoryManager->GetCategoryEntry("Gecko-Content-Viewers",
                                                   aType.get(),
                                                   getter_Copies(value));

  // If the category manager can't find what we're looking for
  // it returns NS_ERROR_NOT_AVAILABLE, we don't want to propagate
  // that to the caller since it's really not a failure

  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)
    return rv;

  // Now try to get an actual document loader factory for this contractid.  If
  // there is no contractid, don't try and just return false for *aIsSupported.
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;
  if (!value.IsEmpty()) {
    docLoaderFactory = do_GetService(value.get());
  }

  // If we got a factory, we should be able to handle this type
  if (!docLoaderFactory) {
    *aIsSupported = nsIWebNavigationInfo::UNSUPPORTED;
  }
  else if (value.EqualsLiteral(CONTENT_DLF_CONTRACT)) {
    PRBool isImage = PR_FALSE;
    mImgLoader->SupportImageWithMimeType(aType.get(), &isImage);
    if (isImage) {
      *aIsSupported = nsIWebNavigationInfo::IMAGE;
    }
    else {
      *aIsSupported = nsIWebNavigationInfo::OTHER;
    }
  }
  else if (value.EqualsLiteral(PLUGIN_DLF_CONTRACT)) {
#ifdef ACCESSIBILITY
    // If a screen reader is running, prefer external handlers over plugins
    // because they are better at dealing with accessibility -- they don't
    // have the keyboard navigation problems and are more likely to expose
    // their content via MSAA.
    // XXX Eventually we will remove this once the major content types are as
    //     screen reader accessible in plugins as in external apps.
    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);
    if (lookAndFeel) {
      PRInt32 isScreenReaderActive;
      lookAndFeel->GetMetric(nsILookAndFeel::eMetric_IsScreenReaderActive,
                            isScreenReaderActive);
      if (isScreenReaderActive) {
        nsCOMPtr<nsIMIMEService> mimeService(do_GetService("@mozilla.org/mime;1"));
        if (mimeService) {
          nsCOMPtr<nsIMIMEInfo> mimeInfo;
          mimeService->GetFromTypeAndExtension(aType, EmptyCString(),
                                              getter_AddRefs(mimeInfo));
          if (mimeInfo) {
            PRBool hasDefaultHandler;
            mimeInfo->GetHasDefaultHandler(&hasDefaultHandler);
            if (hasDefaultHandler) {
              // External app exists to handle this type, so use it instead of PLUGIN
              *aIsSupported = nsIWebNavigationInfo::UNSUPPORTED;
              return NS_OK;
            }
          }
        }
      }
    }
#endif
    *aIsSupported = nsIWebNavigationInfo::PLUGIN;
  }
  else {
    *aIsSupported = nsIWebNavigationInfo::OTHER;
  }
  
  return NS_OK;
}
