/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgMailSession.idl
 */

#ifndef __gen_nsIMsgMailSession_h__
#define __gen_nsIMsgMailSession_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgIncomingServer.h" /* interface nsIMsgIncomingServer */
#include "nsIMsgSignature.h" /* interface nsIMsgSignature */
#include "nsIMsgIdentity.h" /* interface nsIMsgIdentity */
#include "nsIMsgVCard.h" /* interface nsIMsgVCard */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIMsgAccount.h" /* interface nsIMsgAccount */
#include "nsIMsgAccountManager.h" /* interface nsIMsgAccountManager */

/* starting interface:    nsIMsgMailSession */

/* {D5124440-D59E-11d2-806A-006008128C4E} */
#define NS_IMSGMAILSESSION_IID_STR "D5124440-D59E-11d2-806A-006008128C4E"
#define NS_IMSGMAILSESSION_IID \
  {0xD5124440, 0xD59E, 0x11d2, \
    { 0x80, 0x6A, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIMsgMailSession : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGMAILSESSION_IID)

  /* readonly attribute nsIMsgIdentity currentIdentity; */
  NS_IMETHOD GetCurrentIdentity(nsIMsgIdentity * *aCurrentIdentity) = 0;

  /* readonly attribute nsIMsgIncomingServer currentServer; */
  NS_IMETHOD GetCurrentServer(nsIMsgIncomingServer * *aCurrentServer) = 0;

  /* readonly attribute nsIMsgAccountManager accountManager; */
  NS_IMETHOD GetAccountManager(nsIMsgAccountManager * *aAccountManager) = 0;

  /* void AddFolderListener (in nsIFolderListener listener); */
  NS_IMETHOD AddFolderListener(nsIFolderListener *listener) = 0;

  /* void RemoveFolderListener (in nsIFolderListener listener); */
  NS_IMETHOD RemoveFolderListener(nsIFolderListener *listener) = 0;

  /* void NotifyFolderItemPropertyChanged (in nsISupports item, in string property, in string oldValue, in string newValue); */
  NS_IMETHOD NotifyFolderItemPropertyChanged(nsISupports *item, const char *property, const char *oldValue, const char *newValue) = 0;

  /* void NotifyFolderItemPropertyFlagChanged (in nsISupports item, in string property, in unsigned long oldValue, in unsigned long newValue); */
  NS_IMETHOD NotifyFolderItemPropertyFlagChanged(nsISupports *item, const char *property, PRUint32 oldValue, PRUint32 newValue) = 0;

  /* void NotifyFolderItemAdded (in nsIFolder folder, in nsISupports item); */
  NS_IMETHOD NotifyFolderItemAdded(nsIFolder *folder, nsISupports *item) = 0;

  /* void NotifyFolderItemDeleted (in nsIFolder folder, in nsISupports item); */
  NS_IMETHOD NotifyFolderItemDeleted(nsIFolder *folder, nsISupports *item) = 0;
};

#endif /* __gen_nsIMsgMailSession_h__ */
