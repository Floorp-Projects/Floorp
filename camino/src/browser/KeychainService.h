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

#ifndef __KeychainService_h__
#define __KeychainService_h__

#include <Cocoa/Cocoa.h>

#include "CHBrowserView.h"

#include "nsEmbedAPI.h"
#include "nsIFactory.h"
#include "nsIPrompt.h"
#include "nsIAuthPromptWrapper.h"
#include "nsIObserver.h"
#include "nsIFormSubmitObserver.h"

class nsIPrefBranch;

enum KeychainPromptResult { kSave, kDontRemember, kNeverRemember } ;

@class CHBrowserView;


@interface KeychainService : NSObject
{
  IBOutlet id confirmStorePasswordPanel;
  IBOutlet id confirmChangePasswordPanel;

  BOOL mIsEnabled;
  BOOL mIsAutoFillEnabled;

  nsIObserver* mFormSubmitObserver;
}

+ (KeychainService*) instance;
- (void) shutdown:(id)sender;

- (IBAction)hitButtonOK:(id)sender;
- (IBAction)hitButtonCancel:(id)sender;
- (IBAction)hitButtonOther:(id)sender;

- (KeychainPromptResult)confirmStorePassword:(NSWindow*)parent;
- (BOOL)confirmChangedPassword:(NSWindow*)parent;

- (BOOL) getUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort user:(NSMutableString*)username password:(NSMutableString*)pwd item:(KCItemRef*)outItem;
- (BOOL) findUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort;
- (void) storeUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort user:(NSString*)username password:(NSString*)pwd;
- (void) removeUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort item:(KCItemRef)item;
- (void) removeAllUsernamesAndPasswords;
- (void) updateUsernameAndPassword:(NSString*)realm port:(PRInt32)inPort user:(NSString*)username password:(NSString*)pwd item:(KCItemRef)item;

- (void) addListenerToView:(CHBrowserView*)view;

- (BOOL) isEnabled;
- (BOOL) isAutoFillEnabled;

// routines to manipulate the keychain deny list for which hosts we shouldn't
// ask about
- (void) addHostToDenyList:(NSString*)host;
- (BOOL) isHostInDenyList:(NSString*)host;

@end


//
// KeychainDenyList
//
// A singleton object that maintains a list of sites where we should
// not prompt the user for saving in the keychain. This object also
// handles archiving the list in the user's profile dir.
//

@interface KeychainDenyList : NSObject
{
  NSMutableArray* mDenyList;     // the list
  BOOL mIsDirty;                 // do we need to write the list to disk?
}

+ (KeychainDenyList*) instance;
- (void) shutdown:(id)sender;

- (BOOL) isHostPresent:(NSString*)host;
- (void) addHost:(NSString*)host;
- (void) removeHost:(NSString*)host;
- (void) writeToDisk;

@end


class KeychainPrompt : public nsIAuthPromptWrapper
{
public:
  KeychainPrompt();
  virtual ~KeychainPrompt();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHPROMPT
  NS_DECL_NSIAUTHPROMPTWRAPPER
  
protected:
  
  void PreFill(const PRUnichar *, PRUnichar **, PRUnichar **);
  void ProcessPrompt(const PRUnichar *, bool, PRUnichar *, PRUnichar *);
  static void ExtractHostAndPort(const PRUnichar* inRealm, NSString** outHost, PRInt32* outPort);

  nsCOMPtr<nsIPrompt>   mPrompt;
};

//
// Keychain form submit observer
//
class KeychainFormSubmitObserver : public nsIObserver,
                                   public nsIFormSubmitObserver
{
public:
  KeychainFormSubmitObserver();
  virtual ~KeychainFormSubmitObserver();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // NS_DECL_NSIFORMSUBMITOBSERVER
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI* actionURL, PRBool* cancelSubmit);

private:

  static KeychainPromptResult CheckStorePasswordYN(nsIDOMWindowInternal*);
  static BOOL CheckChangeDataYN(nsIDOMWindowInternal*);
  
  static NSWindow* GetNSWindow(nsIDOMWindowInternal* inWindow);
};

//
// Keychain browser listener to auto fill username/passwords.
//
@interface KeychainBrowserListener : NSObject<CHBrowserListener>
{
  CHBrowserView* mBrowserView;
}

- (id)initWithBrowser:(CHBrowserView*)aBrowser;

@end

#endif
