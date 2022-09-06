/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlexLineValues_h
#define mozilla_dom_FlexLineValues_h

#include "mozilla/dom/FlexBinding.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

struct ComputedFlexLineInfo;

namespace mozilla::dom {

class Flex;
class FlexItemValues;

class FlexLineValues : public nsISupports, public nsWrapperCache {
 public:
  explicit FlexLineValues(Flex* aParent, const ComputedFlexLineInfo* aLine);

 protected:
  virtual ~FlexLineValues() = default;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FlexLineValues)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  Flex* GetParentObject() { return mParent; }

  FlexLineGrowthState GrowthState() const;
  double CrossStart() const;
  double CrossSize() const;
  double FirstBaselineOffset() const;
  double LastBaselineOffset() const;

  void GetItems(nsTArray<RefPtr<FlexItemValues>>& aResult);

 protected:
  RefPtr<Flex> mParent;

  FlexLineGrowthState mGrowthState;
  double mCrossStart;
  double mCrossSize;
  double mFirstBaselineOffset;
  double mLastBaselineOffset;

  nsTArray<RefPtr<FlexItemValues>> mItems;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_FlexLineValues_h */
