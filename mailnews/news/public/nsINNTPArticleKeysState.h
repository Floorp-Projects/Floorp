/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPArticleKeysState.idl
 */

#ifndef __nsINNTPArticleKeysState_h__
#define __nsINNTPArticleKeysState_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgNewsgroup.h" /* interface nsIMsgNewsgroup */

/* starting interface nsINNTPArticleKeysState */

/* {921AC214-96B5-11d2-B7EB-00805F05FFA5} */
#define NS_INNTPARTICLEKEYSSTATE_IID_STR "921AC214-96B5-11d2-B7EB-00805F05FFA5"
#define NS_INNTPARTICLEKEYSSTATE_IID \
  {0x921AC214, 0x96B5, 0x11d2, \
    { 0xB7, 0xEB, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPArticleKeysState : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPARTICLEKEYSSTATE_IID;
    return iid;
  }

  /* void Init(in string newsHost, in nsIMsgNewsgroup newsgroup); */
  NS_IMETHOD Init(const char *newsHost, const nsIMsgNewsgroup *newsgroup) = 0;

  /* void AddArticleKey(in  key); */
  NS_IMETHOD AddArticleKey(PRInt32 key) = 0;

  /* void FinishAddingArticleKeys(); */
  NS_IMETHOD FinishAddingArticleKeys() = 0;
};

#endif /* __nsINNTPArticleKeysState_h__ */
