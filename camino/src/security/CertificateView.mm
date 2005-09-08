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
#import "NSDate+Utils.h"
#import "NSView+Utils.h"

#import "nsCOMPtr.h"
#import "nsString.h"
#import "nsArray.h"
#import "nsIArray.h"

#import "nsIX509Cert.h"
#import "nsIX509CertDB.h"

#import "nsServiceManagerUtils.h"

#import "CertificateItem.h"
#import "CertificateView.h"


#pragma mark CertificateContentsView
#pragma mark -

@interface CertificateContentsView : NSView
{
}
@end

@implementation CertificateContentsView

- (BOOL)isFlipped
{
  return YES;
}

@end

#pragma mark -

const float kCertImageViewSize          = 64.0f;
const float kCertHeaderFieldVerticalGap = 4.0f;
const float kCertHeaderFieldRightGap    = 4.0f;
const float kStatusImageSize            = 12.0f;

const float kGroupHeaderLeftOffset  = 4.0f;
const float kGapUnderGroupHeader    = 5.0f;
const float kDisclosureLabelGap     = 4.0f;

const float kDisclosureButtonSize = 12.0f;

const float kGeneralRightGap     = 4.0f;

const float kHexViewExpandySize       = 12.0f;
const float kHexViewExpandyLeftGap    = 4.0f;
const float kHexViewExpandyRightGap   = 4.0f;

const float kHeaderLeftOffset    = 16.0f;
const float kGapUnderHeader      = 5.0f;
const float kGapUnderGroup       = 5.0f;

const float kLabelLeftOffset    = 16.0f;
const float kLabelGutterWidth   = 10.0f;
const float kGapUnderLine       = 5.0f;

#pragma mark -
#pragma mark CertificateView
#pragma mark -

@interface CertificateView(Private)

- (float)labelColumnWidth;

- (NSTextField*)textFieldWithInitialFrame:(NSRect)inFrame stringValue:(NSString*)inString small:(BOOL)useSmallFont bold:(BOOL)useBold;

- (void)addGroupingWithKey:(NSString*)inLabelKey expanded:(BOOL)inExpanded action:(SEL)inAction atOffset:(float*)ioOffset;
- (void)addHeaderWithKey:(NSString*)inLabelKey atOffset:(float*)ioOffset;
- (void)addLineWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData atOffset:(float*)ioOffset ignoreBlankLines:(BOOL)inIgnoreBlank;
- (void)addScrollingTextFieldWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData atOffset:(float*)ioOffset ignoreBlankLines:(BOOL)inIgnoreBlank;
- (NSButton*)addButtonLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction atOffset:(float*)ioOffset;
- (NSButton*)addCheckboxLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction atOffset:(float*)ioOffset;

- (void)refreshView;
- (float)rebuildCertHeader;
- (void)rebuildCertContent;
- (float)rebuildDetails:(float)inOffset;
- (float)rebuildTrustSettings:(float)inOffset;
- (BOOL)showTrustSettings;

- (void)certificateChanged:(NSNotification*)inNotification;

@end

#pragma mark -


@implementation CertificateView

- (id)initWithFrame:(NSRect)inFrame
{
  if ((self = [super initWithFrame:inFrame]))
  {
    mDetailsExpanded = YES;
    mTrustExpanded = YES;
    mCertTrustType = nsIX509Cert::UNKNOWN_CERT;

    mExpandedStatesDictionary = [[NSMutableDictionary alloc] initWithCapacity:2];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(certificateChanged:)
                                                 name:CertificateChangedNotificationName
                                               object:nil];
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mExpandedStatesDictionary release];
  [mContentView release];
  [mCertItem release];
  [super dealloc];
}

- (CertificateItem*)certificateItem
{
  return mCertItem;
}

- (void)setCertificateItem:(CertificateItem*)inCertItem
{
  [mCertItem autorelease];
  mCertItem = [inCertItem retain];

  [self refreshView];
}

- (void)setDelegate:(id)inDelegate
{
  mDelegate = inDelegate;
}

- (id)delegate
{
  return mDelegate;
}

- (void)setCertTypeForTrustSettings:(unsigned int)inCertType
{
  mCertTrustType = inCertType;
}

- (void)setDetailsInitiallyExpanded:(BOOL)inExpanded
{
  mDetailsExpanded = inExpanded;
}

- (void)setTrustInitiallyExpanded:(BOOL)inExpanded
{
  mTrustExpanded = inExpanded;
}

- (BOOL)showTrustSettings
{
  return (mCertTrustType == nsIX509Cert::CA_CERT ||
          mCertTrustType == nsIX509Cert::SERVER_CERT ||
          mCertTrustType == nsIX509Cert::EMAIL_CERT);
}

- (IBAction)toggleDetails:(id)sender
{
  mDetailsExpanded = !mDetailsExpanded;
  [self rebuildCertContent];
}

- (IBAction)toggleTrustSettings:(id)sender
{
  mTrustExpanded = !mTrustExpanded;
  [self rebuildCertContent];
}

- (IBAction)saveTrustSettings:(id)sender
{
  if ([self showTrustSettings])
    [mCertItem setTrustedFor:[self trustMaskSetting] asType:mCertTrustType];
}

- (unsigned int)trustMaskSetting
{
  unsigned int certTrustMask = 0;

  BOOL canGetTrust = ([mCertItem isValid] || [mCertItem isUntrustedRootCACert]) &&
                         (mTrustForWebSitesCheckbox != nil) &&
                         (mTrustForEmailCheckbox != nil) &&
                         (mTrustForObjSigningCheckbox != nil);
  if (canGetTrust)
  {
    if ([mTrustForWebSitesCheckbox state] == NSOnState)
      certTrustMask |= nsIX509CertDB::TRUSTED_SSL;

    if ([mTrustForEmailCheckbox state] == NSOnState)
      certTrustMask |= nsIX509CertDB::TRUSTED_EMAIL;

    if ([mTrustForObjSigningCheckbox state] == NSOnState)
      certTrustMask |= nsIX509CertDB::TRUSTED_OBJSIGN;
  }
  else
  {
    certTrustMask = [mCertItem trustMaskForType:mCertTrustType];
  }
  
  return certTrustMask;
}

- (void)showIssuerCert:(id)sender
{
  nsCOMPtr<nsIX509Cert> issuerCert;
  nsIX509Cert* thisCert = [mCertItem cert];
  if (!thisCert)
    return;

  thisCert->GetIssuer(getter_AddRefs(issuerCert));
  if (issuerCert && ![mCertItem isSameCertAs:issuerCert])
  {
    CertificateItem* issuerCertItem = [CertificateItem certificateItemWithCert:issuerCert];
    if ([mDelegate respondsToSelector:@selector(certificateView:showIssuerCertificate:)])
      [mDelegate certificateView:self showIssuerCertificate:issuerCertItem];
  }
}

#pragma mark -

- (BOOL)isFlipped
{
  return YES;
}

- (void)drawRect:(NSRect)inRect
{
  [[NSColor whiteColor] set];
  NSRectFill(inRect);
}

- (float)labelColumnWidth
{
  // measure all the label strings?
  return 120.0f;
}

- (NSTextField*)textFieldWithInitialFrame:(NSRect)inFrame stringValue:(NSString*)inString small:(BOOL)useSmallFont bold:(BOOL)useBold
{
  NSTextField* theTextField = [[[NSTextField alloc] initWithFrame:inFrame] autorelease];
  [theTextField setEditable:NO];
  [theTextField setSelectable:YES];
  NSFont* fieldFont;
  float fontSize = useSmallFont ? [NSFont smallSystemFontSize] : [NSFont systemFontSize];
  if (useBold)
    fieldFont = [NSFont boldSystemFontOfSize:fontSize];
  else
    fieldFont = [NSFont systemFontOfSize:fontSize];
  
  [theTextField setFont:fieldFont];
  [theTextField setDrawsBackground:NO];
  [theTextField setBezeled:NO];
  [theTextField setStringValue:inString];

  [theTextField sizeToFit];

  return theTextField;
}

- (void)addGroupingWithKey:(NSString*)inLabelKey expanded:(BOOL)inExpanded action:(SEL)inAction atOffset:(float*)ioOffset
{
  NSRect lineRect = NSMakeRect(kGroupHeaderLeftOffset, *ioOffset + kGroupHeaderLeftOffset, NSWidth([self frame]) - 2 * kGroupHeaderLeftOffset, 100.0f);

  NSRect buttonRect;
  buttonRect.origin = lineRect.origin;
  buttonRect.size = NSMakeSize(kDisclosureButtonSize, kDisclosureButtonSize);
  
  NSButton* theButton = [[[NSButton alloc] initWithFrame:buttonRect] autorelease];
  [theButton setBordered:NO];
  [theButton setImage:         inExpanded ? [NSImage imageNamed:@"triangle_down_normal"]  : [NSImage imageNamed:@"triangle_right_normal"]];
  [theButton setAlternateImage:inExpanded ? [NSImage imageNamed:@"triangle_down_pressed"] : [NSImage imageNamed:@"triangle_right_pressed"]];
  [theButton setButtonType:NSMomentaryChangeButton];
  [theButton setAction:inAction];
  [theButton setTarget:self];
  [mContentView addSubview:theButton];
  
  lineRect.origin.x = NSMaxX(buttonRect) + kDisclosureLabelGap;
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* headerField = [self textFieldWithInitialFrame:lineRect stringValue:theLabel small:YES bold:YES];
  [mContentView addSubview:headerField];

  float buttonBottom  = NSMaxY([theButton frame]);
  float labelBottom   = NSMaxY([headerField frame]);
  float maxYPos = (buttonBottom > labelBottom) ? buttonBottom : labelBottom;

  *ioOffset = maxYPos + kGapUnderGroupHeader;
}

- (void)addHeaderWithKey:(NSString*)inLabelKey atOffset:(float*)ioOffset
{
  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect headerRect = NSMakeRect(labelOffset, *ioOffset, NSWidth([self frame]) - labelOffset - kGeneralRightGap, 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* headerField = [self textFieldWithInitialFrame:headerRect stringValue:theLabel small:YES bold:YES];
  [mContentView addSubview:headerField];

  *ioOffset = NSMaxY([headerField frame]) + kGapUnderHeader;
}

// returns current V offset
- (void)addLineWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData atOffset:(float*)ioOffset ignoreBlankLines:(BOOL)inIgnoreBlank
{
  if (inIgnoreBlank && [inData length] == 0)
    return;
  
  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect labelRect = NSMakeRect(labelOffset, *ioOffset, NSWidth([self frame]) - labelOffset - kGeneralRightGap, 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* labelField = [self textFieldWithInitialFrame:labelRect stringValue:theLabel small:YES bold:NO];
  [mContentView addSubview:labelField];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect dataRect = NSMakeRect(xPos, *ioOffset, NSWidth([self frame]) - xPos - kLabelLeftOffset, 100.0f);
  NSTextField* dataField = [self textFieldWithInitialFrame:dataRect stringValue:inData small:YES bold:NO];
  [mContentView addSubview:dataField];

  float labelBottom = NSMaxY([labelField frame]);
  float dataBottom  = NSMaxY([dataField frame]);
  float maxYPos = (dataBottom > labelBottom) ? dataBottom : labelBottom;
  
  *ioOffset = maxYPos + kGapUnderLine;
}

- (void)addScrollingTextFieldWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData atOffset:(float*)ioOffset ignoreBlankLines:(BOOL)inIgnoreBlank
{
  if (inIgnoreBlank && [inData length] == 0)
    return;
  
  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect labelRect = NSMakeRect(labelOffset, *ioOffset, NSWidth([self frame]) - labelOffset - kGeneralRightGap, 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* labelField = [self textFieldWithInitialFrame:labelRect stringValue:theLabel small:YES bold:NO];
  [mContentView addSubview:labelField];

  float leftPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  float rightPos = NSWidth([self frame]) - leftPos - kLabelLeftOffset - kHexViewExpandyLeftGap - kHexViewExpandySize - kHexViewExpandyRightGap;
  float textWidth = rightPos - leftPos;
  
  NSRect dataRect = NSMakeRect(leftPos, *ioOffset, rightPos, 56.0f);
  
  NSScrollView* dataScrollView = [[[NSScrollView alloc] initWithFrame:dataRect] autorelease];
  NSTextView* scrolledTextView = [[[NSTextView alloc] initWithFrame:dataRect] autorelease];
  [dataScrollView setHasVerticalScroller:YES];
  [dataScrollView setBorderType:NSBezelBorder];
  [[dataScrollView verticalScroller] setControlSize:NSSmallControlSize];

  [[[scrolledTextView textStorage] mutableString] setString:[inData stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
  [scrolledTextView setFont:[NSFont fontWithName:@"Monaco" size:10.0f]];
  [scrolledTextView setEditable:NO];
  [dataScrollView setDocumentView:scrolledTextView];

  [mContentView addSubview:dataScrollView];

  float labelBottom = NSMaxY([labelField frame]);
  float dataBottom  = NSMaxY([dataScrollView frame]);
  float maxYPos = (dataBottom > labelBottom) ? dataBottom : labelBottom;
  
  *ioOffset = maxYPos + kGapUnderLine;
}

- (NSButton*)addButtonLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction atOffset:(float*)ioOffset
{
  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect labelRect = NSMakeRect(labelOffset, *ioOffset, NSWidth([self frame]) - labelOffset - kGeneralRightGap, 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* labelField = [self textFieldWithInitialFrame:labelRect stringValue:theLabel small:YES bold:NO];
  [mContentView addSubview:labelField];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect buttonRect = NSMakeRect(xPos, *ioOffset, NSWidth([self frame]) - xPos - kLabelLeftOffset, 100.0f);
  NSButton* theButton = [[[NSButton alloc] initWithFrame:buttonRect] autorelease];

  [theButton setTitle:NSLocalizedStringFromTable(buttonKey, @"CertificateDialogs", @"")];
  [theButton setBezelStyle:NSRoundedBezelStyle];
  [theButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [[theButton cell] setControlSize:NSSmallControlSize];
  [theButton sizeToFit];
  
  [theButton setAction:inAction];
  [theButton setTarget:self];
  [mContentView addSubview:theButton];

  float labelBottom = NSMaxY([labelField frame]);
  float buttonBottom  = NSMaxY([theButton frame]);
  float maxYPos = (buttonBottom > labelBottom) ? buttonBottom : labelBottom;
  
  *ioOffset = maxYPos + kGapUnderLine;
  return theButton;
}

- (NSButton*)addCheckboxLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction atOffset:(float*)ioOffset
{
  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect labelRect = NSMakeRect(labelOffset, *ioOffset, NSWidth([self frame]) - labelOffset - kGeneralRightGap, 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* labelField = [self textFieldWithInitialFrame:labelRect stringValue:theLabel small:YES bold:NO];
  [mContentView addSubview:labelField];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect buttonRect = NSMakeRect(xPos, *ioOffset, NSWidth([self frame]) - xPos - kLabelLeftOffset, 100.0f);
  NSButton* theButton = [[[NSButton alloc] initWithFrame:buttonRect] autorelease];

  [theButton setTitle:NSLocalizedStringFromTable(buttonKey, @"CertificateDialogs", @"")];
  [theButton setButtonType:NSSwitchButton];
  [theButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [[theButton cell] setControlSize:NSSmallControlSize];
  [theButton sizeToFit];
  
  [theButton setAction:inAction];
  [theButton setTarget:self];
  [mContentView addSubview:theButton];

  float labelBottom = NSMaxY([labelField frame]);
  float buttonBottom  = NSMaxY([theButton frame]);
  float maxYPos = (buttonBottom > labelBottom) ? buttonBottom : labelBottom;
  
  *ioOffset = maxYPos + kGapUnderLine;
  return theButton;
}

- (float)rebuildDetails:(float)inOffset
{
  float curOffset = inOffset;
  [self addGroupingWithKey:@"CertDetailsGroupHeader" expanded:mDetailsExpanded action:@selector(toggleDetails:) atOffset:&curOffset];
  curOffset += kGapUnderHeader;

  if (!mDetailsExpanded)
    return curOffset;
  
  [self addHeaderWithKey:@"IssuedTo" atOffset:&curOffset];

  // Owner stuff
  [self addLineWithLabelKey:@"OwnerCommonName"    data:[mCertItem commonName]          atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"OwnerEmailAddress"  data:[mCertItem emailAddress]        atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"OwnerOrganization"  data:[mCertItem organization]        atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"OwnerOrgUnit"       data:[mCertItem organizationalUnit]  atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"Version"            data:[mCertItem version]             atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"SerialNumber"       data:[mCertItem serialNumber]        atOffset:&curOffset ignoreBlankLines:YES];

  // Issuer stuff
  curOffset += kGapUnderGroup;
  [self addHeaderWithKey:@"IssuedBy" atOffset:&curOffset];

  [self addLineWithLabelKey:@"IssuerCommonName"   data:[mCertItem issuerCommonName]          atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"IssuerOrganization" data:[mCertItem issuerOrganization]        atOffset:&curOffset ignoreBlankLines:YES];
  [self addLineWithLabelKey:@"IssuerOrgUnit"      data:[mCertItem issuerOrganizationalUnit]  atOffset:&curOffset ignoreBlankLines:YES];

  nsCOMPtr<nsIX509Cert> issuerCert;
  nsIX509Cert* thisCert = [mCertItem cert];
  if (thisCert)
    thisCert->GetIssuer(getter_AddRefs(issuerCert));
  if (issuerCert && ![mCertItem isSameCertAs:issuerCert])
  {
    [self addButtonLineWithLabelKey:@"ShowIssuerCertLabel"
                                 buttonLabelKey:@"ShowIssuerCertButton"
                                         action:@selector(showIssuerCert:)
                                       atOffset:&curOffset];
  }

  // validity
  curOffset += kGapUnderGroup;
  [self addHeaderWithKey:@"Validity" atOffset:&curOffset];

  [self addLineWithLabelKey:@"NotBeforeLocalTime" data:[mCertItem longValidFromString]  atOffset:&curOffset ignoreBlankLines:NO];
  [self addLineWithLabelKey:@"NotAfterLocalTime"  data:[mCertItem longExpiresString]    atOffset:&curOffset ignoreBlankLines:NO];

  // signature
  [self addLineWithLabelKey:@"SigAlgorithm"       data:[mCertItem signatureAlgorithm]   atOffset:&curOffset ignoreBlankLines:NO];
  [self addScrollingTextFieldWithLabelKey:@"Signature" data:[mCertItem signatureValue]  atOffset:&curOffset ignoreBlankLines:NO];

  // public key info
  curOffset += kGapUnderGroup;
  [self addHeaderWithKey:@"PublicKeyInfo" atOffset:&curOffset];
  [self addLineWithLabelKey:@"PublicKeyAlgorithm" data:[mCertItem publicKeyAlgorithm]   atOffset:&curOffset ignoreBlankLines:NO];
  //[self addLineWithLabelKey:@"PublicKeySize"      data:[mCertItem publicKeySizeBits]    atOffset:&curOffset ignoreBlankLines:NO];
  [self addScrollingTextFieldWithLabelKey:@"PublicKey" data:[mCertItem publicKey]       atOffset:&curOffset ignoreBlankLines:NO];
    
  // usages
  NSArray* certUsages = [mCertItem validUsages];
  if ([certUsages count])
  {
    curOffset += kGapUnderGroup;
    [self addHeaderWithKey:@"UsagesTitle" atOffset:&curOffset];
    for (unsigned int i = 0; i < [certUsages count]; i ++)
    {
      NSString* labelKey = (i == 0) ? @"Usages" : @"";
      [self addLineWithLabelKey:labelKey data:[certUsages objectAtIndex:i] atOffset:&curOffset ignoreBlankLines:NO];
    }
  }

  // fingerprints
  curOffset += kGapUnderGroup;
  [self addHeaderWithKey:@"Fingerprints" atOffset:&curOffset];

  [self addLineWithLabelKey:@"SHA1Fingerprint"  data:[mCertItem sha1Fingerprint] atOffset:&curOffset ignoreBlankLines:NO];
  [self addLineWithLabelKey:@"MD5Fingerprint"   data:[mCertItem md5Fingerprint]  atOffset:&curOffset ignoreBlankLines:NO];

  curOffset += kGapUnderGroup;
  return curOffset;
}

- (float)rebuildTrustSettings:(float)inOffset
{
  float curOffset = inOffset;

  [self addGroupingWithKey:@"CertTrustGroupHeader" expanded:mTrustExpanded action:@selector(toggleTrustSettings:) atOffset:&curOffset];
  curOffset += kGapUnderHeader;

  if (!mTrustExpanded)
    return curOffset;

  // XXX do we need a more comprehensive check than this?
  BOOL enableCheckboxes = [mCertItem isValid] || [mCertItem isUntrustedRootCACert];
  
  // XXX only show relevant checkboxes? (see nsNSSCertificateDB::SetCertTrust)
  // XXX only show checkboxes for allowed usages?
  [self addLineWithLabelKey:@"TrustSettingsLabel" data:@"" atOffset:&curOffset ignoreBlankLines:NO];
  mTrustForWebSitesCheckbox = [self addCheckboxLineWithLabelKey:@"" buttonLabelKey:@"TrustWebSitesCheckboxLabel" action:nil atOffset:&curOffset];
  [mTrustForWebSitesCheckbox setState:[mCertItem trustedForSSLAsType:mCertTrustType]];
  [mTrustForWebSitesCheckbox setEnabled:enableCheckboxes];
  
  mTrustForEmailCheckbox = [self addCheckboxLineWithLabelKey:@"" buttonLabelKey:@"TrustEmailUsersCheckboxLabel" action:nil     atOffset:&curOffset];
  [mTrustForEmailCheckbox setState:[mCertItem trustedForEmailAsType:mCertTrustType]];
  [mTrustForEmailCheckbox setEnabled:enableCheckboxes];

  mTrustForObjSigningCheckbox = [self addCheckboxLineWithLabelKey:@"" buttonLabelKey:@"TrustObjectSignersCheckboxLabel" action:nil  atOffset:&curOffset];
  [mTrustForObjSigningCheckbox setState:[mCertItem trustedForObjectSigningAsType:mCertTrustType]];
  [mTrustForObjSigningCheckbox setEnabled:enableCheckboxes];

  curOffset += kGapUnderGroup;
  return curOffset;
}

- (void)refreshView
{
  [self removeAllSubviews];
  [mContentView release];
  mContentView = nil;

  mTrustForWebSitesCheckbox = nil;
  mTrustForEmailCheckbox = nil;
  mTrustForObjSigningCheckbox = nil;
  
  float headerHeight = [self rebuildCertHeader];

  NSRect contentFrame = [self bounds];
  NSRect headerRect, contentsRect;
  NSDivideRect(contentFrame, &headerRect, &contentsRect, headerHeight, NSMinYEdge);
  
  mContentView = [[CertificateContentsView alloc] initWithFrame:contentsRect];
  [mContentView setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
  [self addSubview:mContentView];

  [self rebuildCertContent];
}

- (float)rebuildCertHeader
{
  // We do all this laborious manual view construction to avoid having to load a nib file each
  // time. This way, we can just have a single custom view in the nib, and do everything in code.
  
  // cert image
  NSRect certImageFrame = NSMakeRect(kGroupHeaderLeftOffset, kGroupHeaderLeftOffset, kCertImageViewSize, kCertImageViewSize);
  NSImageView* certImageView = [[[NSImageView alloc] initWithFrame:certImageFrame] autorelease];
  [certImageView setImage:[NSImage imageNamed:@"certificate"]];
  [self addSubview:certImageView];

  // description
  float headerFieldsLeftEdge = kGroupHeaderLeftOffset + kCertImageViewSize + kGroupHeaderLeftOffset;
  float headerFieldYOffset   = kGroupHeaderLeftOffset;
  float headerFieldWith      = NSWidth([self frame]) - headerFieldYOffset - kCertHeaderFieldRightGap;

  NSRect decriptionRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, headerFieldWith, 100.0f);
  NSTextField* descField = [self textFieldWithInitialFrame:decriptionRect stringValue:[mCertItem displayName] small:NO bold:NO];
  [self addSubview:descField];
  headerFieldYOffset += NSHeight([descField frame]) + kCertHeaderFieldVerticalGap;

  // issuer info
  NSRect issuerRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, headerFieldWith, 100.0f);
  NSString* formatString = NSLocalizedStringFromTable(@"IssuedByHeaderFormat", @"CertificateDialogs", @"");
  NSString* issuerString = [NSString stringWithFormat:formatString, [mCertItem issuerCommonName]];
  NSTextField* issuerField = [self textFieldWithInitialFrame:issuerRect stringValue:issuerString small:YES bold:NO];
  [self addSubview:issuerField];
  headerFieldYOffset += NSHeight([issuerField frame]) + kCertHeaderFieldVerticalGap;

  // expiry info
  NSRect expiryRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, headerFieldWith, 100.0f);
  formatString = NSLocalizedStringFromTable(@"ExpiresHeaderFormat", @"CertificateDialogs", @"");
  NSString* expiryString = [NSString stringWithFormat:formatString, [mCertItem longExpiresString]];
  NSTextField* expiryField = [self textFieldWithInitialFrame:expiryRect stringValue:expiryString small:YES bold:NO];
  [self addSubview:expiryField];
  headerFieldYOffset += NSHeight([expiryField frame]) + kCertHeaderFieldVerticalGap;

  NSRect statusImageRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, kStatusImageSize, kStatusImageSize);
  NSImageView* statusImageView = [[[NSImageView alloc] initWithFrame:statusImageRect] autorelease];
  
  NSString* imageName = @"";
  if ([mCertItem isUntrustedRootCACert])
    imageName = @"mini_cert_untrusted";
  else if ([mCertItem isValid])
    imageName = @"mini_cert_valid";
  else
    imageName = @"mini_cert_invalid";
   
  [statusImageView setImage:[NSImage imageNamed:imageName]];
  [self addSubview:statusImageView];

  // status info
  float statusLeftEdge  = headerFieldsLeftEdge + kStatusImageSize + kGroupHeaderLeftOffset;
  float statusFieldWith = NSWidth([self frame]) - statusLeftEdge - kCertHeaderFieldRightGap;
  
  NSRect statusRect = NSMakeRect(statusLeftEdge, headerFieldYOffset, statusFieldWith, 100.0f);
  NSTextField* statusField = [self textFieldWithInitialFrame:statusRect stringValue:@"" small:YES bold:NO];
  [statusField setAttributedStringValue:[mCertItem attributedLongValidityString]];
  [statusField sizeToFit];
  [self addSubview:statusField];
  headerFieldYOffset += NSHeight([statusField frame]) + kCertHeaderFieldVerticalGap;
}

- (void)rebuildCertContent
{
  // blow away the subviews of the content view
  [mContentView removeAllSubviews];
  
  float curHeight = 0.0f;
  if ([self showTrustSettings])
    curHeight = [self rebuildTrustSettings:curHeight];

  curHeight = [self rebuildDetails:curHeight];
  
  // make sure the details view is big enough to contain all its subviews
  NSSize curSize = [mContentView frame].size;
  curSize.height = curHeight;
  [mContentView setFrameSize:curSize];

  // and then resize us too
  NSRect contentViewFrame = [mContentView frame];
  float myHeight = NSHeight(contentViewFrame) + NSMinY(contentViewFrame);
  curSize = [self frame].size;
  curSize.height = myHeight;
  [self setFrameSize:curSize];
}

- (void)certificateChanged:(NSNotification*)inNotification
{
  CertificateItem* changedCert = [inNotification object];
  if ([mCertItem isEqualTo:changedCert])
  {
   [self refreshView];
  }
}

@end

