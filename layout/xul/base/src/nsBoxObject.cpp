/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsBoxObject.h"
#include "nsIBoxLayoutManager.h"
#include "nsIBoxPaintManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

// Static IIDs/CIDs. Try to minimize these.
// None so far.

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsBoxObject, nsIBoxObject, nsPIBoxObject)

// Constructors/Destructors
nsBoxObject::nsBoxObject(void)
{
  NS_INIT_ISUPPORTS();
}

nsBoxObject::~nsBoxObject(void)
{
}

NS_IMETHODIMP
nsBoxObject::GetLayoutManager(nsIBoxLayoutManager** aResult)
{
  *aResult = mLayoutManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetLayoutManager(nsIBoxLayoutManager* aLayoutManager)
{
  mLayoutManager = aLayoutManager;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPaintManager(nsIBoxPaintManager** aResult)
{
  *aResult = mPaintManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetPaintManager(nsIBoxPaintManager* aPaintManager)
{
  mPaintManager = aPaintManager;
  return NS_OK;
}

// nsPIBoxObject //////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsBoxObject::Init(nsIContent* aContent, nsIPresShell* aShell)
{
  mContent = aContent;
  mPresShell = aShell;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetDocument(nsIDocument* aDocument)
{
  if (aDocument) {
    nsCOMPtr<nsIPresShell> shell = getter_AddRefs(aDocument->GetShellAt(0));
    mPresShell = shell;
  }
  else {
    mPresShell = nsnull;
  }
  return NS_OK;
}

nsIFrame*
nsBoxObject::GetFrame()
{
  nsIFrame* frame = nsnull;
  if (mPresShell)
    mPresShell->GetPrimaryFrameFor(mContent, &frame);
  return frame;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
