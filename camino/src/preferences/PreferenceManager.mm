/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import <Cocoa/Cocoa.h>

#import <SystemConfiguration/SystemConfiguration.h>

#import "PreferenceManager.h"
#import "UserDefaults.h"
#import "CHBrowserService.h"

#include "nsIServiceManager.h"
#include "nsIProfile.h"
#include "nsIPref.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsString.h"
#include "nsEmbedAPI.h"
#include "AppDirServiceProvider.h"


#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

@interface PreferenceManager(PreferenceManagerPrivate)

+ (PreferenceManager *) sharedInstanceDontCreate;

- (void)registerNotificationListener;

- (void)termEmbedding: (NSNotification*)aNotification;
- (void)xpcomTerminate: (NSNotification*)aNotification;

- (void)configureProxies;
- (BOOL)updateOneProxy:(NSDictionary*)configDict
    protocol:(NSString*)protocol
    proxyEnableKey:(NSString*)enableKey
    proxyURLKey:(NSString*)urlKey
    proxyPortKey:(NSString*)portKey;

- (void)registerForProxyChanges;
- (BOOL)readSystemProxySettings;

@end



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

    if ([self initInternetConfig] == NO) {
      // XXXw. throw here
      NSLog (@"Failed to initialize Internet Config");
    }

    if ([self initMozillaPrefs] == NO) {
      // XXXw. throw here too
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
  ::ICStop(mInternetConfig);
  mInternetConfig = nil;
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
}

- (void)savePrefsFile
{
  nsCOMPtr<nsIPrefService> prefsService = do_GetService(NS_PREF_CONTRACTID);
  if (prefsService)
      prefsService->SavePrefFile(nsnull);
}

- (BOOL)initInternetConfig
{
    OSStatus error;
    error = ::ICStart(&mInternetConfig, 'CHIM');
    if (error != noErr) {
        // XXX throw here?
        NSLog(@"Error initializing IC");
        return NO;
    }
    return YES;
}

- (BOOL)initMozillaPrefs
{

#ifdef _BUILD_STATIC_BIN
    // Initialize XPCOM's module info table
    NSGetStaticModuleInfo = app_getModuleInfo;
#endif

    nsresult rv;

    NSString *path = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    const char *binDirPath = [path fileSystemRepresentation];
    nsCOMPtr<nsILocalFile> binDir;
    rv = NS_NewNativeLocalFile(nsDependentCString(binDirPath), PR_TRUE, getter_AddRefs(binDir));
    if (NS_FAILED(rv))
      return NO;

    // This shouldn't be needed since we are initing XPCOM with this
    // directory but causes a (harmless) warning if not defined.
    setenv("MOZILLA_FIVE_HOME", binDirPath, 1);

    // get the 'mozProfileDirName' key from our Info.plist file
    NSString *dirString = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"mozProfileDirName"];
    const char* profileDirName;
    if (dirString)
      profileDirName = [dirString cString];
    else {
      NSLog(@"mozProfileDirName key missing from Info.plist file. Using default profile directory");
      profileDirName = "Chimera";
    }
    
    // Supply our own directory service provider so we can control where
    // the registry and profiles are located.
    AppDirServiceProvider *provider = new AppDirServiceProvider(nsDependentCString(profileDirName));
    if (!provider) return NO;

    nsCOMPtr<nsIDirectoryServiceProvider> dirProvider = (nsIDirectoryServiceProvider*)provider;
    rv = NS_InitEmbedding(binDir, dirProvider);
    if (NS_FAILED(rv)) {
      NSLog(@"Embedding init failed.");
      return NO;
    }
    
    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return NO;
    
    nsAutoString newProfileName(NS_LITERAL_STRING("default"));
    PRBool profileExists = PR_FALSE;
    rv = profileService->ProfileExists(newProfileName.get(), &profileExists);
    if (NS_FAILED(rv))
        return NO;

    if (!profileExists) {
        rv = profileService->CreateNewProfile(newProfileName.get(), nsnull, nsnull, PR_FALSE);
        if (NS_FAILED(rv))
            return NO;
    }

    rv = profileService->SetCurrentProfile(newProfileName.get());
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_FILE_ACCESS_DENIED) {
        NSString *alert = [NSString stringWithFormat: NSLocalizedString(@"AlreadyRunningAlert", @""), NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)];
        NSString *message = [NSString stringWithFormat: NSLocalizedString(@"AlreadyRunningMsg", @""), NSLocalizedStringFromTable(@"CFBundleName", @"InfoPlist", nil)];
        NSString *quit = NSLocalizedString(@"AlreadyRunningButton",@"");
        NSRunAlertPanel(alert,message,quit,nil,nil);
        [NSApp terminate:self];
      }
      return NO;
    }
    else if (rv == NS_ERROR_PROFILE_SETLOCK_FAILED) {
      NSLog(@"SetCurrentProfile returned NS_ERROR_PROFILE_SETLOCK_FAILED");
    }

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    mPrefs = prefs;
    NS_IF_ADDREF(mPrefs);
    
    [self syncMozillaPrefs];
    return YES;
}

- (void)syncMozillaPrefs
{
    if (!mPrefs) {
        NSLog(@"Mozilla prefs not set up successfully");
        return;
    }

    // set the universal charset detector pref. we can't set it in chimera.js
    // because it's a funky locale-specific pref.
    static const char* const kUniversalCharsetDetectorPref = "intl.charset.detector";
    mPrefs->SetCharPref(kUniversalCharsetDetectorPref, "universal_charset_detector");
    
    // fix up the cookie prefs. If 'p3p' or 'accept foreign cookies' are on, remap them to
    // something that chimera can deal with.
    PRInt32 acceptCookies = 0;
    static const char* kCookieBehaviorPref = "network.cookie.cookieBehavior";
    mPrefs->GetIntPref(kCookieBehaviorPref, &acceptCookies);
    if ( acceptCookies == 1 ) {          // accept foreign cookies, assume off  
      acceptCookies = 2;
      mPrefs->SetIntPref(kCookieBehaviorPref, acceptCookies);
    }
    else if ( acceptCookies == 3 ) {     // p3p, assume all cookies on
      acceptCookies = 0;
      mPrefs->SetIntPref(kCookieBehaviorPref, acceptCookies);
    }

    [self configureProxies];
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

- (BOOL)readSystemProxySettings
{
  BOOL usingProxies = NO;
  
  PRInt32 proxyType, newProxyType;
  mPrefs->GetIntPref("network.proxy.type", &proxyType);
  newProxyType = proxyType;
  if (proxyType == 0 || proxyType == 1)
  {
    // get proxies from SystemConfiguration
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

    NSDictionary* proxyConfigDict = (NSDictionary*)SCDynamicStoreCopyProxies(NULL);
    if (proxyConfigDict)
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
        newProxyType = 1;

        NSArray* exceptions = [proxyConfigDict objectForKey:(NSString*)kSCPropNetProxiesExceptionsList];
        if (exceptions)
        {
          NSString* sitesList = [exceptions componentsJoinedByString:@", "];
          if ([sitesList length] > 0)
            [self setPref:"network.proxy.no_proxies_on" toString:sitesList];
        }
        usingProxies = YES;
      }
      else
      {
        newProxyType = 0;
      }
      
      [proxyConfigDict release];
    }

    if (newProxyType != proxyType)
      mPrefs->SetIntPref("network.proxy.type", 1);    
  }
  
  return usingProxies;
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
    mPrefs->GetIntPref(prefName, &intPref);
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


//- (BOOL) getICBoolPref:(ConstStr255Param) prefKey;
//{
//    ICAttr dummy;
//    OSStatus error;
//    SInt32 size;
//    Boolean buf;

//    error = ICGetPref (internetConfig, prefKey, &dummy, &buf, &size);
//    if (error == noErr && buf)
//        return YES;
//    else
//        return NO;
// }

- (NSString *) getICStringPref:(ConstStr255Param) prefKey;
{
    NSString *string;
    ICAttr dummy;
    OSStatus error;
    SInt32 size = 256;
    char *buf;

    do {
        if ((buf = (char*)malloc((unsigned int)size)) == NULL) {
            NSLog (@"malloc failed in [PreferenceManager getICStringPref]");
            return nil;
        }
        error = ICGetPref (mInternetConfig, prefKey, &dummy, buf, &size);
        if (error != noErr && error != icTruncatedErr) {
            free (buf);
            NSLog (@"[IC error %d in [PreferenceManager getICStringPref]", (int) error);
            return nil;
        }
        size *= 2;
    } while (error == icTruncatedErr);
    if (*buf == 0) {
        NSLog (@"ICGetPref returned empty string");
        free (buf);
        return nil;
    }
    CopyPascalStringToC ((ConstStr255Param) buf, buf);
    string = [NSString stringWithCString:buf];
    free(buf);
    return string;
}


- (NSString *) homePage:(BOOL)checkStartupPagePref
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
  if ( checkStartupPagePref )
    rv = mPrefs->GetIntPref("browser.startup.page", &mode);
  if (NS_FAILED(rv) || mode == 1) {
    // see which home page to use
    PRBool boolPref;
    if (NS_SUCCEEDED(mPrefs->GetBoolPref("chimera.use_system_home_page", &boolPref)) && boolPref)
      return [self getICStringPref:kICWWWHomePage];

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

  PRBool boolPref;
  if (NS_SUCCEEDED(mPrefs->GetBoolPref("chimera.use_system_home_page", &boolPref)) && boolPref)
    return [self getICStringPref:kICWebSearchPagePrefs];

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


@end

