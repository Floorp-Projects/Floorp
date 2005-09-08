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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <smfr@smfr.org>
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

#import "nsCOMPtr.h"
#import "nsString.h"

#import "nsIX509Cert.h"

#import "ImageAdditions.h"

#import "CertificateItem.h"
#import "CertificateView.h"
#import "ViewCertificateDialogController.h"
#import "BrowserSecurityDialogs.h"


#pragma mark BrowserSecurityUIProvider
#pragma mark -

@implementation BrowserSecurityUIProvider

static BrowserSecurityUIProvider* gBrowserSecurityUIProvider;

+ (BrowserSecurityUIProvider*)sharedBrowserSecurityUIProvider
{
  if (!gBrowserSecurityUIProvider)
    gBrowserSecurityUIProvider = [[BrowserSecurityUIProvider alloc] init];

  return gBrowserSecurityUIProvider;
}

+ (NSImage*)lockBadgedApplicationIcon
{
  return [[NSImage imageNamed:@"NSApplicationIcon"] imageByApplyingBadge:[NSImage imageNamed:@"lock_badge"]
                                                               withAlpha:1.0f
                                                                   scale:0.6f];
}

- (id)init
{
  if ((self = [super init]))
  {
  }
  return self;
}

- (void)dealloc
{
  if (gBrowserSecurityUIProvider == self)
    gBrowserSecurityUIProvider = nil;
  [super dealloc];
}

- (DownloadCACertDialogController*)downloadCACertDialogController
{
  DownloadCACertDialogController* dialogController = [[DownloadCACertDialogController alloc] initWithWindowNibName:@"DownloadCACertDialog"];
  return [dialogController autorelease];
}

- (MismatchedCertDomainDialogController*)mismatchedCertDomainDialogController
{
  MismatchedCertDomainDialogController* dialogController = [[MismatchedCertDomainDialogController alloc] initWithWindowNibName:@"MismatchedCertDomainDialog"];
  return [dialogController autorelease];
}

- (UnknownCertIssuerDialogController*)unknownCertIssuerDialogController
{
  UnknownCertIssuerDialogController* dialogController = [[UnknownCertIssuerDialogController alloc] initWithWindowNibName:@"UnknownCertIssuerDialog"];
  return [dialogController autorelease];
}

- (ExpiredCertDialogController*)expiredCertDialogController
{
  ExpiredCertDialogController* dialogController = [[ExpiredCertDialogController alloc] initWithWindowNibName:@"ExpiredCertDialog"];
  return [dialogController autorelease];
}

- (CreatePasswordDialogController*)createPasswordDialogController
{
  CreatePasswordDialogController* dialogController = [[CreatePasswordDialogController alloc] initWithWindowNibName:@"CreatePasswordDialog"];
  return [dialogController autorelease];
}

- (GenKeyPairDialogController*)genKeyPairDialogController
{
  GenKeyPairDialogController* dialogController = [[GenKeyPairDialogController alloc] initWithWindowNibName:@"GenerateKeyPairDialog"];
  return [dialogController autorelease];
}

- (ChooseCertDialogController*)chooseCertDialogController
{
  ChooseCertDialogController* dialogController = [[ChooseCertDialogController alloc] initWithWindowNibName:@"ChooseCertificateDialog"];
  return [dialogController autorelease];
}

@end

#pragma mark -
#pragma mark CertificateDialogController
#pragma mark -

@implementation CertificateDialogController

- (void)dealloc
{
  NSLog(@"dealloc %@", self);
  [super dealloc];
}

- (void)windowDidLoad
{
  [mIcon setImage:[BrowserSecurityUIProvider lockBadgedApplicationIcon]];
}

- (void)setCertificateItem:(CertificateItem*)inCert
{
  [self window];  // ensure window loaded

  [mCertificateView setCertificateItem:inCert];
  [mCertificateView setDelegate:self];
}

// CertificateViewDelegate method
- (void)certificateView:(CertificateView*)certView showIssuerCertificate:(CertificateItem*)issuerCert
{
  // if we are modal, then this must also be modal
  [ViewCertificateDialogController runModalCertificateWindowWithCertificateItem:issuerCert
                                                       certTypeForTrustSettings:nsIX509Cert::CA_CERT];
}

@end

#pragma mark -
#pragma mark DownloadCACertDialogController
#pragma mark -

@interface DownloadCACertDialogController(Private)

- (void)setupWindow;

@end

#pragma mark -

@implementation DownloadCACertDialogController

- (void)dealloc
{
  NSLog(@"dealloc %@", self);
  [super dealloc];
}

- (IBAction)onOK:(id)sender
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (void)setCertificateItem:(CertificateItem*)inCert
{
  [self window];    // ensure window loaded

  NSString* messageFormatString = NSLocalizedStringFromTable(@"ConfirmCAMessageFormat", @"CertificateDialogs", @"");
  [mMessageField setStringValue:[NSString stringWithFormat:messageFormatString, [inCert displayName]]];

  [mCertificateView setDetailsInitiallyExpanded:NO];
  [mCertificateView setTrustInitiallyExpanded:YES];
  [mCertificateView setCertTypeForTrustSettings:nsIX509Cert::CA_CERT];

  [super setCertificateItem:inCert];
}

- (unsigned int)trustMaskSetting
{
  return [mCertificateView trustMaskSetting];
}

@end

#pragma mark -
#pragma mark UnknownCertIssuerDialogController
#pragma mark -


@implementation UnknownCertIssuerDialogController

- (IBAction)onAcceptOneTime:(id)sender
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onAcceptAlways:(id)sender
{
  [NSApp stopModalWithCode:NSAlertOtherReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onStop:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (void)setCertificateItem:(CertificateItem*)inCert
{
  [self window];    // ensure window visible

  NSString* titleFormat = NSLocalizedStringFromTable(@"UnverifiedCertTitleFormat", @"CertificateDialogs", @"");
  NSString* title = [NSString stringWithFormat:titleFormat, [inCert commonName]];
  [mTitleField setStringValue:title];

  NSString* msgFormat = NSLocalizedStringFromTable(@"UnverifiedCertMessageFormat", @"CertificateDialogs", @"");
  NSString* msg = [NSString stringWithFormat:msgFormat, [inCert commonName]];
  [mMessageField setStringValue:msg];

  [mCertificateView setDetailsInitiallyExpanded:NO];
  [mCertificateView setTrustInitiallyExpanded:NO];

  [super setCertificateItem:inCert];
}

@end

#pragma mark -
#pragma mark ExpiredCertDialogController
#pragma mark -

@implementation ExpiredCertDialogController

- (IBAction)onOK:(id)sender
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (void)setCertificateItem:(CertificateItem*)inCert
{
  [self window];    // ensure window visible

  NSString* titleFormat = NSLocalizedStringFromTable(@"ExpiredCertTitleFormat", @"CertificateDialogs", @"");
  NSString* title = [NSString stringWithFormat:titleFormat, [inCert commonName]];
  [mTitleField setStringValue:title];

  NSString* msgFormat = NSLocalizedStringFromTable(@"ExpiredCertMessageFormat", @"CertificateDialogs", @"");
  NSString* msg = [NSString stringWithFormat:msgFormat, [inCert commonName], [inCert longExpiresString]];
  [mMessageField setStringValue:msg];

  [mCertificateView setDetailsInitiallyExpanded:NO];
  [mCertificateView setTrustInitiallyExpanded:NO];

  [super setCertificateItem:inCert];
}

- (void)windowDidLoad
{
  [mIcon setImage:[BrowserSecurityUIProvider lockBadgedApplicationIcon]];
}

@end

#pragma mark -
#pragma mark MismatchedCertDomainDialogController
#pragma mark -

@implementation MismatchedCertDomainDialogController

- (void)dealloc
{
  [mTargetURL release];
  [super dealloc];
}

- (IBAction)onOK:(id)sender
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (IBAction)viewCertificate:(id)sender
{
//  [ViewCertificateDialogController runModalCertificateWindowWithCertificateItem:mCertItem
//                                                       certTypeForTrustSettings:nsIX509Cert::SERVER_CERT];
}

- (void)updateDialog
{
  CertificateItem* certItem = [mCertificateView certificateItem];
  if (!certItem) return;

  NSString* msgFormat = NSLocalizedStringFromTable(@"DomainMistmatchMsgFormat", @"CertificateDialogs", @"");
  NSString* msg = [NSString stringWithFormat:msgFormat, mTargetURL, [certItem commonName]];
  [mMessageField setStringValue:msg];
}

- (void)windowDidLoad
{
  [mIcon setImage:[BrowserSecurityUIProvider lockBadgedApplicationIcon]];
  [self updateDialog];
}

- (void)setCertificateItem:(CertificateItem*)inCert
{
  [self window];
  [mCertificateView setDetailsInitiallyExpanded:NO];
  [mCertificateView setTrustInitiallyExpanded:NO];
  [mCertificateView setCertificateItem:inCert];
  [mCertificateView setDelegate:self];
  [self updateDialog];
}

- (void)setTargetURL:(NSString*)inTargetURL
{
  [mTargetURL autorelease];
  mTargetURL = [inTargetURL retain];
}

// CertificateViewDelegate method
- (void)certificateView:(CertificateView*)certView showIssuerCertificate:(CertificateItem*)issuerCert
{
  // if we are modal, then this must also be modal
  [ViewCertificateDialogController runModalCertificateWindowWithCertificateItem:issuerCert
                                                       certTypeForTrustSettings:nsIX509Cert::CA_CERT];
}

@end

#pragma mark -
#pragma mark CreatePasswordDialogController
#pragma mark -

@interface CreatePasswordDialogController(Private)

+ (float)strengthOfPassword:(NSString*)inPassword;
+ (BOOL)isDictionaryWord:(NSString*)inPassword;

- (void)onPasswordFieldChanged:(id)sender;
- (BOOL)checkOldPassword;

@end

#pragma mark -

@implementation CreatePasswordDialogController

+ (float)strengthOfPassword:(NSString*)inPassword
{
  // length
  int numChars = [inPassword length];
  if (numChars > 5)
    numChars = 5;

  // number of numbers
  NSString* strippedString = [inPassword stringByRemovingCharactersInSet:[NSCharacterSet decimalDigitCharacterSet]];
  int numNumeric = [inPassword length] - [strippedString length];
  if (numNumeric > 3)
    numNumeric = 3;

  // number of symbols
  strippedString = [inPassword stringByRemovingCharactersInSet:[NSCharacterSet punctuationCharacterSet]];
  int numSymbols = [inPassword length] - [strippedString length];
  if (numSymbols > 3)
    numSymbols = 3;

  strippedString = [inPassword stringByRemovingCharactersInSet:[NSCharacterSet uppercaseLetterCharacterSet]];
  int numUppercase = [inPassword length] - [strippedString length];
  if (numUppercase > 3)
    numUppercase = 3;

  float passwordStrength = ((numChars * 10) - 20) + (numNumeric * 10) + (numSymbols * 15) + (numUppercase * 10);
  if (passwordStrength < 0.0f)
    passwordStrength = 0.0f;

  if (passwordStrength > 100.0f)
    passwordStrength = 100.0f;
  
  return passwordStrength;
}

+ (BOOL)isDictionaryWord:(NSString*)inPassword
{
  NSSpellChecker* spellChecker = [NSSpellChecker sharedSpellChecker];
  if (!spellChecker)
    return NO;

  // finds misspelled words
  NSRange foundRange = [spellChecker checkSpellingOfString:[inPassword lowercaseString] startingAt:0];
//  if (foundRange.location == NSNotFound)    // no mispellings
//    return YES;

  // if it's a substring, it's OK  
  return !NSEqualRanges(foundRange, NSMakeRange(0, [inPassword length]));
}

- (void)dealloc
{
  NSLog(@"dealloc %@", self);
  [mCurPasswordContainer release];
  [super dealloc];
}

- (void)windowDidLoad
{
  [mCurPasswordContainer retain];   // so we can remove without it going away
  [mIcon setImage:[BrowserSecurityUIProvider lockBadgedApplicationIcon]];
  
  [mNewPasswordNotesField setStringValue:@""];
  [mVerifyPasswordNotesField setStringValue:@""];
  
  mShowingOldPassword = YES;
}

- (void)setDelegate:(id)inDelegate
{
  mDelegate = inDelegate;
}

- (id)delegate
{
  return mDelegate;
}

- (IBAction)onOK:(id)sender
{
  if (![self checkOldPassword])
    return;

  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (void)hideChangePasswordField
{
  [self window];

  NSRect curPasswordFrame = [mCurPasswordContainer frame];
  [mCurPasswordContainer removeFromSuperview];
  
  // move the rest up
  NSPoint containerOrigin = curPasswordFrame.origin;
  containerOrigin.y -= NSHeight([mNewPasswordContainer frame]) - NSHeight(curPasswordFrame);
  [mNewPasswordContainer setFrameOrigin:containerOrigin];

  // and shrink the window
  float heightDelta = NSMinY([mNewPasswordContainer frame]);     // we're flipped
  
  NSRect windowBounds = [[self window] frame];
  windowBounds.origin.x -= heightDelta;
  windowBounds.size.height -= heightDelta;
  [[self window] setFrame:windowBounds display:YES];
  
  [[self window] makeFirstResponder:mNewPasswordField];
  mShowingOldPassword = NO;
}

- (void)setTitle:(NSString*)inTitle message:(NSString*)inMessage
{
  [self window];

  [mTitleField setStringValue:inTitle];
  [mMessageField setStringValue:inMessage];
}

- (NSString*)currentPassword
{
  return [mCurPasswordField stringValue];
}

- (NSString*)newPassword
{
  return [mNewPasswordField stringValue];
}

// called when either of the "new" password fields changes
- (void)onPasswordFieldChanged:(id)sender
{
  // update password quality
  float pwQuality = [CreatePasswordDialogController strengthOfPassword:[mNewPasswordField stringValue]];
  BOOL isDictionaryWord = [CreatePasswordDialogController isDictionaryWord:[mNewPasswordField stringValue]];
  if (isDictionaryWord)
    pwQuality = 1.0f;

  [mQualityIndicator setDoubleValue:pwQuality];
  
  // check for dictionary words
  if (isDictionaryWord)
  {
    NSString* warningString = NSLocalizedStringFromTable(@"IsDictionaryWord", @"CertificateDialogs", @"");

    NSDictionary* attribs = [NSDictionary dictionaryWithObject:[NSColor orangeColor] forKey:NSForegroundColorAttributeName];
    NSAttributedString* attribString = [[[NSAttributedString alloc] initWithString:warningString attributes:attribs] autorelease];
    [mNewPasswordNotesField setAttributedStringValue:attribString];
  }
  else
    [mNewPasswordNotesField setStringValue:@""];
  
  BOOL passwordMatch = [[mNewPasswordField stringValue] isEqualToString:[mVerifyPasswordField stringValue]];
  if (([[mVerifyPasswordField stringValue] length] > 0) && !passwordMatch)
  {
    NSString* warningString = NSLocalizedStringFromTable(@"UnmatchedPasswords", @"CertificateDialogs", @"");
    [mVerifyPasswordNotesField setStringValue:warningString];
  }
  else
  {
    [mVerifyPasswordNotesField setStringValue:@""];
  }

  [mOKButton setEnabled:passwordMatch];
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
  if ([aNotification object] == mNewPasswordField ||
      [aNotification object] == mVerifyPasswordField)
    [self onPasswordFieldChanged:[aNotification object]];
}

- (BOOL)checkOldPassword
{
  if (!mShowingOldPassword)
    return YES;

  BOOL canContinue = YES;
  if (mDelegate && [mDelegate respondsToSelector:@selector(changePasswordDialogController:oldPasswordValid:)])
    canContinue = [mDelegate changePasswordDialogController:self oldPasswordValid:[mCurPasswordField stringValue]];

  if (!canContinue)
  {
    [mCurPasswordField selectText:nil];
  }
  
  return canContinue;
}

@end

#pragma mark -
#pragma mark GenKeyPairDialogController
#pragma mark -

@implementation GenKeyPairDialogController

- (void)windowDidLoad
{
  [mIcon setImage:[BrowserSecurityUIProvider lockBadgedApplicationIcon]];
  [mProgressIndicator startAnimation:nil];    // it just animates the whole time
}

- (IBAction)onCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (void)keyPairGenerationComplete
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

@end

#pragma mark -
#pragma mark GenKeyPairDialogController
#pragma mark -

@implementation ChooseCertDialogController

- (void)windowDidLoad
{
  [mIcon setImage:[BrowserSecurityUIProvider lockBadgedApplicationIcon]];
}

- (IBAction)onOK:(id)sender
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
  [[self window] orderOut:nil];
}

- (IBAction)onCertPopupChanged:(id)sender
{
  [mCertificateView setCertificateItem:[self selectedCert]];
}

- (void)setCommonName:(NSString*)inCommonName organization:(NSString*)inOrg issuer:(NSString*)inIssuer
{
  [self window];  // force window loading

  NSString* messageFormat = NSLocalizedStringFromTable(@"ChooseCertMessageFormat", @"CertificateDialogs", @"");
  NSString* messageString = [NSString stringWithFormat:messageFormat, inCommonName, inOrg];
  [mMessageText setStringValue:messageString];
}

- (void)setCertificates:(NSArray*)inCertificates
{
  [self window];  // force window loading
  
  [mCertificateView setDelegate:self];

  [mCertPopup removeAllItems];
  NSEnumerator* certsEnum = [inCertificates objectEnumerator];
  CertificateItem* thisCert;

  // could improve this format to show the email address
  NSString* popupItemFormat = NSLocalizedStringFromTable(@"ChooserCertPopupFormat", @"CertificateDialogs", @"");
  
  while ((thisCert = [certsEnum nextObject]))
  {
    NSString* itemTitle = [NSString stringWithFormat:popupItemFormat, [thisCert nickname], [thisCert serialNumber]];
    NSMenuItem* certMenuItem = [[[NSMenuItem alloc] initWithTitle:itemTitle
                                                           action:NULL
                                                    keyEquivalent:@""] autorelease];
    [certMenuItem setRepresentedObject:thisCert];
    [[mCertPopup menu] addItem:certMenuItem];
  }
  
  [mCertPopup synchronizeTitleAndSelectedItem];
  [self onCertPopupChanged:nil];
}

- (CertificateItem*)selectedCert
{
  return [[mCertPopup selectedItem] representedObject];
}

// CertificateViewDelegate method
- (void)certificateView:(CertificateView*)certView showIssuerCertificate:(CertificateItem*)issuerCert
{
  [ViewCertificateDialogController runModalCertificateWindowWithCertificateItem:issuerCert
                                                       certTypeForTrustSettings:nsIX509Cert::CA_CERT];
}

@end
