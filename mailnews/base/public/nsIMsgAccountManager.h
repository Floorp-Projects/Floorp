/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgAccountManager.idl
 */

#ifndef __gen_nsIMsgAccountManager_h__
#define __gen_nsIMsgAccountManager_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgIncomingServer.h" /* interface nsIMsgIncomingServer */
#include "nsIMsgSignature.h" /* interface nsIMsgSignature */
#include "nsIMsgIdentity.h" /* interface nsIMsgIdentity */
#include "nsIMsgVCard.h" /* interface nsIMsgVCard */
#include "nsIMsgAccount.h" /* interface nsIMsgAccount */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgAccountManager */

/* {6ed2cc00-e623-11d2-b7fc-00805f05ffa5} */
#define NS_IMSGACCOUNTMANAGER_IID_STR "6ed2cc00-e623-11d2-b7fc-00805f05ffa5"
#define NS_IMSGACCOUNTMANAGER_IID \
  {0x6ed2cc00, 0xe623, 0x11d2, \
    { 0xb7, 0xfc, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgAccountManager : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGACCOUNTMANAGER_IID)

  /* nsIMsgAccount createAccount (in nsIMsgIncomingServer server, in nsIMsgIdentity identity); */
  NS_IMETHOD createAccount(nsIMsgIncomingServer *server, nsIMsgIdentity *identity, nsIMsgAccount **_retval) = 0;

  /* nsIMsgAccount createAccountWithKey (in nsIMsgIncomingServer server, in nsIMsgIdentity identity, in string accountKey); */
  NS_IMETHOD createAccountWithKey(nsIMsgIncomingServer *server, nsIMsgIdentity *identity, const char *accountKey, nsIMsgAccount **_retval) = 0;

  /* void addAccount (in nsIMsgAccount account); */
  NS_IMETHOD addAccount(nsIMsgAccount *account) = 0;

  /* attribute nsIMsgAccount defaultAccount; */
  NS_IMETHOD GetDefaultAccount(nsIMsgAccount * *aDefaultAccount) = 0;
  NS_IMETHOD SetDefaultAccount(nsIMsgAccount * aDefaultAccount) = 0;

  /* nsIEnumerator getAccounts (); */
  NS_IMETHOD getAccounts(nsIEnumerator **_retval) = 0;

  /* string getAccountKey (in nsIMsgAccount account); */
  NS_IMETHOD getAccountKey(nsIMsgAccount *account, char **_retval) = 0;

  /* nsIEnumerator getAllIdentities (); */
  NS_IMETHOD getAllIdentities(nsIEnumerator **_retval) = 0;

  /* nsIEnumerator getAllServers (); */
  NS_IMETHOD getAllServers(nsIEnumerator **_retval) = 0;

  /* void LoadAccounts (); */
  NS_IMETHOD LoadAccounts() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgAccountManager *priv);
#endif
};

#endif /* __gen_nsIMsgAccountManager_h__ */
