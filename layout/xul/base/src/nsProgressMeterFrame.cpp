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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

//
// David Hyatt & Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsProgressMeterFrame.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsIReflowCallback.h"
#include "nsContentUtils.h"
//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewProgressMeterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsProgressMeterFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsProgressMeterFrame)

//
// nsProgressMeterFrame dstr
//
// Cleanup, if necessary
//
nsProgressMeterFrame :: ~nsProgressMeterFrame ( )
{
}

class nsAsyncProgressMeterInit : public nsIReflowCallback
{
public:
  nsAsyncProgressMeterInit(nsIFrame* aFrame) : mWeakFrame(aFrame) {}

  virtual bool ReflowFinished()
  {
    bool shouldFlush = false;
    nsIFrame* frame = mWeakFrame.GetFrame();
    if (frame) {
      nsAutoScriptBlocker scriptBlocker;
      frame->AttributeChanged(kNameSpaceID_None, nsGkAtoms::value, 0);
      shouldFlush = PR_TRUE;
    }
    delete this;
    return shouldFlush;
  }

  virtual void ReflowCallbackCanceled()
  {
    delete this;
  }

  nsWeakFrame mWeakFrame;
};

NS_IMETHODIMP
nsProgressMeterFrame::DoLayout(nsBoxLayoutState& aState)
{
  if (mNeedsReflowCallback) {
    nsIReflowCallback* cb = new nsAsyncProgressMeterInit(this);
    if (cb) {
      PresContext()->PresShell()->PostReflowCallback(cb);
    }
    mNeedsReflowCallback = PR_FALSE;
  }
  return nsBoxFrame::DoLayout(aState);
}

NS_IMETHODIMP
nsProgressMeterFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                       nsIAtom* aAttribute,
                                       PRInt32 aModType)
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
      "Scripts not blocked in nsProgressMeterFrame::AttributeChanged!");
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  if (NS_OK != rv) {
    return rv;
  }

  // did the progress change?
  if (nsGkAtoms::value == aAttribute || nsGkAtoms::max == aAttribute) {
    nsIFrame* barChild = GetFirstPrincipalChild();
    if (!barChild) return NS_OK;
    nsIFrame* remainderChild = barChild->GetNextSibling();
    if (!remainderChild) return NS_OK;
    nsCOMPtr<nsIContent> remainderContent = remainderChild->GetContent();
    if (!remainderContent) return NS_OK;

    nsAutoString value, maxValue;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::max, maxValue);

    PRInt32 error;
    PRInt32 flex = value.ToInteger(&error);
    PRInt32 maxFlex = maxValue.ToInteger(&error);
    if (NS_FAILED(error) || maxValue.IsEmpty()) {
      maxFlex = 100;
    }
    if (maxFlex < 1) {
      maxFlex = 1;
    }
    if (flex < 0) {
      flex = 0;
    }
    if (flex > maxFlex) {
      flex = maxFlex;
    }

    nsContentUtils::AddScriptRunner(new nsSetAttrRunnable(
      barChild->GetContent(), nsGkAtoms::flex, flex));
    nsContentUtils::AddScriptRunner(new nsSetAttrRunnable(
      remainderContent, nsGkAtoms::flex, maxFlex - flex));
    nsContentUtils::AddScriptRunner(new nsReflowFrameRunnable(
      this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY));
  }
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsProgressMeterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ProgressMeter"), aResult);
}
#endif
