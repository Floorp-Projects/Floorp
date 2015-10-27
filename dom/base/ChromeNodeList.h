/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"
#include "nsContentList.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class GlobalObject;

class ChromeNodeList final : public nsSimpleContentList
{
public:
  explicit ChromeNodeList(nsINode* aOwner)
  : nsSimpleContentList(aOwner)
  {
  }

  static already_AddRefed<ChromeNodeList>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void Append(nsINode& aNode, ErrorResult& aError);
  void Remove(nsINode& aNode, ErrorResult& aError);
};

} // namespace dom
} // namespace mozilla
