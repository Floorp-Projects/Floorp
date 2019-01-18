/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  error This code is NOT for internal Gecko use!
#endif  // defined(MOZILLA_INTERNAL_API)

#ifndef mozilla_a11y_HandlerRelation_h
#  define mozilla_a11y_HandlerRelation_h

#  include "AccessibleHandler.h"
#  include "IUnknownImpl.h"
#  include "mozilla/RefPtr.h"

namespace mozilla {
namespace a11y {

class HandlerRelation final : public IAccessibleRelation {
 public:
  explicit HandlerRelation(AccessibleHandler* aHandler, IARelationData& aData);

  DECL_IUNKNOWN

  // IAccessibleRelation
  STDMETHODIMP get_relationType(BSTR* aType) override;
  STDMETHODIMP get_localizedRelationType(BSTR* aLocalizedType) override;
  STDMETHODIMP get_nTargets(long* aNTargets) override;
  STDMETHODIMP get_target(long aIndex, IUnknown** aTarget) override;
  STDMETHODIMP get_targets(long aMaxTargets, IUnknown** aTargets,
                           long* aNTargets) override;

 private:
  virtual ~HandlerRelation();
  HRESULT GetTargets();
  RefPtr<AccessibleHandler> mHandler;
  IARelationData mData;
  IUnknown** mTargets;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_HandlerRelation_h
