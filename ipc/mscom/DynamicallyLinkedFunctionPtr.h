/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_DynamicallyLinkedFunctionPtr_h
#define mozilla_mscom_DynamicallyLinkedFunctionPtr_h

#include "mozilla/Move.h"
#include <windows.h>

namespace mozilla {
namespace mscom {

template <typename T>
class DynamicallyLinkedFunctionPtr;

template <typename R, typename... Args>
class DynamicallyLinkedFunctionPtr<R (__stdcall*)(Args...)>
{
  typedef R (__stdcall* FunctionPtrT)(Args...);

public:
  DynamicallyLinkedFunctionPtr(const wchar_t* aLibName, const char* aFuncName)
    : mModule(NULL)
    , mFunction(nullptr)
  {
    mModule = ::LoadLibraryW(aLibName);
    if (mModule) {
      mFunction = reinterpret_cast<FunctionPtrT>(
                    ::GetProcAddress(mModule, aFuncName));
    }
  }

  DynamicallyLinkedFunctionPtr(const DynamicallyLinkedFunctionPtr&) = delete;
  DynamicallyLinkedFunctionPtr& operator=(const DynamicallyLinkedFunctionPtr&) = delete;

  DynamicallyLinkedFunctionPtr(DynamicallyLinkedFunctionPtr&&) = delete;
  DynamicallyLinkedFunctionPtr& operator=(DynamicallyLinkedFunctionPtr&&) = delete;

  ~DynamicallyLinkedFunctionPtr()
  {
    if (mModule) {
      ::FreeLibrary(mModule);
    }
  }

  R operator()(Args... args)
  {
    return mFunction(mozilla::Forward<Args>(args)...);
  }

  explicit operator bool() const
  {
    return !!mFunction;
  }

private:
  HMODULE       mModule;
  FunctionPtrT  mFunction;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_DynamicallyLinkedFunctionPtr_h

