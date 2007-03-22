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
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
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
#include "nsLayoutUtils.h"
#include "nsISupportsPrimitives.h"
#include "prtypes.h"
#include "nsSupportsPrimitives.h"

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
  :mContent(nsnull)
{
}

nsBoxObject::~nsBoxObject(void)
{
}

NS_IMETHODIMP
nsBoxObject::GetElement(nsIDOMElement** aResult)
{
  if (mContent) {
    return CallQueryInterface(mContent, aResult);
  }

  *aResult = nsnull;
  return NS_OK;
}

// nsPIBoxObject //////////////////////////////////////////////////////////////////////////

nsresult
nsBoxObject::Init(nsIContent* aContent)
{
  mContent = aContent;
  return NS_OK;
}

void
nsBoxObject::Clear()
{
  mPropertyTable = nsnull;
  mContent = nsnull;
}

void
nsBoxObject::ClearCachedValues()
{
}

nsIFrame*
nsBoxObject::GetFrame(PRBool aFlushLayout)
{
  nsIPresShell* shell = GetPresShell(aFlushLayout);
  if (!shell)
    return nsnull;

  if (!aFlushLayout) {
    // If we didn't flush layout when getting the presshell, we should at least
    // flush to make sure our frame model is up to date.
    // XXXbz should flush on document, no?  Except people call this from
    // frame code, maybe?
    shell->FlushPendingNotifications(Flush_Frames);
  }

  return shell->GetPrimaryFrameFor(mContent);
}

nsIPresShell*
nsBoxObject::GetPresShell(PRBool aFlushLayout)
{
  if (!mContent) {
    return nsnull;
  }

  nsIDocument* doc = mContent->GetCurrentDoc();
  if (!doc) {
    return nsnull;
  }

  if (aFlushLayout) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  return doc->GetShellAt(0);
}

nsresult 
nsBoxObject::GetOffsetRect(nsRect& aRect)
{
  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  if (!mContent)
    return NS_ERROR_NOT_INITIALIZED;

  // Get the Frame for our content
  nsIFrame* frame = GetFrame(PR_TRUE);
  if (frame) {
    // Get its origin
    nsPoint origin = frame->GetPositionIgnoringScrolling();

    // Get the union of all rectangles in this and continuation frames
    nsRect rcFrame = nsLayoutUtils::GetAllInFlowBoundingRect(frame);
        
    // Find the frame parent whose content is the document element.
    nsIContent *docElement = mContent->GetCurrentDoc()->GetRootContent();
    nsIFrame* parent = frame->GetParent();
    for (;;) {
      // If we've hit the document element, break here
      if (parent->GetContent() == docElement) {
        break;
      }

      nsIFrame* next = parent->GetParent();
      if (!next) {
        NS_WARNING("We should have hit the document element...");
        origin += parent->GetPosition();
        break;
      }

      // Add the parent's origin to our own to get to the
      // right coordinate system
      origin += next->GetPositionOfChildIgnoringScrolling(parent);
      parent = next;
    }
  
    // For the origin, add in the border for the frame
    const nsStyleBorder* border = frame->GetStyleBorder();
    origin.x += border->GetBorderWidth(NS_SIDE_LEFT);
    origin.y += border->GetBorderWidth(NS_SIDE_TOP);

    // And subtract out the border for the parent
    const nsStyleBorder* parentBorder = parent->GetStyleBorder();
    origin.x -= parentBorder->GetBorderWidth(NS_SIDE_LEFT);
    origin.y -= parentBorder->GetBorderWidth(NS_SIDE_TOP);

    aRect.x = nsPresContext::AppUnitsToIntCSSPixels(origin.x);
    aRect.y = nsPresContext::AppUnitsToIntCSSPixels(origin.y);
    aRect.width = nsPresContext::AppUnitsToIntCSSPixels(rcFrame.width);
    aRect.height = nsPresContext::AppUnitsToIntCSSPixels(rcFrame.height);
  }
 
  return NS_OK;
}

nsresult
nsBoxObject::GetScreenPosition(nsIntPoint& aPoint)
{
  aPoint.x = aPoint.y = 0;
  
  if (!mContent)
    return NS_ERROR_NOT_INITIALIZED;

  nsIFrame* frame = GetFrame(PR_TRUE);
  if (frame) {
    nsIntRect rect = frame->GetScreenRect();
    aPoint.x = rect.x;
    aPoint.y = rect.y;
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
  nsIntPoint position;
  nsresult rv = GetScreenPosition(position);
  if (NS_FAILED(rv)) return rv;
  
  *_retval = position.x;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetScreenY(PRInt32 *_retval)
{
  nsIntPoint position;
  nsresult rv = GetScreenPosition(position);
  if (NS_FAILED(rv)) return rv;
  
  *_retval = position.y;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetLookAndFeelMetric(const PRUnichar* aPropertyName, 
                                  PRUnichar** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsILookAndFeel> lookAndFeel(do_GetService(kLookAndFeelCID));
  if (!lookAndFeel)
    return NS_ERROR_FAILURE;
    
  nsAutoString property(aPropertyName);
  if (property.LowerCaseEqualsLiteral("scrollbararrows")) {
    PRInt32 metricResult;
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ScrollArrowStyle, metricResult);
    nsAutoString result;
    if (metricResult & nsILookAndFeel::eMetric_ScrollArrowStartBackward) {
      result.AppendLiteral("start-backward ");
    }
    if (metricResult & nsILookAndFeel::eMetric_ScrollArrowStartForward) {
      result.AppendLiteral("start-forward ");
    }
    if (metricResult & nsILookAndFeel::eMetric_ScrollArrowEndBackward) {
      result.AppendLiteral("end-backward ");
    }
    if (metricResult & nsILookAndFeel::eMetric_ScrollArrowEndForward) {
      result.AppendLiteral("end-forward");
    }
    *aResult = ToNewUnicode(result);
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
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);
  if (!mPropertyTable) {
    *aResult = nsnull;
    return NS_OK;
  }
  nsDependentString propertyName(aPropertyName);
  mPropertyTable->Get(propertyName, aResult); // Addref here.
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports* aValue)
{
#ifdef DEBUG
  if (aValue) {
    nsIFrame* frame;
    CallQueryInterface(aValue, &frame);
    NS_ASSERTION(!frame,
                 "Calling SetPropertyAsSupports on a frame.  Prepare to crash "
                 "and be exploited any time some random website decides to "
                 "exploit you");
  }
#endif
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);
  
  if (!mPropertyTable) {  
    mPropertyTable = new nsInterfaceHashtable<nsStringHashKey,nsISupports>;  
    if (!mPropertyTable) return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(mPropertyTable->Init(8))) {
       mPropertyTable = nsnull;
       return NS_ERROR_FAILURE;
    }
  }

  nsDependentString propertyName(aPropertyName);
  if (!mPropertyTable->Put(propertyName, aValue))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetProperty(const PRUnichar* aPropertyName, PRUnichar** aResult)
{
  nsCOMPtr<nsISupports> data;
  nsresult rv = GetPropertyAsSupports(aPropertyName,getter_AddRefs(data));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!data) {
    *aResult = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsISupportsString> supportsStr = do_QueryInterface(data);
  if (!supportsStr) 
    return NS_ERROR_FAILURE;
  nsAutoString result;  
  supportsStr->GetData(result);

  *aResult = result.IsVoid() ? nsnull : ToNewUnicode(result);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetProperty(const PRUnichar* aPropertyName, const PRUnichar* aPropertyValue)
{
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);

  nsDependentString propertyName(aPropertyName);
  nsDependentString propertyValue;
  if (aPropertyValue) {
    propertyValue.Rebind(aPropertyValue);
  } else {
    propertyValue.SetIsVoid(PR_TRUE);
  }
  
  nsCOMPtr<nsISupportsString> supportsStr(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(supportsStr, NS_ERROR_OUT_OF_MEMORY);
  supportsStr->SetData(propertyValue);

  return SetPropertyAsSupports(aPropertyName,supportsStr);
}

NS_IMETHODIMP
nsBoxObject::RemoveProperty(const PRUnichar* aPropertyName)
{
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);

  if (!mPropertyTable) return NS_OK;

  nsDependentString propertyName(aPropertyName);
  mPropertyTable->Remove(propertyName);
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetParentBox(nsIDOMElement * *aParentBox)
{
  *aParentBox = nsnull;
  nsIFrame* frame = GetFrame(PR_FALSE);
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
  *aFirstVisibleChild = nsnull;
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame) return NS_OK;
  nsIFrame* firstFrame = frame->GetFirstChild(nsnull);
  if (!firstFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(firstFrame->GetContent());
  el.swap(*aFirstVisibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetLastChild(nsIDOMElement * *aLastVisibleChild)
{
  *aLastVisibleChild = nsnull;
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame) return NS_OK;
  return GetPreviousSibling(frame, nsnull, aLastVisibleChild);
}

NS_IMETHODIMP
nsBoxObject::GetNextSibling(nsIDOMElement **aNextOrdinalSibling)
{
  *aNextOrdinalSibling = nsnull;
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame) return NS_OK;
  nsIFrame* nextFrame = frame->GetNextSibling();
  if (!nextFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(nextFrame->GetContent());
  el.swap(*aNextOrdinalSibling);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPreviousSibling(nsIDOMElement **aPreviousOrdinalSibling)
{
  *aPreviousOrdinalSibling = nsnull;
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame) return NS_OK;
  nsIFrame* parentFrame = frame->GetParent();
  if (!parentFrame) return NS_OK;
  return GetPreviousSibling(parentFrame, frame, aPreviousOrdinalSibling);
}

nsresult
nsBoxObject::GetPreviousSibling(nsIFrame* aParentFrame, nsIFrame* aFrame,
                                nsIDOMElement** aResult)
{
  *aResult = nsnull;
  nsIFrame* nextFrame = aParentFrame->GetFirstChild(nsnull);
  nsIFrame* prevFrame = nsnull;
  while (nextFrame) {
    if (nextFrame == aFrame)
      break;
    prevFrame = nextFrame;
    nextFrame = nextFrame->GetNextSibling();
  }
   
  if (!prevFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(prevFrame->GetContent());
  el.swap(*aResult);
  return NS_OK;
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

