#import "WebFeatures.h"
#import "NSString+Utils.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPermissionManager.h"
#include "nsIPermission.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

@implementation OrgMozillaChimeraPreferenceWebFeatures

- (void) dealloc
{
  NS_IF_RELEASE(mManager);
  [super dealloc];
}


- (void)mainViewDidLoad
{
  if ( !mPrefService )
    return;

  BOOL gotPref = NO;
    
  // Set initial value on Java/JavaScript checkboxes
  BOOL jsEnabled = [self getBooleanPref:"javascript.enabled" withSuccess:&gotPref] && gotPref;
  [mEnableJS setState:jsEnabled];
  BOOL javaEnabled = [self getBooleanPref:"security.enable_java" withSuccess:&gotPref] && gotPref;
  [mEnableJava setState:javaEnabled];
  BOOL pluginsEnabled = [self getBooleanPref:"chimera.enable_plugins" withSuccess:&gotPref] && gotPref;
  [mEnablePlugins setState:pluginsEnabled];

  // set initial value on popup blocking checkbox and disable the whitelist
  // button if it's off
  BOOL enablePopupBlocking = [self getBooleanPref:"dom.disable_open_during_load" withSuccess:&gotPref] && gotPref;  
  [mEnablePopupBlocking setState:enablePopupBlocking];
  [mEditWhitelist setEnabled:enablePopupBlocking];
  
  // set initial value on annoyance blocker checkbox. out of all the prefs,
  // if the "status" capability is turned off, we use that as an indicator
  // to turn them all off.
  BOOL enableAnnoyanceBlocker = [self getBooleanPref:"dom.disable_window_status_change" withSuccess:&gotPref];
  [mEnableAnnoyanceBlocker setState:enableAnnoyanceBlocker];
  
  // set initial value on image-resizing
  BOOL enableImageResize = [self getBooleanPref:"browser.enable_automatic_image_resizing" withSuccess:&gotPref];
  [mImageResize setState:enableImageResize];
  
  // store permission manager service and cache the enumerator.
  nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );
  mManager = pm.get();
  NS_IF_ADDREF(mManager);
}


//
// clickEnableJS
//
// Set pref if JavaScript is enabled
//
-(IBAction) clickEnableJS:(id)sender
{
  [self setPref:"javascript.enabled" toBoolean:[sender state] == NSOnState];
}

//
// clickEnableJava
//
// Set pref if Java is enabled
//
-(IBAction) clickEnableJava:(id)sender
{
  [self setPref:"security.enable_java" toBoolean:[sender state] == NSOnState];
}

//
// clickEnablePlugins
//
// Set pref if plugins are enabled
//
-(IBAction) clickEnablePlugins:(id)sender
{
  [self setPref:"chimera.enable_plugins" toBoolean:[sender state] == NSOnState];
}

//
// clickEnablePopupBlocking
//
// Enable and disable mozilla's popup blocking feature. We use a combination of 
// two prefs to suppress bad popups.
//
- (IBAction)clickEnablePopupBlocking:(id)sender
{
  if ( [sender state] )
    [self setPref:"dom.disable_open_during_load" toBoolean: YES];
  else
    [self setPref:"dom.disable_open_during_load" toBoolean: NO];
  [mEditWhitelist setEnabled:[sender state]];
}

//
// clickEnableImageResizing:
//
// Enable and disable mozilla's auto image resizing feature.
//
-(IBAction) clickEnableImageResizing:(id)sender
{
  [self setPref:"browser.enable_automatic_image_resizing" toBoolean:[sender state] ? YES : NO];
}

//
// populatePermissionCache:
//
// walks the permission list for popups building up a cache that
// we can quickly refer to later.
//
-(void) populatePermissionCache:(nsISupportsArray*)inPermissions
{
  nsCOMPtr<nsISimpleEnumerator> permEnum;
  if ( mManager ) 
    mManager->GetEnumerator(getter_AddRefs(permEnum));

  if ( inPermissions && permEnum ) {
    PRBool hasMoreElements = PR_FALSE;
    permEnum->HasMoreElements(&hasMoreElements);
    while ( hasMoreElements ) {
      nsCOMPtr<nsISupports> curr;
      permEnum->GetNext(getter_AddRefs(curr));
      nsCOMPtr<nsIPermission> currPerm(do_QueryInterface(curr));
      if ( currPerm ) {
        nsCAutoString type;
        currPerm->GetType(type);
        if ( type.Equals(NS_LITERAL_CSTRING("popup")) )
          inPermissions->AppendElement(curr);
      }
      permEnum->HasMoreElements(&hasMoreElements);
    }
  }
}

//
// editWhitelist:
//
// put up a sheet to allow people to edit the popup blocker whitelist
//
-(IBAction) editWhitelist:(id)sender
{
  // build parallel permission list for speed with a lot of blocked sites
  NS_NewISupportsArray(&mCachedPermissions);     // ADDREFs
  [self populatePermissionCache:mCachedPermissions];
  
	[NSApp beginSheet:mWhitelistPanel
        modalForWindow:[mEditWhitelist window]   // any old window accessor
        modalDelegate:self
        didEndSelector:@selector(editWhitelistSheetDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
        
  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mWhitelistTable selectRow:0 byExtendingSelection:NO];
  
  [mWhitelistTable setDeleteAction:@selector(removeWhitelistSite:)];
  [mWhitelistTable setTarget:self];
  
  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mWhitelistTable reloadData];
}

// whitelist sheet methods
-(IBAction) editWhitelistDone:(id)aSender
{
  // save stuff??
  
  [mWhitelistPanel orderOut:self];
  [NSApp endSheet:mWhitelistPanel];
  
  NS_IF_RELEASE(mCachedPermissions);
}

-(IBAction) removeWhitelistSite:(id)aSender
{
  if ( mCachedPermissions && mManager ) {
    // remove from parallel array and cookie permissions list
    int row = [mWhitelistTable selectedRow];
    
    // remove from permission manager (which is done by host, not by row), then 
    // remove it from our parallel array (which is done by row). Since we keep a
    // parallel array, removing multiple items by row is very difficult since after 
    // deleting, the array is out of sync with the next cocoa row we're told to remove. Punt!
    nsCOMPtr<nsISupports> rowItem = dont_AddRef(mCachedPermissions->ElementAt(row));
    nsCOMPtr<nsIPermission> perm ( do_QueryInterface(rowItem) );
    if ( perm ) {
      nsCAutoString host;
      perm->GetHost(host);
      mManager->Remove(host, "popup");           // could this api _be_ any worse? Come on!
      
      mCachedPermissions->RemoveElementAt(row);
    }
    [mWhitelistTable reloadData];
  }
}

//
// addWhitelistSite:
//
// adds a new site to the permission manager whitelist for popups
//
-(IBAction) addWhitelistSite:(id)sender
{
  if ( mCachedPermissions && mManager ) {
    // ensure url has a http:// on the front or NS_NewURI will fail. The PM
    // really doesn't care what the protocol is, we just need to have something
    NSString* url = [[mAddField stringValue] stringByRemovingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ( ![url rangeOfString:@"http://"].length && ![url rangeOfString:@"https://"].length )
      url = [NSString stringWithFormat:@"http://%@", url];
    
    const char* siteURL = [url cString];
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI), siteURL);
    if ( newURI ) {
      mManager->Add(newURI, "popup", nsIPermissionManager::ALLOW_ACTION);
      mCachedPermissions->Clear();
      [self populatePermissionCache:mCachedPermissions];

      [mAddField setStringValue:@""];      
      [mWhitelistTable reloadData];
    }
  }
}

- (void) editWhitelistSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{

}

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
  PRUint32 numRows = 0;
  if ( mCachedPermissions )
    mCachedPermissions->Count(&numRows);

  return (int) numRows;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  NSString* retVal = nil;
  if ( mCachedPermissions ) {
    nsCOMPtr<nsISupports> rowItem = dont_AddRef(mCachedPermissions->ElementAt(rowIndex));
    nsCOMPtr<nsIPermission> perm ( do_QueryInterface(rowItem) );
    if ( perm ) {
      // only 1 column and it's the website url column
      nsCAutoString host;
      perm->GetHost(host);
      retVal = [NSString stringWithCString:host.get()];
    }
  }
  
  return retVal;
}

//
// clickEnableAnnoyanceBlocker:
//
// Enable and disable prefs for allowing webpages to be annoying and move/resize the
// window or tweak the status bar and make it unusable.
//
-(IBAction) clickEnableAnnoyanceBlocker:(id)sender
{
  if ( [sender state] ) 
    [self setAnnoyingWindowPrefsTo:YES];
  else
    [self setAnnoyingWindowPrefsTo:NO];
}

//
// setAnnoyingWindowPrefsTo:
//
// Set all the prefs that allow webpages to muck with the status bar and window position
// (ie, be really annoying) to the given value
//
-(void) setAnnoyingWindowPrefsTo:(BOOL)inValue
{
    [self setPref:"dom.disable_window_move_resize" toBoolean:inValue];
    [self setPref:"dom.disable_window_status_change" toBoolean:inValue];
    [self setPref:"dom.disable_window_flip" toBoolean:inValue];
}
@end
