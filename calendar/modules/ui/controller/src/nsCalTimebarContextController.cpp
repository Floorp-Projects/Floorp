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

#include "nsCalTimebarContextController.h"
#include "nsCalUICIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalContextControllerIID, NS_ICAL_CONTEXT_CONTROLLER_IID);

static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);
static NS_DEFINE_IID(kCalDurationCommandCID, NS_CAL_DURATION_COMMAND_CID);

#define DEFAULT_WIDTH  25
#define DEFAULT_HEIGHT 25

#include "nsCalDurationCommand.h"

nsCalTimebarContextController :: nsCalTimebarContextController(nsISupports * aOuter) : nsCalContextController(aOuter)
{
  NS_INIT_REFCNT();
}

nsCalTimebarContextController :: ~nsCalTimebarContextController()
{
}

nsresult nsCalTimebarContextController::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalContextControllerIID);                         

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

  return (nsCalContextController::QueryInterface(aIID, aInstancePtr));

}


NS_IMPL_ADDREF(nsCalTimebarContextController)
NS_IMPL_RELEASE(nsCalTimebarContextController)

nsEventStatus nsCalTimebarContextController :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                                               const nsRect& aDirtyRect)
{

  nsPoint pts[3];

  aRenderingContext.SetColor(GetForegroundColor());

  GetTrianglePoints(pts);

  RenderController(aRenderingContext, pts,3);

  return nsEventStatus_eConsumeNoDefault;  
}


nsresult nsCalTimebarContextController :: RenderController(nsIRenderingContext& aCtx,
                                                           nsPoint * aPoints,
                                                           PRUint32 aNumPoints)
{
  aCtx.FillPolygon(aPoints,aNumPoints);

  switch(GetOrientation())
  {
    case nsContextControllerOrientation_east:

      aCtx.SetColor(Highlight(GetForegroundColor()));
      aCtx.DrawLine(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y);
      aCtx.DrawLine(aPoints[2].x,aPoints[2].y,aPoints[0].x,aPoints[0].y);

      aCtx.SetColor(Dim(GetForegroundColor()));
      aCtx.DrawLine(aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);

      break;

    case nsContextControllerOrientation_west:

      aCtx.SetColor(Highlight(Highlight(GetForegroundColor())));
      aCtx.DrawLine(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y);

      aCtx.SetColor(Dim(GetForegroundColor()));
      aCtx.DrawLine(aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);
      aCtx.DrawLine(aPoints[2].x,aPoints[2].y,aPoints[0].x,aPoints[0].y);
      break;

    case nsContextControllerOrientation_north:

      aCtx.SetColor(Highlight(Highlight(GetForegroundColor())));
      aCtx.DrawLine(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y);

      aCtx.SetColor(Highlight(GetForegroundColor()));
      aCtx.DrawLine(aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);

      aCtx.SetColor(Dim(GetForegroundColor()));
      aCtx.DrawLine(aPoints[2].x,aPoints[2].y,aPoints[0].x,aPoints[0].y);
      break;

    case nsContextControllerOrientation_south:
      aCtx.SetColor(Dim(GetForegroundColor()));
      aCtx.DrawLine(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y);
      aCtx.DrawLine(aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);

      aCtx.SetColor(Highlight(GetForegroundColor()));
      aCtx.DrawLine(aPoints[2].x,aPoints[2].y,aPoints[0].x,aPoints[0].y);
      break;

  }


  return (NS_OK);
}


nsresult nsCalTimebarContextController :: Init()
{
  return (nsCalContextController::Init());
}


nsresult nsCalTimebarContextController :: GetTrianglePoints(nsPoint * pts)
{
  nsRect rect;
  PRUint32 widthPart, widthHalf;
  PRUint32 heightPart, heightHalf;

  GetBounds(rect);

  widthPart   = (PRUint32)(((float)rect.width) * 0.2f);
  heightPart  = (PRUint32)(((float)rect.height) * 0.2f);
  widthHalf   = (PRUint32)(((float)rect.width) * 0.5f);
  heightHalf  = (PRUint32)(((float)rect.height) * 0.5f);

  switch(GetOrientation())
  {
    case nsContextControllerOrientation_east:

      pts[0].x = rect.x + widthPart;
      pts[0].y = rect.y + heightPart;

      pts[1].x = rect.x + rect.width - widthPart;
      pts[1].y = rect.y + heightHalf;

      pts[2].x = rect.x + widthPart;
      pts[2].y = rect.y + rect.height - heightPart;

      break;

    case nsContextControllerOrientation_west:

      pts[0].x = rect.x + rect.width - widthPart;
      pts[0].y = rect.y + heightPart;

      pts[1].x = rect.x + widthPart;
      pts[1].y = rect.y + heightHalf;

      pts[2].x = rect.x + rect.width - widthPart;
      pts[2].y = rect.y + rect.height - heightPart;

      break;

    case nsContextControllerOrientation_north:

      pts[0].x = rect.x + widthPart;
      pts[0].y = rect.y + rect.height - heightPart;

      pts[1].x = rect.x + widthHalf;
      pts[1].y = rect.y + heightPart;

      pts[2].x = rect.x + rect.width - widthPart;
      pts[2].y = rect.y + rect.height - heightPart;
      
      break;

    case nsContextControllerOrientation_south:

      pts[0].x = rect.x + widthPart;
      pts[0].y = rect.y + heightPart;

      pts[1].x = rect.x + widthHalf;
      pts[1].y = rect.y + rect.height - heightPart;

      pts[2].x = rect.x + rect.width - widthPart;
      pts[2].y = rect.y + heightPart;
      
      break;

  }

  return NS_OK;

}

/*
 * Need to implement point in triangle intersection
 */

PRBool nsCalTimebarContextController::IsPointInTriangle(nsPoint aPoint, nsPoint * aTrinagle)
{

  return PR_TRUE;
}

/*
 * If the user clicked on the foreground drawing area for the context controller,
 * notify the relevant context[s]
 */
nsEventStatus nsCalTimebarContextController::OnLeftButtonDown(nsGUIEvent *aEvent) 
{    
  nsPoint pts[3];
  nsRect rect;
  GetBounds(rect);

  GetTrianglePoints(pts);

  if (IsPointInTriangle(aEvent->point, pts) == PR_TRUE) {

    /*
     * Notify the Observers of the appropriate Command
     *
     */

    nsCalDurationCommand * command;

    nsresult res = nsRepository::CreateInstance(kCalDurationCommandCID, 
                                                nsnull, 
                                                kXPFCCommandIID, 
                                                (void **)&command);

    if (NS_OK != res)
      return nsEventStatus_eConsumeNoDefault ;

    command->Init(GetDuration());

    command->SetPeriodFormat(GetPeriodFormat());

    Notify(command);

    NS_IF_RELEASE(command);

  }

  return nsEventStatus_eConsumeNoDefault;
}

nsresult nsCalTimebarContextController :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalContextController::SetParameter(aKey, aValue));
}

nsresult nsCalTimebarContextController :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}
