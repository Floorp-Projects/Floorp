/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDirectoryServiceDefs.h"
#include "nsIImageLoadingContent.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsILocalFileMac.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsIURL.h"
#include "nsIWebBrowserPersist.h"
#include "nsMacShellService.h"
#include "nsIProperties.h"
#include "nsServiceManagerUtils.h"
#include "nsShellService.h"
#include "nsString.h"
#include "nsIDocShell.h"
#include "nsILoadContext.h"
#include "mozilla/dom/Element.h"

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

using mozilla::dom::Element;

#define NETWORK_PREFPANE NS_LITERAL_CSTRING("/System/Library/PreferencePanes/Network.prefPane")
#define DESKTOP_PREFPANE NS_LITERAL_CSTRING("/System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane")

#define SAFARI_BUNDLE_IDENTIFIER "com.apple.Safari"

NS_IMPL_ISUPPORTS(nsMacShellService, nsIMacShellService, nsIShellService, nsIWebProgressListener)

NS_IMETHODIMP
nsMacShellService::IsDefaultBrowser(bool aStartupCheck,
                                    bool aForAllTypes,
                                    bool* aIsDefaultBrowser)
{
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
  CFStringRef defaultBrowserID = ::LSCopyDefaultHandlerForURLScheme(CFSTR("http"));
  if (defaultBrowserID) {
    *aIsDefaultBrowser = ::CFStringCompare(firefoxID, defaultBrowserID, 0) == kCFCompareEqualTo;
    ::CFRelease(defaultBrowserID);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::SetDefaultBrowser(bool aClaimAllTypes, bool aForAllUsers)
{
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

  if (aClaimAllTypes) {
    if (::LSSetDefaultHandlerForURLScheme(CFSTR("ftp"), firefoxID) != noErr) {
      return NS_ERROR_FAILURE;
    }
    if (::LSSetDefaultRoleHandlerForContentType(kUTTypeHTML, kLSRolesAll, firefoxID) != noErr) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    (void) prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, true);
    // Reset the number of times the dialog should be shown
    // before it is silenced.
    (void) prefs->SetIntPref(PREF_DEFAULTBROWSERCHECKCOUNT, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::SetDesktopBackground(Element* aElement,
                                        int32_t aPosition,
                                        const nsACString& aImageName)
{
  // Note: We don't support aPosition on OS X.

  // Get the image URI:
  nsresult rv;
  nsCOMPtr<nsIImageLoadingContent> imageContent = do_QueryInterface(aElement,
                                                                    &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> imageURI;
  rv = imageContent->GetCurrentURI(getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIURI *docURI = aElement->OwnerDoc()->GetDocumentURI();
  if (!docURI)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIProperties> fileLocator
    (do_GetService("@mozilla.org/file/directory_service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the current user's "Pictures" folder (That's ~/Pictures):
  fileLocator->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, NS_GET_IID(nsIFile),
                   getter_AddRefs(mBackgroundFile));
  if (!mBackgroundFile)
    return NS_ERROR_OUT_OF_MEMORY;

  nsAutoString fileNameUnicode;
  CopyUTF8toUTF16(aImageName, fileNameUnicode);

  // and add the imgage file name itself:
  mBackgroundFile->Append(fileNameUnicode);

  // Download the image; the desktop background will be set in OnStateChange()
  nsCOMPtr<nsIWebBrowserPersist> wbp
    (do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv));
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

  return wbp->SaveURI(imageURI, 0,
                      docURI, aElement->OwnerDoc()->GetReferrerPolicy(),
                      nullptr, nullptr,
                      mBackgroundFile, loadContext);
}

NS_IMETHODIMP
nsMacShellService::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    int32_t aCurSelfProgress,
                                    int32_t aMaxSelfProgress,
                                    int32_t aCurTotalProgress,
                                    int32_t aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI* aLocation,
                                    uint32_t aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const char16_t* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    uint32_t aState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 uint32_t aStateFlags,
                                 nsresult aStatus)
{
  if (aStateFlags & STATE_STOP) {
    nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
    if (os)
      os->NotifyObservers(nullptr, "shell:desktop-background-changed", nullptr);

    bool exists = false;
    mBackgroundFile->Exists(&exists);
    if (!exists)
      return NS_OK;

    nsAutoCString nativePath;
    mBackgroundFile->GetNativePath(nativePath);

    AEDesc tAEDesc = { typeNull, nil };
    OSErr err = noErr;
    AliasHandle aliasHandle = nil;
    FSRef pictureRef;
    OSStatus status;

    // Convert the path into a FSRef
    status = ::FSPathMakeRef((const UInt8*)nativePath.get(), &pictureRef,
                             nullptr);
    if (status == noErr) {
      err = ::FSNewAlias(nil, &pictureRef, &aliasHandle);
      if (err == noErr && aliasHandle == nil)
        err = paramErr;

      if (err == noErr) {
        // We need the descriptor (based on the picture file reference)
        // for the 'Set Desktop Picture' apple event.
        char handleState = ::HGetState((Handle)aliasHandle);
        ::HLock((Handle)aliasHandle);
        err = ::AECreateDesc(typeAlias, *aliasHandle,
                             GetHandleSize((Handle)aliasHandle), &tAEDesc);
        // unlock the alias handler
        ::HSetState((Handle)aliasHandle, handleState);
        ::DisposeHandle((Handle)aliasHandle);
      }
      if (err == noErr) {
        AppleEvent tAppleEvent;
        OSType sig = 'MACS';
        AEBuildError tAEBuildError;
        // Create a 'Set Desktop Pictue' Apple Event
        err = ::AEBuildAppleEvent(kAECoreSuite, kAESetData, typeApplSignature,
                                  &sig, sizeof(OSType), kAutoGenerateReturnID,
                                  kAnyTransactionID, &tAppleEvent, &tAEBuildError,
                                  "'----':'obj '{want:type (prop),form:prop" \
                                  ",seld:type('dpic'),from:'null'()},data:(@)",
                                  &tAEDesc);
        if (err == noErr) {
          AppleEvent reply = { typeNull, nil };
          // Sent the event we built, the reply event isn't necessary
          err = ::AESend(&tAppleEvent, &reply, kAENoReply, kAENormalPriority,
                         kNoTimeOut, nil, nil);
          ::AEDisposeDesc(&tAppleEvent);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OpenApplication(int32_t aApplication)
{
  nsresult rv = NS_OK;
  CFURLRef appURL = nil;
  OSStatus err = noErr;

  switch (aApplication) {
  case nsIShellService::APPLICATION_MAIL:
    {
      CFURLRef tempURL = ::CFURLCreateWithString(kCFAllocatorDefault,
                                                 CFSTR("mailto:"), nullptr);
      err = ::LSGetApplicationForURL(tempURL, kLSRolesAll, nullptr, &appURL);
      ::CFRelease(tempURL);
    }
    break;
  case nsIShellService::APPLICATION_NEWS:
    {
      CFURLRef tempURL = ::CFURLCreateWithString(kCFAllocatorDefault,
                                                 CFSTR("news:"), nullptr);
      err = ::LSGetApplicationForURL(tempURL, kLSRolesAll, nullptr, &appURL);
      ::CFRelease(tempURL);
    }
    break;
  case nsIMacShellService::APPLICATION_KEYCHAIN_ACCESS:
    err = ::LSGetApplicationForInfo('APPL', 'kcmr', nullptr, kLSRolesAll,
                                    nullptr, &appURL);
    break;
  case nsIMacShellService::APPLICATION_NETWORK:
    {
      nsCOMPtr<nsIFile> lf;
      rv = NS_NewNativeLocalFile(NETWORK_PREFPANE, true, getter_AddRefs(lf));
      NS_ENSURE_SUCCESS(rv, rv);
      bool exists;
      lf->Exists(&exists);
      if (!exists)
        return NS_ERROR_FILE_NOT_FOUND;
      return lf->Launch();
    }
  case nsIMacShellService::APPLICATION_DESKTOP:
    {
      nsCOMPtr<nsIFile> lf;
      rv = NS_NewNativeLocalFile(DESKTOP_PREFPANE, true, getter_AddRefs(lf));
      NS_ENSURE_SUCCESS(rv, rv);
      bool exists;
      lf->Exists(&exists);
      if (!exists)
        return NS_ERROR_FILE_NOT_FOUND;
      return lf->Launch();
    }
  }

  if (appURL && err == noErr) {
    err = ::LSOpenCFURLRef(appURL, nullptr);
    rv = err != noErr ? NS_ERROR_FAILURE : NS_OK;

    ::CFRelease(appURL);
  }

  return rv;
}

NS_IMETHODIMP
nsMacShellService::GetDesktopBackgroundColor(uint32_t *aColor)
{
  // This method and |SetDesktopBackgroundColor| has no meaning on Mac OS X.
  // The mac desktop preferences UI uses pictures for the few solid colors it
  // supports.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMacShellService::SetDesktopBackgroundColor(uint32_t aColor)
{
  // This method and |GetDesktopBackgroundColor| has no meaning on Mac OS X.
  // The mac desktop preferences UI uses pictures for the few solid colors it
  // supports.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMacShellService::OpenApplicationWithURI(nsIFile* aApplication, const nsACString& aURI)
{
  nsCOMPtr<nsILocalFileMac> lfm(do_QueryInterface(aApplication));
  CFURLRef appURL;
  nsresult rv = lfm->GetCFURL(&appURL);
  if (NS_FAILED(rv))
    return rv;

  const nsCString spec(aURI);
  const UInt8* uriString = (const UInt8*)spec.get();
  CFURLRef uri = ::CFURLCreateWithBytes(nullptr, uriString, aURI.Length(),
                                        kCFStringEncodingUTF8, nullptr);
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;

  CFArrayRef uris = ::CFArrayCreate(nullptr, (const void**)&uri, 1, nullptr);
  if (!uris) {
    ::CFRelease(uri);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  LSLaunchURLSpec launchSpec;
  launchSpec.appURL = appURL;
  launchSpec.itemURLs = uris;
  launchSpec.passThruParams = nullptr;
  launchSpec.launchFlags = kLSLaunchDefaults;
  launchSpec.asyncRefCon = nullptr;

  OSErr err = ::LSOpenFromURLSpec(&launchSpec, nullptr);

  ::CFRelease(uris);
  ::CFRelease(uri);

  return err != noErr ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsMacShellService::GetDefaultFeedReader(nsIFile** _retval)
{
  nsresult rv = NS_ERROR_FAILURE;
  *_retval = nullptr;

  CFStringRef defaultHandlerID = ::LSCopyDefaultHandlerForURLScheme(CFSTR("feed"));
  if (!defaultHandlerID) {
    defaultHandlerID = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                                   SAFARI_BUNDLE_IDENTIFIER,
                                                   kCFStringEncodingASCII);
  }

  CFURLRef defaultHandlerURL = nullptr;
  OSStatus status = ::LSFindApplicationForInfo(kLSUnknownCreator,
                                               defaultHandlerID,
                                               nullptr, // inName
                                               nullptr, // outAppRef
                                               &defaultHandlerURL);

  if (status == noErr && defaultHandlerURL) {
    nsCOMPtr<nsILocalFileMac> defaultReader =
      do_CreateInstance("@mozilla.org/file/local;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = defaultReader->InitWithCFURL(defaultHandlerURL);
      if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(*_retval = defaultReader);
        rv = NS_OK;
      }
    }

    ::CFRelease(defaultHandlerURL);
  }

  ::CFRelease(defaultHandlerID);

  return rv;
}
