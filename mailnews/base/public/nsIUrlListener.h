/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIUrlListener.idl
 */

#ifndef __gen_nsIUrlListener_h__
#define __gen_nsIUrlListener_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */
class nsIURL; /* forward decl */
#include "nsIURL.h"


/* starting interface:    nsIUrlListener */

/* {47618220-D008-11d2-8069-006008128C4E} */
#define NS_IURLLISTENER_IID_STR "47618220-D008-11d2-8069-006008128C4E"
#define NS_IURLLISTENER_IID \
  {0x47618220, 0xD008, 0x11d2, \
    { 0x80, 0x69, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIUrlListener : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IURLLISTENER_IID)

  /* void OnStartRunningUrl (in nsIURL url); */
  NS_IMETHOD OnStartRunningUrl(nsIURL *url) = 0;

  /* void OnStopRunningUrl (in nsIURL url, in nsresult aExitCode); */
  NS_IMETHOD OnStopRunningUrl(nsIURL *url, nsresult aExitCode) = 0;
};

#endif /* __gen_nsIUrlListener_h__ */
