/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#define NS_IMPL_IDS
#include "pratom.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
//#include "nsIObserver.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsPICSElementObserver.h"
#include "nsString.h"
#include "nsIPICS.h"
#include "nspics.h"
#include "nsIWebShellServices.h"
#include "plstr.h"
#include "prenv.h"

//static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
//static NS_DEFINE_IID(kObserverCID, NS_OBSERVER_CID);
static NS_DEFINE_IID(kIPICSElementObserverIID, NS_IPICSELEMENTOBSERVER_IID);
static NS_DEFINE_IID(kIElementObserverIID,     NS_IELEMENTOBSERVER_IID);
static NS_DEFINE_IID(kIObserverIID,            NS_IOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIPICSIID,                NS_IPICS_IID);
static NS_DEFINE_IID(kPICSCID,                 NS_PICS_CID);




////////////////////////////////////////////////////////////////////////////////
// nsPICSElementObserver Implementation


NS_IMPL_ADDREF(nsPICSElementObserver)                       \
NS_IMPL_RELEASE(nsPICSElementObserver)


NS_INTERFACE_MAP_BEGIN(nsPICSElementObserver)
		/*
			Slight problem here: there is no |class nsIPICSElementObserver|,
			so, this is a slightly un-orthodox entry, which will have to be
			fixed before we could switch over to the table-driven mechanism.
		 */
	if ( aIID.Equals(kIPICSElementObserverIID) )
	  foundInterface = NS_STATIC_CAST(nsIElementObserver*, this);
	else

	NS_INTERFACE_MAP_ENTRY(nsIElementObserver)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIElementObserver)
NS_INTERFACE_MAP_END


NS_PICS nsresult NS_NewPICSElementObserver(nsIObserver** anObserver)
{
    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    } 
    
    nsPICSElementObserver* it = new nsPICSElementObserver();

    if (it == 0) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return it->QueryInterface(kIPICSElementObserverIID, (void **) anObserver);
}

nsPICSElementObserver::nsPICSElementObserver()
{
    NS_INIT_REFCNT();
}

nsPICSElementObserver::~nsPICSElementObserver(void)
{

}

const char* nsPICSElementObserver::GetTagNameAt(PRUint32 aTagIndex)
{
  if (aTagIndex == 0) {
    return "META";
  } if (aTagIndex == 1) {
    return "BODY";
  }else {
    return nsnull;
  }
}

NS_IMETHODIMP nsPICSElementObserver::Notify(PRUint32 aDocumentID, eHTMLTags aTag, 
                    PRUint32 numOfAttributes, const PRUnichar* nameArray[], 
                    const PRUnichar* valueArray[]) 
{
  if(aTag == eHTMLTag_meta) {
      return Notify(aDocumentID, numOfAttributes, nameArray, valueArray);
  }
  return NS_OK;
}
NS_IMETHODIMP nsPICSElementObserver::Notify(PRUint32 aDocumentID, const PRUnichar* aTag, 
                    PRUint32 numOfAttributes, const PRUnichar* nameArray[], 
                    const PRUnichar* valueArray[]) 
{
  return Notify(aDocumentID, numOfAttributes, nameArray, valueArray);
}
NS_IMETHODIMP nsPICSElementObserver::Notify(PRUint32 aDocumentID,  
                    PRUint32 numOfAttributes, const PRUnichar* nameArray[], 
                    const PRUnichar* valueArray[]) 
{
  nsresult rv;
  int status;
  nsIWebShellServices* ws;
//  nsString theURL(aSpec);
// char* url = aSpec.ToNewCString();
  nsIURI* uaURL = nsnull;
//  rv = NS_NewURL(&uaURL, nsString(aSpec));
 
    if(numOfAttributes >= 2) {
      const nsString& theValue1=valueArray[0];
      char *val1 = theValue1.ToNewCString();
      if(theValue1.EqualsIgnoreCase("\"PICS-LABEL\"")) {
#ifdef DEBUG
        printf("\nReceived notification for a PICS-LABEl\n");
#endif
        const nsString& theValue2=valueArray[1];
        char *label = theValue2.ToNewCString();
        if (valueArray[numOfAttributes]) {
          const nsString& theURLValue=valueArray[numOfAttributes];
          nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &rv));
          if (NS_FAILED(rv)) return rv;

          nsIURI *uri = nsnull;
          const char *uriStr = theURLValue.GetBuffer();
          rv = service->NewURI(uriStr, nsnull, &uri);
          if (NS_FAILED(rv)) return rv;

          rv = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&uaURL);
          NS_RELEASE(uri);
          if (NS_FAILED(rv)) return rv;
        }
        nsIPICS *pics = NULL;
        rv = nsRepository::CreateInstance(kPICSCID,
								        NULL,
								        kIPICSIID,
								        (void **) &pics);
        if(rv == NS_OK) {
          pics->GetWebShell(aDocumentID, ws);
          if(ws) {
            status = pics->ProcessPICSLabel(label);
            if(uaURL)
              pics->SetNotified(ws, uaURL, PR_TRUE);

            if(status) {
              if(ws) {
                char * text = PR_GetEnv("NGLAYOUT_HOME");
                nsString mtemplateURL(text ? text : "resource:/res/samples/picstest1.html");
                //  ws->LoadURL(mtemplateURL, nsnull, nsnull);
                nsCharsetSource s;
                ws->SetRendering(PR_TRUE);
                ws->StopDocumentLoad();
                ws->LoadDocument("resource:/res/samples/picstest1.html", nsnull, s);
              }
            }
          }
        } 
      }
    }
  return NS_OK;
    
}

NS_IMETHODIMP nsPICSElementObserver::Observe(nsISupports*, const PRUnichar*, const PRUnichar*) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////////////
// nsPICSElementObserverFactory Implementation

NS_IMPL_ISUPPORTS1(nsPICSElementObserverFactory, nsIFactory)

nsPICSElementObserverFactory::nsPICSElementObserverFactory(void)
{
    NS_INIT_REFCNT();
}

nsPICSElementObserverFactory::~nsPICSElementObserverFactory(void)
{

}

nsresult
nsPICSElementObserverFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;
    
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    nsIObserver* inst = nsnull;

    if (NS_FAILED(rv = NS_NewPICSElementObserver(&inst)))
        return rv;

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(rv)) {
        *aResult = NULL;
    }
    return rv;
}

nsresult
nsPICSElementObserverFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
