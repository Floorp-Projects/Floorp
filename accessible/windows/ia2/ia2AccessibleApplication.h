/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IA2_ACCESSIBLE_APPLICATION_H_
#define IA2_ACCESSIBLE_APPLICATION_H_

#include "AccessibleApplication.h"

namespace mozilla {
namespace a11y {
class ApplicationAccessible;

class ia2AccessibleApplication : public IAccessibleApplication {
 public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleApplication
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_appName(
      /* [retval][out] */ BSTR* name);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_appVersion(
      /* [retval][out] */ BSTR* version);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_toolkitName(
      /* [retval][out] */ BSTR* name);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_toolkitVersion(
      /* [retval][out] */ BSTR* version);

 private:
  ApplicationAccessible* AppAcc();
};

}  // namespace a11y
}  // namespace mozilla

#endif
