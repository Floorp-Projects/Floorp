#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"

class nsIPref;

@interface OrgMozillaChimeraPreferencePrivacy : PreferencePaneBase
{
  IBOutlet id mCookies;
  IBOutlet NSButton* mPromptForCookie;
  
  IBOutlet NSButton* mEnableJS;
  IBOutlet NSButton* mEnableJava;
  
  IBOutlet NSButton* mLeaveEncrypted;
  IBOutlet NSButton* mLoadLowGrade;
  IBOutlet NSButton* mViewMixed;  
}

-(IBAction) clearCookies:(id)aSender;

-(IBAction) clickPromptForCookie:(id)sender;
-(IBAction) clickEnableCookies:(id)sender;
-(IBAction) clickEnableJS:(id)sender;
-(IBAction) clickEnableJava:(id)sender;

-(IBAction) clickEnableLeaveEncrypted:(id)sender;
-(IBAction) clickEnableLoadLowGrade:(id)sender;
-(IBAction) clickEnableViewMixed:(id)sender;

@end
