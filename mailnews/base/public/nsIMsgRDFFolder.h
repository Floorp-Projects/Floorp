/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgRDFFolder.idl
 */

#ifndef __gen_nsIMsgRDFFolder_h__
#define __gen_nsIMsgRDFFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */
#include "nsIRDFNode.h" /* interface nsIRDFNode */
#include "nsIMsgFolder.h" /* interface nsIMsgFolder */

/* starting interface nsIMsgRDFFolder */

/* {5F0E8DC0-BAD1-11d2-9578-00805F8AC615} */
#define NS_IMSGRDFFOLDER_IID_STR "5F0E8DC0-BAD1-11d2-9578-00805F8AC615"
#define NS_IMSGRDFFOLDER_IID \
  {0x5F0E8DC0, 0xBAD1, 0x11d2, \
    { 0x95, 0x78, 0x00, 0x80, 0x5F, 0x8A, 0xC6, 0x15 }}

class nsIMsgRDFFolder : public nsIRDFResource {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGRDFFOLDER_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetFolder(nsIMsgFolder * *aFolder) = 0;
  NS_IMETHOD SetFolder(nsIMsgFolder * aFolder) = 0;
};

#endif /* __gen_nsIMsgRDFFolder_h__ */
