/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIDOMMsgAppCore.idl
 */

#ifndef __gen_nsIDOMMsgAppCore_h__
#define __gen_nsIDOMMsgAppCore_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIDOMBaseAppCore.h" /* interface nsIDOMBaseAppCore */


/* starting interface nsIDOMMsgAppCore */

/* {4f7966d0-c14f-11d2-b7f2-00805f05ffa5} */
#define NS_IDOMMSGAPPCORE_IID_STR "4f7966d0-c14f-11d2-b7f2-00805f05ffa5"
#define NS_IDOMMSGAPPCORE_IID \
  {0x4f7966d0, 0xc14f, 0x11d2, \
    { 0xb7, 0xf2, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIDOMMsgAppCore : public nsIDOMBaseAppCore {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IDOMMSGAPPCORE_IID;
    return iid;
  }

  /* void GetNewMail (); */
  NS_IMETHOD GetNewMail() = 0;
};

#endif /* __gen_nsIDOMMsgAppCore_h__ */
