#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>
#import "PreferencePaneBase.h"

class nsIPref;

@interface OrgMozillaChimeraPreferenceWebFeatures : PreferencePaneBase
{
  IBOutlet NSButton* mEnableJS;
  IBOutlet NSButton* mEnableJava;
  IBOutlet NSButton* mEnablePlugins;

  IBOutlet NSButton *mEnablePopupBlocking;
}

-(IBAction) clickEnableJS:(id)sender;
-(IBAction) clickEnableJava:(id)sender;
-(IBAction) clickEnablePlugins:(id)sender;

-(IBAction) clickEnablePopupBlocking:(id)sender;

@end
