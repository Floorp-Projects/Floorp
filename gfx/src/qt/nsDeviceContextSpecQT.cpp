/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsDeviceContextSpecQT.h"
#include "nsRenderingContextQT.h"
#include "stdlib.h"  // getenv() on Solaris/CC
#include <qapplication.h>

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecQT
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecQT::nsDeviceContextSpecQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::nsDeviceContextSpecQT\n"));
    NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecQT
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecQT::~nsDeviceContextSpecQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::~nsDeviceContextSpecQT\n"));
}

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecQT, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecQT)
NS_IMPL_RELEASE(nsDeviceContextSpecQT)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecQT
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecQT::Init(PRBool aQuiet)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::Init\n"));
    char *path;

    // XXX for now, neutering this per rickg until dcone can play with it
  
    return NS_ERROR_FAILURE;

    // XXX these settings should eventually come out of preferences 

    mPrData.toPrinter = PR_TRUE;
    mPrData.fpf = PR_TRUE;
    mPrData.grayscale = PR_FALSE;
//     mPrData.size = SizeLetter;
//     mPrData.stream = (FILE *) NULL;
    sprintf(mPrData.command, "lpr");

    // PWD, HOME, or fail 

    if ((path = getenv("PWD")) == (char *) NULL) 
    {
        if ((path = getenv("HOME")) == (char *) NULL)
        {
            strcpy(mPrData.path, "netscape.ps");
        }
    }
    if (path != (char *) NULL)
    {
        sprintf(mPrData.path, "%s/netscape.ps", path);
    }
    else
    {
        return NS_ERROR_FAILURE;
    }

    //::UnixPrDialog(&mPrData);
    if (mPrData.cancel == PR_TRUE) 
    {
        return NS_ERROR_FAILURE;
    }
    else
    {
        return NS_OK;
    }
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecQT::ClosePrintManager()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::ClosePrintManager\n"));
	return NS_OK;
}  
