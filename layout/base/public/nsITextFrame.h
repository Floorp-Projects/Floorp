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
#ifndef nsITextFrame_h___
#define nsITextFrame_h___

#include "nsISupports.h"

// {2DC80A03-66EF-11d4-BA58-006008CD3717}
#define NS_ITEXTFRAME_IID \
{ 0x2dc80a03, 0x66ef, 0x11d4, { 0xba, 0x58, 0x00, 0x60, 0x08, 0xcd, 0x37, 0x17 } }

class nsITextFrame : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITEXTFRAME_IID)

  NS_IMETHOD SetOffsets(PRInt32 aStart, PRInt32 aEnd) = 0;
};

#endif /* nsITextFrame_h___ */
