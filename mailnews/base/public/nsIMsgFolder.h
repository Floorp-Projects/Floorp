/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgFolder.idl
 */

#ifndef __gen_nsIMsgFolder_h__
#define __gen_nsIMsgFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsISupportsArray.h" /* interface nsISupportsArray */


/* starting interface nsIMsgFolder */

/* {85e39ff0-b248-11d2-b7ef-00805f05ffa5} */
#define NS_IMSGFOLDER_IID_STR "85e39ff0-b248-11d2-b7ef-00805f05ffa5"
#define NS_IMSGFOLDER_IID \
  {0x85e39ff0, 0xb248, 0x11d2, \
    { 0xb7, 0xef, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIMsgFolder : public nsISupports {
 private:
  void operator delete(void *); // NOT TO BE IMPLEMENTED

 public: 
  static const nsIID& IID() {
    static nsIID iid = NS_IMSGFOLDER_IID;
    return iid;
  }

  /*  <IDL>  */
  NS_IMETHOD GetDepth(PRInt32 *aDepth) = 0;
  NS_IMETHOD SetDepth(PRInt32 aDepth) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetSubFolders(nsISupportsArray **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD FindParentOf(const nsIMsgFolder *childFolder, nsIMsgFolder **_retval) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSubfolder(const nsIMsgFolder *newSubfolder) = 0;

  /*  <IDL>  */
  NS_IMETHOD AddSubfolderIfUnique(const nsIMsgFolder *newSubfolder) = 0;

  /*  <IDL>  */
  NS_IMETHOD RemoveSubfolder(const nsIMsgFolder *folder) = 0;

  /*  <IDL>  */
  NS_IMETHOD ReplaceSubfolder(const nsIMsgFolder *oldFolder, const nsIMsgFolder *newFolder) = 0;

  /*  <IDL>  */
  NS_IMETHOD SetFlag(PRInt32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD ClearFlag(PRInt32 flag) = 0;

  /*  <IDL>  */
  NS_IMETHOD GetFlag(PRInt32 flag, PRBool *_retval) = 0;
};

#endif /* __gen_nsIMsgFolder_h__ */
