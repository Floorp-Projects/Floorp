/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPNewsgroupPost.idl
 */

#ifndef __gen_nsINNTPNewsgroupPost_h__
#define __gen_nsINNTPNewsgroupPost_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsINNTPNewsgroupPost */

/* {c4164a20-bc74-11d2-b7f2-00805f05ffa5} */
#define NS_INNTPNEWSGROUPPOST_IID_STR "c4164a20-bc74-11d2-b7f2-00805f05ffa5"
#define NS_INNTPNEWSGROUPPOST_IID \
  {0xc4164a20, 0xbc74, 0x11d2, \
    { 0xb7, 0xf2, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsINNTPNewsgroupPost : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPNEWSGROUPPOST_IID)

  /* attribute string relayVersion; */
  NS_IMETHOD GetRelayVersion(char * *aRelayVersion) = 0;
  NS_IMETHOD SetRelayVersion(char * aRelayVersion) = 0;

  /* attribute string postingVersion; */
  NS_IMETHOD GetPostingVersion(char * *aPostingVersion) = 0;
  NS_IMETHOD SetPostingVersion(char * aPostingVersion) = 0;

  /* attribute string from; */
  NS_IMETHOD GetFrom(char * *aFrom) = 0;
  NS_IMETHOD SetFrom(char * aFrom) = 0;

  /* attribute string date; */
  NS_IMETHOD GetDate(char * *aDate) = 0;
  NS_IMETHOD SetDate(char * aDate) = 0;

  /* void AddNewsgroup (in string newsgroupName); */
  NS_IMETHOD AddNewsgroup(const char *newsgroupName) = 0;

  /* readonly attribute string newsgroups; */
  NS_IMETHOD GetNewsgroups(char * *aNewsgroups) = 0;

  /* attribute string subject; */
  NS_IMETHOD GetSubject(char * *aSubject) = 0;
  NS_IMETHOD SetSubject(char * aSubject) = 0;

  /* readonly attribute string messageID; */
  NS_IMETHOD GetMessageID(char * *aMessageID) = 0;

  /* attribute string path; */
  NS_IMETHOD GetPath(char * *aPath) = 0;
  NS_IMETHOD SetPath(char * aPath) = 0;

  /* boolean isValid (); */
  NS_IMETHOD isValid(PRBool *_retval) = 0;

  /* attribute string replyTo; */
  NS_IMETHOD GetReplyTo(char * *aReplyTo) = 0;
  NS_IMETHOD SetReplyTo(char * aReplyTo) = 0;

  /* attribute string sender; */
  NS_IMETHOD GetSender(char * *aSender) = 0;
  NS_IMETHOD SetSender(char * aSender) = 0;

  /* attribute string followupTo; */
  NS_IMETHOD GetFollowupTo(char * *aFollowupTo) = 0;
  NS_IMETHOD SetFollowupTo(char * aFollowupTo) = 0;

  /* attribute string dateRecieved; */
  NS_IMETHOD GetDateRecieved(char * *aDateRecieved) = 0;
  NS_IMETHOD SetDateRecieved(char * aDateRecieved) = 0;

  /* attribute string expires; */
  NS_IMETHOD GetExpires(char * *aExpires) = 0;
  NS_IMETHOD SetExpires(char * aExpires) = 0;

  /* void AddReference (in string referenceID); */
  NS_IMETHOD AddReference(const char *referenceID) = 0;

  /* readonly attribute string references; */
  NS_IMETHOD GetReferences(char * *aReferences) = 0;

  /* attribute string control; */
  NS_IMETHOD GetControl(char * *aControl) = 0;
  NS_IMETHOD SetControl(char * aControl) = 0;

  /* attribute string distribution; */
  NS_IMETHOD GetDistribution(char * *aDistribution) = 0;
  NS_IMETHOD SetDistribution(char * aDistribution) = 0;

  /* attribute string organization; */
  NS_IMETHOD GetOrganization(char * *aOrganization) = 0;
  NS_IMETHOD SetOrganization(char * aOrganization) = 0;

  /* attribute string body; */
  NS_IMETHOD GetBody(char * *aBody) = 0;
  NS_IMETHOD SetBody(char * aBody) = 0;

  /* void MakeControlCancel (in string messageID); */
  NS_IMETHOD MakeControlCancel(const char *messageID) = 0;

  /* readonly attribute boolean isControl; */
  NS_IMETHOD GetIsControl(PRBool *aIsControl) = 0;

  /* string GetFullMessage (); */
  NS_IMETHOD GetFullMessage(char **_retval) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPNewsgroupPost *priv);
#endif
};

#endif /* __gen_nsINNTPNewsgroupPost_h__ */
