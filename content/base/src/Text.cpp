/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Text.h"
#include "nsTextNode.h"

namespace mozilla {
namespace dom {

already_AddRefed<Text>
Text::SplitText(uint32_t aOffset, ErrorResult& rv)
{
  nsCOMPtr<nsIContent> newChild;
  rv = SplitData(aOffset, getter_AddRefs(newChild));
  if (rv.Failed()) {
    return nullptr;
  }
  return newChild.forget().downcast<Text>();
}

/* static */ already_AddRefed<Text>
Text::Constructor(const GlobalObject& aGlobal, const nsAString& aData,
                  ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.Get());
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return window->GetDoc()->CreateTextNode(aData);
}

} // namespace dom
} // namespace mozilla
