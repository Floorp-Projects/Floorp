#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"

class nsIPref;

@interface OrgMozillaChimeraPreferenceSecurity : PreferencePaneBase
{
  IBOutlet NSButton* mLeaveEncrypted;
  IBOutlet NSButton* mLoadLowGrade;
  IBOutlet NSButton* mViewMixed;  
}

-(IBAction) clickEnableLeaveEncrypted:(id)sender;
-(IBAction) clickEnableLoadLowGrade:(id)sender;
-(IBAction) clickEnableViewMixed:(id)sender;

@end
