/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  static const nsIID& GetIID() { static nsIID iid = NS_IHTMLTABLECELLELEMENT_IID; return iid; }
  
  /** @return the starting column for this cell.  Always >= 1 */
  NS_IMETHOD GetColIndex (PRInt32* aColIndex) = 0;

  /** set the starting column for this cell.  Always >= 1 */
  NS_IMETHOD SetColIndex (PRInt32 aColIndex) = 0;

};


#endif
