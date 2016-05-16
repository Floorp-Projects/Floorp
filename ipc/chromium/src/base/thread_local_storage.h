/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREAD_LOCAL_STORAGE_H_
#define BASE_THREAD_LOCAL_STORAGE_H_

#include "base/basictypes.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

// Wrapper for thread local storage.  This class doesn't do much except provide
// an API for portability.
class ThreadLocalStorage {
 public:

  // Prototype for the TLS destructor function, which can be optionally used to
  // cleanup thread local storage on thread exit.  'value' is the data that is
  // stored in thread local storage.
  typedef void (*TLSDestructorFunc)(void* value);

  // A key representing one value stored in TLS.
  class Slot {
   public:
    explicit Slot(TLSDestructorFunc destructor = NULL);

    // This constructor should be used for statics.
    // It returns an uninitialized Slot.
    explicit Slot(base::LinkerInitialized x) {}

    // Set up the TLS slot.  Called by the constructor.
    // 'destructor' is a pointer to a function to perform per-thread cleanup of
    // this object.  If set to NULL, no cleanup is done for this TLS slot.
    // Returns false on error.
    bool Initialize(TLSDestructorFunc destructor);

    // Free a previously allocated TLS 'slot'.
    // If a destructor was set for this slot, removes
    // the destructor so that remaining threads exiting
    // will not free data.
    void Free();

    // Get the thread-local value stored in slot 'slot'.
    // Values are guaranteed to initially be zero.
    void* Get() const;

    // Set the thread-local value stored in slot 'slot' to
    // value 'value'.
    void Set(void* value);

    bool initialized() const { return initialized_; }

   private:
    // The internals of this struct should be considered private.
    bool initialized_;
#if defined(OS_WIN)
    int slot_;
#elif defined(OS_POSIX)
    pthread_key_t key_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

#if defined(OS_WIN)
  // Function called when on thread exit to call TLS
  // destructor functions.  This function is used internally.
  static void ThreadExit();

 private:
  // Function to lazily initialize our thread local storage.
  static void **Initialize();

 private:
  // The maximum number of 'slots' in our thread local storage stack.
  // For now, this is fixed.  We could either increase statically, or
  // we could make it dynamic in the future.
  static const int kThreadLocalStorageSize = 64;

  static long tls_key_;
  static long tls_max_;
  static TLSDestructorFunc tls_destructors_[kThreadLocalStorageSize];
#endif  // OS_WIN

  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorage);
};

// Temporary backwards-compatible name.
// TODO(evanm): replace all usage of TLSSlot.
typedef ThreadLocalStorage::Slot TLSSlot;

#endif  // BASE_THREAD_LOCAL_STORAGE_H_
