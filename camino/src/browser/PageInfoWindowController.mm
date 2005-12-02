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
 * The Original Code is Camino code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <smfr@smfr.org>
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

#include "nsXPIDLString.h"

#include "nsIDOMWindow.h"

#include "nsISecureBrowserUI.h"
#include "nsISSLStatusProvider.h"
#include "nsISSLStatus.h"
#include "nsIX509Cert.h"
#include "nsIWebProgressListener.h"

#import "CHBrowserView.h"

#import "CertificateItem.h"
#import "CertificateView.h"
#import "ViewCertificateDialogController.h"

#import "PageInfoWindowController.h"


static PageInfoWindowController* gSingletonPageInfoController;

@interface PageInfoWindowController(Private)

- (void)updateGeneralInfoFromBrowserView:(CHBrowserView*)inBrowserView;
- (void)updateSecurityInfoFromBrowserView:(CHBrowserView*)inBrowserView;

- (void)clearInfo;
- (void)enableButtons;

- (CertificateItem*)certificateItem;
- (void)setCertificateItem:(CertificateItem*)inCertItem;

@end

#pragma mark -

@implementation PageInfoWindowController

+ (PageInfoWindowController*)sharedPageInfoWindowController
{
  if (!gSingletonPageInfoController)
  {
    gSingletonPageInfoController = [[PageInfoWindowController alloc] initWithWindowNibName:@"PageInfo"];
  }
  return gSingletonPageInfoController;
}

+ (PageInfoWindowController*)visiblePageInfoWindowController
{
  return gSingletonPageInfoController;
}

- (void)dealloc
{
  NSLog(@"%@ dealloc", self);
  [mCertificateItem release];
  [super dealloc];
}

- (void)awakeFromNib
{
  [self setShouldCascadeWindows:NO];
  [[self window] setFrameAutosaveName:@"PageInfoWindow"];
}

- (void)windowDidLoad
{
  [self clearInfo];
}

- (void)windowWillClose:(NSNotification *)aNotification
{
  [self clearInfo]; // clear stuff

  if (self == gSingletonPageInfoController)
    gSingletonPageInfoController = nil;  

  // balance the init from when the window was shown
  [self autorelease];
}

- (IBAction)viewCertificate:(id)sender
{
  if (mCertificateItem)
  {
    [ViewCertificateDialogController showCertificateWindowWithCertificateItem:mCertificateItem
                                                         certTypeForTrustSettings:nsIX509Cert::CA_CERT];
  }
}

- (void)clearInfo
{
  [mPageTitleField setStringValue:@""];
  [mPageLocationField setStringValue:@""];
  [mPageModDateField setStringValue:@""];
  
  [mSiteVerifiedTextField setStringValue:@""];
  [mSiteVerifiedDetailsField setStringValue:@""];
  [mSiteVerifiedImageView setImage:nil];

  [mConnectionTextField setStringValue:@""];
  [mConnectionDetailsField setStringValue:@""];
  [mConnectionImageView setImage:nil];
  
  [self setCertificateItem:nil];
}

- (void)enableButtons
{
  [mShowCertificateButton setEnabled:(mCertificateItem != nil)];
}

- (CertificateItem*)certificateItem
{
  return mCertificateItem;
}

- (void)setCertificateItem:(CertificateItem*)inCertItem
{
  [mCertificateItem autorelease];
  mCertificateItem = [inCertItem retain];
  [self enableButtons];
}

- (void)updateFromBrowserView:(CHBrowserView*)inBrowserView
{
  [self window];  // force window loading
  [self clearInfo];

  if (!inBrowserView) return;
  
  [self updateGeneralInfoFromBrowserView:inBrowserView];
  [self updateSecurityInfoFromBrowserView:inBrowserView];
}

- (void)updateGeneralInfoFromBrowserView:(CHBrowserView*)inBrowserView
{
  nsCOMPtr<nsIDOMWindow> contentWindow = [inBrowserView getContentWindow];
  if (!contentWindow) return;

  // general info
  [mPageTitleField setStringValue:[inBrowserView pageTitle]];
  [mPageLocationField setStringValue:[inBrowserView getCurrentURI]];
  NSDate* lastModDate = [inBrowserView pageLastModifiedDate];

  if (lastModDate)
  {
    NSString* dateFormat = NSLocalizedString(@"PageInfoDateFormat", @"");
    NSDictionary* curCalendarLocale = [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];

    NSString* dateString = [lastModDate descriptionWithCalendarFormat:dateFormat
                                                             timeZone:nil
                                                               locale:curCalendarLocale];
    [mPageModDateField setStringValue:dateString];
  }
}

- (void)updateSecurityInfoFromBrowserView:(CHBrowserView*)inBrowserView
{
  // let's see how many hoops we have to jump through
  nsCOMPtr<nsISecureBrowserUI> secureBrowserUI = [inBrowserView getSecureBrowserUI];
  
  nsCOMPtr<nsISSLStatusProvider> statusProvider = do_QueryInterface(secureBrowserUI);
  if (!statusProvider) return;
  
  nsCOMPtr<nsIX509Cert> serverCert;
  PRUint32 sekritKeyLength = 0;
  NSString* cipherNameString = @"";
  
  nsCOMPtr<nsISupports> secStatus;
  statusProvider->GetSSLStatus(getter_AddRefs(secStatus));
  nsCOMPtr<nsISSLStatus> sslStatus = do_QueryInterface(secStatus);
  if (sslStatus)
  {
    sslStatus->GetServerCert(getter_AddRefs(serverCert));
    sslStatus->GetSecretKeyLength(&sekritKeyLength);
    
    nsXPIDLCString cipherName;
    sslStatus->GetCipherName(getter_Copies(cipherName));
    cipherNameString = [NSString stringWith_nsACString:cipherName];
  }
  
  // encryption info
  PRUint32 pageState;
  nsresult rv = secureBrowserUI->GetState(&pageState);
  BOOL isBroken = NS_FAILED(rv) || (pageState == nsIWebProgressListener::STATE_IS_BROKEN);
  
  if (serverCert)
  {
    [mSiteVerifiedTextField setStringValue:NSLocalizedString(@"WebSiteVerified", @"")];
    
    CertificateItem* certItem = [CertificateItem certificateItemWithCert:serverCert];
    NSString* issuerOrg = [certItem issuerOrganization];
    if ([issuerOrg length] == 0)
      issuerOrg = [certItem issuerName];

    NSString* detailsString = [NSString stringWithFormat:NSLocalizedString(@"WebSiteVerifiedDetailsFormat", @""),
                                                         [inBrowserView pageLocationHost],
                                                         issuerOrg];

    [mSiteVerifiedDetailsField setStringValue:detailsString];
    [mSiteVerifiedImageView setImage:[NSImage imageNamed:@"security_lock"]];
    [self setCertificateItem:certItem];
  }
  else
  {
    [mSiteVerifiedImageView setImage:[NSImage imageNamed:@"security_broken"]];
    [mSiteVerifiedTextField setStringValue:NSLocalizedString(@"WebSiteNotVerified", @"")];
    [mSiteVerifiedDetailsField setStringValue:@""];
  }
  
  if (isBroken)
  {
    [mConnectionTextField setStringValue:NSLocalizedString(@"WebSiteNotVerified", @"")];
    [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionMixedContentDetails", @"")];
    [mConnectionImageView setImage:[NSImage imageNamed:@"security_broken"]];  // XXX need "mixed" lock
  }
  else if (sekritKeyLength >= 90)
  {
    NSString* connectionString = [NSString stringWithFormat:NSLocalizedString(@"ConnectionStrongEncryptionFormat", @""),
                                                         cipherNameString,
                                                         sekritKeyLength];
    [mConnectionTextField setStringValue:connectionString];
    [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionStrongEncryptionDetails", @"")];
    [mConnectionImageView setImage:[NSImage imageNamed:@"security_lock"]];
  }
  else if (sekritKeyLength > 0)
  {
    NSString* connectionString = [NSString stringWithFormat:NSLocalizedString(@"ConnectionWeakEncryptionFormat", @""),
                                                         cipherNameString,
                                                         sekritKeyLength];
    [mConnectionTextField setStringValue:connectionString];
    [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionWeakEncryptionDetails", @"")];
    [mConnectionImageView setImage:[NSImage imageNamed:@"security_lock"]];    // XXX nead "weak" lock
  }
  else
  {
    [mConnectionTextField setStringValue:NSLocalizedString(@"ConnectionNoneEncryption", @"")];
    [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionNoneEncryptionDetails", @"")];
    [mConnectionImageView setImage:[NSImage imageNamed:@"security_broken"]];
  }
}

- (void)autosaveWindowFrame
{
}

@end
