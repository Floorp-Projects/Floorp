/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPNewsgroup.idl
 */

#ifndef __gen_nsINNTPNewsgroup_h__
#define __gen_nsINNTPNewsgroup_h__

#include "nsISupports.h" /* interface nsISupports */


/* starting interface nsINNTPNewsgroup */

/* {1A39CD90-ACAF-11d2-B7EE-00805F05FFA5} */
#define NS_INNTPNEWSGROUP_IID_STR "1A39CD90-ACAF-11d2-B7EE-00805F05FFA5"
#define NS_INNTPNEWSGROUP_IID \
  {0x1A39CD90, 0xACAF, 0x11d2, \
    { 0xB7, 0xEE, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPNewsgroup : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPNEWSGROUP_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetPassword(char * *aPassword) = 0;
  NS_IMETHOD SetPassword(char * aPassword) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetUsername(char * *aUsername) = 0;
  NS_IMETHOD SetUsername(char * aUsername) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsNeedsExtraInfo(PRBool *aIsNeedsExtraInfo) = 0;
  NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsOfflineArticle(PRInt32 num, PRBool *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsCategory(PRBool *aIsCategory) = 0;
  NS_IMETHOD SetCategory(PRBool aCategory) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFlag(PRInt32 *aFlag) = 0;
  NS_IMETHOD SetFlag(PRInt32 aFlag) = 0;

  /*  <IDL>  */
  NS_IMETHOD IsSubscribed(PRBool *aIsSubscribed) = 0;
  NS_IMETHOD SetSubscribed(PRBool aSubscribed) = 0;
};

#endif /* __gen_nsINNTPNewsgroup_h__ */
