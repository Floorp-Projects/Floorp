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

#include "nsDeviceContextSpecB.h"
//#include "prmem.h"
//#include "plstr.h"

#include "stdlib.h"  // getenv() on Solaris/CC

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecBeOS
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecBeOS :: nsDeviceContextSpecBeOS()
{
  NS_INIT_REFCNT();
	
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecBeOS :: ~nsDeviceContextSpecBeOS()
{


}

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecBeOS, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecBeOS)
NS_IMPL_RELEASE(nsDeviceContextSpecBeOS)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS :: Init(PRBool	aQuiet)
{
  char *path;

  // XXX for now, neutering this per rickg until dcone can play with it
  
  return NS_ERROR_FAILURE;
#if 0
  // XXX these settings should eventually come out of preferences 

  mPrData.toPrinter = PR_TRUE;
  mPrData.fpf = PR_TRUE;
  mPrData.grayscale = PR_FALSE;
  mPrData.size = SizeLetter;
  mPrData.stream = (FILE *) NULL;
  sprintf( mPrData.command, "lpr" );

  // PWD, HOME, or fail 

  if ( ( path = getenv( "PWD" ) ) == (char *) NULL ) 
	if ( ( path = getenv( "HOME" ) ) == (char *) NULL )
  		strcpy( mPrData.path, "netscape.ps" );
  if ( path != (char *) NULL )
	sprintf( mPrData.path, "%s/netscape.ps", path );
  else
	return NS_ERROR_FAILURE;

  ::UnixPrDialog( &mPrData );
  if ( mPrData.cancel == PR_TRUE ) 
	return NS_ERROR_FAILURE;
  else
        return NS_OK;
#endif
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS :: ClosePrintManager()
{
	return NS_OK;
}  
