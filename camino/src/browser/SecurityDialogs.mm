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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#import "NSString+Utils.h"

#import "nsAlertController.h"
#import "CHBrowserService.h"
#import "SecurityDialogs.h"
#import "MainController.h"

#import "CertificateItem.h"
#import "CertificateView.h"
#import "ViewCertificateDialogController.h"
#import "BrowserSecurityDialogs.h"

#include "nsString.h"
#include "nsIPrefBranch.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMLocation.h"
#include "nsServiceManagerUtils.h"

#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"

#include "nsIPKCS11ModuleDB.h"
#include "nsIPK11TokenDB.h"
#include "nsIPK11Token.h"
#include "nsIPKCS11Slot.h"

#include "nsICRLInfo.h"

#include "nsIObserver.h"
#include "nsIKeygenThread.h"

SecurityDialogs::SecurityDialogs()
{
}

SecurityDialogs::~SecurityDialogs()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS8(SecurityDialogs, nsICertificateDialogs,
                                    nsIBadCertListener,
                                    nsITokenPasswordDialogs,
                                    nsIClientAuthDialogs,
                                    nsISecurityWarningDialogs,
                                    nsITokenDialogs,
                                    nsIDOMCryptoDialogs,
                                    nsIGeneratingKeypairInfoDialogs)

/**
 *  UI shown when a user is asked to download a new CA cert.
 *  Provides user with ability to choose trust settings for the cert.
 *  Asks the user to grant permission to import the certificate.
 *
 *  @param ctx A user interface context.
 *  @param cert The certificate that is about to get installed.
 *  @param trust a bit mask of trust flags, 
 *               see nsIX509CertDB for possible values.
 *
 *  @return true if the user allows to import the certificate.
 */
/* boolean confirmDownloadCACert (in nsIInterfaceRequestor ctx, in nsIX509Cert cert, out unsigned long trust); */
NS_IMETHODIMP SecurityDialogs::ConfirmDownloadCACert(nsIInterfaceRequestor *ctx, nsIX509Cert *cert, PRUint32 *trust, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_ARG_POINTER(trust);
  *trust = 0;
  *_retval = PR_FALSE;
  
#if 0
  // Suckage: no way to get to originating nsIDOMWindow for this request.
  // See bug 306288.
  nsCOMPtr<nsIDOMWindowInternal> parentWindow = do_GetInterface(ctx);
  if (!parentWindow)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMLocation> docLocation;
  parentWindow->GetLocation(getter_AddRefs(docLocation));
  if (!docLocation)
    return NS_ERROR_FAILURE;

  nsAutoString location;
  docLocation->GetHref(location);
#endif

  DownloadCACertDialogController* downloadCertDialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] downloadCACertDialogController] retain];
  if (!downloadCertDialogController)
    return NS_ERROR_FAILURE;

  [downloadCertDialogController setCertificateItem:[CertificateItem certificateItemWithCert:cert]];

  // XXX fix window parenting
  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[downloadCertDialogController window] relativeToWindow:parentWindow];
  else
    result = [NSApp runModalForWindow:[downloadCertDialogController window]];

  if (result == NSAlertDefaultReturn)
  {
    *trust   = [downloadCertDialogController trustMaskSetting];
    *_retval = PR_TRUE;
  }
  else
    *_retval = PR_FALSE;

  [downloadCertDialogController release];
  return NS_OK;
}

/**
 *  UI shown when a web site has delivered a CA certificate to
 *  be imported, but the certificate is already contained in the
 *  user's storage.
 *
 *  @param ctx A user interface context.
 */
/* void notifyCACertExists (in nsIInterfaceRequestor ctx); */
NS_IMETHODIMP SecurityDialogs::NotifyCACertExists(nsIInterfaceRequestor *ctx)
{
  // Yes, this sucks. We can't tell the poor user which CA this is showing for
  // (see bug 306290).
  NSRunAlertPanel(NSLocalizedStringFromTable(@"CACertExistsMsg", @"CertificateDialogs", @""),
                  NSLocalizedStringFromTable(@"CACertExistsDetail", @"CertificateDialogs", @""),
                  nil /* use "OK" */, nil, nil);

  return NS_OK;
}

/**
 *  UI shown when a user's personal certificate is going to be
 *  exported to a backup file.
 *  The implementation of this dialog should make sure 
 *  to prompt the user to type the password twice in order to
 *  confirm correct input.
 *  The wording in the dialog should also motivate the user 
 *  to enter a strong password.
 *
 *  @param ctx A user interface context.
 *  @param password The password provided by the user.
 *
 *  @return false if the user requests to cancel.
 */
/* boolean setPKCS12FilePassword (in nsIInterfaceRequestor ctx, out AString password); */
NS_IMETHODIMP SecurityDialogs::SetPKCS12FilePassword(nsIInterfaceRequestor *ctx, nsAString & password, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  password.Truncate();

  CreatePasswordDialogController* pwDialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] createPasswordDialogController] retain];
  if (!pwDialogController)
    return NS_ERROR_FAILURE;

  NSString* titleString   = NSLocalizedStringFromTable(@"PKCS12BackupPasswordTitle", @"CertificateDialogs", @"");
  NSString* messageString = NSLocalizedStringFromTable(@"PKCS12BackupPasswordMsg", @"CertificateDialogs", @"");
  
  [pwDialogController setTitle:titleString message:messageString];
  [pwDialogController hideChangePasswordField];

  // XXX fix window parenting
#if 0
  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[pwDialogController window] relativeToWindow:parentWindow];
  else
#endif

  int result = [NSApp runModalForWindow:[pwDialogController window]];
  BOOL confirmed = (result == NSAlertDefaultReturn);

  NSString* thePassword = [pwDialogController newPassword];
  *_retval = confirmed;
  if (confirmed)
    [thePassword assignTo_nsAString:password];

  [pwDialogController release];
  return NS_OK;
}

/**
 *  UI shown when a user is about to restore a personal
 *  certificate from a backup file.
 *  The user is requested to enter the password
 *  that was used in the past to protect that backup file.
 *
 *  @param ctx A user interface context.
 *  @param password The password provided by the user.
 *
 *  @return false if the user requests to cancel.
 */
/* boolean getPKCS12FilePassword (in nsIInterfaceRequestor ctx, out AString password); */
NS_IMETHODIMP SecurityDialogs::GetPKCS12FilePassword(nsIInterfaceRequestor *ctx, nsAString & password, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  password.Truncate();

  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // XXX ideally we should use a custom dialog here, following the recommendations above.
  NSMutableString* thePassword = [[NSMutableString alloc] init];
  BOOL confirmed = [controller promptPassword:nil /* no parent, sucky APIs */
                                        title:NSLocalizedStringFromTable(@"PKCS12BackupRestoreTitle", @"CertificateDialogs", @"")
                                         text:NSLocalizedStringFromTable(@"PKCS12BackupRestoreMsg", @"CertificateDialogs", @"")
                                 passwordText:thePassword
                                     checkMsg:nil
                                   checkValue:NULL
                                      doCheck:NO];
  *_retval = confirmed;

  if (confirmed)
    [thePassword assignTo_nsAString:password];

  [thePassword release];

  return NS_OK;
}

/**
 *  UI shown when a certificate needs to be shown to the user.
 *  The implementation should try to display as many attributes
 *  as possible.
 *
 *  @param ctx A user interface context.
 *  @param cert The certificate to be shown to the user.
 */
/* void viewCert (in nsIInterfaceRequestor ctx, in nsIX509Cert cert); */
NS_IMETHODIMP SecurityDialogs::ViewCert(nsIInterfaceRequestor *ctx, nsIX509Cert *cert)
{
  [ViewCertificateDialogController runModalCertificateWindowWithCertificateItem:[CertificateItem certificateItemWithCert:cert]
                                                       certTypeForTrustSettings:nsIX509Cert::UNKNOWN_CERT]; // won't show trust
  return NS_OK;
}

/**
 *  UI shown after a Certificate Revocation List (CRL) has been
 *  successfully imported.
 *
 *  @param ctx A user interface context.
 *  @param crl Information describing the CRL that was imported.
 */
/* void crlImportStatusDialog (in nsIInterfaceRequestor ctx, in nsICRLInfo crl); */
NS_IMETHODIMP SecurityDialogs::CrlImportStatusDialog(nsIInterfaceRequestor *ctx, nsICRLInfo *crl)
{
  NS_ENSURE_ARG(crl);

  nsAutoString crlOrg;
  nsAutoString crlOrgUnit;
  
  crl->GetOrganization(crlOrg);
  crl->GetOrganizationalUnit(crlOrgUnit);
  
  NSString* titleString   = NSLocalizedStringFromTable(@"CRLImportTitle", @"CertificateDialogs", @"");
  NSString* messageFormat = NSLocalizedStringFromTable(@"CRLImportMessageFormat", @"CertificateDialogs", @"");
  
  NSString* crlOrgString     = [NSString stringWith_nsAString:crlOrg];
  NSString* crlOrgUnitString = [NSString stringWith_nsAString:crlOrgUnit];

  NSString* messageString = [NSString stringWithFormat:messageFormat, crlOrgString, crlOrgUnitString];
  NSRunAlertPanel(titleString, messageString, NSLocalizedString(@"OK", @""), nil, nil);
  
  // we currently don't have any UI to manage CRLs, or change their settings (e.g. autoupdate)

  return NS_OK;
}


// nsIBadCertListener implementation

/**
 *  Inform the user there are problems with the trust of a certificate,
 *  and request a decision from the user.
 *  The UI should offer the user a way to look at the certificate in detail.
 *  The following is a sample UI message to be shown to the user:
 *
 *    Unable to verify the identity of %S as a trusted site.
 *    Possible reasons for this error:
 *    - Your browser does not recognize the Certificate Authority 
 *      that issued the site's certificate.
 *    - The site's certificate is incomplete due to a 
 *      server misconfiguration.
 *    - You are connected to a site pretending to be %S, 
 *      possibly to obtain your confidential information.
 *    Please notify the site's webmaster about this problem.
 *    Before accepting this certificate, you should examine this site's 
 *      certificate carefully. Are you willing to to accept this certificate 
 *      for the purpose of identifying the Web site %S?
 *    o Accept this certificate permanently
 *    x Accept this certificate temporarily for this session
 *    o Do not accept this certificate and do not connect to this Web site
 *
 *  @param socketInfo A network communication context that can be used to obtain more information
 *                    about the active connection.
 *  @param cert The certificate that is not trusted and that is having the problem.
 *  @param certAddType The user's trust decision. See constants defined above.
 *
 *  @return true if the user decided to connect anyway, false if the user decided to not connect
 */
/* boolean confirmUnknownIssuer (in nsIInterfaceRequestor socketInfo,
                                 in nsIX509Cert cert, out addType); */
NS_IMETHODIMP
SecurityDialogs::ConfirmUnknownIssuer(nsIInterfaceRequestor *socketInfo,
                                      nsIX509Cert *cert, PRInt16 *outAddType,
                                      PRBool *_retval)
{
  *_retval = PR_FALSE;
  *outAddType = UNINIT_ADD_FLAG;

  UnknownCertIssuerDialogController* dialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] unknownCertIssuerDialogController] retain];
  if (!dialogController)
    return NS_ERROR_FAILURE;

  [dialogController setCertificateItem:[CertificateItem certificateItemWithCert:cert]];

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.

  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[dialogController window] relativeToWindow:parentWindow];
  else
    result = [NSApp runModalForWindow:[dialogController window]];

  switch (result)
  {
    case NSAlertDefaultReturn:      // just this session
      *outAddType = nsIBadCertListener::ADD_TRUSTED_FOR_SESSION;
      *_retval = PR_TRUE;
      break;
    
    default:
    case NSAlertAlternateReturn:    // stop
      *outAddType = nsIBadCertListener::UNINIT_ADD_FLAG;
      *_retval = PR_FALSE;
      break;

    case NSAlertOtherReturn:        // always
      *outAddType = nsIBadCertListener::ADD_TRUSTED_PERMANENTLY;
      *_retval = PR_TRUE;
      break;
  }

  [dialogController release];
  return NS_OK;
}

/**
 *  Inform the user there are problems with the trust of a certificate,
 *  and request a decision from the user.
 *  The hostname mentioned in the server's certificate is not the hostname
 *  that was used as a destination address for the current connection.
 *
 *  @param socketInfo A network communication context that can be used to obtain more information
 *                    about the active connection.
 *  @param targetURL The URL that was used to open the current connection.
 *  @param cert The certificate that was presented by the server.
 *
 *  @return true if the user decided to connect anyway, false if the user decided to not connect
 */
/* boolean confirmMismatchDomain (in nsIInterfaceRequestor socketInfo,
                                  in nsAUTF8String targetURL,
                                  in nsIX509Cert cert); */
NS_IMETHODIMP
SecurityDialogs::ConfirmMismatchDomain(nsIInterfaceRequestor *socketInfo,
                                       const nsACString& targetURL,
                                       nsIX509Cert *cert, PRBool *_retval)
{

// testing
  ConfirmKeyEscrow(cert, _retval);
  
  MismatchedCertDomainDialogController* certDialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] mismatchedCertDomainDialogController] retain];
  if (!certDialogController)
    return NS_ERROR_FAILURE;

  [certDialogController setTargetURL:[NSString stringWith_nsACString:targetURL]];
  [certDialogController setCertificateItem:[CertificateItem certificateItemWithCert:cert]];

  // XXX fix window parenting
  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[certDialogController window] relativeToWindow:parentWindow];
  else
    result = [NSApp runModalForWindow:[certDialogController window]];
  
  *_retval = (result == NSAlertDefaultReturn);

  [certDialogController release];
  return NS_OK;
}

/**
 *  Inform the user there are problems with the trust of a certificate,
 *  and request a decision from the user.
 *  The certificate presented by the server is no longer valid because 
 *  the validity period has expired.
 *
 *  @param socketInfo A network communication context that can be used to obtain more information
 *                    about the active connection.
 *  @param cert The certificate that was presented by the server.
 *
 *  @return true if the user decided to connect anyway, false if the user decided to not connect
 */

/* boolean confirmCertExpired (in nsIInterfaceRequestor socketInfo,
                               in nsIX509Cert cert); */
NS_IMETHODIMP
SecurityDialogs::ConfirmCertExpired(nsIInterfaceRequestor *socketInfo,
                                    nsIX509Cert *cert, PRBool *_retval)
{
  ExpiredCertDialogController* expiredCertController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] expiredCertDialogController] retain];
  if (!expiredCertController)
    return NS_ERROR_FAILURE;

  [expiredCertController setCertificateItem:[CertificateItem certificateItemWithCert:cert]];
  // XXX fix window parenting
  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[expiredCertController window] relativeToWindow:parentWindow];
  else
    result = [NSApp runModalForWindow:[expiredCertController window]];
  
  *_retval = (result == NSAlertDefaultReturn);
  [expiredCertController release];
  return NS_OK;
}

/**
 *  Inform the user there are problems with the trust of a certificate,
 *  and request a decision from the user.
 *  The Certificate Authority (CA) that issued the server's certificate has issued a 
 *  Certificate Revocation List (CRL). 
 *  However, the application does not have a current version of the CA's CRL.
 *  Due to the application configuration, the application disallows the connection
 *  to the remote site.
 *
 *  @param socketInfo A network communication context that can be used to obtain more information
 *                    about the active connection.
 *  @param targetURL The URL that was used to open the current connection.
 *  @param cert The certificate that was presented by the server.
 */
NS_IMETHODIMP
SecurityDialogs::NotifyCrlNextupdate(nsIInterfaceRequestor *socketInfo,
                                     const nsACString& targetURL,
                                     nsIX509Cert *cert)
{
  CertificateItem* certItem = [CertificateItem certificateItemWithCert:cert];

  NSString* titleFormat   = NSLocalizedStringFromTable(@"CRLExpiredTitleFormat", @"CertificateDialogs", @"");
  NSString* titleString   = [NSString stringWithFormat:titleFormat, [NSString stringWith_nsACString:targetURL]];

  NSString* messageFormat = NSLocalizedStringFromTable(@"CRLExpiredMessageFormat", @"CertificateDialogs", @"");
  NSString* messageString = [NSString stringWithFormat:messageFormat, [certItem issuerOrganization]];

  // XXX this is pretty lame. can't we direct them to some UI that allows them to fix the problem?
  NSRunAlertPanel(titleString, messageString, NSLocalizedString(@"OK", @""), nil, nil);
  return NS_OK;
}


// a little delegate object that does checking of the old password in the CreatePasswordDialogController
@interface ChangePasswordDelegate : NSObject
{
  NSString*         mTokenName;
}

- (id)initWithTokenName:(NSString*)inTokenName;

@end

@implementation ChangePasswordDelegate

- (id)initWithTokenName:(NSString*)inTokenName
{
  if ((self = [super init]))
  {
    mTokenName = [inTokenName retain];
  }
  return self;
}

- (void)dealloc
{
  [mTokenName release];
  [super dealloc];
}

- (BOOL)changePasswordDialogController:(CreatePasswordDialogController*)inController oldPasswordValid:(NSString*)oldPassword
{
  nsCOMPtr<nsIPK11TokenDB> tokenDB = do_GetService("@mozilla.org/security/pk11tokendb;1");
  if (!tokenDB)
    return NO;

  nsAutoString tokenName;
  [mTokenName assignTo_nsAString:tokenName];
  
  nsCOMPtr<nsIPK11Token> theToken;
  tokenDB->FindTokenByName(tokenName.get(), getter_AddRefs(theToken));
  if (!theToken)
    return NO;
  
  nsAutoString pwString;
  [oldPassword assignTo_nsAString:pwString];
  PRBool isValid;
  nsresult rv = theToken->CheckPassword(pwString.get(), &isValid);
  if (NS_FAILED(rv))
    return NO;

  if (!isValid)
  {
    NSString* errorString   = NSLocalizedStringFromTable(@"OldPasswordWrongTitle", @"CertificateDialogs", @"");
    NSString* messageString = [NSString stringWithFormat:NSLocalizedStringFromTable(@"OldPasswordWrongMsg", @"CertificateDialogs", @""),
                                                         mTokenName];
    NSRunAlertPanel(errorString, messageString, NSLocalizedString(@"OK", @""), nil, nil);
  }
  return isValid;
}

@end // ChangePasswordDelegate


  /**
   * setPassword - sets the password/PIN on the named token.
   *   The canceled output value should be set to TRUE when
   *   the user (or implementation) cancels the operation.
   *
   */
  /* void setPassword (in nsIInterfaceRequestor ctx, in wstring tokenName, out boolean canceled); */
NS_IMETHODIMP
SecurityDialogs::SetPassword(nsIInterfaceRequestor *ctx, const PRUnichar *tokenName, PRBool *canceled)
{
  NS_ENSURE_ARG_POINTER(canceled);
  *canceled = PR_FALSE;
  
  // get the token DB
  nsCOMPtr<nsIPK11TokenDB> tokenDB = do_GetService("@mozilla.org/security/pk11tokendb;1");
  if (!tokenDB)
    return NS_ERROR_FAILURE;

  // and find this token (seems crazy that tokens are identified by name, and not some
  // unique ID)
  nsCOMPtr<nsIPK11Token> theToken;
  tokenDB->FindTokenByName(tokenName, getter_AddRefs(theToken));
  if (!theToken)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIPKCS11ModuleDB> securityModuleDB = do_GetService("@mozilla.org/security/pkcs11moduledb;1");
  if (!securityModuleDB)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPKCS11Slot> theSlot;
  securityModuleDB->FindSlotByName(tokenName, getter_AddRefs(theSlot));

  BOOL showOldPasswordField = YES;
  NSString* tokenNameString = [NSString stringWithPRUnichars:tokenName];
  NSString* titleString = @"";
  NSString* messageString = @"";
  
  PRUint32 slotStatus = nsIPKCS11Slot::SLOT_UNINITIALIZED;
  if (theSlot)
    theSlot->GetStatus(&slotStatus);    

 // If the token is unitialized, don't use the old password box.
 // Otherwise, do.
  if (slotStatus == nsIPKCS11Slot::SLOT_UNINITIALIZED)
  {
    titleString = [NSString stringWithFormat:NSLocalizedStringFromTable(@"SetTokenPasswordTitleFormat", @"CertificateDialogs", @""),
                                             tokenNameString];
    messageString = [NSString stringWithFormat:NSLocalizedStringFromTable(@"SetTokenPasswordMsgFormat", @"CertificateDialogs", @""),
                                             tokenNameString];

    showOldPasswordField = NO;
  }
  else
  {
    // strings assume that we're changing the password
    titleString = [NSString stringWithFormat:NSLocalizedStringFromTable(@"ChangeTokenPasswordTitleFormat", @"CertificateDialogs", @""),
                                             tokenNameString];
    messageString = [NSString stringWithFormat:NSLocalizedStringFromTable(@"ChangeTokenPasswordMsgFormat", @"CertificateDialogs", @""),
                                             tokenNameString];
  }
  
  CreatePasswordDialogController* pwDialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] createPasswordDialogController] retain];
  if (!pwDialogController)
    return NS_ERROR_FAILURE;

  ChangePasswordDelegate* pwDelegate = [[[ChangePasswordDelegate alloc] initWithTokenName:tokenNameString] autorelease];
  [pwDialogController setDelegate:pwDelegate];
  [pwDialogController setTitle:titleString message:messageString];

  if (!showOldPasswordField)
    [pwDialogController hideChangePasswordField];
  
  // XXX fix window parenting
#if 0
  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[pwDialogController window] relativeToWindow:parentWindow];
  else
#endif

  int result = [NSApp runModalForWindow:[pwDialogController window]];

  [pwDialogController setDelegate:nil];

  if (result != NSAlertDefaultReturn)
  {
    *canceled = PR_TRUE;
    return NS_OK;
  }

  *canceled = PR_FALSE;

  nsAutoString oldPassword, newPassword;
  [[pwDialogController currentPassword] assignTo_nsAString:oldPassword];
  [[pwDialogController newPassword] assignTo_nsAString:newPassword];

  nsresult rv;
  if (slotStatus == nsIPKCS11Slot::SLOT_UNINITIALIZED)
    rv = theToken->InitPassword(newPassword.get());
  else
    rv = theToken->ChangePassword(oldPassword.get(), newPassword.get());

  [pwDialogController release];
  return rv;
}

/* void getPassword (in nsIInterfaceRequestor ctx, in wstring tokenName, out wstring password, out boolean canceled); */
NS_IMETHODIMP
SecurityDialogs::GetPassword(nsIInterfaceRequestor *ctx, const PRUnichar *tokenName, PRUnichar **password, PRBool *canceled)
{
  *canceled = NO;
  *password = nsnull;

  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  NSMutableString* thePassword = [[NSMutableString alloc] init];
  
  NSString* msgFormat = NSLocalizedStringFromTable(@"GetTokenPasswordTitle", @"CertificateDialogs", @"");
  NSString* messageStr = [NSString stringWithFormat:msgFormat, [NSString stringWithPRUnichars:tokenName]];
  BOOL confirmed = [controller promptPassword:nil /* no parent, sucky APIs */
                                        title:messageStr
                                         text:NSLocalizedStringFromTable(@"GetTokenPasswordMsg", @"CertificateDialogs", @"")
                                 passwordText:thePassword
                                     checkMsg:nil
                                   checkValue:NULL
                                      doCheck:NO];

  if (confirmed)
  {  
    *password = [thePassword createNewUnicodeBuffer];
    *canceled = YES;
  }
  else
    *canceled = YES;

  [thePassword release];

  return NS_OK;
}

  /**
   * display
   *   UI shown when a user is asked to do SSL client auth.
   */
  /* void ChooseCertificate (in nsIInterfaceRequestor ctx, in wstring cn, in wstring organization, in wstring issuer, [array, size_is (count)] in wstring certNickList, [array, size_is (count)] in wstring certDetailsList, in unsigned long count, out long selectedIndex, out boolean canceled); */
NS_IMETHODIMP
SecurityDialogs::ChooseCertificate(nsIInterfaceRequestor *ctx, const PRUnichar *cn, const PRUnichar *organization, const PRUnichar *issuer,
                                   const PRUnichar **certNickList, const PRUnichar **certDetailsList, PRUint32 count, PRInt32 *selectedIndex, PRBool *canceled)
{
  NS_ENSURE_ARG_POINTER(selectedIndex);
  NS_ENSURE_ARG_POINTER(canceled);
  *selectedIndex = -1;
  *canceled = PR_TRUE;

  nsCOMPtr<nsIX509CertDB> certDB = do_GetService("@mozilla.org/security/x509certdb;1");
  if (!certDB) return NS_ERROR_FAILURE;

  NSMutableArray* certArray = [NSMutableArray arrayWithCapacity:count];
  for (PRUint32 i = 0; i < count; i ++)
  {
    // OK, this sucks major ass. The backend thinks that it knows what the frontend wants
    // to show, so the strings in "certNickList" are actually "nick [serial#]". So we have
    // to grovel around for the real nick.
    // XXX why can't it just hand us an array of nsIX509Certs?
    const PRUnichar* thisCertNick = certNickList[i];
    if (!thisCertNick) continue;
    
    NSString* nickWithSerialNumber = [NSString stringWithPRUnichars:thisCertNick];
    // look for " [" from the end of the string to find the start of the serial#
    NSRange serialStartRange = [nickWithSerialNumber rangeOfString:@" [" options:NSBackwardsSearch];
    if (serialStartRange.location != NSNotFound && serialStartRange.location > 0)
    {
      NSString* nickname = [nickWithSerialNumber substringToIndex:serialStartRange.location];

      nsAutoString nicknameString;
      [nickname assignTo_nsAString:nicknameString];
      
      nsCOMPtr<nsIX509Cert> thisCert;
      certDB->FindCertByNickname(nsnull, nicknameString, getter_AddRefs(thisCert));
      if (!thisCert) continue;
      
      [certArray addObject:[CertificateItem certificateItemWithCert:thisCert]];
    }
  }
  
  if ([certArray count] == 0)
  {
    NSLog(@"ChooseCertificate failed to find any matching certificates");
    return NS_ERROR_FAILURE;
  }
  
  ChooseCertDialogController* dialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] chooseCertDialogController] retain];
  [dialogController setCommonName:[NSString stringWithPRUnichars:cn]
                     organization:[NSString stringWithPRUnichars:organization]
                           issuer:[NSString stringWithPRUnichars:issuer]];
  [dialogController setCertificates:certArray];

  // XXX fix window parenting
  NSWindow* parentWindow = [(MainController*)[NSApp delegate] getFrontmostBrowserWindow];
  int result;
  if (parentWindow)
    result = [NSApp runModalForWindow:[dialogController window] relativeToWindow:parentWindow];
  else
    result = [NSApp runModalForWindow:[dialogController window]];

  if (result == NSAlertDefaultReturn)
  {
    *selectedIndex = [certArray indexOfObject:[dialogController selectedCert]];
    *canceled = PR_FALSE;
  }
  else
  {
    *canceled = PR_TRUE;
  }
  
  [dialogController release];
  
  return NS_OK;
}

// nsITokenDialogs
/* void ChooseToken (in nsIInterfaceRequestor ctx, [array, size_is (count)] in wstring tokenNameList, in unsigned long count, out wstring tokenName, out boolean canceled); */
NS_IMETHODIMP
SecurityDialogs::ChooseToken(nsIInterfaceRequestor *ctx, const PRUnichar **tokenNameList, PRUint32 count, PRUnichar **tokenName, PRBool *canceled)
{
  NSLog(@"ChooseToken not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDOMCryptoDialogs
/* boolean ConfirmKeyEscrow (in nsIX509Cert escrowAuthority); */
NS_IMETHODIMP
SecurityDialogs::ConfirmKeyEscrow(nsIX509Cert *escrowAuthority, PRBool *_retval)
{
  CertificateItem* certItem = [CertificateItem certificateItemWithCert:escrowAuthority];
  NSString* titleFormat   = NSLocalizedStringFromTable(@"EscrowDialogTitleFormat", @"CertificateDialogs", @"");
  NSString* titleString   = [NSString stringWithFormat:titleFormat, [certItem displayName]];
  NSString* messageString = NSLocalizedStringFromTable(@"EscrowDialogMessage", @"CertificateDialogs", @"");

  NSString* allowButtonString = NSLocalizedStringFromTable(@"AllowEscrowButton", @"CertificateDialogs", @"");
  
  int result = NSRunAlertPanel(titleString, messageString, allowButtonString, NSLocalizedString(@"Cancel", @""), nil);
  *_retval = (result == NSAlertDefaultReturn);
  return NS_OK;
}


class GenKeyPairCompletionObserver : public nsIObserver
{
public:
  GenKeyPairCompletionObserver(GenKeyPairDialogController* inDlgController)
  {
    mDialogController = [inDlgController retain];
  }
  
  virtual ~GenKeyPairCompletionObserver()
  {
    [mDialogController release];
  }

  NS_DECL_ISUPPORTS;
  NS_DECL_NSIOBSERVER;
  
protected:

  GenKeyPairDialogController*   mDialogController;    // retained
};

NS_IMPL_THREADSAFE_ISUPPORTS1(GenKeyPairCompletionObserver, nsIObserver)

NS_IMETHODIMP
GenKeyPairCompletionObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (strcmp(aTopic, "keygen-finished") == 0)
    [mDialogController keyPairGenerationComplete];

  return NS_OK;
}

// nsIGeneratingKeypairInfoDialogs
/* void displayGeneratingKeypairInfo (in nsIInterfaceRequestor ctx, in nsIKeygenThread runnable); */
NS_IMETHODIMP
SecurityDialogs::DisplayGeneratingKeypairInfo(nsIInterfaceRequestor *ctx, nsIKeygenThread *runnable)
{
  if (!runnable)
    return NS_ERROR_FAILURE;
  
  // paranoia -- make sure the thread stays around so that we can call UserCanceled() even after
  // the thread is complete. Not sure if it goes away or not.
  nsCOMPtr<nsIKeygenThread> threadDeathGrip = runnable;
  
  GenKeyPairDialogController* dialogController = [[[BrowserSecurityUIProvider sharedBrowserSecurityUIProvider] genKeyPairDialogController] retain];
  if (!dialogController)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIObserver> completionObserver = new GenKeyPairCompletionObserver(dialogController);
  runnable->StartKeyGeneration(completionObserver);
  
  // does this guy have to be modal?
  int result = [NSApp runModalForWindow:[dialogController window]];

  if (result == NSAlertAlternateReturn) // cancelled
  {
    PRBool threadAlreadyClosedDialog;
    runnable->UserCanceled(&threadAlreadyClosedDialog);
    return NS_ERROR_FAILURE;
  }
  
  [dialogController release];
  return NS_OK;
}


// nsISecurityWarningDialogs implementation
#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
                                     
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

// XXXbryner should we make these real confirmation dialogs?

NS_IMETHODIMP
SecurityDialogs::ConfirmEnteringSecure(nsIInterfaceRequestor *ctx,
                                       PRBool *_retval)
{
  // I don't think any user cares they're entering a secure site.
  #if 0
  rv = AlertDialog(ctx, ENTER_SITE_PREF,
                   NS_LITERAL_STRING("EnterSecureMessage").get(),
                   NS_LITERAL_STRING("EnterSecureShowAgain").get());
  #endif

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
SecurityDialogs::ConfirmEnteringWeak(nsIInterfaceRequestor *ctx,
                                     PRBool *_retval)
{
  *_retval = PR_TRUE;
  return AlertDialog(ctx, WEAK_SITE_PREF,
                     NS_LITERAL_STRING("WeakSecureMessage").get(),
                     NS_LITERAL_STRING("WeakSecureShowAgain").get());
}

NS_IMETHODIMP
SecurityDialogs::ConfirmLeavingSecure(nsIInterfaceRequestor *ctx,
                                      PRBool *_retval)
{
  *_retval = PR_TRUE;
  return AlertDialog(ctx, LEAVE_SITE_PREF,
                     NS_LITERAL_STRING("LeaveSecureMessage").get(),
                     NS_LITERAL_STRING("LeaveSecureShowAgain").get());
}


NS_IMETHODIMP
SecurityDialogs::ConfirmMixedMode(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  *_retval = PR_TRUE;
  return AlertDialog(ctx, MIXEDCONTENT_PREF,
                     NS_LITERAL_STRING("MixedContentMessage").get(),
                     NS_LITERAL_STRING("MixedContentShowAgain").get());
}


NS_IMETHODIMP
SecurityDialogs::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx,
                                       PRBool* _result)
{
  // no user cares about this. the first thing they do is turn it off.
  *_result = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
SecurityDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx,
                                                 PRBool* _result)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.

  // Yes there is:
  // nsCOMPtr<nsIDOMWindowInternal> parent = do_GetInterface(ctx);
  *_result = (PRBool)[controller postToInsecureFromSecure:[(MainController*)[NSApp delegate] getFrontmostBrowserWindow]];

  return NS_OK;
}


// Private helper functions
nsresult
SecurityDialogs::EnsureSecurityStringBundle()
{
  if (!mSecurityStringBundle) {
    #define STRING_BUNDLE_URL "chrome://pipnss/locale/security.properties"
    nsCOMPtr<nsIStringBundleService> service = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if ( service ) {
          nsresult rv = service->CreateBundle(STRING_BUNDLE_URL, getter_AddRefs(mSecurityStringBundle));
          if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}


nsresult
SecurityDialogs::AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
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
  mSecurityStringBundle->GetStringFromName(NS_LITERAL_STRING("Title").get(),
                                           getter_Copies(windowTitle));
  mSecurityStringBundle->GetStringFromName(dialogMessageName,
                                           getter_Copies(message));
  if ( prefName )
    mSecurityStringBundle->GetStringFromName(showAgainName,
                                             getter_Copies(dontShowAgain));
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

