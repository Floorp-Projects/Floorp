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

#ifndef nsCharsetConverterManager_h___
#define nsCharsetConverterManager_h___

#include "nsIFactory.h"

//----------------------------------------------------------------------
// Class nsManagerFactory [declaration]

/**
 * Factory class for the nsICharsetConverterManager objects.
 * 
 * @created         18/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsManagerFactory : public nsIFactory
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsManagerFactory();

  /**
   * Class destructor.
   */
  ~nsManagerFactory();

  //--------------------------------------------------------------------
  // Interface nsIFactory [declaration]

  NS_IMETHOD CreateInstance(nsISupports *aDelegate, const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
};



#endif /* nsCharsetConverterManager_h___ */
