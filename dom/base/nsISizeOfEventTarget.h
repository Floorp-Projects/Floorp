/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISizeOfEventTarget_h___
#define nsISizeOfEventTarget_h___

#include "mozilla/MemoryReporting.h"
#include "nsISupports.h"

#define NS_ISIZEOFEVENTTARGET_IID \
  {0xa1e08cb9, 0x5455, 0x4593, \
    { 0xb4, 0x1f, 0x38, 0x7a, 0x85, 0x44, 0xd0, 0xb5 }}

/**
 * This class is much the same as nsISizeOf, but is specifically for measuring
 * the contents of nsGlobalWindow::mEventTargetObjects.
 *
 * We don't use nsISizeOf because if we did, any object belonging to
 * mEventTargetObjects that implements nsISizeOf would be measured, which we
 * may not want (perhaps because the object is also measured elsewhere).
 */
class nsISizeOfEventTarget : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISIZEOFEVENTTARGET_IID)

  /**
   * Measures the size of the things pointed to by the object, plus the object
   * itself.
   */
  virtual size_t
    SizeOfEventTargetIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISizeOfEventTarget, NS_ISIZEOFEVENTTARGET_IID)

#endif /* nsISizeOfEventTarget_h___ */
