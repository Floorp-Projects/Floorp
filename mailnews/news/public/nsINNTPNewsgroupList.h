/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPNewsgroupList.idl
 */

#ifndef __gen_nsINNTPNewsgroupList_h__
#define __gen_nsINNTPNewsgroupList_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsINNTPNewsgroupList */

/* {E628ED19-9452-11d2-B7EA-00805F05FFA5} */
#define NS_INNTPNEWSGROUPLIST_IID_STR "E628ED19-9452-11d2-B7EA-00805F05FFA5"
#define NS_INNTPNEWSGROUPLIST_IID \
  {0xE628ED19, 0x9452, 0x11d2, \
    { 0xB7, 0xEA, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPNewsgroupList : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPNEWSGROUPLIST_IID)

  /* long GetRangeOfArtsToDownload (in long first_message, in long last_message, in long maxextra, out long real_first_message, out long real_last_message); */
  NS_IMETHOD GetRangeOfArtsToDownload(PRInt32 first_message, PRInt32 last_message, PRInt32 maxextra, PRInt32 *real_first_message, PRInt32 *real_last_message, PRInt32 *_retval) = 0;

  /* void AddToKnownArticles (in long first_message, in long last_message); */
  NS_IMETHOD AddToKnownArticles(PRInt32 first_message, PRInt32 last_message) = 0;

  /* void InitXOVER (in long first_message, in long last_message); */
  NS_IMETHOD InitXOVER(PRInt32 first_message, PRInt32 last_message) = 0;

  /* void ProcessXOVER (in string line, out long status); */
  NS_IMETHOD ProcessXOVER(const char *line, PRUint32 *status) = 0;

  /* void ProcessNonXOVER (in string line); */
  NS_IMETHOD ProcessNonXOVER(const char *line) = 0;

  /* void ResetXOVER (); */
  NS_IMETHOD ResetXOVER() = 0;

  /* void FinishXOVER (in long status, out long newstatus); */
  NS_IMETHOD FinishXOVER(PRInt32 status, PRInt32 *newstatus) = 0;

  /* void ClearXOVERState (); */
  NS_IMETHOD ClearXOVERState() = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPNewsgroupList *priv);
#endif
};

#endif /* __gen_nsINNTPNewsgroupList_h__ */
