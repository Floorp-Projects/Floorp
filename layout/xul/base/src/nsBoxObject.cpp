/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBoxObject.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsView.h"
#ifdef MOZ_XUL
#include "nsIDOMXULElement.h"
#else
#include "nsIDOMElement.h"
#endif
#include "nsLayoutUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsSupportsPrimitives.h"
#include "mozilla/dom/Element.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::dom;

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsBoxObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsBoxObject)

DOMCI_DATA(BoxObject, nsBoxObject)

// QueryInterface implementation for nsBoxObject
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsPIBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BoxObject)
NS_INTERFACE_MAP_END

static PLDHashOperator
PropertyTraverser(const nsAString& aKey, nsISupports* aProperty, void* userArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(userArg);

  cb->NoteXPCOMChild(aProperty);

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsBoxObject)

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsBoxObject)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsBoxObject)
  if (tmp->mPropertyTable) {
    tmp->mPropertyTable->EnumerateRead(PropertyTraverser, &cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// Constructors/Destructors
nsBoxObject::nsBoxObject(void)
  :mContent(nullptr)
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

  *aResult = nullptr;
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
  mPropertyTable = nullptr;
  mContent = nullptr;
}

void
nsBoxObject::ClearCachedValues()
{
}

nsIFrame*
nsBoxObject::GetFrame(bool aFlushLayout)
{
  nsIPresShell* shell = GetPresShell(aFlushLayout);
  if (!shell)
    return nullptr;

  if (!aFlushLayout) {
    // If we didn't flush layout when getting the presshell, we should at least
    // flush to make sure our frame model is up to date.
    // XXXbz should flush on document, no?  Except people call this from
    // frame code, maybe?
    shell->FlushPendingNotifications(Flush_Frames);
  }

  // The flush might have killed mContent.
  if (!mContent) {
    return nullptr;
  }

  return mContent->GetPrimaryFrame();
}

nsIPresShell*
nsBoxObject::GetPresShell(bool aFlushLayout)
{
  if (!mContent) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = mContent->GetCurrentDoc();
  if (!doc) {
    return nullptr;
  }

  if (aFlushLayout) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  return doc->GetShell();
}

nsresult 
nsBoxObject::GetOffsetRect(nsIntRect& aRect)
{
  aRect.SetRect(0, 0, 0, 0);
 
  if (!mContent)
    return NS_ERROR_NOT_INITIALIZED;

  // Get the Frame for our content
  nsIFrame* frame = GetFrame(true);
  if (frame) {
    // Get its origin
    nsPoint origin = frame->GetPositionIgnoringScrolling();

    // Find the frame parent whose content is the document element.
    Element *docElement = mContent->GetCurrentDoc()->GetRootElement();
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
    const nsStyleBorder* border = frame->StyleBorder();
    origin.x += border->GetComputedBorderWidth(NS_SIDE_LEFT);
    origin.y += border->GetComputedBorderWidth(NS_SIDE_TOP);

    // And subtract out the border for the parent
    const nsStyleBorder* parentBorder = parent->StyleBorder();
    origin.x -= parentBorder->GetComputedBorderWidth(NS_SIDE_LEFT);
    origin.y -= parentBorder->GetComputedBorderWidth(NS_SIDE_TOP);

    aRect.x = nsPresContext::AppUnitsToIntCSSPixels(origin.x);
    aRect.y = nsPresContext::AppUnitsToIntCSSPixels(origin.y);
    
    // Get the union of all rectangles in this and continuation frames.
    // It doesn't really matter what we use as aRelativeTo here, since
    // we only care about the size. Using 'parent' might make things
    // a bit faster by speeding up the internal GetOffsetTo operations.
    nsRect rcFrame = nsLayoutUtils::GetAllInFlowRectsUnion(frame, parent);
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

  nsIFrame* frame = GetFrame(true);
  if (frame) {
    nsIntRect rect = frame->GetScreenRect();
    aPoint.x = rect.x;
    aPoint.y = rect.y;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetX(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.x;
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetY(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.y;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetWidth(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.width;
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetHeight(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetScreenX(int32_t *_retval)
{
  nsIntPoint position;
  nsresult rv = GetScreenPosition(position);
  if (NS_FAILED(rv)) return rv;
  
  *_retval = position.x;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetScreenY(int32_t *_retval)
{
  nsIntPoint position;
  nsresult rv = GetScreenPosition(position);
  if (NS_FAILED(rv)) return rv;
  
  *_retval = position.y;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports** aResult)
{
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);
  if (!mPropertyTable) {
    *aResult = nullptr;
    return NS_OK;
  }
  nsDependentString propertyName(aPropertyName);
  mPropertyTable->Get(propertyName, aResult); // Addref here.
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports* aValue)
{
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);
  
  if (!mPropertyTable) {  
    mPropertyTable = new nsInterfaceHashtable<nsStringHashKey,nsISupports>(8);
  }

  nsDependentString propertyName(aPropertyName);
  mPropertyTable->Put(propertyName, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetProperty(const PRUnichar* aPropertyName, PRUnichar** aResult)
{
  nsCOMPtr<nsISupports> data;
  nsresult rv = GetPropertyAsSupports(aPropertyName,getter_AddRefs(data));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!data) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsISupportsString> supportsStr = do_QueryInterface(data);
  if (!supportsStr) 
    return NS_ERROR_FAILURE;
  
  return supportsStr->ToString(aResult);
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
    propertyValue.SetIsVoid(true);
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
  *aParentBox = nullptr;
  nsIFrame* frame = GetFrame(false);
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
  *aFirstVisibleChild = nullptr;
  nsIFrame* frame = GetFrame(false);
  if (!frame) return NS_OK;
  nsIFrame* firstFrame = frame->GetFirstPrincipalChild();
  if (!firstFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(firstFrame->GetContent());
  el.swap(*aFirstVisibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetLastChild(nsIDOMElement * *aLastVisibleChild)
{
  *aLastVisibleChild = nullptr;
  nsIFrame* frame = GetFrame(false);
  if (!frame) return NS_OK;
  return GetPreviousSibling(frame, nullptr, aLastVisibleChild);
}

NS_IMETHODIMP
nsBoxObject::GetNextSibling(nsIDOMElement **aNextOrdinalSibling)
{
  *aNextOrdinalSibling = nullptr;
  nsIFrame* frame = GetFrame(false);
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
  *aPreviousOrdinalSibling = nullptr;
  nsIFrame* frame = GetFrame(false);
  if (!frame) return NS_OK;
  nsIFrame* parentFrame = frame->GetParent();
  if (!parentFrame) return NS_OK;
  return GetPreviousSibling(parentFrame, frame, aPreviousOrdinalSibling);
}

nsresult
nsBoxObject::GetPreviousSibling(nsIFrame* aParentFrame, nsIFrame* aFrame,
                                nsIDOMElement** aResult)
{
  *aResult = nullptr;
  nsIFrame* nextFrame = aParentFrame->GetFirstPrincipalChild();
  nsIFrame* prevFrame = nullptr;
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

