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
#include "nsIDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsLayoutAtoms.h"

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

class PrintContext : public nsPresContext {
public:
  PrintContext();
  ~PrintContext();

  NS_IMETHOD GetMedium(nsIAtom*& aMedium);
  virtual PRBool IsPaginated();
  virtual nscoord GetPageWidth();
  virtual nscoord GetPageHeight();
};

PrintContext::PrintContext()
{
}

PrintContext::~PrintContext()
{
}

NS_IMETHODIMP
PrintContext::GetMedium(nsIAtom*& aMedium)
{
  aMedium = nsLayoutAtoms::print;
  NS_ADDREF(aMedium);
  return NS_OK;
}

PRBool PrintContext::IsPaginated()
{
  return PR_TRUE;
}

nscoord PrintContext::GetPageWidth()
{
  // XXX assumes a 1/2 margin around all sides
  return (nscoord) NS_INCHES_TO_TWIPS(7.5);
}

nscoord PrintContext::GetPageHeight()
{
  // XXX assumes a 1/2 margin around all sides
  return (nscoord) NS_INCHES_TO_TWIPS(10);
}

NS_LAYOUT nsresult
NS_NewPrintContext(nsIPresContext** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  PrintContext *it = new PrintContext;

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIPresContextIID, (void **) aInstancePtrResult);
}
