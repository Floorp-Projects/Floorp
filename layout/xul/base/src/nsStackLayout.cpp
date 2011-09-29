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
 *   David Hyatt (hyatt@netscape.com)
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
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsStackLayout.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

nsBoxLayout* nsStackLayout::gInstance = nsnull;

#define SPECIFIED_LEFT (1 << NS_SIDE_LEFT)
#define SPECIFIED_RIGHT (1 << NS_SIDE_RIGHT)
#define SPECIFIED_TOP (1 << NS_SIDE_TOP)
#define SPECIFIED_BOTTOM (1 << NS_SIDE_BOTTOM)

nsresult
NS_NewStackLayout( nsIPresShell* aPresShell, nsCOMPtr<nsBoxLayout>& aNewLayout)
{
  if (!nsStackLayout::gInstance) {
    nsStackLayout::gInstance = new nsStackLayout();
    NS_IF_ADDREF(nsStackLayout::gInstance);
  }
  // we have not instance variables so just return our static one.
  aNewLayout = nsStackLayout::gInstance;
  return NS_OK;
} 

/*static*/ void
nsStackLayout::Shutdown()
{
  NS_IF_RELEASE(gInstance);
}

nsStackLayout::nsStackLayout()
{
}

/*
 * Sizing: we are as wide as the widest child plus its left offset
 * we are tall as the tallest child plus its top offset.
 *
 * Only children which have -moz-stack-sizing set to stretch-to-fit
 * (the default) will be included in the size computations.
 */

nsSize
nsStackLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsSize prefSize (0, 0);

  nsIBox* child = aBox->GetChildBox();
  while (child) {
    if (child->GetStyleXUL()->mStretchStack) {
      nsSize pref = child->GetPrefSize(aState);

      AddMargin(child, pref);
      nsMargin offset;
      GetOffset(aState, child, offset);
      pref.width += offset.LeftRight();
      pref.height += offset.TopBottom();
      AddLargestSize(prefSize, pref);
    }

    child = child->GetNextBox();
  }

  AddBorderAndPadding(aBox, prefSize);

  return prefSize;
}

nsSize
nsStackLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsSize minSize (0, 0);

  nsIBox* child = aBox->GetChildBox();
  while (child) {
    if (child->GetStyleXUL()->mStretchStack) {
      nsSize min = child->GetMinSize(aState);

      AddMargin(child, min);
      nsMargin offset;
      GetOffset(aState, child, offset);
      min.width += offset.LeftRight();
      min.height += offset.TopBottom();
      AddLargestSize(minSize, min);
    }

    child = child->GetNextBox();
  }

  AddBorderAndPadding(aBox, minSize);

  return minSize;
}

nsSize
nsStackLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsSize maxSize (NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  nsIBox* child = aBox->GetChildBox();
  while (child) {
    if (child->GetStyleXUL()->mStretchStack) {
      nsSize min = child->GetMinSize(aState);
      nsSize max = child->GetMaxSize(aState);

      max = nsBox::BoundsCheckMinMax(min, max);

      AddMargin(child, max);
      nsMargin offset;
      GetOffset(aState, child, offset);
      max.width += offset.LeftRight();
      max.height += offset.TopBottom();
      AddSmallestSize(maxSize, max);
    }

    child = child->GetNextBox();
  }

  AddBorderAndPadding(aBox, maxSize);

  return maxSize;
}


nscoord
nsStackLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nscoord vAscent = 0;

  nsIBox* child = aBox->GetChildBox();
  while (child) {  
    nscoord ascent = child->GetBoxAscent(aState);
    nsMargin margin;
    child->GetMargin(margin);
    ascent += margin.top;
    if (ascent > vAscent)
      vAscent = ascent;

    child = child->GetNextBox();
  }

  return vAscent;
}

PRUint8
nsStackLayout::GetOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsMargin& aOffset)
{
  aOffset = nsMargin(0, 0, 0, 0);

  // get the left, right, top and bottom offsets

  // As an optimization, we cache the fact that we are not positioned to avoid
  // wasting time fetching attributes.
  if (aChild->IsBoxFrame() &&
      (aChild->GetStateBits() & NS_STATE_STACK_NOT_POSITIONED))
    return 0;

  PRUint8 offsetSpecified = 0;
  nsIContent* content = aChild->GetContent();
  if (content) {
    bool ltr = aChild->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR;
    nsAutoString value;
    PRInt32 error;

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::start, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      if (ltr) {
        aOffset.left =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
        offsetSpecified |= SPECIFIED_LEFT;
      } else {
        aOffset.right =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
        offsetSpecified |= SPECIFIED_RIGHT;
      }
    }

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::end, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      if (ltr) {
        aOffset.right =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
        offsetSpecified |= SPECIFIED_RIGHT;
      } else {
        aOffset.left =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
        offsetSpecified |= SPECIFIED_LEFT;
      }
    }

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::left, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      aOffset.left =
        nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      offsetSpecified |= SPECIFIED_LEFT;
    }

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::right, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      aOffset.right =
        nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      offsetSpecified |= SPECIFIED_RIGHT;
    }

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::top, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      aOffset.top =
        nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      offsetSpecified |= SPECIFIED_TOP;
    }

    content->GetAttr(kNameSpaceID_None, nsGkAtoms::bottom, value);
    if (!value.IsEmpty()) {
      value.Trim("%");
      aOffset.bottom =
        nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      offsetSpecified |= SPECIFIED_BOTTOM;
    }
  }

  if (!offsetSpecified && aChild->IsBoxFrame()) {
    // If no offset was specified at all, then we cache this fact to avoid requerying
    // CSS or the content model.
    aChild->AddStateBits(NS_STATE_STACK_NOT_POSITIONED);
  }

  return offsetSpecified;
}


NS_IMETHODIMP
nsStackLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  bool grow;

  do {
    nsIBox* child = aBox->GetChildBox();
    grow = PR_FALSE;

    while (child) 
    {  
      nsMargin margin;
      child->GetMargin(margin);
      nsRect childRect(clientRect);
      childRect.Deflate(margin);

      if (childRect.width < 0)
        childRect.width = 0;

      if (childRect.height < 0)
        childRect.height = 0;

      nsRect oldRect(child->GetRect());
      bool sizeChanged = !oldRect.IsEqualEdges(childRect);

      // only lay out dirty children or children whose sizes have changed
      if (sizeChanged || NS_SUBTREE_DIRTY(child)) {
          // add in the child's margin
          nsMargin margin;
          child->GetMargin(margin);

          // obtain our offset from the top left border of the stack's content box.
          nsMargin offset;
          PRUint8 offsetSpecified = GetOffset(aState, child, offset);

          // Set the position and size based on which offsets have been specified:
          //   left only - offset from left edge, preferred width
          //   right only - offset from right edge, preferred width
          //   left and right - offset from left and right edges, width in between this
          //   neither - no offset, full width of stack
          // Vertical direction is similar.
          //
          // Margins on the child are also included in the edge offsets
          if (offsetSpecified) {
            if (offsetSpecified & SPECIFIED_LEFT) {
              childRect.x = clientRect.x + offset.left + margin.left;
              if (offsetSpecified & SPECIFIED_RIGHT) {
                nsSize min = child->GetMinSize(aState);
                nsSize max = child->GetMaxSize(aState);
                nscoord width = clientRect.width - offset.LeftRight() - margin.LeftRight();
                childRect.width = NS_MAX(min.width, NS_MIN(max.width, width));
              }
              else {
                childRect.width = child->GetPrefSize(aState).width;
              }
            }
            else if (offsetSpecified & SPECIFIED_RIGHT) {
              childRect.width = child->GetPrefSize(aState).width;
              childRect.x = clientRect.XMost() - offset.right - margin.right - childRect.width;
            }

            if (offsetSpecified & SPECIFIED_TOP) {
              childRect.y = clientRect.y + offset.top + margin.top;
              if (offsetSpecified & SPECIFIED_BOTTOM) {
                nsSize min = child->GetMinSize(aState);
                nsSize max = child->GetMaxSize(aState);
                nscoord height = clientRect.height - offset.TopBottom() - margin.TopBottom();
                childRect.height = NS_MAX(min.height, NS_MIN(max.height, height));
              }
              else {
                childRect.height = child->GetPrefSize(aState).height;
              }
            }
            else if (offsetSpecified & SPECIFIED_BOTTOM) {
              childRect.height = child->GetPrefSize(aState).height;
              childRect.y = clientRect.YMost() - offset.bottom - margin.bottom - childRect.height;
            }
          }

          // Now place the child.
          child->SetBounds(aState, childRect);

          // Flow the child.
          child->Layout(aState);

          // Get the child's new rect.
          nsRect childRectNoMargin;
          childRectNoMargin = childRect = child->GetRect();
          childRect.Inflate(margin);

          if (child->GetStyleXUL()->mStretchStack) {
            // Did the child push back on us and get bigger?
            if (offset.LeftRight() + childRect.width > clientRect.width) {
              clientRect.width = childRect.width + offset.LeftRight();
              grow = PR_TRUE;
            }

            if (offset.TopBottom() + childRect.height > clientRect.height) {
              clientRect.height = childRect.height + offset.TopBottom();
              grow = PR_TRUE;
            }
          }

          if (!childRectNoMargin.IsEqualInterior(oldRect))
          {
            // redraw the new and old positions if the 
            // child moved or resized.
            // if the new and old rect intersect meaning we just moved a little
            // then just redraw the union. If they don't intersect (meaning
            // we moved a good distance) redraw both separately.
            if (childRectNoMargin.Intersects(oldRect)) {
              nsRect u;
              u.UnionRect(oldRect, childRectNoMargin);
              aBox->Redraw(aState, &u);
            } else {
              aBox->Redraw(aState, &oldRect);
              aBox->Redraw(aState, &childRectNoMargin);
            }
          }
       }

       child = child->GetNextBox();
     }
   } while (grow);
   
   // if some HTML inside us got bigger we need to force ourselves to
   // get bigger
   nsRect bounds(aBox->GetRect());
   nsMargin bp;
   aBox->GetBorderAndPadding(bp);
   clientRect.Inflate(bp);

   if (clientRect.width > bounds.width || clientRect.height > bounds.height)
   {
     if (clientRect.width > bounds.width)
       bounds.width = clientRect.width;
     if (clientRect.height > bounds.height)
       bounds.height = clientRect.height;

     aBox->SetBounds(aState, bounds);
   }

   return NS_OK;
}

