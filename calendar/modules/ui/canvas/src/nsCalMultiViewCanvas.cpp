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

#include "nsCalMultiViewCanvas.h"
#include "nsCalTimebarTimeHeading.h"
#include "nsBoxLayout.h"
#include "nsCalUICIID.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsCalToolkit.h"
#include "nsCalNewModelCommand.h"
#include "nscalstrings.h"


static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalMultiViewCanvasCID,  NS_CAL_MULTIVIEWCANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,         NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kCalTimebarCanvasCID,    NS_CAL_TIMEBARCANVAS_CID);

nsCalMultiViewCanvas :: nsCalMultiViewCanvas(nsISupports* outer) : nsCalTimebarComponentCanvas(outer)
{
  NS_INIT_REFCNT();

  mShowHeaders    = PR_TRUE;
  mShowStatus     = PR_FALSE;
  mShowTimeScale  = PR_FALSE;
}

nsCalMultiViewCanvas :: ~nsCalMultiViewCanvas()
{
}

nsresult nsCalMultiViewCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalMultiViewCanvasCID);                         
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
  if (aIID.Equals(kIXPFCCanvasIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsCalTimebarComponentCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsCalMultiViewCanvas)
NS_IMPL_RELEASE(nsCalMultiViewCanvas)

/*
 *
 */

nsresult nsCalMultiViewCanvas :: Init()
{
  return (nsCalTimebarComponentCanvas::Init());
}

nsEventStatus nsCalMultiViewCanvas :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                                         const nsRect& aDirtyRect)
{
  return (nsCalCanvas :: PaintBackground(aRenderingContext,aDirtyRect));  
}


PRBool nsCalMultiViewCanvas :: GetShowHeaders()
{
  return (mShowHeaders);
}

nsresult nsCalMultiViewCanvas :: SetShowHeaders(PRBool aShowHeaders)
{
  mShowHeaders = aShowHeaders;
  return (NS_OK);
}


PRBool nsCalMultiViewCanvas :: GetShowStatus()
{
  return (mShowStatus);
}

nsresult nsCalMultiViewCanvas :: SetShowStatus(PRBool aShowStatus)
{
  mShowStatus = aShowStatus;
  return (NS_OK);
}

PRBool nsCalMultiViewCanvas :: GetShowTimeScale()
{
  return (mShowTimeScale);
}

nsresult nsCalMultiViewCanvas :: SetShowTimeScale(PRBool aShowTimeScale)
{
  mShowTimeScale = aShowTimeScale;
  return (NS_OK);
}


nsEventStatus nsCalMultiViewCanvas::Action(nsIXPFCCommand * aCommand)
{
  return (nsCalTimebarComponentCanvas::Action(aCommand));
}

nsresult nsCalMultiViewCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalTimebarComponentCanvas::SetParameter(aKey, aValue));
}
