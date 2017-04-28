/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BoxObject.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsContainerFrame.h"
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
#include "mozilla/dom/BoxObjectBinding.h"

// Implementation /////////////////////////////////////////////////////////////////

namespace mozilla {
namespace dom {

// Static member variable initialization

// Implement our nsISupports methods
NS_IMPL_CYCLE_COLLECTION_CLASS(BoxObject)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BoxObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BoxObject)

// QueryInterface implementation for BoxObject
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BoxObject)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsPIBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BoxObject)
  // XXX jmorton: why aren't we unlinking mPropertyTable?
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BoxObject)
  if (tmp->mPropertyTable) {
    for (auto iter = tmp->mPropertyTable->Iter(); !iter.Done(); iter.Next()) {
      cb.NoteXPCOMChild(iter.UserData());
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BoxObject)

// Constructors/Destructors
BoxObject::BoxObject()
  : mContent(nullptr)
{
}

BoxObject::~BoxObject()
{
}

NS_IMETHODIMP
BoxObject::GetElement(nsIDOMElement** aResult)
{
  if (mContent) {
    return CallQueryInterface(mContent, aResult);
  }

  *aResult = nullptr;
  return NS_OK;
}

// nsPIBoxObject //////////////////////////////////////////////////////////////////////////

nsresult
BoxObject::Init(nsIContent* aContent)
{
  mContent = aContent;
  return NS_OK;
}

void
BoxObject::Clear()
{
  mPropertyTable = nullptr;
  mContent = nullptr;
}

void
BoxObject::ClearCachedValues()
{
}

nsIFrame*
BoxObject::GetFrame(bool aFlushLayout)
{
  nsIPresShell* shell = GetPresShell(aFlushLayout);
  if (!shell)
    return nullptr;

  if (!aFlushLayout) {
    // If we didn't flush layout when getting the presshell, we should at least
    // flush to make sure our frame model is up to date.
    // XXXbz should flush on document, no?  Except people call this from
    // frame code, maybe?
    shell->FlushPendingNotifications(FlushType::Frames);
  }

  // The flush might have killed mContent.
  if (!mContent) {
    return nullptr;
  }

  return mContent->GetPrimaryFrame();
}

nsIPresShell*
BoxObject::GetPresShell(bool aFlushLayout)
{
  if (!mContent) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = mContent->GetUncomposedDoc();
  if (!doc) {
    return nullptr;
  }

  if (aFlushLayout) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  return doc->GetShell();
}

nsresult
BoxObject::GetOffsetRect(nsIntRect& aRect)
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
    Element* docElement = mContent->GetComposedDoc()->GetRootElement();
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
    origin.x += border->GetComputedBorderWidth(eSideLeft);
    origin.y += border->GetComputedBorderWidth(eSideTop);

    // And subtract out the border for the parent
    const nsStyleBorder* parentBorder = parent->StyleBorder();
    origin.x -= parentBorder->GetComputedBorderWidth(eSideLeft);
    origin.y -= parentBorder->GetComputedBorderWidth(eSideTop);

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
BoxObject::GetScreenPosition(nsIntPoint& aPoint)
{
  aPoint.x = aPoint.y = 0;

  if (!mContent)
    return NS_ERROR_NOT_INITIALIZED;

  nsIFrame* frame = GetFrame(true);
  if (frame) {
    CSSIntRect rect = frame->GetScreenRect();
    aPoint.x = rect.x;
    aPoint.y = rect.y;
  }

  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetX(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.x;
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetY(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.y;
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetWidth(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.width;
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetHeight(int32_t* aResult)
{
  nsIntRect rect;
  GetOffsetRect(rect);
  *aResult = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetScreenX(int32_t *_retval)
{
  nsIntPoint position;
  nsresult rv = GetScreenPosition(position);
  if (NS_FAILED(rv)) return rv;
  *_retval = position.x;
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetScreenY(int32_t *_retval)
{
  nsIntPoint position;
  nsresult rv = GetScreenPosition(position);
  if (NS_FAILED(rv)) return rv;
  *_retval = position.y;
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetPropertyAsSupports(const char16_t* aPropertyName, nsISupports** aResult)
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
BoxObject::SetPropertyAsSupports(const char16_t* aPropertyName, nsISupports* aValue)
{
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);

  if (!mPropertyTable) {
    mPropertyTable = new nsInterfaceHashtable<nsStringHashKey,nsISupports>(4);
  }

  nsDependentString propertyName(aPropertyName);
  mPropertyTable->Put(propertyName, aValue);
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetProperty(const char16_t* aPropertyName, char16_t** aResult)
{
  nsCOMPtr<nsISupports> data;
  nsresult rv = GetPropertyAsSupports(aPropertyName,getter_AddRefs(data));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!data) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsISupportsString> supportsStr = do_QueryInterface(data);
  if (!supportsStr) {
    return NS_ERROR_FAILURE;
  }

  return supportsStr->ToString(aResult);
}

NS_IMETHODIMP
BoxObject::SetProperty(const char16_t* aPropertyName, const char16_t* aPropertyValue)
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
BoxObject::RemoveProperty(const char16_t* aPropertyName)
{
  NS_ENSURE_ARG(aPropertyName && *aPropertyName);

  if (!mPropertyTable) return NS_OK;

  nsDependentString propertyName(aPropertyName);
  mPropertyTable->Remove(propertyName);
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetParentBox(nsIDOMElement * *aParentBox)
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
BoxObject::GetFirstChild(nsIDOMElement * *aFirstVisibleChild)
{
  *aFirstVisibleChild = nullptr;
  nsIFrame* frame = GetFrame(false);
  if (!frame) return NS_OK;
  nsIFrame* firstFrame = frame->PrincipalChildList().FirstChild();
  if (!firstFrame) return NS_OK;
  // get the content for the box and query to a dom element
  nsCOMPtr<nsIDOMElement> el = do_QueryInterface(firstFrame->GetContent());
  el.swap(*aFirstVisibleChild);
  return NS_OK;
}

NS_IMETHODIMP
BoxObject::GetLastChild(nsIDOMElement * *aLastVisibleChild)
{
  *aLastVisibleChild = nullptr;
  nsIFrame* frame = GetFrame(false);
  if (!frame) return NS_OK;
  return GetPreviousSibling(frame, nullptr, aLastVisibleChild);
}

NS_IMETHODIMP
BoxObject::GetNextSibling(nsIDOMElement **aNextOrdinalSibling)
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
BoxObject::GetPreviousSibling(nsIDOMElement **aPreviousOrdinalSibling)
{
  *aPreviousOrdinalSibling = nullptr;
  nsIFrame* frame = GetFrame(false);
  if (!frame) return NS_OK;
  nsIFrame* parentFrame = frame->GetParent();
  if (!parentFrame) return NS_OK;
  return GetPreviousSibling(parentFrame, frame, aPreviousOrdinalSibling);
}

nsresult
BoxObject::GetPreviousSibling(nsIFrame* aParentFrame, nsIFrame* aFrame,
                              nsIDOMElement** aResult)
{
  *aResult = nullptr;
  nsIFrame* nextFrame = aParentFrame->PrincipalChildList().FirstChild();
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

nsIContent*
BoxObject::GetParentObject() const
{
  return mContent;
}

JSObject*
BoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BoxObjectBinding::Wrap(aCx, this, aGivenProto);
}

Element*
BoxObject::GetElement() const
{
  return mContent && mContent->IsElement() ? mContent->AsElement() : nullptr;
}

int32_t
BoxObject::X()
{
  int32_t ret = 0;
  GetX(&ret);
  return ret;
}

int32_t
BoxObject::Y()
{
  int32_t ret = 0;
  GetY(&ret);
  return ret;
}

int32_t
BoxObject::GetScreenX(ErrorResult& aRv)
{
  int32_t ret = 0;
  aRv = GetScreenX(&ret);
  return ret;
}

int32_t
BoxObject::GetScreenY(ErrorResult& aRv)
{
  int32_t ret = 0;
  aRv = GetScreenY(&ret);
  return ret;
}

int32_t
BoxObject::Width()
{
  int32_t ret = 0;
  GetWidth(&ret);
  return ret;
}

int32_t
BoxObject::Height()
{
  int32_t ret = 0;
  GetHeight(&ret);
  return ret;
}

already_AddRefed<nsISupports>
BoxObject::GetPropertyAsSupports(const nsAString& propertyName)
{
  nsCOMPtr<nsISupports> ret;
  GetPropertyAsSupports(PromiseFlatString(propertyName).get(), getter_AddRefs(ret));
  return ret.forget();
}

void
BoxObject::SetPropertyAsSupports(const nsAString& propertyName, nsISupports* value)
{
  SetPropertyAsSupports(PromiseFlatString(propertyName).get(), value);
}

void
BoxObject::GetProperty(const nsAString& propertyName, nsString& aRetVal, ErrorResult& aRv)
{
  nsCOMPtr<nsISupports> data(GetPropertyAsSupports(propertyName));
  if (!data) {
    return;
  }

  nsCOMPtr<nsISupportsString> supportsStr(do_QueryInterface(data));
  if (!supportsStr) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  supportsStr->GetData(aRetVal);
}

void
BoxObject::SetProperty(const nsAString& propertyName, const nsAString& propertyValue)
{
  SetProperty(PromiseFlatString(propertyName).get(), PromiseFlatString(propertyValue).get());
}

void
BoxObject::RemoveProperty(const nsAString& propertyName)
{
  RemoveProperty(PromiseFlatString(propertyName).get());
}

already_AddRefed<Element>
BoxObject::GetParentBox()
{
  nsCOMPtr<nsIDOMElement> el;
  GetParentBox(getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

already_AddRefed<Element>
BoxObject::GetFirstChild()
{
  nsCOMPtr<nsIDOMElement> el;
  GetFirstChild(getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

already_AddRefed<Element>
BoxObject::GetLastChild()
{
  nsCOMPtr<nsIDOMElement> el;
  GetLastChild(getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

already_AddRefed<Element>
BoxObject::GetNextSibling()
{
  nsCOMPtr<nsIDOMElement> el;
  GetNextSibling(getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

already_AddRefed<Element>
BoxObject::GetPreviousSibling()
{
  nsCOMPtr<nsIDOMElement> el;
  GetPreviousSibling(getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

} // namespace dom
} // namespace mozilla

// Creation Routine ///////////////////////////////////////////////////////////////////////

using namespace mozilla::dom;

nsresult
NS_NewBoxObject(nsIBoxObject** aResult)
{
  NS_ADDREF(*aResult = new BoxObject());
  return NS_OK;
}
