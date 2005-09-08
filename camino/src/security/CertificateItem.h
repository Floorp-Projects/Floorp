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

// object is the CertificateItem. No userinfo.
extern NSString* const CertificateChangedNotificationName;

// A class that wraps a single nsIX509Cert certificate object.
// Note that these aren't necessarily singletons (more than one
// per cert may exist; use -isEqual to test for equality). This
// is a limitation imposed by nsIX509Cert.
//
// Note also that the lifetimes of these things matter if you
// are deleting certs. NSS keeps the underlying certs around
// until the last ref to a nsIX509Cert* has gone away, so
// if you tell the nsIX509CertDB to delete a cert, you have to
// release all refs to the deleted cert before reloading the
// data source. Sucky, eh?
// 
@interface CertificateItem : NSObject
{
  nsIX509Cert*    mCert;    // owned

  NSDictionary*   mASN1InfoDict;    // owned. this is a nested set of dictionaries
                                    // keyed by nsIASN1Object display names
  
  unsigned long   mVerification;    // we cache this because it's slow to obtain
  BOOL            mGotVerification;
}

+ (CertificateItem*)certificateItemWithCert:(nsIX509Cert*)inCert;
- (id)initWithCert:(nsIX509Cert*)inCert;

- (nsIX509Cert*)cert;   // does not addref
- (BOOL)isSameCertAs:(nsIX509Cert*)inCert;
- (BOOL)isEqualTo:(id)object;
- (BOOL)certificateIsIssuerCert:(CertificateItem*)inCert;
- (BOOL)certificateIsInParentChain:(CertificateItem*)inCert;   // includes self

- (NSString*)displayName;

- (NSString*)nickname;
- (NSString*)subjectName;
- (NSString*)organization;
- (NSString*)organizationalUnit;
- (NSString*)commonName;
- (NSString*)emailAddress;
- (NSString*)serialNumber;

- (NSString*)sha1Fingerprint;
- (NSString*)md5Fingerprint;

- (NSString*)issuerName;
- (NSString*)issuerCommonName;
- (NSString*)issuerOrganization;
- (NSString*)issuerOrganizationalUnit;

- (NSString*)signatureAlgorithm;
- (NSString*)signatureValue;
- (NSString*)signatureValue;

- (NSString*)publicKeyAlgorithm;
- (NSString*)publicKey;
- (NSString*)publicKeySizeBits;   // returns the wrong value at present

- (NSString*)version;   // certificate "Version"

// - (NSArray*)extensions;

- (NSString*)usagesStringIgnoringOSCP:(BOOL)inIgnoreOSCP;
// return an array of human-readable usage strings
- (NSArray*)validUsages;

- (BOOL)isRootCACert;
- (BOOL)isUntrustedRootCACert;

- (NSDate*)expiresDate;
- (NSString*)shortExpiresString;
- (NSString*)longExpiresString;

- (NSDate*)validFromDate;
- (NSString*)shortValidFromString;
- (NSString*)longValidFromString;

- (BOOL)isExpired;
- (BOOL)isNotYetValid;

// these just check the parent CA chain and expiry etc. They do not indicate
// validity for any particular usage (e.g. the cert may still be untrusted).
- (BOOL)isValid;
- (NSString*)validity;
- (NSAttributedString*)attributedShortValidityString;
- (NSAttributedString*)attributedLongValidityString;

- (unsigned int)trustMaskForType:(unsigned int)inType;
- (BOOL)trustedFor:(unsigned int)inUsage asType:(unsigned int)inType;
- (BOOL)trustedForSSLAsType:(unsigned int)inType;
- (BOOL)trustedForEmailAsType:(unsigned int)inType;
- (BOOL)trustedForObjectSigningAsType:(unsigned int)inType;

// inUsageMask is flags in nsIX509CertDB
- (void)setTrustedFor:(unsigned int)inUsageMask asType:(unsigned int)inType;
- (void)setTrustedForSSL:(BOOL)inTrustSSL forEmail:(BOOL)inForEmail forObjSigning:(BOOL)inForObjSigning asType:(unsigned int)inType;

@end


class CertificateItemManagerObjects;

// an object that manages CertificateItems.
@interface CertificateItemManager : NSObject
{

  CertificateItemManagerObjects*      mDataObjects;   // C++ wrapper for nsCOMPtrs

}

+ (CertificateItemManager*)sharedCertificateItemManager;
+ (CertificateItem*)certificateItemWithCert:(nsIX509Cert*)inCert;

- (NSString*)valueForStringBundleKey:(NSString*)inKey;

@end

