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
#include "nsPresContext.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

class GalleyContext : public nsPresContext {
public:
  GalleyContext();
  ~GalleyContext();

  virtual PRBool IsPaginated();
  virtual nscoord GetPageWidth();
  virtual nscoord GetPageHeight();
};

GalleyContext::GalleyContext()
{
  nsresult  res;

  static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

  res = NSRepository::CreateInstance(kDeviceContextCID, nsnull,
                                     kDeviceContextIID,
                                     (void **)&mDeviceContext);
  if (NS_OK == res) {
    mDeviceContext->Init(nsnull);
    mDeviceContext->SetDevUnitsToAppUnits(mDeviceContext->GetDevUnitsToTwips());
    mDeviceContext->SetAppUnitsToDevUnits(mDeviceContext->GetTwipsToDevUnits());
    mDeviceContext->SetGamma(1.7f);
  }
}

GalleyContext::~GalleyContext()
{
  mDeviceContext->FlushFontCache();
  NS_IF_RELEASE(mDeviceContext);
}

PRBool GalleyContext::IsPaginated()
{
  return PR_FALSE;
}

nscoord GalleyContext::GetPageWidth()
{
  return 0;
}

nscoord GalleyContext::GetPageHeight()
{
  return 0;
}

NS_LAYOUT nsresult
NS_NewGalleyContext(nsIPresContext** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  GalleyContext *it = new GalleyContext();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIPresContextIID, (void **) aInstancePtrResult);
}
