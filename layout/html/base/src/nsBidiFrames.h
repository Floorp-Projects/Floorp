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

#ifndef nsBidiFrames_h___
#define nsBidiFrames_h___

#include "nsFrame.h"


// IID for the nsDirectionalFrame interface 
// {90D69900-67AF-11d4-BA59-006008CD3717}
#define NS_DIRECTIONAL_FRAME_IID \
{ 0x90d69900, 0x67af, 0x11d4, { 0xba, 0x59, 0x00, 0x60, 0x08, 0xcd, 0x37, 0x17 } }

class nsDirectionalFrame : public nsFrame
{
protected:
  virtual ~nsDirectionalFrame();

public:
  nsDirectionalFrame(PRUnichar aChar);

  void* operator new(size_t aSize) CPP_THROW_NEW;

  static const nsIID& GetIID();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  virtual nsIAtom* GetType() const;

  PRUnichar GetChar(void) const;

private:
  PRUnichar mChar;
};

#endif /* nsBidiFrames_h___ */
#endif /* IBMBIDI */

