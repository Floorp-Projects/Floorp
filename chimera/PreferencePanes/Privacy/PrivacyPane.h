#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>

@interface OrgMozillaChimeraPreferencePrivacy : NSPreferencePane
{
  IBOutlet id mCookies;
  IBOutlet NSButton* mPromptForCookie;
  
  IBOutlet NSButton* mEnableJS;
  IBOutlet NSButton* mEnableJava;
}

-(IBAction) clearCookies:(id)aSender;
-(IBAction) clearDiskCache:(id)aSender;

@end
