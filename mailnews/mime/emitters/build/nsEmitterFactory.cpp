/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "msgCore.h"
#include "nsCOMPtr.h"
#include "pratom.h"

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsICategoryManager.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsMimeEmitterCID.h"
#include "nsIMimeEmitter.h"
#include "nsMimeHtmlEmitter.h"
#include "nsMimeRawEmitter.h"
#include "nsMimeXmlEmitter.h"
#include "nsMimeXULEmitter.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeRawEmitter);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeXmlEmitter);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeXULEmitter);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMimeHtmlDisplayEmitter, Init);


static NS_METHOD RegisterMimeEmitter(nsIComponentManager *aCompMgr, nsIFile *aPath, const char *registryLocation, 
                                       const char *componentType, const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsXPIDLCString previous;
  
  return catman->AddCategoryEntry("mime-emitter", info->mContractID, info->mContractID,
                                  PR_TRUE, PR_FALSE, getter_Copies(previous));
}

static NS_METHOD UnRegisterMimeEmitter(nsIComponentManager *aCompMgr,
                                            nsIFile *aPath,
                                            const char *registryLocation,
                                            const nsModuleComponentInfo *info)
{
  // do we need to unregister our category entries??
  return NS_OK;
}

static nsModuleComponentInfo components[] =
{
  { "HTML MIME Emitter",
    NS_HTML_MIME_EMITTER_CID,
    NS_HTML_MIME_EMITTER_CONTRACTID,
    nsMimeHtmlDisplayEmitterConstructor,
    RegisterMimeEmitter,
    UnRegisterMimeEmitter
  },
  { "XML MIME Emitter",
    NS_XML_MIME_EMITTER_CID,
    NS_XML_MIME_EMITTER_CONTRACTID,
    nsMimeXmlEmitterConstructor,
    RegisterMimeEmitter,
    UnRegisterMimeEmitter
  },
  { "RAW MIME Emitter",
    NS_RAW_MIME_EMITTER_CID,
    NS_RAW_MIME_EMITTER_CONTRACTID,
    nsMimeRawEmitterConstructor,
    RegisterMimeEmitter,
    UnRegisterMimeEmitter
  },
  { "XUL MIME Emitter",
    NS_XUL_MIME_EMITTER_CID,
    NS_XUL_MIME_EMITTER_CONTRACTID,
    nsMimeXULEmitterConstructor,
    RegisterMimeEmitter,
    UnRegisterMimeEmitter
  }
};

NS_IMPL_NSGETMODULE(nsMimeEmitterModule, components)
