/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCocoaBrowserService.h"

#include "nsIWindowWatcher.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIProfile.h"
#include "nsIPrefService.h"
#include "CHBrowserView.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIExternalHelperAppService.h"
#include "nsIDownload.h"

#include "nsINSSDialogs.h"
#include "nsIFactory.h"
#include "nsIComponentRegistrar.h"

// {0ffd3880-7a1a-11d6-a384-975d1d5f86fc}
#define NS_BADCERTHANDLER_CID \
  {0x0ffd3880, 0x7a1a, 0x11d6,{0xa3, 0x84, 0x97, 0x5d, 0x1d, 0x5f, 0x86, 0xfc}}

nsAlertController* nsCocoaBrowserService::sController = nsnull;
nsCocoaBrowserService* nsCocoaBrowserService::sSingleton = nsnull;
PRUint32 nsCocoaBrowserService::sNumBrowsers = 0;
PRBool nsCocoaBrowserService::sCanTerminate = PR_FALSE;

// nsCocoaBrowserService implementation
nsCocoaBrowserService::nsCocoaBrowserService()
{
  NS_INIT_ISUPPORTS();
}

nsCocoaBrowserService::~nsCocoaBrowserService()
{
}

NS_IMPL_ISUPPORTS9(nsCocoaBrowserService,
                   nsIWindowCreator,
                   nsIPromptService,
                   nsIFactory, 
                   nsIBadCertListener, nsISecurityWarningDialogs, nsINSSDialogs,
                   nsIHelperAppLauncherDialog, nsIDownload, nsIWebProgressListener)

nsresult
nsCocoaBrowserService::InitEmbedding()
{
  sNumBrowsers++;
  
  if (sSingleton)
    return NS_OK;

  sSingleton = new nsCocoaBrowserService();
  if (!sSingleton)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(sSingleton);
  
  nsCOMPtr<nsIComponentRegistrar> cr;
  NS_GetComponentRegistrar(getter_AddRefs(cr));
  if ( !cr )
    return NS_ERROR_FAILURE;

  // Register as the prompt service
  #define NS_PROMPTSERVICE_CID \
     {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
  static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);
	nsresult rv = cr->RegisterFactory(kPromptServiceCID, "Prompt Service", "@mozilla.org/embedcomp/prompt-service;1",
                                    sSingleton);
  if (NS_FAILED(rv))
    return rv;

  // Register as the window creator
  nsCOMPtr<nsIWindowWatcher> watcher(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  if (!watcher) 
    return NS_ERROR_FAILURE;
  watcher->SetWindowCreator(sSingleton);

  // replace gecko's cert dialogs with our own implementation that doesn't use XUL. We rely
  // on our own dialogs in the alert nib and use the alert controller to display them.
  static NS_DEFINE_CID(kBadCertHandlerCID, NS_BADCERTHANDLER_CID);
  rv = cr->RegisterFactory(kBadCertHandlerCID, "Bad Cert Handler", NS_NSSDIALOGS_CONTRACTID,
                            sSingleton);

  // replace the external helper app dialog with our own
  #define NS_HELPERAPPLAUNCHERDIALOG_CID \
          {0xf68578eb, 0x6ec2, 0x4169, {0xae, 0x19, 0x8c, 0x62, 0x43, 0xf0, 0xab, 0xe1}}
  static NS_DEFINE_CID(kHelperDlgCID, NS_HELPERAPPLAUNCHERDIALOG_CID);
  rv = cr->RegisterFactory(kHelperDlgCID, NS_IHELPERAPPLAUNCHERDLG_CLASSNAME, NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
                            sSingleton);
  
  // replace the downloader with our own which does rely on the xpfe downlaod manager
  static NS_DEFINE_CID(kDownloadCID, NS_DOWNLOAD_CID);
  rv = cr->RegisterFactory(kDownloadCID, "Download", NS_DOWNLOAD_CONTRACTID,
                            sSingleton);

  return rv;
}

void
nsCocoaBrowserService::BrowserClosed()
{
    sNumBrowsers--;
    if (sCanTerminate && sNumBrowsers == 0) {
        // The app is terminating *and* our count dropped to 0.
        NS_IF_RELEASE(sSingleton);
        NS_TermEmbedding();
#if DEBUG
        NSLog(@"Shutting down embedding!");
#endif
    }
}

void
nsCocoaBrowserService::TermEmbedding()
{
    sCanTerminate = PR_TRUE;
    if (sNumBrowsers == 0) {
        NS_IF_RELEASE(sSingleton);
        NS_TermEmbedding();
#if DEBUG
        NSLog(@"Shutting down embedding.");
#endif
    }
    else {
#if DEBUG
        NSLog(@"Cannot yet shut down embedding.");
#endif
        // Otherwise we cannot yet terminate.  We have to let the death of the browser windows
        // induce termination.
    }
}

#define NS_ALERT_NIB_NAME "alert"

nsAlertController* 
nsCocoaBrowserService::GetAlertController()
{
  if (!sController) {
    NSBundle* bundle = [NSBundle bundleForClass:[CHBrowserView class]];
    [bundle loadNibFile:@NS_ALERT_NIB_NAME externalNameTable:nsnull withZone:[NSApp zone]];
  }
  return sController;
}

void
nsCocoaBrowserService::SetAlertController(nsAlertController* aController)
{
  // XXX When should the controller be released?
  sController = aController;
  [sController retain];
}

// nsIFactory implementation
NS_IMETHODIMP 
nsCocoaBrowserService::CreateInstance(nsISupports *aOuter, 
                                      const nsIID & aIID, 
                                      void **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  return sSingleton->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP 
nsCocoaBrowserService::LockFactory(PRBool lock)
{
  return NS_OK;
}

// nsIPromptService implementation
static NSWindow*
GetNSWindowForDOMWindow(nsIDOMWindow* window)
{
  nsCOMPtr<nsIWindowWatcher> watcher(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  if (!watcher) {
    return nsnull;
  }
  
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  watcher->GetChromeForWindow(window, getter_AddRefs(chrome));
  if (!chrome) {
    return nsnull;
  }

  nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_QueryInterface(chrome));
  if (!siteWindow) {
    return nsnull;
  }

  NSWindow* nswin;
  nsresult rv = siteWindow->GetSiteWindow((void**)&nswin);
  if (NS_FAILED(rv))
    return nsnull;

  return nswin;
}

// Implementation of nsIPromptService
NS_IMETHODIMP 
nsCocoaBrowserService::Alert(nsIDOMWindow *parent, 
                             const PRUnichar *dialogTitle, 
                             const PRUnichar *text)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSWindow* window = GetNSWindowForDOMWindow(parent);
  if (!window)
    return NS_ERROR_FAILURE;

  [controller alert:window title:titleStr text:textStr];

  return NS_OK;
}

/* void alertCheck (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP 
nsCocoaBrowserService::AlertCheck(nsIDOMWindow *parent, 
                                  const PRUnichar *dialogTitle, 
                                  const PRUnichar *text, 
                                  const PRUnichar *checkMsg, 
                                  PRBool *checkValue)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }
 
  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSString* msgStr = [NSString stringWithCharacters:checkMsg length:(checkMsg ? nsCRT::strlen(checkMsg) : 0)];
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  if (checkValue) {
    BOOL valueBool = *checkValue ? YES : NO;
    
    [controller alertCheck:window title:titleStr text:textStr checkMsg:msgStr checkValue:&valueBool];
    
    *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
  }
  else {
    [controller alert:window title:titleStr text:textStr];    
  }
  
  return NS_OK;
}

/* boolean confirm (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text); */
NS_IMETHODIMP 
nsCocoaBrowserService::Confirm(nsIDOMWindow *parent, 
                               const PRUnichar *dialogTitle, 
                               const PRUnichar *text, 
                               PRBool *_retval)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  *_retval = (PRBool)[controller confirm:window title:titleStr text:textStr];
  
  return NS_OK;
}

/* boolean confirmCheck (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP 
nsCocoaBrowserService::ConfirmCheck(nsIDOMWindow *parent, 
                                    const PRUnichar *dialogTitle, 
                                    const PRUnichar *text, 
                                    const PRUnichar *checkMsg, 
                                    PRBool *checkValue, PRBool *_retval)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSString* msgStr = [NSString stringWithCharacters:checkMsg length:(checkMsg ? nsCRT::strlen(checkMsg) : 0)];
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  if (checkValue) {
    BOOL valueBool = *checkValue ? YES : NO;
    
    *_retval = (PRBool)[controller confirmCheck:window title:titleStr text:textStr checkMsg:msgStr checkValue:&valueBool];
    
    *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
  }
  else {
    *_retval = (PRBool)[controller confirm:window title:titleStr text:textStr];
  }
  
  return NS_OK;
}

NSString *
nsCocoaBrowserService::GetCommonDialogLocaleString(const char *key)
{
  NSString *returnValue = @"";

  nsresult rv;
  if (!mCommonDialogStringBundle) {
    #define kCommonDialogsStrings "chrome://global/locale/commonDialogs.properties"
    nsCOMPtr<nsIStringBundleService> service = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if ( service )
      rv = service->CreateBundle(kCommonDialogsStrings, getter_AddRefs(mCommonDialogStringBundle));
    else
      rv = NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) return returnValue;
  }

  nsXPIDLString string;
  rv = mCommonDialogStringBundle->GetStringFromName(NS_ConvertASCIItoUCS2(key).get(), getter_Copies(string));
  if (NS_FAILED(rv)) return returnValue;

  returnValue = [NSString stringWithCharacters:string length:(string ? nsCRT::strlen(string) : 0)];
  return returnValue;
}

NSString *
nsCocoaBrowserService::GetButtonStringFromFlags(PRUint32 btnFlags,
                                                PRUint32 btnIDAndShift,
                                                const PRUnichar *btnTitle)
{
  NSString *btnStr = nsnull;
  switch ((btnFlags >> btnIDAndShift) & 0xff) {
    case BUTTON_TITLE_OK:
      btnStr = GetCommonDialogLocaleString("OK");
      break;
    case BUTTON_TITLE_CANCEL:
      btnStr = GetCommonDialogLocaleString("Cancel");
      break;
    case BUTTON_TITLE_YES:
      btnStr = GetCommonDialogLocaleString("Yes");
      break;
    case BUTTON_TITLE_NO:
      btnStr = GetCommonDialogLocaleString("No");
      break;
    case BUTTON_TITLE_SAVE:
      btnStr = GetCommonDialogLocaleString("Save");
      break;
    case BUTTON_TITLE_DONT_SAVE:
      btnStr = GetCommonDialogLocaleString("DontSave");
      break;
    case BUTTON_TITLE_REVERT:
      btnStr = GetCommonDialogLocaleString("Revert");
      break;
    case BUTTON_TITLE_IS_STRING:
      btnStr = [NSString stringWithCharacters:btnTitle length:(btnTitle ? nsCRT::strlen(btnTitle) : 0)];
  }

  return btnStr;
}

// these constants are used for identifying the buttons and are intentionally overloaded to
// correspond to the number of bits needed for shifting to obtain the flags for a particular
// button (should be defined in nsIPrompt*.idl instead of here)
const PRUint32 kButton0 = 0;
const PRUint32 kButton1 = 8;
const PRUint32 kButton2 = 16;

/* void confirmEx (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in unsigned long buttonFlags, in wstring button0Title, in wstring button1Title, in wstring button2Title, in wstring checkMsg, inout boolean checkValue, out PRInt32 buttonPressed); */
NS_IMETHODIMP 
nsCocoaBrowserService::ConfirmEx(nsIDOMWindow *parent, 
                                 const PRUnichar *dialogTitle, 
                                 const PRUnichar *text, 
                                 PRUint32 buttonFlags, 
                                 const PRUnichar *button0Title, 
                                 const PRUnichar *button1Title, 
                                 const PRUnichar *button2Title, 
                                 const PRUnichar *checkMsg, 
                                 PRBool *checkValue, PRInt32 *buttonPressed)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSString* msgStr = [NSString stringWithCharacters:checkMsg length:(checkMsg ? nsCRT::strlen(checkMsg) : 0)];
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  NSString* btn1Str = GetButtonStringFromFlags(buttonFlags, kButton0, button0Title);
  NSString* btn2Str = GetButtonStringFromFlags(buttonFlags, kButton1, button1Title);
  NSString* btn3Str = GetButtonStringFromFlags(buttonFlags, kButton2, button2Title);
  
  if (checkValue) {
    BOOL valueBool = *checkValue ? YES : NO;
    
    *buttonPressed = [controller confirmCheckEx:window title:titleStr text:textStr 
                               button1: btn1Str button2: btn2Str button3: btn3Str
                               checkMsg:msgStr checkValue:&valueBool];
    
    *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
  }
  else {
    *buttonPressed = [controller confirmEx:window title:titleStr text:textStr
                               button1: btn1Str button2: btn2Str button3: btn3Str];
  }

  return NS_OK;

}

/* boolean prompt (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, inout wstring value, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP 
nsCocoaBrowserService::Prompt(nsIDOMWindow *parent, 
                              const PRUnichar *dialogTitle, 
                              const PRUnichar *text, 
                              PRUnichar **value, 
                              const PRUnichar *checkMsg, 
                              PRBool *checkValue, 
                              PRBool *_retval)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSString* msgStr = [NSString stringWithCharacters:checkMsg length:(checkMsg ? nsCRT::strlen(checkMsg) : 0)];
  NSMutableString* valueStr = [NSMutableString stringWithCharacters:*value length:(*value ? nsCRT::strlen(*value) : 0)];
    
  BOOL valueBool;
  if (checkValue) {
    valueBool = *checkValue ? YES : NO;
  }
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  *_retval = (PRBool)[controller prompt:window title:titleStr text:textStr promptText:valueStr checkMsg:msgStr checkValue:&valueBool doCheck:(checkValue != nsnull)];

  if (checkValue) {
    *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
  }
  PRUint32 length = [valueStr length];
  PRUnichar* retStr = (PRUnichar*)nsMemory::Alloc((length + 1) * sizeof(PRUnichar));
  [valueStr getCharacters:retStr];
  retStr[length] = PRUnichar(0);
  *value = retStr;

  return NS_OK;
}

/* boolean promptUsernameAndPassword (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, inout wstring username, inout wstring password, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP 
nsCocoaBrowserService::PromptUsernameAndPassword(nsIDOMWindow *parent, 
                                                 const PRUnichar *dialogTitle, 
                                                 const PRUnichar *text, 
                                                 PRUnichar **username, 
                                                 PRUnichar **password, 
                                                 const PRUnichar *checkMsg, 
                                                 PRBool *checkValue, 
                                                 PRBool *_retval)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSString* msgStr = [NSString stringWithCharacters:checkMsg length:(checkMsg ? nsCRT::strlen(checkMsg) : 0)];
  NSMutableString* userNameStr = [NSMutableString stringWithCharacters:*username length:(*username ? nsCRT::strlen(*username) : 0)];
  NSMutableString* passwordStr = [NSMutableString stringWithCharacters:*password length:(*password ? nsCRT::strlen(*password) : 0)];
    
  BOOL valueBool;
  if (checkValue) {
    valueBool = *checkValue ? YES : NO;
  }
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  *_retval = (PRBool)[controller promptUserNameAndPassword:window title:titleStr text:textStr userNameText:userNameStr passwordText:passwordStr checkMsg:msgStr checkValue:&valueBool doCheck:(checkValue != nsnull)];
  
  if (checkValue) {
    *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
  }

  PRUint32 length = [userNameStr length];
  PRUnichar* retStr = (PRUnichar*)nsMemory::Alloc((length + 1) * sizeof(PRUnichar));
  [userNameStr getCharacters:retStr];
  retStr[length] = PRUnichar(0);
  *username = retStr;

  length = [passwordStr length];
  retStr = (PRUnichar*)nsMemory::Alloc((length + 1) * sizeof(PRUnichar));
  [passwordStr getCharacters:retStr];
  retStr[length] = PRUnichar(0);
  *password = retStr;

  return NS_OK;
}

/* boolean promptPassword (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, inout wstring password, in wstring checkMsg, inout boolean checkValue); */
NS_IMETHODIMP 
nsCocoaBrowserService::PromptPassword(nsIDOMWindow *parent, 
                                      const PRUnichar *dialogTitle, 
                                      const PRUnichar *text, 
                                      PRUnichar **password, 
                                      const PRUnichar *checkMsg, 
                                      PRBool *checkValue, 
                                      PRBool *_retval)
{
  nsAlertController* controller = GetAlertController();
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  NSString* titleStr = [NSString stringWithCharacters:dialogTitle length:(dialogTitle ? nsCRT::strlen(dialogTitle) : 0)];
  NSString* textStr = [NSString stringWithCharacters:text length:(text ? nsCRT::strlen(text) : 0)];
  NSString* msgStr = [NSString stringWithCharacters:checkMsg length:(checkMsg ? nsCRT::strlen(checkMsg) : 0)];
  NSMutableString* passwordStr = [NSMutableString stringWithCharacters:*password length:(*password ? nsCRT::strlen(*password) : 0)];
    
  BOOL valueBool;
  if (checkValue) {
    valueBool = *checkValue ? YES : NO;
  }
  NSWindow* window = GetNSWindowForDOMWindow(parent);

  *_retval = (PRBool)[controller promptPassword:window title:titleStr text:textStr passwordText:passwordStr checkMsg:msgStr checkValue:&valueBool doCheck:(checkValue != nsnull)];

  if (checkValue) {
    *checkValue = (valueBool == YES) ? PR_TRUE : PR_FALSE;
  }

  PRUint32 length = [passwordStr length];
  PRUnichar* retStr = (PRUnichar*)nsMemory::Alloc((length + 1) * sizeof(PRUnichar));
  [passwordStr getCharacters:retStr];
  retStr[length] = PRUnichar(0);
  *password = retStr;

  return NS_OK;
}

/* boolean select (in nsIDOMWindow parent, in wstring dialogTitle, in wstring text, in PRUint32 count, [array, size_is (count)] in wstring selectList, out long outSelection); */
NS_IMETHODIMP 
nsCocoaBrowserService::Select(nsIDOMWindow *parent, 
                              const PRUnichar *dialogTitle, 
                              const PRUnichar *text, 
                              PRUint32 count, 
                              const PRUnichar **selectList, 
                              PRInt32 *outSelection, 
                              PRBool *_retval)
{
#if DEBUG
  NSLog(@"Uh-oh. Select has not been implemented.");
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Implementation of nsIWindowCreator
/* nsIWebBrowserChrome createChromeWindow (in nsIWebBrowserChrome parent, in PRUint32 chromeFlags); */
NS_IMETHODIMP 
nsCocoaBrowserService::CreateChromeWindow(nsIWebBrowserChrome *parent, 
                                          PRUint32 chromeFlags, 
                                          nsIWebBrowserChrome **_retval)
{
  if (!parent) {
#if DEBUG
    NSLog(@"Attempt to create a new browser window with a null parent.  Should not happen in Chimera.");
#endif
    return NS_ERROR_FAILURE;
  }
    
  nsCOMPtr<nsIWindowCreator> browserChrome(do_QueryInterface(parent));
  return browserChrome->CreateChromeWindow(parent, chromeFlags, _retval);
}


/* boolean unknownIssuer (in nsITransportSecurityInfo socketInfo,
                          in nsIX509Cert cert, out addType); */
NS_IMETHODIMP
nsCocoaBrowserService::UnknownIssuer(nsITransportSecurityInfo *socketInfo,
                                      nsIX509Cert *cert, PRInt16 *outAddType,
                                      PRBool *_retval)
{
  *_retval = PR_TRUE;
  *outAddType = ADD_TRUSTED_FOR_SESSION;

  nsAlertController* controller = GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *outAddType = (PRBool)[controller unknownCert:[NSApp mainWindow]];
  switch ( *outAddType ) {
    case nsIBadCertListener::ADD_TRUSTED_FOR_SESSION:
    case nsIBadCertListener::ADD_TRUSTED_PERMANENTLY:
      *_retval = PR_TRUE;
      break;  
    default:
      *_retval = PR_FALSE;
  }
  
  return NS_OK;
}

/* boolean mismatchDomain (in nsITransportSecurityInfo socketInfo, 
                           in wstring targetURL, 
                           in nsIX509Cert cert); */
NS_IMETHODIMP 
nsCocoaBrowserService::MismatchDomain(nsITransportSecurityInfo *socketInfo, 
                                        const PRUnichar *targetURL, 
                                        nsIX509Cert *cert, PRBool *_retval) 
{
  nsAlertController* controller = GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *_retval = (PRBool)[controller badCert:[NSApp mainWindow]];
  
  return NS_OK;
}


/* boolean certExpired (in nsITransportSecurityInfo socketInfo, 
                        in nsIX509Cert cert); */
NS_IMETHODIMP 
nsCocoaBrowserService::CertExpired(nsITransportSecurityInfo *socketInfo, 
                                      nsIX509Cert *cert, PRBool *_retval)
{
  nsAlertController* controller = GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *_retval = (PRBool)[controller expiredCert:[NSApp mainWindow]];
  
  return NS_OK;
}

NS_IMETHODIMP 
nsCocoaBrowserService::CrlNextupdate(nsITransportSecurityInfo *socketInfo, 
                                      const PRUnichar * targetURL, nsIX509Cert *cert)
{
  // what does this do!?
  return NS_OK;
}


#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

NS_IMETHODIMP
nsCocoaBrowserService::AlertEnteringSecure(nsIInterfaceRequestor *ctx)
{
  // I don't think any user cares they're entering a secure site.
#if 0
  rv = AlertDialog(ctx, ENTER_SITE_PREF, 
                   NS_LITERAL_STRING("EnterSecureMessage").get(),
                   NS_LITERAL_STRING("EnterSecureShowAgain").get());
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::AlertEnteringWeak(nsIInterfaceRequestor *ctx)
{
  return AlertDialog(ctx, WEAK_SITE_PREF,
                      NS_LITERAL_STRING("WeakSecureMessage").get(),
                      NS_LITERAL_STRING("WeakSecureShowAgain").get());
}

NS_IMETHODIMP
nsCocoaBrowserService::AlertLeavingSecure(nsIInterfaceRequestor *ctx)
{
  return AlertDialog(ctx, LEAVE_SITE_PREF, 
                      NS_LITERAL_STRING("LeaveSecureMessage").get(),
                      NS_LITERAL_STRING("LeaveSecureShowAgain").get());
}


NS_IMETHODIMP
nsCocoaBrowserService::AlertMixedMode(nsIInterfaceRequestor *ctx)
{
  return AlertDialog(ctx, MIXEDCONTENT_PREF, 
                        NS_LITERAL_STRING("MixedContentMessage").get(),
                        NS_LITERAL_STRING("MixedContentShowAgain").get());
}


nsresult
nsCocoaBrowserService::EnsureSecurityStringBundle()
{
  if (!mSecurityStringBundle) {
    #define STRING_BUNDLE_URL "chrome://communicator/locale/security.properties"
    nsCOMPtr<nsIStringBundleService> service = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if ( service ) {
      nsresult rv = service->CreateBundle(STRING_BUNDLE_URL, getter_AddRefs(mSecurityStringBundle));
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}


nsresult
nsCocoaBrowserService::AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                          const PRUnichar *dialogMessageName,
                          const PRUnichar *showAgainName)
{
  nsresult rv = NS_OK;

  // Get user's preference for this alert
  nsCOMPtr<nsIPrefBranch> pref;
  PRBool prefValue = PR_TRUE;
  if ( prefName ) {
    pref = do_GetService("@mozilla.org/preferences-service;1");
    if ( pref )
      pref->GetBoolPref(prefName, &prefValue);

    // Stop if alert is not requested
    if (!prefValue) return NS_OK;
  }
  
  if ( NS_FAILED(rv = EnsureSecurityStringBundle()) )
    return rv;
  
  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ctx);
  if (!prompt) return NS_ERROR_FAILURE;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, dontShowAgain;

  mSecurityStringBundle->GetStringFromName(NS_LITERAL_STRING("Title").get(), getter_Copies(windowTitle));
  mSecurityStringBundle->GetStringFromName(dialogMessageName, getter_Copies(message));
  if ( prefName )
    mSecurityStringBundle->GetStringFromName(showAgainName, getter_Copies(dontShowAgain));
  if (!windowTitle.get() || !message.get()) return NS_ERROR_FAILURE;
  
  if ( prefName )
    rv = prompt->AlertCheck(windowTitle, message, dontShowAgain, &prefValue);
  else
    rv = prompt->AlertCheck(windowTitle, message, nil, nil);
  if (NS_FAILED(rv)) return rv;
      
  if (prefName && !prefValue)
    pref->SetBoolPref(prefName, PR_FALSE);
 
  return rv;
}

nsresult
nsCocoaBrowserService::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx, PRBool* _result)
{
  // no user cares about this. the first thing they do is turn it off.
  *_result = PR_TRUE;
  return NS_OK;
}

nsresult
nsCocoaBrowserService::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx, PRBool* _result)
{
  nsAlertController* controller = GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *_result = (PRBool)[controller postToInsecureFromSecure:[NSApp mainWindow]];
  
  return NS_OK;
}

//    void show( in nsIHelperAppLauncher aLauncher, in nsISupports aContext );
NS_IMETHODIMP
nsCocoaBrowserService::Show(nsIHelperAppLauncher* inLauncher, nsISupports* inContext)
{
NSLog(@"Show");
  return inLauncher->SaveToDisk(nsnull, PR_FALSE);
}

NS_IMETHODIMP
nsCocoaBrowserService::PromptForSaveToFile(nsISupports *aWindowContext, const PRUnichar *aDefaultFile, const PRUnichar *aSuggestedFileExtension, nsILocalFile **_retval)
{
NSLog(@"PromptForSaveToFile");
  NSString* filename = [NSString stringWithCharacters:aDefaultFile length:nsCRT::strlen(aDefaultFile)];
  NSSavePanel *thePanel = [NSSavePanel savePanel];
  
  // Note: although the docs for NSSavePanel specifically state "path and filename can be empty strings, but
  // cannot be nil" if you want the last used directory to persist between calls to display the save panel
  // use nil for the path given to runModalForDirectory
  int runResult = [thePanel runModalForDirectory: nil file:filename];
  if (runResult == NSOKButton) {
NSLog([thePanel filename]);
    NSString *theName = [thePanel filename];
    return NS_NewNativeLocalFile(nsDependentCString([theName fileSystemRepresentation]), PR_FALSE, _retval);
  }

  return NS_ERROR_FAILURE;
}

/* void showProgressDialog (in nsIHelperAppLauncher aLauncher, in nsISupports aContext); */
NS_IMETHODIMP
nsCocoaBrowserService::ShowProgressDialog(nsIHelperAppLauncher *aLauncher, nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::Init(nsIURI* aSource,
                            nsILocalFile* aTarget,
                            const PRUnichar* aDisplayName,
                            const PRUnichar* aOpeningWith,
                            PRInt64 aStartTime,
                            nsIWebBrowserPersist* aPersist)
{
  NSLog(@"nsIDownload::Init");
  return NS_OK;
}


NS_IMETHODIMP
nsCocoaBrowserService::GetDisplayName(PRUnichar** aDisplayName)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::SetDisplayName(const PRUnichar* aDisplayName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::GetOpeningWith(PRUnichar** aOpeningWith)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::GetSource(nsIURI** aSource)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::GetTarget(nsILocalFile** aTarget)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::GetStartTime(PRInt64* aStartTime)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::GetPercentComplete(PRInt32* aPercentComplete)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
NS_IMETHODIMP
nsCocoaBrowserService::GetListener(nsIWebProgressListener** aListener)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::SetListener(nsIWebProgressListener* aListener)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::GetObserver(nsIObserver** aObserver)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::SetObserver(nsIObserver* aObserver)
{
  return NS_OK;
}
  
NS_IMETHODIMP
nsCocoaBrowserService::GetPersist(nsIWebBrowserPersist** aPersist)
{
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCocoaBrowserService::OnStateChange(nsIWebProgress* aWebProgress,
                            nsIRequest* aRequest, PRUint32 aStateFlags,
                            PRUint32 aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::OnStatusChange(nsIWebProgress *aWebProgress,
                              nsIRequest *aRequest, nsresult aStatus,
                              const PRUnichar *aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::OnLocationChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest, nsIURI *aLocation)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::OnProgressChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest,
                                PRInt32 aCurSelfProgress,
                                PRInt32 aMaxSelfProgress,
                                PRInt32 aCurTotalProgress,
                                PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCocoaBrowserService::OnSecurityChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest, PRUint32 aState)
{
  return NS_OK;
}
