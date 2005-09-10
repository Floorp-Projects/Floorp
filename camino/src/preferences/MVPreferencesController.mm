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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#import "PreferencePaneBase.h"

#import "MVPreferencesController.h"
#import "ToolbarAdditions.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "CHBrowserService.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_3
// fix warnings
@interface NSToolbar(CaminoNSToolbarPantherAdditions)
- (void)setSelectedItemIdentifier:(NSString *)itemIdentifier;
- (NSString *)selectedItemIdentifier;
- (void)setSizeMode:(NSToolbarSizeMode)sizeMode;
- (NSToolbarSizeMode)sizeMode;
@end
#endif


static MVPreferencesController *gSharedInstance = nil;

NSString* const MVPreferencesWindowNotification = @"MVPreferencesWindowNotification";

static NSString* const CacheInfoPaneImageKey  = @"MVPreferencePaneImage";
static NSString* const CacheInfoPaneLabelKey  = @"MVPreferencePaneLabel";
static NSString* const CacheInfoPaneSeenKey   = @"MVPreferencePaneSeen";    // NSNumber with bool


@interface NSToolbar (NSToolbarPrivate)

- (NSView *) _toolbarView;

@end

#pragma mark -

@interface MVPreferencesController (MVPreferencesControllerPrivate)

+ (NSString*)applicationSupportPathForDomain:(short)inDomain;

- (void)loadPreferencePanesAtPath:(NSString*)inFolderPath;
- (void)buildPrefPaneIdentifierList;
- (void)doUnselect:(NSNotification *) notification;
- (IBAction)selectPreferencePane:(id) sender;
- (void)resizeWindowForContentView:(NSView *) view;
- (NSMutableDictionary*)infoCacheForPane:(NSString*)paneIdentifier;
- (NSImage*)imageForPane:(NSString*)paneIdentifier;
- (NSString*)labelForPane:(NSString*)paneIdentifier;
- (id)currentPane;

@end

#pragma mark -

@implementation MVPreferencesController

+ (MVPreferencesController *)sharedInstance
{
  if (!gSharedInstance)
    gSharedInstance = [[MVPreferencesController alloc] init];
    
  return gSharedInstance;
}

+ (void)clearSharedInstance
{
  [[gSharedInstance window] performClose:nil];

  [gSharedInstance release];
  gSharedInstance = nil;
}

- (id)init
{
  if ((self = [super init]))
  {
    mPaneBundles  = [[NSMutableArray alloc] initWithCapacity:16];
    mLoadedPanes  = [[NSMutableDictionary dictionary] retain];
    mPaneInfo     = [[NSMutableDictionary dictionary] retain];

    NSString* paneBundlesPath = [NSString stringWithFormat:@"%@/Contents/PreferencePanes", [[NSBundle mainBundle] bundlePath]];
    [self loadPreferencePanesAtPath:paneBundlesPath];

    // this matches code in -initMozillaPrefs
    NSString* profileDirName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"mozNewProfileDirName"];
    NSString* prefPanesSubpath = [profileDirName stringByAppendingPathComponent:@"PreferencePanes"];

    NSString* userLibPath = [MVPreferencesController applicationSupportPathForDomain:kUserDomain];
    if (userLibPath)
    {
      userLibPath = [userLibPath stringByAppendingPathComponent:prefPanesSubpath];
      [self loadPreferencePanesAtPath:[userLibPath stringByStandardizingPath]];
    }
    
    NSString* globalLibPath = [MVPreferencesController applicationSupportPathForDomain:kLocalDomain];
    if (globalLibPath)
    {
      globalLibPath = [globalLibPath stringByAppendingPathComponent:prefPanesSubpath];
      [self loadPreferencePanesAtPath:[globalLibPath stringByStandardizingPath]];
    }
    
    [self buildPrefPaneIdentifierList];
    
    [NSBundle loadNibNamed:@"MVPreferences" owner:self];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(doUnselect:)
                                                 name:NSPreferencePaneDoUnselectNotification
                                               object:nil];
  }
  return self;
}

- (void) dealloc
{
  [mCurrentPaneIdentifier autorelease];
  [mLoadedPanes autorelease];
  [mPaneBundles autorelease];
  [mPaneInfo autorelease];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  
  [super dealloc];
}

- (void) awakeFromNib
{
  NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:@"preferences.toolbar"] autorelease];

  [mWindow setDelegate:self];
  [mWindow setFrameAutosaveName:@"CaminoPreferenceWindowFrame"];

  [toolbar setAllowsUserCustomization:NO];
  [toolbar setAutosavesConfiguration:NO];
  [toolbar setDelegate:self];
  [toolbar setAlwaysCustomizableByDrag:NO];
  [toolbar setShowsContextMenu:NO];
  [mWindow setToolbar:toolbar];
  [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
}

- (NSWindow *)window
{
  return mWindow;
}

- (IBAction) showPreferences:(id) sender
{
  if ( ![mWindow isVisible] ) {
    // on being shown, register as a window that cares about XPCOM being active until we're done
    // with it in |windowDidClose()|. Need to ensure this is exactly balanced with |BrowserClosed()|
    // calls as it increments a refcount. As a result, we can only call it when we're making
    // the window visible. Too bad cocoa doesn't give us any notifications of this.
    CHBrowserService::InitEmbedding();
  }

  // If a pref pane is not showing, then show the general pane
  if (!mCurrentPaneIdentifier && (![[mWindow contentView] isEqual:mMainView]))
    [self selectPreferencePaneByIdentifier:@"org.mozilla.chimera.preference.navigation"];
    
  [mWindow makeKeyAndOrderFront:nil];
}

- (void)selectPreferencePaneByIdentifier:(NSString *)identifier
{
  NSBundle *bundle = [NSBundle bundleWithIdentifier:identifier];
  
  if (bundle && ![mCurrentPaneIdentifier isEqualToString:identifier])
  {
    if ( mCurrentPaneIdentifier &&
        [[self currentPane] shouldUnselect] != NSUnselectNow ) {
      /* more to handle later */
#if DEBUG
      NSLog( @"can't unselect current" );
#endif
      mCloseWhenPaneIsReady = NO;
      [mPendingPaneIdentifier autorelease];
      mPendingPaneIdentifier = [identifier retain];
      return;
    }
    
    [mPendingPaneIdentifier autorelease];
    mPendingPaneIdentifier = nil;

    [mLoadingImageView setImage:[self imageForPane:identifier]];
    [mLoadingTextFeld setStringValue:[NSString stringWithFormat:NSLocalizedString( @"Loading %@...", nil ), [self labelForPane:identifier]]];
    
    [mWindow setTitle:[self labelForPane:identifier]];
    [mWindow setContentView:mLoadingView];
    [mWindow display];
    
    NSPreferencePane *pane = nil;
    NSView *prefView = nil;

    if (!(pane = [mLoadedPanes objectForKey:identifier]))
    {
      pane = [[[[bundle principalClass] alloc] initWithBundle:bundle] autorelease];
      if (pane)
        [mLoadedPanes setObject:pane forKey:identifier];
    }
    
    if ( [pane loadMainView] )
    {
      [pane willSelect];
      prefView = [pane mainView];

      [self resizeWindowForContentView:prefView];

      [[self currentPane] willUnselect];
      [mWindow setContentView:prefView];
      [[self currentPane] didUnselect];

      [pane didSelect];
      [[NSNotificationCenter defaultCenter]
        postNotificationName: MVPreferencesWindowNotification
        object: self
        userInfo: [NSDictionary dictionaryWithObjectsAndKeys:mWindow, @"window", nil]];

      [mCurrentPaneIdentifier autorelease];
      mCurrentPaneIdentifier = [identifier copy];

      [mWindow setInitialFirstResponder:[pane initialKeyView]];
      [mWindow makeFirstResponder:[pane initialKeyView]];
      if ([NSToolbar instancesRespondToSelector:@selector(setSelectedItemIdentifier:)])
        [[mWindow toolbar] setSelectedItemIdentifier:mCurrentPaneIdentifier];
    }
    else
    {
      NSRunCriticalAlertPanel( NSLocalizedString( @"Preferences Error", nil ),
        [NSString stringWithFormat:NSLocalizedString( @"Could not load %@", nil ),
        [self labelForPane:identifier]], nil, nil, nil );
    }
  }
}

- (BOOL) windowShouldClose:(id) sender
{
  if ( mCurrentPaneIdentifier && [[self currentPane] shouldUnselect] != NSUnselectNow ) {
#if DEBUG
    NSLog( @"can't unselect current" );
#endif
    mCloseWhenPaneIsReady = YES;
    return NO;
  }
  return YES;
}

- (void) windowWillClose:(NSNotification *) notification
{
  // we want to behave as if we're unselecting, but we're not really unselecting. We are leaving
  // the current pref pane selected per Apple's recommendation.
  [[self currentPane] willUnselect];
  [[self currentPane] didUnselect];
  
  // write out prefs and user defaults
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, prefs unsaved");
  if ( prefService )
    prefService->SavePrefFile(nsnull);      // nsnull means write to prefs.js
  [[NSUserDefaults standardUserDefaults] synchronize];

  // tell gecko that this window no longer needs it around.
  CHBrowserService::BrowserClosed();
}

- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
  if ([[self currentPane] respondsToSelector:@selector(didActivate)])
    [[self currentPane] didActivate];
}

- (id)windowWillReturnFieldEditor:(NSWindow *)sender toObject:(id)anObject
{
  if ([[self currentPane] respondsToSelector:@selector(fieldEditorForObject:)])
    return [[self currentPane] fieldEditorForObject:anObject];
  
  return nil;
}

#pragma mark -

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
  NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier] autorelease];

  [toolbarItem setLabel:[self labelForPane:itemIdentifier]];
  [toolbarItem setImage:[self imageForPane:itemIdentifier]];
  [toolbarItem setTarget:self];
  [toolbarItem setAction:@selector( selectPreferencePane: )];

  return toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *) toolbar
{
  return mToolbarItemIdentifiers;
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *) toolbar
{
  return mToolbarItemIdentifiers;
}

// For OS X 10.3, set the selectable toolbar items (draws a gray rect around the active icon in the toolbar)
- (NSArray *) toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
  NSMutableArray* items = [NSMutableArray array];
  NSEnumerator* enumerator = [mPaneBundles objectEnumerator];
  id item = nil;
  while ( ( item = [enumerator nextObject] ) )
    [items addObject:[item bundleIdentifier]];
  return items;
}

@end

#pragma mark -

@implementation MVPreferencesController (MVPreferencesControllerPrivate)

+ (NSString*)applicationSupportPathForDomain:(short)inDomain
{
  FSRef   folderRef;
  if (::FSFindFolder(inDomain, kApplicationSupportFolderType, kCreateFolder, &folderRef) == noErr)
  {
    CFURLRef theURL = ::CFURLCreateFromFSRef(kCFAllocatorDefault, &folderRef);
    if (theURL)
    {
      NSString* folderPath = [(NSURL*)theURL path];
      ::CFRelease(theURL);
      return folderPath;
    }
  }
  
  return nil;
}

- (IBAction)selectPreferencePane:(id) sender
{
  [self selectPreferencePaneByIdentifier:[sender itemIdentifier]];
}

- (void)loadPreferencePanesAtPath:(NSString*)inFolderPath
{
  NSArray* folderContents = [[NSFileManager defaultManager] directoryContentsAtPath:inFolderPath];

  NSEnumerator* filesEnum = [folderContents objectEnumerator];
  NSString* curPath;
  while ((curPath = [filesEnum nextObject]))
  {
    NSString* bundlePath = [inFolderPath stringByAppendingPathComponent:curPath];
    
    if (![[NSWorkspace sharedWorkspace] isFilePackageAtPath:bundlePath])
      continue;
      
    NSBundle* curBundle = [NSBundle bundleWithPath:bundlePath];
    
    // require the bundle signature to be "MOZC" or "CHIM" (former creator code)
    NSString* bundleSig = [[curBundle infoDictionary] objectForKey:@"CFBundleSignature"];
    if (![bundleSig isEqualToString:@"MOZC"] &&
        ![bundleSig isEqualToString:@"CHIM"])
    {
      NSLog(@"%@ not loaded as Camino pref pane: signature is not 'MOZC'", bundlePath);
      continue;
    }
    
    NSString* paneIdentifier = [curBundle bundleIdentifier];
    if ([[self infoCacheForPane:paneIdentifier] objectForKey:CacheInfoPaneSeenKey])
    {
      NSLog(@"%@ not loaded as Camino pref pane: already loaded pane with identifier %@", bundlePath, paneIdentifier);
      continue;
    }
    
    if (![curBundle load])
    {
      NSLog(@"%@ not loaded as Camino pref pane: failed to load", bundlePath);
      continue;
    }

    // require the principle class to be a subclass of NSPreferencePane
    if (![[curBundle principalClass] isSubclassOfClass:[NSPreferencePane class]])
    {
      NSLog(@"%@ not loaded as Camino pref pane: principal class not a subclass of NSPreferencePane", bundlePath);
      continue;
    }
    
    [mPaneBundles addObject:curBundle];   // retains    
    [[self infoCacheForPane:paneIdentifier] setObject:[NSNumber numberWithBool:YES] forKey:CacheInfoPaneSeenKey];
  }
}

- (void)buildPrefPaneIdentifierList
{
  if (mToolbarItemIdentifiers)
    return;

  NSArray* builtinItems = [NSArray arrayWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"MVPreferencePaneDefaults" ofType:@"plist"]];
  
  NSMutableArray* addonItems = [NSMutableArray array];

  NSEnumerator *enumerator = [mPaneBundles objectEnumerator];
  id item;
  while ((item = [enumerator nextObject]))
  {
    if (![builtinItems containsObject:[item bundleIdentifier]])
      [addonItems addObject:[item bundleIdentifier]];
  }
  
  mToolbarItemIdentifiers = [builtinItems arrayByAddingObjectsFromArray:addonItems];
}

- (void)doUnselect:(NSNotification *)notification
{
  if (mCloseWhenPaneIsReady)
    [mWindow close];
  [self selectPreferencePaneByIdentifier:mPendingPaneIdentifier];
}

- (void) resizeWindowForContentView:(NSView *) view
{
  NSRect windowFrame = [NSWindow contentRectForFrameRect:[mWindow frame] styleMask:[mWindow styleMask]];
  float  newWindowHeight = NSHeight( [view frame] );

  if ( [[mWindow toolbar] isVisible] )
    newWindowHeight += NSHeight( [[[mWindow toolbar] _toolbarView] frame] );
    
  NSRect newWindowFrame = [NSWindow frameRectForContentRect:NSMakeRect( NSMinX( windowFrame ), NSMaxY( windowFrame ) - newWindowHeight, NSWidth( windowFrame ), newWindowHeight ) styleMask:[mWindow styleMask]];
  [mWindow setFrame:newWindowFrame display:YES animate:[mWindow isVisible]];
}

- (NSMutableDictionary*)infoCacheForPane:(NSString*)paneIdentifier
{
  NSMutableDictionary*  cache = [mPaneInfo objectForKey:paneIdentifier];
  if (!cache)
  {
    cache = [NSMutableDictionary dictionary];
    [mPaneInfo setObject:cache forKey:paneIdentifier];
  }
  return cache;
}

- (NSImage*)imageForPane:(NSString*)paneIdentifier
{
  NSMutableDictionary*  cache = [self infoCacheForPane:paneIdentifier];
  
  NSImage* paneImage = [cache objectForKey:CacheInfoPaneImageKey];
  if (!paneImage)
  {
    NSBundle*     bundle = [NSBundle bundleWithIdentifier:paneIdentifier];
    NSDictionary* info   = [bundle infoDictionary];
    // try to get the icon for the bundle
    paneImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:[info objectForKey:@"NSPrefPaneIconFile"]]];
    // if that failed, fall back on a generic icon
    if (!paneImage )
      paneImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:[info objectForKey:@"CFBundleIconFile"]]];

    if (paneImage)
    {
      [cache setObject:paneImage forKey:CacheInfoPaneImageKey];
      [paneImage release];
    }
  }
  return paneImage;
}

- (NSString*)labelForPane:(NSString*)paneIdentifier
{
  NSMutableDictionary*  cache = [self infoCacheForPane:paneIdentifier];
  
  NSString* paneLabel = [cache objectForKey:CacheInfoPaneLabelKey];
  if (!paneLabel)
  {
    NSBundle*     bundle = [NSBundle bundleWithIdentifier:paneIdentifier];
    NSDictionary* info   = [bundle infoDictionary];

    paneLabel = NSLocalizedStringFromTableInBundle(@"PreferencePanelLabel", nil, bundle, nil);

    // if we failed to get the string
    if ([paneLabel isEqualToString:@"PreferencePanelLabel"])
      paneLabel = NSLocalizedStringFromTableInBundle( @"CFBundleName", @"InfoPlist", bundle, nil );

    // if we failed again...
    if ([paneLabel isEqualToString:@"CFBundleName"])
      paneLabel = [info objectForKey:@"CFBundleName"];

    if (!paneLabel)
      paneLabel = [bundle bundleIdentifier];

    if (paneLabel)
      [cache setObject:paneLabel forKey:CacheInfoPaneLabelKey];
  }
  return paneLabel;
}

- (id)currentPane
{
  return [mLoadedPanes objectForKey:mCurrentPaneIdentifier];
}

@end

// When using the Font Panel without an obvious target (like an editable text field), 
// changeFont action messages propagate all the way up the responder chain to here.
// There isn't an easy way for individual prefs panes to get themselves into
// the responder chain, so we just bounce this message back to the currently
// loaded pane.
@implementation MVPreferencesController (FontChangeNotifications)

- (void)changeFont:(id)sender
{
  if ([[self currentPane] respondsToSelector:@selector(changeFont:)])
    [[self currentPane] changeFont:sender];
}

- (BOOL)fontManager:(id)theFontManager willIncludeFont:(NSString *)fontName
{
  if ([[self currentPane] respondsToSelector:@selector(fontManager:willIncludeFont:)])
    return [[self currentPane] fontManager:theFontManager willIncludeFont:fontName];

  return YES;
}

- (unsigned int)validModesForFontPanel:(NSFontPanel *)fontPanel
{
  if ([[self currentPane] respondsToSelector:@selector(validModesForFontPanel:)])
    return [[self currentPane] validModesForFontPanel:fontPanel];

  return 0xFFFF;  // NSFontPanelStandardModesMask
}

@end

