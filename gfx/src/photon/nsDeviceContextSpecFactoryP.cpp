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
#include "nsGfxCIID.h"
#include "plstr.h"

#include <Pt.h>
#include "nsPhGfxLog.h"

nsDeviceContextSpecFactoryPh :: nsDeviceContextSpecFactoryPh()
{
  NS_INIT_REFCNT();
}

nsDeviceContextSpecFactoryPh :: ~nsDeviceContextSpecFactoryPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecFactoryPh::~nsDeviceContextSpecFactoryPh Destructor called\n"));
}

static NS_DEFINE_IID(kDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecFactoryPh, kDeviceContextSpecFactoryIID)
NS_IMPL_ADDREF(nsDeviceContextSpecFactoryPh)
NS_IMPL_RELEASE(nsDeviceContextSpecFactoryPh)

NS_IMETHODIMP nsDeviceContextSpecFactoryPh :: Init(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecFactoryPh::Init - Not Implemented\n"));
  return NS_OK;
}

//XXX this method needs to do what the API says...

NS_IMETHODIMP nsDeviceContextSpecFactoryPh :: CreateDeviceContextSpec(nsIDeviceContextSpec *aOldSpec,
                                                                       nsIDeviceContextSpec *&aNewSpec,
                                                                       PRBool aQuiet)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecFactoryPh::CreateDeviceContextSpec aQuiet=<%d> aOldSpec=<%p>\n", aQuiet, aOldSpec));

  nsresult  			  rv = NS_ERROR_FAILURE;
  PpPrintContext_t 		  *pc = nsnull;
  int                     action;
  
	/* Create a Printer Context */
	pc = PpPrintCreatePC();

    /* copy over user selections from prOps to PrintContext */

	/* REVISIT: Need to find my parent widget */
    action = PtPrintSelection(NULL, NULL, NULL, pc, (Pt_PRINTSEL_DFLT_LOOK));
    if (action == Pt_PRINTSEL_PRINT)
	{
	  nsIDeviceContextSpec    *devSpec = nsnull;
	  nsComponentManager::CreateInstance(kDeviceContextSpecCID, nsnull, kIDeviceContextSpecIID, (void **)&devSpec);

		if (nsnull != devSpec)
		{
		  /* Pass ownership of the "pc" to the nsDeviceContextSpecPh */
		  if (NS_OK == ((nsDeviceContextSpecPh *)devSpec)->Init(aQuiet, pc))
		  {
		    aNewSpec = devSpec;
			rv = NS_OK;
		  }
		}
	}
	else if (action == Pt_PRINTSEL_PREVIEW)
	{
		/* REVISIT: Somehow trigger a Print Preview? */	
	}

	return rv;
}
