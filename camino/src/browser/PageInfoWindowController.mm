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
#include "nsIX509Cert.h"

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
                                                         certTypeForTrustSettings:nsIX509Cert::SERVER_CERT];
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
  CHSecurityStatus  securityStatus = [inBrowserView securityStatus];
  CertificateItem*  serverCert = [inBrowserView siteCertificate];
  
  // XXX ideally we should tell the user if the cert has expired, or whether
  // it has a domain mismatch here too.
  switch (securityStatus)
  {
    case CHSecurityInsecure:
      [mSiteVerifiedImageView setImage:[NSImage imageNamed:@"security_broken"]];
      [mSiteVerifiedTextField setStringValue:NSLocalizedString(@"WebSiteNotVerified", @"")];
      [mSiteVerifiedDetailsField setStringValue:@""];

      [mConnectionImageView setImage:[NSImage imageNamed:@"security_broken"]];
      [mConnectionTextField setStringValue:NSLocalizedString(@"ConnectionNoneEncryption", @"")];
      [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionNoneEncryptionDetails", @"")];
      break;
      
    case CHSecurityBroken:    // or mixed
      {
        if (serverCert)
        {
          NSString* issuerOrg = [serverCert issuerOrganization];
          if ([issuerOrg length] == 0)
            issuerOrg = [serverCert issuerName];

          NSString* detailsString = [NSString stringWithFormat:NSLocalizedString(@"WebSiteVerifiedMixedDetailsFormat", @""),
                                                               [inBrowserView pageLocationHost],
                                                               issuerOrg];

          [mSiteVerifiedImageView setImage:[NSImage imageNamed:@"security_lock"]];    // XXX need "mixed" lock
          [mSiteVerifiedDetailsField setStringValue:detailsString];
          [mSiteVerifiedTextField setStringValue:NSLocalizedString(@"WebSiteVerifiedMixed", @"")];
          [self setCertificateItem:serverCert];
        }
        else
        {
          [mSiteVerifiedImageView setImage:[NSImage imageNamed:@"security_broken"]];
          [mSiteVerifiedTextField setStringValue:NSLocalizedString(@"WebSiteNotVerified", @"")];
          [mSiteVerifiedDetailsField setStringValue:@""];
        }
        
        [mConnectionImageView setImage:[NSImage imageNamed:@"security_broken"]];  // XXX need "mixed" lock
        [mConnectionTextField setStringValue:NSLocalizedString(@"ConnectionMixedContent", @"")];
        [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionMixedContentDetails", @"")];
      }
      break;
      
    case CHSecuritySecure:
      {
        NSString* issuerOrg = [serverCert issuerOrganization];
        if ([issuerOrg length] == 0)
          issuerOrg = [serverCert issuerName];

        NSString* detailsString = [NSString stringWithFormat:NSLocalizedString(@"WebSiteVerifiedDetailsFormat", @""),
                                                             [inBrowserView pageLocationHost],
                                                             issuerOrg];

        [mSiteVerifiedImageView setImage:[NSImage imageNamed:@"security_lock"]];
        [mSiteVerifiedTextField setStringValue:NSLocalizedString(@"WebSiteVerified", @"")];
        [mSiteVerifiedDetailsField setStringValue:detailsString];
        [self setCertificateItem:serverCert];

        CHSecurityStrength strength = [inBrowserView securityStrength];
        if (strength == CHSecurityHigh)
        {
          [mConnectionImageView setImage:[NSImage imageNamed:@"security_lock"]];
          NSString* connectionString = [NSString stringWithFormat:NSLocalizedString(@"ConnectionStrongEncryptionFormat", @""),
                                                               [inBrowserView cipherName],
                                                               [inBrowserView secretKeyLength]];
          [mConnectionTextField setStringValue:connectionString];
          [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionStrongEncryptionDetails", @"")];
        }
        else if (strength == CHSecurityMedium || strength == CHSecurityLow)
        {
          // "medium" seems to be unused
          [mConnectionImageView setImage:[NSImage imageNamed:@"security_lock"]];    // XXX nead "weak" lock
          NSString* connectionString = [NSString stringWithFormat:NSLocalizedString(@"ConnectionWeakEncryptionFormat", @""),
                                                               [inBrowserView cipherName],
                                                               [inBrowserView secretKeyLength]];
          [mConnectionTextField setStringValue:connectionString];
          [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionWeakEncryptionDetails", @"")];
        }
        else  // this should never happen
        {
          [mConnectionImageView setImage:[NSImage imageNamed:@"security_broken"]];
          [mConnectionTextField setStringValue:NSLocalizedString(@"ConnectionNoneEncryption", @"")];
          [mConnectionDetailsField setStringValue:NSLocalizedString(@"ConnectionNoneEncryptionDetails", @"")];
        }
      }
      break;
  }
}

@end
