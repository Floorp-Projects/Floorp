
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benoit Foucher <bfoucher@mac.com>
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

#import "NSString+Utils.h"
#import "KeychainService.h"
#import "CHBrowserService.h"
#import "PreferenceManager.h"

#import <CoreServices/CoreServices.h>
#import <Carbon/Carbon.h>

#include "nsIPref.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIURL.h"
#include "nsIDOMWindowCollection.h"
#include "nsIContent.h"
#include "nsIWindowWatcher.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsAppDirectoryServiceDefs.h"

extern NSString* XPCOMShutDownNotificationName;


nsresult
FindUsernamePasswordFields(nsIDOMHTMLFormElement* inFormElement, nsIDOMHTMLInputElement** outUsername,
                            nsIDOMHTMLInputElement** outPassword, PRBool inStopWhenFound);


@implementation KeychainService

static KeychainService *sInstance = nil;
static const char* const gUseKeychainPref = "chimera.store_passwords_with_keychain";
static const char* const gAutoFillEnabledPref = "chimera.keychain_passwords_autofill";

int KeychainPrefChangedCallback(const char* pref, void* data);

//
// KeychainPrefChangedCallback
//
// Pref callback to tell us when the pref values for using the keychain
// have changed. We need to re-cache them at that time.
//
int KeychainPrefChangedCallback(const char* inPref, void* unused)
{
  BOOL success = NO;
  if ( strcmp(inPref, gUseKeychainPref) == 0 )
    [KeychainService instance]->mIsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:gUseKeychainPref withSuccess:&success];
  else if ( strcmp(inPref, gAutoFillEnabledPref) == 0)
    [KeychainService instance]->mIsAutoFillEnabled = [[PreferenceManager sharedInstance] getBooleanPref:gAutoFillEnabledPref withSuccess:&success];
  
  return NS_OK;
}


+ (KeychainService*) instance
{
  return sInstance ? sInstance : sInstance = [[self alloc] init];
}

- (id) init
{
  if ( (self = [super init]) ) {
    // Add a new form submit observer. We explicitly hold a ref in case the
    // observer service uses a weakref.
    nsCOMPtr<nsIObserverService> svc = do_GetService("@mozilla.org/observer-service;1");
    NS_ASSERTION(svc, "Keychain can't get observer service");
    mFormSubmitObserver = new KeychainFormSubmitObserver();
    if ( mFormSubmitObserver && svc ) {
      NS_ADDREF(mFormSubmitObserver);
      svc->AddObserver(mFormSubmitObserver, NS_FORMSUBMIT_SUBJECT, PR_FALSE);
    }
  
    // register for the cocoa notification posted when XPCOM shutdown so we
    // can unregister the pref callbacks we register below
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(shutdown:) name:XPCOMShutDownNotificationName
      object:nil];
    
    // cache the values of the prefs and register pref-changed callbacks. Yeah, I know
    // nsIPref is obsolete, but i'm not about to create an nsIObserver just for this.
    mIsEnabled = NO;
    mIsAutoFillEnabled = NO;
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
    if ( pref ) {
      BOOL success = NO;
      mIsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:gUseKeychainPref withSuccess:&success];
      mIsAutoFillEnabled = [[PreferenceManager sharedInstance] getBooleanPref:gAutoFillEnabledPref withSuccess:&success];
      if ( pref ) {
        pref->RegisterCallback(gUseKeychainPref, KeychainPrefChangedCallback, nsnull);
        pref->RegisterCallback(gAutoFillEnabledPref, KeychainPrefChangedCallback, nsnull);
      }
    }
    
    // load the keychain.nib file with our dialogs in it
    BOOL success = [NSBundle loadNibNamed:@"Keychain" owner:self];
    NS_ASSERTION(success, "can't load keychain prompt dialogs");
  }
  return self;
}

- (void) dealloc
{
  // unregister for shutdown notification. It may have already happened, but just in case.
  NS_IF_RELEASE(mFormSubmitObserver);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  
  [super dealloc];
}


//
// shutdown:
//
// Called in response to the cocoa notification "XPCOM Shutdown" sent by the cocoa
// browser service before it terminates embedding and shuts down xpcom. Allows us
// to get rid of anything we're holding onto for the length of the app.
//
- (void) shutdown:(id)unused
{
  // unregister ourselves as listeners of prefs before prefs go away
  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
  if ( pref ) {
    pref->UnregisterCallback(gUseKeychainPref, KeychainPrefChangedCallback, nsnull);
    pref->UnregisterCallback(gAutoFillEnabledPref, KeychainPrefChangedCallback, nsnull);
  }
  
  [sInstance release];
}


- (BOOL) isEnabled
{
  return mIsEnabled;
}

- (BOOL) isAutoFillEnabled
{
  return mIsAutoFillEnabled;
}

//
// getUsernameAndPassword:user:password:item
//
// looks up the username and password based on the realm. |username| and |pwd| must
// be existing, non-null NSMutableStrings and are filled in by this method. It
// also fills in the keychain's reference to this item in |outItemRef| so that
// routines following this don't have to look it up again to make changes.
//
- (BOOL) getUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort user:(NSMutableString*)username password:(NSMutableString*)pwd item:(KCItemRef*)outItemRef 
{
  if ( !outItemRef )
    return false;
  if ( !realm || ![realm length] )
    return false;

  const int kBufferLen = 255;
  OSStatus status;

  char buffer[kBufferLen];
  UInt32 actualSize;

  if ( inPort == -1 )
    inPort = kAnyPort;
  if(kcfindinternetpassword([realm UTF8String], 0, 0, inPort, kKCProtocolTypeHTTP, kKCAuthTypeHTTPDigest, 
                            kBufferLen, buffer, &actualSize, outItemRef) != noErr)
    return false;

  //
  // Set password and username
  //
  buffer[actualSize] = NULL;
  [pwd setString:[NSString stringWithUTF8String:buffer]];

  KCAttribute attr;
  attr.tag = kAccountKCItemAttr;
  attr.length = kBufferLen;
  attr.data = buffer;
  status = KCGetAttribute( *outItemRef, &attr, &actualSize );
  
  buffer[actualSize] = NULL;
  [username setString:[NSString stringWithUTF8String:buffer]];
  
  return true;
}

- (BOOL) findUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort
{
  KCItemRef ignore;
	return [self getUsernameAndPassword:realm port:inPort user:[NSMutableString string] password:[NSMutableString string] item:&ignore];
}


//
// updateUsernameAndPassword:user:passowrd:item
//
// updates an existing username and password associated with the given realm. If |inItemRef| is
// a valid keychain item, it uses that (bypassing additional queries to the chain), if not
// it will look it up based on the realm.
//
- (void) updateUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort user:(NSString*)username password:(NSString*)pwd item:(KCItemRef)inItemRef
{
  const int kBufferLen = 255;
  OSStatus status;

  char buffer[kBufferLen];
  UInt32 actualSize;

  if ( !inItemRef ) {
    if ( inPort == -1 )
      inPort = kAnyPort;
    if(kcfindinternetpassword([realm UTF8String], 0, 0, inPort, kKCProtocolTypeHTTP, kKCAuthTypeHTTPDigest, 
                              kBufferLen, buffer, &actualSize, &inItemRef) != noErr)
      return;
  }
  KCAttribute attr;

  //
  // Update item username and password.
  //
  attr.tag = kAccountKCItemAttr;
  attr.data = (char*)[username UTF8String];
  attr.length = strlen(NS_REINTERPRET_CAST(const char*, attr.data));
  status = KCSetAttribute(inItemRef, &attr);
  if(status != noErr)
    NSLog(@"Couldn't update keychain item account");

  status = KCSetData(inItemRef, strlen([pwd UTF8String]), [pwd UTF8String]);
  if(status != noErr)
    NSLog(@"Couldn't update keychain item data");

  //
  // Update the item now.
  //
  status = KCUpdateItem(inItemRef);
  if(status != noErr)
    NSLog(@"Couldn't update keychain item");
}


//
// storeUsernameAndPassword:port:user:password
//
// Adds a new username/password combo to the keychain based on the given realm
//
- (void) storeUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort user:(NSString*)username password:(NSString*)pwd
{
  if ( inPort == -1 )
    inPort = kAnyPort;
  kcaddinternetpassword([realm UTF8String], 0, [username UTF8String], inPort, kKCProtocolTypeHTTP, kKCAuthTypeHTTPDigest,
                        strlen([pwd UTF8String]), [pwd UTF8String], 0);
}


//
// removeUsernameAndPassword:item
//
// removes the username/password combo from the keychain. If |inItemRef| is a valid item, it
// uses that. If it's a null ref, it will look it up in the keychain based on the realm.
//
- (void) removeUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort item:(KCItemRef)inItemRef
{
  if ( !inItemRef ) {
    if ( inPort == -1 )
      inPort = kAnyPort;
    const int kBufferLen = 255;
    char buffer[kBufferLen];
    UInt32 actualSize;
    kcfindinternetpassword([realm UTF8String], 0, 0, inPort, kKCProtocolTypeHTTP, kKCAuthTypeHTTPDigest, 
                            kBufferLen, buffer, &actualSize, &inItemRef);
  }
                            
  if ( inItemRef )
    KCDeleteItem(inItemRef);
}

//
// addListenerToView:
//
// Add a listener to the view to auto fill username and passwords
// fields if the values are stored in the Keychain.
//
- (void) addListenerToView:(CHBrowserView*)view
{
  [view addListener:[[[KeychainBrowserListener alloc] initWithBrowser:view] autorelease]];
}

//
// hitButtonOK:
// hitButtonCancel:
// hitButtonOther:
//
// actions for the buttons of the keychain prompt dialogs.
//

enum { kOKButton = 0, kCancelButton = 1, kOtherButton = 2 };

- (IBAction)hitButtonOK:(id)sender
{
  [NSApp stopModalWithCode:kOKButton];
}

- (IBAction)hitButtonCancel:(id)sender
{
  [NSApp stopModalWithCode:kCancelButton];
}

- (IBAction)hitButtonOther:(id)sender
{
  [NSApp stopModalWithCode:kOtherButton];
}

//
// confirmStorePassword:
//
// Puts up a dialog when the keychain doesn't yet have an entry from
// this site asking to store it, forget it this once, or mark the site
// on a deny list so we never ask again.
//
- (KeychainPromptResult)confirmStorePassword:(NSWindow*)parent
{
  int result = [NSApp runModalForWindow:confirmStorePasswordPanel relativeToWindow:parent];
  [confirmStorePasswordPanel close];
  
  // the results of hitButtonXX: map to the corresponding values in the
  // |KeychainPromptResult| enum so we can just cast and return
  return NS_STATIC_CAST(KeychainPromptResult, result);
}

//
// confirmChangedPassword:
//
// The password stored in the keychain differs from what the user typed
// in. Ask what they want to do to resolve the issue.
//
- (BOOL)confirmChangedPassword:(NSWindow*)parent
{
  int result = [NSApp runModalForWindow:confirmChangePasswordPanel relativeToWindow:parent];
  [confirmChangePasswordPanel close];
  return (result == kOKButton);
}


- (void) addHostToDenyList:(NSString*)host
{
  [[KeychainDenyList instance] addHost:host];
}

- (BOOL) isHostInDenyList:(NSString*)host
{
  return [[KeychainDenyList instance] isHostPresent:host];
}

@end


@interface KeychainDenyList (KeychainDenyListPrivate)
- (NSString*) pathToDenyListFile;
@end


@implementation KeychainDenyList

static KeychainDenyList *sDenyListInstance = nil;

+ (KeychainDenyList*) instance
{
  return sDenyListInstance ? sDenyListInstance : sDenyListInstance = [[self alloc] init];
}

- (id) init
{
  if ( (self = [super init]) ) {
    mDenyList = [[NSUnarchiver unarchiveObjectWithFile:[self pathToDenyListFile]] retain];
    if ( !mDenyList )
      mDenyList = [[NSMutableArray alloc] init];
    
    mIsDirty = NO;
    
    // register for the cocoa notification posted when XPCOM shutdown so we
    // can release our singleton and flush the file
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(shutdown:) name:XPCOMShutDownNotificationName object:nil];
  }
  return self;
}

- (void) dealloc
{
  [self writeToDisk];
  [mDenyList release];
  [super dealloc];
}

//
// shutdown:
//
// Called in response to the cocoa notification "XPCOM Shutdown" sent by the cocoa
// browser service before it terminates embedding and shuts down xpcom. Allows us
// to get rid of anything we're holding onto for the length of the app.
//
- (void) shutdown:(id)unused
{
  [sDenyListInstance release];
}

//
// writeToDisk
//
// flushes the deny list to the save file in the user's profile, but only
// if it has changed since we read it in.
//
- (void) writeToDisk
{
  if ( mIsDirty )
    [NSArchiver archiveRootObject:mDenyList toFile:[self pathToDenyListFile]];
  mIsDirty = NO;
}

- (BOOL) isHostPresent:(NSString*)host
{
  return [mDenyList containsObject:host];
}

- (void) addHost:(NSString*)host
{
  if ( ![self isHostPresent:host] ) {
    [mDenyList addObject:host];
    mIsDirty = YES;
  }
}

- (void) removeHost:(NSString*)host
{
  if ( [self isHostPresent:host] ) {
    [mDenyList removeObject:host];
    mIsDirty = YES;
  }
}


//
// pathToDenyListFile
//
// returns a path ('/' delimited) that cocoa can use to point to the
// deny list save file in the current user's profile
//
- (NSString*) pathToDenyListFile
{
  NSMutableString* path = [[[NSMutableString alloc] init] autorelease];

  nsCOMPtr<nsIFile> appProfileDir;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(appProfileDir));
  if ( appProfileDir ) {
    nsAutoString profilePath;
    appProfileDir->GetPath(profilePath);
    [path setString:[NSString stringWith_nsAString:profilePath]];
    [path appendString:@"/Keychain Deny List"];    // |profilePath| is '/' delimited
  }
  
  return path;
}

@end


//
// Keychain prompt implementation.
// 
NS_IMPL_ISUPPORTS2(KeychainPrompt,
                   nsIAuthPrompt,
                   nsIAuthPromptWrapper)

KeychainPrompt::KeychainPrompt() 
{
}

KeychainPrompt::~KeychainPrompt()
{
  // NSLog(@"Keychain prompt died.");
}


//
// TODO: add support for ftp username/password. 
//
// Get server name and port from the realm ("hostname:port (realm)",
// see nsHttpChannel.cpp). we can't use CFURL routines or nsIURI
// routines because they require a protocol, and we don't have one.
//
// The given realm for an ftp server has the form ftp://<username>@<server>/<path>, see
// netwerk/protocol/ftp/src/nsFtpConnectionThread.cpp).
//
void
KeychainPrompt::ExtractHostAndPort(const PRUnichar* inRealm, NSString** outHost, PRInt32* outPort)
{
  if ( !outHost || !outPort )
    return;
  *outHost = @"";
  *outPort = kAnyPort;
  
  NSString* realmStr = [NSString stringWithPRUnichars:inRealm];

  // first check for an ftp url and pull out the server from the realm
  if ( [realmStr rangeOfString:@"ftp://"].location != NSNotFound ) {
    // cut out ftp://
    realmStr = [realmStr substringFromIndex:strlen("ftp://")];
    
    // cut out any of the path
    NSRange pathDelimeter = [realmStr rangeOfString:@"/"];
    if ( pathDelimeter.location != NSNotFound )
      realmStr = [realmStr substringToIndex:pathDelimeter.location-1];
    
    // now we're left with "username@server" with username being optional
    NSRange usernameMarker = [realmStr rangeOfString:@"@"];
    if ( usernameMarker.location != NSNotFound )
      *outHost = [realmStr substringFromIndex:usernameMarker.location+1];
    else
      *outHost = realmStr;
  }
  else {
    // we're an http url, strip off the "(realm)" part
    NSRange firstParen = [realmStr rangeOfString:@"("];
    if ( firstParen.location == NSNotFound )
      firstParen.location = [realmStr length];
    realmStr = [realmStr substringToIndex:firstParen.location-1];
    
    // separate the host and the port
    NSRange endOfHost = [realmStr rangeOfString:@":"];
    if ( endOfHost.location == NSNotFound )
      *outHost = realmStr;
    else {
      *outHost = [realmStr substringToIndex:endOfHost.location];
      *outPort = [[realmStr substringFromIndex:endOfHost.location+1] intValue];
    }
  }
}

void
KeychainPrompt::PreFill(const PRUnichar *realm, PRUnichar **user, PRUnichar **pwd)
{
  KeychainService* keychain = [KeychainService instance];
  if(![keychain isEnabled] || ![keychain isAutoFillEnabled])
    return;

  NSString* host = nil;
  PRInt32 port = -1;
  ExtractHostAndPort(realm, &host, &port);

  NSMutableString* username = nil;
  NSMutableString* password = nil;
  if ( user )
    username = [NSMutableString stringWithCharacters:*user length:(*user ? nsCRT::strlen(*user) : 0)];
  if ( pwd )
  	password = [NSMutableString stringWithCharacters:*pwd length:(*pwd ? nsCRT::strlen(*pwd) : 0)];

  //
  // Pre-fill user/password if found in the keychain.
  //
  KCItemRef ignore;
  if([keychain getUsernameAndPassword:(NSString*)host port:port user:username password:password item:&ignore]) {
    if ( user )
      *user = [username createNewUnicodeBuffer];
    if ( pwd )
      *pwd = [password createNewUnicodeBuffer];
  }
}

void
KeychainPrompt::ProcessPrompt(const PRUnichar* realm, bool checked, PRUnichar* user, PRUnichar *pwd)
{
  NSString* host = nil;
  PRInt32 port = -1;
  ExtractHostAndPort(realm, &host, &port);
  
  NSString* username = [NSString stringWithPRUnichars:user];
  NSString* password = [NSString stringWithPRUnichars:pwd];

  KeychainService* keychain = [KeychainService instance];

  NSMutableString* origUsername = [NSMutableString string];
  NSMutableString* origPwd = [NSMutableString string];
  KCItemRef itemRef;
  bool found = [keychain getUsernameAndPassword:(NSString*)host port:port user:origUsername password:origPwd item:&itemRef];
  
  //
  // Update, store or remove the user/password depending on the user
  // choice and whether or not we found the username/password in the
  // keychain.
  //
  if(checked && !found)
    [keychain storeUsernameAndPassword:(NSString*)host port:port user:username password:password];
  else if(checked && found && (![origUsername isEqualToString:username] || ![origPwd isEqualToString:password]))
    [keychain updateUsernameAndPassword:(NSString*)host port:port user:username password:password item:itemRef];
  else if(!checked && found)
    [keychain removeUsernameAndPassword:(NSString*)host port:port item:itemRef];
}

//
// Implementation of nsIAuthPrompt
//
NS_IMETHODIMP
KeychainPrompt::Prompt(const PRUnichar *dialogTitle, 
                        const PRUnichar *text, 
                        const PRUnichar *passwordRealm, 
                        PRUint32 savePassword,
                        const PRUnichar *defaultText, 
                        PRUnichar **result, 
                        PRBool *_retval)
{
  if (defaultText)
    *result = ToNewUnicode(nsDependentString(defaultText));

  mPrompt->Prompt(dialogTitle, text, result, nsnull, nsnull, _retval);

  return NS_OK;
}

NS_IMETHODIMP
KeychainPrompt::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                            const PRUnichar *text, 
                                            const PRUnichar *realm,
                                            PRUint32 savePassword, 
                                            PRUnichar **user, 
                                            PRUnichar **pwd, 
                                            PRBool *_retval)
{
  PreFill(realm, user, pwd);

  PRBool checked = [[KeychainService instance] isEnabled];
  PRUnichar* checkTitle = [NSLocalizedString(@"KeychainCheckTitle", @"") createNewUnicodeBuffer];

  nsresult rv = mPrompt->PromptUsernameAndPassword(dialogTitle, text, user, pwd, checkTitle, &checked, _retval);
  if ( checkTitle )
    nsMemory::Free(checkTitle);
  if (NS_FAILED(rv))
    return rv;
  
  if(*_retval)
    ProcessPrompt(realm, checked, *user, *pwd);

  return NS_OK;
}

NS_IMETHODIMP
KeychainPrompt::PromptPassword(const PRUnichar *dialogTitle,
                                const PRUnichar *text, 
                                const PRUnichar *realm,
                                PRUint32 savePassword, 
                                PRUnichar **pwd, 
                                PRBool *_retval)
{
  PreFill(realm, nsnull, pwd);

  PRBool checked = [[KeychainService instance] isEnabled];
  PRUnichar* checkTitle = [NSLocalizedString(@"KeychainCheckTitle", @"") createNewUnicodeBuffer];

  nsresult rv = mPrompt->PromptPassword(dialogTitle, text, pwd, checkTitle, &checked, _retval);
  if ( checkTitle )
    nsMemory::Free(checkTitle);
  if (NS_FAILED(rv))
    return rv;
  
  if(*_retval)
    ProcessPrompt(realm, checked, nsnull, *pwd);

  return NS_OK;
}

NS_IMETHODIMP
KeychainPrompt::SetPromptDialogs(nsIPrompt* dialogs)
{
  mPrompt = dialogs;
  return NS_OK;
}

//
// Keychain form submit observer implementation.
//
NS_IMPL_ISUPPORTS2(KeychainFormSubmitObserver,
                   nsIObserver,
                   nsIFormSubmitObserver)

KeychainFormSubmitObserver::KeychainFormSubmitObserver()
{
  //NSLog(@"Keychain form submit observer created.");
}

KeychainFormSubmitObserver::~KeychainFormSubmitObserver()
{
  //NSLog(@"Keychain form submit observer died.");
}

NS_IMETHODIMP
KeychainFormSubmitObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData) 
{
  return NS_OK;
}

NS_IMETHODIMP 
KeychainFormSubmitObserver::Notify(nsIContent* node, nsIDOMWindowInternal* window, nsIURI* actionURL, 
                                    PRBool* cancelSubmit)
{
	KeychainService* keychain = [KeychainService instance];
  if (![keychain isEnabled])
    return NS_OK;

  nsCOMPtr<nsIDOMHTMLFormElement> formNode(do_QueryInterface(node));
  if (!formNode)
    return NS_OK;

  // seek out the username and password fields. If there are two password fields, we don't
  // want to do anything since it's probably not a signin form. Passing PR_FALSE in the last
  // param will cause FUPF to look for multiple password fields and then bail if that is the 
  // case, seting |passwordElement| to nsnull.
  nsCOMPtr<nsIDOMHTMLInputElement> usernameElement, passwordElement;
  nsresult rv = FindUsernamePasswordFields(formNode, getter_AddRefs(usernameElement), getter_AddRefs(passwordElement),
                                           PR_FALSE);

  if (NS_SUCCEEDED(rv) && usernameElement && passwordElement) {
  
    // extract username and password from the fields
    nsAutoString uname, pword;
    usernameElement->GetValue(uname);
    passwordElement->GetValue(pword);
    NSString* username = [NSString stringWith_nsAString:uname];
    NSString* password = [NSString stringWith_nsAString:pword];
    if ( ![username length] || ![password length] )      // bail if either is empty
      return NS_OK;

    nsCOMPtr<nsIDocument> doc = node->GetDocument();
    if (!doc)
      return NS_OK;
    
    nsIURI* docURL = doc->GetDocumentURI();
    if (!docURL)
      return NS_OK;

    nsCAutoString host;
    PRInt32 port = -1;
    docURL->GetHost(host);
    docURL->GetPort(&port);

    // is the host in the deny list? if yes, bail. otherwise check the keychain.
    NSString* realm = [NSString stringWithCString:host.get()];
    if ( [keychain isHostInDenyList:realm] )
      return NS_OK;
    
    //
    // If there's already an entry in the keychain, check if the username
    // and password match. If not, ask the user what they want to do and replace
    // it as necessary. If there's no entry, ask if they want to remember it
    // and then put it into the keychain
    //
    NSString* existingUser = [NSMutableString string];
    NSString* existingPassword = [NSMutableString string];
    KCItemRef itemRef;
    BOOL foundExistingPassword = [keychain getUsernameAndPassword:realm port:port user:existingUser password:existingPassword item:&itemRef];
    if ( foundExistingPassword ) {
      if ( !([existingUser isEqualToString:username] && [existingPassword isEqualToString:password]) )
        if ( CheckChangeDataYN(window) )
          [keychain updateUsernameAndPassword:realm port:port user:username password:password item:itemRef];
    }
    else {
      switch (CheckStorePasswordYN(window)) {
        case kSave:
          [keychain storeUsernameAndPassword:realm port:port user:username password:password];
          break;
        
        case kNeverRemember:
          // tell the keychain we never want to be prompted about this host again
          [keychain addHostToDenyList:realm];
          break;
        
        case kDontRemember:
          // do nothing at all
          break;
      }
    }
  }

  return NS_OK;
}


NSWindow*
KeychainFormSubmitObserver::GetNSWindow(nsIDOMWindowInternal* inWindow)
{
  //
  // TODO: Refactor: Getting the NSWindow for the nsIDOMWindowInternal
  // is already implemented in CocoaPromptService.
  //
  nsCOMPtr<nsIWindowWatcher> watcher(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  if (!watcher)
    return nsnull;

  nsCOMPtr<nsIWebBrowserChrome> chrome;
  watcher->GetChromeForWindow(inWindow, getter_AddRefs(chrome));
  if (!chrome)
    return nsnull;

  nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_QueryInterface(chrome));
  if (!siteWindow)
    return nsnull;

  NSWindow* nswindow;
  nsresult rv = siteWindow->GetSiteWindow((void**)&nswindow);
  if (NS_FAILED(rv))
    return nsnull;

  return nswindow;
}

KeychainPromptResult
KeychainFormSubmitObserver::CheckStorePasswordYN(nsIDOMWindowInternal* window)
{
  NSWindow* nswindow = GetNSWindow(window);
  return [[KeychainService instance] confirmStorePassword:nswindow];
}


BOOL
KeychainFormSubmitObserver::CheckChangeDataYN(nsIDOMWindowInternal* window)
{
  NSWindow* nswindow = GetNSWindow(window);
  return [[KeychainService instance] confirmChangedPassword:nswindow];
}

@implementation KeychainBrowserListener

- (id)initWithBrowser:(CHBrowserView*)aBrowser
{
  if ( (self = [super init]) ) {
    mBrowserView = aBrowser;
  }
  return self;
}

- (void) dealloc
{
  // NSLog(@"Keychain browser listener died.");
  [super dealloc];
}

- (void)onLoadingCompleted:(BOOL)succeeded;
{
  if(!succeeded)
    return;

  KeychainService* keychain = [KeychainService instance];
  if(![keychain isEnabled] || ![keychain isAutoFillEnabled])
    return;

  nsCOMPtr<nsIDOMWindow> domWin = getter_AddRefs([mBrowserView getContentWindow]);
  if (!domWin)
    return;
  nsCOMPtr<nsIDOMDocument> domDoc;
  domWin->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc ( do_QueryInterface(domDoc) );
  if (!doc) {
    NS_ASSERTION(0, "no document available");
    return;
  }
  
  nsCOMPtr<nsIDOMHTMLDocument> htmldoc(do_QueryInterface(doc));
  if (!htmldoc)
    return;

  nsCOMPtr<nsIDOMHTMLCollection> forms;
  nsresult rv = htmldoc->GetForms(getter_AddRefs(forms));
  if (NS_FAILED(rv) || !forms) 
    return;

  PRUint32 numForms;
  forms->GetLength(&numForms);

  //
  // Seek out username and password element in all forms. If found in
  // a form, check the keychain to see if the username passowrd are
  // stored and prefill the elements.
  //
  for (PRUint32 formX = 0; formX < numForms; formX++) {

    nsCOMPtr<nsIDOMNode> formNode;
    rv = forms->Item(formX, getter_AddRefs(formNode));
    if (NS_FAILED(rv) || formNode == nsnull)
      continue;

    // search the current form for the text fields
    nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(formNode));
    if (!formElement) 
      continue;
    nsCOMPtr<nsIDOMHTMLInputElement> usernameElement, passwordElement;
    rv = FindUsernamePasswordFields(formElement, getter_AddRefs(usernameElement), getter_AddRefs(passwordElement), 
                                    PR_TRUE);

    if (NS_SUCCEEDED(rv) && usernameElement && passwordElement) {    
      //
      // We found the text field and password field. Check if there's
      // a username/password stored in the keychain for this host and
      // pre-fill the fields if that's the case.
      //
      nsIURI* docURL = doc->GetDocumentURI();
      if (!docURL)
        return;
  
      NSMutableString* username = [NSMutableString string];
      NSMutableString* password = [NSMutableString string];
      
      nsCAutoString host;
      docURL->GetHost(host);
      NSString *hostStr = [NSString stringWithCString:host.get()];
      PRInt32 port = -1;
      docURL->GetPort(&port);
      
      KCItemRef ignore;
      if ([keychain getUsernameAndPassword:hostStr port:port user:username password:password item:&ignore]) {
        nsAutoString user, pwd;
        [username assignTo_nsAString:user];
        [password assignTo_nsAString:pwd];
        
        // if the server specifies a value attribute (bug 169760), only autofill
        // the password if what we have in keychain matches what the server supplies,
        // otherwise don't. Don't bother checking the password field for a value; i can't
        // imagine the server ever prefilling a password
        nsAutoString userValue;
        usernameElement->GetAttribute(NS_LITERAL_STRING("value"), userValue);
        if (!userValue.Length() || userValue.Equals(user)) {
          rv = usernameElement->SetValue(user);
          rv = passwordElement->SetValue(pwd);
        }
      }
        
      // We found the sign-in form so return now. This means we don't
      // support pages where there's multiple sign-in forms.
      return;
    }

  } // for each form on page
}

- (void)onLoadingStarted
{
}

- (void)onProgressChange:(int)currentBytes outOf:(int)maxBytes
{
}

- (void)onLocationChange:(NSString*)urlSpec
{
}

- (void)onStatusChange:(NSString*)aMessage
{
}

- (void)onSecurityStateChange:(unsigned long)newState
{
}

- (void)onShowContextMenu:(int)flags domEvent:(nsIDOMEvent*)aEvent domNode:(nsIDOMNode*)aNode
{
}

- (void)onShowTooltip:(NSPoint)where withText:(NSString*)text
{
}

- (void)onHideTooltip
{
}

- (void)onPopupBlocked:(nsIURI*)inURIBlocked fromSite:(nsIURI*)inSite
{
}

@end

//
// FindUsernamePasswordFields
//
// Searches the form for the first username and password fields. If
// none are found, the out params will be nsnull. |inStopWhenFound|
// determines how we proceed once we find things. When true, we bail
// as soon as we find both a username and password field. If false, we
// continue searching the form for a 2nd password field (such as in a
// "change your password" form). If we find one, null out
// |outPassword| since we probably don't want to prefill this form.
//
nsresult
FindUsernamePasswordFields(nsIDOMHTMLFormElement* inFormElement, nsIDOMHTMLInputElement** outUsername,
                            nsIDOMHTMLInputElement** outPassword, PRBool inStopWhenFound)
{
  if ( !outUsername || !outPassword )
    return NS_ERROR_FAILURE;
  *outUsername = *outPassword = nsnull;

  // pages can specify that they don't want autofill by setting a
  // "autocomplete=off" attribute on the form. 
  nsAutoString autocomplete;
  inFormElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
  if ( autocomplete.EqualsIgnoreCase("off") )
    return NS_OK;
  
  //
  // Search the form the password field and the preceding text field
  // We are only interested in signon forms, so we require
  // only one password in the form. If there's more than one password
  // it's probably a form to setup the password or to change the
  // password. We don't want to handle this kind of form at this
  // point.
  //
  nsCOMPtr<nsIDOMHTMLCollection> elements;
  nsresult rv = inFormElement->GetElements(getter_AddRefs(elements));
  if (NS_FAILED(rv) || !elements) 
  return NS_OK;

  PRUint32 numElements;
  elements->GetLength(&numElements);

  for (PRUint32 elementX = 0; elementX < numElements; elementX++) {

    nsCOMPtr<nsIDOMNode> elementNode;
    rv = elements->Item(elementX, getter_AddRefs(elementNode));
    if (NS_FAILED(rv) || !elementNode)
      continue;

    nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode));
    if (!inputElement)
      continue;

    nsAutoString type;
    rv = inputElement->GetType(type);
    if (NS_FAILED(rv))
      continue;

    bool isText = (type.IsEmpty() || type.Equals(NS_LITERAL_STRING("text"), nsCaseInsensitiveStringComparator()));
    bool isPassword = type.Equals(NS_LITERAL_STRING("password"), nsCaseInsensitiveStringComparator());
    inputElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);

    if((!isText && !isPassword) || autocomplete.EqualsIgnoreCase("off"))
      continue;

    //
    // If there's a second password in the form, it's probably not a
    // signin form, probably a form to setup or change
    // passwords. Stop here and ensure we don't store this password.
    //
    if (!inStopWhenFound && isPassword && *outPassword) {
      NS_RELEASE(*outPassword);
      *outPassword = nsnull;
      return NS_OK;
    }
    
    // Keep taking text fields until we have a password, so that
    // username is the field before password.
    if(isText && !*outPassword) {
      if (*outUsername) NS_RELEASE(*outUsername);
      *outUsername = inputElement;
      NS_ADDREF(*outUsername);
    }
    else if (isPassword && !*outPassword) {
      *outPassword = inputElement;
      NS_ADDREF(*outPassword);
    }
    
    // we've got everything we need, we're done.
    if (inStopWhenFound && *outPassword && *outUsername)
      return NS_OK;
    
  } // for each item in form

  return NS_OK;
}
