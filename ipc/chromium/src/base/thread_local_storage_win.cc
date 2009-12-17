// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_local_storage.h"

#include <windows.h>

#include "base/logging.h"

// In order to make TLS destructors work, we need to keep function
// pointers to the destructor for each TLS that we allocate.
// We make this work by allocating a single OS-level TLS, which
// contains an array of slots for the application to use.  In
// parallel, we also allocate an array of destructors, which we
// keep track of and call when threads terminate.

// tls_key_ is the one native TLS that we use.  It stores our
// table.
long ThreadLocalStorage::tls_key_ = TLS_OUT_OF_INDEXES;

// tls_max_ is the high-water-mark of allocated thread local storage.
// We intentionally skip 0 so that it is not confused with an
// unallocated TLS slot.
long ThreadLocalStorage::tls_max_ = 1;

// An array of destructor function pointers for the slots.  If
// a slot has a destructor, it will be stored in its corresponding
// entry in this array.
ThreadLocalStorage::TLSDestructorFunc
  ThreadLocalStorage::tls_destructors_[kThreadLocalStorageSize];

void** ThreadLocalStorage::Initialize() {
  if (tls_key_ == TLS_OUT_OF_INDEXES) {
    long value = TlsAlloc();
    DCHECK(value != TLS_OUT_OF_INDEXES);

    // Atomically test-and-set the tls_key.  If the key is TLS_OUT_OF_INDEXES,
    // go ahead and set it.  Otherwise, do nothing, as another
    // thread already did our dirty work.
    if (InterlockedCompareExchange(&tls_key_, value, TLS_OUT_OF_INDEXES) !=
            TLS_OUT_OF_INDEXES) {
      // We've been shortcut. Another thread replaced tls_key_ first so we need
      // to destroy our index and use the one the other thread got first.
      TlsFree(value);
    }
  }
  DCHECK(TlsGetValue(tls_key_) == NULL);

  // Create an array to store our data.
  void** tls_data = new void*[kThreadLocalStorageSize];
  memset(tls_data, 0, sizeof(void*[kThreadLocalStorageSize]));
  TlsSetValue(tls_key_, tls_data);
  return tls_data;
}

ThreadLocalStorage::Slot::Slot(TLSDestructorFunc destructor)
    : initialized_(false) {
  Initialize(destructor);
}

bool ThreadLocalStorage::Slot::Initialize(TLSDestructorFunc destructor) {
  if (tls_key_ == TLS_OUT_OF_INDEXES || !TlsGetValue(tls_key_))
    ThreadLocalStorage::Initialize();

  // Grab a new slot.
  slot_ = InterlockedIncrement(&tls_max_) - 1;
  if (slot_ >= kThreadLocalStorageSize) {
    NOTREACHED();
    return false;
  }

  // Setup our destructor.
  tls_destructors_[slot_] = destructor;
  initialized_ = true;
  return true;
}

void ThreadLocalStorage::Slot::Free() {
  // At this time, we don't reclaim old indices for TLS slots.
  // So all we need to do is wipe the destructor.
  tls_destructors_[slot_] = NULL;
  initialized_ = false;
}

void* ThreadLocalStorage::Slot::Get() const {
  void** tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  if (!tls_data)
    tls_data = ThreadLocalStorage::Initialize();
  DCHECK(slot_ >= 0 && slot_ < kThreadLocalStorageSize);
  return tls_data[slot_];
}

void ThreadLocalStorage::Slot::Set(void* value) {
  void** tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  if (!tls_data)
    tls_data = ThreadLocalStorage::Initialize();
  DCHECK(slot_ >= 0 && slot_ < kThreadLocalStorageSize);
  tls_data[slot_] = value;
}

void ThreadLocalStorage::ThreadExit() {
  if (tls_key_ == TLS_OUT_OF_INDEXES)
    return;

  void** tls_data = static_cast<void**>(TlsGetValue(tls_key_));

  // Maybe we have never initialized TLS for this thread.
  if (!tls_data)
    return;

  for (int slot = 0; slot < tls_max_; slot++) {
    if (tls_destructors_[slot] != NULL) {
      void* value = tls_data[slot];
      tls_destructors_[slot](value);
    }
  }

  delete[] tls_data;

  // In case there are other "onexit" handlers...
  TlsSetValue(tls_key_, NULL);
}

// Thread Termination Callbacks.
// Windows doesn't support a per-thread destructor with its
// TLS primitives.  So, we build it manually by inserting a
// function to be called on each thread's exit.
// This magic is from http://www.codeproject.com/threads/tls.asp
// and it works for VC++ 7.0 and later.

#ifdef _WIN64

// This makes the linker create the TLS directory if it's not already
// there.  (e.g. if __declspec(thread) is not used).
#pragma comment(linker, "/INCLUDE:_tls_used")

#else  // _WIN64

// This makes the linker create the TLS directory if it's not already
// there.  (e.g. if __declspec(thread) is not used).
#pragma comment(linker, "/INCLUDE:__tls_used")

#endif  // _WIN64

// Static callback function to call with each thread termination.
void NTAPI OnThreadExit(PVOID module, DWORD reason, PVOID reserved)
{
  // On XP SP0 & SP1, the DLL_PROCESS_ATTACH is never seen. It is sent on SP2+
  // and on W2K and W2K3. So don't assume it is sent.
  if (DLL_THREAD_DETACH == reason || DLL_PROCESS_DETACH == reason)
    ThreadLocalStorage::ThreadExit();
}

// .CRT$XLA to .CRT$XLZ is an array of PIMAGE_TLS_CALLBACK pointers that are
// called automatically by the OS loader code (not the CRT) when the module is
// loaded and on thread creation. They are NOT called if the module has been
// loaded by a LoadLibrary() call. It must have implicitly been loaded at
// process startup.
// By implicitly loaded, I mean that it is directly referenced by the main EXE
// or by one of its dependent DLLs. Delay-loaded DLL doesn't count as being
// implicitly loaded.
//
// See VC\crt\src\tlssup.c for reference.
#ifdef _WIN64

// .CRT section is merged with .rdata on x64 so it must be constant data.
#pragma const_seg(".CRT$XLB")
// When defining a const variable, it must have external linkage to be sure the
// linker doesn't discard it. If this value is discarded, the OnThreadExit
// function will never be called.
extern const PIMAGE_TLS_CALLBACK p_thread_callback;
const PIMAGE_TLS_CALLBACK p_thread_callback = OnThreadExit;

// Reset the default section.
#pragma const_seg()

#else  // _WIN64

#pragma data_seg(".CRT$XLB")
PIMAGE_TLS_CALLBACK p_thread_callback = OnThreadExit;

// Reset the default section.
#pragma data_seg()

#endif  // _WIN64
