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

#include "nsCalMultiDayViewCanvas.h"
#include "nsCalMultiUserViewCanvas.h"
#include "nsCalTimebarTimeHeading.h"
#include "nsBoxLayout.h"
#include "nsCalUICIID.h"
#include "nsIVector.h"
#include "nsIIterator.h"
#include "nsCalToolkit.h"
#include "nsCalNewModelCommand.h"
#include "nscalstrings.h"
#include "nsxpfcstrings.h"
#include "nsCalNewModelCommand.h"


static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalMultiViewCanvasCID,      NS_CAL_MULTIVIEWCANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,             NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kCalTimebarCanvasCID,        NS_CAL_TIMEBARCANVAS_CID);
static NS_DEFINE_IID(kCalMultiDayViewCanvasCID,   NS_CAL_MULTIDAYVIEWCANVAS_CID);

nsCalMultiUserViewCanvas :: nsCalMultiUserViewCanvas(nsISupports* outer) : nsCalMultiViewCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsCalMultiUserViewCanvas :: ~nsCalMultiUserViewCanvas()
{
}

nsresult nsCalMultiUserViewCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
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
  return (nsCalMultiViewCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsCalMultiUserViewCanvas)
NS_IMPL_RELEASE(nsCalMultiUserViewCanvas)

/*
 * A MultiUser Canvas has 1 or more MultiDay Canvas's, one for each user
 *
 * So, let's create the default of 1 MuiltiDay for this canvas and give it 
 * a week's worth of views.
 *
 * Ideally, we should probably figure out an XML syntax to specify this stuff
 *
 */

nsresult nsCalMultiUserViewCanvas :: Init()
{

  /*
   * create a multiday as a child of us
   */
  nsCalMultiViewCanvas::Init();

  return (AddMultiDayView(nsnull));

}

nsresult nsCalMultiUserViewCanvas :: AddMultiDayView(nsIModel * aModel)
{
  static NS_DEFINE_IID(kIXPFCCanvasIID,           NS_IXPFC_CANVAS_IID);

  nsCalMultiDayViewCanvas * multi;
  
  nsresult res = nsRepository::CreateInstance(kCalMultiDayViewCanvasCID, 
                                              nsnull, 
                                              kIXPFCCanvasIID, 
                                              (void **)&multi);

  if (NS_OK == res)
  {

    multi->Init();

    AddChildCanvas(multi);

    multi->SetShowTimeScale(PR_TRUE);

    multi->SetTimeContext(GetTimeContext());

    SetMultiUserLayout(((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment());

    multi->SetNumberViewableDays(1); // XXX

    if (nsnull != aModel)
    {
      multi->SetModel(aModel);
    }

    Layout();
    
  }


  return (res);
}

nsresult nsCalMultiUserViewCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  PRInt32 error = 0;

  if (aKey.EqualsIgnoreCase(XPFC_STRING_LAYOUT)) 
  {
    // XXX: Layout should implement this interface.
    //      Then, put functionality in the core layout class
    //      to identify the type of layout object needed.

    if (aValue.EqualsIgnoreCase(XPFC_STRING_XBOX)) {
      ((nsBoxLayout *)GetLayout())->SetLayoutAlignment(eLayoutAlignment_horizontal);
    } else if (aValue.EqualsIgnoreCase(XPFC_STRING_YBOX)) {
      ((nsBoxLayout *)GetLayout())->SetLayoutAlignment(eLayoutAlignment_vertical);
    }

    // XXX:  We need to separate layout from the content model ... arghh...
    //
    // If someone changes our layout, pass it on to any MultiDay canvas
    // that are our children.

    SetMultiUserLayout(((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment());

  } 
  
  return (nsXPFCCanvas :: SetParameter(aKey, aValue));
}

nsresult nsCalMultiUserViewCanvas :: SetMultiUserLayout(nsLayoutAlignment aLayoutAlignment)
{
  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas ;

  res = CreateIterator(&iterator);

  nsLayoutAlignment la = aLayoutAlignment;

  if (NS_OK == res)
  {

    iterator->Init();

    while(!(iterator->IsDone()))
    {
      canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

      nsCalMultiDayViewCanvas * md = nsnull;

      res = canvas->QueryInterface(kCalMultiDayViewCanvasCID, (void**)&md);

      if (NS_OK == res)
      {
        ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(la);
        md->SetMultiDayLayout(la);
        NS_RELEASE(md);
      }

      iterator->Next();
    }

    NS_RELEASE(iterator);
  }    

  return NS_OK;
}

nsresult nsCalMultiUserViewCanvas :: SetTimeContext(nsICalTimeContext * aContext)
{
  //aContext->SetHorizontal(PR_TRUE);
  return (nsCalMultiViewCanvas :: SetTimeContext(aContext));
}


nsEventStatus nsCalMultiUserViewCanvas::Action(nsIXPFCCommand * aCommand)
{
  nsresult res;

  nsCalNewModelCommand * newmodel_command = nsnull;
  static NS_DEFINE_IID(kCalNewModelCommandCID, NS_CAL_NEWMODEL_COMMAND_CID);                 

  res = aCommand->QueryInterface(kCalNewModelCommandCID,(void**)&newmodel_command);

  if (NS_OK == res)
  {

    /*
     * A NewModel Command will Add a new MultiDayView in this MultiUser
     * canvas, which *clones* the attributes of the other MultiDay views
     *
     * For now, let's just limp along by adding it to the list...
     */

    AddMultiDayView(newmodel_command->mModel);

    NS_RELEASE(newmodel_command);

    return (nsEventStatus_eConsumeNoDefault);
  }

  return (nsCalMultiViewCanvas::Action(aCommand));
}

