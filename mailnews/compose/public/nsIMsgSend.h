/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgSend.idl
 */

#ifndef __gen_nsIMsgSend_h__
#define __gen_nsIMsgSend_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgCompFields.h" /* interface nsIMsgCompFields */
#include "nsMsgComposeBE.h"

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgSend */

/* {9E9BD970-C5D6-11d2-8297-000000000000} */
#define NS_IMSGSEND_IID_STR "9E9BD970-C5D6-11d2-8297-000000000000"
#define NS_IMSGSEND_IID \
  {0x9E9BD970, 0xC5D6, 0x11d2, \
    { 0x82, 0x97, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}

class nsIMsgSend : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGSEND_IID)

  NS_IMETHOD CreateAndSendMessage(
 						            nsIMsgCompFields                  *fields,
						            PRBool                            digest_p,
						            PRBool                            dont_deliver_p,
						            nsMsgDeliverMode                  mode, 
						            const char                        *attachment1_type,
						            const char                        *attachment1_body,
						            PRUint32                          attachment1_body_length,
						            const struct nsMsgAttachmentData  *attachments,
						            const struct nsMsgAttachedFile    *preloaded_attachments,
						            void                              *relatedPart /* nsMsgSendPart  */,
                        nsMsgSendCompletionCallback       completionCallback,
                        void                              *tagData) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgSend *priv);
#endif
};

#endif /* __gen_nsIMsgSend_h__ */
