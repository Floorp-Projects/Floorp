/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIPop3Sink.idl
 */

#ifndef __gen_nsIPop3Sink_h__
#define __gen_nsIPop3Sink_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIPop3IncomingServer.h" /* interface nsIPop3IncomingServer */
#include "nsrootidl.h" /* interface nsrootidl */
class nsIURL; /* forward decl */

/* starting interface:    nsIPop3Sink */

/* {EC54D490-BB00-11D2-AB5E-00805F8AC968} */
#define NS_IPOP3SINK_IID_STR "EC54D490-BB00-11D2-AB5E-00805F8AC968"
#define NS_IPOP3SINK_IID \
  {0xEC54D490, 0xBB00, 0x11D2, \
    { 0xAB, 0x5E, 0x00, 0x80, 0x5F, 0x8A, 0xC9, 0x68 }}

class nsIPop3Sink : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPOP3SINK_IID)

  /* attribute boolean userAuthenticated; */
  NS_IMETHOD GetUserAuthenticated(PRBool *aUserAuthenticated) = 0;
  NS_IMETHOD SetUserAuthenticated(PRBool aUserAuthenticated) = 0;

  /* attribute string mailAccountURL; */
  NS_IMETHOD GetMailAccountURL(char * *aMailAccountURL) = 0;
  NS_IMETHOD SetMailAccountURL(char * aMailAccountURL) = 0;

  /* boolean BeginMailDelivery (); */
  NS_IMETHOD BeginMailDelivery(PRBool *_retval) = 0;

  /* void EndMailDelivery (); */
  NS_IMETHOD EndMailDelivery() = 0;

  /* void AbortMailDelivery (); */
  NS_IMETHOD AbortMailDelivery() = 0;

  /* voidStar IncorporateBegin (in string uidlString, in nsIURL aURL, in unsigned long flags); */
  NS_IMETHOD IncorporateBegin(const char *uidlString, nsIURL *aURL, PRUint32 flags, void * *_retval) = 0;

  /* void IncorporateWrite (in voidStar closure, in string block, in long length); */
  NS_IMETHOD IncorporateWrite(void * closure, const char *block, PRInt32 length) = 0;

  /* void IncorporateComplete (in voidStar closure); */
  NS_IMETHOD IncorporateComplete(void * closure) = 0;

  /* void IncorporateAbort (in voidStar closure, in long status); */
  NS_IMETHOD IncorporateAbort(void * closure, PRInt32 status) = 0;

  /* void BiffGetNewMail (); */
  NS_IMETHOD BiffGetNewMail() = 0;

  /* void SetBiffStateAndUpdateFE (in unsigned long biffState); */
  NS_IMETHOD SetBiffStateAndUpdateFE(PRUint32 biffState) = 0;

  /* void SetSenderAuthedFlag (in voidStar closure, in boolean authed); */
  NS_IMETHOD SetSenderAuthedFlag(void * closure, PRBool authed) = 0;

  /* attribute nsIPop3IncomingServer popServer; */
  NS_IMETHOD GetPopServer(nsIPop3IncomingServer * *aPopServer) = 0;
  NS_IMETHOD SetPopServer(nsIPop3IncomingServer * aPopServer) = 0;
};

#endif /* __gen_nsIPop3Sink_h__ */
