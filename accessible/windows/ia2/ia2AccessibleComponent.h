/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IA2_ACCESSIBLE_COMPONENT_H_
#define IA2_ACCESSIBLE_COMPONENT_H_

#include "AccessibleComponent.h"

namespace mozilla {
namespace a11y {

class ia2AccessibleComponent : public IAccessibleComponent
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleComponent
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_locationInParent(
      /* [out] */ long *x,
      /* [retval][out] */ long *y);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_foreground(
      /* [retval][out] */ IA2Color *foreground);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_background(
      /* [retval][out] */ IA2Color *background);
};

} // namespace a11y
} // namespace mozilla

#endif
