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

#ifndef __nsEditProperty_h__
#define __nsEditProperty_h__

#include "nsIEditProperty.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"

/** simple class for describing a single property as it relates to a range of content.
  * not ref counted.
  *
  */ 
class nsEditProperty : public nsIEditProperty
{
public:
  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

protected:
  nsEditProperty ();
  virtual ~nsEditProperty();

  friend nsresult NS_NewEditProperty(nsIEditProperty **aResult);
};


#endif
