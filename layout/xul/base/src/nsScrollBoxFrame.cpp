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
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsButtonBoxFrame.h"
#include "nsITimer.h"
#include "nsRepeatService.h"

class nsAutoRepeatBoxFrame : public nsButtonBoxFrame, 
                             public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  friend nsIFrame* NS_NewAutoRepeatBoxFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

  NS_IMETHOD Init(nsIContent*     aContent,
                  nsIFrame*       aParent,
                  nsIFrame*       aPrevInFlow);

  virtual void Destroy();

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD HandlePress(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext, 
                           nsGUIEvent*     aEvent,
                           nsEventStatus*  aEventStatus);

  NS_DECL_NSITIMERCALLBACK

protected:
  nsAutoRepeatBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext):
    nsButtonBoxFrame(aPresShell, aContext) {}

  PRPackedBool mTrustedEvent;
  PRPackedBool mIsPressMode;

  void InitRepeatMode();
};

nsIFrame*
NS_NewAutoRepeatBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsAutoRepeatBoxFrame (aPresShell, aContext);
} // NS_NewScrollBarButtonFrame

NS_INTERFACE_MAP_BEGIN(nsAutoRepeatBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(nsButtonBoxFrame)

NS_IMPL_ADDREF_INHERITED(nsAutoRepeatBoxFrame, nsButtonBoxFrame)
NS_IMPL_RELEASE_INHERITED(nsAutoRepeatBoxFrame, nsButtonBoxFrame)

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Init(nsIContent* aContent,
                           nsIFrame* aParent,
                           nsIFrame* aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  InitRepeatMode();

  return rv;
}


NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{  
  switch(aEvent->message)
  {
    case NS_MOUSE_ENTER:
    case NS_MOUSE_ENTER_SYNTH:
      if (!mIsPressMode) {
        nsRepeatService::GetInstance()->Start(this);
        mTrustedEvent = NS_IS_TRUSTED_EVENT(aEvent);
      }
      break;

    case NS_MOUSE_EXIT:
    case NS_MOUSE_EXIT_SYNTH:
      // always stop on mouse exit
      nsRepeatService::GetInstance()->Stop();
      // Not really necessary but do this to be safe
      mTrustedEvent = PR_FALSE;
      break;

    case NS_MOUSE_CLICK:
      if (NS_IS_MOUSE_LEFT_CLICK(aEvent)) {
        // skip button frame handling to prevent click handling
         return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
      }
      break;
  }
     
  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandlePress(nsPresContext* aPresContext, 
                                  nsGUIEvent* aEvent,
                                  nsEventStatus* aEventStatus)
{
  if (mIsPressMode) {
    mTrustedEvent = NS_IS_TRUSTED_EVENT(aEvent);
    DoMouseClick(aEvent, mTrustedEvent);
    nsRepeatService::GetInstance()->Start(this);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsAutoRepeatBoxFrame::HandleRelease(nsPresContext* aPresContext, 
                                    nsGUIEvent* aEvent,
                                    nsEventStatus* aEventStatus)
{
  if (mIsPressMode) {
    nsRepeatService::GetInstance()->Stop();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                       nsIAtom* aAttribute,
                                       PRInt32 aModType)
{
  if (aAttribute == nsGkAtoms::type) {
    nsRepeatService::GetInstance()->Stop();
    InitRepeatMode();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Notify(nsITimer *timer)
{
  DoMouseClick(nsnull, mTrustedEvent);
  return NS_OK;
}

void
nsAutoRepeatBoxFrame::Destroy()
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();
  nsButtonBoxFrame::Destroy();
}

void
nsAutoRepeatBoxFrame::InitRepeatMode()
{
  // repeat mode may be "hover" for repeating while the mouse is hovering
  // over the element, otherwise repetition is done while the element is
  // active (pressed).
  nsAutoString repeat;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::repeat, repeat);
  mIsPressMode = !repeat.EqualsLiteral("hover");
}
