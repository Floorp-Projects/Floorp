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

#include "nsDeviceContextSpecG.h"
//#include "prmem.h"
//#include "plstr.h"

#include "stdlib.h"  // getenv() on Solaris/CC

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecGTK
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecGTK :: nsDeviceContextSpecGTK()
{
  NS_INIT_REFCNT();
	
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecGTK :: ~nsDeviceContextSpecGTK()
{
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID);

#if 0
NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecGTK, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecGTK)
NS_IMPL_RELEASE(nsDeviceContextSpecGTK)
#endif

NS_IMETHODIMP nsDeviceContextSpecGTK :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDeviceContextSpecIID))
  {
    nsIDeviceContextSpec* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDeviceContextSpecPSIID))
  {
    nsIDeviceContextSpecPS* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIDeviceContextSpec* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDeviceContextSpecGTK)
NS_IMPL_RELEASE(nsDeviceContextSpecGTK)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecGTK :: Init(PRBool	aQuiet)
{
  char *path;

  // XXX these settings should eventually come out of preferences 

  mPrData.toPrinter = PR_TRUE;
  mPrData.fpf = PR_TRUE;
  mPrData.grayscale = PR_FALSE;
  mPrData.size = NS_LETTER_SIZE;
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
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetToPrinter( PRBool &aToPrinter )     
{
  aToPrinter = mPrData.toPrinter;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetFirstPageFirst ( PRBool &aFpf )      
{
  aFpf = mPrData.fpf;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetGrayscale ( PRBool &aGrayscale )      
{
  aGrayscale = mPrData.grayscale;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetSize ( int &aSize )      
{
  aSize = mPrData.size;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetTopMargin ( float &value )      
{
  value = mPrData.top;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetBottomMargin ( float &value )      
{
  value = mPrData.bottom;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetRightMargin ( float &value )      
{
  value = mPrData.right;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetLeftMargin ( float &value )      
{
  value = mPrData.left;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetCommand ( char **aCommand )      
{
  *aCommand = &mPrData.command[0];
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetPath ( char **aPath )      
{
  *aPath = &mPrData.path[0];
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetUserCancelled( PRBool &aCancel )     
{
  aCancel = mPrData.cancel;
  return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecGTK :: ClosePrintManager()
{
	return NS_OK;
}  
