/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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
#ifndef nsTextEditFactory_h___
#define nsTextEditFactory_h___

#include "nsISupports.h"
#include "nsIFactory.h"

/*
Factory that can make a text editor
*/

/**
 *  This supplies the neccessary entrance to the edit module. it will return any 
 *  instantiations that we need.
 */
class nsTextEditFactory;

extern nsresult GetTextEditFactory(nsIFactory **aFactory, const nsCID & aClass);

class nsTextEditFactory : public nsIFactory {
public:
  ////////////////////////////////////////////////////////////////////////////
  // from nsISupports and AggregatedQueryInterface:

  NS_DECL_ISUPPORTS
  
  ////////////////////////////////////////////////////////////////////////////
  // from nsIFactory:

  NS_IMETHOD
  CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_IMETHOD
  LockFactory(PRBool aLock);


  ////////////////////////////////////////////////////////////////////////////
  // from nsTextEditFactory:

  virtual ~nsTextEditFactory(void);
private:
  nsTextEditFactory(const nsCID &aClass); //will fill the aFactory with the result from queryinterface

  /** GetTextEditFactory
   *  creates an edit factory other CSID supported friend functions here.
   */
  friend nsresult GetTextEditFactory(nsIFactory **, const nsCID & );
  const nsCID &mCID;
};

#endif //nsIEditFactory_h___
