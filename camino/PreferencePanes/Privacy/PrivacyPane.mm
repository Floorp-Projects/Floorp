#import "PrivacyPane.h"
#import "NSString+Utils.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsNetCID.h"
#include "nsICookie.h"
#include "nsICookieManager.h"
#include "nsIPermissionManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIPermission.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsString.h"


// prefs for keychain password autofill
static const char* const gUseKeychainPref = "chimera.store_passwords_with_keychain";
static const char* const gAutoFillEnabledPref = "chimera.keychain_passwords_autofill";

// network.cookie.cookieBehavior settings
// these are the defaults, overriden by whitelist/blacklist
const int kAcceptAllCookies = 0;
const int kAcceptCookiesFromOriginatingServer = 1;
const int kDenyAllCookies = 2;

// network.cookie.lifetimePolicy settings
const int kAcceptCookiesNormally = 0;
const int kWarnAboutCookies = 1;

// popup indices
const int kAllowIndex = 0;
const int kDenyIndex = 1;

// callbacks for sorting the permission list
PR_STATIC_CALLBACK(int) comparePermHosts(nsIPermission* aPerm1, nsIPermission* aPerm2, void* aData)
{
  nsCAutoString host1;
  aPerm1->GetHost(host1);
  nsCAutoString host2;
  aPerm2->GetHost(host2);
  
  return Compare(host1, host2);
}

PR_STATIC_CALLBACK(int) compareCapabilities(nsIPermission* aPerm1, nsIPermission* aPerm2, void* aData)
{
  PRUint32 cap1 = 0;
  aPerm1->GetCapability(&cap1);
  PRUint32 cap2 = 0;
  aPerm2->GetCapability(&cap2);
  
  if(cap1 == cap2)
    return comparePermHosts(aPerm1, aPerm2, aData);
  
  return (cap1 < cap2) ? -1 : 1;
}

PR_STATIC_CALLBACK(int) compareCookieHosts(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString host1;
  aCookie1->GetHost(host1);
  nsCAutoString host2;
  aCookie2->GetHost(host2);
  return Compare(host1, host2);
}

PR_STATIC_CALLBACK(int) compareNames(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString name1;
  aCookie1->GetName(name1);
  nsCAutoString name2;
  aCookie2->GetName(name2);
  return Compare(name1, name2);
}

PR_STATIC_CALLBACK(int) comparePaths(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString path1;
  aCookie1->GetPath(path1);
  nsCAutoString path2;
  aCookie2->GetPath(path2);
  return Compare(path1, path2);
}

PR_STATIC_CALLBACK(int) compareSecures(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  PRBool secure1;
  aCookie1->GetIsSecure(&secure1);
  PRBool secure2;
  aCookie2->GetIsSecure(&secure2);
  if (secure1 == secure2)
    return -1;
  return (secure1) ? -1 : 1;
}

PR_STATIC_CALLBACK(int) compareExpires(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  PRUint64 expires1;
  aCookie1->GetExpires(&expires1);
  PRUint64 expires2;
  aCookie2->GetExpires(&expires2);
  return (expires1 < expires2) ? -1 : 1;
}

PR_STATIC_CALLBACK(int) compareValues(nsICookie* aCookie1, nsICookie* aCookie2, void* aData)
{
  nsCAutoString value1;
  aCookie1->GetValue(value1);
  nsCAutoString value2;
  aCookie2->GetValue(value2);
  return Compare(value1, value2);
}

@implementation OrgMozillaChimeraPreferencePrivacy

-(void) dealloc
{
  // NOTE: no need to worry about mCachedPermissions or mCachedCookies because if we're going away
  // the respective sheets have closed and cleaned up.
  
  NS_IF_RELEASE(mPermissionManager);
  NS_IF_RELEASE(mCookieManager);
  [super dealloc];
}

-(void) mainViewDidLoad
{
  if ( !mPrefService )
    return;
    
  // Hookup cookie prefs.
  PRInt32 acceptCookies = kAcceptAllCookies;
  mPrefService->GetIntPref("network.cookie.cookieBehavior", &acceptCookies);
  [self mapCookiePrefToGUI: acceptCookies];
  // lifetimePolicy now controls asking about cookies, despite being totally unintuitive
  PRInt32 lifetimePolicy = kAcceptCookiesNormally;
  mPrefService->GetIntPref("network.cookie.lifetimePolicy", &lifetimePolicy);
  if ( lifetimePolicy == kWarnAboutCookies )
    [mAskAboutCookies setState:YES];
  
  // store permission manager service and cache the enumerator.
  nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );
  mPermissionManager = pm.get();
  NS_IF_ADDREF(mPermissionManager);

  // store cookie manager service
  nsCOMPtr<nsICookieManager> cm ( do_GetService(NS_COOKIEMANAGER_CONTRACTID) );
  mCookieManager = cm.get();
  NS_IF_ADDREF(mCookieManager);
  
  // Keychain checkboxes
  PRBool storePasswords = PR_TRUE;
  mPrefService->GetBoolPref(gUseKeychainPref, &storePasswords);
  [mStorePasswords setState:(storePasswords ? NSOnState : NSOffState)];
  
  PRBool autoFillPasswords = PR_TRUE;
  mPrefService->GetBoolPref(gAutoFillEnabledPref, &autoFillPasswords);
  [mAutoFillPasswords setState:(autoFillPasswords ? NSOnState : NSOffState)];
  
  // setup allow/deny table popups
  NSPopUpButtonCell *popupButtonCell = [mPermissionColumn dataCell];
  [popupButtonCell setEditable:YES];
  [popupButtonCell addItemsWithTitles:[NSArray arrayWithObjects:[self getLocalizedString:@"Allow"], [self getLocalizedString:@"Deny"], nil]];
}

-(void) mapCookiePrefToGUI: (int)pref
{
  [mCookieBehavior selectCellWithTag:pref];
  [mAskAboutCookies setEnabled:(pref == kAcceptAllCookies || pref == kAcceptCookiesFromOriginatingServer)];
}

//
// Stored cookie editing methods
//

-(void) populateCookieCache
{
  nsCOMPtr<nsISimpleEnumerator> cookieEnum;
  if ( mCookieManager )
    mCookieManager->GetEnumerator(getter_AddRefs(cookieEnum));
  
  mCachedCookies = new nsCOMArray<nsICookie>;
  if ( mCachedCookies && cookieEnum ) {
    mCachedCookies->Clear();
    PRBool hasMoreElements = PR_FALSE;
    cookieEnum->HasMoreElements(&hasMoreElements);
    while ( hasMoreElements ) {
      nsCOMPtr<nsICookie> cookie;
      cookieEnum->GetNext(getter_AddRefs(cookie));
      mCachedCookies->AppendObject(cookie);
      cookieEnum->HasMoreElements(&hasMoreElements);
    }
  }
}

-(IBAction) editCookies:(id)aSender
{
  // build parallel cookie list
  [self populateCookieCache];
  
  mCachedCookies->Sort(compareCookieHosts, nsnull);
  
  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mCookiesTable selectRow: 0 byExtendingSelection: NO];
  [mCookiesTable setHighlightedTableColumn:[mCookiesTable tableColumnWithIdentifier:@"Website"]];
  
  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mCookiesTable reloadData];
  
  // bring up sheet
  [NSApp beginSheet:mCookiesPanel
     modalForWindow:[mAskAboutCookies window]   // any old window accessor
      modalDelegate:self
     didEndSelector:NULL
        contextInfo:NULL];
  NSSize min = {440, 240};
  [mCookiesPanel setMinSize:min];
}

-(IBAction) removeCookies:(id)aSender
{
  if (mCachedCookies && mCookieManager) {
    NSArray *rows = [[mCookiesTable selectedRowEnumerator] allObjects];
    NSEnumerator *e = [rows reverseObjectEnumerator];
    NSNumber *index;
    while (index = [e nextObject])
    {
      int row = [index intValue];
      nsCAutoString host, name, path;
      mCachedCookies->ObjectAt(row)->GetHost(host);
      mCachedCookies->ObjectAt(row)->GetName(name);
      mCachedCookies->ObjectAt(row)->GetPath(path);
      mCookieManager->Remove(host, name, path, PR_FALSE);  // don't block permanently
      mCachedCookies->RemoveObjectAt(row);
    }
  }
  [mCookiesTable deselectAll: self];   // don't want any traces of previous selection
  [mCookiesTable reloadData];
}

-(IBAction) removeAllCookies: (id)aSender
{
  if ( mCookieManager ) {
    // remove all cookies from cookie manager
    mCookieManager->RemoveAll();
    // create new cookie cache
    delete mCachedCookies;
    mCachedCookies = nsnull;
    mCachedCookies = new nsCOMArray<nsICookie>;
  }
  [mCookiesTable reloadData];
}

-(IBAction) editCookiesDone:(id)aSender
{
  // save stuff
  [mCookiesPanel orderOut:self];
  [NSApp endSheet:mCookiesPanel];

  delete mCachedCookies;
  mCachedCookies = nsnull;
}

//
// Site permission editing methods

-(void) populatePermissionCache
{
  nsCOMPtr<nsISimpleEnumerator> permEnum;
  if ( mPermissionManager ) 
    mPermissionManager->GetEnumerator(getter_AddRefs(permEnum));
  
  mCachedPermissions = new nsCOMArray<nsIPermission>;
  if ( mCachedPermissions && permEnum ) {
    mCachedPermissions->Clear();
    PRBool hasMoreElements = PR_FALSE;
    permEnum->HasMoreElements(&hasMoreElements);
    while ( hasMoreElements ) {
      nsCOMPtr<nsISupports> curr;
      permEnum->GetNext(getter_AddRefs(curr));
      nsCOMPtr<nsIPermission> currPerm(do_QueryInterface(curr));
      if ( currPerm ) {
        nsCAutoString type;
        currPerm->GetType(type);
        if ( type.Equals(NS_LITERAL_CSTRING("cookie")) )
          mCachedPermissions->AppendObject(currPerm);
      }
      permEnum->HasMoreElements(&hasMoreElements);
    }
  }
}

-(IBAction) editPermissions:(id)aSender
{
  // build parallel permission list for speed with a lot of blocked sites
  [self populatePermissionCache];
  
  mCachedPermissions->Sort(comparePermHosts, nsnull);
  
  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mPermissionsTable selectRow:0 byExtendingSelection:NO];
  [mPermissionsTable setHighlightedTableColumn:[mPermissionsTable tableColumnWithIdentifier:@"Website"]];
  
  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mPermissionsTable reloadData];
  
  // bring up sheet
  [NSApp beginSheet:mPermissionsPanel
     modalForWindow:[mAskAboutCookies window]   // any old window accessor
      modalDelegate:self
     didEndSelector:NULL
        contextInfo:NULL];
  NSSize min = {440, 240};
  [mPermissionsPanel setMinSize:min];
}

-(IBAction) removeCookiePermissions:(id)aSender
{
  if ( mCachedPermissions && mPermissionManager ) {
    // remove from parallel array and cookie permissions list
    NSArray *rows = [[mPermissionsTable selectedRowEnumerator] allObjects];
    NSEnumerator *e = [rows reverseObjectEnumerator];
    NSNumber *index;
    while (index = [e nextObject])  {
      int row = [index intValue];
      nsCAutoString host;
      mCachedPermissions->ObjectAt(row)->GetHost(host);
      mPermissionManager->Remove(host, "cookie");
      mCachedPermissions->RemoveObjectAt(row);
    }
  } 
  [mPermissionsTable deselectAll: self];   // don't want any traces of previous selection
  [mPermissionsTable reloadData];
}

-(IBAction) removeAllCookiePermissions: (id)aSender
{
  if ( mPermissionManager ) {
    // remove all permissions from permission manager
    mPermissionManager->RemoveAll();
    delete mCachedPermissions;
    mCachedPermissions = nsnull;
    mCachedPermissions = new nsCOMArray<nsIPermission>;
  }
  [mPermissionsTable reloadData];
}

-(IBAction) editPermissionsDone:(id)aSender
{
  // save stuff
  [mPermissionsPanel orderOut:self];
  [NSApp endSheet:mPermissionsPanel];
  
  delete mCachedPermissions;
  mCachedPermissions = nsnull;
}

//
// NSTableDataSource protocol methods
//

-(int) numberOfRowsInTableView:(NSTableView *)aTableView
{
  PRUint32 numRows = 0;
  if (aTableView == mPermissionsTable) {
    if ( mCachedPermissions )
      numRows = mCachedPermissions->Count();
  } else if (aTableView == mCookiesTable) {
    if ( mCachedCookies )
      numRows = mCachedCookies->Count();
  }

  return (int) numRows;
}

-(id) tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  NSString* retVal = nil;
  if (aTableView == mPermissionsTable) {
    if ( mCachedPermissions ) {
      if ( [[aTableColumn identifier] isEqualToString:@"Website"] ) {
        // website url column
        nsCAutoString host;
        mCachedPermissions->ObjectAt(rowIndex)->GetHost(host);
        retVal = [NSString stringWithUTF8String:host.get()];
      } else {
        // allow/deny column
        PRUint32 capability = PR_FALSE;
        mCachedPermissions->ObjectAt(rowIndex)->GetCapability(&capability);
        if ( capability == nsIPermissionManager::ALLOW_ACTION )
          return [NSNumber numberWithInt:kAllowIndex];   // special case return
        else
          return [NSNumber numberWithInt:kDenyIndex];    // special case return
      }
    }
  }
  else if (aTableView == mCookiesTable) {
    if ( mCachedCookies ) {
      nsCAutoString cookieVal;
      if ( [[aTableColumn identifier] isEqualToString: @"Website"] ) {
        mCachedCookies->ObjectAt(rowIndex)->GetHost(cookieVal);
      } else if ( [[aTableColumn identifier] isEqualToString: @"Name"] ) {
        mCachedCookies->ObjectAt(rowIndex)->GetName(cookieVal);
      } else if ( [[aTableColumn identifier] isEqualToString: @"Path"] ) {
        mCachedCookies->ObjectAt(rowIndex)->GetPath(cookieVal);
      } else if ( [[aTableColumn identifier] isEqualToString: @"Secure"] ) {
        PRBool secure = PR_FALSE;
        mCachedCookies->ObjectAt(rowIndex)->GetIsSecure(&secure);
        if (secure)
          cookieVal = "yes";
        else
          cookieVal = "no";
      } else if ( [[aTableColumn identifier] isEqualToString: @"Expires"] ) {
        PRUint64 expires = 0;
        mCachedCookies->ObjectAt(rowIndex)->GetExpires(&expires);
        if (expires == 0) {
          // if expires is 0, it's a session cookie; display as expiring on the current date.
          // It's not perfect, but it's better than showing the epoch.
          NSDate *date = [NSDate date];
          return date;   // special case return
        } else {
          NSDate *date = [NSDate dateWithTimeIntervalSince1970: (NSTimeInterval)expires];
          return date;   // special case return
        }
      } else if ( [[aTableColumn identifier] isEqualToString: @"Value"] ) {
        mCachedCookies->ObjectAt(rowIndex)->GetValue(cookieVal);
      }
      retVal = [NSString stringWithCString: cookieVal.get()];
    }
  }
	  
  return retVal;
}

// currently, this only applies to the site allow/deny, since that's the only editable column
-(void) tableView:(NSTableView *)aTableView
   setObjectValue:anObject
   forTableColumn:(NSTableColumn *)aTableColumn
              row:(int)rowIndex
{
  if ( aTableView == mPermissionsTable && aTableColumn == mPermissionColumn ) {
    if ( mCachedPermissions && mPermissionManager ) {
      // create a URI from the hostname of the changed site
      nsCAutoString host;
      mCachedPermissions->ObjectAt(rowIndex)->GetHost(host);
      NSString* url = [NSString stringWithFormat:@"http://%s", host.get()];
      const char* siteURL = [url UTF8String];
      nsCOMPtr<nsIURI> newURI;
      NS_NewURI(getter_AddRefs(newURI), siteURL);
      if ( newURI ) {
        // nsIPermissions are immutable, and there's no API to change the action,
        // so instead we have to delete the old pref and insert a new one.
        // remove the old entry
        mPermissionManager->Remove(host, "cookie");
        // add a new entry with the new permission
        if ( [anObject intValue] == kAllowIndex )
          mPermissionManager->Add(newURI, "cookie", nsIPermissionManager::ALLOW_ACTION);
        else if ( [anObject intValue] == kDenyIndex )
          mPermissionManager->Add(newURI, "cookie", nsIPermissionManager::DENY_ACTION);
        
        // there really should be a better way to keep the cache up-to-date than rebuilding
        // it, but the nsIPermissionManager interface doesn't have a way to get a pointer
        // to a site's nsIPermission. It's this, use a custom class that duplicates the
        // information (wasting a lot of memory), or find a way to tie in to
        // PERM_CHANGE_NOTIFICATION to get the new nsIPermission that way.
        [self populatePermissionCache];
        //re-sort
        [self sortPermissionsByColumn:[mPermissionsTable highlightedTableColumn]];
      }
    }
  }
}

-(void) sortPermissionsByColumn:(NSTableColumn *)aTableColumn
{
  if( mCachedPermissions ) {
    if ( [[aTableColumn identifier] isEqualToString:@"Website"] )
      mCachedPermissions->Sort(comparePermHosts, nsnull);
    else
      mCachedPermissions->Sort(compareCapabilities, nsnull);
    [mPermissionsTable setHighlightedTableColumn:aTableColumn];
    [mPermissionsTable reloadData];
  }
}

-(void) sortCookiesByColumn:(NSTableColumn *)aTableColumn
{
  if( mCachedCookies ) {
    if ( [[aTableColumn identifier] isEqualToString:@"Website"] )
      mCachedCookies->Sort(compareCookieHosts, nsnull);
    else if ( [[aTableColumn identifier] isEqualToString:@"Name"] )
      mCachedCookies->Sort(compareNames, nsnull);
    else if ( [[aTableColumn identifier] isEqualToString:@"Path"] )
      mCachedCookies->Sort(comparePaths, nsnull);
    else if ( [[aTableColumn identifier] isEqualToString:@"Secure"] )
      mCachedCookies->Sort(compareSecures, nsnull);
    else if ( [[aTableColumn identifier] isEqualToString:@"Expires"] )
      mCachedCookies->Sort(compareExpires, nsnull);
    else if ( [[aTableColumn identifier] isEqualToString:@"Value"] )
      mCachedCookies->Sort(compareValues, nsnull);
    [mCookiesTable setHighlightedTableColumn:aTableColumn];
    [mCookiesTable reloadData];
  }
}

// NSTableView delegate methods

- (void) tableView:(NSTableView *)aTableView didClickTableColumn:(NSTableColumn *)aTableColumn
{
  if (aTableView == mPermissionsTable) {
    if ( mCachedPermissions && aTableColumn != [aTableView highlightedTableColumn] ) {
      // save the currently selected row, if any.
      int selectedRowIndex = [aTableView selectedRow];
      nsCOMPtr<nsIPermission> selectedItem = (selectedRowIndex != -1) ?
        mCachedPermissions->ObjectAt(selectedRowIndex) : nsnull;
      // sort the table data
      [self sortPermissionsByColumn:aTableColumn];
      // if a row was selected before, find it again
      if ( selectedItem ) {
        int newRowIndex = mCachedPermissions->IndexOf(selectedItem);
        if ( newRowIndex >= 0 ) {
          [aTableView selectRow:newRowIndex byExtendingSelection:NO];
          [aTableView scrollRowToVisible:newRowIndex];
        }
      }
    }
  } else if (aTableView == mCookiesTable) {
    if ( mCachedCookies && aTableColumn != [aTableView highlightedTableColumn] ) {
      // save the currently selected row, if any
      int selectedRowIndex = [aTableView selectedRow];
      nsCOMPtr<nsICookie> selectedItem = (selectedRowIndex != -1) ?
        mCachedCookies->ObjectAt(selectedRowIndex) : nsnull;
      // sort the table data
      [self sortCookiesByColumn:aTableColumn];
      // if a row was selected before, find it again
      if ( selectedItem ) {
        int newRowIndex = mCachedCookies->IndexOf(selectedItem);
        if ( newRowIndex >= 0 ) {
          [aTableView selectRow:newRowIndex byExtendingSelection:NO];
          [aTableView scrollRowToVisible:newRowIndex];
        }
      }
    }
  }
}

//
// Buttons
//

-(IBAction) clickCookieBehavior:(id)sender
{
  int row = [mCookieBehavior selectedRow];
  [self setPref:"network.cookie.cookieBehavior" toInt:row];
  [self mapCookiePrefToGUI:row];
}

-(IBAction) clickAskAboutCookies:(id)sender
{
  [self setPref:"network.cookie.lifetimePolicy" toInt:([sender state] == NSOnState) ? kWarnAboutCookies : kAcceptCookiesNormally];
}


//
// clickStorePasswords
//
-(IBAction) clickStorePasswords:(id)sender
{
  if ( !mPrefService )
    return;
  mPrefService->SetBoolPref("chimera.store_passwords_with_keychain",
                            ([mStorePasswords state] == NSOnState) ? PR_TRUE : PR_FALSE);
  [mAutoFillPasswords setEnabled:([mStorePasswords state] == NSOnState)];
}

//
// clickAutoFillPasswords
//
// Set pref if autofill is enabled
//
-(IBAction) clickAutoFillPasswords:(id)sender
{
  if ( !mPrefService )
    return;
  mPrefService->SetBoolPref("chimera.keychain_passwords_autofill",
                            ([mAutoFillPasswords state] == NSOnState) ? PR_TRUE : PR_FALSE);
}

-(IBAction) launchKeychainAccess:(id)sender
{
  FSRef fsRef;
  CFURLRef urlRef;
  OSErr err = ::LSGetApplicationForInfo('APPL', 'kcmr', NULL, kLSRolesAll, &fsRef, &urlRef);
  if ( !err ) {
    CFStringRef fileSystemURL = ::CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
    [[NSWorkspace sharedWorkspace] launchApplication:(NSString*)fileSystemURL];
    CFRelease(fileSystemURL);
  }
}
@end
