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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Simon Fraser <sfraser@netscape.com>
 *
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
 
#import "Appearance.h"

@interface OrgMozillaChimeraPreferenceAppearance(Private)

- (void)setupFontRegionPopup;
- (void)updateFontPreviews;
- (void)loadFontPrefs;
- (void)saveFontPrefs;

- (NSMutableDictionary *)makeDictFromPrefsForFontType:(NSString*)fontType andRegion:(NSString*)regionCode;
- (NSMutableDictionary *)makeFontSizesDictFromPrefsForRegion:(NSString*)regionCode;

- (void)saveFontNamePrefsForRegion:(NSDictionary*)regionDict forFontType:(NSString*)fontType;
- (void)saveFontSizePrefsForRegion:(NSDictionary*)entryDict;

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


@implementation OrgMozillaChimeraPreferenceAppearance

- (void)dealloc
{
  [regionMappingTable release];
  [defaultFontType release];
  [super dealloc];
}

- (id)initWithBundle:(NSBundle *)bundle
{
  self = [super initWithBundle:bundle];
  fontButtonForEditor = nil;
  return self;
}

- (void)mainViewDidLoad
{
  BOOL gotPref;
  [checkboxUnderlineLinks setState:
      [self getBooleanPref:"browser.underline_anchors" withSuccess:&gotPref]];
  [checkboxUseMyColors setState:
      ![self getBooleanPref:"browser.display.use_document_colors" withSuccess:&gotPref]];

  // should save and restore this
  [[NSColorPanel sharedColorPanel] setContinuous:NO];
  
  [colorwellBackgroundColor setColor:[self getColorPref:"browser.display.background_color" withSuccess:&gotPref]];
  [colorwellTextColor setColor:[self getColorPref:"browser.display.foreground_color" withSuccess:&gotPref]];
  [colorwellUnvisitedLinks setColor:[self getColorPref:"browser.anchor_color" withSuccess:&gotPref]];
  [colorwellVisitedLinks setColor:[self getColorPref:"browser.visited_color" withSuccess:&gotPref]];
  
  BOOL gotDefaultFont;
  NSString* defaultFont = [self getStringPref:"font.default" withSuccess:&gotDefaultFont];
  if (!gotDefaultFont || (![defaultFont isEqualTo:@"serif"] && ![defaultFont isEqualTo:@"sans-serif"]))
    defaultFont = @"serif";
  
  defaultFontType = [defaultFont retain];

  [self setupFontRegionPopup];
  [self updateFontPreviews];
}

- (void)willUnselect
{
  // time to save stuff
  [self saveFontPrefs];
}

- (void)setupFontRegionPopup
{
  NSBundle* prefBundle = [NSBundle bundleForClass:[self class]];
  NSString* resPath = [prefBundle pathForResource:@"RegionMapping" ofType:@"plist"];

  // we use the dictionaries in this array as temporary storage of font
  // values until the pane is unloaded, at which time they are saved
  regionMappingTable = [[NSArray arrayWithContentsOfFile:resPath] retain];

  [self loadFontPrefs];

  [popupFontRegion removeAllItems];
  for (unsigned int i = 0; i < [regionMappingTable count]; i++) {
    NSDictionary* regionDict = [regionMappingTable objectAtIndex:i];
    [popupFontRegion addItemWithTitle:[regionDict objectForKey:@"region"]];
  }
}

- (IBAction)buttonClicked:(id)sender
{
  if (sender == checkboxUnderlineLinks)
    [self setPref:"browser.underline_anchors" toBoolean:[sender state]];
  else if (sender == checkboxUseMyColors)
    [self setPref:"browser.display.use_document_colors" toBoolean:![sender state]];
}

- (IBAction)colorChanged:(id)sender
{
  const char* prefName = NULL;
  
  if (sender == colorwellBackgroundColor)
  	prefName = "browser.display.background_color";
  else if (sender == colorwellTextColor)
  	prefName = "browser.display.foreground_color";
  else if (sender == colorwellUnvisitedLinks)
  	prefName = "browser.anchor_color";
  else if (sender == colorwellVisitedLinks)
  	prefName = "browser.visited_color";

  if (prefName)
    [self setPref:prefName toColor:[sender color]];
}

- (IBAction)proportionalFontChoiceButtonClicked:(id)sender
{
  NSFontManager *fontManager = [NSFontManager sharedFontManager];
  NSFont *newFont = [[self getFontSampleForType:[chooseProportionalFontButton alternateTitle]] font];
  fontButtonForEditor = chooseProportionalFontButton;
  [fontManager setSelectedFont:newFont isMultiple:NO];
  [[fontManager fontPanel:YES] makeKeyAndOrderFront:self];
}

- (IBAction)monospaceFontChoiceButtonClicked:(id)sender
{
  NSFontManager *fontManager = [NSFontManager sharedFontManager];
  NSFont *newFont = [[self getFontSampleForType:[chooseMonospaceFontButton alternateTitle]] font];
  fontButtonForEditor = chooseMonospaceFontButton;
  [fontManager setSelectedFont:newFont isMultiple:NO];
  [[fontManager fontPanel:YES] makeKeyAndOrderFront:self];
}

- (IBAction)fontRegionPopupClicked:(id)sender
{
  // save the old values
  [self updateFontPreviews];
}

- (void)loadFontPrefs
{
  for (unsigned int i = 0; i < [regionMappingTable count]; i++) {
    NSMutableDictionary	*regionDict = [regionMappingTable objectAtIndex:i];
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
  }
}

- (void)saveFontPrefs
{
  if (!regionMappingTable)
    return;

  for (unsigned int i = 0; i < [regionMappingTable count]; i++) {
    NSMutableDictionary	*regionDict = [regionMappingTable objectAtIndex:i];

    [self saveFontNamePrefsForRegion:regionDict forFontType:@"serif"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"sans-serif"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"monospace"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"cursive"];
    [self saveFontNamePrefsForRegion:regionDict forFontType:@"fantasy"];
    
    [self saveFontSizePrefsForRegion:regionDict];
  }
  
  if (defaultFontType && [defaultFontType length] > 0)
    [self setPref:"font.default" toString:defaultFontType];
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

- (void)saveFontNamePrefsForRegion:(NSDictionary*)regionDict forFontType:(NSString*)fontType;
{
  NSString *regionCode = [regionDict objectForKey:@"code"];

  if (!regionDict || !fontType || !regionCode) return;

  NSDictionary* fontTypeDict = [regionDict objectForKey:fontType];
  
  NSString *fontName = [fontTypeDict objectForKey:@"fontfamily"];
  NSString *fontPrefName = [NSString stringWithFormat:@"font.name.%@.%@", fontType, regionCode];

  if (fontName)
    [self setPref:[fontPrefName cString] toString:fontName];
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

  if (variableSize)
    [self setPref:[variableSizePref cString] toInt:variableSize];

  if (fixedSize)
    [self setPref:[fixedSizePref cString] toInt:fixedSize];

  if (minSize)
    [self setPref:[minSizePref cString] toInt:minSize];
  else
    [self clearPref:[minSizePref cString]];
}

#pragma mark -

- (void)saveFont:(NSFont*)font toDict:(NSMutableDictionary*)regionDict forType:(NSString*)fontType
{
  NSMutableDictionary *fontTypeDict = [regionDict objectForKey:fontType];
  NSMutableDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
  
  if ([[fontTypeDict objectForKey:@"missing"] boolValue]) // will be false if no object
    return;

  if (font) {
    [fontTypeDict setObject:[font familyName] forKey:@"fontfamily"];
    [fontSizeDict setObject:[NSNumber numberWithInt:(int)[font pointSize]] forKey:[self getFontSizeType:fontType]];
  }
}
 
- (NSFont*)getFontOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;
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
  [self setupFontSampleOfType:@"serif" fromDict:regionDict]; // one of these...
  [self setupFontSampleOfType:@"sans-serif"	fromDict:regionDict]; // is a no-op
  [self setupFontSampleOfType:@"monospace" fromDict:regionDict];
}

- (void)setupFontSampleOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;
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
  
  if (font == nil) {
    if (regionDict) {
      NSDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
      NSString *fontName = [fontTypeDict objectForKey:@"fontfamily"];
      int fontSize = [[fontSizeDict objectForKey:[self getFontSizeType:fontType]] intValue];

      displayString = [NSString stringWithFormat:@"%@, %dpt %@", fontName, fontSize, [self getLocalizedString:@"Missing"]];
      font = [NSFont userFontOfSize:14.0];

      // set the missing flag in the dict
      if (![fontTypeDict objectForKey:@"missing"] || ![[fontTypeDict objectForKey:@"missing"] boolValue]) {
        [fontTypeDict setObject:[NSNumber numberWithBool:YES] forKey:@"missing"];
      }
    } else {
      // should never happen
      // XXX localize
      displayString = @"Font missing";
      font = [NSFont userFontOfSize:16.0];
    }
  } else {
    NS_DURING
      displayString = [NSString stringWithFormat:@"%@, %dpt", [font displayName], (int)[font pointSize]];
    NS_HANDLER
      displayString = [NSString stringWithFormat:@"%@, %dpt", [font familyName], (int)[font pointSize]];
    NS_ENDHANDLER
    
    // make sure we don't have a missing entry
    [fontTypeDict removeObjectForKey:@"missing"];
  }
  
  [sampleCell setFont:font];
  [sampleCell setStringValue:displayString];
}

- (void)updateFontSampleOfType:(NSString *)fontType
{
  int selectedRegion = [popupFontRegion indexOfSelectedItem];
  if (selectedRegion == -1)
    return;

  NSMutableDictionary *regionDict = [regionMappingTable objectAtIndex:selectedRegion];

  NSTextField *sampleCell = [self getFontSampleForType:fontType];
  NSFont *sampleFont = [[NSFontManager sharedFontManager] convertFont:[sampleCell font]];

  // save the font in the dictionaries
  [self saveFont:sampleFont toDict:regionDict forType:fontType];
  // and update the sample
  [self setFontSampleOfType:fontType withFont:sampleFont andDict:regionDict];
}

- (NSTextField*)getFontSampleForType:(NSString *)fontType
{
  if ([fontType isEqualToString:defaultFontType])
    return fontSampleProportional;

  if ([fontType isEqualToString:@"monospace"])
    return fontSampleMonospace;

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
  int selectedRegion = [popupFontRegion indexOfSelectedItem];
  if (selectedRegion == -1)
    return;

  NSDictionary *regionDict = [regionMappingTable objectAtIndex:selectedRegion];

  [chooseProportionalFontButton setAlternateTitle:defaultFontType];

  // make sure the 'proportional' label matches
  NSString* propLabelString = [NSString stringWithFormat:[self getLocalizedString:@"ProportionalLableFormat"], [self getLocalizedString:defaultFontType]];
  [proportionalSampleLabel setStringValue:propLabelString];
  
  [self setupFontSamplesFromDict:regionDict];
}

#pragma mark -

const int kDefaultFontSerifTag = 0;
const int kDefaultFontSansSerifTag = 1;

- (IBAction)showAdvancedFontsDialog:(id)sender
{
  int selectedRegion = [popupFontRegion indexOfSelectedItem];
  if (selectedRegion == -1)
    return;

  NSDictionary *regionDict = [regionMappingTable objectAtIndex:selectedRegion];

  NSString* advancedLabel = [NSString stringWithFormat:[self getLocalizedString:@"AdditionalFontsLabelFormat"], [regionDict objectForKey:@"region"]];
  [advancedFontsLabel setStringValue:advancedLabel];
  
  // set up the dialog for the current region
  [self setupFontPopup:serifFontPopup forType:@"serif" fromDict:regionDict];
  [self setupFontPopup:sansSerifFontPopup forType:@"sans-serif" fromDict:regionDict];
  [self setupFontPopup:cursiveFontPopup forType:@"cursive" fromDict:regionDict];
  [self setupFontPopup:fantasyFontPopup forType:@"fantasy" fromDict:regionDict];
  
  // setup min size popup
  int itemIndex = 0;
  NSMutableDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
  NSNumber* minSize = [fontSizeDict objectForKey:@"minimum"];
  if (minSize) {
    itemIndex = [minFontSizePopup indexOfItemWithTag:[minSize intValue]];
    if (itemIndex == -1)
      itemIndex = 0;
  }
  [minFontSizePopup selectItemAtIndex:itemIndex];
  
  // set up default font radio buttons (default to serif)
  [defaultFontMatrix selectCellWithTag:([defaultFontType isEqualToString:@"sans-serif"] ? kDefaultFontSansSerifTag : kDefaultFontSerifTag)];
  
	[NSApp beginSheet:advancedFontsDialog
        modalForWindow:[tabView window] // any old window accessor
        modalDelegate:self
        didEndSelector:@selector(advancedFontsSheetDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
}

- (IBAction)advancedFontsDone:(id)sender
{
  // save settings
  int selectedRegion = [popupFontRegion indexOfSelectedItem];
  if (selectedRegion == -1)
    return;

  NSDictionary *regionDict = [regionMappingTable objectAtIndex:selectedRegion];

  [self getFontFromPopup:serifFontPopup forType:@"serif" intoDict:regionDict];
  [self getFontFromPopup:sansSerifFontPopup forType:@"sans-serif" intoDict:regionDict];
  [self getFontFromPopup:cursiveFontPopup forType:@"cursive" intoDict:regionDict];
  [self getFontFromPopup:fantasyFontPopup forType:@"fantasy" intoDict:regionDict];

  int minSize = [[minFontSizePopup selectedItem] tag];
  // a value of 0 indicates 'none'; we'll clear the pref on save
  NSMutableDictionary *fontSizeDict = [regionDict objectForKey:@"fontsize"];
  [fontSizeDict setObject:[NSNumber numberWithInt:(int)minSize] forKey:@"minimum"];
    
  // save the default font
  [defaultFontType release];
  defaultFontType = ([[defaultFontMatrix selectedCell] tag] == kDefaultFontSerifTag) ? @"serif" : @"sans-serif";
  [defaultFontType retain];
  
  [advancedFontsDialog orderOut:self];
  [NSApp endSheet:advancedFontsDialog];

  [self updateFontPreviews];
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

- (void)changeFont:(id)sender
{
  if (fontButtonForEditor) {
    NSString *fontType = [fontButtonForEditor alternateTitle];
    [self updateFontSampleOfType:fontType];
  }
}

- (BOOL)fontManager:(id)theFontManager willIncludeFont:(NSString *)fontName
{
  // filter out fonts for the selected language
  return YES;
}

@end
