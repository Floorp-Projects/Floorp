#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"
#include "nsCOMArray.h"
#import "ExtendedTableView.h"

class nsIPref;
class nsIPermissionManager;
class nsIPermission;
class nsICookieManager;
class nsICookie;

@interface OrgMozillaChimeraPreferencePrivacy : PreferencePaneBase
{
  // pane
  IBOutlet NSMatrix* mCookieBehavior;
  IBOutlet NSButton* mAskAboutCookies;
  
  IBOutlet NSButton* mStorePasswords;
  IBOutlet NSButton* mAutoFillPasswords;

  BOOL mSortedAscending;                           // sort direction for tables in sheets

  // permission sheet
  IBOutlet id mPermissionsPanel;
  IBOutlet NSTableView* mPermissionsTable;
  IBOutlet NSTableColumn* mPermissionColumn;
  nsIPermissionManager* mPermissionManager;        // STRONG (should be nsCOMPtr)
  nsCOMArray<nsIPermission>* mCachedPermissions;   // parallel list for speed, STRONG
      
  // cookie sheet
  IBOutlet id mCookiesPanel;
  IBOutlet NSTableView* mCookiesTable;
  nsICookieManager* mCookieManager;
  nsCOMArray<nsICookie>* mCachedCookies;
}

// main panel button actions
-(IBAction) clickCookieBehavior:(id)aSender;
-(IBAction) clickAskAboutCookies:(id)sender;
-(IBAction) clickStorePasswords:(id)sender;
-(IBAction) clickAutoFillPasswords:(id)sender;
-(IBAction) launchKeychainAccess:(id)sender;

// cookie editing functions
-(void) populateCookieCache;
-(IBAction) editCookies:(id)aSender;
-(IBAction) editCookiesDone:(id)aSender;
-(IBAction) removeCookies:(id)aSender;
-(IBAction) removeAllCookies:(id)aSender;

// permission editing functions
-(void) populatePermissionCache;
-(IBAction) editPermissions:(id)aSender;
-(IBAction) editPermissionsDone:(id)aSender;
-(IBAction) removeCookiePermissions:(id)aSender;
-(IBAction) removeAllCookiePermissions:(id)aSender;
-(int) getRowForPermissionWithHost:(NSString *)aHost;

-(void) mapCookiePrefToGUI:(int)pref;

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (void)tableView:(NSTableView *)aTableView setObjectValue:anObject forTableColumn:(NSTableColumn *)aTableColumn  row:(int)rowIndex;

// NSTableView delegate methods
- (void) tableView:(NSTableView *)aTableView didClickTableColumn:(NSTableColumn *)aTableColumn;

// sorting support methods
-(void) sortCookiesByColumn:(NSTableColumn *)aTableColumn inAscendingOrder:(BOOL)ascending;
-(void) sortPermissionsByColumn:(NSTableColumn *)aTableColumn inAscendingOrder:(BOOL)ascending;

@end
