/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIDOMComposeAppCore.idl
 */

#ifndef __gen_nsIDOMComposeAppCore_h__
#define __gen_nsIDOMComposeAppCore_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIDOMWindow.h" /* interface nsIDOMWindow */
#include "nsID.h" /* interface nsID */
#include "nsIDOMBaseAppCore.h" /* interface nsIDOMBaseAppCore */
#include "nsIDOMEditorAppCore.h" /* interface nsIDOMEditorAppCore */
#include "nsIDOMMsgAppCore.h" /* interface nsIDOMEditorAppCore */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface nsIDOMComposeAppCore */

/* {D4779C9A-CAA6-11d2-A6F2-0060B0EB39B5} */
#define NS_IDOMCOMPOSEAPPCORE_IID_STR "D4779C9A-CAA6-11d2-A6F2-0060B0EB39B5"
#define NS_IDOMCOMPOSEAPPCORE_IID \
  {0xD4779C9A, 0xCAA6, 0x11d2, \
    { 0xA6, 0xF2, 0x00, 0x60, 0xB0, 0xEB, 0x39, 0xB5 }}

class nsIDOMComposeAppCore : public nsIDOMBaseAppCore {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IDOMCOMPOSEAPPCORE_IID;
    return iid;
  }

  /* void SetWindow (in nsIDOMWindow ptr); */
  NS_IMETHOD SetWindow(nsIDOMWindow *ptr) = 0;

  /* void SetEditor (in nsIDOMWindow ptr); */
  NS_IMETHOD SetEditor(nsIDOMEditorAppCore *ptr) = 0;

  /* void CompleteCallback (in nsAutoString script); */
  NS_IMETHOD CompleteCallback(nsAutoString& script) = 0;

  /* void NewMessage (in nsAutoString url); */
  NS_IMETHOD NewMessage(nsAutoString& url, nsIDOMXULTreeElement *tree,
												nsIDOMNodeList *nodeList, nsIDOMMsgAppCore *
												msgAppCore, const PRInt32 messageType) = 0;

	/* void SendMessage (in nsAutoString addrTo, in nsAutoString addrCc, in
		 nsAutoString addrBcc, in nsAutoString subject, in nsAutoString msg); */ 
  NS_IMETHOD SendMessage(nsAutoString& addrTo, nsAutoString& addrCc,
												 nsAutoString& addrBcc, nsAutoString& subject,
												 nsAutoString& msg) = 0;

  /* void SendMessage2 (); */
  NS_IMETHOD SendMessage2(PRInt32 *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx,
																						nsIDOMComposeAppCore *priv);
#endif
};
extern "C" 
nsresult NS_InitComposeAppCoreClass(nsIScriptContext *aContext, void
																		**aPrototype);
 


#endif /* __gen_nsIDOMComposeAppCore_h__ */
