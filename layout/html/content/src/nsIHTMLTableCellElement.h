/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef _nsIHTMLTableCellElement__h_
#define _nsIHTMLTableCellElement__h_


#include "nsISupports.h"

// XXX ask Peter if this can go away
#define NS_IHTMLTABLECELLELEMENT_IID \
{ 0x243CA090,  0x4914, 0x11d2, \
 { 0x8F, 0x3F, 0x00, 0x60, 0x08, 0x15, 0x9B, 0x0C } } 


class nsIHTMLTableCellElement : public nsISupports
{
public:
  
  /** @return the starting column for this cell.  Always >= 1 */
  NS_IMETHOD GetColIndex (PRInt32* aColIndex) = 0;

  /** set the starting column for this cell.  Always >= 1 */
  NS_IMETHOD SetColIndex (PRInt32 aColIndex) = 0;

};


#endif
