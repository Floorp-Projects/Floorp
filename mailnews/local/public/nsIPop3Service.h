/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIPop3Service.idl
 */

#ifndef __gen_nsIPop3Service_h__
#define __gen_nsIPop3Service_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIPop3IncomingServer.h" /* interface nsIPop3IncomingServer */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIUrlListener.h" /* interface nsIUrlListener */
class nsIURL; /* forward decl */
class nsIStreamListener; /* forward decl */

/* starting interface:    nsIPop3Service */

/* {3BB459E0-D746-11d2-806A-006008128C4E} */
#define NS_IPOP3SERVICE_IID_STR "3BB459E0-D746-11d2-806A-006008128C4E"
#define NS_IPOP3SERVICE_IID \
  {0x3BB459E0, 0xD746, 0x11d2, \
    { 0x80, 0x6A, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIPop3Service : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPOP3SERVICE_IID)

  /* nsIURL GetNewMail (in nsIUrlListener aUrlListener, in nsIPop3IncomingServer popServer); */
  NS_IMETHOD GetNewMail(nsIUrlListener *aUrlListener, nsIPop3IncomingServer *popServer, nsIURL **_retval) = 0;

  /* nsIURL CheckForNewMail (in nsIUrlListener aUrlListener, in nsIPop3IncomingServer popServer); */
  NS_IMETHOD CheckForNewMail(nsIUrlListener *aUrlListener, nsIPop3IncomingServer *popServer, nsIURL **_retval) = 0;
};

#endif /* __gen_nsIPop3Service_h__ */
