/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMailboxService.idl
 */

#ifndef __gen_nsIMailboxService_h__
#define __gen_nsIMailboxService_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIUrlListener.h" /* interface nsIUrlListener */
class nsIURL; /* forward decl */
class nsIStreamListener; /* forward decl */
#include "nsFileSpec.h"


/* starting interface:    nsIMailboxService */

/* {EEF82460-CB69-11d2-8065-006008128C4E} */
#define NS_IMAILBOXSERVICE_IID_STR "EEF82460-CB69-11d2-8065-006008128C4E"
#define NS_IMAILBOXSERVICE_IID \
  {0xEEF82460, 0xCB69, 0x11d2, \
    { 0x80, 0x65, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIMailboxService : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMAILBOXSERVICE_IID)

  /* nsIURL ParseMailbox (in nsFileSpec aMailboxPath, in nsIStreamListener aMailboxParser, in nsIUrlListener aUrlListener); */
  NS_IMETHOD ParseMailbox(nsFileSpec & aMailboxPath, nsIStreamListener *aMailboxParser, nsIUrlListener *aUrlListener, nsIURL **_retval) = 0;

  /* nsIURL DisplayMessageNumber (in string url, in unsigned long aMessageNumber, in nsISupports aDisplayConsumer, in nsIUrlListener aUrlListener); */
  NS_IMETHOD DisplayMessageNumber(const char *url, PRUint32 aMessageNumber, nsISupports *aDisplayConsumer, nsIUrlListener *aUrlListener, nsIURL **_retval) = 0;
};

#endif /* __gen_nsIMailboxService_h__ */
