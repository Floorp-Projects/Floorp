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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsBoxObject.h"
#include "nsIBoxLayoutManager.h"
#include "nsIBoxPaintManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIFrameFrame.h"
#include "nsIDocShell.h"
#include "nsReadableUtils.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMClassInfo.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsIDOMXULElement.h"
#include "nsIFrame.h"

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
}

nsBoxObject::~nsBoxObject(void)
{
}

NS_IMETHODIMP
nsBoxObject::GetElement(nsIDOMElement** aResult)
{
  if (mContent)
    mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aResult);
  else
    *aResult = nsnull;
  return NS_OK;
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
    mPresShell = aDocument->GetShellAt(0);
  }
  else {
    mPresShell = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::InvalidatePresentationStuff()
{
  mPresShell = nsnull;

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
  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  if (!mContent)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  if (doc) {
    // Get Presentation shell 0
    nsIPresShell *presShell = doc->GetShellAt(0);

    if(presShell) {
      // Flush all pending notifications so that our frames are uptodate
      doc->FlushPendingNotifications(Flush_Layout);

      // Get the Frame for our content
      nsIFrame* frame = nsnull;
      presShell->GetPrimaryFrameFor(mContent, &frame);
      if(frame != nsnull) {
        // Get its origin
        nsPoint origin = frame->GetPosition();

        // Get the union of all rectangles in this and continuation frames
        nsRect rcFrame;
        nsIFrame* next = frame;
        do {
          rcFrame.UnionRect(rcFrame, next->GetRect());
          next = next->GetNextInFlow();
        } while (nsnull != next);
        

        // Find the frame parent whose content's tagName either matches 
        // the tagName passed in or is the document element.
        nsIContent *docElement = doc->GetRootContent();
        nsIFrame* parent = frame->GetParent();
        while (parent) {
          // If we've hit the document element, break here
          if (parent->GetContent() == docElement) {
            break;
          }

          // Add the parent's origin to our own to get to the
          // right coordinate system
          origin += parent->GetPosition();

          parent = parent->GetParent();
        }
  
        // For the origin, add in the border for the frame
        nsStyleCoord coord;
        const nsStyleBorder* border = frame->GetStyleBorder();
        if (eStyleUnit_Coord == border->mBorder.GetLeftUnit()) {
          origin.x += border->mBorder.GetLeft(coord).GetCoordValue();
        }
        if (eStyleUnit_Coord == border->mBorder.GetTopUnit()) {
          origin.y += border->mBorder.GetTop(coord).GetCoordValue();
        }

        // And subtract out the border for the parent
        if (parent) {
          const nsStyleBorder* parentBorder = parent->GetStyleBorder();
          if (eStyleUnit_Coord == parentBorder->mBorder.GetLeftUnit()) {
            origin.x -= parentBorder->mBorder.GetLeft(coord).GetCoordValue();
          }
          if (eStyleUnit_Coord == parentBorder->mBorder.GetTopUnit()) {
            origin.y -= parentBorder->mBorder.GetTop(coord).GetCoordValue();
          }
        }

        // Get the Presentation Context from the Shell
        nsPresContext *context = presShell->GetPresContext();
        if (context) {
          // Get the scale from that Presentation Context
          float scale;
          scale = context->TwipsToPixels();
              
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
 
  if (!mContent)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  if (doc) {
    // Get Presentation shell 0
    nsIPresShell *presShell = doc->GetShellAt(0);

    if (presShell) {
      // Flush all pending notifications so that our frames are uptodate
      doc->FlushPendingNotifications(Flush_Layout);

      nsPresContext *presContext = presShell->GetPresContext();
      if (presContext) {
        nsIFrame* frame;
        presShell->GetPrimaryFrameFor(mContent, &frame);
        
        PRInt32 offsetX = 0;
        PRInt32 offsetY = 0;
        nsIWidget* widget = nsnull;
        
        while (frame) {
          // Look for a widget so we can get screen coordinates
          if (frame->HasView()) {
            widget = frame->GetView()->GetWidget();
            if (widget)
              break;
          }
          
          // No widget yet, so count up the coordinates of the frame 
          nsPoint origin = frame->GetPosition();
          offsetX += origin.x;
          offsetY += origin.y;
      
          frame = frame->GetParent();
        }
        
        if (widget) {
          // Get the scale from that Presentation Context
          float scale;
          scale = presContext->TwipsToPixels();
          
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
  if (property.LowerCaseEqualsLiteral("scrollbarstyle")) {
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
  else if (property.LowerCaseEqualsLiteral("thumbstyle")) {
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

  nsDependentString propertyName(aPropertyName);
  return mPresState->GetStatePropertyAsSupports(propertyName, aResult); // Addref here.
}

NS_IMETHODIMP
nsBoxObject::SetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports* aValue)
{
  if (!mPresState)
    NS_NewPresState(getter_AddRefs(mPresState));

  nsDependentString propertyName(aPropertyName);
  return mPresState->SetStatePropertyAsSupports(propertyName, aValue);
}

NS_IMETHODIMP
nsBoxObject::GetProperty(const PRUnichar* aPropertyName, PRUnichar** aResult)
{
  if (!mPresState) {
    *aResult = nsnull;
    return NS_OK;
  }

  nsDependentString propertyName(aPropertyName);
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

  nsDependentString propertyName(aPropertyName);
  nsDependentString propertyValue(aPropertyValue);
  return mPresState->SetStateProperty(propertyName, propertyValue);
}

NS_IMETHODIMP
nsBoxObject::RemoveProperty(const PRUnichar* aPropertyName)
{
  if (!mPresState)
    return NS_OK;

  nsDependentString propertyName(aPropertyName);
  return mPresState->RemoveStateProperty(propertyName);
}

NS_IMETHODIMP 
nsBoxObject::GetParentBox(nsIDOMElement * *aParentBox)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;
  nsIFrame* parent = frame->GetParent();
  if (!parent) return NS_OK;

  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(parent->GetContent());
  *aParentBox = el;
  NS_IF_ADDREF(*aParentBox);
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetFirstChild(nsIDOMElement * *aFirstVisibleChild)
{
  if (!mContent) {
    *aFirstVisibleChild = nsnull;
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aFirstVisibleChild = GetChildByOrdinalAt(0);
  NS_IF_ADDREF(*aFirstVisibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetLastChild(nsIDOMElement * *aLastVisibleChild)
{
  if (!mContent) {
    *aLastVisibleChild = nsnull;
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRUint32 count = mContent->GetChildCount();

  if (count > 0) {
    NS_IF_ADDREF(*aLastVisibleChild = GetChildByOrdinalAt(count-1));
  } else {
    *aLastVisibleChild = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetNextSibling(nsIDOMElement **aNextOrdinalSibling)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;
  nsIFrame* nextFrame = frame->GetNextSibling();
  if (!nextFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(nextFrame->GetContent());
  *aNextOrdinalSibling = el;
  NS_IF_ADDREF(*aNextOrdinalSibling);

  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPreviousSibling(nsIDOMElement **aPreviousOrdinalSibling)
{
  nsIFrame* frame = GetFrame();
  if (!frame) return NS_OK;
  nsIFrame* parentFrame = frame->GetParent();
  if (!parentFrame) return NS_OK;
  
  nsIFrame* nextFrame = parentFrame->GetFirstChild(nsnull);
  nsIFrame* prevFrame = nsnull;
  while (nextFrame) {
    if (nextFrame == frame)
      break;
    prevFrame = nextFrame;
    nextFrame = nextFrame->GetNextSibling();
  }
   
  if (!prevFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(prevFrame->GetContent());
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
  
  // get the first child box
  nsIFrame* childFrame = frame->GetFirstChild(nsnull);
  
  PRUint32 i = 0;
  while (childFrame && i < aIndex) {
    childFrame = childFrame->GetNextSibling();
    ++i;
  }

  if (!childFrame) return nsnull;

  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(childFrame->GetContent());
  
  return el;
}

nsresult
nsBoxObject::GetDocShell(nsIDocShell** aResult)
{
  *aResult = nsnull;

  if (!mPresShell) {
    return NS_OK;
  }

  nsIFrame *frame = GetFrame();

  if (frame) {
    nsIFrameFrame *frame_frame = nsnull;
    CallQueryInterface(frame, &frame_frame);

    if (frame_frame) {
      // Ok, the frame for mContent is a nsIFrameFrame, it knows how
      // to reach the docshell, so ask it...

      return frame_frame->GetDocShell(aResult);
    }
  }

  // No nsIFrameFrame available for mContent, try if there's a mapping
  // between mContent's document to mContent's subdocument.

  nsIDocument *sub_doc =
    mPresShell->GetDocument()->GetSubDocumentFor(mContent);

  if (!sub_doc) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> container = sub_doc->GetContainer();

  if (!container) {
    return NS_OK;
  }

  return CallQueryInterface(container, aResult);
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

