/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

// This class is part of the strange printing `architecture'.
// The `CreateDeviceContextSpec' method basically selects a print queue,
//   known here as an `nsIDeviceContextSpec'.  This is given to a method
//   in nsIDeviceContext which creates a fresh device context for that
//   printer.

#include "nsGfxDefs.h"
#include "libprint.h"

#include "nsDeviceContextSpecFactoryO.h"
#include "nsDeviceContextSpecOS2.h"
#include "nsRegionOS2.h"
#include "nsGfxCIID.h"

nsDeviceContextSpecFactoryOS2::nsDeviceContextSpecFactoryOS2()
{
   NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecFactoryOS2, nsIDeviceContextSpecFactory::GetIID())

NS_IMETHODIMP nsDeviceContextSpecFactoryOS2::Init()
{
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecFactoryOS2::CreateDeviceContextSpec(
                                            nsIDeviceContextSpec *aOldSpec,
                                            nsIDeviceContextSpec *&aNewSpec,
                                            PRBool aQuiet)
{
   nsresult rc = NS_ERROR_FAILURE;

   // This currently ignores aOldSpec.  This may be of no consequence...
   PRTQUEUE *pq = PrnSelectPrinter( HWND_DESKTOP, aQuiet ? TRUE : FALSE);

   if( pq)
   {
      nsDeviceContextSpecOS2 *spec = new nsDeviceContextSpecOS2;
      if (!spec)
	return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(spec);
      spec->Init( pq);
      aNewSpec = spec;
      rc = NS_OK;
   }

   return rc;
}
