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
#include "jdefines.h"
#include "ptrarray.h"
#include "julnstr.h"
#include "nsLayerCollection.h"
#include "nsCurlParser.h"
#include "nsCoreCIID.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kILayerIID,              NS_ILAYER_IID);
static NS_DEFINE_IID(kILayerCollectionIID,    NS_ILAYER_COLLECTION_IID);
static NS_DEFINE_IID(kCLayerCollectionCID,    NS_LAYER_COLLECTION_CID);
static NS_DEFINE_IID(kCLayerCID,              NS_LAYER_CID);

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
  nsILayer * pLayer;

  while (mLayers->Count() > 0 )
  {
    pLayer = (nsILayer*)mLayers->ElementAt(0);
    RemoveLayer(pLayer);
    mLayers->RemoveAt(0);
  }

  NS_IF_RELEASE(mLayers);
}

nsresult nsLayerCollection::Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

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

/**
 * Remove any existing layers, and create a layer for each
 * of the supplied urls.
 * @param sCurl  A string of space-delimited calendar urls 
 *               that form the layer list.
 * @return NS_OK on success
 */
nsresult nsLayerCollection::SetCurl(const JulianString& sC)
{
  nsILayer* pLayer;
  nsresult res;
  JulianString sCurl = sC;

  /*
   * Delete all layers we currently hold...
   */
  while (mLayers->Count() > 0 )
  {
    pLayer = (nsILayer*)mLayers->ElementAt(0);
    RemoveLayer(pLayer);
    mLayers->RemoveAt(0);
  }

  /*
   * Now build up the layers based on what we have...
   */
  JulianString s;
  int32 i,j;
  for (i = 0; i >= 0 && i < (PRInt32)sCurl.GetStrlen(); i = j + 1 )
  {
    while (' ' == sCurl.GetBuffer()[i] && i < (PRInt32)sCurl.GetStrlen())
      ++i;
    if (i >= (PRInt32)sCurl.GetStrlen())
      break;
    /*
     * XXX:
     * this is a complete hack.  I want to use the commented out 
     * lines below. But if I
     * do we get library link errors. Maybe it will go away when we make
     * the big changeover to nsString.
     */
    
#if 0
     if (-1 == (j = sCurl.Strpbrk(i," \t")))
       j = sCurl.GetStrlen()-1;
     s = sCurl(i,j-i+1);
#endif

#if 1
    {
      /*
       * XXX: REMOVE this code when replacing the string.
       */
    char sBuf[1024];
    int k;
    char c;
    for ( k = 0, j = i;
          (' ' != (c = sCurl.GetBuffer()[j])) && (j < (PRInt32)sCurl.GetStrlen());
          j++, k++)
          {
            sBuf[k] = c;
          }
    sBuf[k] = 0;
    s = sBuf;
    }
#endif

    /*
     * create a new layer for this curl
     */
    if (NS_OK != (res = nsRepository::CreateInstance(
            kCLayerCID,         // class id that we want to create
            nsnull,             // not aggregating anything  (this is the aggregatable interface)
            kILayerIID,         // interface id of the object we want to get back
            (void**)&pLayer)))
      return 1;  // XXX fix this
    pLayer->Init();
    pLayer->SetCurl(s);
    AddLayer(pLayer);
    NS_RELEASE(pLayer);
  }
  return (NS_OK);
}

/**
 * Build a string containing all the layers in this collection.
 * @param s the returned string
 * @return NS_OK on success.
 */
nsresult nsLayerCollection::GetCurl(JulianString& s)
{
  PRInt32 i;
  PRInt32 iSize = mLayers->Count();
  JulianString sTmp;
  nsILayer* pLayer;
  nsresult res;

  s = "";
  for ( i = 0; i < (PRInt32)mLayers->Count(); i++)
  {
    pLayer = (nsILayer*)mLayers->ElementAt(i);
    if ( NS_OK != (res = pLayer->GetCurl(sTmp)))
      return res;
    if (i > 0)
      s += " ";
    s += sTmp;
  }
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


/**
 * @param aStart Starting time for fetch
 * @param aStart ending time for fetch
 * @param anArray Match a returned boolean value. If a match is found this value
 *               is returned as PR_TRUE. Otherwise, the it is set to PR_FALSE.
 *
 * @return NS_OK on success
 */
nsresult nsLayerCollection::FetchEventsByRange(
                      DateTime*  aStart, 
                      DateTime*  aStop,
                      JulianPtrArray*  anArray
                      )
{
  PRInt32 i,j;
  PRInt32 iSize = mLayers->Count();
  PRInt32 iTmpSize;
  nsILayer *pLayer;
  JulianPtrArray TmpArray;

  /*
   *  XXX:
   *  This should be made multi-threaded...
   */
  for ( i = 0; i < iSize; i++)
  {
    pLayer = (nsILayer*)mLayers->ElementAt(i);
    if (NS_OK == (pLayer->FetchEventsByRange(aStart,aStop,&TmpArray)))
    {
      /*
       * copy the stuff in TmpArray into anArray...
       */
      for (j = 0, iTmpSize = TmpArray.GetSize(); j < iTmpSize; j++)
      {
        /*
         *  XXX:  should sort these chronologically
         */
        anArray->Add( TmpArray.GetAt(j) );
      }

      TmpArray.RemoveAll();
    }
  }
  return NS_OK;
}

#if 0
nsresult nsLayerCollection::FetchEventsByRange(
                      DateTime*  aStart, 
                      DateTime*  aStop,
                      JulianPtrArray*  anArray
                      )
{
  PRLock *pLock;
  PRCondVar *pSomeThreadsCompleted;
  nsArray PendingThreadCompletions;
  PRInt32 i;
  PRInt32 iTmpSize;
  PRInt32 iFinishedThreadCount = 0;
  PRInt32 iCount = mLayers->Count();
  nsILayer *pLayer = 0;
  PRThread *pThread = 0;

  pLock = PR_NewLock();
  pAThreadCompleted = PR_NewCondVar(pLock);

  /*
   * Create a thread for each layer in the collection...
   */
  for ( i = 0; i < iCount; i++)
  {
    pLayer = (nsILayer*)mLayers->ElementAt(i);
    /*
     * create a thread for pLayer..
     */
	pThread = PR_CreateThread(
				PR_USER_THREAD,
                main_LayerOpHandler,
                pLayerOp,
                PR_PRIORITY_NORMAL,
                PR_LOCAL_THREAD,
                PR_UNJOINABLE_THREAD,
                0);
  }

  while (iFinishedThreadCount < iCount)
  {
    /*
     * Wait for individual layers to complete...
     */
    PR_Lock(pLock);
    while ( 0 == PendingThreadCompletions.Count() )
    {
      PR_WaitCondVar(pSomeThreadsCompleted,PR_INTERVAL_NO_TIMEOUT);
    }

    /*
     * At least one layer is now complete. Remove any threads from
     * the pending list and put them into the completed list
     */
    iFinishedThreadCount += PendingThreadCompletions.Count();
    PendingThreadCompletions.RemoveAll();
    PR_Unlock(pLock);
  }

  /*
   * All threads are done. Send command indicating data collection is done.
   */


  return NS_OK;
}

#endif

/**
 * @param aUrl   the url for comparison. In this case, we check to see if 
 *               both the host and cal store id match on any layer in
 *               the list.
 * @param aMatch a returned boolean value. If a match is found this value
 *               is returned as PR_TRUE. Otherwise, the it is set to PR_FALSE.
 *
 * @return NS_OK on success
 */
nsresult nsLayerCollection::URLMatch(const JulianString& aUrl, PRBool& aMatch)
{
  PRInt32 i;
  PRInt32 iSize = mLayers->Count();
  nsILayer *pLayer;
  aMatch = PR_FALSE;
  for ( i = 0; i < iSize; i++)
  {
    pLayer = (nsILayer*)mLayers->ElementAt(i);
    if (NS_OK == (pLayer->URLMatch(aUrl,aMatch)))
    {
      if (PR_TRUE == aMatch)
        return NS_OK;
    }
  }
  return NS_OK;
}

/**
 * @param aShell the shell object 
 *
 * @return NS_OK on success
 */
nsresult nsLayerCollection::SetShell(nsCalendarShell* aShell)
{
  mpShell = aShell;
  PRInt32 i;
  PRInt32 iSize = mLayers->Count();
  nsILayer *pLayer;
  for ( i = 0; i < iSize; i++)
  {
    pLayer = (nsILayer*)mLayers->ElementAt(i);
    pLayer->SetShell(aShell);
  }
  return NS_OK;
}

/**
 * @param aStart Starting time for fetch
 * @param aStart ending time for fetch
 * @param anArray Match a returned boolean value. If a match is found this value
 *               is returned as PR_TRUE. Otherwise, the it is set to PR_FALSE.
 *
 * @return NS_OK on success
 */
nsresult nsLayerCollection::StoreEvent(VEvent& addEvent)
{
  PRInt32 i,j;
  PRInt32 iSize = mLayers->Count();
  PRInt32 iTmpSize;
  nsILayer *pLayer;
  JulianPtrArray TmpArray;

  /*
   *  XXX:
   *  This should be made multi-threaded...
   */
  for ( i = 0; i < iSize; i++)
  {
    pLayer = (nsILayer*)mLayers->ElementAt(i);
    if (NS_OK == (pLayer->StoreEvent(addEvent)))
    {
      break;
    }
  }
  return NS_OK;
}
