/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIDOMMsgAppCore.idl
 */

#ifndef __gen_nsIDOMMsgAppCore_h__
#define __gen_nsIDOMMsgAppCore_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIDOMWindow.h" /* interface nsIDOMWindow */
#include "nsID.h" /* interface nsID */
#include "nsIDOMXULTreeElement.h" /* interface nsIDOMXULTreeElement */
#include "nsIDOMBaseAppCore.h" /* interface nsIDOMBaseAppCore */
#include "nsIDOMNodeList.h" /* interface nsIDOMNodeList */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface nsIDOMMsgAppCore */

/* {4f7966d0-c14f-11d2-b7f2-00805f05ffa5} */
#define NS_IDOMMSGAPPCORE_IID_STR "4f7966d0-c14f-11d2-b7f2-00805f05ffa5"
#define NS_IDOMMSGAPPCORE_IID \
  {0x4f7966d0, 0xc14f, 0x11d2, \
    { 0xb7, 0xf2, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIDOMMsgAppCore : public nsIDOMBaseAppCore {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IDOMMSGAPPCORE_IID;
    return iid;
  }

  /* void GetNewMail (); */
  NS_IMETHOD GetNewMail() = 0;

  /* void Open3PaneWindow (); */
  NS_IMETHOD Open3PaneWindow() = 0;

  /* void SetWindow (in nsIDOMWindow ptr); */
  NS_IMETHOD SetWindow(nsIDOMWindow *ptr) = 0;

  /* void OpenURL (in string str); */
  NS_IMETHOD OpenURL(const char *str) = 0;

  /* void DeleteMessage (in nsIDOMXULTreeElement tree, in nsIDOMNodeList node); */
  NS_IMETHOD DeleteMessage(nsIDOMXULTreeElement *tree, nsIDOMNodeList *node) = 0;

  /* nsISupports GetRDFResourceForMessage (in nsIDOMXULTreeElement tree, in nsIDOMNodeList node); */
  NS_IMETHOD GetRDFResourceForMessage(nsIDOMXULTreeElement *tree, nsIDOMNodeList *node, nsISupports **_retval) = 0;

  /* void Exit (); */
  NS_IMETHOD Exit() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIDOMMsgAppCore *priv);
#endif
};
extern "C" 
nsresult NS_InitMsgAppCoreClass(nsIScriptContext *aContext, void **aPrototype);
 


#endif /* __gen_nsIDOMMsgAppCore_h__ */
