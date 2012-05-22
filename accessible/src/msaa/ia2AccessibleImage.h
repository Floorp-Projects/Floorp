/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_IMAGE_H
#define _ACCESSIBLE_IMAGE_H

#include "AccessibleImage.h"

class ia2AccessibleImage : public IAccessibleImage
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleImage
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_description(
      /* [retval][out] */ BSTR *description);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_imagePosition(
      /* [in] */ enum IA2CoordinateType coordinateType,
      /* [out] */ long *x,
      /* [retval][out] */ long *y);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_imageSize(
      /* [out] */ long *height,
      /* [retval][out] */ long *width);
};

#endif

