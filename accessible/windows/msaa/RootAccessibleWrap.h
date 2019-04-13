/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "mozilla/mscom/Aggregation.h"
#include "RootAccessible.h"

namespace mozilla {

class PresShell;

namespace a11y {

class RootAccessibleWrap : public RootAccessible {
 public:
  RootAccessibleWrap(dom::Document* aDocument, PresShell* aPresShell);
  virtual ~RootAccessibleWrap();

  // RootAccessible
  virtual void DocumentActivated(DocAccessible* aDocument);

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
  DECLARE_AGGREGATABLE(RootAccessibleWrap);
  IUnknown* mOuter;
};

}  // namespace a11y
}  // namespace mozilla

#endif
