/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The scoped_ptr.h our IPC copy of Chromium's code does not include
// scoped_array, so adapt it to nsAutoArrayPtr here.

#ifndef FAKE_SCOPED_PTR_H_
#define FAKE_SCOPED_PTR_H_

#include "mozilla/ArrayUtils.h"
#include "nsAutoPtr.h"

template<class T>
class scoped_array : public nsAutoArrayPtr<T> {
public:
  scoped_array(T* t) : nsAutoArrayPtr<T>(t) {}
  void reset(T* t) {
    scoped_array<T> other(t);
    operator=(other);
  }
};

#define arraysize mozilla::ArrayLength

#endif
