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

#import <PreferencePanes/PreferencePanes.h>
#import "MVPreferencesController.h"
#import "MVPreferencesMultipleIconView.h"
#import "MVPreferencesGroupedIconView.h"
#import "ToolbarAdditions.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "CHBrowserService.h"

// #import "Defines.h"

static MVPreferencesController *sharedInstance = nil;

NSString *MVToolbarShowAllItemIdentifier = @"MVToolbarShowAllItem";
NSString *MVPreferencesWindowNotification = @"MVPreferencesWindowNotification";

@interface NSToolbar (NSToolbarPrivate)
- (NSView *) _toolbarView;
@end

@interface MVPreferencesController (MVPreferencesControllerPrivate)
- (void) _doUnselect:(NSNotification *) notification;
- (IBAction) _selectPreferencePane:(id) sender;
- (void) _resizeWindowForContentView:(NSView *) view;
- (NSImage *) _imageForPaneBundle:(NSBundle *) bundle;
- (NSString *) _paletteLabelForPaneBundle:(NSBundle *) bundle;
- (NSString *) _labelForPaneBundle:(NSBundle *) bundle;
@end

@implementation MVPreferencesController

+ (MVPreferencesController *) sharedInstance
{
  return ( sharedInstance ? sharedInstance : [[[self alloc] init] autorelease] );
}

- (id) init
{
  if ( ( self = [super init] ) ) {
    unsigned i = 0;
    NSBundle *bundle = nil;
    NSString *bundlePath = [NSString stringWithFormat:@"%@/Contents/PreferencePanes",
        [[NSBundle mainBundle] bundlePath]];
    panes = [[[NSFileManager defaultManager] directoryContentsAtPath:bundlePath] mutableCopy];
    for ( i = 0; i < [panes count]; i++ ) {
      bundle = [NSBundle bundleWithPath:[NSString stringWithFormat:@"%@/%@", bundlePath, [panes objectAtIndex:i]]];
      [bundle load];
      if( bundle ) [panes replaceObjectAtIndex:i withObject:bundle];
      else {
        [panes removeObjectAtIndex:i];
        i--;
      }
    }
    loadedPanes = [[NSMutableDictionary dictionary] retain];
    paneInfo = [[NSMutableDictionary dictionary] retain];
    [NSBundle loadNibNamed:@"MVPreferences" owner:self];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector( _doUnselect: ) name:NSPreferencePaneDoUnselectNotification object:nil];
  }
  return self;
}

- (void) dealloc
{
  [loadedPanes autorelease];
  [panes autorelease];
  [paneInfo autorelease];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  loadedPanes = nil;
  panes = nil;
  paneInfo = nil;
  
  [super dealloc];
}

- (void) awakeFromNib
{
  NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:@"preferences.toolbar"] autorelease];
  NSArray *groups = [NSArray arrayWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"MVPreferencePaneGroups" ofType:@"plist"]];

  if( groups ) {
    [groupView setPreferencePanes:panes];
    [groupView setPreferencePaneGroups:groups];
    mainView = groupView;
  } else {
    [multiView setPreferencePanes:panes];
    mainView = multiView;
  }

  [window setDelegate:self];

  [toolbar setAllowsUserCustomization:YES];
  [toolbar setAutosavesConfiguration:YES];
  [toolbar setDelegate:self];
  [toolbar setAlwaysCustomizableByDrag:YES];
  [toolbar setShowsContextMenu:NO];
  [window setToolbar:toolbar];
  [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
  [toolbar setIndexOfFirstMovableItem:2];
}

- (NSWindow *) window
{
  return [[window retain] autorelease];
}

- (IBAction) showPreferences:(id) sender
{
  if ( ![window isVisible] ) {
    // on being shown, register as a window that cares about XPCOM being active until we're done
    // with it in |windowDidClose()|. Need to ensure this is exactly balanced with |BrowserClosed()|
    // calls as it increments a refcount. As a result, we can only call it when we're making
    // the window visible. Too bad cocoa doesn't give us any notifications of this.
    CHBrowserService::InitEmbedding();
  }
  [self showAll:nil];
  [window makeKeyAndOrderFront:nil];
}

- (IBAction) showAll:(id) sender
{
  if ( [[window contentView] isEqual:mainView] ) return;
  if ( currentPaneIdentifier && [[loadedPanes objectForKey:currentPaneIdentifier] shouldUnselect] != NSUnselectNow ) {
    /* more to handle later */
#if DEBUG
    NSLog(@"can't unselect current preferences pane");
#endif
    return;
  }
  [window setContentView:[[[NSView alloc] initWithFrame:[mainView frame]] autorelease]];

  [window setTitle:[NSString stringWithFormat:NSLocalizedString( @"PrefsWindowTitleFormat", @"" ), [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"]]];
  [self _resizeWindowForContentView:mainView];

  [[loadedPanes objectForKey:currentPaneIdentifier] willUnselect];
  [window setContentView:mainView];
  [[loadedPanes objectForKey:currentPaneIdentifier] didUnselect];

  [currentPaneIdentifier autorelease];
  currentPaneIdentifier = nil;

  [window setInitialFirstResponder:mainView];
  [window makeFirstResponder:mainView];
  if ([NSToolbar instancesRespondToSelector:@selector(setSelectedItemIdentifier:)])
    [[window toolbar] setSelectedItemIdentifier:MVToolbarShowAllItemIdentifier];
}

- (void) selectPreferencePaneByIdentifier:(NSString *) identifier
{
  NSBundle *bundle = [NSBundle bundleWithIdentifier:identifier];
  
  if ( bundle && ! [currentPaneIdentifier isEqualToString:identifier] ) {
    NSPreferencePane *pane = nil;
    NSView *prefView = nil;
    if ( currentPaneIdentifier &&
        [[loadedPanes objectForKey:currentPaneIdentifier] shouldUnselect] != NSUnselectNow ) {
      /* more to handle later */
#if DEBUG
      NSLog( @"can't unselect current" );
#endif
      closeWhenPaneIsReady = NO;
      [pendingPane autorelease];
      pendingPane = [identifier retain];
      return;
    }
    
    [pendingPane autorelease];
    pendingPane = nil;
    [loadingImageView setImage:[self _imageForPaneBundle:bundle]];
    [loadingTextFeld setStringValue:[NSString stringWithFormat:NSLocalizedString( @"Loading %@...", nil ),
        [self _labelForPaneBundle:bundle]]];
    
    [window setTitle:[self _labelForPaneBundle:bundle]];
    [window setContentView:loadingView];
    [window display];
    
    if ( ! ( pane = [loadedPanes objectForKey:identifier] ) ) {
      pane = [[[[bundle principalClass] alloc] initWithBundle:bundle] autorelease];
      if( pane ) [loadedPanes setObject:pane forKey:identifier];
    }
    
    if ( [pane loadMainView] ) {
      [pane willSelect];
      prefView = [pane mainView];

      [self _resizeWindowForContentView:prefView];

      [[loadedPanes objectForKey:currentPaneIdentifier] willUnselect];
      [window setContentView:prefView];
      [[loadedPanes objectForKey:currentPaneIdentifier] didUnselect];
      [pane didSelect];
      [[NSNotificationCenter defaultCenter]
        postNotificationName: MVPreferencesWindowNotification
        object: self
        userInfo: [NSDictionary dictionaryWithObjectsAndKeys:window, @"window", nil]];
      [currentPaneIdentifier autorelease];
      currentPaneIdentifier = [identifier copy];

      [window setInitialFirstResponder:[pane initialKeyView]];
      [window makeFirstResponder:[pane initialKeyView]];
      if ([NSToolbar instancesRespondToSelector:@selector(setSelectedItemIdentifier:)])
        [[window toolbar] setSelectedItemIdentifier:currentPaneIdentifier];
    }
    else {
      NSRunCriticalAlertPanel( NSLocalizedString( @"Preferences Error", nil ),
        [NSString stringWithFormat:NSLocalizedString( @"Could not load %@", nil ),
        [self _labelForPaneBundle:bundle]], nil, nil, nil );
    }
  }
}

- (BOOL) windowShouldClose:(id) sender
{
  if ( currentPaneIdentifier && [[loadedPanes objectForKey:currentPaneIdentifier] shouldUnselect] != NSUnselectNow )   	{
#if DEBUG
    NSLog( @"can't unselect current" );
#endif
    closeWhenPaneIsReady = YES;
    return NO;
  }
  return YES;
}

- (void) windowWillClose:(NSNotification *) notification
{
  [[loadedPanes objectForKey:currentPaneIdentifier] willUnselect];
  [[loadedPanes objectForKey:currentPaneIdentifier] didUnselect];
  [currentPaneIdentifier autorelease];
  currentPaneIdentifier = nil;
  //[loadedPanes removeAllObjects];
  
  // write out prefs and user defaults
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, prefs unsaved");
  if ( prefService )
    prefService->SavePrefFile(nsnull);			// nsnull means write to prefs.js
  [[NSUserDefaults standardUserDefaults] synchronize];

  // tell gecko that this window no longer needs it around.
  CHBrowserService::BrowserClosed();
}

- (NSToolbarItem *) toolbar:(NSToolbar *) toolbar
    itemForItemIdentifier:(NSString *) itemIdentifier
    willBeInsertedIntoToolbar:(BOOL) flag
{
  NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier] autorelease];
  if ( [itemIdentifier isEqualToString:MVToolbarShowAllItemIdentifier] ) {
    [toolbarItem setLabel:NSLocalizedString( @"ShowAllPrefItems", @"" )];
    [toolbarItem setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
    [toolbarItem setTarget:self];
    [toolbarItem setAction:@selector( showAll: )];
  } else {
    NSBundle *bundle = [NSBundle bundleWithIdentifier:itemIdentifier];
    if( bundle ) {
      [toolbarItem setLabel:[self _labelForPaneBundle:bundle]];
      [toolbarItem setPaletteLabel:[self _paletteLabelForPaneBundle:bundle]];
      [toolbarItem setImage:[self _imageForPaneBundle:bundle]];
      [toolbarItem setTarget:self];
      [toolbarItem setAction:@selector( _selectPreferencePane: )];
    } else toolbarItem = nil;
  }
  return toolbarItem;
}

- (NSArray *) toolbarDefaultItemIdentifiers:(NSToolbar *) toolbar
{
  NSMutableArray *fixed = [NSMutableArray arrayWithObjects:MVToolbarShowAllItemIdentifier, NSToolbarSeparatorItemIdentifier, nil];
  NSArray *defaults = [NSArray arrayWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"MVPreferencePaneDefaults" ofType:@"plist"]];
  [fixed addObjectsFromArray:defaults];
  return fixed;
}

- (NSArray *) toolbarAllowedItemIdentifiers:(NSToolbar *) toolbar
{
  NSMutableArray *items = [NSMutableArray array];
  NSEnumerator *enumerator = [panes objectEnumerator];
  id item = nil;
  while ( ( item = [enumerator nextObject] ) )
    [items addObject:[item bundleIdentifier]];
  //[items addObject:MVToolbarShowAllItemIdentifier];
  [items addObject:NSToolbarSeparatorItemIdentifier];
  return items;
}

// For OS X 10.3, set the selectable toolbar items (draws a gray rect around the active icon in the toolbar)
- (NSArray *) toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
  NSMutableArray* items = [NSMutableArray array];
  NSEnumerator* enumerator = [panes objectEnumerator];
  id item = nil;
  while ( ( item = [enumerator nextObject] ) )
    [items addObject:[item bundleIdentifier]];
  [items addObject:MVToolbarShowAllItemIdentifier];
  return items;
}

@end

@implementation MVPreferencesController (MVPreferencesControllerPrivate)

- (IBAction) _selectPreferencePane:(id) sender
{
  [self selectPreferencePaneByIdentifier:[sender itemIdentifier]];
}

- (void) _doUnselect:(NSNotification *) notification
{
  if( closeWhenPaneIsReady ) [window close];
  [self selectPreferencePaneByIdentifier:pendingPane];
}

- (void) _resizeWindowForContentView:(NSView *) view
{
  NSRect windowFrame = [NSWindow contentRectForFrameRect:[window frame] styleMask:[window styleMask]];
  float  newWindowHeight = NSHeight( [view frame] );

  if ( [[window toolbar] isVisible] )
    newWindowHeight += NSHeight( [[[window toolbar] _toolbarView] frame] );
    
  NSRect newWindowFrame = [NSWindow frameRectForContentRect:NSMakeRect( NSMinX( windowFrame ), NSMaxY( windowFrame ) - newWindowHeight, NSWidth( windowFrame ), newWindowHeight ) styleMask:[window styleMask]];
  [window setFrame:newWindowFrame display:YES animate:[window isVisible]];
}

- (NSImage *) _imageForPaneBundle:(NSBundle *) bundle
{
  NSImage *image = nil;
  NSMutableDictionary *cache = [paneInfo objectForKey:[bundle bundleIdentifier]];
  image = [[[cache objectForKey:@"MVPreferencePaneImage"] retain] autorelease];
  if ( ! image ) {
    NSDictionary *info = [bundle infoDictionary];
    image = [[[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:[info objectForKey:@"NSPrefPaneIconFile"]]] autorelease];
    if ( ! image ) image = [[[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:[info objectForKey:@"CFBundleIconFile"]]] autorelease];
    if ( ! cache ) [paneInfo setObject:[NSMutableDictionary dictionary] forKey:[bundle bundleIdentifier]];
    cache = [paneInfo objectForKey:[bundle bundleIdentifier]];
    if ( image ) [cache setObject:image forKey:@"MVPreferencePaneImage"];
  }
  return image;
}

- (NSString *) _paletteLabelForPaneBundle:(NSBundle *) bundle
{
  NSString *label = nil;
  NSMutableDictionary *cache = [paneInfo objectForKey:[bundle bundleIdentifier]];
  label = [[[cache objectForKey:@"MVPreferencePanePaletteLabel"] retain] autorelease];
  if ( ! label ) {
    NSDictionary *info = [bundle infoDictionary];
    label = NSLocalizedStringFromTableInBundle(@"PreferencePanelLabel", nil, bundle, nil);
    if ( [label isEqualToString:@"PreferencePanelLabel"] ) label = NSLocalizedStringFromTableInBundle( @"NSPrefPaneIconLabel", @"InfoPlist", bundle, nil );
    if ( [label isEqualToString:@"NSPrefPaneIconLabel"] ) label = [info objectForKey:@"NSPrefPaneIconLabel"];
    if ( ! label ) label = NSLocalizedStringFromTableInBundle( @"CFBundleName", @"InfoPlist", bundle, nil );
    if ( [label isEqualToString:@"CFBundleName"] ) label = [info objectForKey:@"CFBundleName"];
    if ( ! label ) label = [bundle bundleIdentifier];
    if ( ! cache ) [paneInfo setObject:[NSMutableDictionary dictionary] forKey:[bundle bundleIdentifier]];
    cache = [paneInfo objectForKey:[bundle bundleIdentifier]];
    if ( label ) [cache setObject:label forKey:@"MVPreferencePanePaletteLabel"];
  }
  return label;
}

- (NSString *) _labelForPaneBundle:(NSBundle *) bundle
{
  NSString *label = nil;
  NSMutableDictionary *cache = [paneInfo objectForKey:[bundle bundleIdentifier]];
  label = [[[cache objectForKey:@"MVPreferencePaneLabel"] retain] autorelease];
  if ( ! label ) {
    NSDictionary *info = [bundle infoDictionary];
    label = NSLocalizedStringFromTableInBundle(@"PreferencePanelLabel", nil, bundle, nil);
    if ( [label isEqualToString:@"PreferencePanelLabel"] ) label = NSLocalizedStringFromTableInBundle( @"CFBundleName", @"InfoPlist", bundle, nil );
    if ( [label isEqualToString:@"CFBundleName"] ) label = [info objectForKey:@"CFBundleName"];
    if ( ! label ) label = [bundle bundleIdentifier];
    if ( ! cache ) [paneInfo setObject:[NSMutableDictionary dictionary] forKey:[bundle bundleIdentifier]];
    cache = [paneInfo objectForKey:[bundle bundleIdentifier]];
    if ( label ) [cache setObject:label forKey:@"MVPreferencePaneLabel"];
  }
  return label;
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
  [[loadedPanes objectForKey:currentPaneIdentifier] changeFont:sender];
}

@end

