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

@interface NSToolbar (NSToolbarPrivate)

- (NSView *) _toolbarView;

@end

#pragma mark -

@interface MVPreferencesController (MVPreferencesControllerPrivate)

- (void) doUnselect:(NSNotification *) notification;
- (IBAction)selectPreferencePane:(id) sender;
- (void)resizeWindowForContentView:(NSView *) view;
- (NSImage *)imageForPaneBundle:(NSBundle *) bundle;
- (NSString *)labelForPaneBundle:(NSBundle *) bundle;
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
    NSString* paneBundlesPath = [NSString stringWithFormat:@"%@/Contents/PreferencePanes", [[NSBundle mainBundle] bundlePath]];
    NSArray* paneFiles = [[NSFileManager defaultManager] directoryContentsAtPath:paneBundlesPath];

    mPanes = [[NSMutableArray alloc] initWithCapacity:[paneFiles count]];

    NSEnumerator* panesEnum = [paneFiles objectEnumerator];
    NSString* curPath;
    while ((curPath = [panesEnum nextObject]))
    {
      NSBundle* curBundle = [NSBundle bundleWithPath:[paneBundlesPath stringByAppendingPathComponent:curPath]];
      if ([curBundle load])
        [mPanes addObject:curBundle];    // retains
    }

    mLoadedPanes = [[NSMutableDictionary dictionary] retain];
    mPaneInfo = [[NSMutableDictionary dictionary] retain];
    [NSBundle loadNibNamed:@"MVPreferences" owner:self];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(doUnselect:) name:NSPreferencePaneDoUnselectNotification object:nil];
  }
  return self;
}

- (void) dealloc
{
  [mLoadedPanes autorelease];
  [mPanes autorelease];
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
  [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];\
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
    [mLoadingImageView setImage:[self imageForPaneBundle:bundle]];
    [mLoadingTextFeld setStringValue:[NSString stringWithFormat:NSLocalizedString( @"Loading %@...", nil ),
        [self labelForPaneBundle:bundle]]];
    
    [mWindow setTitle:[self labelForPaneBundle:bundle]];
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
        [self labelForPaneBundle:bundle]], nil, nil, nil );
    }
  }
}

- (BOOL) windowShouldClose:(id) sender
{
  if ( mCurrentPaneIdentifier && [[self currentPane] shouldUnselect] != NSUnselectNow )   	{
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
    prefService->SavePrefFile(nsnull);			// nsnull means write to prefs.js
  [[NSUserDefaults standardUserDefaults] synchronize];

  // tell gecko that this window no longer needs it around.
  CHBrowserService::BrowserClosed();
}

- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
  if ([[self currentPane] respondsToSelector:@selector(didActivate)])
    [[self currentPane] didActivate];
}

#pragma mark -

- (NSToolbarItem *) toolbar:(NSToolbar *) toolbar
    itemForItemIdentifier:(NSString *) itemIdentifier
    willBeInsertedIntoToolbar:(BOOL) flag
{
  NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier] autorelease];
  NSBundle *bundle = [NSBundle bundleWithIdentifier:itemIdentifier];
  if (bundle) {
    [toolbarItem setLabel:[self labelForPaneBundle:bundle]];
    [toolbarItem setImage:[self imageForPaneBundle:bundle]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector( selectPreferencePane: )];
  } else toolbarItem = nil;
  return toolbarItem;
}

- (NSArray *) toolbarDefaultItemIdentifiers:(NSToolbar *) toolbar
{
  return [NSArray arrayWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"MVPreferencePaneDefaults" ofType:@"plist"]];
}

- (NSArray *) toolbarAllowedItemIdentifiers:(NSToolbar *) toolbar
{
  NSMutableArray *items = [NSMutableArray array];
  NSEnumerator *enumerator = [mPanes objectEnumerator];
  id item = nil;
  while ( ( item = [enumerator nextObject] ) )
    [items addObject:[item bundleIdentifier]];
  return items;
}

// For OS X 10.3, set the selectable toolbar items (draws a gray rect around the active icon in the toolbar)
- (NSArray *) toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
  NSMutableArray* items = [NSMutableArray array];
  NSEnumerator* enumerator = [mPanes objectEnumerator];
  id item = nil;
  while ( ( item = [enumerator nextObject] ) )
    [items addObject:[item bundleIdentifier]];
  return items;
}

@end

#pragma mark -

@implementation MVPreferencesController (MVPreferencesControllerPrivate)

- (IBAction)selectPreferencePane:(id) sender
{
  [self selectPreferencePaneByIdentifier:[sender itemIdentifier]];
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

- (NSImage *)imageForPaneBundle:(NSBundle *)bundle
{
  NSString*             paneIdentifier = [bundle bundleIdentifier];
  NSMutableDictionary*  cache = [mPaneInfo objectForKey:paneIdentifier];
  // first, make sure we have a cache dict
  if (!cache)
  {
    cache = [NSMutableDictionary dictionary];
    [mPaneInfo setObject:cache forKey:paneIdentifier];
  }
  
  NSImage* paneImage = [cache objectForKey:@"MVPreferencePaneImage"];
  if (!paneImage)
  {
    NSDictionary *info = [bundle infoDictionary];
    // try to get the icon for the bundle
    paneImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:[info objectForKey:@"NSPrefPaneIconFile"]]];
    // if that failed, fall back on a generic icon
    if (!paneImage )
      paneImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:[info objectForKey:@"CFBundleIconFile"]]];

    if (paneImage)
    {
      [cache setObject:paneImage forKey:@"MVPreferencePaneImage"];
      [paneImage release];
    }
  }
  return paneImage;
}

- (NSString *)labelForPaneBundle:(NSBundle *)bundle
{
  NSString*             paneIdentifier = [bundle bundleIdentifier];
  NSMutableDictionary*  cache = [mPaneInfo objectForKey:[bundle bundleIdentifier]];
  // first, make sure we have a cache dict
  if (!cache)
  {
    cache = [NSMutableDictionary dictionary];
    [mPaneInfo setObject:cache forKey:paneIdentifier];
  }
  
  NSString* paneLabel = [cache objectForKey:@"MVPreferencePaneLabel"];
  if (!paneLabel)
  {
    NSDictionary *info = [bundle infoDictionary];
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
      [cache setObject:paneLabel forKey:@"MVPreferencePaneLabel"];
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
  [[self currentPane] changeFont:sender];
}

@end

