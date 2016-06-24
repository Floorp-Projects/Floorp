/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridLine_h
#define mozilla_dom_GridLine_h

#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class GridLines;

class GridLine : public nsISupports
               , public nsWrapperCache
{
public:
  GridLine(GridLines* aParent);

protected:
  virtual ~GridLine();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GridLine)

  void GetNames(nsTArray<nsString>& aNames) const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  GridLines* GetParentObject()
  {
    return mParent;
  }

  double Start() const;
  double Breadth() const;
  uint32_t Number() const;

  void SetLineValues(double aStart,
                     double aBreadth,
                     uint32_t aNumber,
                     const nsTArray<nsString>& aNames);

protected:
  RefPtr<GridLines> mParent;
  double mStart;
  double mBreadth;
  uint32_t mNumber;
  nsTArray<nsString> mNames;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_GridLine_h */
