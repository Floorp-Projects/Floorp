/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIPop3IncomingServer.idl
 */

#ifndef __gen_nsIImapIncomingServer_h__
#define __gen_nsIImapIncomingServer_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

/* starting interface:    nsIImapIncomingServer */

/* {3d2e7e38-f9d8-11d2-af8f-001083002da8} */
#define NS_IIMAPINCOMINGSERVER_IID_STR "3d2e7e38-f9d8-11d2-af8f-001083002da8"
#define NS_IIMAPINCOMINGSERVER_IID \
  {0x3d2e7e38, 0xf9d8, 0x11d2, \
    { 0xaf, 0x8f, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 }}

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
