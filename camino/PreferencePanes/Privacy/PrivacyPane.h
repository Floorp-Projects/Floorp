#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"
#include "nsCOMArray.h"
#import "ExtendedTableView.h"

class nsIPref;
class nsIPermissionManager;
class nsISimpleEnumerator;
class nsIPermission;

@interface OrgMozillaChimeraPreferencePrivacy : PreferencePaneBase
{
  IBOutlet NSButton* mCookiesEnabled;
  IBOutlet NSButton* mAskAboutCookies;
  IBOutlet id mCookieSitePanel;
  IBOutlet NSButton* mEditSitesButton;
  IBOutlet NSTextField* mEditSitesText;
  
  IBOutlet NSButton* mStorePasswords;
  IBOutlet NSButton* mAutoFillPasswords;
  
  IBOutlet ExtendedTableView* mSiteTable;
  nsIPermissionManager* mManager;         // STRONG (should be nsCOMPtr)  
  nsCOMArray<nsIPermission>* mCachedPermissions;	// parallel list of permissions for speed
}

-(IBAction) clearCookies:(id)aSender;
-(IBAction) editCookieSites:(id)aSender;
-(IBAction) editCookieSitesDone:(id)aSender;
-(IBAction) removeCookieSite:(id)aSender;
- (void) editCookieSitesSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;

// NSTableView delegate methods
- (void)tableView:(NSTableView *)aTableView didClickTableColumn:(NSTableColumn *)aTableColumn;

-(IBAction) clickEnableCookies:(id)sender;
-(IBAction) clickAskAboutCookies:(id)sender;

-(IBAction) clickStorePasswords:(id)sender;
-(IBAction) clickAutoFillPasswords:(id)sender;
-(IBAction) launchKeychainAccess:(id)sender;

// helpers going between the enable cookie checkbox and the mozilla pref
-(BOOL)mapCookiePrefToCheckbox:(int)inCookiePref;
-(int)mapCookieCheckboxToPref:(BOOL)inCheckboxValue;

@end
