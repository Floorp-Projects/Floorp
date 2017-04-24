/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ListBoxObject.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsListBoxBodyFrame.h"
#include "ChildIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ListBoxObjectBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED(ListBoxObject, BoxObject, nsIListBoxObject,
                            nsPIListBoxObject)

ListBoxObject::ListBoxObject()
  : mListBoxBody(nullptr)
{
}

ListBoxObject::~ListBoxObject()
{
}

JSObject* ListBoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ListBoxObjectBinding::Wrap(aCx, this, aGivenProto);
}

// nsIListBoxObject
NS_IMETHODIMP
ListBoxObject::GetRowCount(int32_t *aResult)
{
  *aResult = GetRowCount();
  return NS_OK;
}

NS_IMETHODIMP
ListBoxObject::GetItemAtIndex(int32_t index, nsIDOMElement **_retval)
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    return body->GetItemAtIndex(index, _retval);
  }
  return NS_OK;
 }

NS_IMETHODIMP
ListBoxObject::GetIndexOfItem(nsIDOMElement* aElement, int32_t *aResult)
{
  *aResult = 0;

  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    return body->GetIndexOfItem(aElement, aResult);
  }
  return NS_OK;
}

// ListBoxObject

int32_t
ListBoxObject::GetRowCount()
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    return body->GetRowCount();
  }
  return 0;
}

int32_t
ListBoxObject::GetRowHeight()
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    return body->GetRowHeightPixels();
  }
  return 0;
}

int32_t
ListBoxObject::GetNumberOfVisibleRows()
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    return body->GetNumberOfVisibleRows();
  }
  return 0;
}

int32_t
ListBoxObject::GetIndexOfFirstVisibleRow()
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    return body->GetIndexOfFirstVisibleRow();
  }
  return 0;
}

void
ListBoxObject::EnsureIndexIsVisible(int32_t aRowIndex)
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    body->EnsureIndexIsVisible(aRowIndex);
  }
}

void
ListBoxObject::ScrollToIndex(int32_t aRowIndex)
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    body->ScrollToIndex(aRowIndex);
  }
}

void
ListBoxObject::ScrollByLines(int32_t aNumLines)
{
  nsListBoxBodyFrame* body = GetListBoxBody(true);
  if (body) {
    body->ScrollByLines(aNumLines);
  }
}

already_AddRefed<Element>
ListBoxObject::GetItemAtIndex(int32_t index)
{
  nsCOMPtr<nsIDOMElement> el;
  GetItemAtIndex(index, getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

int32_t
ListBoxObject::GetIndexOfItem(Element& aElement)
{
  int32_t ret;
  nsCOMPtr<nsIDOMElement> el(do_QueryInterface(&aElement));
  GetIndexOfItem(el, &ret);
  return ret;
}

//////////////////////

static nsIContent*
FindBodyContent(nsIContent* aParent)
{
  if (aParent->IsXULElement(nsGkAtoms::listboxbody)) {
    return aParent;
  }

  mozilla::dom::FlattenedChildIterator iter(aParent);
  for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
    nsIContent* result = FindBodyContent(child);
    if (result) {
      return result;
    }
  }

  return nullptr;
}

nsListBoxBodyFrame*
ListBoxObject::GetListBoxBody(bool aFlush)
{
  if (mListBoxBody) {
    return mListBoxBody;
  }

  nsIPresShell* shell = GetPresShell(false);
  if (!shell) {
    return nullptr;
  }

  nsIFrame* frame = aFlush ?
                      GetFrame(false) /* does FlushType::Frames */ :
                      mContent->GetPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> content = FindBodyContent(frame->GetContent());

  if (!content) {
    return nullptr;
  }

  // this frame will be a nsGFXScrollFrame
  frame = content->GetPrimaryFrame();
  if (!frame) {
     return nullptr;
  }

  nsIScrollableFrame* scrollFrame = do_QueryFrame(frame);
  if (!scrollFrame) {
    return nullptr;
  }

  // this frame will be the one we want
  nsIFrame* yeahBaby = scrollFrame->GetScrolledFrame();
  if (!yeahBaby) {
     return nullptr;
  }

  // It's a frame. Refcounts are irrelevant.
  nsListBoxBodyFrame* listBoxBody = do_QueryFrame(yeahBaby);
  NS_ENSURE_TRUE(listBoxBody &&
                 listBoxBody->SetBoxObject(this),
                 nullptr);
  mListBoxBody = listBoxBody;
  return mListBoxBody;
}

void
ListBoxObject::Clear()
{
  ClearCachedValues();
  BoxObject::Clear();
}

void
ListBoxObject::ClearCachedValues()
{
  mListBoxBody = nullptr;
}

} // namespace dom
} // namespace mozilla

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewListBoxObject(nsIBoxObject** aResult)
{
  NS_ADDREF(*aResult = new mozilla::dom::ListBoxObject());
  return NS_OK;
}
