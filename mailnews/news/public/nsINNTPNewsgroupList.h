/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINNTPNewsgroupList.idl
 */

#ifndef __gen_nsINNTPNewsgroupList_h__
#define __gen_nsINNTPNewsgroupList_h__

#include "nsISupports.h" /* interface nsISupports */


/* starting interface nsINNTPNewsgroupList */

/* {E628ED19-9452-11d2-B7EA-00805F05FFA5} */
#define NS_INNTPNEWSGROUPLIST_IID_STR "E628ED19-9452-11d2-B7EA-00805F05FFA5"
#define NS_INNTPNEWSGROUPLIST_IID \
  {0xE628ED19, 0x9452, 0x11d2, \
    { 0xB7, 0xEA, 0x00, 0x80, 0x5F, 0x05, 0xFF, 0xA5 }}

class nsINNTPNewsgroupList : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_INNTPNEWSGROUPLIST_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetRangeOfArtsToDownload(PRInt32 first_message, PRInt32 last_message, PRInt32 maxextra, PRInt32 *real_first_message, PRInt32 *real_last_message, PRInt32 *_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddToKnownArticles(PRInt32 first_message, PRInt32 last_message) = 0;

  /*  <IDL>  */
  NS_IMETHOD InitXOVER(PRInt32 first_message, PRInt32 last_message) = 0;

  /*  <IDL>  */
  NS_IMETHOD ProcessXOVER(const char *line, PRInt32 *status) = 0;

  /*  <IDL>  */
  NS_IMETHOD ProcessNonXOVER(const char *line) = 0;

  /*  <IDL>  */
  NS_IMETHOD ResetXOVER() = 0;

  /*  <IDL>  */
  NS_IMETHOD FinishXOVER(PRInt32 status, PRInt32 *newstatus) = 0;

  /*  <IDL>  */
  NS_IMETHOD ClearXOVERState() = 0;
};

#endif /* __gen_nsINNTPNewsgroupList_h__ */
