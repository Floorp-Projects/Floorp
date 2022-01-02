/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaRootAccessible_h__
#define mozilla_a11y_MsaaRootAccessible_h__

#include "mozilla/mscom/Aggregation.h"
#include "MsaaDocAccessible.h"

namespace mozilla {

namespace a11y {

class MsaaRootAccessible : public MsaaDocAccessible {
 public:
  explicit MsaaRootAccessible(Accessible* aAcc)
      : MsaaDocAccessible(aAcc), mOuter(&mInternalUnknown) {}

  /**
   * This method enables a RootAccessibleWrap to be wrapped by a
   * LazyInstantiator.
   *
   * @param aOuter The IUnknown of the object that is wrapping this
   *               RootAccessibleWrap, or nullptr to unwrap the aOuter from
   *               a previous call.
   * @return This objects own IUnknown (as opposed to aOuter's IUnknown).
   */
  already_AddRefed<IUnknown> Aggregate(IUnknown* aOuter);

  /**
   * @return This object's own IUnknown, as opposed to its wrapper's IUnknown
   *         which is what would be returned by QueryInterface(IID_IUnknown).
   */
  already_AddRefed<IUnknown> GetInternalUnknown();

  virtual /* [id] */ HRESULT STDMETHODCALLTYPE accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR* pvarEndUpAt) override;

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accFocus(
      /* [retval][out] */ VARIANT __RPC_FAR* pvarChild) override;

 private:
  // DECLARE_AGGREGATABLE declares the internal IUnknown methods as well as
  // mInternalUnknown.
  DECLARE_AGGREGATABLE(MsaaRootAccessible);
  IUnknown* mOuter;

  RootAccessible* RootAcc();
};

}  // namespace a11y
}  // namespace mozilla

#endif
