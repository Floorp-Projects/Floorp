/* 
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
 *
 * Contributors:
 *   Joe Hewitt <hewitt@netscape.com>
 */

#ifndef __inBitmapURI_h__
#define __inBitmapURI_h__

#include "inIBitmapURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class inBitmapURI : public inIBitmapURI
{
public:    
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_INIBITMAPURI

  inBitmapURI();
  virtual ~inBitmapURI();

protected:
  nsCString mBitmapName;

  nsresult FormatSpec(char** result);
};

#endif // __inBitmapURI_h__
