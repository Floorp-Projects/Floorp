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

#include "nsCalTodoComponentCanvas.h"
#include "nsCalUICIID.h"
#include "nsIListWidget.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"


static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTodoComponentCanvasCID,  NS_CAL_TODOCOMPONENTCANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,             NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kIListWidgetIID,             NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kIWidgetIID,                 NS_IWIDGET_IID);
static NS_DEFINE_IID(kCListWidgetCID,             NS_LISTBOX_CID);

nsEventStatus PR_CALLBACK HandleEventListWidget(nsGUIEvent *aEvent);

nsCalTodoComponentCanvas :: nsCalTodoComponentCanvas(nsISupports* outer) : nsCalTimebarComponentCanvas(outer)
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
}

nsCalTodoComponentCanvas :: ~nsCalTodoComponentCanvas()
{
}

nsresult nsCalTodoComponentCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTodoComponentCanvasCID);                         
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
  return (nsCalTimebarComponentCanvas::QueryInterface(aIID,aInstancePtr));                                                 
}

NS_IMPL_ADDREF(nsCalTodoComponentCanvas)
NS_IMPL_RELEASE(nsCalTodoComponentCanvas)

nsresult nsCalTodoComponentCanvas :: Init()
{
  nsRect rect;

  GetBounds(rect);

  nsIWidget * parent = GetWidget();
  nsIListWidget * listWidget = nsnull;

  nsresult res = nsRepository::CreateInstance(kCListWidgetCID, 
                                              nsnull, 
                                              kIWidgetIID, 
                                              (void **)&mWidget);

  if (NS_OK != res)
    return res;

  res = mWidget->QueryInterface(kIListWidgetIID, (void**)&listWidget);

  if (NS_OK != res)
    return res;

  nsIDeviceContext * context = nsnull;

  if (parent)
    context = parent->GetDeviceContext();

  mWidget->Create(parent, 
                  rect, 
                  HandleEventListWidget, 
                  context, 
                  nsnull, nsnull);

  NS_IF_RELEASE(context);

  listWidget->AddItemAt(nsString("TODO LIST"),-1);
  listWidget->AddItemAt(nsString("---------"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #1"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #2"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #3"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #4"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #5"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #6"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #7"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #8"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #9"),-1);
  listWidget->AddItemAt(nsString("Todo: Item #10"),-1);
  
  NS_RELEASE(listWidget);

  SetBackgroundColor(NS_RGB(255,255,192));
  mWidget->Show(PR_TRUE);

  return NS_OK;
}

nsEventStatus PR_CALLBACK HandleEventListWidget(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
}

nsresult nsCalTodoComponentCanvas :: SetBounds(const nsRect &aBounds)
{
  return (nsXPFCCanvas::SetBounds(aBounds));
  //return NS_OK;
}


void nsCalTodoComponentCanvas :: SetBackgroundColor(const nscolor &aColor)
{
//  if (mWidget)
//      mWidget->SetBackgroundColor(aColor);

  nsXPFCCanvas::SetBackgroundColor(aColor);
}

nsEventStatus nsCalTodoComponentCanvas :: OnPaint(nsIRenderingContext& aRenderingContext,
                                                  const nsRect& aDirtyRect)
{
  mWidget->Invalidate(PR_FALSE);
  return (nsEventStatus_eConsumeNoDefault);
}
