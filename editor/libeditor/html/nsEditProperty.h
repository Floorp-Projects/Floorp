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

public:
  NS_IMETHOD Init(nsIAtom *aPropName, nsIAtom *aValue, PRBool aAppliesToAll);
  NS_IMETHOD GetProperty(nsIAtom **aProperty) const;  
  NS_IMETHOD GetValue(nsIAtom **aValue) const;  
  NS_IMETHOD GetAppliesToAll(PRBool *aAppliesToAll) const; 

  // temporary methods
  static void InstanceInit();
  static void InstanceShutdown();

protected:
  nsCOMPtr<nsIAtom>mProperty;
  nsCOMPtr<nsIAtom>mValue;
  PRBool mAppliesToAll;

  friend nsresult NS_NewEditProperty(nsIEditProperty **aResult);
};

inline nsEditProperty::nsEditProperty() 
{
  NS_INIT_REFCNT();
};

inline nsEditProperty::~nsEditProperty() {};

inline nsresult nsEditProperty::Init(nsIAtom *aPropName, nsIAtom *aValue, PRBool aAppliesToAll)
{
  if (!aPropName)
    return NS_ERROR_NULL_POINTER;

  mProperty = do_QueryInterface(aPropName);
  mValue = do_QueryInterface(aValue);
  mAppliesToAll = aAppliesToAll;
  return NS_OK;
};

inline nsresult nsEditProperty::GetProperty(nsIAtom **aProperty) const
{
  if (!aProperty) {
    return NS_ERROR_NULL_POINTER;
  }
  *aProperty = mProperty;
  if (*aProperty) {
    NS_ADDREF(*aProperty);
  }
  return NS_OK;
};

inline nsresult nsEditProperty::GetValue(nsIAtom **aValue) const
{
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  *aValue = mValue;
  if (*aValue) {
    NS_ADDREF(*aValue);
  }  return NS_OK;
};

inline nsresult nsEditProperty::GetAppliesToAll(PRBool *aAppliesToAll) const
{
  if (!aAppliesToAll) {
    return NS_ERROR_NULL_POINTER;
  }
  *aAppliesToAll = mAppliesToAll;
  return NS_OK;
};



#endif
