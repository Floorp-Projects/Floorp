/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgOfflineNewsState.idl
 */

#ifndef __gen_nsIMsgOfflineNewsState_h__
#define __gen_nsIMsgOfflineNewsState_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsID.h" /* interface nsID */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgOfflineNewsState */

/* {921AC210-96B5-11d2-B7EB-00805F05FFA5} */
#define NS_IMSGOFFLINENEWSSTATE_IID_STR "921AC210-96B5-11d2-B7EB-00805F05FFA5"
#define NS_IMSGOFFLINENEWSSTATE_IID \
  {0x921AC210, 0x96B5, 0x11d2, \
    { 0xB7, 0xEB, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsIMsgOfflineNewsState : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGOFFLINENEWSSTATE_IID)

  /* attribute nsINNTPNewsgroup newsgroup; */
  NS_IMETHOD GetNewsgroup(nsINNTPNewsgroup * *aNewsgroup) = 0;
  NS_IMETHOD SetNewsgroup(nsINNTPNewsgroup * aNewsgroup) = 0;

  /* long Process (out string outputBuffer, in long bufferSize); */
  NS_IMETHOD Process(char **outputBuffer, PRInt32 bufferSize, PRInt32 *_retval) = 0;

  /* long Interrupt (); */
  NS_IMETHOD Interrupt(PRInt32 *_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgOfflineNewsState *priv);
#endif
};

#endif /* __gen_nsIMsgOfflineNewsState_h__ */
