/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgSend.idl
 */

#ifndef __gen_nsIMsgSend_h__
#define __gen_nsIMsgSend_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgCompFields.h" /* interface nsIMsgCompFields */


/* starting interface nsIMsgSend */

/* {9E9BD970-C5D6-11d2-8297-000000000000} */
#define NS_IMSGSEND_IID_STR "9E9BD970-C5D6-11d2-8297-000000000000"
#define NS_IMSGSEND_IID \
  {0x9E9BD970, 0xC5D6, 0x11d2, \
    { 0x82, 0x97, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}

class nsIMsgSend : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGSEND_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD Test() = 0;

  /*  <IDL>  */
  NS_IMETHOD SendMessage(const nsIMsgCompFields *fields) = 0;
};

#endif /* __gen_nsIMsgSend_h__ */
