/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Brian Stell <bstell@netscape.com>
 */

/*
 * Implementation of nsIFontList
 */

#ifndef _nsFontList_H_
#define _nsFontList_H_

#include "nsIFontList.h"
#include "nsComObsolete.h"

#define NS_FONTLIST_CONTRACTID "@mozilla.org/gfx/fontlist;1"


class NS_GFX nsFontList : public nsIFontList
{
public:
  nsFontList();
  virtual ~nsFontList();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIFontList
  NS_DECL_NSIFONTLIST

protected:

};

#endif /* _nsFontList_H_ */
