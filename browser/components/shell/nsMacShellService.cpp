/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CocoaFileUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIImageLoadingContent.h"
#include "mozilla/dom/Document.h"
#include "nsComponentManagerUtils.h"
#include "nsIContent.h"
#include "nsICookieJarSettings.h"
#include "nsIObserverService.h"
#include "nsIWebBrowserPersist.h"
#include "nsMacShellService.h"
#include "nsIProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsShellService.h"
#include "nsString.h"
#include "nsIDocShell.h"
#include "nsILoadContext.h"
#include "nsIPrefService.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "DesktopBackgroundImage.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

using mozilla::dom::Element;
using mozilla::widget::SetDesktopImage;

#define NETWORK_PREFPANE "/System/Library/PreferencePanes/Network.prefPane"_ns
#define DESKTOP_PREFPANE \
  nsLiteralCString(      \
      "/System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane")

#define SAFARI_BUNDLE_IDENTIFIER "com.apple.Safari"

NS_IMPL_ISUPPORTS(nsMacShellService, nsIMacShellService, nsIShellService,
                  nsIToolkitShellService, nsIWebProgressListener)

NS_IMETHODIMP
nsMacShellService::IsDefaultBrowser(bool aForAllTypes,
                                    bool* aIsDefaultBrowser) {
  *aIsDefaultBrowser = false;

  CFStringRef firefoxID = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
  if (!firefoxID) {
    // CFBundleGetIdentifier is expected to return nullptr only if the specified
    // bundle doesn't have a bundle identifier in its plist. In this case, that
    // means a failure, since our bundle does have an identifier.
    return NS_ERROR_FAILURE;
  }

  // Get the default http handler's bundle ID (or nullptr if it has not been
  // explicitly set)
  CFStringRef defaultBrowserID =
      ::LSCopyDefaultHandlerForURLScheme(CFSTR("http"));
  if (defaultBrowserID) {
    *aIsDefaultBrowser =
        ::CFStringCompare(firefoxID, defaultBrowserID, 0) == kCFCompareEqualTo;
    ::CFRelease(defaultBrowserID);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::SetDefaultBrowser(bool aForAllUsers) {
  // Note: We don't support aForAllUsers on Mac OS X.

  CFStringRef firefoxID = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
  if (!firefoxID) {
    return NS_ERROR_FAILURE;
  }

  if (::LSSetDefaultHandlerForURLScheme(CFSTR("http"), firefoxID) != noErr) {
    return NS_ERROR_FAILURE;
  }
  if (::LSSetDefaultHandlerForURLScheme(CFSTR("https"), firefoxID) != noErr) {
    return NS_ERROR_FAILURE;
  }

  if (::LSSetDefaultRoleHandlerForContentType(kUTTypeHTML, kLSRolesAll,
                                              firefoxID) != noErr) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    (void)prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, true);
    // Reset the number of times the dialog should be shown
    // before it is silenced.
    (void)prefs->SetIntPref(PREF_DEFAULTBROWSERCHECKCOUNT, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::SetDesktopBackground(Element* aElement, int32_t aPosition,
                                        const nsACString& aImageName) {
  // Note: We don't support aPosition on OS X.

  // Get the image URI:
  nsresult rv;
  nsCOMPtr<nsIImageLoadingContent> imageContent =
      do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> imageURI;
  rv = imageContent->GetCurrentURI(getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIURI* docURI = aElement->OwnerDoc()->GetDocumentURI();
  if (!docURI) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIProperties> fileLocator(
      do_GetService("@mozilla.org/file/directory_service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the current user's "Pictures" folder (That's ~/Pictures):
  fileLocator->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, NS_GET_IID(nsIFile),
                   getter_AddRefs(mBackgroundFile));
  if (!mBackgroundFile) return NS_ERROR_OUT_OF_MEMORY;

  nsAutoString fileNameUnicode;
  CopyUTF8toUTF16(aImageName, fileNameUnicode);

  // and add the imgage file name itself:
  mBackgroundFile->Append(fileNameUnicode);

  // Download the image; the desktop background will be set in OnStateChange()
  nsCOMPtr<nsIWebBrowserPersist> wbp(do_CreateInstance(
      "@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags = nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION |
                   nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                   nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;

  wbp->SetPersistFlags(flags);
  wbp->SetProgressListener(this);

  nsCOMPtr<nsILoadContext> loadContext;
  nsCOMPtr<nsISupports> container = aElement->OwnerDoc()->GetContainer();
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
  if (docShell) {
    loadContext = do_QueryInterface(docShell);
  }

  auto referrerInfo =
      mozilla::MakeRefPtr<mozilla::dom::ReferrerInfo>(*aElement);

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      aElement->OwnerDoc()->CookieJarSettings();
  return wbp->SaveURI(imageURI, aElement->NodePrincipal(), 0, referrerInfo,
                      cookieJarSettings, nullptr, nullptr, mBackgroundFile,
                      nsIContentPolicy::TYPE_IMAGE,
                      loadContext->UsePrivateBrowsing());
}

NS_IMETHODIMP
nsMacShellService::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    int32_t aCurSelfProgress,
                                    int32_t aMaxSelfProgress,
                                    int32_t aCurTotalProgress,
                                    int32_t aMaxTotalProgress) {
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, nsIURI* aLocation,
                                    uint32_t aFlags) {
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest, nsresult aStatus,
                                  const char16_t* aMessage) {
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, uint32_t aState) {
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aEvent) {
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest, uint32_t aStateFlags,
                                 nsresult aStatus) {
  if (NS_SUCCEEDED(aStatus) && (aStateFlags & STATE_STOP) &&
      (aRequest == nullptr)) {
    nsCOMPtr<nsIObserverService> os(
        do_GetService("@mozilla.org/observer-service;1"));
    if (os)
      os->NotifyObservers(nullptr, "shell:desktop-background-changed", nullptr);

    bool exists = false;
    nsresult rv = mBackgroundFile->Exists(&exists);
    if (NS_FAILED(rv) || !exists) {
      return NS_OK;
    }

    SetDesktopImage(mBackgroundFile);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::ShowDesktopPreferences() {
  nsCOMPtr<nsIFile> lf;
  nsresult rv =
      NS_NewNativeLocalFile(DESKTOP_PREFPANE, true, getter_AddRefs(lf));
  NS_ENSURE_SUCCESS(rv, rv);
  bool exists;
  lf->Exists(&exists);
  if (!exists) return NS_ERROR_FILE_NOT_FOUND;
  return lf->Launch();
}

NS_IMETHODIMP
nsMacShellService::GetDesktopBackgroundColor(uint32_t* aColor) {
  // This method and |SetDesktopBackgroundColor| has no meaning on Mac OS X.
  // The mac desktop preferences UI uses pictures for the few solid colors it
  // supports.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMacShellService::SetDesktopBackgroundColor(uint32_t aColor) {
  // This method and |GetDesktopBackgroundColor| has no meaning on Mac OS X.
  // The mac desktop preferences UI uses pictures for the few solid colors it
  // supports.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMacShellService::ShowSecurityPreferences(const nsACString& aPaneID) {
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  CFStringRef paneID = ::CFStringCreateWithBytes(
      kCFAllocatorDefault, (const UInt8*)PromiseFlatCString(aPaneID).get(),
      aPaneID.Length(), kCFStringEncodingUTF8, false);

  if (paneID) {
    CFStringRef format =
        CFSTR("x-apple.systempreferences:com.apple.preference.security?%@");
    if (format) {
      CFStringRef urlStr =
          CFStringCreateWithFormat(kCFAllocatorDefault, NULL, format, paneID);
      if (urlStr) {
        CFURLRef url = ::CFURLCreateWithString(NULL, urlStr, NULL);
        rv = CocoaFileUtils::OpenURL(url);

        ::CFRelease(urlStr);
      }

      ::CFRelease(format);
    }

    ::CFRelease(paneID);
  }
  return rv;
}
