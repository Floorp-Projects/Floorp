#import <Cocoa/Cocoa.h>
#import <PreferencePanes/NSPreferencePane.h>

class nsIPref;

@interface OrgMozillaChimeraPreferencePrivacy : NSPreferencePane
{
  IBOutlet id mCookies;
  IBOutlet NSButton* mPromptForCookie;
  
  IBOutlet NSButton* mEnableJS;
  IBOutlet NSButton* mEnableJava;
  
  nsIPref* mPrefService;					// strong ref, but can't use comPtr here
}

-(IBAction) clearCookies:(id)aSender;
-(IBAction) clearDiskCache:(id)aSender;

-(IBAction) clickPromptForCookie:(id)sender;
-(IBAction) clickEnableCookies:(id)sender;
-(IBAction) clickEnableJS:(id)sender;
-(IBAction) clickEnableJava:(id)sender;

@end
