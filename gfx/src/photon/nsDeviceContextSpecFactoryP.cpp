/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDeviceContextSpecFactoryP.h"
#include "nsDeviceContextSpecPh.h"
#include <Pt.h>
#include "nsGfxCIID.h"
#include "plstr.h"

nsDeviceContextSpecFactoryPh :: nsDeviceContextSpecFactoryPh()
{
  NS_INIT_REFCNT();
}

nsDeviceContextSpecFactoryPh :: ~nsDeviceContextSpecFactoryPh()
{
}

static NS_DEFINE_IID(kDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecFactoryPh, kDeviceContextSpecFactoryIID)
NS_IMPL_ADDREF(nsDeviceContextSpecFactoryPh)
NS_IMPL_RELEASE(nsDeviceContextSpecFactoryPh)

NS_IMETHODIMP nsDeviceContextSpecFactoryPh :: Init(void)
{
  return NS_OK;
}

//XXX this method needs to do what the API says...

NS_IMETHODIMP nsDeviceContextSpecFactoryPh :: CreateDeviceContextSpec(nsIDeviceContextSpec *aOldSpec,
                                                                       nsIDeviceContextSpec *&aNewSpec,
                                                                       PRBool aQuiet)
{

  return NS_OK;
}
