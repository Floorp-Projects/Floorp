/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * IBM Corporation.  Portions created by IBM are
 * Copyright (C) 2000 IBM Corporation.
 * All Rights Reserved.
 *
 * Copyright (C) 2000, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * Contributor(s):
 */

#ifdef IBMBIDI

#include "nsBidiFrames.h"
#include "nsLayoutAtoms.h"


nsDirectionalFrame::nsDirectionalFrame(PRUnichar aChar)
  : mChar(aChar)
{
}

nsDirectionalFrame::~nsDirectionalFrame()
{
}

PRUnichar
nsDirectionalFrame::GetChar(void) const
{
  return mChar;
}

/**
 * Get the "type" of the frame
 *
 * @see nsLayoutAtoms::directionalFrame
 */
nsIAtom*
nsDirectionalFrame::GetType() const
{ 
  return nsLayoutAtoms::directionalFrame; 
}
  
const nsIID&
nsDirectionalFrame::GetIID()
{
  static nsIID iid = NS_DIRECTIONAL_FRAME_IID;
  return iid;
}

NS_IMETHODIMP
nsDirectionalFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (!aInstancePtr) {
    rv = NS_ERROR_NULL_POINTER;
  }
  else if (aIID.Equals(NS_GET_IID(nsDirectionalFrame) ) ) {
    *aInstancePtr = (void*) this;
    rv = NS_OK;
  }
  return rv;
}

void*
nsDirectionalFrame::operator new(size_t aSize) CPP_THROW_NEW
{
  void* frame = ::operator new(aSize);
  if (frame) {
    memset(frame, 0, aSize);
  }
  return frame;
}

nsresult
NS_NewDirectionalFrame(nsIFrame** aNewFrame, PRUnichar aChar)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDirectionalFrame* frame = new nsDirectionalFrame(aChar);
  *aNewFrame = frame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

#endif /* IBMBIDI */
