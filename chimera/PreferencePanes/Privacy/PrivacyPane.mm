#import "PrivacyPane.h"

#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsCCookieManager.h"
#include "nsICacheService.h"

@implementation OrgMozillaChimeraPreferencePrivacy

- (void) dealloc
{
  NS_IF_RELEASE(mPrefService);
  [super dealloc];
}

- (id) initWithBundle:(NSBundle *) bundle
{
  self = [super initWithBundle:bundle] ;
  
  nsCOMPtr<nsIPref> prefService ( do_GetService(NS_PREF_CONTRACTID) );
  NS_ASSERTION(prefService, "Could not get pref service, pref panel left uninitialized");
  mPrefService = prefService.get();
  NS_IF_ADDREF(mPrefService);
  
  return self;
}

- (void)awakeFromNib
{
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
  
  PRBool jsEnabled = PR_TRUE;
  mPrefService->GetBoolPref("javascript.enabled", &jsEnabled);
  [mEnableJS setState:(jsEnabled ? NSOnState : NSOffState)];

  PRBool javaEnabled = PR_TRUE;
  mPrefService->GetBoolPref("security.enable_java", &javaEnabled);
  [mEnableJava setState:(javaEnabled ? NSOnState : NSOffState)];
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

//
// clickPromptForCookie:
//
// Set if the user should be prompted for each cookie
//
-(IBAction) clickPromptForCookie:(id)sender
{
  if ( !mPrefService )
    return;
  mPrefService->SetBoolPref("network.cookie.warnAboutCookies",
                            [mPromptForCookie state] == NSOnState ? PR_TRUE : PR_FALSE);
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
  if ( !mPrefService )
    return;
  mPrefService->SetBoolPref("javascript.enabled",
                            [mEnableJS state] == NSOnState ? PR_TRUE : PR_FALSE);
}

//
// clickEnableJava
//
// Set pref if Java is enabled
//
-(IBAction) clickEnableJava:(id)sender
{
  if ( !mPrefService )
    return;
  mPrefService->SetBoolPref("security.enable_java",
                            [mEnableJava state] == NSOnState ? PR_TRUE : PR_FALSE);
}

@end
