/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgBiffManager.idl
 */

#ifndef __gen_nsIMsgBiffManager_h__
#define __gen_nsIMsgBiffManager_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgIncomingServer.h" /* interface nsIMsgIncomingServer */
#include "nsrootidl.h" /* interface nsrootidl */

/* starting interface:    nsIMsgBiffManager */

/* {17275D52-1622-11d3-8A84-0060B0FC04D2} */
#define NS_IMSGBIFFMANAGER_IID_STR "17275D52-1622-11d3-8A84-0060B0FC04D2"
#define NS_IMSGBIFFMANAGER_IID \
  {0x17275D52, 0x1622, 0x11d3, \
    { 0x8A, 0x84, 0x00, 0x60, 0xB0, 0xFC, 0x04, 0xD2 }}

class nsIMsgBiffManager : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGBIFFMANAGER_IID)

  /* void AddServerBiff (in nsIMsgIncomingServer server); */
  NS_IMETHOD AddServerBiff(nsIMsgIncomingServer *server) = 0;

  /* void RemoveServerBiff (in nsIMsgIncomingServer server); */
  NS_IMETHOD RemoveServerBiff(nsIMsgIncomingServer *server) = 0;

  /* void ForceBiff (in nsIMsgIncomingServer server); */
  NS_IMETHOD ForceBiff(nsIMsgIncomingServer *server) = 0;

  /* void ForceBiffAll (); */
  NS_IMETHOD ForceBiffAll() = 0;
};

#endif /* __gen_nsIMsgBiffManager_h__ */
