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

#include "nscore.h"
#include "nsCalDayListCommand.h"
#include "nsCalUICIID.h"
#include "nsCalUtilCIID.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);
static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);

nsCalDayListCommand :: nsCalDayListCommand()
{
  NS_INIT_REFCNT();
  mDateTimes = nsnull;
}

nsCalDayListCommand :: ~nsCalDayListCommand()  
{
  NS_IF_RELEASE(mDateTimes);
}

NS_IMPL_ADDREF(nsCalDayListCommand)
NS_IMPL_RELEASE(nsCalDayListCommand)

nsresult nsCalDayListCommand::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kXPFCCommandIID);                         
  static NS_DEFINE_IID(kCalDayListCommandCID, NS_CAL_DAYLIST_COMMAND_CID);                         

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
  if (aIID.Equals(kCalDayListCommandCID)) {                                      
    *aInstancePtr = (void*)(nsCalDayListCommand *) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (NS_NOINTERFACE);

}

nsresult nsCalDayListCommand::Init()
{
  nsresult res ;


  res = nsRepository::CreateInstance(kCVectorCID, 
                                     nsnull, 
                                     kCVectorCID, 
                                     (void **)&mDateTimes);

  if (NS_OK != res)
    return res ;

  mDateTimes->Init();

  return res ;
}

nsresult nsCalDayListCommand :: CreateIterator(nsIIterator ** aIterator)
{
  if (mDateTimes) {
    mDateTimes->CreateIterator(aIterator);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsCalDayListCommand :: AddDateTime(nsIDateTime * aDateTime)
{
  mDateTimes->Append(aDateTime);

  return NS_OK;
}

nsresult nsCalDayListCommand :: AddDateVector(nsIArray * aDateVector)
{
  NS_IF_RELEASE(mDateTimes);

  mDateTimes = aDateVector;

  mDateTimes->AddRef();

  return NS_OK;
}
