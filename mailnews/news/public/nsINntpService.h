/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsINntpService.idl
 */

#ifndef __gen_nsINntpService_h__
#define __gen_nsINntpService_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsINntpIncomingServer.h" /* interface nsINntpIncomingServer */
#include "nsrootidl.h" /* interface nsrootidl */
#include "nsIUrlListener.h" /* interface nsIUrlListener */
class nsIURL; /* forward decl */
class nsIStreamListener; /* forward decl */
class nsISupportsArray; /* forward decl */
#include "nsFileSpec.h"
#include "nsString.h"


/* starting interface:    nsINntpService */

/* {4C9F90E0-E19B-11d2-806E-006008128C4E} */
#define NS_INNTPSERVICE_IID_STR "4C9F90E0-E19B-11d2-806E-006008128C4E"
#define NS_INNTPSERVICE_IID \
  {0x4C9F90E0, 0xE19B, 0x11d2, \
    { 0x80, 0x6E, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsINntpService : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INNTPSERVICE_IID)

  /* nsIURL PostMessage (in nsFilePath pathToFile, in string subject, in string newsgroup, in nsIUrlListener aUrlListener); */
  NS_IMETHOD PostMessage(nsFilePath & pathToFile, const char *subject, const char *newsgroup, nsIUrlListener *aUrlListener, nsIURL **_retval) = 0;

  /* nsIURL RunNewsUrl (in nsString urlString, in nsISupports aConsumer, in nsIUrlListener aUrlListener); */
  NS_IMETHOD RunNewsUrl(nsString & urlString, nsISupports *aConsumer, nsIUrlListener *aUrlListener, nsIURL **_retval) = 0;

  /* nsIURL GetNewNews (in nsINntpIncomingServer nntpServer, in string uri, in nsIUrlListener aUrlListener); */
  NS_IMETHOD GetNewNews(nsINntpIncomingServer *nntpServer, const char *uri, nsIUrlListener *aUrlListener, nsIURL **_retval) = 0;

  /* void CancelMessages (in nsISupportsArray messages, in nsIUrlListener aUrlListener); */
  NS_IMETHOD CancelMessages(nsISupportsArray *messages, nsIUrlListener *aUrlListener) = 0;
};

#endif /* __gen_nsINntpService_h__ */
