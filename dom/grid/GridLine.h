/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridLine_h
#define mozilla_dom_GridLine_h

#include "mozilla/dom/GridBinding.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class GridLines;

class GridLine : public nsISupports, public nsWrapperCache {
 public:
  explicit GridLine(GridLines* aParent);

 protected:
  virtual ~GridLine();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GridLine)

  void GetNames(nsTArray<nsString>& aNames) const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  GridLines* GetParentObject() { return mParent; }

  double Start() const;
  double Breadth() const;
  GridDeclaration Type() const;
  uint32_t Number() const;
  int32_t NegativeNumber() const;

  void SetLineValues(const nsTArray<nsString>& aNames, double aStart,
                     double aBreadth, uint32_t aNumber, int32_t aNegativeNumber,
                     GridDeclaration aType);

  void SetLineNames(const nsTArray<nsString>& aNames);

 protected:
  RefPtr<GridLines> mParent;
  nsTArray<nsString> mNames;
  double mStart;
  double mBreadth;
  GridDeclaration mType;
  uint32_t mNumber;
  int32_t mNegativeNumber;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_GridLine_h */
