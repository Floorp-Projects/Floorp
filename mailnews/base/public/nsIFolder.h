/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFolder.idl
 */

#ifndef __gen_nsIFolder_h__
#define __gen_nsIFolder_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */

/* starting interface:    nsIFolder */

/* {75621650-0fce-11d3-8b49-006008948010} */
#define NS_IFOLDER_IID_STR "75621650-0fce-11d3-8b49-006008948010"
#define NS_IFOLDER_IID \
  {0x75621650, 0x0fce, 0x11d3, \
    { 0x8b, 0x49, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10 }}

class nsIFolder : public nsICollection {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFOLDER_IID)

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI) = 0;

  /* attribute string name; */
  NS_IMETHOD GetName(char * *aName) = 0;
  NS_IMETHOD SetName(char * aName) = 0;

  /* nsISupports GetChildNamed (in string name); */
  NS_IMETHOD GetChildNamed(const char *name, nsISupports **_retval) = 0;

  /* attribute nsIFolder parent; */
  NS_IMETHOD GetParent(nsIFolder * *aParent) = 0;
  NS_IMETHOD SetParent(nsIFolder * aParent) = 0;

  /* nsIEnumerator GetSubFolders (); */
  NS_IMETHOD GetSubFolders(nsIEnumerator **_retval) = 0;

  /* readonly attribute boolean hasSubFolders; */
  NS_IMETHOD GetHasSubFolders(PRBool *aHasSubFolders) = 0;

  /* void AddFolderListener (in nsIFolderListener listener); */
  NS_IMETHOD AddFolderListener(nsIFolderListener *listener) = 0;

  /* void RemoveFolderListener (in nsIFolderListener listener); */
  NS_IMETHOD RemoveFolderListener(nsIFolderListener *listener) = 0;

  /* nsIFolder FindSubFolder (in string subFolderName); */
  NS_IMETHOD FindSubFolder(const char *subFolderName, nsIFolder **_retval) = 0;
};

#endif /* __gen_nsIFolder_h__ */
