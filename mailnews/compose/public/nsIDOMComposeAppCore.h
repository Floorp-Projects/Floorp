/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIDOMComposeAppCore.idl
 */

#ifndef __gen_nsIDOMComposeAppCore_h__
#define __gen_nsIDOMComposeAppCore_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIDOMWindow.h" /* interface nsIDOMWindow */
#include "nsIDOMBaseAppCore.h" /* interface nsIDOMBaseAppCore */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif


/* starting interface nsIDOMComposeAppCore */

/* {D4779C9A-CAA6-11d2-A6F2-0060B0EB39B5} */
#define NS_IDOMCOMPOSEAPPCORE_IID_STR "D4779C9A-CAA6-11d2-A6F2-0060B0EB39B5"
#define NS_IDOMCOMPOSEAPPCORE_IID \
  {0xd4779c9a, 0xcaa6, 0x11d2, \
    { 0xa6, 0xf2, 0x0, 0x60, 0xb0, 0xeb, 0x39, 0xb5 }}

class nsIDOMComposeAppCore : public nsIDOMBaseAppCore {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IDOMCOMPOSEAPPCORE_IID;
    return iid;
  }

  /* void SetWindow (in nsIDOMWindow ptr); */
  NS_IMETHOD SetWindow(nsIDOMWindow *ptr) = 0;

  /* void MailCompleteCallback (); */
  NS_IMETHOD CompleteCallback(const nsString& aScript) = 0;

  /* void NewMessage (); */
  NS_IMETHOD NewMessage(const nsString& aUrl) = 0;

  /* void SendMessage (); */
  NS_IMETHOD SendMessage(const nsString& aAddrTo, const nsString& aAddrCc,  const nsString& aAddrBcc, 
	  const nsString& aSubject, const nsString& aMsg) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIDOMMsgAppCore *priv);
#endif
};

extern "C" 
nsresult NS_InitComposeAppCoreClass(nsIScriptContext *aContext, void **aPrototype);


#endif /* __gen_nsIDOMComposeAppCore_h__ */

