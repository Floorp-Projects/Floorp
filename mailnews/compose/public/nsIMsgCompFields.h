/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsImsgCompFields.idl
 */

#ifndef __gen_nsImsgCompFields_h__
#define __gen_nsImsgCompFields_h__

#include "nsISupports.h" /* interface nsISupports */


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

  /*  <IDL>  */
  NS_IMETHOD Test() = 0;

  /*  <IDL>  */
  NS_IMETHOD Copy(const nsIMsgCompFields *pMsgCompFields) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetHeader(PRInt32 header, const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetHeader(PRInt32 header, char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetBoolHeader(PRInt32 header, PRBool bValue, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetBoolHeader(PRInt32 header, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetFrom(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFrom(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetReplyTo(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetReplyTo(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetTo(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetTo(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetCc(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetCc(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetBcc(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetBcc(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetFcc(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFcc(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetNewsFcc(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewsFcc(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetNewsBcc(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewsBcc(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetNewsgroups(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewsgroups(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetFollowupTo(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFollowupTo(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetSubject(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSubject(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetAttachments(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetAttachments(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetOrganization(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetOrganization(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetReferences(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetReferences(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetOtherRandomHeaders(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetOtherRandomHeaders(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetNewspostUrl(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetNewspostUrl(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetDefaultBody(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetDefaultBody(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetPriority(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPriority(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetMessageEncoding(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetMessageEncoding(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetCharacterSet(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetCharacterSet(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetMessageId(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetMessageId(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetHTMLPart(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetHTMLPart(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetTemplateName(const char *value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetTemplateName(char **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetReturnReceipt(PRBool value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetReturnReceipt(PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetAttachVCard(PRBool value, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetAttachVCard(PRBool *_retval) = 0;
};

#endif /* __gen_nsImsgCompFields_h__ */
