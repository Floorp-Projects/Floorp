// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the type used to provide sources for NotificationService
// notifications.

#ifndef CHROME_COMMON_NOTIFICATION_SOURCE_H__
#define CHROME_COMMON_NOTIFICATION_SOURCE_H__

#include "base/basictypes.h"

// Do not declare a NotificationSource directly--use either
// "Source<sourceclassname>(sourceclasspointer)" or
// NotificationService::AllSources().
class NotificationSource {
 public:
  NotificationSource(const NotificationSource& other) : ptr_(other.ptr_) { }
  ~NotificationSource() {}

  // NotificationSource can be used as the index for a map; this method
  // returns the pointer to the current source as an identifier, for use as a
  // map index.
  uintptr_t map_key() const { return reinterpret_cast<uintptr_t>(ptr_); }

  bool operator!=(const NotificationSource& other) const {
    return ptr_ != other.ptr_;
  }
  bool operator==(const NotificationSource& other) const {
    return ptr_ == other.ptr_;
  }

 protected:
  NotificationSource(void* ptr) : ptr_(ptr) {}

  void* ptr_;
};

template <class T>
class Source : public NotificationSource {
 public:
  Source(T* ptr) : NotificationSource(ptr) {}

  Source(const NotificationSource& other)
    : NotificationSource(other) {}

  T* operator->() const { return ptr(); }
  T* ptr() const { return static_cast<T*>(ptr_); }
};

#endif  // CHROME_COMMON_NOTIFICATION_SOURCE_H__
