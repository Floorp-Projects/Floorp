/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIPop3IncomingServer.idl
 */

#ifndef __gen_nsIImapIncomingServer_h__
#define __gen_nsIImapIncomingServer_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

/* starting interface:    nsIImapIncomingServer */

/* {758a8970-e628-11d2-b7fc-00805f05ffa5} */
#define NS_IIMAPINCOMINGSERVER_IID_STR "758a8970-e628-11d2-b7fc-00805f05ffa5"
#define NS_IIMAPINCOMINGSERVER_IID \
  {0x758a8970, 0xe628, 0x11d2, \
    { 0xb7, 0xfc, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5 }}

class nsIImapIncomingServer : public nsISupports {
 public: 
  static const nsIID& GetIID() {
    static nsIID iid = NS_IIMAPINCOMINGSERVER_IID;
    return iid;
  }

  /* attribute string rootFolderPath; */
  NS_IMETHOD GetRootFolderPath(char * *aRootFolderPath) = 0;
  NS_IMETHOD SetRootFolderPath(char * aRootFolderPath) = 0;

};

#endif /* __gen_nsIPop3IncomingServer_h__ */
