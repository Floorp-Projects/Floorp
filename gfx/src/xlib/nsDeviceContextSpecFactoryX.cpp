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
 *    Vino Fernando Crescini <vino@igelaus.com.au>
 */

#include "nsDeviceContextSpecFactoryX.h"
#include "nsDeviceContextSpecXlib.h"
#include "nsGfxCIID.h"
#include "plstr.h"

static NS_DEFINE_IID(kDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecFactoryXlib, kDeviceContextSpecFactoryIID)
NS_IMPL_ADDREF(nsDeviceContextSpecFactoryXlib)
NS_IMPL_RELEASE(nsDeviceContextSpecFactoryXlib)

nsDeviceContextSpecFactoryXlib::nsDeviceContextSpecFactoryXlib() 
{
  NS_INIT_REFCNT();
}

nsDeviceContextSpecFactoryXlib::~nsDeviceContextSpecFactoryXlib() 
{
}

NS_IMETHODIMP nsDeviceContextSpecFactoryXlib::Init(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecFactoryXlib::CreateDeviceContextSpec(nsIDeviceContextSpec *aOldSpec,
                                                                      nsIDeviceContextSpec *&aNewSpec,
                                                                      PRBool aQuiet)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIDeviceContextSpec *devSpec = nsnull;

	nsComponentManager::CreateInstance(kDeviceContextSpecCID, nsnull, kIDeviceContextSpecIID, (void **)&devSpec);

	if (nsnull != devSpec) {
	  if (NS_OK == ((nsDeviceContextSpecXlib *)devSpec)->Init(aQuiet)) {
	    aNewSpec = devSpec;
	    rv = NS_OK;
	  }
	}
	return rv;
}

