#import "SecurityPane.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"

// prefs for showing security dialogs
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"


@implementation OrgMozillaChimeraPreferenceSecurity

- (void) dealloc
{
  [super dealloc];
}


- (void)mainViewDidLoad
{
  if ( !mPrefService )
    return;
    
  // Set initial value on Security checkboxes
  PRBool leaveEncrypted = PR_TRUE;
  mPrefService->GetBoolPref(LEAVE_SITE_PREF, &leaveEncrypted);
  [mLeaveEncrypted setState:(leaveEncrypted ? NSOnState : NSOffState)];
  
  PRBool loadLowGrade = PR_TRUE;
  mPrefService->GetBoolPref(WEAK_SITE_PREF, &loadLowGrade);
  [mLoadLowGrade setState:(loadLowGrade ? NSOnState : NSOffState)];

  PRBool viewMixed = PR_TRUE;
  mPrefService->GetBoolPref(MIXEDCONTENT_PREF, &viewMixed);
  [mViewMixed setState:(viewMixed ? NSOnState : NSOffState)];
}


//
// clickEnableViewMixed:
// clickEnableLoadLowGrade:
// clickEnableLeaveEncrypted:
//
// Set prefs for warnings/alerts wrt secure sites
//

-(IBAction) clickEnableViewMixed:(id)sender
{
  [self setPref:MIXEDCONTENT_PREF toBoolean:[sender state] == NSOnState];
}

-(IBAction) clickEnableLoadLowGrade:(id)sender
{
  [self setPref:WEAK_SITE_PREF toBoolean:[sender state] == NSOnState];
}

-(IBAction) clickEnableLeaveEncrypted:(id)sender
{
  [self setPref:LEAVE_SITE_PREF toBoolean:[sender state] == NSOnState];
}

@end
