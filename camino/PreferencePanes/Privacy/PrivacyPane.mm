#import "PrivacyPane.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsCCookieManager.h"

// prefs for showing security dialogs
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"

@implementation OrgMozillaChimeraPreferencePrivacy

- (void) dealloc
{
  NS_IF_RELEASE(mPrefService);
  [super dealloc];
}


- (void)mainViewDidLoad
{
  if ( !mPrefService )
    return;
    
  // Hookup cookie prefs. Relies on the tags of the radio buttons in the matrix being
  // set such that "enable all" is 0 and "disable all" is 2. If mozilla has other prefs
  // that we don't quite know about, we assume they were remapped by the CHPreferenceManager
  // at startup.
  PRInt32 acceptCookies = 0;
  mPrefService->GetIntPref("network.cookie.cookieBehavior", &acceptCookies);
  if ( [mCookies selectCellWithTag:acceptCookies] != YES )
    NS_WARNING("Bad value for network.cookie.cookieBehavior");
    
  PRBool warnAboutCookies = PR_TRUE;
  mPrefService->GetBoolPref("network.cookie.warnAboutCookies", &warnAboutCookies);
  [mPromptForCookie setState:(warnAboutCookies ? NSOnState : NSOffState)];
  
  // Set initial value on Java/JavaScript checkboxes
  
  PRBool jsEnabled = PR_TRUE;
  mPrefService->GetBoolPref("javascript.enabled", &jsEnabled);
  [mEnableJS setState:(jsEnabled ? NSOnState : NSOffState)];

  PRBool javaEnabled = PR_TRUE;
  mPrefService->GetBoolPref("security.enable_java", &javaEnabled);
  [mEnableJava setState:(javaEnabled ? NSOnState : NSOffState)];
  
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
// clearCookies:
//
// Clear all the user's cookies.
//
-(IBAction) clearCookies:(id)aSender
{
  nsCOMPtr<nsICookieManager> cookieMonster ( do_GetService(NS_COOKIEMANAGER_CONTRACTID) );
  if ( cookieMonster )
    cookieMonster->RemoveAll();
}

//
// clickPromptForCookie:
//
// Set if the user should be prompted for each cookie
//
-(IBAction) clickPromptForCookie:(id)sender
{
  [self setPref:"network.cookie.warnAboutCookies" toBoolean:[sender state] == NSOnState];
}

//
// clickEnableCookies:
//
// Set cookie prefs. Relies on the tags of the radio buttons in the matrix being
// set such that "enable all" is 0 and "disable all" is 2.
//
-(IBAction) clickEnableCookies:(id)sender
{
  if ( !mPrefService )
    return;
  mPrefService->SetIntPref("network.cookie.cookieBehavior", [[mCookies selectedCell] tag]);
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
