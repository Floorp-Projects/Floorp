/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgIdentity.idl
 */

#ifndef __gen_nsIMsgIdentity_h__
#define __gen_nsIMsgIdentity_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgSignature.h" /* interface nsIMsgSignature */
#include "nsIMsgVCard.h" /* interface nsIMsgVCard */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsIMsgIdentity */

/* {D3B4A420-D5AC-11d2-806A-006008128C4E} */
#define NS_IMSGIDENTITY_IID_STR "D3B4A420-D5AC-11d2-806A-006008128C4E"
#define NS_IMSGIDENTITY_IID \
  {0xD3B4A420, 0xD5AC, 0x11d2, \
    { 0x80, 0x6A, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIMsgIdentity : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGIDENTITY_IID)

  /* attribute string key; */
  NS_IMETHOD GetKey(char * *aKey) = 0;
  NS_IMETHOD SetKey(char * aKey) = 0;

  /* attribute string identityName; */
  NS_IMETHOD GetIdentityName(char * *aIdentityName) = 0;
  NS_IMETHOD SetIdentityName(char * aIdentityName) = 0;

  /* attribute string fullName; */
  NS_IMETHOD GetFullName(char * *aFullName) = 0;
  NS_IMETHOD SetFullName(char * aFullName) = 0;

  /* attribute string email; */
  NS_IMETHOD GetEmail(char * *aEmail) = 0;
  NS_IMETHOD SetEmail(char * aEmail) = 0;

  /* attribute string replyTo; */
  NS_IMETHOD GetReplyTo(char * *aReplyTo) = 0;
  NS_IMETHOD SetReplyTo(char * aReplyTo) = 0;

  /* attribute string organization; */
  NS_IMETHOD GetOrganization(char * *aOrganization) = 0;
  NS_IMETHOD SetOrganization(char * aOrganization) = 0;

  /* attribute boolean useHtml; */
  NS_IMETHOD GetUseHtml(PRBool *aUseHtml) = 0;
  NS_IMETHOD SetUseHtml(PRBool aUseHtml) = 0;

  /* attribute boolean attachSignature; */
  NS_IMETHOD GetAttachSignature(PRBool *aAttachSignature) = 0;
  NS_IMETHOD SetAttachSignature(PRBool aAttachSignature) = 0;

  /* attribute boolean attachVCard; */
  NS_IMETHOD GetAttachVCard(PRBool *aAttachVCard) = 0;
  NS_IMETHOD SetAttachVCard(PRBool aAttachVCard) = 0;

  /* attribute nsIMsgSignature signature; */
  NS_IMETHOD GetSignature(nsIMsgSignature * *aSignature) = 0;
  NS_IMETHOD SetSignature(nsIMsgSignature * aSignature) = 0;

  /* attribute nsIMsgVCard vCard; */
  NS_IMETHOD GetVCard(nsIMsgVCard * *aVCard) = 0;
  NS_IMETHOD SetVCard(nsIMsgVCard * aVCard) = 0;

  /* attribute string smtpHostname; */
  NS_IMETHOD GetSmtpHostname(char * *aSmtpHostname) = 0;
  NS_IMETHOD SetSmtpHostname(char * aSmtpHostname) = 0;

  /* attribute string smtpUsername; */
  NS_IMETHOD GetSmtpUsername(char * *aSmtpUsername) = 0;
  NS_IMETHOD SetSmtpUsername(char * aSmtpUsername) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgIdentity *priv);
#endif
};

#endif /* __gen_nsIMsgIdentity_h__ */
