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
#include "nscore.h"
#include "nsLayerCollection.h"
#include "nsCoreCIID.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kILayerIID,                 NS_ILAYER_IID);
static NS_DEFINE_IID(kILayerCollectionIID,       NS_ILAYER_COLLECTION_IID);
static NS_DEFINE_IID(kCLayerCollectionCID,       NS_LAYER_COLLECTION_CID);

nsLayerCollection::nsLayerCollection(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mLayers = nsnull;

}

nsresult nsLayerCollection::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kCLayerCollectionCID);                  

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsLayerCollection *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kILayerCollectionIID)) {                                          
    *aInstancePtr = (void*) (nsILayerCollection *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kILayerIID)) {                                          
    *aInstancePtr = (void*) (nsILayer *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this) ;
    AddRef();
    return NS_OK;                                                        
  }                                                                      
  return (NS_NOINTERFACE);
}

NS_IMPL_ADDREF(nsLayerCollection)
NS_IMPL_RELEASE(nsLayerCollection)

nsLayerCollection::~nsLayerCollection()
{
  // XXX: Need to add a way to remove ref when delete all!
  NS_IF_RELEASE(mLayers);
}

nsresult nsLayerCollection::Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);

  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                              nsnull, 
                                              kCVectorCID, 
                                              (void **)&mLayers);

  if (NS_OK != res)
    return res ;

  mLayers->Init();

  return (NS_OK);
}


nsresult nsLayerCollection :: CreateIterator(nsIIterator ** aIterator)
{
  if (mLayers) {
    mLayers->CreateIterator(aIterator);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsLayerCollection :: AddLayer(nsILayer * aLayer)
{
  mLayers->Append(aLayer);
  NS_ADDREF(aLayer);
  return NS_OK;
}

nsresult nsLayerCollection :: RemoveLayer(nsILayer * aLayer)
{
  mLayers->Remove(aLayer);
  NS_IF_RELEASE(aLayer);
  return NS_OK;
}

nsresult nsLayerCollection::SetCurl(const JulianString& s)
{
  return (NS_OK);
}

nsresult nsLayerCollection::GetCurl(JulianString& s)
{
  return (NS_OK);
}


nsresult nsLayerCollection::SetCal(NSCalendar* aCal)
{
  return (NS_OK);
}


nsresult nsLayerCollection::GetCal(NSCalendar*& aCal)
{
  return (NS_OK);
}


nsresult nsLayerCollection::FetchEventsByRange(
                      const DateTime*  aStart, 
                      const DateTime*  aStop,
                      JulianPtrArray*& anArray
                      )
{
  return (NS_OK);
}
