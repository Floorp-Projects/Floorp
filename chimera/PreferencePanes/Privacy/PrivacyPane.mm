#import "PrivacyPane.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsCCookieManager.h"
#include "nsICacheService.h"

@implementation PrivacyPane

- (void) dealloc
{
  NSLog(@"Going away from Privacy Panel!!!!");

}

- (id) initWithBundle:(NSBundle *) bundle {
  self = [super initWithBundle:bundle] ;
  return self;
}

- (void)awakeFromNib
{
  NSLog(@"PrivacyPane awoke from nib");
}

- (void)mainViewDidLoad
{
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, pref panel left uninitialized");
  if ( !prefService )
    return;
    
  // Hookup cookie prefs. Relies on the tags of the radio buttons in the matrix being
  // set such that "enable all" is 0 and "disable all" is 2.
  PRInt32 acceptCookies = 0;
  prefService->GetIntPref("network.accept_cookies", &acceptCookies);
  if ( acceptCookies == 1 )					// be safe in case of importing a mozilla profile
    acceptCookies = 2;
  if ( [mCookies selectCellWithTag:acceptCookies] != YES )
    NS_WARNING("Bad value for network.accept_cookies");
    
  PRBool warnAboutCookies = PR_TRUE;
  prefService->GetBoolPref("network.cookie.warnAboutCookies", &warnAboutCookies);
  [mPromptForCookie setState:(warnAboutCookies ? NSOnState : NSOffState)];
  
  PRBool jsEnabled = PR_TRUE;
  prefService->GetBoolPref("javascript.enabled", &jsEnabled);
  [mEnableJS setState:(jsEnabled ? NSOnState : NSOffState)];

  PRBool javaEnabled = PR_TRUE;
  prefService->GetBoolPref("security.enable_java", &javaEnabled);
  [mEnableJava setState:(javaEnabled ? NSOnState : NSOffState)];
}


//
// didUnselect
//
// Called when our panel is no longer the current panel, or when the window is going
// away. Use this as an opportunity to save the current values.
//
- (void)didUnselect
{
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, pref panel values not saved");
  if ( !prefService )
    return;

  // Save cookie prefs. Relies on the tags of the radio buttons in the matrix being
  // set such that "enable all" is 0 and "disable all" is 2.
  prefService->SetIntPref("network.accept_cookies", [[mCookies selectedCell] tag]);
  prefService->SetBoolPref("network.cookie.warnAboutCookies",
                            [mPromptForCookie state] == NSOnState ? PR_TRUE : PR_FALSE);
  
  prefService->SetBoolPref("javascript.enabled",
                            [mEnableJS state] == NSOnState ? PR_TRUE : PR_FALSE);
  prefService->SetBoolPref("security.enable_java",
                            [mEnableJava state] == NSOnState ? PR_TRUE : PR_FALSE);
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
// clearDiskCache:
//
// Clear the user's disk cache
//
-(IBAction) clearDiskCache:(id)aSender
{
  nsCOMPtr<nsICacheService> cacheServ ( do_GetService("@mozilla.org/network/cache-service;1") );
  if ( cacheServ )
    cacheServ->EvictEntries(nsICache::STORE_ON_DISK);
}

@end
