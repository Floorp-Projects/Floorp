//
//  Appearance pref pane for chimera
//  
//
//  Created by Simon Fraser on Wed Jun 19 2002.
//  Copyright (c) 2000 __MyCompanyName__. All rights reserved.
//

#import "Appearance.h"

#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPref.h"
#include "nsIMemory.h"

// Use a category to put a changeFont message onto the tab view
@interface NSTabView (FontManagerDelegate)
- (void)changeFont:(id)sender;
@end

@interface OrgMozillaChimeraPreferenceAppearance(Private)

- (void)setupFontRegionTable;
- (void)loadFontPrefs;
- (void)saveFontPrefs;

- (NSMutableDictionary *)makeDictFromPrefsForFontType:(NSString*)fontType andRegion:(NSString*)regionCode;
- (void)saveToPrefsEntriesInDict:(NSDictionary*)entryDict forFontType:(NSString*)fontType;

- (void)setupFontSamplesFromDict:(NSDictionary*)regionDict;
- (void)setupFontSampleOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;

- (NSFont*)getFontOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;

- (void)setFontSampleOfType:(NSString *)fontType withFont:(NSFont*)font andDict:(NSMutableDictionary*)regionDict;

- (void)updateFontSampleOfType:(NSString *)fontType;
- (NSTextField*)getFontSampleForType:(NSString *)fontType;
- (void)syncFontPanel;
- (void)saveCurrentFonts;

@end

@implementation OrgMozillaChimeraPreferenceAppearance

- (void) dealloc
{
  [regionMappingTable release];
}

- (void) mainViewDidLoad
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
  
  [self setupFontRegionTable];
  [NSFontManager sharedFontManager];
}

- (void)willUnselect
{
  // time to save stuff
  [self saveCurrentFonts];
  [self saveFontPrefs];
}

- (void)setupFontRegionTable
{
  NSBundle* prefBundle 	= [NSBundle bundleForClass:[self class]];
  NSString* resPath 		= [prefBundle pathForResource:@"RegionMapping" ofType:@"plist"];

  // we use the dictionaries in this array as temporary storage of font
  // values until the pane is unloaded, at which time they are saved
  regionMappingTable = [[NSArray arrayWithContentsOfFile:resPath] retain];

  [self loadFontPrefs];

  [tableViewFontRegion reloadData];
  [tableViewFontRegion selectRow:0 byExtendingSelection:NO];		// trigger initial setup
}

- (IBAction)buttonClicked:(id)sender
{
  if (sender == checkboxUnderlineLinks)
  {
    [self setPref:"browser.underline_anchors" toBoolean:[sender state]];
  }
  else if (sender == checkboxUseMyColors)
  {
    [self setPref:"browser.display.use_document_colors" toBoolean:![sender state]];
  }
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

- (IBAction)fontChoiceButtonClicked:(id)sender
{  
  [self syncFontPanel];
  [[NSFontManager sharedFontManager] orderFrontFontPanel:self];
}

- (IBAction)fontRegionListClicked:(id)sender
{
  // do nothing. we update stuff on selection change
}

- (void)loadFontPrefs
{
  for (int i = 0; i < [regionMappingTable count]; i ++)
  {
    NSMutableDictionary	*regionDict = [regionMappingTable objectAtIndex:i];
    NSString	*regionCode = [regionDict objectForKey:@"code"];
    
    /*
      For each region in the array, there is a dictionary of 
        {
          region =
          code   =

      to which we add here a sub-dictionary per font type, thus:
        
          serif = {
              fontname =
              fontsize = 
            }
          sans-serif = {
              fontname =
              fontsize = 
              missing  = 		// set if a font is missing
            }
          monospace =   {
              fontname =
              fontsize = 
            }
        }
    */
    
    NSMutableDictionary *serifDict = [self makeDictFromPrefsForFontType:@"serif" andRegion:regionCode];
    [regionDict setObject:serifDict forKey:@"serif"];

    NSMutableDictionary	*sanssDict = [self makeDictFromPrefsForFontType:@"sans-serif" andRegion:regionCode];
    [regionDict setObject:sanssDict forKey:@"sans-serif"];

    NSMutableDictionary *monoDict = [self makeDictFromPrefsForFontType:@"monospace" andRegion:regionCode];
    [regionDict setObject:monoDict forKey:@"monospace"];
  }

}

- (void)saveFontPrefs
{
  if (!regionMappingTable)
    return;  

  for (int i = 0; i < [regionMappingTable count]; i ++)
  {
    NSMutableDictionary	*regionDict = [regionMappingTable objectAtIndex:i];

    [self saveToPrefsEntriesInDict:(NSDictionary*)regionDict forFontType:@"serif"];
    [self saveToPrefsEntriesInDict:(NSDictionary*)regionDict forFontType:@"sans-serif"];
    [self saveToPrefsEntriesInDict:(NSDictionary*)regionDict forFontType:@"monospace"];
  }
}

#pragma mark -

- (NSMutableDictionary *)makeDictFromPrefsForFontType:(NSString*)fontType andRegion:(NSString*)regionCode
{
  NSMutableDictionary *fontDict = [NSMutableDictionary dictionaryWithCapacity:2];
  
  NSString	*fontPrefName	= [NSString stringWithFormat:@"font.name.%@.%@", fontType, regionCode];
  NSString	*sizeType			= ([fontType isEqualToString:@"monospace"]) ? @"fixed" : @"variable";
  NSString	*fontSizeName	= [NSString stringWithFormat:@"font.size.%@.%@", sizeType, regionCode];

  BOOL gotPref, gotSize;
  NSString	*fontName	 	= [self getStringPref:[fontPrefName cString] withSuccess:&gotPref];
  int				fontSize		= [self getIntPref:[fontSizeName cString] withSuccess:&gotSize];

  //NSLog(@"Got font name %@ and size %d from prefs (success %d %d)", fontName, fontSize, gotPref, gotSize);

  if (gotPref && gotSize)
  {
    [fontDict setObject:fontName forKey:@"fontname"];
    [fontDict setObject:[NSNumber numberWithInt:fontSize] forKey:@"fontsize"];
  }	

  return fontDict;
}

- (void)saveToPrefsEntriesInDict:(NSDictionary*)regionDict forFontType:(NSString*)fontType
{
  NSDictionary	*fontTypeDict = [regionDict objectForKey:fontType];
  NSString			*regionCode 	= [regionDict objectForKey:@"code"];

  if (!fontTypeDict || !regionCode) return;
  
  NSString	*fontName			= [fontTypeDict objectForKey:@"fontname"];
  int				fontSize			= [[fontTypeDict objectForKey:@"fontsize"] intValue];
    
  NSString	*fontPrefName	= [NSString stringWithFormat:@"font.name.%@.%@", fontType, regionCode];
  NSString	*sizeType			= ([fontType isEqualToString:@"monospace"]) ? @"fixed" : @"variable";
  NSString	*fontSizeName	= [NSString stringWithFormat:@"font.size.%@.%@", sizeType, regionCode];

  if (fontName && fontSize > 0)
  {
    //NSLog(@"Setting '%@' to '%@' and '%@' to %d", fontPrefName, [theFont familyName], fontSizeName, (int)[theFont pointSize]);
    [self setPref:[fontPrefName cString] toString:fontName];
    [self setPref:[fontSizeName cString] toInt:fontSize];
  }
}

#pragma mark -

- (void) saveFontsToDict:(NSMutableDictionary*)regionDict forFontType:(NSString*)fontType
{
  NSMutableDictionary	*fontTypeDict = [regionDict objectForKey:fontType];
  
  BOOL missingFont = [[fontTypeDict objectForKey:@"missing"] boolValue];		// will be NO if no object
  if (missingFont)
    return;

  NSFont	*theFont	= [[self getFontSampleForType:fontType] font];

  if (theFont)
  {
    [fontTypeDict setObject:[theFont familyName] forKey:@"fontname"];
    [fontTypeDict setObject:[NSNumber numberWithInt:(int)[theFont pointSize]] forKey:@"fontsize"];
  }
}
 
- (NSFont*)getFontOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;
{
  NSDictionary	*fontTypeDict = [regionDict objectForKey:fontType];
  NSString			*fontName 		= [fontTypeDict objectForKey:@"fontname"];
  int						fontSize			= [[fontTypeDict objectForKey:@"fontsize"] intValue];
  
  NSFont				*returnFont = nil;
  
  if (fontName && fontSize > 0)
    returnFont = [NSFont fontWithName:fontName size:fontSize];
  else if (fontName)		// no size
    returnFont = [NSFont fontWithName:fontName size:16.0];
  
/*
  if (returnFont == nil)
    returnFont = ([fontType isEqualToString:@"monospace"]) ?
              [NSFont userFixedPitchFontOfSize:14.0] :
              [NSFont userFontOfSize:16.0];

*/
  // we return nil if the font was not found
  return returnFont;
}

- (void)setupFontSamplesFromDict:(NSDictionary*)regionDict
{
  [self setupFontSampleOfType:@"serif" 			fromDict:regionDict];
  [self setupFontSampleOfType:@"sans-serif" fromDict:regionDict];
  [self setupFontSampleOfType:@"monospace" 	fromDict:regionDict];
}

- (void)setupFontSampleOfType:(NSString*)fontType fromDict:(NSDictionary*)regionDict;
{
  NSFont	*foundFont = [self getFontOfType:fontType fromDict:regionDict];
  [self setFontSampleOfType:fontType withFont:foundFont andDict:regionDict];
}

- (void)setFontSampleOfType:(NSString *)fontType withFont:(NSFont*)font andDict:(NSMutableDictionary*)regionDict
{
  // font may be nil here, in which case the font is missing, and we construct
  // a string to display from the dict.
  NSMutableDictionary	*fontTypeDict = [regionDict objectForKey:fontType];

  NSTextField		*textField = [self getFontSampleForType:fontType];
  NSString			*displayString = nil;
  
  if (font == nil)
  {
    if (regionDict)
    {
      NSString						*fontName 		= [fontTypeDict objectForKey:@"fontname"];
      int									fontSize			= [[fontTypeDict objectForKey:@"fontsize"] intValue];

      // XXX localize
      displayString = [NSString stringWithFormat:@"%@, %dpt (missing)", fontName, fontSize];
      font = [NSFont userFontOfSize:14.0];

      // set the missing flag in the dict
      if (![fontTypeDict objectForKey:@"missing"] || ![[fontTypeDict objectForKey:@"missing"] boolValue])
      {
        [fontTypeDict setObject:[NSNumber numberWithBool:YES] forKey:@"missing"];
      }
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
    NS_DURING
      displayString = [NSString stringWithFormat:@"%@, %dpt", [font displayName], (int)[font pointSize]];
    NS_HANDLER
      NSLog(@"Exception %@ getting [font displayName] for %@", localException, font);
      displayString = [NSString stringWithFormat:@"%@, %dpt", [font familyName], (int)[font pointSize]];
    NS_ENDHANDLER
    
    // make sure we don't have a missing entry
    [fontTypeDict removeObjectForKey:@"missing"];
  }
  
  [textField setFont:font];
  [textField setStringValue:displayString];
}

- (void)updateFontSampleOfType:(NSString *)fontType
{
  int selectedRow 	= [tableViewFontRegion selectedRow];
  if (selectedRow == -1)
    return;

  NSMutableDictionary	*regionDict	= [regionMappingTable objectAtIndex:selectedRow];

  NSTextField		*textField 	= [self getFontSampleForType:fontType];
  NSFont				*sampleFont = [[NSFontManager sharedFontManager] convertFont:[textField font]];

  [self setFontSampleOfType:fontType withFont:sampleFont andDict:regionDict];
}

- (NSTextField*)getFontSampleForType:(NSString *)fontType
{
  if ([fontType isEqualToString:@"serif"])
    return fontSampleSerif;

  if ([fontType isEqualToString:@"sans-serif"])
    return fontSampleSansSerif;

  if ([fontType isEqualToString:@"monospace"])
    return fontSampleMonospace;
    
  return nil;
}

- (void)syncFontPanel
{
  NSString 	*fontType = [[matrixChooseFont selectedCell] alternateTitle];
  NSFont		*newFont	= [[self getFontSampleForType:fontType] font];
  
  [[NSFontManager sharedFontManager] setSelectedFont:newFont isMultiple:NO];
}

- (void)saveCurrentFonts
{
  int selectedRow 	= [tableViewFontRegion selectedRow];
  if (selectedRow == -1)
    return;

  NSDictionary	*regionDict 	= [regionMappingTable objectAtIndex:selectedRow];

  [self saveFontsToDict:regionDict forFontType:@"serif"];
  [self saveFontsToDict:regionDict forFontType:@"sans-serif"];
  [self saveFontsToDict:regionDict forFontType:@"monospace"];
}

@end

@implementation OrgMozillaChimeraPreferenceAppearance (FontRegionTableDelegate)

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
  NSLog(@"Table selection changed");
  
  if ([aNotification object] != tableViewFontRegion)
    return;

  int	selectedRow = [tableViewFontRegion selectedRow];
  if (selectedRow == -1)
    return;
    
  NSDictionary	*regionDict = [regionMappingTable objectAtIndex:selectedRow];
  
  [self setupFontSamplesFromDict:regionDict];
  [self syncFontPanel];
}

- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTableView
{
  [self saveCurrentFonts];
  return YES;
}

@end


@implementation OrgMozillaChimeraPreferenceAppearance (FontRegionTableDataSource)

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
  if (aTableView == tableViewFontRegion)
    return [regionMappingTable count];
    
  return 0;
}

- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
    row:(int)rowIndex
{
  NSParameterAssert(rowIndex >= 0 && rowIndex < [regionMappingTable count]);
	
  NSDictionary* regionDict = [regionMappingTable objectAtIndex:rowIndex];
  return [regionDict objectForKey:[aTableColumn identifier]];
}

@end

@implementation OrgMozillaChimeraPreferenceAppearance (FontManagerDelegate)

- (void)changeFont:(id)sender
{
  NSString 	*fontType = [[matrixChooseFont selectedCell] alternateTitle];
  [self updateFontSampleOfType:fontType];
}

- (BOOL)fontManager:(id)theFontManager willIncludeFont:(NSString *)fontName
{
  NSLog(@"theFontManager willIncludeFont %@", fontName);
  // filter out fonts for the selected language
  return YES;
}

@end

// Use a category to put a changeFont message onto the tab view (since this
// is guaranteed to be in the repsonder chain. We've set the tab view's delegate
// to be the file's owner in IB, so we just forward the message.
@implementation NSTabView (FontManagerDelegate)

- (void)changeFont:(id)sender
{
  [[self delegate] changeFont:sender];
}

- (BOOL)fontManager:(id)theFontManager willIncludeFont:(NSString *)fontName
{
  NSLog(@"willIncludeFont called");
  return YES;
}

@end
