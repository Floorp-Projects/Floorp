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

#include "nsCalTimebarComponentCanvas.h"
#include "nsCalUICIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarComponentCanvasCID, NS_CAL_TIMEBARCOMPONENTCANVAS_CID);

nsCalTimebarComponentCanvas :: nsCalTimebarComponentCanvas(nsISupports* outer) : nsCalTimebarCanvas(outer)
{
  NS_INIT_REFCNT();
  mComponentColor = NS_RGB(68,141,192);
}

nsCalTimebarComponentCanvas :: ~nsCalTimebarComponentCanvas()
{
}

nsresult nsCalTimebarComponentCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimebarComponentCanvasCID);                         
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
  return (nsCalTimebarCanvas::QueryInterface(aIID, aInstancePtr));                                                 
}

NS_IMPL_ADDREF(nsCalTimebarComponentCanvas)
NS_IMPL_RELEASE(nsCalTimebarComponentCanvas)

nsresult nsCalTimebarComponentCanvas :: Init()
{
  SetBackgroundColor(NS_RGB(255,255,255));
  return (nsCalTimebarCanvas::Init()) ;
}
