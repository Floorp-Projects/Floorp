/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsICopyMessageListener.idl
 */

#ifndef __gen_nsICopyMessageListener_h__
#define __gen_nsICopyMessageListener_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgThread.h" /* interface nsIMsgThread */
#include "nsIMsgIncomingServer.h" /* interface nsIMsgIncomingServer */
#include "MailNewsTypes.h" /* interface MailNewsTypes */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIMsgFolder.h" /* interface nsIMsgFolder */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIMsgHdr.h" /* interface nsIMsgHdr */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIMessage.h" /* interface nsIMessage */
#include "nsIFolder.h" /* interface nsIFolder */
class nsIInputStream; /* forward decl */
#include "nsIInputStream.h";


/* starting interface:    nsICopyMessageListener */

/* {53CA78FE-E231-11d2-8A4D-0060B0FC04D2} */
#define NS_ICOPYMESSAGELISTENER_IID_STR "53CA78FE-E231-11d2-8A4D-0060B0FC04D2"
#define NS_ICOPYMESSAGELISTENER_IID \
  {0x53CA78FE, 0xE231, 0x11d2, \
    { 0x8A, 0x4D, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsICopyMessageListener : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOPYMESSAGELISTENER_IID)

  /* void BeginCopy (in nsIMessage message); */
  NS_IMETHOD BeginCopy(nsIMessage *message) = 0;

  /* void CopyData (in nsIInputStream aIStream, in long aLength); */
  NS_IMETHOD CopyData(nsIInputStream *aIStream, PRInt32 aLength) = 0;

  /* void EndCopy (in boolean copySucceeded); */
  NS_IMETHOD EndCopy(PRBool copySucceeded) = 0;
};

#endif /* __gen_nsICopyMessageListener_h__ */
