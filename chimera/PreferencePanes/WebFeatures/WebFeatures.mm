#import "WebFeatures.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"


@implementation OrgMozillaChimeraPreferenceWebFeatures

- (void) dealloc
{
  [super dealloc];
}


- (void)mainViewDidLoad
{
  if ( !mPrefService )
    return;
    
  // Set initial value on Java/JavaScript checkboxes
  
  PRBool jsEnabled = PR_TRUE;
  mPrefService->GetBoolPref("javascript.enabled", &jsEnabled);
  [mEnableJS setState:(jsEnabled ? NSOnState : NSOffState)];

  PRBool javaEnabled = PR_TRUE;
  mPrefService->GetBoolPref("security.enable_java", &javaEnabled);
  [mEnableJava setState:(javaEnabled ? NSOnState : NSOffState)];

  PRBool pluginsEnabled = PR_TRUE;
  mPrefService->GetBoolPref("chimera.enable_plugins", &pluginsEnabled);
  [mEnablePlugins setState:(pluginsEnabled ? NSOnState : NSOffState)];

  // set initial value on popup blocking checkbox
  BOOL gotPref = NO;
  BOOL enablePopupBlocking = [self getBooleanPref:"dom.disable_open_during_load" withSuccess:&gotPref] && gotPref;  
  [mEnablePopupBlocking setState:enablePopupBlocking];
}


//
// clickEnableJS
//
// Set pref if JavaScript is enabled
//
-(IBAction) clickEnableJS:(id)sender
{
  [self setPref:"javascript.enabled" toBoolean:[sender state] == NSOnState];
}

//
// clickEnableJava
//
// Set pref if Java is enabled
//
-(IBAction) clickEnableJava:(id)sender
{
  [self setPref:"security.enable_java" toBoolean:[sender state] == NSOnState];
}

//
// clickEnablePlugins
//
// Set pref if plugins are enabled
//
-(IBAction) clickEnablePlugins:(id)sender
{
  [self setPref:"chimera.enable_plugins" toBoolean:[sender state] == NSOnState];
}

//
// clickEnablePopupBlocking
//
// Enable and disable mozilla's popup blocking feature. We use a combination of 
// two prefs to suppress bad popups.
//
- (IBAction)clickEnablePopupBlocking:(id)sender
{
  if ( [sender state] ) {
    [self setPref:"dom.disable_open_during_load" toBoolean: YES];
    [self setPref:"dom.disable_open_click_delay" toInt: 1000];
  }
  else {
    [self setPref:"dom.disable_open_during_load" toBoolean: NO];
    [self setPref:"dom.disable_open_click_delay" toInt: 0];
  }

}

@end
