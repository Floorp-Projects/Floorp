/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Eric D Vaughan <evaughan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsLayoutAtoms.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollbarMediator.h"
#include "nsIFormControlFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsButtonBoxFrame.h"
#include "nsITimer.h"
#include "nsRepeatService.h"

class nsAutoRepeatBoxFrame : public nsButtonBoxFrame, 
                             public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsAutoRepeatBoxFrame(nsIPresShell* aPresShell);
  friend nsIFrame* NS_NewAutoRepeatBoxFrame(nsIPresShell* aPresShell);

  NS_IMETHOD Destroy(nsPresContext* aPresContext);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_DECL_NSITIMERCALLBACK

protected:
  PRPackedBool mTrustedEvent;
};

nsIFrame*
NS_NewAutoRepeatBoxFrame (nsIPresShell* aPresShell)
{
  return new (aPresShell) nsAutoRepeatBoxFrame (aPresShell);
} // NS_NewScrollBarButtonFrame


nsAutoRepeatBoxFrame::nsAutoRepeatBoxFrame(nsIPresShell* aPresShell)
:nsButtonBoxFrame(aPresShell)
{
}

NS_INTERFACE_MAP_BEGIN(nsAutoRepeatBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(nsButtonBoxFrame)

NS_IMPL_ADDREF_INHERITED(nsAutoRepeatBoxFrame, nsButtonBoxFrame)
NS_IMPL_RELEASE_INHERITED(nsAutoRepeatBoxFrame, nsButtonBoxFrame)

NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{  
  switch(aEvent->message)
  {
    case NS_MOUSE_ENTER:
    case NS_MOUSE_ENTER_SYNTH:
      nsRepeatService::GetInstance()->Start(this);
      mTrustedEvent = NS_IS_TRUSTED_EVENT(aEvent);
      break;

    case NS_MOUSE_EXIT:
    case NS_MOUSE_EXIT_SYNTH:
      nsRepeatService::GetInstance()->Stop();
      // Not really necessary but do this to be safe
      mTrustedEvent = PR_FALSE;
      break;
  }
     
  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Notify(nsITimer *timer)
{
  DoMouseClick(nsnull, mTrustedEvent);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Destroy(nsPresContext* aPresContext)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();
  return nsButtonBoxFrame::Destroy(aPresContext);
}
