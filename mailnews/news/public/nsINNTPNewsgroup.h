/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPNewsgroup.idl
 */

#ifndef __gen_nsINNTPNewsgroup_h__
#define __gen_nsINNTPNewsgroup_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

/* starting interface:    nsINNTPNewsgroup */

/* {1A39CD90-ACAF-11d2-B7EE-00805F05FFA5} */
#define NS_INNTPNEWSGROUP_IID_STR "1A39CD90-ACAF-11d2-B7EE-00805F05FFA5"
#define NS_INNTPNEWSGROUP_IID \
  {0x1A39CD90, 0xACAF, 0x11d2, \
    { 0xB7, 0xEE, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPNewsgroup : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPNEWSGROUP_IID)

  /* attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /* attribute string prettyName; */
  NS_IMETHOD GetPrettyName(char * *aPrettyName) = 0;
  NS_IMETHOD SetPrettyName(char * aPrettyName) = 0;

  /* attribute string password; */
  NS_IMETHOD GetPassword(char * *aPassword) = 0;
  NS_IMETHOD SetPassword(char * aPassword) = 0;

  /* attribute string username; */
  NS_IMETHOD GetUsername(char * *aUsername) = 0;
  NS_IMETHOD SetUsername(char * aUsername) = 0;

  /* attribute boolean needsExtraInfo; */
  NS_IMETHOD GetNeedsExtraInfo(PRBool *aNeedsExtraInfo) = 0;
  NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo) = 0;

  /* boolean IsOfflineArticle (in long num); */
  NS_IMETHOD IsOfflineArticle(PRInt32 num, PRBool *_retval) = 0;

  /* attribute boolean category; */
  NS_IMETHOD GetCategory(PRBool *aCategory) = 0;
  NS_IMETHOD SetCategory(PRBool aCategory) = 0;

  /* attribute boolean subscribed; */
  NS_IMETHOD GetSubscribed(PRBool *aSubscribed) = 0;
  NS_IMETHOD SetSubscribed(PRBool aSubscribed) = 0;

  /* attribute boolean wantNewTotals; */
  NS_IMETHOD GetWantNewTotals(PRBool *aWantNewTotals) = 0;
  NS_IMETHOD SetWantNewTotals(PRBool aWantNewTotals) = 0;

  /* attribute nsINNTPNewsgroupList newsgroupList; */
  NS_IMETHOD GetNewsgroupList(nsINNTPNewsgroupList * *aNewsgroupList) = 0;
  NS_IMETHOD SetNewsgroupList(nsINNTPNewsgroupList * aNewsgroupList) = 0;

  /* void UpdateSummaryFromNNTPInfo (in long oldest, in long youngest, in long total_messages); */
  NS_IMETHOD UpdateSummaryFromNNTPInfo(PRInt32 oldest, PRInt32 youngest, PRInt32 total_messages) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsINNTPNewsgroup *priv);
#endif
};

#endif /* __gen_nsINNTPNewsgroup_h__ */
