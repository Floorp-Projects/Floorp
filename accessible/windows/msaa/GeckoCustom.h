/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_GeckoCustom_h_
#define mozilla_a11y_GeckoCustom_h_

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"
#include "IGeckoCustom.h"

namespace mozilla {
namespace a11y {

/**
 * a dumpster to put things exposed by the xpcom API but not a windows platform
 * API for the purposes of testing.
 */
class GeckoCustom final : public IGeckoCustom
{
public:
  explicit GeckoCustom(AccessibleWrap* aAcc) : mAcc(aAcc) {}

  // IUnknown
  DECL_IUNKNOWN

  virtual STDMETHODIMP get_anchorCount(long* aCount);
  virtual STDMETHODIMP get_DOMNodeID(BSTR* aID);
  virtual STDMETHODIMP get_ID(uint64_t* aID);

private:
  GeckoCustom() = delete;
  GeckoCustom& operator =(const GeckoCustom&) = delete;
  GeckoCustom(const GeckoCustom&) = delete;
  GeckoCustom(GeckoCustom&&) = delete;
  GeckoCustom& operator=(GeckoCustom&&) = delete;

  ~GeckoCustom() { }

protected:
  RefPtr<AccessibleWrap> mAcc;
};

} // a11y namespace
} // mozilla namespace

#endif
