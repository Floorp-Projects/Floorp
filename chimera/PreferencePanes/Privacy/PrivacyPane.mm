#import "PrivacyPane.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsCCookieManager.h"
#include "nsIPermissionManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIPermission.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"
#include "nsString.h"

// prefs for keychain password autofill
static const char* const gUseKeychainPref = "chimera.store_passwords_with_keychain";
static const char* const gAutoFillEnabledPref = "chimera.keychain_passwords_autofill";

const int kPromptForCookiesTag = 99;
const int kEnableAllCookies = 0;
const int kDisableAllCookies = 2;

@implementation OrgMozillaChimeraPreferencePrivacy

- (void) dealloc
{
  NS_IF_RELEASE(mManager);
  [super dealloc];
}

//
// mapCookiePrefToCheckbox:
//
// Takes an int from the mozilla cookie pref and turns it into a BOOL
// that can be used by our "enable cookies" checkbox.
//
-(BOOL)mapCookiePrefToCheckbox:(int)inCookiePref
{
  BOOL retval = YES;
  if ( inCookiePref == kDisableAllCookies )
    retval = NO;
  return retval;
}

//
// mapCookieCheckboxToPref:
//
// Takes a BOOL from a checkbox and maps it to the mozilla pref values
//
-(int)mapCookieCheckboxToPref:(BOOL)inCheckboxValue
{
  PRInt32 retval = kDisableAllCookies;
  if ( inCheckboxValue )
    retval = kEnableAllCookies;
  return retval;
}

- (void)mainViewDidLoad
{
  if ( !mPrefService )
    return;
    
  // Hookup cookie prefs. The "ask about" and "cookie sites" buttons need to
  // be disabled when cookies aren't enabled because they aren't applicable.
  PRInt32 acceptCookies = kEnableAllCookies;
  mPrefService->GetIntPref("network.cookie.cookieBehavior", &acceptCookies);
  BOOL cookiesEnabled = [self mapCookiePrefToCheckbox:acceptCookies];
  [mCookiesEnabled setState:cookiesEnabled];
  if ( !cookiesEnabled ) {
    [mAskAboutCookies setEnabled:NO];
    [mEditSitesButton setEnabled:NO];
    [mEditSitesText setTextColor:[NSColor lightGrayColor]];
  }
  PRBool warnAboutCookies = PR_TRUE;
  mPrefService->GetBoolPref("network.cookie.warnAboutCookies", &warnAboutCookies);
  if ( warnAboutCookies )
    [mAskAboutCookies setState:YES];
  
  // store permission manager service and cache the enumerator.
  nsCOMPtr<nsIPermissionManager> pm ( do_GetService(NS_PERMISSIONMANAGER_CONTRACTID) );
  mManager = pm.get();
  NS_IF_ADDREF(mManager);
  
  // Keychain checkboxes	

  PRBool storePasswords = PR_TRUE;
  mPrefService->GetBoolPref(gUseKeychainPref, &storePasswords);
  [mStorePasswords setState:(storePasswords ? NSOnState : NSOffState)];
  [mAutoFillPasswords setEnabled:storePasswords ? YES : NO];

  PRBool autoFillPasswords = PR_TRUE;
  mPrefService->GetBoolPref(gAutoFillEnabledPref, &autoFillPasswords);
  [mAutoFillPasswords setState:(autoFillPasswords ? NSOnState : NSOffState)];
}

//
// clearCookies:
//
// Clear all the user's cookies.
//
-(IBAction) clearCookies:(id)aSender
{
  nsCOMPtr<nsICookieManager> cookieMonster ( do_GetService(NS_COOKIEMANAGER_CONTRACTID) );
  if ( cookieMonster )
    cookieMonster->RemoveAll();
}


//
// clickEnableCookies:
//
// Set cookie prefs and updates the enabled/disabled states of the
// "ask" and "edit sites" buttons.
//
-(IBAction) clickEnableCookies:(id)sender
{
  if ( !mPrefService )
    return;
    
  // set the pref
  BOOL enabled = [mCookiesEnabled state];
  [self setPref:"network.cookie.cookieBehavior" toInt:[self mapCookieCheckboxToPref:enabled]];

  // update the buttons
  [mAskAboutCookies setEnabled:enabled];
  [mEditSitesButton setEnabled:enabled];
  [mEditSitesText setTextColor:(enabled ? [NSColor blackColor] : [NSColor lightGrayColor])];
}


-(IBAction) clickAskAboutCookies:(id)sender
{
  [self setPref:"network.cookie.warnAboutCookies" toBoolean:[sender state]];
}


-(IBAction) editCookieSites:(id)aSender
{
  nsCOMPtr<nsISimpleEnumerator> permEnum;
  if ( mManager ) 
    mManager->GetEnumerator(getter_AddRefs(permEnum));

  // build parallel permission list for speed with a lot of blocked sites
  NS_NewISupportsArray(&mCachedPermissions);     // ADDREFs
  if ( mCachedPermissions && permEnum ) {
    PRBool hasMoreElements = PR_FALSE;
    permEnum->HasMoreElements(&hasMoreElements);
    while ( hasMoreElements ) {
      nsCOMPtr<nsISupports> curr;
      permEnum->GetNext(getter_AddRefs(curr));
      mCachedPermissions->AppendElement(curr);
      
      permEnum->HasMoreElements(&hasMoreElements);
    }
  }
  
	[NSApp beginSheet:mCookieSitePanel
        modalForWindow:[mCookiesEnabled window]   // any old window accessor
        modalDelegate:self
        didEndSelector:@selector(editCookieSitesSheetDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
        
  // ensure a row is selected (cocoa doesn't do this for us, but will keep
  // us from unselecting a row once one is set; go figure).
  [mSiteTable selectRow:0 byExtendingSelection:NO];
  
  // we shouldn't need to do this, but the scrollbar won't enable unless we
  // force the table to reload its data. Oddly it gets the number of rows correct,
  // it just forgets to tell the scrollbar. *shrug*
  [mSiteTable reloadData];
}

-(IBAction) editCookieSitesDone:(id)aSender
{
  // save stuff
  
  [mCookieSitePanel orderOut:self];
  [NSApp endSheet:mCookieSitePanel];
  
	NS_IF_RELEASE(mCachedPermissions);
}

- (void)editCookieSitesSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo
{
}


-(IBAction) removeCookieSite:(id)aSender
{
  if ( mCachedPermissions && mManager ) {
    // remove from parallel array and cookie permissions list
    int row = [mSiteTable selectedRow];
    
    // remove from permission manager (which is done by host, not by row), then 
    // remove it from our parallel array (which is done by row). Since we keep a
    // parallel array, removing multiple items by row is very difficult since after 
    // deleting, the array is out of sync with the next cocoa row we're told to remove. Punt!
    nsCOMPtr<nsISupports> rowItem = dont_AddRef(mCachedPermissions->ElementAt(row));
    nsCOMPtr<nsIPermission> perm ( do_QueryInterface(rowItem) );
    if ( perm ) {
      nsXPIDLCString host;
      perm->GetHost(getter_Copies(host));
      mManager->Remove(nsDependentCString(host), 0);           // could this api _be_ any worse? Come on!
      
      mCachedPermissions->RemoveElementAt(row);
    }
    [mSiteTable reloadData];
  }
}


//
// NSTableDataSource protocol methods
//

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
      if ( [[aTableColumn identifier] isEqualToString:@"Website"] ) {
        // website url column
        nsXPIDLCString host;
        perm->GetHost(getter_Copies(host));
        retVal = [NSString stringWithCString:host];
      }
      else {
        // allow/deny column
        PRBool capability = PR_FALSE;			// false = deny, true = accept;
        perm->GetCapability(&capability);
        if ( capability )
          retVal = [self getLocalizedString:@"Allow"];
        else
          retVal = [self getLocalizedString:@"Deny"];
      }
    }
  }
  
  return retVal;
}


//
// clickStorePasswords
//
-(IBAction) clickStorePasswords:(id)sender
{
  if ( !mPrefService )
    return;
  if([mStorePasswords state] == NSOnState)
  {
      mPrefService->SetBoolPref("chimera.store_passwords_with_keychain", PR_TRUE);
      [mAutoFillPasswords setEnabled:YES];
  }
  else
  {
      mPrefService->SetBoolPref("chimera.store_passwords_with_keychain", PR_FALSE);
      [mAutoFillPasswords setEnabled:NO];
  }        
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
                            [mAutoFillPasswords state] == NSOnState ? PR_TRUE : PR_FALSE);
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
