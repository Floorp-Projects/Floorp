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

#ifndef __CThrobber__
#define __CThrobber__

#include <LControl.h>

#ifndef nsError_h
#include "nsError.h"
#endif

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#ifndef nsGUIEvent_h__
#include "nsGUIEvent.h"
#endif

#include "nsString.h"
#include "nsIImageObserver.h"

#include <map>
#include <vector>
using namespace std;

class nsIWidget;
class nsIImageGroup;
class nsITimer;
class nsIImageRequest;

class CThrobber : public LControl,
                  public nsIImageRequestObserver
{
public:
	enum { class_ID = FOUR_CHAR_CODE('Thrb') };

                           CThrobber();
						         CThrobber(LStream*	inStream);

	virtual				      ~CThrobber();

   NS_DECL_ISUPPORTS

  // nsIImageRequestObserver
  virtual void             Notify(nsIImageRequest *aImageRequest,
                                  nsIImage *aImage,
                                  nsImageNotification aNotificationType,
                                  PRInt32 aParam1, PRInt32 aParam2,
                                  void *aParam3);

   virtual void            NotifyError(nsIImageRequest *aImageRequest,
                                       nsImageError aErrorType);
	
	// CThrobber
	virtual void            FinishCreateSelf();
	virtual void	         ShowSelf();
	virtual void	         HideSelf();
	virtual void            DrawSelf();
   virtual void            AdjustCursorSelf(Point inPortPt,
	                                         const EventRecord& inMacEvent);

   void                    ResizeFrameBy(SInt16		inWidthDelta,
                            				  SInt16		inHeightDelta,
                            				  Boolean	inRefresh);
   void                    MoveBy(SInt32		inHorizDelta,
         				             SInt32		inVertDelta,
         							    Boolean    inRefresh);
	
	virtual void            Start();
	virtual void            Stop();
 protected:

   enum { kDefaultImageIndex = 0, kAnimImageIndex };
   
   void                    AdjustFrame(Boolean inRefresh);
   nsresult                LoadImages();
   void                    DestroyImages();
   void                    Tick();
   
   char                    mDefImageURL[256], mAnimImageURL[256];
       
   nsCOMPtr<nsIWidget>     mWidget;
   vector<nsIImageRequest*> *mImages;
   bool                    mRunning;
   SInt32                  mNumImages, mCompletedImages;
   nsIImageGroup           *mImageGroup;
   nsCOMPtr<nsITimer>      mTimer;
   
   static map<nsIWidget*, CThrobber*> mgThrobberMap;
   
   static CThrobber*       FindThrobberForWidget(nsIWidget* aWidget);
   static void             AddThrobber(CThrobber* aThrobber);
   static void             RemoveThrobber(CThrobber* aThrobber);
   static nsEventStatus PR_CALLBACK HandleThrobberEvent(nsGUIEvent *aEvent);
   static void             ThrobTimerCallback(nsITimer *aTimer, void *aClosure);

};


#endif
