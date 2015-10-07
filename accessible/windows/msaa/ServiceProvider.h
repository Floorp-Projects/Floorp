/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ServiceProvider_h_
#define mozilla_a11y_ServiceProvider_h_

#include <servprov.h>

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"

namespace mozilla {
namespace a11y {

class ServiceProvider final : public IServiceProvider
{
public:
  ServiceProvider(AccessibleWrap* aAcc) : mAccessible(aAcc) {}
  ~ServiceProvider() {}

  DECL_IUNKNOWN

  // IServiceProvider
  virtual HRESULT STDMETHODCALLTYPE QueryService(REFGUID aGuidService,
                                                 REFIID aIID,
                                                 void** aInstancePtr);

private:
  RefPtr<AccessibleWrap> mAccessible;
};

}
}

#endif
