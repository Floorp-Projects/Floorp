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
 *   Vino Fernando Crescini <vino@igelaus.com.au>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 */

#include "nsDeviceContextSpecFactoryX.h"
#include "nsDeviceContextSpecXlib.h"
#include "nsGfxCIID.h"
#include "plstr.h"

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecFactoryXlib, nsIDeviceContextSpecFactory)

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

NS_IMETHODIMP nsDeviceContextSpecFactoryXlib::CreateDeviceContextSpec(nsIWidget *aWidget,
                                                                      nsIDeviceContextSpec *&aNewSpec,
                                                                      PRBool aQuiet)
{
  nsresult rv;
  static NS_DEFINE_CID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
  nsCOMPtr<nsIDeviceContextSpec> devSpec = do_CreateInstance(kDeviceContextSpecCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = ((nsDeviceContextSpecXlib *)devSpec.get())->Init(aQuiet);
    if (NS_SUCCEEDED(rv))
    {
      aNewSpec = devSpec;
      NS_ADDREF(aNewSpec);
    }
  }

  return rv;
}

