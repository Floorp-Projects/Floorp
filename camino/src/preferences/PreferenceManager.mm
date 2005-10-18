/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bruce Davidson <Bruce.Davidson@ipl.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import <SystemConfiguration/SystemConfiguration.h>

#import "NSString+Utils.h"

#import "PreferenceManager.h"
#import "UserDefaults.h"
#import "CHBrowserService.h"
#import "CHISupportsOwner.h"

#include "nsBuildID.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsWeakReference.h"
#include "nsIServiceManager.h"
#include "nsIObserver.h"
#include "nsProfileDirServiceProvider.h"
#include "nsIPref.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsEmbedAPI.h"
#include "AppDirServiceProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIRegistry.h"
#include "nsIStyleSheetService.h"
#include "nsNetUtil.h"
#include "nsStaticComponents.h"

#ifndef _BUILD_STATIC_BIN
nsStaticModuleInfo const *const kPStaticModules = nsnull;
PRUint32 const kStaticModuleCount = 0;
#endif

NSString* const kPrefChangedNotificationName = @"PrefChangedNotification";
// userInfo entries:
  NSString* const kPrefChangedPrefNameUserInfoKey = @"pref_name";


static NSString* const AdBlockingChangedNotificationName = @"AdBlockingChanged";

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_4
// These are not available for linkage before the 10.4 SDK, but the feature is in 10.3.2 and later;
// the strings were obtained by inspection.
static NSString* const kSCPropNetProxiesProxyAutoConfigEnable    = @"ProxyAutoConfigEnable";
static NSString* const kSCPropNetProxiesProxyAutoConfigURLString = @"ProxyAutoConfigURLString";
static NSString* const kSCPropNetProxiesProxyAutoDiscoveryEnable = @"ProxyAutoDiscoveryEnable";
#endif

// This is an arbitrary version stamp that gets written to the prefs file.
// It can be used to detect when a new version of Camino is run that needs
// some prefs to be upgraded.
static const PRInt32 kCurrentPrefsVersion = 1;

@interface PreferenceManager(PreferenceManagerPrivate)

- (void)registerNotificationListener;

- (void)termEmbedding: (NSNotification*)aNotification;
- (void)xpcomTerminate: (NSNotification*)aNotification;

- (NSString*)oldProfilePath;
- (void)migrateChimeraProfile:(NSString*)newProfilePath;

- (void)showLaunchFailureAndQuitWithErrorTitle:(NSString*)inTitleFormat errorMessage:(NSString*)inMessageFormat;

- (void)configureProxies;
- (BOOL)updateOneProxy:(NSDictionary*)configDict
    protocol:(NSString*)protocol
    proxyEnableKey:(NSString*)enableKey
    proxyURLKey:(NSString*)urlKey
    proxyPortKey:(NSString*)portKey;

- (void)registerForProxyChanges;
- (void)readSystemProxySettings;

- (BOOL)cleanupUserContentCSS;
- (void)refreshAdBlockingStyleSheet:(BOOL)inLoad;

@end

#pragma mark -

// 
// PrefChangeObserver.
// 
// We create one of these each time someone adds a pref observer
// 
class PrefChangeObserver: public nsIObserver
{
public:
                        PrefChangeObserver(id inObject)  // inObject can be nil
                        : mObject(inObject)
                        {}
                        
  virtual               ~PrefChangeObserver()
                        {}
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  id                    GetObject() const { return mObject; }
  nsresult              RegisterForPref(const char* inPrefName);
  nsresult              UnregisterForPref(const char* inPrefName);

protected:

  id                    mObject;    // not retained
};

NS_IMPL_ISUPPORTS1(PrefChangeObserver, nsIObserver);

nsresult
PrefChangeObserver::RegisterForPref(const char* inPrefName)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch2> pbi = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  return pbi->AddObserver(inPrefName, this, PR_FALSE);
}

nsresult
PrefChangeObserver::UnregisterForPref(const char* inPrefName)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch2> pbi = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  return pbi->RemoveObserver(inPrefName, this);
}

NS_IMETHODIMP
PrefChangeObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aSomeData)
{
  if (nsCRT::strcmp(aTopic, "nsPref:changed") != 0)
    return NS_OK;   // not a pref changed notification

  NSDictionary* userInfoDict = [NSDictionary dictionaryWithObject:[NSString stringWithPRUnichars:aSomeData]
                                                           forKey:kPrefChangedPrefNameUserInfoKey];
  
  [[NSNotificationCenter defaultCenter] postNotificationName:kPrefChangedNotificationName
                                                      object:mObject
                                                    userInfo:userInfoDict];
  return NS_OK;
}

// little wrapper for the C++ observer class, which takes care of registering
// and unregistering the observer.
@interface PrefChangeObserverOwner : CHISupportsOwner
{
@private
  NSString*           mPrefName;
}

- (id)initWithPrefName:(NSString*)inPrefName object:(id)inObject;
- (BOOL)hasObject:(id)inObject;

@end

@implementation PrefChangeObserverOwner

- (id)initWithPrefName:(NSString*)inPrefName object:(id)inObject
{
  PrefChangeObserver* changeObserver = new PrefChangeObserver(inObject);
  if ((self = [super initWithValue:(nsISupports*)changeObserver]))    // retains it
  {
    mPrefName = [inPrefName retain];
    NSLog(@"registering observer %@ on %@", self, mPrefName);
    changeObserver->RegisterForPref([mPrefName UTF8String]);
  }
  return self;
}

- (void)dealloc
{
  NSLog(@"unregistering observer %@ on %@", self, mPrefName);

  PrefChangeObserver* changeObserver = NS_REINTERPRET_CAST(PrefChangeObserver*, [super value]);
  if (changeObserver)
    changeObserver->UnregisterForPref([mPrefName UTF8String]);

  [mPrefName release];
  [super dealloc];
}

- (BOOL)hasObject:(id)inObject
{
  PrefChangeObserver* changeObserver = NS_REINTERPRET_CAST(PrefChangeObserver*, [super value]);
  return (changeObserver && (changeObserver->GetObject() == inObject));
}

@end

#pragma mark -

@implementation PreferenceManager

static PreferenceManager* gSharedInstance = nil;
#if DEBUG
static BOOL gMadePrefManager;
#endif

+ (PreferenceManager *)sharedInstance
{
  if (!gSharedInstance)
  {
#if DEBUG
    if (gMadePrefManager)
      NSLog(@"Recreating preferences manager on shutdown!");
    gMadePrefManager = YES;
#endif
    gSharedInstance = [[PreferenceManager alloc] init];
  }
    
  return gSharedInstance;
}

+ (PreferenceManager *)sharedInstanceDontCreate
{
  return gSharedInstance;
}

- (id) init
{
  if ((self = [super init]))
  {
    mRunLoopSource = NULL;
    
    [self registerNotificationListener];

    if ([self initMozillaPrefs] == NO) {
      // we should never get here
      NSLog (@"Failed to initialize mozilla prefs");
    }
    
    mDefaults = [NSUserDefaults standardUserDefaults];
  }
  return self;
}

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (self == gSharedInstance)
    gSharedInstance = nil;

  [super dealloc];
}

- (void)termEmbedding: (NSNotification*)aNotification
{
  NS_IF_RELEASE(mPrefs);
  // remove our runloop observer
  if (mRunLoopSource)
  {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mRunLoopSource, kCFRunLoopCommonModes);
    CFRelease(mRunLoopSource);
    mRunLoopSource = NULL;
  }
}

- (void)xpcomTerminate: (NSNotification*)aNotification
{
  [mPrefChangeObservers release];
  mPrefChangeObservers = nil;

  // this will notify observers that the profile is about to go away.
  if (mProfileProvider)
  {
      mProfileProvider->Shutdown();
      // directory service holds a strong ref to this as well.
      NS_RELEASE(mProfileProvider);
  }

  // save prefs now, in case any termination listeners set prefs.
  [self savePrefsFile];

  [gSharedInstance release];
}

- (void)registerNotificationListener
{
  [[NSNotificationCenter defaultCenter] addObserver:  self
                                        selector:     @selector(termEmbedding:)
                                        name:         TermEmbeddingNotificationName
                                        object:       nil];

  [[NSNotificationCenter defaultCenter] addObserver:  self
                                        selector:     @selector(xpcomTerminate:)
                                        name:         XPCOMShutDownNotificationName
                                        object:       nil];

  [[NSNotificationCenter defaultCenter] addObserver:  self
                                        selector:     @selector(adBlockingPrefChanged:)
                                        name:         AdBlockingChangedNotificationName
                                        object:       nil];

}

- (void)savePrefsFile
{
  nsCOMPtr<nsIPrefService> prefsService = do_GetService(NS_PREF_CONTRACTID);
  if (prefsService)
      prefsService->SavePrefFile(nsnull);
}

- (void)showLaunchFailureAndQuitWithErrorTitle:(NSString*)inTitleFormat errorMessage:(NSString*)inMessageFormat
{
  NSString* applicationName = NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil);
  
  NSString* errorString   = [NSString stringWithFormat:inTitleFormat, applicationName];
  NSString* messageString = [NSString stringWithFormat:inMessageFormat, applicationName];

  NSRunAlertPanel(errorString, messageString, NSLocalizedString(@"QuitButtonText", @""), nil, nil);
  [NSApp terminate:self];
}

- (BOOL)initMozillaPrefs
{
    nsresult rv;

    NSString *path = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    const char *binDirPath = [path fileSystemRepresentation];
    nsCOMPtr<nsILocalFile> binDir;
    rv = NS_NewNativeLocalFile(nsDependentCString(binDirPath), PR_TRUE, getter_AddRefs(binDir));
    if (NS_FAILED(rv))
    {
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureBinPathMsg", @"")];
      // not reached
      return NO;
    }

    // This shouldn't be needed since we are initing XPCOM with this
    // directory but causes a (harmless) warning if not defined.
    setenv("MOZILLA_FIVE_HOME", binDirPath, 1);

    // get the 'mozNewProfileDirName' key from our Info.plist file
    NSString *dirString = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"mozNewProfileDirName"];
    const char* profileDirName;
    if (dirString)
      profileDirName = [dirString UTF8String];
    else {
      NSLog(@"mozNewProfileDirName key missing from Info.plist file. Using default profile directory");
      profileDirName = "Camino";
    }
    
    // Supply our own directory service provider so we can control where
    // the registry and profiles are located.
    AppDirServiceProvider *provider = new AppDirServiceProvider(nsDependentCString(profileDirName));
    if (!provider)
    {
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureMsg", @"")];
      // not reached
      return NO;
    }

    nsCOMPtr<nsIDirectoryServiceProvider> dirProvider = (nsIDirectoryServiceProvider*)provider;
    rv = NS_InitEmbedding(binDir, dirProvider,
                          kPStaticModules, kStaticModuleCount);
    if (NS_FAILED(rv)) {
      NSLog(@"Embedding init failed.");
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureInitEmbeddingMsg", @"")];
      // not reached
      return NO;
    }
    
    NSString* profilePath = [self newProfilePath];
    if (!profilePath) {
      NSLog(@"Failed to determine profile path!");
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureProfilePathMsg", @"")];
      // not reached
      return NO;
    }

    // Check for the existence of prefs.js in our new (as of 0.8) profile dir.
    // If it doesn't exist, attempt to migrate over the contents of the old
    // one at ~/Library/Application Support/Chimera/Profiles/default/xxxxxxxx.slt/
    NSFileManager *fileMgr = [NSFileManager defaultManager];
    if (![fileMgr fileExistsAtPath:[profilePath stringByAppendingPathComponent:@"prefs.js"]])
        [self migrateChimeraProfile:profilePath];

    rv = NS_NewProfileDirServiceProvider(PR_TRUE, &mProfileProvider);
    if (NS_FAILED(rv))
    {
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureMsg", @"")];
      
      // not reached
      return NO;
    }
    mProfileProvider->Register();

    nsCOMPtr<nsILocalFile> profileDir;
    rv = NS_NewNativeLocalFile(nsDependentCString([profilePath fileSystemRepresentation]),
                                PR_TRUE, getter_AddRefs(profileDir));
    if (NS_FAILED(rv))
    {
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureMsg", @"")];
      // not reached
      return NO;
    }

    rv = mProfileProvider->SetProfileDir(profileDir);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
        [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"AlreadyRunningAlert", @"")
                                        errorMessage:NSLocalizedString(@"AlreadyRunningMsg", @"")];
      }
      else {
        [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                        errorMessage:NSLocalizedString(@"StartupFailureProfileSetupMsg", @"")];
      }
      // not reached
      return NO;
    }

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (!prefs)
    {
      [self showLaunchFailureAndQuitWithErrorTitle:NSLocalizedString(@"StartupFailureAlert", @"")
                                      errorMessage:NSLocalizedString(@"StartupFailureNoPrefsMsg", @"")];
      // not reached
      return NO;
    }
    
    mPrefs = prefs;
    NS_ADDREF(mPrefs);
    
    [self syncMozillaPrefs];

    // send out initted notification
    [[NSNotificationCenter defaultCenter] postNotificationName:InitEmbeddingNotificationName object:nil];

    return YES;
}

// Convert an Apple locale (or language with the dialect specified) from the form en_GB
// to the en-gb form required for HTTP accept-language headers.
// If the locale isn't in the expected form we return nil. (Systems upgraded
// from 10.1 report human readable locales (e.g. "English")).
+ (NSString*)convertLocaleToHTTPLanguage: (NSString*)inAppleLocale
{
    NSString* r = nil;
    if ( inAppleLocale ) {
      NSMutableString* language = [NSMutableString string];
      NSArray* localeParts = [inAppleLocale componentsSeparatedByString:@"_"];
			
      [language appendString:[localeParts objectAtIndex:0]];
      if ( [localeParts count] > 1 ) {
        [language appendString:@"-"];
        [language appendString:[[localeParts objectAtIndex:1] lowercaseString]];
      }

      // We accept standalone primary subtags (e.g. "en") and also
      // a primary subtag with additional subtags of between two and eight characters long
      // We ignore i- and x- primary subtags
      if ( [language length] == 2 ||
           ( [language length] >= 5 && [language length] <= 13 && [language characterAtIndex:2] == '-' ) )
        r = [NSString stringWithString:language];
    }
    return r;
}

- (void)syncMozillaPrefs
{
    if (!mPrefs) {
        NSLog(@"Mozilla prefs not set up successfully");
        return;
    }
    
    PRInt32 lastRunPrefsVersion = 0;
    mPrefs->GetIntPref("camino.prefs_version", &lastRunPrefsVersion);
    mLastRunPrefsVersion = lastRunPrefsVersion;

    if (mLastRunPrefsVersion < 1) // version at which we turn off the universal charset detector
      mPrefs->SetCharPref("intl.charset.detector", "");

    mPrefs->SetIntPref("camino.prefs_version", kCurrentPrefsVersion);
        
    // fix up the cookie prefs. If 'p3p' or 'accept foreign cookies' are on, remap them to
    // something that chimera can deal with.
    PRInt32 acceptCookies = 0;
    static const char* kCookieBehaviorPref = "network.cookie.cookieBehavior";
    mPrefs->GetIntPref(kCookieBehaviorPref, &acceptCookies);
    if ( acceptCookies == 3 ) {     // p3p, assume all cookies on
      acceptCookies = 0;
      mPrefs->SetIntPref(kCookieBehaviorPref, acceptCookies);
    }

    // previous versions set dom.disable_open_click_delay to block some popups, but
    // we really shouldn't be doing that (mozilla no longer does). Ensure we clear it out
    // with authority so that it doesn't bite us later. Yes, this will break someone setting
    // it manually, but that case is pretty rare. This will also clear it for someone who
    // goes back to a previous version that shares the same pref folder, but that's ok 
    // as well.
    mPrefs->ClearUserPref("dom.disable_open_click_delay");
    
    // previous versions set capability.policy.* to turn off some web features regarding
    // windows, but that caused exceptions to be thrown that webpages weren't ready to 
    // handle. Clear those prefs with authority if they are set.
    if ( [[self getStringPref:"capability.policy.default.Window.defaultStatus" withSuccess:nil] length] > 0 ) {
      mPrefs->ClearUserPref("capability.policy.default.Window.defaultStatus");
      mPrefs->ClearUserPref("capability.policy.default.Window.status");
      mPrefs->ClearUserPref("capability.policy.default.Window.focus");
      mPrefs->ClearUserPref("capability.policy.default.Window.innerHeight.set");
      mPrefs->ClearUserPref("capability.policy.default.Window.innerWidth.set");
      mPrefs->ClearUserPref("capability.policy.default.Window.moveBy");
      mPrefs->ClearUserPref("capability.policy.default.Window.moveTo");
      mPrefs->ClearUserPref("capability.policy.default.Window.outerHeight.set");
      mPrefs->ClearUserPref("capability.policy.default.Window.outerWidth.set");
      mPrefs->ClearUserPref("capability.policy.default.Window.resizeBy");
      mPrefs->ClearUserPref("capability.policy.default.Window.resizeTo");
      mPrefs->ClearUserPref("capability.policy.default.Window.screenX.set");
      mPrefs->ClearUserPref("capability.policy.default.Window.screenY.set");
      mPrefs->ClearUserPref("capability.policy.default.Window.sizeToContent");
      
      // now set the correct prefs
      [self setPref:"dom.disable_window_move_resize" toBoolean:YES];
      [self setPref:"dom.disable_window_status_change" toBoolean:YES];
      [self setPref:"dom.disable_window_flip" toBoolean:YES];
    }

    [self configureProxies];

    // Determine if the user specified their own language override. If so
    // use it. If not work out the languages from the system preferences.
    BOOL userProvidedLangOverride = NO;
    NSString* userLanguageOverride = [self getStringPref:"camino.accept_languages" withSuccess:&userProvidedLangOverride];
    
    if (userProvidedLangOverride && [userLanguageOverride length] > 0)
      [self setPref:"intl.accept_languages" toString:userLanguageOverride];
    else {
      NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
      NSArray *languages = [defs objectForKey:@"AppleLanguages"];
      NSMutableArray* acceptableLanguages = [NSMutableArray array];

      // Build the list of languages the user understands (from System Preferences | International).
      BOOL languagesOkaySoFar = YES;
      for (unsigned long i = 0; languagesOkaySoFar && i < [languages count]; ++i) {
        NSString* language = [PreferenceManager convertLocaleToHTTPLanguage:[languages objectAtIndex:i]];
        if (language)
          [acceptableLanguages addObject:language];
        else {
          // If we don't understand a language don't set any, rather than risk leaving the user with
          // their n'th choice (which may be one Apple made and they don't actually read)
          // Mainly occurs on systems upgraded from 10.1, see convertLocaleToHTTPLanguage(). 
          NSLog( @"Unable to set languages - language '%@' not a valid ISO language identifier", [languages objectAtIndex:i] );
          languagesOkaySoFar = NO;
        }
      }

      // If we understood all the languages in the list set the accept-language header.
      // Note that necko will determine quality factors itself.
      // If we don't set this we'll fall back to the "en-us, en" default from all-camino.js
      if (languagesOkaySoFar && [acceptableLanguages count] > 0) {
        NSString* acceptLangHeader = [acceptableLanguages componentsJoinedByString:@","];
        [self setPref:"intl.accept_languages" toString:acceptLangHeader];
      }
    }
    
    // load up the default stylesheet (is this the best place to do this?)
    BOOL prefExists = NO;
    BOOL enableAdBlocking = [self getBooleanPref:"camino.enable_ad_blocking" withSuccess:&prefExists];
    if (!prefExists)
    {
      enableAdBlocking = [self cleanupUserContentCSS];
      [self setPref:"camino.enable_ad_blocking" toBoolean:enableAdBlocking];
    }

    if (enableAdBlocking)
      [self refreshAdBlockingStyleSheet:YES];
}

#pragma mark -

- (void)configureProxies
{
  [self readSystemProxySettings];
  [self registerForProxyChanges];
}

static void SCProxiesChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void * /* info */)
{
  PreferenceManager* prefsManager = [PreferenceManager sharedInstanceDontCreate];
  [prefsManager readSystemProxySettings];
#if DEBUG
  NSLog(@"Updating proxies");
#endif
}
					
- (void)registerForProxyChanges
{
  if (mRunLoopSource)   // don't register twice
    return;

  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  
  SCDynamicStoreRef dynamicStoreRef = SCDynamicStoreCreate(NULL, CFSTR("ChimeraProxiesNotification"), SCProxiesChangedCallback, &context);
  if (dynamicStoreRef)
  {
    CFStringRef proxyIdentifier = SCDynamicStoreKeyCreateProxies(NULL);
    CFArrayRef  keyList = CFArrayCreate(NULL, (const void **)&proxyIdentifier, 1, &kCFTypeArrayCallBacks);

    Boolean set = SCDynamicStoreSetNotificationKeys(dynamicStoreRef, keyList, NULL);
    if (set)
    {
      mRunLoopSource = SCDynamicStoreCreateRunLoopSource(NULL, dynamicStoreRef, 0);
      if (mRunLoopSource)
      {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), mRunLoopSource, kCFRunLoopCommonModes);
        // we keep the ref to the source, so that we can remove it when the prefs manager is cleaned up.
      }
    }
    
    CFRelease(proxyIdentifier);
    CFRelease(keyList);
    CFRelease(dynamicStoreRef);
  }
}

- (BOOL)updateOneProxy:(NSDictionary*)configDict
  protocol:(NSString*)protocol
  proxyEnableKey:(NSString*)enableKey
  proxyURLKey:(NSString*)urlKey
  proxyPortKey:(NSString*)portKey
{
  BOOL gotProxy = NO;

  BOOL enabled = (BOOL)[[configDict objectForKey:enableKey] intValue];
  if (enabled)
  {
    NSString* protocolProxy = [configDict objectForKey:urlKey];
    int proxyPort = [[configDict objectForKey:portKey] intValue];
    if ([protocolProxy length] > 0 && proxyPort != 0)
    {
      [self setPref:[[NSString stringWithFormat:@"network.proxy.%@", protocol] cString] toString:protocolProxy];
      [self setPref:[[NSString stringWithFormat:@"network.proxy.%@_port", protocol] cString] toInt:proxyPort];
      gotProxy = YES;
    }
  }
  
  return gotProxy;
}

typedef enum EProxyConfig {
  eProxyConfig_Direct = 0,
  eProxyConfig_Manual,
  eProxyConfig_PAC,
  eProxyConfig_Direct4x,
  eProxyConfig_WPAD,
  eProxyConfig_Last
} EProxyConfig;

- (void)readSystemProxySettings
{
  // if the user has set "camino.use_system_proxy_settings" to false, they want
  // to specify their own proxies (or a PAC), so don't read the OS proxy settings
  if (![self getBooleanPref:"camino.use_system_proxy_settings" withSuccess:NULL])
    return;
  
  PRInt32 curProxyType, newProxyType;
  mPrefs->GetIntPref("network.proxy.type", &curProxyType);
  newProxyType = curProxyType;

  mPrefs->ClearUserPref("network.proxy.http");
  mPrefs->ClearUserPref("network.proxy.http_port");
  mPrefs->ClearUserPref("network.proxy.ssl");
  mPrefs->ClearUserPref("network.proxy.ssl_port");
  mPrefs->ClearUserPref("network.proxy.ftp");
  mPrefs->ClearUserPref("network.proxy.ftp_port");
  mPrefs->ClearUserPref("network.proxy.gopher");
  mPrefs->ClearUserPref("network.proxy.gopher_port");
  mPrefs->ClearUserPref("network.proxy.socks");
  mPrefs->ClearUserPref("network.proxy.socks_port");
  mPrefs->ClearUserPref("network.proxy.no_proxies_on");

  // get proxies from SystemConfiguration
  NSDictionary* proxyConfigDict = (NSDictionary*)SCDynamicStoreCopyProxies(NULL);
  if (proxyConfigDict)
  {
    // look for PAC
    NSNumber* proxyAutoConfig = (NSNumber*)[proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesProxyAutoConfigEnable];
    NSString* proxyURLString  = (NSString*)[proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesProxyAutoConfigURLString];
    if ([proxyAutoConfig intValue] != 0 && [proxyURLString length] > 0)
    {
      NSLog(@"Using Proxy Auto-Config (PAC) file %@", proxyURLString);
      [self setPref:"network.proxy.autoconfig_url" toString:proxyURLString];
      newProxyType = eProxyConfig_PAC;
    }
    else
    {
      BOOL gotAProxy = NO;
      
      gotAProxy |= [self updateOneProxy:proxyConfigDict protocol:@"http"
                              proxyEnableKey:(NSString*)kSCPropNetProxiesHTTPEnable
                                 proxyURLKey:(NSString*)kSCPropNetProxiesHTTPProxy
                                proxyPortKey:(NSString*)kSCPropNetProxiesHTTPPort];

      gotAProxy |= [self updateOneProxy:proxyConfigDict protocol:@"ssl"
                              proxyEnableKey:(NSString*)kSCPropNetProxiesHTTPSEnable
                                 proxyURLKey:(NSString*)kSCPropNetProxiesHTTPSProxy
                                proxyPortKey:(NSString*)kSCPropNetProxiesHTTPSPort];

      gotAProxy |= [self updateOneProxy:proxyConfigDict protocol:@"ftp"
                              proxyEnableKey:(NSString*)kSCPropNetProxiesFTPEnable
                                 proxyURLKey:(NSString*)kSCPropNetProxiesFTPProxy
                                proxyPortKey:(NSString*)kSCPropNetProxiesFTPPort];

      gotAProxy |= [self updateOneProxy:proxyConfigDict protocol:@"gopher"
                              proxyEnableKey:(NSString*)kSCPropNetProxiesGopherEnable
                                 proxyURLKey:(NSString*)kSCPropNetProxiesGopherProxy
                                proxyPortKey:(NSString*)kSCPropNetProxiesGopherPort];
      
      gotAProxy |= [self updateOneProxy:proxyConfigDict protocol:@"socks"
                              proxyEnableKey:(NSString*)kSCPropNetProxiesSOCKSEnable
                                 proxyURLKey:(NSString*)kSCPropNetProxiesSOCKSProxy
                                proxyPortKey:(NSString*)kSCPropNetProxiesSOCKSPort];

      if (gotAProxy)
      {
        newProxyType = eProxyConfig_Manual;

        NSArray* exceptions = [proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesExceptionsList];
        if (exceptions)
        {
          NSString* sitesList = [exceptions componentsJoinedByString:@", "];
          if ([sitesList length] > 0)
            [self setPref:"network.proxy.no_proxies_on" toString:sitesList];
        }
      }
      else
      {
        // no proxy hosts found; turn them off
        newProxyType = eProxyConfig_Direct;
      }
    }
    
    [proxyConfigDict release];
  }

  if (newProxyType != curProxyType)
    mPrefs->SetIntPref("network.proxy.type", newProxyType);
}

#pragma mark -

- (void)adBlockingPrefChanged:(NSNotification*)inNotification
{
  BOOL adBlockingEnabled = [self getBooleanPref:"camino.enable_ad_blocking" withSuccess:nil];
  [self refreshAdBlockingStyleSheet:adBlockingEnabled];
}

// some versions of 0.9a copied ad_blocking.css into <profile>/chrome/userContent.css.
// now that we load ad_blocking.css dynamically, we have to move that file aside to
// avoid loading it.
// returns YES if there was a userContent.css in the chrome dir.
- (BOOL)cleanupUserContentCSS
{
  NSString* profilePath = [self newProfilePath];
  NSString* chromeDirPath = [profilePath stringByAppendingPathComponent:@"chrome"];
  NSString* userContentCSSPath = [chromeDirPath stringByAppendingPathComponent:@"userContent.css"];
  
  if ([[NSFileManager defaultManager] fileExistsAtPath:userContentCSSPath])
  {
    NSString* userContentBackPath = [chromeDirPath stringByAppendingPathComponent:@"userContent_unused.css"];
    BOOL moveSucceeded = [[NSFileManager defaultManager] movePath:userContentCSSPath toPath:userContentBackPath handler:nil];
    NSLog(@"Ad blocking now users a built-in CSS file; moving previous userContent.css file at\n  %@\nto\n  %@",
      userContentCSSPath, userContentBackPath);
    if (!moveSucceeded)
      NSLog(@"Move failed; does %@ exist already?", userContentBackPath);
    return YES;
  }
  
  return NO;
}

// this will reload the sheet if it's already registered, or unload it if the 
// param is NO
- (void)refreshAdBlockingStyleSheet:(BOOL)inLoad
{
  nsCOMPtr<nsIStyleSheetService> ssService = do_GetService("@mozilla.org/content/style-sheet-service;1");
  if (!ssService)
    return;

  // the the uri of the sheet in our bundle
  NSString* cssFilePath = [[NSBundle mainBundle] pathForResource:@"ad_blocking" ofType:@"css"];
  if (![[NSFileManager defaultManager] isReadableFileAtPath:cssFilePath])
  {
    NSLog(@"ad_blocking.css file not found; ad blocking will be disabled");
    return;
  }
  
  nsresult rv;
  nsCOMPtr<nsILocalFile> cssFile;
  rv = NS_NewNativeLocalFile(nsDependentCString([cssFilePath fileSystemRepresentation]), PR_TRUE, getter_AddRefs(cssFile));
  if (NS_FAILED(rv))
    return;
  
  nsCOMPtr<nsIURI> cssFileURI;
  rv = NS_NewFileURI(getter_AddRefs(cssFileURI), cssFile);
  if (NS_FAILED(rv))
    return;
  
  PRBool alreadyRegistered = PR_FALSE;
  rv = ssService->SheetRegistered(cssFileURI, nsIStyleSheetService::USER_SHEET, &alreadyRegistered);
  if (alreadyRegistered)
    ssService->UnregisterSheet(cssFileURI, nsIStyleSheetService::USER_SHEET);
  
  if (inLoad)
    ssService->LoadAndRegisterSheet(cssFileURI, nsIStyleSheetService::USER_SHEET);
}

#pragma mark -

- (NSString*)getStringPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  NSString *prefValue = @"";

  char *buf = nsnull;
  nsresult rv = NS_ERROR_FAILURE;
  if (mPrefs)
    rv = mPrefs->GetCharPref(prefName, &buf);

  if (NS_SUCCEEDED(rv) && buf) {
    // prefs are UTF-8
    prefValue = [NSString stringWithUTF8String:buf];
    free(buf);
    if (outSuccess) *outSuccess = YES;
  } else {
    if (outSuccess) *outSuccess = NO;
  }
  
  return prefValue;
}

- (NSColor*)getColorPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  // colors are stored in HTML-like #FFFFFF strings
  NSString* colorString = [self getStringPref:prefName withSuccess:outSuccess];
  NSColor*  returnColor = [NSColor blackColor];

  if ([colorString hasPrefix:@"#"] && [colorString length] == 7)
  {
    unsigned int redInt, greenInt, blueInt;
    sscanf([colorString cString], "#%02x%02x%02x", &redInt, &greenInt, &blueInt);
    
    float redFloat    = ((float)redInt / 255.0);
    float greenFloat  = ((float)greenInt / 255.0);
    float blueFloat   = ((float)blueInt / 255.0);
    
    returnColor = [NSColor colorWithCalibratedRed:redFloat green:greenFloat blue:blueFloat alpha:1.0f];
    if (outSuccess) *outSuccess = YES;
  }
  else
  {
    if (outSuccess) *outSuccess = NO;
  }

  return returnColor;
}

- (BOOL)getBooleanPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  PRBool boolPref = PR_FALSE;
  nsresult rv = NS_ERROR_FAILURE;
  if (mPrefs)
    rv = mPrefs->GetBoolPref(prefName, &boolPref);

  if (outSuccess)
    *outSuccess = NS_SUCCEEDED(rv);

  return boolPref ? YES : NO;
}

- (int)getIntPref: (const char*)prefName withSuccess:(BOOL*)outSuccess
{
  PRInt32 intPref = 0;
  nsresult rv = NS_ERROR_FAILURE;
  if (mPrefs)
    rv = mPrefs->GetIntPref(prefName, &intPref);

  if (outSuccess)
    *outSuccess = NS_SUCCEEDED(rv);

  return intPref;
}

- (void)setPref:(const char*)prefName toString:(NSString*)value
{
  if (mPrefs)
    (void)mPrefs->SetCharPref(prefName, [value UTF8String]);
}

- (void)setPref:(const char*)prefName toInt:(int)value
{
  if (mPrefs)
    (void)mPrefs->SetIntPref(prefName, (PRInt32)value);
}

- (void)setPref:(const char*)prefName toBoolean:(BOOL)value
{
  if (mPrefs)
    (void)mPrefs->SetBoolPref(prefName, (PRBool)value);
}

- (NSString *) homePageUsingStartPage:(BOOL)checkStartupPagePref
{
  if (!mPrefs)
    return @"about:blank";

  PRInt32 mode = 1;
  
  // In some cases, we need to check browser.startup.page to see if
  // we want to use the homepage or if the user wants a blank window.
  // mode 0 is blank page, mode 1 is home page. 2 is "last page visited"
  // but we don't care about that. Default to 1 unless |checkStartupPagePref|
  // is true.
  nsresult rv = NS_OK;
  if (checkStartupPagePref)
    rv = mPrefs->GetIntPref("browser.startup.page", &mode);

  if (NS_FAILED(rv) || mode == 1) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(mPrefs);
    if (!prefBranch) return @"about:blank";
    
    NSString* homepagePref = nil;
    PRInt32 haveUserPref;
    if (NS_FAILED(prefBranch->PrefHasUserValue("browser.startup.homepage", &haveUserPref)) || !haveUserPref)
    {
      // no home page pref is set in user prefs.
      homepagePref = NSLocalizedStringFromTable( @"HomePageDefault", @"WebsiteDefaults", nil);
      // and let's copy this into the homepage pref if it's not bad
      if (![homepagePref isEqualToString:@"HomePageDefault"])
        mPrefs->SetCharPref("browser.startup.homepage", [homepagePref UTF8String]);
    }
    else {
      homepagePref = [self getStringPref:"browser.startup.homepage" withSuccess:NULL];
    }

    if (homepagePref && [homepagePref length] > 0 && ![homepagePref isEqualToString:@"HomePageDefault"])
      return homepagePref;
  }
  
  return @"about:blank";
}

- (NSString *)searchPage
{
  NSString* resultString = @"http://www.google.com/";
  if (!mPrefs)
    return resultString;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(mPrefs);
  if (!prefBranch)
    return resultString;

  NSString* searchPagePref = nil;
  PRBool haveUserPref = PR_FALSE;
  prefBranch->PrefHasUserValue("chimera.search_page", &haveUserPref);
  if (haveUserPref)
    searchPagePref = [self getStringPref:"chimera.search_page" withSuccess:NULL];
  
  if (!haveUserPref || (searchPagePref == NULL) || ([searchPagePref length] == 0))
  {
    // no home page pref is set in user prefs, or it's an empty string
    searchPagePref = NSLocalizedStringFromTable( @"SearchPageDefault", @"WebsiteDefaults", nil);
    // and let's copy this into the homepage pref if it's not bad
    if (![searchPagePref isEqualToString:@"SearchPageDefault"])
      mPrefs->SetCharPref("chimera.search_page", [searchPagePref UTF8String]);
  }

  if (searchPagePref && [searchPagePref length] > 0 && ![searchPagePref isEqualToString:@"SearchPageDefault"])
    return searchPagePref;
  
  return resultString;
}

//
// -oldProfilePath
//
// Find the path to the pre-0.8 profile folder using the old registry. It will be
// along the lines of ~/Library/Application Support/Chimera/profiles/default/xxxxx.slt/
// Returns |nil| if there are any problems finding the path.
//
- (NSString *) oldProfilePath
{
#define kRegistryProfileSubtreeString (NS_LITERAL_STRING("Profiles"))
#define kRegistryCurrentProfileString (NS_LITERAL_STRING("CurrentProfile"))
#define kRegistryDirectoryString (NS_LITERAL_STRING("directory"))

  NSString *resultPath = nil;

  // The old registry file is at ~/Library/Application Support/Chimera/Application.regs
  FSRef foundRef;
  OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType,
                            kCreateFolder, &foundRef);
  if (err != noErr)
    return nil;
      
  UInt8 pathBuf[PATH_MAX];                        
  err = FSRefMakePath(&foundRef, pathBuf, sizeof(pathBuf));
  if (err != noErr)
    return nil;

  NSString *oldDirName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"mozOldProfileDirName"];
  if (!oldDirName) {
    NSLog(@"mozNewProfileDirName key missing from Info.plist file - using default");
    oldDirName = @"Chimera";
  }
  NSString *registryPath = [[[NSString stringWithUTF8String:(char*)pathBuf]
                              stringByAppendingPathComponent:oldDirName]
                              stringByAppendingPathComponent:@"Application.regs"];

  nsresult rv;
  nsCOMPtr<nsILocalFile> registryFile;
  rv = NS_NewNativeLocalFile(nsDependentCString([registryPath fileSystemRepresentation]),
                              PR_TRUE, getter_AddRefs(registryFile));
  if (NS_FAILED(rv))
    return nil;
  nsCOMPtr<nsIRegistry> registry = do_CreateInstance(NS_REGISTRY_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return nil;
  rv = registry->Open(registryFile);
  if (NS_FAILED(rv))
    return nil;
  
  nsRegistryKey profilesTreeKey;
  rv = registry->GetKey(nsIRegistry::Common,
                          kRegistryProfileSubtreeString.get(),
                          &profilesTreeKey);
  if (NS_FAILED(rv))
    return nil;

  // Get the current profile
  nsXPIDLString currProfileName;
  rv = registry->GetString(profilesTreeKey,
                             kRegistryCurrentProfileString.get(),
                             getter_Copies(currProfileName));
  if (NS_FAILED(rv))
    return nil;
                             
  nsRegistryKey currProfileKey;
  rv = registry->GetKey(profilesTreeKey,
                          currProfileName.get(),
                          &currProfileKey);
  if (NS_FAILED(rv))
    return nil;

  nsXPIDLString profDirDesc;
  rv = registry->GetString(currProfileKey,
                           kRegistryDirectoryString.get(),
                           getter_Copies(profDirDesc));
  if (NS_FAILED(rv))
    return nil;

  nsCOMPtr<nsILocalFile> profDirFile;
  rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(profDirFile));
  if (NS_SUCCEEDED(rv)) {
    // profDirDesc is ASCII so no loss
    rv = profDirFile->SetPersistentDescriptor(NS_LossyConvertUCS2toASCII(profDirDesc));
    PRBool exists;
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(profDirFile->Exists(&exists)) && exists) {
      nsCAutoString nativePath;
      profDirFile->GetNativePath(nativePath);
      resultPath = [NSString stringWithUTF8String:nativePath.get()];

      // If the descriptor, which is an alias, is broken, this would be the place to
      // do some exhaustive alias searching, prompt the user, etc.    
    }
  }
  return resultPath;
}

//
// -newProfilePath
//
// Returns the path for our post 0.8 profiles stored in Application Support/Camino.
// We no longer have distinct profiles. The profile dir is the same as
// NS_APP_USER_PROFILES_ROOT_DIR - imposed by our own AppDirServiceProvider. Will
// return |nil| if there is a problem.
//
- (NSString*) newProfilePath
{
  nsCOMPtr<nsIFile> appSupportDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR,
                                       getter_AddRefs(appSupportDir));
  if (NS_FAILED(rv))
    return nil;
  nsCAutoString nativePath;
  rv = appSupportDir->GetNativePath(nativePath);
  if (NS_FAILED(rv))
    return nil;
  return [NSString stringWithUTF8String:nativePath.get()];
}

//
// -migrateChimeraProfile:
//
// Takes the old profile and copies out the pertinent info into the new profile
// given by the path in |newProfilePath|. Copies everything except the cache dir.
//
- (void)migrateChimeraProfile:(NSString*)newProfilePath;
{
  NSFileManager *fileMgr = [NSFileManager defaultManager];
  NSString *oldProfilePath = [self oldProfilePath];
  if (!oldProfilePath)
    return;
  
  BOOL exists, isDir;
  exists = [fileMgr fileExistsAtPath:oldProfilePath isDirectory:&isDir];
  if (!exists || !isDir)
    return;
      
  // The parent of the terminal node in the dest path given to copyPath has to exist already.
  exists = [fileMgr fileExistsAtPath:newProfilePath isDirectory:&isDir];
  if (exists && !isDir) {
    NSLog(@"Can't migrate profile to %@: there's a file in the way", newProfilePath);
    return;
  }
  else if (!exists) {
    NSLog(@"%@ should exist if [self newProfilePath] has been called", newProfilePath);
    return;
  }
  
  NSArray *profileContents = [fileMgr directoryContentsAtPath:oldProfilePath];
  NSEnumerator *enumerator = [profileContents objectEnumerator];
  id anItem;
  
  // loop over the contents of the profile copying everything except for invisible
  // files and the cache
  while ((anItem = [enumerator nextObject])) {
    NSString *sourcePath = [oldProfilePath stringByAppendingPathComponent:anItem];
    NSString *destPath = [newProfilePath stringByAppendingPathComponent:anItem];
    // Ensure that the file exists
    if ([fileMgr fileExistsAtPath:sourcePath isDirectory:&isDir]) {
      // That it's not invisible (.parentlock, .DS_Store)
      if ([anItem hasPrefix:@"."] == NO) {
        // That it's not the Cache or Cache.Trash dir
        if (!isDir || ![anItem hasPrefix:@"Cache"])
            [fileMgr copyPath:sourcePath toPath:destPath handler:nil];
      }
    }
  }
}


- (void)addObserver:(id)inObject forPref:(const char*)inPrefName
{
  if (!mPrefChangeObservers)
    mPrefChangeObservers = [[NSMutableDictionary alloc] initWithCapacity:4];

  NSString* prefName = [NSString stringWithUTF8String:inPrefName];

  // get the array of pref observers for this pref
  NSMutableArray* existingObservers = [mPrefChangeObservers objectForKey:prefName];
  if (!existingObservers)
  {
    existingObservers = [NSMutableArray arrayWithCapacity:1];
    [mPrefChangeObservers setObject:existingObservers forKey:prefName];
  }

  // look for an existing observer with this target object
  NSEnumerator* observersEnum = [existingObservers objectEnumerator];
  PrefChangeObserverOwner* curValue;
  while ((curValue = [observersEnum nextObject]))
  {
    if ([curValue hasObject:inObject])
      return;   // found it; nothing to do
  }

  // if it doesn't exist, make one
  PrefChangeObserverOwner* observerOwner = [[PrefChangeObserverOwner alloc] initWithPrefName:prefName object:inObject];
  [existingObservers addObject:observerOwner];    // takes ownership
  [observerOwner release];
}

- (void)removeObserver:(id)inObject
{
  NSEnumerator* observerArraysEnum = [mPrefChangeObservers objectEnumerator];
  NSMutableArray* curArray;
  while ((curArray = [observerArraysEnum nextObject]))
  {
    // look for an existing observer with this target object
    NSEnumerator* observersEnum = [curArray objectEnumerator];
    PrefChangeObserverOwner* prefObserverOwner = nil;
    
    PrefChangeObserverOwner* curValue;
    while ((curValue = [observersEnum nextObject]))
    {
      if ([curValue hasObject:inObject])
      {
        prefObserverOwner = curValue;
        break;
      }
    }
    
    if (prefObserverOwner)
      [curArray removeObjectIdenticalTo:prefObserverOwner];   // this should release it and unregister the observer
  }
}

- (void)removeObserver:(id)inObject forPref:(const char*)inPrefName
{
  NSString* prefName = [NSString stringWithUTF8String:inPrefName];

  NSMutableArray* existingObservers = [mPrefChangeObservers objectForKey:prefName];
  if (!existingObservers) return;

  // look for an existing observer with this target object
  NSEnumerator* observersEnum = [existingObservers objectEnumerator];
  PrefChangeObserverOwner* prefObserverOwner = nil;
  
  PrefChangeObserverOwner* curValue;
  while ((curValue = [observersEnum nextObject]))
  {
    if ([curValue hasObject:inObject])
    {
      prefObserverOwner = curValue;
      break;
    }
  }
  
  if (prefObserverOwner)
    [existingObservers removeObjectIdenticalTo:prefObserverOwner];   // this should release it and unregister the observer
}

@end
