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
}

GalleyContext::~GalleyContext()
{
  mDeviceContext->FlushFontCache();
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
