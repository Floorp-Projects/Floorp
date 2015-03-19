/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContainerBoxObject.h"
#include "mozilla/dom/ContainerBoxObjectBinding.h"
#include "nsCOMPtr.h"
#include "nsIDocShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsSubDocumentFrame.h"

namespace mozilla {
namespace dom {
  
ContainerBoxObject::ContainerBoxObject()
{
}

ContainerBoxObject::~ContainerBoxObject()
{
}

JSObject*
ContainerBoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ContainerBoxObjectBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsIDocShell>
ContainerBoxObject::GetDocShell()
{
  nsSubDocumentFrame *subDocFrame = do_QueryFrame(GetFrame(false));
  if (subDocFrame) {
    // Ok, the frame for mContent is an nsSubDocumentFrame, it knows how
    // to reach the docshell, so ask it...
    nsCOMPtr<nsIDocShell> ret;
    subDocFrame->GetDocShell(getter_AddRefs(ret));
    return ret.forget();
  }

  if (!mContent) {
    return nullptr;
  }

  // No nsSubDocumentFrame available for mContent, try if there's a mapping
  // between mContent's document to mContent's subdocument.

  nsIDocument *doc = mContent->GetComposedDoc();

  if (!doc) {
    return nullptr;
  }

  nsIDocument *sub_doc = doc->GetSubDocumentFor(mContent);

  if (!sub_doc) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> result = sub_doc->GetDocShell();
  return result.forget();
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewContainerBoxObject(nsIBoxObject** aResult)
{
  NS_ADDREF(*aResult = new mozilla::dom::ContainerBoxObject());
  return NS_OK;
}
