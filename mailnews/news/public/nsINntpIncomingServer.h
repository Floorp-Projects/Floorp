/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINntpIncomingServer.idl
 */

#ifndef __gen_nsINntpIncomingServer_h__
#define __gen_nsINntpIncomingServer_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsINntpIncomingServer */

/* {21ea0654-f773-11d2-8aec-004005263078} */
#define NS_INNTPINCOMINGSERVER_IID_STR "21ea0654-f773-11d2-8aec-004005263078"
#define NS_INNTPINCOMINGSERVER_IID \
  {0x21ea0654, 0xf773, 0x11d2, \
    { 0x8a, 0xec, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78 }}

class nsINntpIncomingServer : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPINCOMINGSERVER_IID)

  /* attribute string rootFolderPath; */
  NS_IMETHOD GetRootFolderPath(char * *aRootFolderPath) = 0;
  NS_IMETHOD SetRootFolderPath(char * aRootFolderPath) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINntpIncomingServer *priv);
#endif
};

#endif /* __gen_nsINntpIncomingServer_h__ */
