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

#import "AutoSizingTextField.h"

#import "CHStackView.h"
#import "CertificateItem.h"
#import "CertificateView.h"


const float kCertImageViewSize          = 64.0f;
const float kCertHeaderFieldVerticalGap = 4.0f;
const float kCertHeaderFieldRightGap    = 4.0f;
const float kStatusImageSize            = 12.0f;

const float kGroupHeaderLeftOffset  = 4.0f;
const float kGapAboveGroup          = 2.0f;
const float kGapUnderGroupHeader    = 5.0f;
const float kDisclosureLabelGap     = 4.0f;

const float kDisclosureButtonSize = 12.0f;

const float kGeneralRightGap     = 4.0f;

const float kHeaderLeftOffset    = 16.0f;
const float kGapAboveHeader      = 2.0f;
const float kGapUnderHeader      = 3.0f;
const float kGapUnderGroup       = 5.0f;

const float kLabelLeftOffset    = 16.0f;
const float kLabelGutterWidth   = 10.0f;
const float kGapUnderLine         = 5.0f;
const float kGapUnderCheckboxLine = 3.0f;

#pragma mark -
#pragma mark CertificateView
#pragma mark -

@interface CertificateView(Private)

- (float)labelColumnWidth;

- (CHShrinkWrapView*)containerViewWithTopPadding:(float)inTop bottomPadding:(float)inBottom;
- (NSTextField*)textFieldWithInitialFrame:(NSRect)inFrame stringValue:(NSString*)inString autoSizing:(BOOL)inAutosize small:(BOOL)useSmallFont bold:(BOOL)useBold;

- (CHStackView *)groupingWithKey:(NSString*)inLabelKey expanded:(BOOL)inExpanded action:(SEL)inAction;
- (NSView*)headerWithKey:(NSString*)inLabelKey;
- (NSTextField*)addLineLabelWithKey:(NSString*)inLabelKey toView:(NSView*)inLineContainerView;
- (NSView*)wholeLineLabelWithKey:(NSString*)inLabelKey;
- (NSView*)lineWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData ignoreBlankLines:(BOOL)inIgnoreBlank;
- (NSView*)scrollingTextFieldWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData ignoreBlankLines:(BOOL)inIgnoreBlank;
- (NSView*)buttonLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction button:(NSButton**)outButton;
- (NSView*)checkboxLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction button:(NSButton**)outButton;

- (void)refreshView;
- (void)rebuildCertHeader;
- (void)rebuildCertContent;

- (void)rebuildDetails;
- (void)rebuildTrustSettings;

- (BOOL)showTrustSettings;

- (IBAction)trustCheckboxClicked:(id)inSender;

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

  mTrustedForWebSites       = [mCertItem trustedForSSLAsType:mCertTrustType];
  mTrustedForEmail          = [mCertItem trustedForEmailAsType:mCertTrustType];
  mTrustedForObjectSigning  = [mCertItem trustedForObjectSigningAsType:mCertTrustType];

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

- (void)setShowTrust:(BOOL)inShowTrust
{
  mForceShowTrust = inShowTrust;
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
  return ((mForceShowTrust || [mCertItem canGetTrust]) &&
           (mCertTrustType == nsIX509Cert::CA_CERT ||
            mCertTrustType == nsIX509Cert::SERVER_CERT ||
            mCertTrustType == nsIX509Cert::EMAIL_CERT));
}

- (IBAction)trustCheckboxClicked:(id)inSender
{
  if (inSender == mTrustForWebSitesCheckbox)
    mTrustedForWebSites = ([inSender state] == NSOnState);

  if (inSender == mTrustForEmailCheckbox)
    mTrustedForEmail = ([inSender state] == NSOnState);

  if (inSender == mTrustForObjSigningCheckbox)
    mTrustedForObjectSigning = ([inSender state] == NSOnState);
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

  BOOL canGetTrust = ([mCertItem isValid] || [mCertItem isUntrustedRootCACert]);
  if (canGetTrust)
  {
    if (mTrustedForWebSites)
      certTrustMask |= nsIX509CertDB::TRUSTED_SSL;

    if (mTrustedForEmail)
      certTrustMask |= nsIX509CertDB::TRUSTED_EMAIL;

    if (mTrustedForObjectSigning)
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
  static BOOL   sGotWidth = NO;
  static float  sLongestLabelWidth = 0.0f;

  if (!sGotWidth)
  {
    NSArray*    labelKeys = [NSArray arrayWithObjects:
                                    @"IssuedTo",
                                    @"OwnerCommonName",
                                    @"OwnerEmailAddress",
                                    @"OwnerOrganization",
                                    @"OwnerOrgUnit",
                                    @"Version",
                                    @"SerialNumber",
                                    @"IssuedBy",
                                    @"IssuerCommonName",
                                    @"IssuerOrganization",
                                    @"IssuerOrgUnit",
                                    @"ShowIssuerCertLabel",
                                    @"Validity",
                                    @"NotBeforeLocalTime",
                                    @"NotAfterLocalTime",
                                    @"SigAlgorithm",
                                    @"Signature",
                                    @"PublicKeyInfo",
                                    @"PublicKeyAlgorithm",
                                    @"PublicKey",
                                    @"UsagesTitle",
                                    @"Usages",
                                    @"Fingerprints",
                                    @"SHA1Fingerprint",
                                    @"MD5Fingerprint",
                                    nil];
  
    float maxWidth = 0.0f;
    NSDictionary* fontAttributes = [NSDictionary dictionaryWithObject:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]
                                                               forKey:NSFontAttributeName];
    NSEnumerator* keysEnum = [labelKeys objectEnumerator];
    NSString* curKey;
    while ((curKey = [keysEnum nextObject]))
    {
      NSString* theLabel = NSLocalizedStringFromTable(curKey, @"CertificateDialogs", @"");
      NSSize stringSize = [theLabel sizeWithAttributes:fontAttributes];
      if (stringSize.width > maxWidth)
        maxWidth = stringSize.width;
    }
    
    if (maxWidth < 120.0f)
      maxWidth = 120.0f;

    sLongestLabelWidth = maxWidth;
    sGotWidth = YES;
  }

  return sLongestLabelWidth;
}

- (CHShrinkWrapView*)containerViewWithTopPadding:(float)inTop bottomPadding:(float)inBottom
{
  // the origin of the rect doesn't matter, and we'll resize the height later
  NSRect headerRect = NSMakeRect(0, 0, NSWidth([self frame]), NSHeight([self frame]));
  CHShrinkWrapView* containerView = [[[CHShrinkWrapView alloc] initWithFrame:headerRect] autorelease];
  [containerView setIntrinsicPadding:kGroupHeaderLeftOffset forEdge:NSMinXEdge];
  [containerView setIntrinsicPadding:inBottom forEdge:NSMinYEdge];
  [containerView setIntrinsicPadding:kGroupHeaderLeftOffset forEdge:NSMaxXEdge];
  [containerView setIntrinsicPadding:inTop forEdge:NSMaxYEdge];
  [containerView setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  return containerView;
}

- (NSTextField*)textFieldWithInitialFrame:(NSRect)inFrame stringValue:(NSString*)inString autoSizing:(BOOL)inAutosize small:(BOOL)useSmallFont bold:(BOOL)useBold
{
  NSTextField* theTextField;
  if (inAutosize)
    theTextField = [[[AutoSizingTextField alloc] initWithFrame:inFrame] autorelease];
  else
    theTextField = [[[NSTextField alloc] initWithFrame:inFrame] autorelease];

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
  [theTextField setStringValue:inString ? inString : @""];
  [[theTextField cell] setWraps:inAutosize];
  [theTextField setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  
  NSSize theCellSize = [[theTextField cell] cellSizeForBounds:inFrame];
  theCellSize.width = NSWidth(inFrame);   // keep the provided width
  [theTextField setFrameSizeMaintainingTopLeftOrigin:theCellSize];

  return theTextField;
}

- (CHStackView *)groupingWithKey:(NSString*)inLabelKey expanded:(BOOL)inExpanded action:(SEL)inAction
{
  NSRect stackFrame = [self bounds];
  
  CHStackView* groupContainer = [[[CHStackView alloc] initWithFrame:stackFrame] autorelease];
  [groupContainer setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  
  NSView* lineContainer = [self containerViewWithTopPadding:kGapAboveGroup bottomPadding:kGapUnderGroupHeader];

  NSRect lineRect = NSMakeRect(kGroupHeaderLeftOffset, 0.0f, NSWidth([self frame]) - 2 * kGroupHeaderLeftOffset, 100.0f);

  NSRect buttonRect = NSMakeRect(0, 0, kDisclosureButtonSize, kDisclosureButtonSize);
  buttonRect.origin = lineRect.origin;
  buttonRect.size = NSMakeSize(kDisclosureButtonSize, kDisclosureButtonSize);
  
  NSButton* theButton = [[[NSButton alloc] initWithFrame:[groupContainer subviewRectFromTopRelativeRect:buttonRect]] autorelease];
  [theButton setBordered:NO];
  [theButton setImage:         inExpanded ? [NSImage imageNamed:@"triangle_down_normal"]  : [NSImage imageNamed:@"triangle_right_normal"]];
  [theButton setAlternateImage:inExpanded ? [NSImage imageNamed:@"triangle_down_pressed"] : [NSImage imageNamed:@"triangle_right_pressed"]];
  [theButton setButtonType:NSMomentaryChangeButton];
  [theButton setAction:inAction];
  [theButton setTarget:self];
  [theButton setAutoresizingMask:NSViewMinYMargin];
  [lineContainer addSubview:theButton];
  
  lineRect.origin.x = NSMaxX(buttonRect) + kDisclosureLabelGap;
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* headerField = [self textFieldWithInitialFrame:[groupContainer subviewRectFromTopRelativeRect:lineRect] stringValue:theLabel autoSizing:NO small:YES bold:YES];
  [lineContainer addSubview:headerField];

  [groupContainer addSubview:lineContainer];
  return groupContainer;
}

- (NSView*)headerWithKey:(NSString*)inLabelKey
{
  NSView* headerContainer = [self containerViewWithTopPadding:kGapAboveHeader bottomPadding:kGapUnderHeader];

  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect headerRect = NSMakeRect(labelOffset, 0.0f, [self labelColumnWidth], 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* headerField = [self textFieldWithInitialFrame:[headerContainer subviewRectFromTopRelativeRect:headerRect] stringValue:theLabel autoSizing:NO small:YES bold:YES];
  [headerField setAlignment:NSRightTextAlignment];
  [headerField setTextColor:[NSColor lightGrayColor]];
  [headerField setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [headerContainer addSubview:headerField];

  NSFont* labelFont = [[headerField cell] font];
  float baselineOffset = [labelFont ascender];
  
  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect boxFrame = NSMakeRect(xPos, baselineOffset, NSWidth([self frame]) - xPos - kLabelLeftOffset, 2.0f);
  
  NSBox* lineBox = [[[NSBox alloc] initWithFrame:[headerContainer subviewRectFromTopRelativeRect:boxFrame]] autorelease];
  [lineBox setBoxType:NSBoxSeparator];
  [lineBox setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [headerContainer addSubview:lineBox];
  
  return headerContainer;
}

- (NSTextField*)addLineLabelWithKey:(NSString*)inLabelKey toView:(NSView*)inLineContainerView
{
  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect labelRect = NSMakeRect(labelOffset, 0.0f, [self labelColumnWidth], 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* labelField = [self textFieldWithInitialFrame:[inLineContainerView subviewRectFromTopRelativeRect:labelRect] stringValue:theLabel autoSizing:NO small:YES bold:NO];
  [labelField setAlignment:NSRightTextAlignment];
  [labelField setTextColor:[NSColor darkGrayColor]];
  [labelField setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [inLineContainerView addSubview:labelField];
  return labelField;
}

- (NSView*)wholeLineLabelWithKey:(NSString*)inLabelKey
{
  NSView* lineContainer = [self containerViewWithTopPadding:0.0f bottomPadding:kGapUnderLine];

  float labelOffset = kGroupHeaderLeftOffset + kDisclosureButtonSize + kDisclosureLabelGap;
  NSRect labelRect = NSMakeRect(labelOffset, 0.0f, NSWidth([self frame]) - labelOffset, 100.0f);
  
  NSString* theLabel = NSLocalizedStringFromTable(inLabelKey, @"CertificateDialogs", @"");
  NSTextField* labelField = [self textFieldWithInitialFrame:[lineContainer subviewRectFromTopRelativeRect:labelRect] stringValue:theLabel autoSizing:YES small:YES bold:NO];
  [lineContainer addSubview:labelField];
  return lineContainer;
}

- (NSView*)lineWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData ignoreBlankLines:(BOOL)inIgnoreBlank
{
  if (inIgnoreBlank && [inData length] == 0)
    return nil;

  NSView* lineContainer = [self containerViewWithTopPadding:0.0f bottomPadding:kGapUnderLine];
  [self addLineLabelWithKey:inLabelKey toView:lineContainer];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect dataRect = NSMakeRect(xPos, 0.0f, NSWidth([self frame]) - xPos - kLabelLeftOffset, 100.0f);
  NSTextField* dataField = [self textFieldWithInitialFrame:[lineContainer subviewRectFromTopRelativeRect:dataRect] stringValue:inData autoSizing:YES small:YES bold:NO];
  [lineContainer addSubview:dataField];
  return lineContainer;
}

- (NSView*)scrollingTextFieldWithLabelKey:(NSString*)inLabelKey data:(NSString*)inData ignoreBlankLines:(BOOL)inIgnoreBlank
{
  if (inIgnoreBlank && [inData length] == 0)
    return nil;
  
  NSView* lineContainer = [self containerViewWithTopPadding:0.0f bottomPadding:kGapUnderLine];
  [self addLineLabelWithKey:inLabelKey toView:lineContainer];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect dataRect = NSMakeRect(xPos, 0.0f, NSWidth([self frame]) - xPos - kLabelLeftOffset, 56.0f);
  dataRect = [lineContainer subviewRectFromTopRelativeRect:dataRect];

  NSScrollView* dataScrollView = [[[NSScrollView alloc] initWithFrame:dataRect] autorelease];
  NSTextView* scrolledTextView = [[[NSTextView alloc] initWithFrame:dataRect] autorelease];
  [dataScrollView setHasVerticalScroller:YES];
  [dataScrollView setBorderType:NSBezelBorder];
  [dataScrollView setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [[dataScrollView verticalScroller] setControlSize:NSSmallControlSize];

  if (inData)
    [[[scrolledTextView textStorage] mutableString] setString:[inData stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
  [scrolledTextView setFont:[NSFont fontWithName:@"Monaco" size:10.0f]];
  [scrolledTextView setEditable:NO];
  [dataScrollView setDocumentView:scrolledTextView];

  [lineContainer addSubview:dataScrollView];
  return lineContainer;
}

- (NSView*)buttonLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction button:(NSButton**)outButton
{
  NSView* lineContainer = [self containerViewWithTopPadding:0.0f bottomPadding:kGapUnderLine];
  [self addLineLabelWithKey:inLabelKey toView:lineContainer];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect buttonRect = NSMakeRect(xPos, 0.0f, NSWidth([self frame]) - xPos - kLabelLeftOffset, 100.0f);
  NSButton* theButton = [[[NSButton alloc] initWithFrame:[lineContainer subviewRectFromTopRelativeRect:buttonRect]] autorelease];

  [theButton setTitle:NSLocalizedStringFromTable(buttonKey, @"CertificateDialogs", @"")];
  [theButton setBezelStyle:NSRoundedBezelStyle];
  [theButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [[theButton cell] setControlSize:NSSmallControlSize];
  //[theButton sizeToFit];
  
  NSSize theCellSize = [[theButton cell] cellSizeForBounds:[theButton frame]];
  [theButton setFrameSizeMaintainingTopLeftOrigin:theCellSize];
  
  [theButton setAutoresizingMask:NSViewMinYMargin];

  [theButton setAction:inAction];
  [theButton setTarget:self];
  [lineContainer addSubview:theButton];

  if (outButton)
    *outButton = theButton;
    
  return lineContainer;
}

- (NSView*)checkboxLineWithLabelKey:(NSString*)inLabelKey buttonLabelKey:(NSString*)buttonKey action:(SEL)inAction button:(NSButton**)outButton
{
  NSView* lineContainer = [self containerViewWithTopPadding:0.0f bottomPadding:kGapUnderCheckboxLine];

  if ([inLabelKey length] > 0)
    [self addLineLabelWithKey:inLabelKey toView:lineContainer];

  float xPos = kLabelLeftOffset + [self labelColumnWidth] + kLabelGutterWidth;
  NSRect buttonRect = NSMakeRect(xPos, 0.0f, NSWidth([self frame]) - xPos - kLabelLeftOffset, 100.0f);
  NSButton* theButton = [[[NSButton alloc] initWithFrame:[lineContainer subviewRectFromTopRelativeRect:buttonRect]] autorelease];

  [theButton setTitle:NSLocalizedStringFromTable(buttonKey, @"CertificateDialogs", @"")];
  [theButton setButtonType:NSSwitchButton];
  [theButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [[theButton cell] setControlSize:NSSmallControlSize];
  // resize maintaining the width
  NSRect wideButtonRect = buttonRect;
  wideButtonRect.size.width = 1000.0f;    // if the width is narrow, -cellSizeForBounds: returns a greater height, which we don't want.
  NSSize theCellSize = [[theButton cell] cellSizeForBounds:wideButtonRect];
  theCellSize.width = NSWidth(buttonRect);
  [theButton setFrameSizeMaintainingTopLeftOrigin:theCellSize];

  [theButton setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  
  [theButton setAction:inAction];
  [theButton setTarget:self];
  [lineContainer addSubview:theButton];

  if (outButton)
    *outButton = theButton;

  return lineContainer;
}

- (void)rebuildDetails
{
  mDetailsItemsView = [self groupingWithKey:@"CertDetailsGroupHeader" expanded:mDetailsExpanded action:@selector(toggleDetails:)];
  [mContentView addSubview:mDetailsItemsView];

  if (!mDetailsExpanded)
    return;

  // 
  // NOTE: add label string keys to the array in -labelColumnWidth if you add new ones.
  // 
  [mDetailsItemsView addSubview:[self headerWithKey:@"IssuedTo"]];

  // Owner stuff
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"OwnerCommonName"    data:[mCertItem commonName]          ignoreBlankLines:YES]];

  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"OwnerEmailAddress"  data:[mCertItem emailAddress]        ignoreBlankLines:YES]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"OwnerOrganization"  data:[mCertItem organization]        ignoreBlankLines:YES]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"OwnerOrgUnit"       data:[mCertItem organizationalUnit]  ignoreBlankLines:YES]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"Version"            data:[mCertItem version]             ignoreBlankLines:YES]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"SerialNumber"       data:[mCertItem serialNumber]        ignoreBlankLines:YES]];

  // Issuer stuff
  [mDetailsItemsView addSubview:[self headerWithKey:@"IssuedBy"]];

  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"IssuerCommonName"   data:[mCertItem issuerCommonName]          ignoreBlankLines:YES]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"IssuerOrganization" data:[mCertItem issuerOrganization]        ignoreBlankLines:YES]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"IssuerOrgUnit"      data:[mCertItem issuerOrganizationalUnit]  ignoreBlankLines:YES]];

  nsCOMPtr<nsIX509Cert> issuerCert;
  nsIX509Cert* thisCert = [mCertItem cert];
  if (thisCert)
    thisCert->GetIssuer(getter_AddRefs(issuerCert));
  if (issuerCert && ![mCertItem isSameCertAs:issuerCert])
  {
    [mDetailsItemsView addSubview:[self buttonLineWithLabelKey:@"ShowIssuerCertLabel"
                                                buttonLabelKey:@"ShowIssuerCertButton"
                                                        action:@selector(showIssuerCert:)
                                                        button:NULL]];
  }

  // validity
  [mDetailsItemsView addSubview:[self headerWithKey:@"Validity"]];

  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"NotBeforeLocalTime" data:[mCertItem longValidFromString]  ignoreBlankLines:NO]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"NotAfterLocalTime"  data:[mCertItem longExpiresString]    ignoreBlankLines:NO]];

  // signature
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"SigAlgorithm"       data:[mCertItem signatureAlgorithm]   ignoreBlankLines:NO]];
  [mDetailsItemsView addSubview:[self scrollingTextFieldWithLabelKey:@"Signature" data:[mCertItem signatureValue]  ignoreBlankLines:NO]];

  // public key info
  [mDetailsItemsView addSubview:[self headerWithKey:@"PublicKeyInfo"]];

  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"PublicKeyAlgorithm" data:[mCertItem publicKeyAlgorithm]   ignoreBlankLines:NO]];
  //[self lineWithLabelKey:@"PublicKeySize"      data:[mCertItem publicKeySizeBits]    ignoreBlankLines:NO];
  [mDetailsItemsView addSubview:[self scrollingTextFieldWithLabelKey:@"PublicKey" data:[mCertItem publicKey]       ignoreBlankLines:NO]];
    
  // usages
  NSArray* certUsages = [mCertItem validUsages];
  if ([certUsages count])
  {
    [mDetailsItemsView addSubview:[self headerWithKey:@"UsagesTitle"]];
    for (unsigned int i = 0; i < [certUsages count]; i ++)
    {
      NSString* labelKey = (i == 0) ? @"Usages" : @"";
      [mDetailsItemsView addSubview:[self lineWithLabelKey:labelKey data:[certUsages objectAtIndex:i] ignoreBlankLines:NO]];
    }
  }

  // fingerprints
  [mDetailsItemsView addSubview:[self headerWithKey:@"Fingerprints"]];

  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"SHA1Fingerprint"  data:[mCertItem sha1Fingerprint] ignoreBlankLines:NO]];
  [mDetailsItemsView addSubview:[self lineWithLabelKey:@"MD5Fingerprint"   data:[mCertItem md5Fingerprint]  ignoreBlankLines:NO]];
}

- (void)rebuildTrustSettings
{
  mTrustItemsView = [self groupingWithKey:@"CertTrustGroupHeader" expanded:mTrustExpanded action:@selector(toggleTrustSettings:)];
  [mContentView addSubview:mTrustItemsView];

  if (!mTrustExpanded)
    return;

  // XXX do we need a more comprehensive check than this?
  BOOL enableCheckboxes = YES;  // [mCertItem isValid] || [mCertItem isUntrustedRootCACert];
  
  // XXX only show relevant checkboxes? (see nsNSSCertificateDB::SetCertTrust)
  // XXX only show checkboxes for allowed usages?
  [mTrustItemsView addSubview:[self wholeLineLabelWithKey:@"TrustSettingsLabel"]];
  [mTrustItemsView addSubview:[self checkboxLineWithLabelKey:@""
                                              buttonLabelKey:@"TrustWebSitesCheckboxLabel"
                                                      action:@selector(trustCheckboxClicked:)
                                                      button:&mTrustForWebSitesCheckbox]];

  [mTrustForWebSitesCheckbox setState:mTrustedForWebSites];
  [mTrustForWebSitesCheckbox setEnabled:enableCheckboxes];
  
  [mTrustItemsView addSubview:[self checkboxLineWithLabelKey:@""
                                              buttonLabelKey:@"TrustEmailUsersCheckboxLabel"
                                                      action:@selector(trustCheckboxClicked:)
                                                      button:&mTrustForEmailCheckbox]];
  [mTrustForEmailCheckbox setState:mTrustedForEmail];
  [mTrustForEmailCheckbox setEnabled:enableCheckboxes];

  [mTrustItemsView addSubview:[self checkboxLineWithLabelKey:@""
                                              buttonLabelKey:@"TrustObjectSignersCheckboxLabel"
                                                      action:@selector(trustCheckboxClicked:)
                                                      button:&mTrustForObjSigningCheckbox]];
  [mTrustForObjSigningCheckbox setState:mTrustedForObjectSigning];
  [mTrustForObjSigningCheckbox setEnabled:enableCheckboxes];
}

- (void)refreshView
{
  [self removeAllSubviews];
  [mContentView release];
  mContentView = nil;

  mTrustForWebSitesCheckbox = nil;
  mTrustForEmailCheckbox = nil;
  mTrustForObjSigningCheckbox = nil;
  
  mContentView = [[CHStackView alloc] initWithFrame:[self bounds]];
  [mContentView setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
  
  [self setIntrinsicPadding:0.0f forEdge:NSMinXEdge];
  [self setIntrinsicPadding:0.0f forEdge:NSMinYEdge];
  [self setIntrinsicPadding:0.0f forEdge:NSMaxXEdge];
  [self setIntrinsicPadding:0.0f forEdge:NSMaxYEdge];
  
  [self addSubview:mContentView];

  [self rebuildCertHeader];
  [self rebuildCertContent];
}

- (void)rebuildCertHeader
{
  // We do all this laborious manual view construction to avoid having to load a nib file each
  // time. This way, we can just have a single custom view in the nib, and do everything in code.
  
  // container for the header
  float headerFieldsLeftEdge = kGroupHeaderLeftOffset + kCertImageViewSize + kGroupHeaderLeftOffset;
  float headerFieldYOffset   = kGroupHeaderLeftOffset;
  float headerFieldWith      = NSWidth([self frame]) - headerFieldYOffset - kCertHeaderFieldRightGap;

  NSView* headerContainer = [self containerViewWithTopPadding:0.0f bottomPadding:kGapUnderGroup];
  
  // cert image
  NSRect certImageFrame = NSMakeRect(0.0f, 0.0f, kCertImageViewSize, kCertImageViewSize);
  NSImageView* certImageView = [[[NSImageView alloc] initWithFrame:[headerContainer subviewRectFromTopRelativeRect:certImageFrame]] autorelease];
  [certImageView setImage:[NSImage imageNamed:@"certificate"]];
  [certImageView setAutoresizingMask:NSViewMinYMargin];
  [headerContainer addSubview:certImageView];

  // description
  NSRect decriptionRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, headerFieldWith, 100.0f);
  NSTextField* descField = [self textFieldWithInitialFrame:[headerContainer subviewRectFromTopRelativeRect:decriptionRect] stringValue:[mCertItem displayName] autoSizing:NO small:NO bold:NO];
  [headerContainer addSubview:descField];
  headerFieldYOffset += NSHeight([descField frame]) + kCertHeaderFieldVerticalGap;

  // issuer info
  NSRect issuerRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, headerFieldWith, 100.0f);
  NSString* formatString = NSLocalizedStringFromTable(@"IssuedByHeaderFormat", @"CertificateDialogs", @"");
  NSString* issuerString = [NSString stringWithFormat:formatString, [mCertItem issuerCommonName]];
  NSTextField* issuerField = [self textFieldWithInitialFrame:[headerContainer subviewRectFromTopRelativeRect:issuerRect] stringValue:issuerString autoSizing:NO small:YES bold:NO];
  [headerContainer addSubview:issuerField];
  headerFieldYOffset += NSHeight([issuerField frame]) + kCertHeaderFieldVerticalGap;

  // expiry info
  NSRect expiryRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, headerFieldWith, 100.0f);
  formatString = NSLocalizedStringFromTable(@"ExpiresHeaderFormat", @"CertificateDialogs", @"");
  NSString* expiryString = [NSString stringWithFormat:formatString, [mCertItem longExpiresString]];
  NSTextField* expiryField = [self textFieldWithInitialFrame:[headerContainer subviewRectFromTopRelativeRect:expiryRect] stringValue:expiryString autoSizing:NO small:YES bold:NO];
  [headerContainer addSubview:expiryField];
  headerFieldYOffset += NSHeight([expiryField frame]) + kCertHeaderFieldVerticalGap;

  NSRect statusImageRect = NSMakeRect(headerFieldsLeftEdge, headerFieldYOffset, kStatusImageSize, kStatusImageSize);
  NSImageView* statusImageView = [[[NSImageView alloc] initWithFrame:[headerContainer subviewRectFromTopRelativeRect:statusImageRect]] autorelease];
  
  NSString* imageName = @"";
  if ([mCertItem isUntrustedRootCACert])
    imageName = @"mini_cert_untrusted";
  else if ([mCertItem isValid])
    imageName = @"mini_cert_valid";
  else
    imageName = @"mini_cert_invalid";
   
  [statusImageView setImage:[NSImage imageNamed:imageName]];
  [statusImageView setAutoresizingMask:NSViewMinYMargin];
  [headerContainer addSubview:statusImageView];

  // status info
  float statusLeftEdge  = headerFieldsLeftEdge + kStatusImageSize + kGroupHeaderLeftOffset;
  float statusFieldWith = NSWidth([self frame]) - statusLeftEdge - kCertHeaderFieldRightGap;
  
  NSRect statusRect = NSMakeRect(statusLeftEdge, headerFieldYOffset, statusFieldWith, 100.0f);
  NSTextField* statusField = [self textFieldWithInitialFrame:[headerContainer subviewRectFromTopRelativeRect:statusRect] stringValue:@"" autoSizing:NO small:YES bold:NO];
  [statusField setAttributedStringValue:[mCertItem attributedLongValidityString]];
  [headerContainer addSubview:statusField];

  [mContentView addSubview:headerContainer];
}

- (void)rebuildCertContent
{
  // blow away the subviews of the content view
  [mTrustItemsView removeFromSuperviewWithoutNeedingDisplay];
  mTrustItemsView = nil;

  [mDetailsItemsView removeFromSuperviewWithoutNeedingDisplay];
  mDetailsItemsView = nil;
  
  if ([self showTrustSettings])
    [self rebuildTrustSettings];

  [self rebuildDetails];
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

