/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"

// Spacer type's
#define TYPE_WORD  0            // horizontal space
#define TYPE_LINE  1            // line-break + vertical space
#define TYPE_IMAGE 2            // acts like a sized image with nothing to see

class SpacerFrame : public nsFrame {
public:
  friend nsresult NS_NewSpacerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  PRUint8 GetType();

protected:
  SpacerFrame();
  virtual ~SpacerFrame();
};

nsresult
NS_NewSpacerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  SpacerFrame* it = new (aPresShell) SpacerFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

SpacerFrame::SpacerFrame()
{
}

SpacerFrame::~SpacerFrame()
{
}

NS_IMETHODIMP
SpacerFrame::Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("SpacerFrame", aReflowState.reason);
  aStatus = NS_FRAME_COMPLETE;

  // By default, we have no area
  aMetrics.width = 0;
  aMetrics.height = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;

  nscoord width = 0;
  nscoord height = 0;
  PRUint8 type = GetType();
  nsresult ca;
  nsIHTMLContent* hc = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (nsnull != hc) {
    if (type != TYPE_IMAGE) {
      nsHTMLValue val;
      ca = hc->GetHTMLAttribute(nsHTMLAtoms::size, val);
      if (NS_CONTENT_ATTR_HAS_VALUE == ca) {
        width = val.GetPixelValue();
      }
    } else {
      nsHTMLValue val;
      ca = hc->GetHTMLAttribute(nsHTMLAtoms::width, val);
      if (NS_CONTENT_ATTR_HAS_VALUE == ca) {
        if (eHTMLUnit_Pixel == val.GetUnit()) {
          width = val.GetPixelValue();
        }
      }
      ca = hc->GetHTMLAttribute(nsHTMLAtoms::height, val);
      if (NS_CONTENT_ATTR_HAS_VALUE == ca) {
        if (eHTMLUnit_Pixel == val.GetUnit()) {
          height = val.GetPixelValue();
        }
      }
    }
    NS_RELEASE(hc);
  }

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  switch (type) {
  case TYPE_WORD:
    if (0 != width) {
      aMetrics.width = NSIntPixelsToTwips(width, p2t);
    }
    break;

  case TYPE_LINE:
    if (0 != width) {
      aStatus = NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_COMPLETE);
      aMetrics.height = NSIntPixelsToTwips(width, p2t);
      aMetrics.ascent = aMetrics.height;
    }
    break;

  case TYPE_IMAGE:
    aMetrics.width = NSIntPixelsToTwips(width, p2t);
    aMetrics.height = NSIntPixelsToTwips(height, p2t);
    aMetrics.ascent = aMetrics.height;
    break;
  }

  if (aMetrics.width || aMetrics.height) {
    // Make sure that the other dimension is non-zero
    if (!aMetrics.width) aMetrics.width = 1;
    if (!aMetrics.height) aMetrics.height = 1;
  }

  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  return NS_OK;
}

PRUint8
SpacerFrame::GetType()
{
  PRUint8 type = TYPE_WORD;
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, value)) {
    if (value.EqualsIgnoreCase("line") ||
        value.EqualsIgnoreCase("vert") ||
        value.EqualsIgnoreCase("vertical")) {
      return TYPE_LINE;
    }
    if (value.EqualsIgnoreCase("block")) {
      return TYPE_IMAGE;
    }
  }
  return type;
}
