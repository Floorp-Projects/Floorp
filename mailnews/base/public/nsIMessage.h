/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMessage.idl
 */

#ifndef __gen_nsIMessage_h__
#define __gen_nsIMessage_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgThread.h" /* interface nsIMsgThread */
#include "nsIMsgIncomingServer.h" /* interface nsIMsgIncomingServer */
#include "MailNewsTypes.h" /* interface MailNewsTypes */
#include "nsICollection.h" /* interface nsICollection */
#include "nsIMsgFolder.h" /* interface nsIMsgFolder */
#include "nsIFolderListener.h" /* interface nsIFolderListener */
#include "nsIMsgHdr.h" /* interface nsIMsgHdr */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIEnumerator.h" /* interface nsIEnumerator */
#include "nsIFolder.h" /* interface nsIFolder */

/* starting interface:    nsIMessage */

/* {5B926BB4-F839-11d2-8A5F-0060B0FC04D2} */
#define NS_IMESSAGE_IID_STR "5B926BB4-F839-11d2-8A5F-0060B0FC04D2"
#define NS_IMESSAGE_IID \
  {0x5B926BB4, 0xF839, 0x11d2, \
    { 0x8A, 0x5F, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsIMessage : public nsIMsgHdr {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMESSAGE_IID)

  /* nsIMsgFolder GetMsgFolder (); */
  NS_IMETHOD GetMsgFolder(nsIMsgFolder **_retval) = 0;

  /* void SetMsgFolder (in nsIMsgFolder folder); */
  NS_IMETHOD SetMsgFolder(nsIMsgFolder *folder) = 0;
};

/* starting interface:    nsIDBMessage */

/* {82702556-01A2-11d3-8A69-0060B0FC04D2} */
#define NS_IDBMESSAGE_IID_STR "82702556-01A2-11d3-8A69-0060B0FC04D2"
#define NS_IDBMESSAGE_IID \
  {0x82702556, 0x01A2, 0x11d3, \
    { 0x8A, 0x69, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsIDBMessage : public nsIMessage {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDBMESSAGE_IID)

  /* void SetMsgDBHdr (in nsIMsgDBHdr hdr); */
  NS_IMETHOD SetMsgDBHdr(nsIMsgDBHdr *hdr) = 0;

  /* nsIMsgDBHdr GetMsgDBHdr (); */
  NS_IMETHOD GetMsgDBHdr(nsIMsgDBHdr **_retval) = 0;
};

#endif /* __gen_nsIMessage_h__ */
