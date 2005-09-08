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

#import <AppKit/AppKit.h>

class nsIX509Cert;

@class DownloadCACertDialogController;
@class MismatchedCertDomainDialogController;
@class UnknownCertIssuerDialogController;
@class ExpiredCertDialogController;
@class CreatePasswordDialogController;
@class GenKeyPairDialogController;
@class ChooseCertDialogController;

@interface BrowserSecurityUIProvider : NSObject
{
}

+ (BrowserSecurityUIProvider*)sharedBrowserSecurityUIProvider;
+ (NSImage*)lockBadgedApplicationIcon;

// these dialog controllers are autoreleased. You should retain them while running the dialog.
- (DownloadCACertDialogController*)downloadCACertDialogController;
- (MismatchedCertDomainDialogController*)mismatchedCertDomainDialogController;
- (UnknownCertIssuerDialogController*)unknownCertIssuerDialogController;
- (ExpiredCertDialogController*)expiredCertDialogController;
- (CreatePasswordDialogController*)createPasswordDialogController;
- (GenKeyPairDialogController*)genKeyPairDialogController;
- (ChooseCertDialogController*)chooseCertDialogController;

@end


@class CertificateView;

// generic base class for a dialog that contains an icon and a certificate view
@interface CertificateDialogController : NSWindowController
{
  IBOutlet NSImageView*           mIcon;
  IBOutlet CertificateView*       mCertificateView;
}

- (void)setCertificateItem:(CertificateItem*)inCert;

@end


@interface DownloadCACertDialogController : CertificateDialogController
{
  IBOutlet NSTextField*           mMessageField;
}

- (IBAction)onOK:(id)sender;
- (IBAction)onCancel:(id)sender;

// mask of nsIX509CertDB usage flags, from the cert view
- (unsigned int)trustMaskSetting;

@end


// this is really a misnomer. it's shown when a site presents a certificate
// that is invalid or untrusted.
@interface UnknownCertIssuerDialogController : CertificateDialogController
{
  IBOutlet NSTextField*       mTitleField;
  IBOutlet NSTextField*       mMessageField;
}

- (IBAction)onAcceptOneTime:(id)sender;
- (IBAction)onAcceptAlways:(id)sender;
- (IBAction)onStop:(id)sender;

@end


@interface ExpiredCertDialogController : CertificateDialogController
{
  IBOutlet NSTextField*       mTitleField;
  IBOutlet NSTextField*       mMessageField;
}

- (IBAction)onOK:(id)sender;
- (IBAction)onCancel:(id)sender;


@end

@interface MismatchedCertDomainDialogController : CertificateDialogController
{
  IBOutlet NSTextField*       mMessageField;

  NSString*                   mTargetURL;   // retained
}

- (IBAction)onOK:(id)sender;
- (IBAction)onCancel:(id)sender;

- (IBAction)viewCertificate:(id)sender;

- (void)setCertificateItem:(CertificateItem*)inCert;
- (void)setTargetURL:(NSString*)inTargetURL;

@end


// informal protocol implemented by delegates of CreatePasswordDialogController, used to check
// the old password
@interface NSObject(CreatePasswordDelegate)

- (BOOL)changePasswordDialogController:(CreatePasswordDialogController*)inController oldPasswordValid:(NSString*)oldPassword;

@end

@interface CreatePasswordDialogController : NSWindowController
{
  IBOutlet NSImageView*         mIcon;

  IBOutlet NSButton*            mOKButton;
  IBOutlet NSButton*            mCancelButton;
  
  IBOutlet NSTextField*         mTitleField;
  IBOutlet NSTextField*         mMessageField;

  IBOutlet NSView*              mCurPasswordContainer;
  IBOutlet NSView*              mNewPasswordContainer;

  IBOutlet NSTextField*         mCurPasswordField;

  IBOutlet NSTextField*         mNewPasswordField;
  IBOutlet NSTextField*         mVerifyPasswordField;

  IBOutlet NSTextField*         mNewPasswordNotesField;
  IBOutlet NSTextField*         mVerifyPasswordNotesField;

  // this should really be an NSLevelIndicator
  IBOutlet NSProgressIndicator* mQualityIndicator;
  
  BOOL                          mShowingOldPassword;
  id                            mDelegate;
}

- (void)setDelegate:(id)inDelegate;
- (id)delegate;

- (IBAction)onOK:(id)sender;
- (IBAction)onCancel:(id)sender;

- (void)setTitle:(NSString*)inTitle message:(NSString*)inMessage;
- (void)hideChangePasswordField;

- (NSString*)currentPassword;
- (NSString*)newPassword;

@end


@interface GenKeyPairDialogController : NSWindowController
{
  IBOutlet NSImageView*           mIcon;
  IBOutlet NSProgressIndicator*   mProgressIndicator;
}

- (IBAction)onCancel:(id)sender;
- (void)keyPairGenerationComplete;

@end

@interface ChooseCertDialogController : NSWindowController
{
  IBOutlet NSImageView*           mIcon;

  IBOutlet NSTextField*           mMessageText;
  
  IBOutlet NSPopUpButton*         mCertPopup;
  IBOutlet CertificateView*       mCertificateView;
}

- (IBAction)onOK:(id)sender;
- (IBAction)onCancel:(id)sender;

- (IBAction)onCertPopupChanged:(id)sender;

- (void)setCommonName:(NSString*)inCommonName organization:(NSString*)inOrg issuer:(NSString*)inIssuer;
- (void)setCertificates:(NSArray*)inCertificates;
- (CertificateItem*)selectedCert;

@end

