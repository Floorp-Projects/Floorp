/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBoxObject.h"
#include "nsIBoxLayoutManager.h"
#include "nsIBoxPaintManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsReadableUtils.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMClassInfo.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsIDOMXULElement.h"
#include "nsIBox.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods

// QueryInterface implementation for nsBoxObject
NS_INTERFACE_MAP_BEGIN(nsBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsPIBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(BoxObject)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsBoxObject)
NS_IMPL_RELEASE(nsBoxObject)


// Constructors/Destructors
nsBoxObject::nsBoxObject(void)
  :mContent(nsnull), mPresShell(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsBoxObject::~nsBoxObject(void)
{
}

NS_IMETHODIMP
nsBoxObject::GetLayoutManager(nsIBoxLayoutManager** aResult)
{
  *aResult = mLayoutManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetLayoutManager(nsIBoxLayoutManager* aLayoutManager)
{
  mLayoutManager = aLayoutManager;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPaintManager(nsIBoxPaintManager** aResult)
{
  *aResult = mPaintManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetPaintManager(nsIBoxPaintManager* aPaintManager)
{
  mPaintManager = aPaintManager;
  return NS_OK;
}

// nsPIBoxObject //////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsBoxObject::Init(nsIContent* aContent, nsIPresShell* aShell)
{
  mContent = aContent;
  mPresShell = aShell;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetDocument(nsIDocument* aDocument)
{
  mPresState = nsnull;
  if (aDocument) {
    nsCOMPtr<nsIPresShell> shell;
    aDocument->GetShellAt(0, getter_AddRefs(shell));
    mPresShell = shell;
  }
  else {
    mPresShell = nsnull;
  }
  return NS_OK;
}

nsIFrame*
nsBoxObject::GetFrame()
{
  nsIFrame* frame = nsnull;
  if (mPresShell)
    mPresShell->GetPrimaryFrameFor(mContent, &frame);
  return frame;
}

nsresult 
nsBoxObject::GetOffsetRect(nsRect& aRect)
{
  nsresult res = NS_OK;

  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));

  if (doc) {
    // Get Presentation shell 0
    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));

    if(presShell) {
      // Flush all pending notifications so that our frames are uptodate
      presShell->FlushPendingNotifications(PR_FALSE);

      // Get the Frame for our content
      nsIFrame* frame = nsnull;
      presShell->GetPrimaryFrameFor(mContent, &frame);
      if(frame != nsnull) {
        // Get its origin
        nsPoint origin;
        frame->GetOrigin(origin);

        // Get the union of all rectangles in this and continuation frames
        nsRect rcFrame;
        nsIFrame* next = frame;
        do {
          nsRect rect;
          next->GetRect(rect);
          rcFrame.UnionRect(rcFrame, rect);
          next->GetNextInFlow(&next);
        } while (nsnull != next);
        

        // Find the frame parent whose content's tagName either matches 
        // the tagName passed in or is the document element.
        nsCOMPtr<nsIContent> docElement;
        doc->GetRootContent(getter_AddRefs(docElement));
        nsIFrame* parent = frame;
        nsCOMPtr<nsIContent> parentContent;
        frame->GetParent(&parent);
        while (parent) {
          parent->GetContent(getter_AddRefs(parentContent));
          if (parentContent) {
            // If we've hit the document element, break here
            if (parentContent.get() == docElement.get()) {
              break;
            }
          }

          // Add the parent's origin to our own to get to the
          // right coordinate system
          nsPoint parentOrigin;
          parent->GetOrigin(parentOrigin);
          origin += parentOrigin;

          parent->GetParent(&parent);
        }
  
        // For the origin, add in the border for the frame
        const nsStyleBorder* border;
        nsStyleCoord coord;
        frame->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border);
        if (border) {
          if (eStyleUnit_Coord == border->mBorder.GetLeftUnit()) {
            origin.x += border->mBorder.GetLeft(coord).GetCoordValue();
          }
          if (eStyleUnit_Coord == border->mBorder.GetTopUnit()) {
            origin.y += border->mBorder.GetTop(coord).GetCoordValue();
          }
        }

        // And subtract out the border for the parent
        if (parent) {
          const nsStyleBorder* parentBorder;
          parent->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)parentBorder);
          if (parentBorder) {
            if (eStyleUnit_Coord == parentBorder->mBorder.GetLeftUnit()) {
              origin.x -= parentBorder->mBorder.GetLeft(coord).GetCoordValue();
            }
            if (eStyleUnit_Coord == parentBorder->mBorder.GetTopUnit()) {
              origin.y -= parentBorder->mBorder.GetTop(coord).GetCoordValue();
            }
          }
        }

        // Get the Presentation Context from the Shell
        nsCOMPtr<nsIPresContext> context;
        presShell->GetPresContext(getter_AddRefs(context));
       
        if(context) {
          // Get the scale from that Presentation Context
          float scale;
          context->GetTwipsToPixels(&scale);
              
          // Convert to pixels using that scale
          aRect.x = NSTwipsToIntPixels(origin.x, scale);
          aRect.y = NSTwipsToIntPixels(origin.y, scale);
          aRect.width = NSTwipsToIntPixels(rcFrame.width, scale);
          aRect.height = NSTwipsToIntPixels(rcFrame.height, scale);
        }
      }
    }
  }
 
  return res;
}

nsresult
nsBoxObject::GetScreenRect(nsRect& aRect)
{
  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));

  if (doc) {
    // Get Presentation shell 0
    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));
    
    if (presShell) {
      // Flush all pending notifications so that our frames are uptodate
      presShell->FlushPendingNotifications(PR_FALSE);

      nsCOMPtr<nsIPresContext> presContext;
      presShell->GetPresContext(getter_AddRefs(presContext));
      
      if (presContext) {
        nsIFrame* frame;
        nsresult rv = presShell->GetPrimaryFrameFor(mContent, &frame);
        
        PRInt32 offsetX = 0;
        PRInt32 offsetY = 0;
        nsCOMPtr<nsIWidget> widget;
        
        while (frame) {
          // Look for a widget so we can get screen coordinates
          nsIView* view;
          rv = frame->GetView(presContext, &view);
          if (view) {
            rv = view->GetWidget(*getter_AddRefs(widget));
            if (widget)
              break;
          }
          
          // No widget yet, so count up the coordinates of the frame 
          nsPoint origin;
          frame->GetOrigin(origin);
          offsetX += origin.x;
          offsetY += origin.y;
      
          frame->GetParent(&frame);
        }
        
        if (widget) {
          // Get the scale from that Presentation Context
          float scale;
          presContext->GetTwipsToPixels(&scale);
          
          // Convert to pixels using that scale
          offsetX = NSTwipsToIntPixels(offsetX, scale);
          offsetY = NSTwipsToIntPixels(offsetY, scale);
          
          // Add the widget's screen coordinates to the offset we've counted
          nsRect oldBox(0,0,0,0);
          widget->WidgetToScreen(oldBox, aRect);
          aRect.x += offsetX;
          aRect.y += offsetY;
        }
      }
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetX(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.x;
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetY(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.y;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetWidth(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.width;
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetHeight(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetScreenX(PRInt32 *_retval)
{
  nsRect rect;
  nsresult rv = GetScreenRect(rect);
  if (NS_FAILED(rv)) return rv;
  
  *_retval = rect.x;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetScreenY(PRInt32 *_retval)
{
  nsRect rect;
  nsresult rv = GetScreenRect(rect);
  if (NS_FAILED(rv)) return rv;
  
  *_retval = rect.y;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetLookAndFeelMetric(const PRUnichar* aPropertyName, 
                                  PRUnichar** aResult)
{
  nsCOMPtr<nsILookAndFeel> lookAndFeel(do_GetService(kLookAndFeelCID));
  if (!lookAndFeel)
    return NS_ERROR_FAILURE;
    
  nsAutoString property(aPropertyName);
  if (property.EqualsIgnoreCase("scrollbarStyle")) {
    PRInt32 metricResult;
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ScrollArrowStyle, metricResult);
    switch (metricResult) {
      case nsILookAndFeel::eMetric_ScrollArrowStyleBothAtBottom:
        *aResult = ToNewUnicode(NS_LITERAL_STRING("doublebottom"));
        break;
      case nsILookAndFeel::eMetric_ScrollArrowStyleBothAtEachEnd:
        *aResult = ToNewUnicode(NS_LITERAL_STRING("double"));
        break;
      case nsILookAndFeel::eMetric_ScrollArrowStyleBothAtTop:
        *aResult = ToNewUnicode(NS_LITERAL_STRING("doubletop"));
        break;
      default:
        *aResult = ToNewUnicode(NS_LITERAL_STRING("single"));
        break;   
    } 
  }
  else if (property.EqualsIgnoreCase("thumbStyle")) {
    PRInt32 metricResult;
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ScrollSliderStyle, metricResult);
    if ( metricResult == nsILookAndFeel::eMetric_ScrollThumbStyleNormal )
      *aResult = ToNewUnicode(NS_LITERAL_STRING("fixed"));
    else
      *aResult = ToNewUnicode(NS_LITERAL_STRING("proportional"));   
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports** aResult)
{
  if (!mPresState) {
    *aResult = nsnull;
    return NS_OK;
  }

  nsAutoString propertyName(aPropertyName);
  return mPresState->GetStatePropertyAsSupports(propertyName, aResult); // Addref here.
}

NS_IMETHODIMP
nsBoxObject::SetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports* aValue)
{
  if (!mPresState)
    NS_NewPresState(getter_AddRefs(mPresState));

  nsAutoString propertyName(aPropertyName);
  return mPresState->SetStatePropertyAsSupports(propertyName, aValue);
}

NS_IMETHODIMP
nsBoxObject::GetProperty(const PRUnichar* aPropertyName, PRUnichar** aResult)
{
  if (!mPresState) {
    *aResult = nsnull;
    return NS_OK;
  }

  nsAutoString propertyName(aPropertyName);
  nsAutoString result;
  nsresult rv = mPresState->GetStateProperty(propertyName, result);
  if (NS_FAILED(rv))
    return rv;
  *aResult = ToNewUnicode(result);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetProperty(const PRUnichar* aPropertyName, const PRUnichar* aPropertyValue)
{
  if (!mPresState)
    NS_NewPresState(getter_AddRefs(mPresState));

  nsAutoString propertyName(aPropertyName);
  nsAutoString propertyValue(aPropertyValue);
  return mPresState->SetStateProperty(propertyName, propertyValue);
}

NS_IMETHODIMP
nsBoxObject::RemoveProperty(const PRUnichar* aPropertyName)
{
  if (!mPresState)
    return NS_OK;

  nsAutoString propertyName(aPropertyName);
  return mPresState->RemoveStateProperty(propertyName);
}

NS_IMETHODIMP 
nsBoxObject::GetParentBox(nsIDOMElement * *aParentBox)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;
  nsIFrame* parent;
  frame->GetParent(&parent);
  if (!parent) return NS_OK;

  nsCOMPtr<nsIContent> content;
  parent->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(content);
  *aParentBox = el;
  NS_IF_ADDREF(*aParentBox);
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetFirstChild(nsIDOMElement * *aFirstVisibleChild)
{
  *aFirstVisibleChild = GetChildByOrdinalAt(0);
  NS_IF_ADDREF(*aFirstVisibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetLastChild(nsIDOMElement * *aLastVisibleChild)
{
  PRInt32 count;
  mContent->ChildCount(count);
  *aLastVisibleChild = GetChildByOrdinalAt(count-1);
  NS_IF_ADDREF(*aLastVisibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetNextSibling(nsIDOMElement **aNextOrdinalSibling)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;
  nsIFrame* nextFrame;
  frame->GetNextSibling(&nextFrame);
  if (!nextFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIContent> nextContent;
  nextFrame->GetContent(getter_AddRefs(nextContent));
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(nextContent);
  *aNextOrdinalSibling = el;
  NS_IF_ADDREF(*aNextOrdinalSibling);

  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPreviousSibling(nsIDOMElement **aPreviousOrdinalSibling)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;
  nsIFrame* parentFrame;
  frame->GetParent(&parentFrame);
  if (!parentFrame) return NS_OK;
  
  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));

  nsIFrame* nextFrame;
  parentFrame->FirstChild(presContext, nsnull, &nextFrame);
  nsIFrame* prevFrame = nsnull;
  while (nextFrame) {
    if (nextFrame == frame)
      break;
    prevFrame = nextFrame;
    nextFrame->GetNextSibling(&nextFrame);
  }
   
  if (!prevFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIContent> nextContent;
  prevFrame->GetContent(getter_AddRefs(nextContent));
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(nextContent);
  *aPreviousOrdinalSibling = el;
  NS_IF_ADDREF(*aPreviousOrdinalSibling);

  return NS_OK;
}

nsIDOMElement*
nsBoxObject::GetChildByOrdinalAt(PRUint32 aIndex)
{
  // cast our way down to our nsContainerBox interface
  nsIFrame* frame = GetFrame();
  if (!frame) return nsnull;
  
  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));

  // get the first child box
  nsIFrame* childFrame;
  frame->FirstChild(presContext, nsnull, &childFrame);
  
  PRUint32 i = 0;
  while (childFrame && i < aIndex) {
    childFrame->GetNextSibling(&childFrame);
    ++i;
  }

  if (!childFrame) return nsnull;

  // get the content for the box and query to a dom element
  nsCOMPtr<nsIContent> content;
  childFrame->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(content);
  
  return el;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

