/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPArticleList.idl
 */

#ifndef __gen_nsINNTPArticleList_h__
#define __gen_nsINNTPArticleList_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */
#include "nsIMsgHost.h" /* interface nsIMsgHost */
#include "nsINNTPNewsgroupList.h" /* interface nsINNTPNewsgroupList */
#include "nsINNTPHost.h" /* interface nsINNTPHost */


/* starting interface nsINNTPArticleList */

/* {921AC214-96B5-11d2-B7EB-00805F05FFA5} */
#define NS_INNTPARTICLELIST_IID_STR "921AC214-96B5-11d2-B7EB-00805F05FFA5"
#define NS_INNTPARTICLELIST_IID \
  {0x921AC214, 0x96B5, 0x11d2, \
    { 0xB7, 0xEB, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPArticleList : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPARTICLELIST_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD Init(const char *newsHost, const nsINNTPNewsgroup *newsgroup) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddArticleKey(PRInt32 key) = 0;

  /*  <IDL>  */
  NS_IMETHOD FinishAddingArticleKeys() = 0;
};

#endif /* __gen_nsINNTPArticleList_h__ */
