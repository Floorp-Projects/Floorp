/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgNewsArticleList.idl
 */

#ifndef __nsIMsgNewsArticleList_h__
#define __nsIMsgNewsArticleList_h__

#include "nsISupports.h" /* interface nsISupports */

/* starting interface nsIMsgNewsArticleList */

/* {E628ED19-9452-11d2-B7EA-00805F05FFA5} */
#define NS_IMSGNEWSARTICLELIST_IID_STR "E628ED19-9452-11d2-B7EA-00805F05FFA5"
#define NS_IMSGNEWSARTICLELIST_IID \
  {0xE628ED19, 0x9452, 0x11d2, \
    { 0xB7, 0xEA, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsIMsgNewsArticleList : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGNEWSARTICLELIST_IID;
    return iid;
  }

  /* void InitAddArticleKeyToGroup(); */
  NS_IMETHOD InitAddArticleKeyToGroup() = 0;

  /* void AddArticleKeyToGroup(in  found_id); */
  NS_IMETHOD AddArticleKeyToGroup(PRInt32 found_id) = 0;

  /* void FinishAddArticleKeyToGroup(); */
  NS_IMETHOD FinishAddArticleKeyToGroup() = 0;

  /* void GetRangeOfArtsToDownload(out  status, in  first_message, in  last_message, in  maxextra, out  real_first_message, out  real_last_message); */
  NS_IMETHOD GetRangeOfArtsToDownload(PRInt32 *status, PRInt32 first_message, PRInt32 last_message, PRInt32 maxextra, PRInt32 *real_first_message, PRInt32 *real_last_message) = 0;

  /* void AddToKnownArticles(in  first_message, in  last_message); */
  NS_IMETHOD AddToKnownArticles(PRInt32 first_message, PRInt32 last_message) = 0;
};

#endif /* __nsIMsgNewsArticleList_h__ */
