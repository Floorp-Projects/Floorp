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
 *   Simon Fraser <sfraser@netscape.com>
 *   Asaf Romano <mozilla.mano@sent.com>
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
 
#import "Appearance.h"

@interface OrgMozillaChimeraPreferenceAppearance(Private)

- (void)setupFontRegionPopup;
- (void)updateFontPreviews;
- (void)loadFontPrefs;
- (void)saveFontPrefs;

- (NSMutableDictionary *)makeDictFromPrefsForFontType:(NSString*)fontType andRegion:(NSString*)regionCode;
- (NSMutableDictionary *)makeDefaultFontTypeDictFromPrefsForRegion:(NSString*)regionCode;
- (NSMutableDictionary *)makeFontSizesDictFromPrefsForRegion:(NSString*)regionCode;

- (NSMutableDictionary*)settingsForCurrentRegion;
- (NSString*)defaultProportionalFontTypeForCurrentRegion;

- (void)saveFontNamePrefsForRegion:(NSDictionary*)regionDict forFontType:(NSString*)fontType;
- (void)saveFontSizePrefsForRegion:(NSDictionary*)entryDict;
- (void)saveDefaultFontTypePrefForRegion:(NSDictionary*)entryDict;

- (BOOL)useMyFontsPref;
- (void)setUseMyFontsPref:(BOOL)checkboxState;

- (void)setupFontSamplesFromDict:(NSDictionary*)regionDict;
- (void)setupFontSampleOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;

- (NSFont*)getFontOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;

- (void)setFontSampleOfType:(NSString *)fontType withFont:(NSFont*)font andDict:(NSMutableDictionary*)regionDict;
- (void)saveFont:(NSFont*)font toDict:(NSMutableDictionary*)regionDict forType:(NSString*)fontType;

- (void)updateFontSampleOfType:(NSString *)fontType;
- (NSTextField*)getFontSampleForType:(NSString *)fontType;
- (NSString*)getFontSizeType:(NSString*)fontType;

- (void)buildFontPopup:(NSPopUpButton*)popupButton;

- (void)setupFontPopup:(NSPopUpButton*)popupButton forType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;
- (void)getFontFromPopup:(NSPopUpButton*)popupButton forType:(NSString*)fontType intoDict:(NSDictionary*)regionDict;

@end

#pragma mark -

// We use instances of this class as the field editor, to allow us to catch
// |changeFont:| messages.

@interface SampleTextView : NSTextView
{
  id    mPrefPane;
}
- (void)setPrefPane:(id)inPrefPane;

@end

@implementation SampleTextView

- (void)setPrefPane:(id)inPrefPane
{
  mPrefPane = inPrefPane;
}

- (void)changeFont:(id)sender
{
  [mPrefPane changeFont:sender];
}

- (BOOL)fontManager:(id)theFontManager willIncludeFont:(NSString *)fontName
{
  return [mPrefPane fontManager:theFontManager willIncludeFont:fontName];
}

- (unsigned int)validModesForFontPanel:(NSFontPanel *)fontPanel
{
  return [mPrefPane validModesForFontPanel:fontPanel];
}

@end

#pragma mark -

@implementation OrgMozillaChimeraPreferenceAppearance

- (void)dealloc
{
  [mRegionMappingTable release];
  [mPropSampleFieldEditor release];
  [mMonoSampleFieldEditor release];

  [super dealloc];
}

- (id)initWithBundle:(NSBundle *)bundle
{
  self = [super initWithBundle:bundle];
  mFontButtonForEditor = nil;
  return self;
}

- (void)mainViewDidLoad
{
  BOOL gotPref;
  [mUnderlineLinksCheckbox setState:
      [self getBooleanPref:"browser.underline_anchors" withSuccess:&gotPref]];
  [mUseMyColorsCheckbox setState:
      ![self getBooleanPref:"browser.display.use_document_colors" withSuccess:&gotPref]];

  // should save and restore this
  [[NSColorPanel sharedColorPanel] setContinuous:NO];
  
  [mBackgroundColorWell     setColor:[self getColorPref:"browser.display.background_color"  withSuccess:&gotPref]];
  [mTextColorWell           setColor:[self getColorPref:"browser.display.foreground_color"  withSuccess:&gotPref]];
  [mUnvisitedLinksColorWell setColor:[self getColorPref:"browser.anchor_color"              withSuccess:&gotPref]];
  [mVisitedLinksColorWell   setColor:[self getColorPref:"browser.visited_color"             withSuccess:&gotPref]];
  
  [mUseMyFontsCheckbox setState:[self useMyFontsPref]];

  [self setupFontRegionPopup];
  [self updateFontPreviews];
}

- (void)willUnselect
{
  // time to save stuff
  [self saveFontPrefs];
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
}

- (void)setupFontRegionPopup
{
  NSBundle* prefBundle = [NSBundle bundleForClass:[self class]];
  NSString* resPath = [prefBundle pathForResource:@"RegionMapping" ofType:@"plist"];

  // we use the dictionaries in this array as temporary storage of font
  // values until the pane is unloaded, at which time they are saved
  mRegionMappingTable = [[NSArray arrayWithContentsOfFile:resPath] retain];

  [self loadFontPrefs];

  [mFontRegionPopup removeAllItems];
  for (unsigned int i = 0; i < [mRegionMappingTable count]; i++) {
    NSDictionary* regionDict = [mRegionMappingTable objectAtIndex:i];
    [mFontRegionPopup addItemWithTitle:[regionDict objectForKey:@"region"]];
  }
}

- (IBAction)buttonClicked:(id)sender
{
  if (sender == mUnderlineLinksCheckbox)
    [self setPref:"browser.underline_anchors" toBoolean:[sender state]];
  else if (sender == mUseMyColorsCheckbox)
    [self setPref:"browser.display.use_document_colors" toBoolean:![sender state]];
}

- (IBAction)colorChanged:(id)sender
{
  const char* prefName = NULL;
  
  if (sender == mBackgroundColorWell)
  	prefName = "browser.display.background_color";
  else if (sender == mTextColorWell)
  	prefName = "browser.display.foreground_color";
  else if (sender == mUnvisitedLinksColorWell)
  	prefName = "browser.anchor_color";
  else if (sender == mVisitedLinksColorWell)
  	prefName = "browser.visited_color";

  if (prefName)
    [self setPref:prefName toColor:[sender color]];
}

- (IBAction)proportionalFontChoiceButtonClicked:(id)sender
{
  NSFontManager *fontManager = [NSFontManager sharedFontManager];

  NSString* defaultFontType = [self defaultProportionalFontTypeForCurrentRegion];

  NSFont *newFont = [[self getFontSampleForType:defaultFontType] font];
  mFontButtonForEditor = mChooseProportionalFontButton;
  [fontManager setSelectedFont:newFont isMultiple:NO];
  [[fontManager fontPanel:YES] makeKeyAndOrderFront:self];
}

- (IBAction)monospaceFontChoiceButtonClicked:(id)sender
{
  NSFontManager *fontManager = [NSFontManager sharedFontManager];
  NSFont *newFont = [[self getFontSampleForType:@"monospace"] font];
  mFontButtonForEditor = mChooseMonospaceFontButton;
  [fontManager setSelectedFont:newFont isMultiple:NO];
  [[fontManager fontPanel:YES] makeKeyAndOrderFront:self];
}

- (IBAction)useMyFontsButtonClicked:(id)sender
{
  [self setUseMyFontsPref:[sender state]];
}

- (IBAction)fontRegionPopupClicked:(id)sender
{
  // save the old values
  [self updateFontPreviews];
}

- (void)loadFontPrefs
{
  for (unsigned int i = 0; i < [mRegionMappingTable count]; i++) {
    NSMutableDictionary	*regionDict = [mRegionMappingTable objectAtIndex:i];
    NSString *regionCode = [regionDict objectForKey:@"code"];
    
    /*
      For each region in the array, there is a dictionary of 
        {
          code   =
          region = (from localized strings file)

      to which we add here a sub-dictionary per font type, thus:
        
          serif = {
              fontfamily = Times-Regular
            }
          sans-serif = {
              fontfamily =
              missing  = 		// set if a font is missing
            }
          cursive =   {
              fontfamily =
            }
          monospace =   {
              fontfamily =
            }

      an entry that stores the default font type:

          defaultFontType = {
              type =
            }

      and an entry that stores font sizes:
          
          fontsize = {
            variable = 
            fixed = 
            minimum =   // optional
          }
      
        }
    */
    NSString *regionName = NSLocalizedStringFromTableInBundle(regionCode, @"RegionNames",
        [NSBundle bundleForClass:[OrgMozillaChimeraPreferenceAppearance class]], @"");
    [regionDict setObject:regionName forKey:@"region"];
    
    NSMutableDictionary *serifDict = [self makeDictFromPrefsForFontType:@"serif" andRegion:regionCode];
    [regionDict setObject:serifDict forKey:@"serif"];

    NSMutableDictionary	*sanssDict = [self makeDictFromPrefsForFontType:@"sans-serif" andRegion:regionCode];
    [regionDict setObject:sanssDict forKey:@"sans-serif"];

    NSMutableDictionary *monoDict = [self makeDictFromPrefsForFontType:@"monospace" andRegion:regionCode];
    [regionDict setObject:monoDict forKey:@"monospace"];

    NSMutableDictionary *cursDict = [self makeDictFromPrefsForFontType:@"cursive" andRegion:regionCode];
    [regionDict setObject:cursDict forKey:@"cursive"];

    NSMutableDictionary *fantasyDict = [self makeDictFromPrefsForFontType:@"fantasy" andRegion:regionCode];
    [regionDict setObject:fantasyDict forKey:@"fantasy"];

    // font sizes dict
    NSMutableDictionary *sizesDict = [self makeFontSizesDictFromPrefsForRegion:regionCode];
    [regionDict setObject:sizesDict forKey:@"fontsize"];

    // default font type dict
    NSMutableDictionary *defaultFontTypeDict = [self makeDefaultFontTypeDictFromPrefsForRegion:regionCode];
    [regionDict setObject:defaultFontTypeDict forKey:@"defaultFontType"];
  }
}

- (void)saveFontPrefs
{
  if (!mRegionMappingTable)
    return;

  for (unsigned int i = 0; i < [mRegionMappingTable count]; i++) {
    NSMutableDictionary	*regionDict = [mRegionMappingTable objectAtIndex:i];

    [self saveFontNamePrefsForRegion:regionDict forFontType:@"serif"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"sans-serif"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"monospace"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"cursive"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"fantasy"];
    
    [self saveDefaultFontTypePrefForRegion:regionDict];
    [self saveFontSizePrefsForRegion:regionDict];
  }
}

#pragma mark -

- (NSMutableDictionary *)makeDictFromPrefsForFontType:(NSString*)fontType andRegion:(NSString*)regionCode
{
  NSMutableDictionary *fontDict = [NSMutableDictionary dictionaryWithCapacity:1];
  NSString *fontPrefName = [NSString stringWithFormat:@"font.name.%@.%@", fontType, regionCode];

  BOOL gotPref;
  NSString *fontName = [self getStringPref:[fontPrefName cString] withSuccess:&gotPref];

  if (gotPref)
    [fontDict setObject:fontName forKey:@"fontfamily"];

  return fontDict;
}

- (NSMutableDictionary *)makeDefaultFontTypeDictFromPrefsForRegion:(NSString*)regionCode
{
  NSMutableDictionary *fontTypeDict = [NSMutableDictionary dictionaryWithCapacity:1];
  NSString *prefName = [NSString stringWithFormat:@"font.default.%@", regionCode];

  BOOL gotPref;
  NSString *fontType = [self getStringPref:[prefName cString] withSuccess:&gotPref];

  if (gotPref)
    [fontTypeDict setObject:fontType forKey:@"type"];

  return fontTypeDict;
}

- (NSMutableDictionary *)makeFontSizesDictFromPrefsForRegion:(NSString*)regionCode
{
  NSMutableDictionary *fontDict = [NSMutableDictionary dictionaryWithCapacity:2];

  NSString *variableSizePref = [NSString stringWithFormat:@"font.size.variable.%@", regionCode];
  NSString *fixedSizePref = [NSString stringWithFormat:@"font.size.fixed.%@", regionCode];
  NSString *minSizePref = [NSString stringWithFormat:@"font.minimum-size.%@", regionCode];

  BOOL gotFixed, gotVariable, gotMinSize;
  int variableSize = [self getIntPref:[variableSizePref cString] withSuccess:&gotVariable];
  int fixedSize = [self getIntPref:[fixedSizePref cString] withSuccess:&gotFixed];
  int minSize = [self getIntPref:[minSizePref cString] withSuccess:&gotMinSize];

  if (gotVariable)
    [fontDict setObject:[NSNumber numberWithInt:variableSize] forKey:@"variable"];
  
  if (gotFixed)
    [fontDict setObject:[NSNumber numberWithInt:fixedSize] forKey:@"fixed"];

  if (gotMinSize)
    [fontDict setObject:[NSNumber numberWithInt:minSize] forKey:@"minimum"];
    
  return fontDict;
}

- (NSMutableDictionary*)settingsForCurrentRegion
{
  int selectedRegion = [mFontRegionPopup indexOfSelectedItem];
  if (selectedRegion == -1)
    return nil;

  return [mRegionMappingTable objectAtIndex:selectedRegion];
}

- (NSString*)defaultProportionalFontTypeForCurrentRegion
{
  NSDictionary* regionDict = [self settingsForCurrentRegion];
  return [[regionDict objectForKey:@"defaultFontType"] objectForKey:@"type"];
}

- (void)saveFontNamePrefsForRegion:(NSDictionary*)regionDict forFontType:(NSString*)fontType
{
  NSString *regionCode = [regionDict objectForKey:@"code"];

  if (!regionDict || !fontType || !regionCode) return;

  NSDictionary* fontTypeDict = [regionDict objectForKey:fontType];
  
  NSString *fontName = [fontTypeDict objectForKey:@"fontfamily"];
  NSString *fontPrefName = [NSString stringWithFormat:@"font.name.%@.%@", fontType, regionCode];

  if (fontName)
    [self setPref:[fontPrefName cString] toString:fontName];
  else
    // If the preferences were reset to defaults, this key could be gone.
    [self clearPref:[fontPrefName cString]];
}

- (void)saveFontSizePrefsForRegion:(NSDictionary*)regionDict
{
  NSString *regionCode = [regionDict objectForKey:@"code"];
  
  NSString *variableSizePref = [NSString stringWithFormat:@"font.size.variable.%@", regionCode];
  NSString *fixedSizePref = [NSString stringWithFormat:@"font.size.fixed.%@", regionCode];
  NSString *minSizePref = [NSString stringWithFormat:@"font.minimum-size.%@", regionCode];

  NSDictionary* fontSizeDict = [regionDict objectForKey:@"fontsize"];

  int variableSize = [[fontSizeDict objectForKey:@"variable"] intValue];
  int fixedSize = [[fontSizeDict objectForKey:@"fixed"] intValue];
  int minSize = [[fontSizeDict objectForKey:@"minimum"] intValue];

  // If the preferences were reset to defaults, these keys could be gone.
  if (variableSize)
    [self setPref:[variableSizePref cString] toInt:variableSize];
  else
    [self clearPref:[variableSizePref cString]];

  if (fixedSize)
    [self setPref:[fixedSizePref cString] toInt:fixedSize];
  else
    [self clearPref:[fixedSizePref cString]];

  if (minSize)
    [self setPref:[minSizePref cString] toInt:minSize];
  else
    [self clearPref:[minSizePref cString]];
}

- (void)saveDefaultFontTypePrefForRegion:(NSDictionary*)regionDict
{
  NSString *regionCode = [regionDict objectForKey:@"code"];
  
  NSString *prefName = [NSString stringWithFormat:@"font.default.%@", regionCode];
  NSString *value = [[regionDict objectForKey:@"defaultFontType"] objectForKey:@"type"];

  // If the preferences were reset to defaults, this key could be gone.
  if (value)
    [self setPref:[prefName cString] toString:value];
  else
    [self clearPref:[prefName cString]];
}

// The exposed "Use My Fonts" pref has reverse logic from the internal pref 
// (when mUseMyFontsCheckbox == 1, use_document_fonts == 0).  These private accessor methods allow code
// elsewhere to use the exposed pref's logic
- (BOOL)useMyFontsPref
{
  BOOL gotPref;
  return [self getIntPref:"browser.display.use_document_fonts" withSuccess:&gotPref] == 0;
}

- (void)setUseMyFontsPref:(BOOL)checkboxState
{
  int useDocumentFonts = checkboxState ? 0 : 1;
  [self setPref:"browser.display.use_document_fonts" toInt:useDocumentFonts];
}

#pragma mark -

- (void)saveFont:(NSFont*)font toDict:(NSMutableDictionary*)regionDict forType:(NSString*)fontType
{
  NSMutableDictionary *fontTypeDict = [regionDict objectForKey:fontType];
  NSMutableDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
  
  if (font)
  {
    // clear any missing flag
    [fontTypeDict removeObjectForKey:@"missing"];
    
    [fontTypeDict setObject:[font familyName] forKey:@"fontfamily"];
    [fontSizeDict setObject:[NSNumber numberWithInt:(int)[font pointSize]] forKey:[self getFontSizeType:fontType]];
  }
}
 
- (NSFont*)getFontOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict
{
  NSDictionary	*fontTypeDict = [regionDict objectForKey:fontType];
  NSDictionary	*fontSizeDict = [regionDict objectForKey:@"fontsize"];

  NSString *fontName = [fontTypeDict objectForKey:@"fontfamily"];
  int fontSize = [[fontSizeDict objectForKey:[self getFontSizeType:fontType]] intValue];
  
  NSFont *returnFont = nil;
  
  if (fontName && fontSize > 0) {
    // we can't use [NSFont fontWithName] here, because we only store font
    // family names in the prefs file. So use the font manager instead
    // returnFont = [NSFont fontWithName:fontName size:fontSize];
    returnFont = [[NSFontManager sharedFontManager] fontWithFamily:fontName traits:0 weight:5 size:fontSize];
  } else if (fontName) {
    // no size
    returnFont = [[NSFontManager sharedFontManager] fontWithFamily:fontName traits:0 weight:5 size:16.0];
  }
  
  // if still no font, get defaults
  if (fontName == nil && returnFont == nil)
    returnFont = ([fontType isEqualToString:@"monospace"]) ?
              [NSFont userFixedPitchFontOfSize:14.0] :
              [NSFont userFontOfSize:16.0];

  // we return nil if the font was not found
  return returnFont;
}

- (void)setupFontSamplesFromDict:(NSDictionary*)regionDict
{
  // For proportional, show either serif or sans-serif font,
  // per region's default-font-type (font.default.%regionCode% pref)
  NSString *defaultFontType = [[regionDict objectForKey:@"defaultFontType"] objectForKey:@"type"];
  [self setupFontSampleOfType:defaultFontType fromDict:regionDict];
  [self setupFontSampleOfType:@"monospace" fromDict:regionDict];
}

- (void)setupFontSampleOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict
{
  NSFont *foundFont = [self getFontOfType:fontType fromDict:regionDict];
  [self setFontSampleOfType:fontType withFont:foundFont andDict:regionDict];
}

- (void)setFontSampleOfType:(NSString *)fontType withFont:(NSFont*)font andDict:(NSMutableDictionary*)regionDict
{
  // font may be nil here, in which case the font is missing, and we construct
  // a string to display from the dict.
  NSMutableDictionary *fontTypeDict = [regionDict objectForKey:fontType];

  NSTextField *sampleCell = [self getFontSampleForType:fontType];
  NSString *displayString = nil;
  
  if (font == nil)
  {
    if (regionDict)
    {
      NSDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
      NSString *fontName = [fontTypeDict objectForKey:@"fontfamily"];
      int fontSize = [[fontSizeDict objectForKey:[self getFontSizeType:fontType]] intValue];

      displayString = [NSString stringWithFormat:@"%@, %dpt %@", fontName, fontSize, [self getLocalizedString:@"Missing"]];
      font = [NSFont userFontOfSize:14.0];

      // set the missing flag in the dict
      if (![fontTypeDict objectForKey:@"missing"] || ![[fontTypeDict objectForKey:@"missing"] boolValue])
        [fontTypeDict setObject:[NSNumber numberWithBool:YES] forKey:@"missing"];
    }
    else
    {
      // should never happen
      // XXX localize
      displayString = @"Font missing";
      font = [NSFont userFontOfSize:16.0];
    }
  }
  else
  {
    displayString = [NSString stringWithFormat:@"%@, %dpt", [font familyName], (int)[font pointSize]];
    
    // make sure we don't have a missing entry
    [fontTypeDict removeObjectForKey:@"missing"];
  }
  
  // Set the font of the sample to a font that is not bold, italic etc.
  NSFont* baseFont = [[NSFontManager sharedFontManager] fontWithFamily:[font familyName] traits:0 weight:5 /* normal weight */ size:[font pointSize]];
  
  [sampleCell setFont:baseFont];
  [sampleCell setStringValue:displayString];
}

- (void)updateFontSampleOfType:(NSString *)fontType
{
  NSMutableDictionary* regionDict = [self settingsForCurrentRegion];
  if (!regionDict) return;

  NSTextField *sampleCell = [self getFontSampleForType:fontType];
  NSFont *sampleFont = [[NSFontManager sharedFontManager] convertFont:[sampleCell font]];

  // save the font in the dictionaries
  [self saveFont:sampleFont toDict:regionDict forType:fontType];
  // and update the sample
  [self setFontSampleOfType:fontType withFont:sampleFont andDict:regionDict];
}

- (NSTextField*)getFontSampleForType:(NSString *)fontType
{
  if ([fontType isEqualToString:@"serif"] || [fontType isEqualToString:@"sans-serif"])
    return mFontSampleProportional;

  if ([fontType isEqualToString:@"monospace"])
    return mFontSampleMonospace;

  return nil;
}

- (NSString*)getFontSizeType:(NSString*)fontType
{
  if ([fontType isEqualToString:@"monospace"])
    return @"fixed";

  return @"variable";
}

- (void)updateFontPreviews
{
  NSDictionary* regionDict = [self settingsForCurrentRegion];
  if (!regionDict) return;

  NSString* defaultFontType = [self defaultProportionalFontTypeForCurrentRegion];
  // make sure the 'proportional' label matches
  NSString* propLabelString = [NSString stringWithFormat:[self getLocalizedString:@"ProportionalLabelFormat"], [self getLocalizedString:defaultFontType]];
  [mProportionalSampleLabel setStringValue:propLabelString];
  
  NSString* sublabelValue = [self getLocalizedString:[defaultFontType stringByAppendingString:@"_note"]];
  [mProportionalSubLabel setStringValue:sublabelValue];
  
  [self setupFontSamplesFromDict:regionDict];
}

#pragma mark -

const int kDefaultFontSerifTag = 0;
const int kDefaultFontSansSerifTag = 1;

- (IBAction)showAdvancedFontsDialog:(id)sender
{
  NSDictionary* regionDict = [self settingsForCurrentRegion];
  if (!regionDict) return;

  NSString* advancedLabel = [NSString stringWithFormat:[self getLocalizedString:@"AdditionalFontsLabelFormat"], [regionDict objectForKey:@"region"]];
  [mAdvancedFontsLabel setStringValue:advancedLabel];
  
  // set up the dialog for the current region
  [self setupFontPopup:mSerifFontPopup forType:@"serif" fromDict:regionDict];
  [self setupFontPopup:mSansSerifFontPopup forType:@"sans-serif" fromDict:regionDict];
  [self setupFontPopup:mCursiveFontPopup forType:@"cursive" fromDict:regionDict];
  [self setupFontPopup:mFantasyFontPopup forType:@"fantasy" fromDict:regionDict];
  
  // setup min size popup
  int itemIndex = 0;
  NSMutableDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
  NSNumber* minSize = [fontSizeDict objectForKey:@"minimum"];
  if (minSize) {
    itemIndex = [mMinFontSizePopup indexOfItemWithTag:[minSize intValue]];
    if (itemIndex == -1)
      itemIndex = 0;
  }
  [mMinFontSizePopup selectItemAtIndex:itemIndex];
  
  // set up default font radio buttons (default to serif)
  NSString* defaultFontType = [self defaultProportionalFontTypeForCurrentRegion];
  [mDefaultFontMatrix selectCellWithTag:([defaultFontType isEqualToString:@"sans-serif"] ? kDefaultFontSansSerifTag : kDefaultFontSerifTag)];
  
  mFontPanelWasVisible = [[NSFontPanel sharedFontPanel] isVisible];
  [[NSFontPanel sharedFontPanel] orderOut:nil];
  
	[NSApp beginSheet:mAdvancedFontsDialog
        modalForWindow:[mTabView window] // any old window accessor
        modalDelegate:self
        didEndSelector:@selector(advancedFontsSheetDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
}

- (IBAction)advancedFontsDone:(id)sender
{
  // save settings
  NSDictionary* regionDict = [self settingsForCurrentRegion];
  if (!regionDict) return;

  [self getFontFromPopup:mSerifFontPopup forType:@"serif" intoDict:regionDict];
  [self getFontFromPopup:mSansSerifFontPopup forType:@"sans-serif" intoDict:regionDict];
  [self getFontFromPopup:mCursiveFontPopup forType:@"cursive" intoDict:regionDict];
  [self getFontFromPopup:mFantasyFontPopup forType:@"fantasy" intoDict:regionDict];

  int minSize = [[mMinFontSizePopup selectedItem] tag];
  // a value of 0 indicates 'none'; we'll clear the pref on save
  NSMutableDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
  [fontSizeDict setObject:[NSNumber numberWithInt:(int)minSize] forKey:@"minimum"];

  // save the default font type
  NSMutableDictionary* defaultFontTypeDict = [regionDict objectForKey:@"defaultFontType"];
  NSString* defaultFontType = ([[mDefaultFontMatrix selectedCell] tag] == kDefaultFontSerifTag) ? @"serif" : @"sans-serif";
  [defaultFontTypeDict setObject:defaultFontType forKey:@"type"];
  
  [mAdvancedFontsDialog orderOut:self];
  [NSApp endSheet:mAdvancedFontsDialog];

  [self updateFontPreviews];

  if (mFontPanelWasVisible)
     [[NSFontPanel sharedFontPanel] makeKeyAndOrderFront:self];    
}

// Reset the "Colors and Links" tab to application factory defaults.
- (IBAction)resetColorsToDefaults:(id)sender
{
  [self clearPref:"browser.underline_anchors"];
  [self clearPref:"browser.display.use_document_colors"];
  [self clearPref:"browser.display.background_color"];
  [self clearPref:"browser.display.foreground_color"];
  [self clearPref:"browser.anchor_color"];
  [self clearPref:"browser.visited_color"];
  
  // update the UI of the Appearance pane
  int state;
  state = [self getBooleanPref:"browser.underline_anchors" withSuccess:NULL] ? NSOnState : NSOffState;
  [mUnderlineLinksCheckbox setState:state];
  state = [self getBooleanPref:"browser.display.use_document_colors" withSuccess:NULL] ? NSOffState : NSOnState;
  [mUseMyColorsCheckbox setState:state];
  
  [mBackgroundColorWell setColor:[self getColorPref:"browser.display.background_color" withSuccess:NULL]];
  [mTextColorWell setColor:[self getColorPref:"browser.display.foreground_color" withSuccess:NULL]];
  [mUnvisitedLinksColorWell setColor:[self getColorPref:"browser.anchor_color" withSuccess:NULL]];
  [mVisitedLinksColorWell setColor:[self getColorPref:"browser.visited_color" withSuccess:NULL]];
}

// Reset the Fonts tab to application factory defaults.
- (IBAction)resetFontsToDefaults:(id)sender
{
  // clear all the preferences for the font regions
  for (unsigned int i = 0; i < [mRegionMappingTable count]; i++)
  {
    NSMutableDictionary* regionDict = [mRegionMappingTable objectAtIndex:i];
    // NSString* regionCode = [regionDict objectForKey:@"code"];
    
    // for each region, we reset the dictionaries to be empty.
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"serif"];
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"sans-serif"];
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"monospace"];
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"cursive"];
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"fantasy"];
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"fontsize"];
    [regionDict setObject:[NSMutableDictionary dictionary] forKey:@"defaultFontType"];
  }
  
  [self clearPref:"browser.display.use_document_fonts"];

  // commit the changes
  // first, we clear the preferences by saving them. This clears the values that are returned by
  // -[self getPref:].
  [self saveFontPrefs];
  // Since the rest of the Appearance code doesn't access the prefs by getPref directly,
  // we need to re-store the default preference values in the region dicts.
  [self loadFontPrefs];
  
  // Update the UI of the Appearance pane
  // order is important here -- syncing the font panel depends on the font previews being correct.
  [self updateFontPreviews];
  [mUseMyFontsCheckbox setState:[self useMyFontsPref]];
}

- (void)advancedFontsSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
}

const int kMissingFontPopupItemTag = 9999;

- (void)setupFontPopup:(NSPopUpButton*)popupButton forType:(NSString*)fontType fromDict:(NSDictionary*)regionDict
{
  NSDictionary* fontTypeDict = [regionDict objectForKey:fontType];
  NSString*     defaultValue = [fontTypeDict objectForKey:@"fontfamily"];
  
  [self buildFontPopup:popupButton];

  // check to see if the font exists
  NSFont *foundFont = nil;
  if (defaultValue)
    foundFont = [[NSFontManager sharedFontManager] fontWithFamily:defaultValue traits:0 weight:5 size:16.0];
  else {
    foundFont = [fontType isEqualToString:@"monospace"]
                      ? [NSFont userFixedPitchFontOfSize:16.0]
                      : [NSFont userFontOfSize:16.0];
    defaultValue = [foundFont familyName];
  }
  
  if (!foundFont) {
    NSMenuItem* missingFontItem = [[popupButton menu] itemWithTag:kMissingFontPopupItemTag];
    if (!missingFontItem) {
      missingFontItem = [[[NSMenuItem alloc] initWithTitle:@"temp" action:NULL keyEquivalent:@""] autorelease];
      [missingFontItem setTag:kMissingFontPopupItemTag];
      [[popupButton menu] addItem:missingFontItem];
    }

    NSString* itemTitle = [NSString stringWithFormat:@"%@ %@", defaultValue, [self getLocalizedString:@"Missing"]];
    [missingFontItem setTitle:itemTitle];
    [popupButton selectItem:missingFontItem];
  } else {
    // remove the missing item if it exists
    NSMenuItem* missingFontItem = [[popupButton menu] itemWithTag:kMissingFontPopupItemTag];
    if (missingFontItem)
      [[popupButton menu] removeItem: missingFontItem];

    [popupButton selectItemWithTitle:defaultValue];
  }
}

- (void)getFontFromPopup:(NSPopUpButton*)popupButton forType:(NSString*)fontType intoDict:(NSDictionary*)regionDict
{
  NSMenuItem* selectedItem = [popupButton selectedItem];
  if ([selectedItem tag] != kMissingFontPopupItemTag)
    [[regionDict objectForKey:fontType] setObject:[selectedItem title] forKey:@"fontfamily"];
}

- (void)buildFontPopup:(NSPopUpButton*)popupButton
{
  NSMenu* menu = [popupButton menu];

  [menu setAutoenablesItems:NO];

  // remove existing items
  while ([menu numberOfItems] > 0)
    [menu removeItemAtIndex:0];

  NSArray*	fontList = [[NSFontManager sharedFontManager] availableFontFamilies];
  NSArray*  sortedFontList = [fontList sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
  
  for (unsigned int i = 0; i < [sortedFontList count]; i ++) {
    NSString* fontFamilyName = [sortedFontList objectAtIndex:i];
    unichar firstChar = [fontFamilyName characterAtIndex:0];
    
    if (firstChar == unichar('.') || firstChar == unichar('#'))
      continue; // skip fonts with ugly names
    
    NSString* uiFamilyName = [[NSFontManager sharedFontManager] localizedNameForFamily:fontFamilyName face:nil];
    NSMenuItem* newItem = [[NSMenuItem alloc] initWithTitle:uiFamilyName action:nil keyEquivalent:@""];

#if SUBMENUS_FOR_VARIANTS
    NSArray* fontFamilyMembers = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:fontFamilyName];
    if ([fontFamilyMembers count] > 1) {
      NSMenu*  familySubmenu = [[NSMenu alloc] initWithTitle:fontFamilyName];
      [familySubmenu setAutoenablesItems:NO];
      
      for (unsigned int j = 0; j < [fontFamilyMembers count]; j ++) {
        NSArray* fontFamilyItems = [fontFamilyMembers objectAtIndex:j];
        NSString* fontItemName = [fontFamilyItems objectAtIndex:1];

        NSMenuItem* newSubmenuItem = [[NSMenuItem alloc] initWithTitle:fontItemName action:nil keyEquivalent:@""];
        [familySubmenu addItem:newSubmenuItem];
      }
      
      [newItem setSubmenu:familySubmenu];
    } else {
      // use the name from the font family info?
    }
#endif

    [menu addItem:newItem];
  }
}

@end

@implementation OrgMozillaChimeraPreferenceAppearance (FontManagerDelegate)

- (id)fieldEditorForObject:(id)inObject
{
  if (inObject == mFontSampleProportional)
  {
    if (!mPropSampleFieldEditor)
    {
      SampleTextView* propFieldEditor = [[SampleTextView alloc] initWithFrame:[inObject bounds]];
      [propFieldEditor setPrefPane:self];
      [propFieldEditor setFieldEditor:YES];
      [propFieldEditor setDelegate:inObject];
      mPropSampleFieldEditor = propFieldEditor;
    }
      
    return mPropSampleFieldEditor;
  }
  else if (inObject == mFontSampleMonospace)
  {
    if (!mMonoSampleFieldEditor)
    {
      SampleTextView* monoFieldEditor = [[SampleTextView alloc] initWithFrame:[inObject bounds]];
      [monoFieldEditor setPrefPane:self];
      [monoFieldEditor setFieldEditor:YES];
      [monoFieldEditor setDelegate:inObject];
      mMonoSampleFieldEditor = monoFieldEditor;
    }
    
    return mMonoSampleFieldEditor;
  }
  
  return nil;
}

- (void)changeFont:(id)sender
{
  // ignore font panel changes if the advanced panel is up
  if ([mAdvancedFontsDialog isVisible])
    return;
  
  if (mFontButtonForEditor == mChooseProportionalFontButton)
  {
    NSString* fontType = [self defaultProportionalFontTypeForCurrentRegion];
    [self updateFontSampleOfType:fontType];
  }
  else if (mFontButtonForEditor == mChooseMonospaceFontButton)
  {
    [self updateFontSampleOfType:@"monospace"];
  }
  
}

- (BOOL)fontManager:(id)theFontManager willIncludeFont:(NSString *)fontName
{
  // filter out fonts for the selected language
  //NSLog(@"willIncludeFont:%@", fontName);
  return YES;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_3

enum {
    NSFontPanelFaceModeMask = 1 << 0,
    NSFontPanelSizeModeMask = 1 << 1,
    NSFontPanelCollectionModeMask = 1 << 2,
    NSFontPanelStandardModesMask = 0xFFFF,
    NSFontPanelAllModesMask = 0xFFFFFFFF
};

#endif

// this allows us to hide the font face panel
- (unsigned int)validModesForFontPanel:(NSFontPanel *)fontPanel
{
  // hide the face panel
  return (NSFontPanelStandardModesMask & ~NSFontPanelFaceModeMask);
}

@end
