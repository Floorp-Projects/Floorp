/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIPop3URL.idl
 */

#ifndef __gen_nsIPop3URL_h__
#define __gen_nsIPop3URL_h__

#include "nsISupports.h"
#include "nsIPop3IncomingServer.h"
#include "nsIMsgThread.h"
#include "nsIMsgIncomingServer.h"
#include "nsIFileSpec.h"
#include "nsICollection.h"
#include "nsIMsgFolder.h"
#include "nsIPop3Sink.h"
#include "nsIFolderListener.h"
#include "nsrootidl.h"
#include "nsIEnumerator.h"
#include "nsIFolder.h"
#include "MailNewsTypes2.h"

/* starting interface:    nsIPop3URL */

/* {73c043d0-b7e2-11d2-ab5c-00805f8ac968} */
#define NS_IPOP3URL_IID_STR "73c043d0-b7e2-11d2-ab5c-00805f8ac968"
#define NS_IPOP3URL_IID \
  {0x73c043d0, 0xb7e2, 0x11d2, \
    { 0xab, 0x5c, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 }}

class nsIPop3URL : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPOP3URL_IID)

  /* attribute nsIPop3Sink pop3Sink; */
  NS_IMETHOD GetPop3Sink(nsIPop3Sink * *aPop3Sink) = 0;
  NS_IMETHOD SetPop3Sink(nsIPop3Sink * aPop3Sink) = 0;
};

#endif /* __gen_nsIPop3URL_h__ */
