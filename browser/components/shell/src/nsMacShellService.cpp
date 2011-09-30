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
 * The Original Code is Shell Service.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@mozilla.org> (Original Author)
 *   Asaf Romano <mozilla.mano@sent.com>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsDirectoryServiceDefs.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLImageElement.h"
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
#include "nsNetUtil.h"
#include "nsShellService.h"
#include "nsStringAPI.h"

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

#define NETWORK_PREFPANE NS_LITERAL_CSTRING("/System/Library/PreferencePanes/Network.prefPane")
#define DESKTOP_PREFPANE NS_LITERAL_CSTRING("/System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane")

#define SAFARI_BUNDLE_IDENTIFIER "com.apple.Safari"

NS_IMPL_ISUPPORTS3(nsMacShellService, nsIMacShellService, nsIShellService, nsIWebProgressListener)

NS_IMETHODIMP
nsMacShellService::IsDefaultBrowser(bool aStartupCheck, bool* aIsDefaultBrowser)
{
  *aIsDefaultBrowser = PR_FALSE;

  CFStringRef firefoxID = ::CFBundleGetIdentifier(::CFBundleGetMainBundle());
  if (!firefoxID) {
    // CFBundleGetIdentifier is expected to return NULL only if the specified
    // bundle doesn't have a bundle identifier in its plist. In this case, that
    // means a failure, since our bundle does have an identifier.
    return NS_ERROR_FAILURE;
  }

  // Get the default http handler's bundle ID (or NULL if it has not been explicitly set)
  CFStringRef defaultBrowserID = ::LSCopyDefaultHandlerForURLScheme(CFSTR("http"));
  if (defaultBrowserID) {
    *aIsDefaultBrowser = ::CFStringCompare(firefoxID, defaultBrowserID, 0) == kCFCompareEqualTo;
    ::CFRelease(defaultBrowserID);
  }

  // If this is the first browser window, maintain internal state that we've
  // checked this session (so that subsequent window opens don't show the 
  // default browser dialog).
  if (aStartupCheck)
    mCheckedThisSession = PR_TRUE;

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
  
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::GetShouldCheckDefaultBrowser(bool* aResult)
{
  // If we've already checked, the browser has been started and this is a 
  // new window open, and we don't want to check again.
  if (mCheckedThisSession) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  prefs->GetBoolPref(PREF_CHECKDEFAULTBROWSER, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::SetShouldCheckDefaultBrowser(bool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs;
  nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pserve)
    pserve->GetBranch("", getter_AddRefs(prefs));

  prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheck);

  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::SetDesktopBackground(nsIDOMElement* aElement, 
                                        PRInt32 aPosition)
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

  // We need the referer URI for nsIWebBrowserPersist::saveURI
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocument> doc;
  doc = content->GetOwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;

  nsIURI *docURI = doc->GetDocumentURI();
  if (!docURI)
    return NS_ERROR_FAILURE;

  // Get the desired image file name
  nsCOMPtr<nsIURL> imageURL(do_QueryInterface(imageURI));
  if (!imageURL) {
    // XXXmano (bug 300293): Non-URL images (e.g. the data: protocol) are not
    // yet supported. What filename should we take here?
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCAutoString fileName;
  imageURL->GetFileName(fileName);
  nsCOMPtr<nsIProperties> fileLocator
    (do_GetService("@mozilla.org/file/directory_service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the current user's "Pictures" folder (That's ~/Pictures):
  fileLocator->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(mBackgroundFile));
  if (!mBackgroundFile)
    return NS_ERROR_OUT_OF_MEMORY;

  nsAutoString fileNameUnicode;
  CopyUTF8toUTF16(fileName, fileNameUnicode);

  // and add the imgage file name itself:
  mBackgroundFile->Append(fileNameUnicode);

  // Download the image; the desktop background will be set in OnStateChange()
  nsCOMPtr<nsIWebBrowserPersist> wbp
    (do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION | 
                   nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                   nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;

  wbp->SetPersistFlags(flags);
  wbp->SetProgressListener(this);

  return wbp->SaveURI(imageURI, nsnull, docURI, nsnull, nsnull,
                      mBackgroundFile);
}

NS_IMETHODIMP
nsMacShellService::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress,
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI* aLocation)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRUint32 aState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMacShellService::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 PRUint32 aStateFlags,
                                 nsresult aStatus)
{
  if (aStateFlags & STATE_STOP) {
    nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
    if (os)
      os->NotifyObservers(nsnull, "shell:desktop-background-changed", nsnull);

    bool exists = false;
    mBackgroundFile->Exists(&exists);
    if (!exists)
      return NS_OK;

    nsCAutoString nativePath;
    mBackgroundFile->GetNativePath(nativePath);

    AEDesc tAEDesc = { typeNull, nil };
    OSErr err = noErr;
    AliasHandle aliasHandle = nil;
    FSRef pictureRef;
    OSStatus status;

    // Convert the path into a FSRef
    status = ::FSPathMakeRef((const UInt8*)nativePath.get(), &pictureRef, NULL);
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
nsMacShellService::OpenApplication(PRInt32 aApplication)
{
  nsresult rv = NS_OK;
  CFURLRef appURL = nil;
  OSStatus err = noErr;

  switch (aApplication) {
  case nsIShellService::APPLICATION_MAIL:
    {
      CFURLRef tempURL = ::CFURLCreateWithString(kCFAllocatorDefault,
                                                 CFSTR("mailto:"), NULL);
      err = ::LSGetApplicationForURL(tempURL, kLSRolesAll, NULL, &appURL);
      ::CFRelease(tempURL);
    }
    break;
  case nsIShellService::APPLICATION_NEWS:
    {
      CFURLRef tempURL = ::CFURLCreateWithString(kCFAllocatorDefault,
                                                 CFSTR("news:"), NULL);
      err = ::LSGetApplicationForURL(tempURL, kLSRolesAll, NULL, &appURL);
      ::CFRelease(tempURL);
    }
    break;
  case nsIMacShellService::APPLICATION_KEYCHAIN_ACCESS:
    err = ::LSGetApplicationForInfo('APPL', 'kcmr', NULL, kLSRolesAll, NULL,
                                    &appURL);
    break;
  case nsIMacShellService::APPLICATION_NETWORK:
    {
      nsCOMPtr<nsILocalFile> lf;
      rv = NS_NewNativeLocalFile(NETWORK_PREFPANE, PR_TRUE, getter_AddRefs(lf));
      NS_ENSURE_SUCCESS(rv, rv);
      bool exists;
      lf->Exists(&exists);
      if (!exists)
        return NS_ERROR_FILE_NOT_FOUND;
      return lf->Launch();
    }  
    break;
  case nsIMacShellService::APPLICATION_DESKTOP:
    {
      nsCOMPtr<nsILocalFile> lf;
      rv = NS_NewNativeLocalFile(DESKTOP_PREFPANE, PR_TRUE, getter_AddRefs(lf));
      NS_ENSURE_SUCCESS(rv, rv);
      bool exists;
      lf->Exists(&exists);
      if (!exists)
        return NS_ERROR_FILE_NOT_FOUND;
      return lf->Launch();
    }  
    break;
  }

  if (appURL && err == noErr) {
    err = ::LSOpenCFURLRef(appURL, NULL);
    rv = err != noErr ? NS_ERROR_FAILURE : NS_OK;

    ::CFRelease(appURL);
  }

  return rv;
}

NS_IMETHODIMP
nsMacShellService::GetDesktopBackgroundColor(PRUint32 *aColor)
{
  // This method and |SetDesktopBackgroundColor| has no meaning on Mac OS X.
  // The mac desktop preferences UI uses pictures for the few solid colors it
  // supports.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMacShellService::SetDesktopBackgroundColor(PRUint32 aColor)
{
  // This method and |GetDesktopBackgroundColor| has no meaning on Mac OS X.
  // The mac desktop preferences UI uses pictures for the few solid colors it
  // supports.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMacShellService::OpenApplicationWithURI(nsILocalFile* aApplication, const nsACString& aURI)
{
  nsCOMPtr<nsILocalFileMac> lfm(do_QueryInterface(aApplication));
  CFURLRef appURL;
  nsresult rv = lfm->GetCFURL(&appURL);
  if (NS_FAILED(rv))
    return rv;
  
  const nsCString spec(aURI);
  const UInt8* uriString = (const UInt8*)spec.get();
  CFURLRef uri = ::CFURLCreateWithBytes(NULL, uriString, aURI.Length(),
                                        kCFStringEncodingUTF8, NULL);
  if (!uri) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  CFArrayRef uris = ::CFArrayCreate(NULL, (const void**)&uri, 1, NULL);
  if (!uris) {
    ::CFRelease(uri);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  LSLaunchURLSpec launchSpec;
  launchSpec.appURL = appURL;
  launchSpec.itemURLs = uris;
  launchSpec.passThruParams = NULL;
  launchSpec.launchFlags = kLSLaunchDefaults;
  launchSpec.asyncRefCon = NULL;
  
  OSErr err = ::LSOpenFromURLSpec(&launchSpec, NULL);
  
  ::CFRelease(uris);
  ::CFRelease(uri);
  
  return err != noErr ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsMacShellService::GetDefaultFeedReader(nsILocalFile** _retval)
{
  nsresult rv = NS_ERROR_FAILURE;
  *_retval = nsnull;

  CFStringRef defaultHandlerID = ::LSCopyDefaultHandlerForURLScheme(CFSTR("feed"));
  if (!defaultHandlerID) {
    defaultHandlerID = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                                   SAFARI_BUNDLE_IDENTIFIER,
                                                   kCFStringEncodingASCII);
  }

  CFURLRef defaultHandlerURL = NULL;
  OSStatus status = ::LSFindApplicationForInfo(kLSUnknownCreator,
                                               defaultHandlerID,
                                               NULL, // inName
                                               NULL, // outAppRef
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
