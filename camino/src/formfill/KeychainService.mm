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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Foucher <bfoucher@mac.com>
 *   Stuart Morgan <stuart.morgan@gmail.com>
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

// Security must be imported first, because of uint32 declaration conflicts
#import <Security/Security.h>

#import "NSString+Utils.h"

#import "KeychainItem.h"
#import "KeychainService.h"
#import "CHBrowserService.h"
#import "PreferenceManager.h"

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

#ifdef __LITTLE_ENDIAN__
const PRUint32 kSecAuthenticationTypeHTTPDigestReversed = 'httd';
#else
const PRUint32 kSecAuthenticationTypeHTTPDigestReversed = 'dtth';
#endif

const OSType kCaminoKeychainCreatorCode = 'CMOZ';

const int kDefaultHTTPSPort = 443;

// from CHBrowserService.h
extern NSString* const XPCOMShutDownNotificationName;


nsresult
FindUsernamePasswordFields(nsIDOMHTMLFormElement* inFormElement, nsIDOMHTMLInputElement** outUsername,
                           nsIDOMHTMLInputElement** outPassword, PRBool inStopWhenFound);

@interface KeychainService(Private)
- (KeychainItem*)findLegacyKeychainEntryForHost:(NSString*)host port:(PRInt32)port;
@end

@implementation KeychainService

static KeychainService *sInstance = nil;
static const char* const gUseKeychainPref = "chimera.store_passwords_with_keychain";

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
  if (strcmp(inPref, gUseKeychainPref) == 0)
    [KeychainService instance]->mFormPasswordFillIsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:gUseKeychainPref withSuccess:&success];
  
  return NS_OK;
}


+ (KeychainService*)instance
{
  return sInstance ? sInstance : sInstance = [[self alloc] init];
}

- (id)init
{
  if ((self = [super init])) {
    // Add a new form submit observer. We explicitly hold a ref in case the
    // observer service uses a weakref.
    nsCOMPtr<nsIObserverService> svc = do_GetService("@mozilla.org/observer-service;1");
    NS_ASSERTION(svc, "Keychain can't get observer service");
    mFormSubmitObserver = new KeychainFormSubmitObserver();
    if (mFormSubmitObserver && svc) {
      NS_ADDREF(mFormSubmitObserver);
      svc->AddObserver(mFormSubmitObserver, NS_FORMSUBMIT_SUBJECT, PR_FALSE);
    }
  
    // register for the cocoa notification posted when XPCOM shutdown so we
    // can unregister the pref callbacks we register below
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(shutdown:) name:XPCOMShutDownNotificationName
      object:nil];
    
    // cache the values of the prefs and register pref-changed callbacks. Yeah, I know
    // nsIPref is obsolete, but i'm not about to create an nsIObserver just for this.
    mFormPasswordFillIsEnabled = NO;
    nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
    if (pref) {
      BOOL success = NO;
      mFormPasswordFillIsEnabled = [[PreferenceManager sharedInstance] getBooleanPref:gUseKeychainPref withSuccess:&success];
      pref->RegisterCallback(gUseKeychainPref, KeychainPrefChangedCallback, nsnull);
    }
    
    // load the keychain.nib file with our dialogs in it
    BOOL success = [NSBundle loadNibNamed:@"Keychain" owner:self];
    NS_ASSERTION(success, "can't load keychain prompt dialogs");
  }
  return self;
}

- (void)dealloc
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
- (void)shutdown:(id)unused
{
  // unregister ourselves as listeners of prefs before prefs go away
  nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID));
  if (pref)
    pref->UnregisterCallback(gUseKeychainPref, KeychainPrefChangedCallback, nsnull);
  
  [sInstance release];
}


- (BOOL)formPasswordFillIsEnabled
{
  return mFormPasswordFillIsEnabled;
}

- (KeychainItem*)findKeychainEntryForHost:(NSString*)host
                                     port:(PRInt32)port
                                   scheme:(NSString*)scheme
                                   isForm:(BOOL)isForm;
{
  if (port == -1)
    port = kAnyPort;
  SecProtocolType protocol = [scheme isEqualToString:@"https"] ? kSecProtocolTypeHTTPS : kSecProtocolTypeHTTP;
  SecAuthenticationType authType = isForm ? kSecAuthenticationTypeHTMLForm : kSecAuthenticationTypeDefault;

  NSArray* newKeychainItems = [KeychainItem allKeychainItemsForHost:host
                                                               port:(UInt16)port
                                                           protocol:protocol
                                                 authenticationType:authType
                                                            creator:0];

  // First, check for a new-style Camino keychain entry
  KeychainItem* item;
  NSEnumerator* keychainEnumerator = [newKeychainItems objectEnumerator];
  while ((item = [keychainEnumerator nextObject])) {
    if ([item creator] == kCaminoKeychainCreatorCode)
      return item;
  }

  // If there isn't one, check for an old-style Camino entry
  item = [self findLegacyKeychainEntryForHost:host port:port];
  if (item)
    return item;

  // Finally, check for a new style entry created by something other than Camino.
  // Since we don't yet have any UI for multiple accounts, we use Safari's default
  // if we can find it, otherwise we just arbitrarily pick the first one.
  keychainEnumerator = [newKeychainItems objectEnumerator];
  while ((item = [keychainEnumerator nextObject])) {
    NSString* comment = [item comment];
    if (comment && ([comment rangeOfString:@"default"].location != NSNotFound))
      return item;
  }
  if ([newKeychainItems count] > 0) {
    return [newKeychainItems objectAtIndex:0];
  }

  return nil;
}

// Prior to 1.1, keychain items were stored with a different authentication type
// and may have been created with the authentication type backward due to an
// endian bug in KeychainManager. This can be removed once it's reasonable to
// assume people no longer have old-style keychain items.
- (KeychainItem*)findLegacyKeychainEntryForHost:(NSString*)host port:(PRInt32)port
{
  KeychainItem* item = [KeychainItem keychainItemForHost:host
                                                    port:(UInt16)port
                                                protocol:kSecProtocolTypeHTTP
                                      authenticationType:kSecAuthenticationTypeHTTPDigest];
  if (!item) {
    // check for the reverse auth type
    item = [KeychainItem keychainItemForHost:host
                                        port:(UInt16)port
                                    protocol:kSecProtocolTypeHTTP
                          authenticationType:kSecAuthenticationTypeHTTPDigestReversed];
    // fix it for future queries
    [item setAuthenticationType:kSecAuthenticationTypeHTTPDigest];
  }
  return item;
}

- (void)storeUsername:(NSString*)username
             password:(NSString*)password
              forHost:(NSString*)host
                 port:(PRInt32)port
               scheme:(NSString*)scheme
               isForm:(BOOL)isForm
{
  if (port == -1)
    port = kAnyPort;
  SecProtocolType protocol = [scheme isEqualToString:@"https"] ? kSecProtocolTypeHTTPS : kSecProtocolTypeHTTP;
  SecAuthenticationType authType = isForm ? kSecAuthenticationTypeHTMLForm : kSecAuthenticationTypeDefault;

  KeychainItem* newItem = [KeychainItem addKeychainItemForHost:host
                                                          port:(UInt16)port
                                                      protocol:protocol
                                            authenticationType:authType
                                                  withUsername:username
                                                      password:password];
  [newItem setCreator:kCaminoKeychainCreatorCode];
}

// Stores changes to a site's stored account. Because we don't handle multiple accounts, we want to
// allow users to store an acccount in Camino that isn't the one we pick up from Safari, so for
// password updates we update the existing item, but for username updates we make a new item if
// the one we were using wasn't Camino-created.
- (void)updateKeychainEntry:(KeychainItem*)keychainItem
               withUsername:(NSString*)username
                   password:(NSString*)password
                     scheme:(NSString*)scheme
                     isForm:(BOOL)isForm
{
  if ([username isEqualToString:[keychainItem username]] || [keychainItem creator] == kCaminoKeychainCreatorCode) {
    [keychainItem setUsername:username password:password];
    // If this is one of our old items, upgrade it to the new format. We do this here instead of when
    // we first find the item so that users trying out 1.1 previews won't be missing the passwords of
    // all the sites they visit if/when they go back to 1.0.x, as well as to prevent passwords from accidentally
    // being associated with the wrong type (e.g., if a user has a password stored for a site's web form,
    // but hits the http auth dialog first).
    if ([keychainItem authenticationType] == kSecAuthenticationTypeHTTPDigest) {
      [keychainItem setProtocol:([scheme isEqualToString:@"https"] ? kSecProtocolTypeHTTPS : kSecProtocolTypeHTTP)];
      [keychainItem setAuthenticationType:(isForm ? kSecAuthenticationTypeHTMLForm : kSecAuthenticationTypeDefault)];
      [keychainItem setCreator:kCaminoKeychainCreatorCode];
    }
  }
  else {
    KeychainItem* newItem = [KeychainItem addKeychainItemForHost:[keychainItem host]
                                                            port:[keychainItem port]
                                                        protocol:[keychainItem protocol]
                                              authenticationType:[keychainItem authenticationType]
                                                    withUsername:username
                                                        password:password];
    [newItem setCreator:kCaminoKeychainCreatorCode];
  }
}

// Removes all Camino-created keychain entries. This will only remove new-style entries,
// because it's not safe to assume that all passwords that match the old format were
// necessarily created by us.
- (void)removeAllUsernamesAndPasswords
{
  NSArray* keychainItems = [KeychainItem allKeychainItemsForHost:nil
                                                            port:kAnyPort
                                                        protocol:0
                                              authenticationType:0
                                                         creator:kCaminoKeychainCreatorCode];
  KeychainItem* item;
  NSEnumerator* keychainEnumerator = [keychainItems objectEnumerator];
  while ((item = [keychainEnumerator nextObject])) {
    [item removeFromKeychain];
  }

  // Reset the deny list as well
  [[KeychainDenyList instance] removeAllHosts];
}

//
// addListenerToView:
//
// Add a listener to the view to auto fill username and passwords
// fields if the values are stored in the Keychain.
//
- (void)addListenerToView:(CHBrowserView*)view
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

- (IBAction)hitButtonOK:(id)sender
{
  [NSApp stopModalWithCode:NSAlertDefaultReturn];
}

- (IBAction)hitButtonCancel:(id)sender
{
  [NSApp stopModalWithCode:NSAlertAlternateReturn];
}

- (IBAction)hitButtonOther:(id)sender
{
  [NSApp stopModalWithCode:NSAlertOtherReturn];
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
  
  KeychainPromptResult keychainAction = kDontRemember;
  switch (result) {
    case NSAlertDefaultReturn:    keychainAction = kSave;          break;
    default:
    case NSAlertAlternateReturn:  keychainAction = kDontRemember;  break;
    case NSAlertOtherReturn:      keychainAction = kNeverRemember; break;
  }

  return keychainAction;
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
  return (result == NSAlertDefaultReturn);
}


- (void)addHostToDenyList:(NSString*)host
{
  [[KeychainDenyList instance] addHost:host];
}

- (BOOL)isHostInDenyList:(NSString*)host
{
  return [[KeychainDenyList instance] isHostPresent:host];
}

@end


@interface KeychainDenyList (KeychainDenyListPrivate)
- (void)writeToDisk;
- (NSString*)pathToDenyListFile;
@end


@implementation KeychainDenyList

static KeychainDenyList *sDenyListInstance = nil;

+ (KeychainDenyList*)instance
{
  return sDenyListInstance ? sDenyListInstance : sDenyListInstance = [[self alloc] init];
}

- (id)init
{
  if ((self = [super init])) {
    mDenyList = [[NSUnarchiver unarchiveObjectWithFile:[self pathToDenyListFile]] retain];
    if (!mDenyList)
      mDenyList = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)dealloc
{
  [mDenyList release];
  [super dealloc];
}

//
// writeToDisk
//
// flushes the deny list to the save file in the user's profile.
//
- (void)writeToDisk
{
  [NSArchiver archiveRootObject:mDenyList toFile:[self pathToDenyListFile]];
}

- (BOOL)isHostPresent:(NSString*)host
{
  return [mDenyList containsObject:host];
}

- (void)addHost:(NSString*)host
{
  if (![self isHostPresent:host]) {
    [mDenyList addObject:host];
    [self writeToDisk];
  }
}

- (void)removeHost:(NSString*)host
{
  if ([self isHostPresent:host]) {
    [mDenyList removeObject:host];
    [self writeToDisk];
  }
}

- (void)removeAllHosts
{
  [mDenyList removeAllObjects];
  [self writeToDisk];
}


//
// pathToDenyListFile
//
// returns a path ('/' delimited) that cocoa can use to point to the
// deny list save file in the current user's profile
//
- (NSString*)pathToDenyListFile
{
  NSString* path = nil;

  nsCOMPtr<nsIFile> appProfileDir;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(appProfileDir));
  if (appProfileDir) {
    nsAutoString profilePath;
    appProfileDir->GetPath(profilePath);
    path = [[NSString stringWith_nsAString:profilePath] stringByAppendingPathComponent:@"Keychain Deny List"];
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
  if (!outHost || !outPort)
    return;
  *outHost = @"";
  *outPort = -1;
  
  NSString* realmStr = [NSString stringWithPRUnichars:inRealm];

  // first check for an ftp url and pull out the server from the realm
  if ([realmStr rangeOfString:@"ftp://"].location != NSNotFound) {
    // cut out ftp://
    realmStr = [realmStr substringFromIndex:strlen("ftp://")];
    
    // cut out any of the path
    NSRange pathDelimeter = [realmStr rangeOfString:@"/"];
    if (pathDelimeter.location != NSNotFound)
      realmStr = [realmStr substringToIndex:pathDelimeter.location-1];
    
    // now we're left with "username@server" with username being optional
    NSRange usernameMarker = [realmStr rangeOfString:@"@"];
    if (usernameMarker.location != NSNotFound)
      *outHost = [realmStr substringFromIndex:usernameMarker.location+1];
    else
      *outHost = realmStr;
  }
  else {
    // we're an http url, strip off the "(realm)" part
    NSRange firstParen = [realmStr rangeOfString:@"("];
    if (firstParen.location == NSNotFound)
      firstParen.location = [realmStr length];
    realmStr = [realmStr substringToIndex:firstParen.location-1];
    
    // separate the host and the port
    NSRange endOfHost = [realmStr rangeOfString:@":"];
    if (endOfHost.location == NSNotFound)
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
  NSString* host = nil;
  PRInt32 port = -1;
  ExtractHostAndPort(realm, &host, &port);

  // Pre-fill user/password if found in the keychain.
  // We don't get scheme information from gecko, so guess based on the port
  NSString* scheme = (port == kDefaultHTTPSPort) ? @"https" : @"http";

  KeychainService* keychain = [KeychainService instance];
  KeychainItem* entry = [keychain findKeychainEntryForHost:host port:port scheme:scheme isForm:NO];
  if (entry) {
    if (user)
      *user = [[entry username] createNewUnicodeBuffer];
    if (pwd)
      *pwd = [[entry password] createNewUnicodeBuffer];
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

  // We don't get scheme information from gecko, so guess based on the port
  NSString* scheme = (port == kDefaultHTTPSPort) ? @"https" : @"http";

  KeychainService* keychain = [KeychainService instance];
  KeychainItem* existingEntry = [keychain findKeychainEntryForHost:(NSString*)host port:port scheme:scheme isForm:NO];

  // Update, store or remove the user/password depending on the user
  // choice and whether or not we found the username/password in the
  // keychain.
  if (checked && !existingEntry)
    [keychain storeUsername:username password:password forHost:(NSString*)host port:port scheme:scheme isForm:NO];
  else if (checked && existingEntry && (![[existingEntry username] isEqualToString:username] ||
                                        ![[existingEntry password] isEqualToString:password]))
    [keychain updateKeychainEntry:existingEntry withUsername:username password:password scheme:scheme isForm:NO];
  else if (!checked && existingEntry)
    [existingEntry removeFromKeychain];
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

  PRBool checked = (pwd != NULL && *pwd != NULL);
  PRUnichar* checkTitle = [NSLocalizedString(@"KeychainCheckTitle", @"") createNewUnicodeBuffer];

  nsresult rv = mPrompt->PromptUsernameAndPassword(dialogTitle, text, user, pwd, checkTitle, &checked, _retval);
  if (checkTitle)
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

  PRBool checked = (pwd != NULL && *pwd != NULL);
  PRUnichar* checkTitle = [NSLocalizedString(@"KeychainCheckTitle", @"") createNewUnicodeBuffer];

  nsresult rv = mPrompt->PromptPassword(dialogTitle, text, pwd, checkTitle, &checked, _retval);
  if (checkTitle)
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
  if (![keychain formPasswordFillIsEnabled])
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
    if (![username length] || ![password length])      // bail if either is empty
      return NS_OK;

    nsCOMPtr<nsIDocument> doc = node->GetDocument();
    if (!doc)
      return NS_OK;

    nsIURI* docURL = doc->GetDocumentURI();
    if (!docURL)
      return NS_OK;

    nsCAutoString hostCAString;
    docURL->GetHost(hostCAString);
    NSString* host = [NSString stringWithCString:hostCAString.get()];

    // is the host in the deny list? if yes, bail. otherwise check the keychain.
    if ([keychain isHostInDenyList:host])
      return NS_OK;

    nsCAutoString schemeCAString;
    docURL->GetScheme(schemeCAString);
    NSString* scheme = [NSString stringWithCString:schemeCAString.get()];
    PRInt32 port = -1;
    docURL->GetPort(&port);

    //
    // If there's already an entry in the keychain, check if the username
    // and password match. If not, ask the user what they want to do and replace
    // it as necessary. If there's no entry, ask if they want to remember it
    // and then put it into the keychain
    //
    KeychainItem* existingEntry = [keychain findKeychainEntryForHost:host port:port scheme:scheme isForm:YES];
    if (existingEntry) {
      if (!([[existingEntry username] isEqualToString:username] && [[existingEntry password] isEqualToString:password]))
        if (CheckChangeDataYN(window))
          [keychain updateKeychainEntry:existingEntry withUsername:username password:password scheme:scheme isForm:YES];
    }
    else {
      switch (CheckStorePasswordYN(window)) {
        case kSave:
          [keychain storeUsername:username password:password forHost:host port:port scheme:scheme isForm:YES];
          break;

        case kNeverRemember:
          // tell the keychain we never want to be prompted about this host again
          [keychain addHostToDenyList:host];
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
  CHBrowserView* browserView = [CHBrowserView browserViewFromDOMWindow:inWindow];
  return [browserView nativeWindow];
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
  if ((self = [super init])) {
    mBrowserView = aBrowser;
  }
  return self;
}

- (void)dealloc
{
  // NSLog(@"Keychain browser listener died.");
  [super dealloc];
}

//
// -fillDOMWindow:
//
// Given a dom window (the root of a page, frame, or iframe), fill in the username
// and password if there are appropriate fields in any frame within this "window". Once
// it's done, it recurses over all of the subframes and fills them in as well.
//
- (void)fillDOMWindow:(nsIDOMWindow*)inDOMWindow
{
  KeychainService* keychain = [KeychainService instance];
  if(![keychain formPasswordFillIsEnabled])
    return;

  nsCOMPtr<nsIDOMDocument> domDoc;
  inDOMWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc (do_QueryInterface(domDoc));
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
  // a form, check the keychain to see if the username password are
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

      nsCAutoString hostCAString;
      docURL->GetHost(hostCAString);
      NSString *host = [NSString stringWithCString:hostCAString.get()];
      nsCAutoString schemeCAString;
      docURL->GetScheme(schemeCAString);
      NSString *scheme = [NSString stringWithCString:schemeCAString.get()];
      PRInt32 port = -1;
      docURL->GetPort(&port);

      KeychainItem* keychainEntry = [keychain findKeychainEntryForHost:host port:port scheme:scheme isForm:YES];
      if (keychainEntry) {
        nsAutoString user, pwd;
        [[keychainEntry username] assignTo_nsAString:user];
        [[keychainEntry password] assignTo_nsAString:pwd];

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
    }
  } // for each form on page

  // recursively check sub-frames and iframes
  nsCOMPtr<nsIDOMWindowCollection> frames;
  inDOMWindow->GetFrames(getter_AddRefs(frames));
  if (frames) {
    PRUint32 numFrames;
    frames->GetLength(&numFrames);
    for (PRUint32 i = 0; i < numFrames; i++) {
      nsCOMPtr<nsIDOMWindow> frameNode;
      frames->Item(i, getter_AddRefs(frameNode));
      if (frameNode)
        [self fillDOMWindow:frameNode];
    }
  }
}

- (void)onLoadingCompleted:(BOOL)succeeded;
{
  if(!succeeded)
    return;
  
  // prime the pump with the top dom window.
  nsCOMPtr<nsIDOMWindow> domWin = [mBrowserView getContentWindow];
  if (!domWin)
    return;
  
  // recursively fill frames and iFrames.
  [self fillDOMWindow:domWin];
}

- (void)onLoadingStarted
{
}

- (void)onProgressChange:(long)currentBytes outOf:(long)maxBytes
{
}

- (void)onProgressChange64:(long long)currentBytes outOf:(long long)maxBytes
{
}

- (void)onLocationChange:(NSString*)urlSpec isNewPage:(BOOL)newPage requestSucceeded:(BOOL)requestOK
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

- (void)onPopupBlocked:(nsIDOMPopupBlockedEvent*)data;
{
}

- (void)onFoundShortcutIcon:(NSString*)inIconURI
{
}

- (void)onFeedDetected:(NSString*)inFeedURI feedTitle:(NSString*)inFeedTitle;
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
  if (!outUsername || !outPassword)
    return NS_ERROR_FAILURE;
  *outUsername = *outPassword = nsnull;

  PRBool autoCompleteOverride = [[PreferenceManager sharedInstance] getBooleanPref:"wallet.crypto.autocompleteoverride" withSuccess:NULL];

  // pages can specify that they don't want autofill by setting a
  // "autocomplete=off" attribute on the form.
  nsAutoString autocomplete;
  inFormElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
  if (autocomplete.EqualsIgnoreCase("off") && !autoCompleteOverride)
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

    if ((!isText && !isPassword) || (autocomplete.EqualsIgnoreCase("off") && !autoCompleteOverride))
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
