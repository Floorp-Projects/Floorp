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

#include "nsDeviceContextSpecPh.h"
#include "prmem.h"
#include "plstr.h"
#include "nsPhGfxLog.h"

nsDeviceContextSpecPh :: nsDeviceContextSpecPh()
{
  mDriverName = nsnull;
  mDeviceName = nsnull;
}

nsDeviceContextSpecPh :: ~nsDeviceContextSpecPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecPh::~nsDeviceContextSpecPh Destructor Called\n"));
}

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecPh, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecPh)
NS_IMPL_RELEASE(nsDeviceContextSpecPh)

NS_IMETHODIMP nsDeviceContextSpecPh :: Init(char *aDriverName, char *aDeviceName )
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecPh::Init - Not Implemented\n"));

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecPh :: GetDriverName(char *&aDriverName) const
{
 PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecPh::GetDriverName - Not Implemented\n"));

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecPh :: GetDeviceName(char *&aDeviceName) const
{
 PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsDeviceContextSpecPh::GetDeviceName - Not Implemented\n"));

  return NS_OK;
}

