/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2014, Mozilla
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 ** Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 ** Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 ** Neither the name of Google nor the names of its contributors may
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMP_PLATFORM_h_
#define GMP_PLATFORM_h_

#include "gmp-errors.h"
#include <stdint.h>

/* Platform helper API. */

class GMPTask {
public:
  virtual ~GMPTask() {}
  virtual void Run() = 0;
};

class GMPThread {
public:
  virtual ~GMPThread() {}
  virtual void Post(GMPTask* aTask) = 0;
  virtual void Join() = 0;
};

class GMPMutex {
public:
  virtual ~GMPMutex() {}
  virtual void Acquire() = 0;
  virtual void Release() = 0;
};

typedef GMPErr (*GMPCreateThreadPtr)(GMPThread** aThread);
typedef GMPErr (*GMPRunOnMainThreadPtr)(GMPTask* aTask);
typedef GMPErr (*GMPSyncRunOnMainThreadPtr)(GMPTask* aTask);
typedef GMPErr (*GMPCreateMutexPtr)(GMPMutex** aMutex);

struct GMPPlatformAPI {
  // Increment the version when things change. Can only add to the struct,
  // do not change what already exists. Pointers to functions may be NULL
  // when passed to plugins, but beware backwards compat implications of
  // doing that.
  uint16_t version; // Currently version 0

  GMPCreateThreadPtr createthread;
  GMPRunOnMainThreadPtr runonmainthread;
  GMPSyncRunOnMainThreadPtr syncrunonmainthread;
  GMPCreateMutexPtr createmutex;
};

#endif // GMP_PLATFORM_h_
