/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFolderListener.idl
 */

#ifndef __gen_nsIFolderListener_h__
#define __gen_nsIFolderListener_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */
class nsIFolder; /* forward decl */

/* starting interface:    nsIFolderListener */

/* {1c5ef9f0-d1c0-11d2-94CA-006097222B83} */
#define NS_IFOLDERLISTENER_IID_STR "1c5ef9f0-d1c0-11d2-94CA-006097222B83"
#define NS_IFOLDERLISTENER_IID \
  {0x1c5ef9f0, 0xd1c0, 0x11d2, \
    { 0x94, 0xCA, 0x00, 0x60, 0x97, 0x22, 0x2B, 0x83 }}

class nsIFolderListener : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFOLDERLISTENER_IID)

  /* void OnItemAdded (in nsIFolder parentFolder, in nsISupports item); */
  NS_IMETHOD OnItemAdded(nsIFolder *parentFolder, nsISupports *item) = 0;

  /* void OnItemRemoved (in nsIFolder parentFolder, in nsISupports item); */
  NS_IMETHOD OnItemRemoved(nsIFolder *parentFolder, nsISupports *item) = 0;

  /* void OnItemPropertyChanged (in nsISupports item, in string property, in string oldValue, in string newValue); */
  NS_IMETHOD OnItemPropertyChanged(nsISupports *item, const char *property, const char *oldValue, const char *newValue) = 0;

  /* void OnItemPropertyFlagChanged (in nsISupports item, in string property, in unsigned long oldFlag, in unsigned long newFlag); */
  NS_IMETHOD OnItemPropertyFlagChanged(nsISupports *item, const char *property, PRUint32 oldFlag, PRUint32 newFlag) = 0;
};

#endif /* __gen_nsIFolderListener_h__ */
