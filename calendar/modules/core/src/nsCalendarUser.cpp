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

#include <stdio.h>
#include "nsCalendarUser.h"
#include "nsCoreCIID.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kICalendarUserIID, NS_ICALENDAR_USER_IID);
static NS_DEFINE_IID(kIUserIID,         NS_IUSER_IID);
static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCUserCID,         NS_USER_CID);
static NS_DEFINE_IID(kILayerIID,        NS_ILAYER_IID);
static NS_DEFINE_IID(kCLayerCollectionCID,  NS_LAYER_COLLECTION_CID);

nsCalendarUser::nsCalendarUser(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mpUserSupports  = nsnull;
  mpUser          = nsnull;
  mpLayer         = nsnull;
}

nsresult nsCalendarUser::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kClassIID, kICalendarUserIID);                         

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  if (nsnull != mpUserSupports)
    return mpUserSupports->QueryInterface(aIID, aInstancePtr);

  return (NS_NOINTERFACE);

}


NS_IMPL_ADDREF(nsCalendarUser)
NS_IMPL_RELEASE(nsCalendarUser)

nsCalendarUser::~nsCalendarUser()
{
  NS_IF_RELEASE(mpUserSupports); 
  NS_IF_RELEASE(mpLayer);
  mpUser = nsnull; // Do Not Release
}

nsresult nsCalendarUser::Init()
{
  nsresult res;

  /*
   * Aggregate in the XPFC User Implementation
   */
  nsISupports * supports ;
  res = QueryInterface(kISupportsIID, (void **) &supports);
  if (NS_OK != res)
    return res;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  res = nsRepository::CreateInstance(kCUserCID, 
                                     supports, 
                                     kISupportsIID, 
                                     (void**)&mpUserSupports);

  if (NS_OK == res) 
  {
    res = mpUserSupports->QueryInterface(kIUserIID, (void**)&mpUser);
    if (NS_OK != res) 
    {
	    mpUserSupports->Release();
	    mpUserSupports = NULL;
    }
    else 
    {
      mpUser->Release();
    }
  }

  /*
   * Create a LayerCollection for this User, by default
   */
  res = nsRepository::CreateInstance(
          kCLayerCollectionCID,  // class id that we want to create
          nsnull,                // not aggregating anything  (this is the aggregatable interface)
          kILayerIID,            // interface id of the object we want to get back
          (void**)&mpLayer);     // pointer to the interface object

  if (NS_OK == res) 
    mpLayer->Init();

  NS_RELEASE(supports);

  return res;
}


NS_IMETHODIMP nsCalendarUser :: GetLayer(nsILayer *& aLayer)
{
  aLayer = mpLayer;
  NS_ADDREF(mpLayer);
  return NS_OK;
}

NS_IMETHODIMP nsCalendarUser :: SetLayer(nsILayer* aLayer)
{
  NS_IF_RELEASE(mpLayer);
  mpLayer = aLayer;
  NS_ADDREF(mpLayer);
  return NS_OK;
}

NS_IMETHODIMP nsCalendarUser::HasLayer(const JulianString& aCurl, PRBool& aContains)
{
  return  mpLayer->URLMatch(aCurl,aContains);
}
