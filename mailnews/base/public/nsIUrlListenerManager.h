/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIUrlListenerManager.idl
 */

#ifndef __gen_nsIUrlListenerManager_h__
#define __gen_nsIUrlListenerManager_h__

#include "nsISupports.h"
#include "nsrootidl.h"
#include "nsIUrlListener.h"
class nsIMsgMailNewsUrl; /* forward decl */
#include "nsIMsgMailNewsUrl.h"

/* starting interface:    nsIUrlListenerManager */

/* {5BCDB610-D00D-11d2-8069-006008128C4E} */
#define NS_IURLLISTENERMANAGER_IID_STR "5BCDB610-D00D-11d2-8069-006008128C4E"
#define NS_IURLLISTENERMANAGER_IID \
  {0x5BCDB610, 0xD00D, 0x11d2, \
    { 0x80, 0x69, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIUrlListenerManager : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IURLLISTENERMANAGER_IID)

  /* void RegisterListener (in nsIUrlListener aUrlListener); */
  NS_IMETHOD RegisterListener(nsIUrlListener *aUrlListener) = 0;

  /* void UnRegisterListener (in nsIUrlListener aUrlListener); */
  NS_IMETHOD UnRegisterListener(nsIUrlListener *aUrlListener) = 0;

  /* void OnStartRunningUrl (in nsIMsgMailNewsUrl aUrl); */
  NS_IMETHOD OnStartRunningUrl(nsIMsgMailNewsUrl *aUrl) = 0;

  /* void OnStopRunningUrl (in nsIMsgMailNewsUrl aUrl, in nsresult aExitCode); */
  NS_IMETHOD OnStopRunningUrl(nsIMsgMailNewsUrl *aUrl, nsresult aExitCode) = 0;
};

#endif /* __gen_nsIUrlListenerManager_h__ */
