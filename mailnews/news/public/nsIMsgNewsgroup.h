/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgNewsgroup.idl
 */

#ifndef __nsIMsgNewsgroup_h__
#define __nsIMsgNewsgroup_h__

#include "nsISupports.h" /* interface nsISupports */

/* starting interface nsIMsgNewsgroup */

/* {E628ED10-9452-11d2-B7EA-00805F05FFA5} */
#define NS_IMSGNEWSGROUP_IID_STR "E628ED10-9452-11d2-B7EA-00805F05FFA5"
#define NS_IMSGNEWSGROUP_IID \
  {0xE628ED10, 0x9452, 0x11d2, \
    { 0xB7, 0xEA, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsIMsgNewsgroup : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGNEWSGROUP_IID;
    return iid;
  }

  /* attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /* attribute string password; */
  NS_IMETHOD GetPassword(char * *aPassword) = 0;
  NS_IMETHOD SetPassword(char * aPassword) = 0;

  /* attribute string username; */
  NS_IMETHOD GetUsername(char * *aUsername) = 0;
  NS_IMETHOD SetUsername(char * aUsername) = 0;

  /* attribute boolean needsExtraInfo; */
  NS_IMETHOD IsNeedsExtraInfo(PRBool *aIsNeedsExtraInfo) = 0;
  NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo) = 0;

  /* boolean IsOfflineArticle(in  num); */
  NS_IMETHOD IsOfflineArticle(PRInt32 num, PRBool *_retval) = 0;
};

#endif /* __nsIMsgNewsgroup_h__ */
