/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
 */

#include "CThrobber.h"
#include "CBrowserWindow.h"

#include <LString.h>
#include <LStream.h>
#include <UDrawingState.h>

#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIImageGroup.h"
#include "nsIDeviceContext.h"
#include "nsITimer.h"
#include "nsIImageRequest.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "prprf.h"

// CIDs
static NS_DEFINE_IID(kChildCID,          NS_CHILD_CID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

// Static variables
map<nsIWidget*, CThrobber*> CThrobber::mgThrobberMap;

// Constants
const PRUint32 kThrobFrequency = 66;   // animation frequency in milliseconds

//*****************************************************************************
//***    CThrobber: constructors/destructor
//*****************************************************************************

CThrobber::CThrobber() :
   mImages(nsnull),
   mNumImages(0), mCompletedImages(0), mIndex(0), mRunning(false),
   mImageGroup(nsnull)
{
   NS_INIT_REFCNT(); // caller must add ref as normal
   
   AddThrobber(this);
}


CThrobber::CThrobber(LStream*	inStream) :
   LControl(inStream),
   mImages(nsnull),
   mNumImages(0), mCompletedImages(0), mIndex(0), mRunning(false),
   mImageGroup(nsnull)
{
   mRefCnt = 1; // PowerPlant is making us, and it sure isn't going to do an AddRef.
  
   LStr255  tempStr;
   *inStream >> (StringPtr) tempStr;
   mFileNamePattern.AssignWithConversion((char *)&tempStr[1], (PRInt32)tempStr.Length());
   
   mNumImages = mMaxValue;
}


CThrobber::~CThrobber()
{
   if (mWidget)
      mWidget->Destroy();
   DestroyImages();
   RemoveThrobber(this);
}

NS_IMPL_ISUPPORTS(CThrobber, kIImageObserverIID)

void CThrobber::Notify(nsIImageRequest *aImageRequest,
                       nsIImage *aImage,
                       nsImageNotification aNotificationType,
                       PRInt32 aParam1, PRInt32 aParam2,
                       void *aParam3)
{
   if (aNotificationType == nsImageNotification_kImageComplete)
   {
      mCompletedImages++;
      
      // Remove ourselves as an observer of the image request object, because
      // the image request objects each hold a reference to us. This avoids a
      // circular reference problem. If we don't, our ref count will never reach
      // 0 and we won't get destroyed and neither will the image request objects

      aImageRequest->RemoveObserver((nsIImageRequestObserver*)this);
   }
}

void  CThrobber::NotifyError(nsIImageRequest *aImageRequest,
                             nsImageError aErrorType)
{
}

	
void CThrobber::FinishCreateSelf()
{
   CBrowserWindow *ourWindow = dynamic_cast<CBrowserWindow*>(LWindow::FetchWindowObject(GetMacPort()));
   ThrowIfNil_(ourWindow);
   
   // Get the widget from the browser window
   nsCOMPtr<nsIWidget>  parentWidget;
   ourWindow->GetWidget(getter_AddRefs(parentWidget));
   ThrowIfNil_(parentWidget);
   
   FocusDraw();
   
	Rect portFrame;
	CalcPortFrameRect(portFrame);
	nsRect r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
   
  // Create widget
   nsresult rv;
   
   mWidget = do_CreateInstance(kChildCID, &rv);
   if (!mWidget)
      Throw_(NS_ERROR_GET_CODE(rv));
   mWidget->Create(parentWidget, r, HandleThrobberEvent, NULL);
   
   rv = LoadImages(mFileNamePattern, mNumImages);
   if (NS_SUCCEEDED(rv))
      AddThrobber(this); 
}


void CThrobber::ShowSelf()
{
   mWidget->Show(PR_TRUE);
}


void CThrobber::HideSelf()
{
   mWidget->Show(PR_FALSE);
}


void CThrobber::DrawSelf()
{
   // Draw directly with the rendering context instead of passing an
   // update event through nsMacMessageSink. By the time this routine is
   // called, PowerPlant has taken care of the location, z order, and clipping
   // of each view. Since focusing puts the the origin at our top left corner,
   // all we have to do is get the bounds of the widget and put that at (0,0)

   StColorPortState	origState(UQDGlobals::GetCurrentPort());

   nsIRenderingContext *cx = mWidget->GetRenderingContext();
   nsRect bounds;
   nsIImageRequest *imgreq;
   nsIImage *img;

   mWidget->GetClientBounds(bounds);
   bounds.x = bounds.y = 0;

   //cx->SetClipRect(bounds, nsClipCombine_kReplace, clipState);

   cx->SetColor(NS_RGB(255, 255, 255));
   cx->DrawLine(0, bounds.height - 1, 0, 0);
   cx->DrawLine(0, 0, bounds.width, 0);

   cx->SetColor(NS_RGB(128, 128, 128));
   cx->DrawLine(bounds.width - 1, 1, bounds.width - 1, bounds.height - 1);
   cx->DrawLine(bounds.width - 1, bounds.height - 1, 0, bounds.height - 1);

   imgreq = (*mImages)[mIndex];

   if ((nsnull == imgreq) || (nsnull == (img = imgreq->GetImage())))
   {
     char str[10];
     nsFont tfont = nsFont("monospace", 0, 0, 0, 0, 10);
     nsIFontMetrics *met;
     nscoord w, h;

     cx->SetColor(NS_RGB(0, 0, 0));
     cx->FillRect(1, 1, bounds.width - 2, bounds.height - 2);

     PR_snprintf(str, sizeof(str), "%02d", mIndex);

     cx->SetColor(NS_RGB(255, 255, 255));
     cx->SetFont(tfont);
     cx->GetFontMetrics(met);
     if (nsnull != met)
     {
       cx->GetWidth(str, w);
       met->GetHeight(h);
       cx->DrawString(str, PRUint32(2), (bounds.width - w) >> 1, (bounds.height - h) >> 1);
       NS_RELEASE(met);
     }
   }
   else
   {
     cx->DrawImage(img, 1, 1);
     NS_RELEASE(img);
   }
}


void CThrobber::AdjustCursorSelf(Point				/* inPortPt */,
	                              const EventRecord&	/* inMacEvent */)
{
   // Overridden to do nothing - Cursor handling is done by HandleThrobberEvent
}


void CThrobber::ResizeFrameBy(SInt16		inWidthDelta,
                					SInt16		inHeightDelta,
                					Boolean	   inRefresh)
{
	LControl::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	AdjustFrame(inRefresh);
}


void CThrobber::MoveBy(SInt32		inHorizDelta,
				           SInt32		inVertDelta,
							  Boolean	inRefresh)
{
	LControl::MoveBy(inHorizDelta, inVertDelta, inRefresh);
	AdjustFrame(inRefresh);
}

	
void CThrobber::Start()
{
   mRunning = true;
}


void CThrobber::Stop()
{
   mRunning = false;
}


void CThrobber::AdjustFrame(Boolean inRefresh)
{
	FocusDraw();

	Rect portFrame;
	CalcPortFrameRect(portFrame);
	nsRect r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
   
   mWidget->Resize(r.x, r.y, r.width, r.height, inRefresh); 		
}


NS_METHOD CThrobber::LoadImages(const nsString& aFileNameMask, PRInt32 aNumImages)
{
  nsresult rv;
  char url[2000];

  mImages = new vector<nsIImageRequest*>(mNumImages);
  if (nsnull == mImages) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = NS_NewImageGroup(&mImageGroup);
  if (NS_OK != rv) {
    return rv;
  }

  nsIDeviceContext *deviceCtx = mWidget->GetDeviceContext();
  mImageGroup->Init(deviceCtx, nsnull);
  NS_RELEASE(deviceCtx);

  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_OK != rv) {
    return rv;
  }
  mTimer->Init(ThrobTimerCallback, this, kThrobFrequency, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
    
  char * mask = aFileNameMask.ToNewCString();
  for (PRInt32 cnt = 0; cnt < mNumImages; cnt++)
  {
    PR_snprintf(url, sizeof(url), mask, cnt);
    nscolor bgcolor = NS_RGB(0, 0, 0);
    (*mImages)[cnt] = mImageGroup->GetImage(url,
                                             (nsIImageRequestObserver *)this,
                                             &bgcolor,
                                             mFrameSize.width - 2,
                                             mFrameSize.height - 2, 0);
  }

  if (nsnull != mask)
    nsCRT::free(mask);

  mWidget->Invalidate(PR_TRUE);

  return rv;
}

void CThrobber::DestroyImages()
{
  if (mTimer)
  {
    mTimer->Cancel();
  }

  if (mImageGroup)
  {
    mImageGroup->Interrupt();
    
    for (vector<nsIImageRequest*>::iterator iter = mImages->begin(); iter < mImages->end(); ++iter)
    {
      NS_IF_RELEASE(*iter);
    }
    NS_RELEASE(mImageGroup);
  }

  if (mImages)
  {
    delete mImages;
    mImages = nsnull;
  }
}

void CThrobber::Tick()
{
  if (mRunning) {
    mIndex++;
    if (mIndex >= mNumImages)
      mIndex = 0;
    FocusDraw();
    mWidget->Invalidate(PR_TRUE);
  } else if (mCompletedImages == (PRUint32)mNumImages) {
      FocusDraw();
    mWidget->Invalidate(PR_TRUE);
    mCompletedImages = 0;
  }

#ifndef REPEATING_TIMERS
  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_OK == rv) {
    mTimer->Init(ThrobTimerCallback, this, kThrobFrequency);
  }
#endif
}



CThrobber* CThrobber::FindThrobberForWidget(nsIWidget* aWidget)
{
   map<nsIWidget*, CThrobber*>::iterator iter = mgThrobberMap.find(aWidget);
   if (iter == mgThrobberMap.end())
      return nsnull;
   else
      return iter->second;
}


void CThrobber::AddThrobber(CThrobber* aThrobber)
{
   pair<nsIWidget*, CThrobber*>  entry(aThrobber->mWidget, aThrobber);
   mgThrobberMap[aThrobber->mWidget] = aThrobber;
}


void CThrobber::RemoveThrobber(CThrobber* aThrobber)
{
   map<nsIWidget*, CThrobber*>::iterator iter = mgThrobberMap.find(aThrobber->mWidget);
   if (iter != mgThrobberMap.end())
      mgThrobberMap.erase(iter);
}


nsEventStatus PR_CALLBACK CThrobber::HandleThrobberEvent(nsGUIEvent *aEvent)
{
  CThrobber* throbber = FindThrobberForWidget(aEvent->widget);
  if (nsnull == throbber) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->message)
  {
    case NS_PAINT:
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      // Broadcast a message
      break;

    case NS_MOUSE_ENTER:
      aEvent->widget->SetCursor(eCursor_hyperlink);
      break;

    case NS_MOUSE_EXIT:
      aEvent->widget->SetCursor(eCursor_standard);
      break;
  }

  return nsEventStatus_eIgnore;
}


void CThrobber::ThrobTimerCallback(nsITimer *aTimer, void *aClosure)
{
  CThrobber* throbber = (CThrobber*)aClosure;
  throbber->Tick();
}
