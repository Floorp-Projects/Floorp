/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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

#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecodeHelper.h"
#include "nsUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsUnicodeEncodeHelper.h"
#include "nsIPlatformCharset.h"
#include "nsPlatformCharsetFactory.h"
#include "nsICharsetAlias.h"
#include "nsCharsetAliasFactory.h"
#include "nsITextToSubURI.h"
#include "nsTextToSubURI.h"
#include "nsIServiceManager.h"
#include "nsCharsetMenu.h"
#include "rdf.h"
#include "nsUConvDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kCharsetMenuCID, NS_CHARSETMENU_CID);
static NS_DEFINE_CID(kTextToSubURICID, NS_TEXTTOSUBURI_CID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

//----------------------------------------------------------------------
// Global functions and data [implementation]

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr)
{
  return PR_FALSE;
}

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) 
      return NS_ERROR_NULL_POINTER;

  *aFactory = NULL; 
  // the converter manager
  if (aClass.Equals(kCharsetConverterManagerCID)) {
    nsManagerFactory *factory = new nsManagerFactory();

    if(nsnull == factory)
       return NS_ERROR_OUT_OF_MEMORY;

    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  // the Unicode Decode helper
  if (aClass.Equals(kUnicodeDecodeHelperCID)) {
    nsDecodeHelperFactory *factory = new nsDecodeHelperFactory();
    if(nsnull == factory)
       return NS_ERROR_OUT_OF_MEMORY;
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  // the Unicode Encode helper
  if (aClass.Equals(kUnicodeEncodeHelperCID)) {
    nsEncodeHelperFactory *factory = new nsEncodeHelperFactory();
    if(nsnull == factory)
       return NS_ERROR_OUT_OF_MEMORY;
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  if (aClass.Equals(kPlatformCharsetCID)) {
    nsIFactory *factory = NEW_PLATFORMCHARSETFACTORY();
	nsresult res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  if (aClass.Equals(kCharsetAliasCID)) {
    nsIFactory *factory = NEW_CHARSETALIASFACTORY();
	nsresult res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  if (aClass.Equals(kCharsetMenuCID)) {
    nsIFactory *factory = new nsCharsetMenuFactory();
	nsresult res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  if (aClass.Equals(kTextToSubURICID)) {
    nsIFactory *factory = NEW_TEXTTOSUBURI_FACTORY();
	nsresult res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char * path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kCharsetMenuCID, 
      NS_CHARSETMENU_PID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CHARSETMENU_PID, 
      path, PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kUnicodeDecodeHelperCID, 
      "Unicode Decode Helper",
      NS_UNICODEDECODEHELPER_CONTRACTID, 
      path, PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kUnicodeEncodeHelperCID, 
      "Unicode Encode Helper",
      NS_UNICODEENCODEHELPER_CONTRACTID, 
      path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kCharsetAliasCID, 
      "Charset Alias Information", 
      NS_CHARSETALIAS_CONTRACTID, 
      path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kTextToSubURICID, 
      "Text To Sub URI Helper", 
      NS_ITEXTTOSUBURI_CONTRACTID, 
      path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kCharsetConverterManagerCID, 
      "Charset Conversion Manager", 
      NS_CHARSETCONVERTERMANAGER_CONTRACTID,
      path, PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kPlatformCharsetCID, 
      "Platform Charset Information", 
      NS_PLATFORMCHARSET_CONTRACTID, 
      path, 
      PR_TRUE, PR_TRUE);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char * path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kUnicodeDecodeHelperCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kUnicodeEncodeHelperCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kCharsetAliasCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kTextToSubURICID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kCharsetConverterManagerCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kPlatformCharsetCID, path);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
