/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __searchItemImage_h__
#define __searchItemImage_h__

#include "inISearchItem.h"

#include "nsCOMPtr.h"
#include "nsString.h"

class inSearchItemImage : public inISearchItem
{
public:
  inSearchItemImage(nsAutoString* aURL);
  ~inSearchItemImage();

protected:
  nsAutoString mURL;

public:
  NS_DECL_ISUPPORTS

  NS_DECL_INISEARCHITEM
};

////////////////////////////////////////////////////////////////////

// {AA526D81-7523-4fc6-9B6A-22D74CC55AB7}
#define IN_SEARCHITEMIMAGE_CID \
{ 0xaa526d81, 0x7523, 0x4fc6, { 0x9b, 0x6a, 0x22, 0xd7, 0x4c, 0xc5, 0x5a, 0xb7 } }

#define IN_SEARCHITEMIMAGE_CONTRACTID \
"@mozilla.org/inspector/searchitem;1?type=image"

#endif
