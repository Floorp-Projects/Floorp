/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgCompFields.idl
 */

#ifndef __gen_nsIMsgCompFields_h__
#define __gen_nsIMsgCompFields_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"


/* starting interface nsIMsgCompFields */

/* {3E89CAE3-BD2D-11d2-8293-000000000000} */
#define NS_IMSGCOMPFIELDS_IID_STR "3E89CAE3-BD2D-11d2-8293-000000000000"
#define NS_IMSGCOMPFIELDS_IID \
  {0x3E89CAE3, 0xBD2D, 0x11d2, \
    { 0x82, 0x93, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}

class nsIMsgCompFields : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGCOMPFIELDS_IID;
    return iid;
  }

  /* void Test (); */
  NS_IMETHOD Test() = 0;

  /* void Copy (in nsIMsgCompFields pMsgCompFields); */
  NS_IMETHOD Copy(nsIMsgCompFields *pMsgCompFields) = 0;

  /* long SetHeader (in long header, in string value); */
  NS_IMETHOD SetHeader(PRInt32 header, char *value, PRInt32 *_retval) = 0;

  /* string GetHeader (in long header); */
  NS_IMETHOD GetHeader(PRInt32 header, char **_retval) = 0;

  /* long SetBoolHeader (in long header, in boolean bValue); */
  NS_IMETHOD SetBoolHeader(PRInt32 header, PRBool bValue, PRInt32 *_retval) = 0;

  /* boolean GetBoolHeader (in long header); */
  NS_IMETHOD GetBoolHeader(PRInt32 header, PRBool *_retval) = 0;

  /* long SetFrom (in string value); */
  NS_IMETHOD SetFrom(char *value, PRInt32 *_retval) = 0;

  /* string GetFrom (); */
  NS_IMETHOD GetFrom(char **_retval) = 0;

  /* long SetReplyTo (in string value); */
  NS_IMETHOD SetReplyTo(char *value, PRInt32 *_retval) = 0;

  /* string GetReplyTo (); */
  NS_IMETHOD GetReplyTo(char **_retval) = 0;

  /* long SetTo (in string value); */
  NS_IMETHOD SetTo(char *value, PRInt32 *_retval) = 0;

  /* string GetTo (); */
  NS_IMETHOD GetTo(char **_retval) = 0;

  /* long SetCc (in string value); */
  NS_IMETHOD SetCc(char *value, PRInt32 *_retval) = 0;

  /* string GetCc (); */
  NS_IMETHOD GetCc(char **_retval) = 0;

  /* long SetBcc (in string value); */
  NS_IMETHOD SetBcc(char *value, PRInt32 *_retval) = 0;

  /* string GetBcc (); */
  NS_IMETHOD GetBcc(char **_retval) = 0;

  /* long SetFcc (in string value); */
  NS_IMETHOD SetFcc(char *value, PRInt32 *_retval) = 0;

  /* string GetFcc (); */
  NS_IMETHOD GetFcc(char **_retval) = 0;

  /* long SetNewsFcc (in string value); */
  NS_IMETHOD SetNewsFcc(char *value, PRInt32 *_retval) = 0;

  /* string GetNewsFcc (); */
  NS_IMETHOD GetNewsFcc(char **_retval) = 0;

  /* long SetNewsBcc (in string value); */
  NS_IMETHOD SetNewsBcc(char *value, PRInt32 *_retval) = 0;

  /* string GetNewsBcc (); */
  NS_IMETHOD GetNewsBcc(char **_retval) = 0;

  /* long SetNewsgroups (in string value); */
  NS_IMETHOD SetNewsgroups(char *value, PRInt32 *_retval) = 0;

  /* string GetNewsgroups (); */
  NS_IMETHOD GetNewsgroups(char **_retval) = 0;

  /* long SetFollowupTo (in string value); */
  NS_IMETHOD SetFollowupTo(char *value, PRInt32 *_retval) = 0;

  /* string GetFollowupTo (); */
  NS_IMETHOD GetFollowupTo(char **_retval) = 0;

  /* long SetSubject (in string value); */
  NS_IMETHOD SetSubject(char *value, PRInt32 *_retval) = 0;

  /* string GetSubject (); */
  NS_IMETHOD GetSubject(char **_retval) = 0;

  /* long SetAttachments (in string value); */
  NS_IMETHOD SetAttachments(char *value, PRInt32 *_retval) = 0;

  /* string GetAttachments (); */
  NS_IMETHOD GetAttachments(char **_retval) = 0;

  /* long SetOrganization (in string value); */
  NS_IMETHOD SetOrganization(char *value, PRInt32 *_retval) = 0;

  /* string GetOrganization (); */
  NS_IMETHOD GetOrganization(char **_retval) = 0;

  /* long SetReferences (in string value); */
  NS_IMETHOD SetReferences(char *value, PRInt32 *_retval) = 0;

  /* string GetReferences (); */
  NS_IMETHOD GetReferences(char **_retval) = 0;

  /* long SetOtherRandomHeaders (in string value); */
  NS_IMETHOD SetOtherRandomHeaders(char *value, PRInt32 *_retval) = 0;

  /* string GetOtherRandomHeaders (); */
  NS_IMETHOD GetOtherRandomHeaders(char **_retval) = 0;

  /* long SetNewspostUrl (in string value); */
  NS_IMETHOD SetNewspostUrl(char *value, PRInt32 *_retval) = 0;

  /* string GetNewspostUrl (); */
  NS_IMETHOD GetNewspostUrl(char **_retval) = 0;

  /* long SetDefaultBody (in string value); */
  NS_IMETHOD SetDefaultBody(char *value, PRInt32 *_retval) = 0;

  /* string GetDefaultBody (); */
  NS_IMETHOD GetDefaultBody(char **_retval) = 0;

  /* long SetPriority (in string value); */
  NS_IMETHOD SetPriority(char *value, PRInt32 *_retval) = 0;

  /* string GetPriority (); */
  NS_IMETHOD GetPriority(char **_retval) = 0;

  /* long SetMessageEncoding (in string value); */
  NS_IMETHOD SetMessageEncoding(char *value, PRInt32 *_retval) = 0;

  /* string GetMessageEncoding (); */
  NS_IMETHOD GetMessageEncoding(char **_retval) = 0;

  /* long SetCharacterSet (in string value); */
  NS_IMETHOD SetCharacterSet(char *value, PRInt32 *_retval) = 0;

  /* string GetCharacterSet (); */
  NS_IMETHOD GetCharacterSet(char **_retval) = 0;

  /* long SetMessageId (in string value); */
  NS_IMETHOD SetMessageId(char *value, PRInt32 *_retval) = 0;

  /* string GetMessageId (); */
  NS_IMETHOD GetMessageId(char **_retval) = 0;

  /* long SetHTMLPart (in string value); */
  NS_IMETHOD SetHTMLPart(char *value, PRInt32 *_retval) = 0;

  /* string GetHTMLPart (); */
  NS_IMETHOD GetHTMLPart(char **_retval) = 0;

  /* long SetTemplateName (in string value); */
  NS_IMETHOD SetTemplateName(char *value, PRInt32 *_retval) = 0;

  /* string GetTemplateName (); */
  NS_IMETHOD GetTemplateName(char **_retval) = 0;

  /* long SetReturnReceipt (in boolean value); */
  NS_IMETHOD SetReturnReceipt(PRBool value, PRInt32 *_retval) = 0;

  /* boolean GetReturnReceipt (); */
  NS_IMETHOD GetReturnReceipt(PRBool *_retval) = 0;

  /* long SetAttachVCard (in boolean value); */
  NS_IMETHOD SetAttachVCard(PRBool value, PRInt32 *_retval) = 0;

  /* boolean GetAttachVCard (); */
  NS_IMETHOD GetAttachVCard(PRBool *_retval) = 0;

  /* long SetBody (in string value); */
  NS_IMETHOD SetBody(char *value, PRInt32 *_retval) = 0;

  /* string GetBody (); */
  NS_IMETHOD GetBody(char **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgCompFields *priv);
#endif
};

#endif /* __gen_nsIMsgCompFields_h__ */
