/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef threading_Mutex_h
#define threading_Mutex_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"

#include <new>
#include <string.h>

namespace js {

class Mutex
{
public:
  struct PlatformData;

  Mutex();
  ~Mutex();

  void lock();
  void unlock();

  Mutex(Mutex&& rhs)
    : platformData_(rhs.platformData_)
  {
    MOZ_ASSERT(this != &rhs, "self move disallowed!");
    rhs.platformData_ = nullptr;
  }

  Mutex& operator=(Mutex&& rhs) {
    this->~Mutex();
    new (this) Mutex(mozilla::Move(rhs));
    return *this;
  }

  bool operator==(const Mutex& rhs) {
    return platformData_ == rhs.platformData_;
  }

private:
  Mutex(const Mutex&) = delete;
  void operator=(const Mutex&) = delete;

  friend class ConditionVariable;
  PlatformData* platformData() {
    MOZ_ASSERT(platformData_);
    return platformData_;
  };

  PlatformData* platformData_;
};

} // namespace js

#endif // threading_Mutex_h
