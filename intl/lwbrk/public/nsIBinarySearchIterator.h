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
#ifndef nsIBinarySearchIterator_h__
#define nsIBinarySearchIterator_h__


#include "nsISupports.h"
#include "nscore.h"

// {E86B3373-BF89-11d2-B3AF-00805F8A6670}
#define NS_IBINARYSEARCHITERATOR_IID \
{ 0xe86b3373, 0xbf89, 0x11d2, \
    { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };


class nsIBinarySearchIterator : public nsISupports {

public: 

  NS_IMETHOD First() = 0; 
  NS_IMETHOD Next(PRBool aForward) = 0; 
  NS_IMETHOD Current(PRUint32 *oPosition) = 0; 
  NS_IMETHOD IsDone(PRBool *oResult) = 0; 
   
};

#endif  /* nsIBinarySearchIterator_h__ */
