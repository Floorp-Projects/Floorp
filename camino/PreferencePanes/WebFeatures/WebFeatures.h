#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"
#import "ExtendedTableView.h"

class nsIPref;
class nsIPermissionManager;
class nsISupportsArray;

@interface OrgMozillaChimeraPreferenceWebFeatures : PreferencePaneBase
{
  IBOutlet NSButton* mEnableJS;
  IBOutlet NSButton* mEnableJava;
  IBOutlet NSButton* mEnablePlugins;

  IBOutlet NSButton *mEnablePopupBlocking;
  IBOutlet NSButton *mEditWhitelist;
  
  IBOutlet id mWhitelistPanel;
  IBOutlet ExtendedTableView* mWhitelistTable;
  IBOutlet NSTextField* mAddField;
  nsIPermissionManager* mManager;         // STRONG (should be nsCOMPtr)  
  nsISupportsArray* mCachedPermissions;		// parallel list of permissions for speed, STRONG (should be nsCOMPtr)
  
  IBOutlet NSButton* mEnableAnnoyanceBlocker;
}

-(IBAction) clickEnableJS:(id)sender;
-(IBAction) clickEnableJava:(id)sender;
-(IBAction) clickEnablePlugins:(id)sender;

-(IBAction) clickEnablePopupBlocking:(id)sender;
-(IBAction) editWhitelist:(id)sender;

-(IBAction) clickEnableAnnoyanceBlocker:(id)sender;
-(void) setAnnoyingWindowPrefsTo:(BOOL)inValue;

// whitelist sheet methods
-(IBAction) editWhitelistDone:(id)aSender;
-(IBAction) removeWhitelistSite:(id)aSender;
-(IBAction) addWhitelistSite:(id)sender;
-(void) editWhitelistSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;
-(void) populatePermissionCache:(nsISupportsArray*)inPermissions;

// data source informal protocol (NSTableDataSource)
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;

@end
