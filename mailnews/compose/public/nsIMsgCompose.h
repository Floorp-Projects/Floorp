/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgCompose.idl
 */

#ifndef __gen_nsIMsgCompose_h__
#define __gen_nsIMsgCompose_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgCompFields.h" /* interface nsIMsgCompFields */


/* starting interface nsIMsgCompose */

/* {4E606270-B588-11D2-8289-00805F2A0107} */
#define NS_IMSGCOMPOSE_IID_STR "4E606270-B588-11D2-8289-00805F2A0107"
#define NS_IMSGCOMPOSE_IID \
  {0x4E606270, 0xB588, 0x11D2, \
    { 0x82, 0x89, 0x00, 0x80, 0x5F, 0x2A, 0x01, 0x07 }}

class nsIMsgCompose : public nsISupports {
 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGCOMPOSE_IID;
    return iid;
  }

  NS_IMETHOD Test() = 0;

/*
  NS_IMETHOD CreateAndInit(PRInt32 a_context, PRInt32 old_context, PRInt32 prefs, const nsIMsgCompFields *initfields, PRInt32 master) = 0;
  NS_IMETHOD Create(PRInt32 a_context, PRInt32 prefs, PRInt32 master) = 0;
  NS_IMETHOD Initialize(PRInt32 old_context, const nsIMsgCompFields *initfields) = 0;
  NS_IMETHOD Dispose() = 0;
*/
};

#endif /* __gen_nsIMsgCompose_h__ */
