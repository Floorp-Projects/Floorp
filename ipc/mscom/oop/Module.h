/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Module_h
#define mozilla_mscom_Module_h

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include <objbase.h>

/* WARNING! The code in this file may be loaded into the address spaces of other
   processes! It MUST NOT link against xul.dll or other Gecko binaries! Only
   inline code may be included! */

namespace mozilla {
namespace mscom {

class Module
{
public:
  static HRESULT CanUnload() { return sRefCount == 0 ? S_OK : S_FALSE; }

  static void Lock() { ++sRefCount; }
  static void Unlock() { --sRefCount; }

private:
  static ULONG sRefCount;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Module_h

