/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMessage.idl
 */

#ifndef __gen_nsIMessage_h__
#define __gen_nsIMessage_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgThread.h" /* interface nsIMsgThread */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "MailNewsTypes.h" /* interface MailNewsTypes */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIMsgFolder.h" /* interface nsIMsgFolder */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsIMsgHdr.h" /* interface nsIMsgHdr */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIFolder.h" /* interface nsIFolder */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
class nsIMsgFolder; /* forward decl */

/* starting interface:    nsIMessage */

/* {5B926BB4-F839-11d2-8A5F-0060B0FC04D2} */
#define NS_IMESSAGE_IID_STR "5B926BB4-F839-11d2-8A5F-0060B0FC04D2"
#define NS_IMESSAGE_IID \
  {0x5B926BB4, 0xF839, 0x11d2, \
    { 0x8A, 0x5F, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsIMessage : public nsIMsgHdr {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMESSAGE_IID)

  /* nsIMsgFolder GetMsgFolder (); */
  NS_IMETHOD GetMsgFolder(nsIMsgFolder **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMessage *priv);
#endif
};

#endif /* __gen_nsIMessage_h__ */
