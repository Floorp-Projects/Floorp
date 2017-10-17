/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlexItem_h
#define mozilla_dom_FlexItem_h

#include "mozilla/dom/FlexBinding.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

struct ComputedFlexItemInfo;

namespace mozilla {
namespace dom {

class FlexLine;

class FlexItem : public nsISupports
               , public nsWrapperCache
{
public:
  explicit FlexItem(FlexLine* aParent,
                    const ComputedFlexItemInfo* aItem);

protected:
  virtual ~FlexItem() = default;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FlexItem)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  FlexLine* GetParentObject()
  {
    return mParent;
  }

  nsINode* GetNode() const;
  double MainBaseSize() const;
  double MainDeltaSize() const;
  double MainMinSize() const;
  double MainMaxSize() const;
  double CrossMinSize() const;
  double CrossMaxSize() const;

protected:
  RefPtr<FlexLine> mParent;
  RefPtr<nsINode> mNode;

  // These sizes are all CSS pixel units.
  double mMainBaseSize;
  double mMainDeltaSize;
  double mMainMinSize;
  double mMainMaxSize;
  double mCrossMinSize;
  double mCrossMaxSize;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_FlexItem_h */
