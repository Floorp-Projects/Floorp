#import <Foundation/NSUserDefaults.h>
#import <Carbon/Carbon.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreServices/CoreServices.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import "CHPreferenceManager.h"
#include "nsIServiceManager.h"
#include "nsIProfile.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsString.h"
#include "nsEmbedAPI.h"

extern const char *prefContractID;

@implementation CHPreferenceManager

- (id) init
{
    if ((self = [super init])) {
        if ([self initInternetConfig] == NO) {
            // XXXw. throw here
            NSLog (@"Failed to initialize Internet Config.\n");
        }
        if ([self initMozillaPrefs] == NO) {
            // XXXw. throw here too
            NSLog (@"Failed to initialize mozilla prefs.\n");
        }
    }
    return self;
}

- (void) dealloc
{
    nsresult rv;
    ICStop (internetConfig);
    nsCOMPtr<nsIPrefService> pref(do_GetService(prefContractID, &rv));
    if (!NS_FAILED(rv))
        pref->SavePrefFile(nsnull);
    [super dealloc];
}

- (BOOL) initInternetConfig
{
    OSStatus error;
    error = ICStart (&internetConfig, '????');
    if (error != noErr) {
        // XXX throw here?
        NSLog(@"Error initializing IC.\n");
        return NO;
    }
    return YES;
}

- (BOOL) initMozillaPrefs
{
  nsresult rv = NS_InitEmbedding(nsnull, nsnull);
  if (NS_FAILED(rv))
    return NO;

    NSBundle *mainBundle = [NSBundle mainBundle];
    NSString *path = [mainBundle bundlePath];
    NSMutableString *mutablePath = [NSMutableString stringWithString:path];
    [mutablePath appendString:@"/Contents/MacOS/"];
    const char *cstr = [mutablePath cString];
    setenv("MOZILLA_FIVE_HOME", cstr, 1);

    nsCOMPtr<nsIProfile> profileService(do_GetService(NS_PROFILE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return NO;
    
    nsAutoString newProfileName(NS_LITERAL_STRING("Chimera"));
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
    if (NS_FAILED(rv))
        return NO;

    [self syncMozillaPrefs];
    return YES;
}

- (void) syncMozillaPrefs
{
    CFArrayRef cfArray;
    CFDictionaryRef cfDictionary;
    CFNumberRef cfNumber;
    CFStringRef cfString;
    char strbuf[1024];
    int numbuf;
    NSString *string;

    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(prefContractID));
    if (!prefs) {
        // XXXw. throw?
        return;
    }

    // get home page from Internet Config
    string = [self getICStringPref:kICWWWHomePage];
    if (string) {
        prefs->SetCharPref("browser.startup.homepage", [string cString]);
    }

    // get proxies from SystemConfiguration
    prefs->SetIntPref("network.proxy.type", 0); // 0 == no proxies
    prefs->ClearUserPref("network.proxy.http");
    prefs->ClearUserPref("network.proxy.http_port");
    prefs->ClearUserPref("network.proxy.ssl");
    prefs->ClearUserPref("network.proxy.ssl_port");
    prefs->ClearUserPref("network.proxy.ftp");
    prefs->ClearUserPref("network.proxy.ftp_port");
    prefs->ClearUserPref("network.proxy.gopher");
    prefs->ClearUserPref("network.proxy.gopher_port");
    prefs->ClearUserPref("network.proxy.socks");
    prefs->ClearUserPref("network.proxy.socks_port");
    prefs->ClearUserPref("network.proxy.no_proxies_on");

    if ((cfDictionary = SCDynamicStoreCopyProxies (NULL)) != NULL) {
        cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesHTTPEnable);
        if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE && numbuf == 1) {
            cfString = (CFStringRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesHTTPProxy);
            if (CFStringGetCString (cfString, &strbuf[0], sizeof(strbuf)-1, kCFStringEncodingASCII) == TRUE) {
                prefs->SetCharPref("network.proxy.http", strbuf);
            }
            cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesHTTPPort);
            if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE) {
                prefs->SetIntPref("network.proxy.http_port", numbuf);
            }
            prefs->SetIntPref("network.proxy.type", 1);
        }
        cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesHTTPSEnable);
        if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE && numbuf == 1) {
            cfString = (CFStringRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesHTTPSProxy);
            if (CFStringGetCString (cfString, &strbuf[0], sizeof(strbuf)-1, kCFStringEncodingASCII) == TRUE) {
                prefs->SetCharPref("network.proxy.ssl", strbuf);
            }
            cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesHTTPSPort);
            if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE) {
                prefs->SetIntPref("network.proxy.ssl_port", numbuf);
            }
            prefs->SetIntPref("network.proxy.type", 1);
        }
        cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesFTPEnable);
        if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE && numbuf == 1) {
            cfString = (CFStringRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesFTPProxy);
            if (CFStringGetCString (cfString, &strbuf[0], sizeof(strbuf)-1, kCFStringEncodingASCII) == TRUE) {
                prefs->SetCharPref("network.proxy.ftp", strbuf);
            }
            cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesFTPPort);
            if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE) {
                prefs->SetIntPref("network.proxy.ftp_port", numbuf);
            }
            prefs->SetIntPref("network.proxy.type", 1);
        }
        cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesGopherEnable);
        if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE && numbuf == 1) {
            cfString = (CFStringRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesGopherProxy);
            if (CFStringGetCString (cfString, &strbuf[0], sizeof(strbuf)-1, kCFStringEncodingASCII) == TRUE) {
                prefs->SetCharPref("network.proxy.gopher", strbuf);
            }
            cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesGopherPort);
            if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE) {
                prefs->SetIntPref("network.proxy.gopher_port", numbuf);
            }
            prefs->SetIntPref("network.proxy.type", 1);
        }
        cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesSOCKSEnable);
        if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE && numbuf == 1) {
            cfString = (CFStringRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesSOCKSProxy);
            if (CFStringGetCString (cfString, &strbuf[0], sizeof(strbuf)-1, kCFStringEncodingASCII) == TRUE) {
                prefs->SetCharPref("network.proxy.socks", strbuf);
            }
            cfNumber = (CFNumberRef)CFDictionaryGetValue (cfDictionary, kSCPropNetProxiesSOCKSPort);
            if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numbuf) == TRUE) {
                prefs->SetIntPref("network.proxy.socks_port", numbuf);
            }
            prefs->SetIntPref("network.proxy.type", 1);
        }
        if (CFDictionaryGetValueIfPresent (cfDictionary, kSCPropNetProxiesExceptionsList, (const void **)&cfArray) == TRUE) {
            cfString = CFStringCreateByCombiningStrings (NULL, cfArray, CFSTR(", "));
            if (CFStringGetLength (cfString) > 0) {
                if (CFStringGetCString (cfString, &strbuf[0], sizeof(strbuf)-1, kCFStringEncodingASCII) == TRUE) {
                    prefs->SetCharPref("network.proxy.no_proxies_on", strbuf);
                }
            }
        }
        CFRelease (cfDictionary);
    }
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
        if ((buf = malloc ((unsigned int)size)) == NULL) {
            NSLog (@"malloc failed in [PreferenceManager getICStringPref].");
            return nil;
        }
        error = ICGetPref (internetConfig, prefKey, &dummy, buf, &size);
        if (error != noErr && error != icTruncatedErr) {
            free (buf);
            NSLog (@"[IC error %d in [PreferenceManager getICStringPref].\n", (int) error);
            return nil;
        }
        size *= 2;
    } while (error == icTruncatedErr);
    if (*buf == 0) {
        NSLog (@"ICGetPref returned empty string.");
        free (buf);
        return nil;
    }
    CopyPascalStringToC ((ConstStr255Param) buf, buf);
    string = [NSString stringWithCString:buf];
    free (buf);
    return string;
}

- (NSString *) homePage
{
    NSString *url;
    char *buf;
    PRInt32 mode;
    nsresult rv;
    NSLog(@"getting home page");
    
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(prefContractID));
    if (!prefs)
        return @"about:blank";

    // copied from Gecko: mode 0 is blank page, mode 1 is home page.
    // 2 is "last page visited" but we don't care about that.
    rv = prefs->GetIntPref("browser.startup.page", &mode);
    // if the pref isn't set, default to mode 1
    NSLog(@"startup.page: %d", (int)mode);
    if (NS_FAILED(rv) || mode == 1) {
        prefs->GetCharPref("browser.startup.homepage", &buf);
        if (buf && *buf)
            url = [NSString stringWithCString:buf];
        else
            url = @"about:blank";
        free (buf);
        return url;
    } else
        return @"about:blank";
}

@end

