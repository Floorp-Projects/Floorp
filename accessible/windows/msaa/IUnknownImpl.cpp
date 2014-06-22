/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. *
 */

#include "IUnknownImpl.h"

#include "nsDebug.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {
namespace a11y {

HRESULT
GetHRESULT(nsresult aResult)
{
  switch (aResult) {
    case NS_OK:
      return S_OK;

    case NS_ERROR_INVALID_ARG:
      return E_INVALIDARG;

    case NS_ERROR_OUT_OF_MEMORY:
      return E_OUTOFMEMORY;

    case NS_ERROR_NOT_IMPLEMENTED:
      return E_NOTIMPL;

    default:
      return E_FAIL;
  }
}

int
FilterExceptions(unsigned int aCode, EXCEPTION_POINTERS* aExceptionInfo)
{
  if (aCode == EXCEPTION_ACCESS_VIOLATION) {
#ifdef MOZ_CRASHREPORTER
    // MSAA swallows crashes (because it is COM-based) but we still need to
    // learn about those crashes so we can fix them. Make sure to pass them to
    // the crash reporter.
    CrashReporter::WriteMinidumpForException(aExceptionInfo);
#endif
  } else {
    NS_NOTREACHED("We should only be catching crash exceptions");
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace a11y
} // namespace mozilla
