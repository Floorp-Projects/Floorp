/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgOfflineNewsState.idl
 */

#ifndef __nsIMsgOfflineNewsState_h__
#define __nsIMsgOfflineNewsState_h__

#include "nsISupports.h" /* interface nsISupports */

/* starting interface nsIMsgOfflineNewsState */

/* {921AC210-96B5-11d2-B7EB-00805F05FFA5} */
#define NS_IMSGOFFLINENEWSSTATE_IID_STR "921AC210-96B5-11d2-B7EB-00805F05FFA5"
#define NS_IMSGOFFLINENEWSSTATE_IID \
  {0x921AC210, 0x96B5, 0x11d2, \
    { 0xB7, 0xEB, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsIMsgOfflineNewsState : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGOFFLINENEWSSTATE_IID;
    return iid;
  }

  /*  Process(out string outputBuffer, in  bufferSize); */
  NS_IMETHOD Process(char **outputBuffer, PRInt32 bufferSize, PRInt32 *_retval) = 0;

  /*  Interrupt(); */
  NS_IMETHOD Interrupt(PRInt32 *_retval) = 0;
};

#endif /* __nsIMsgOfflineNewsState_h__ */
