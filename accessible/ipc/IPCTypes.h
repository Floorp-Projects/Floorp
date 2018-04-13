/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_IPCTypes_h
#define mozilla_a11y_IPCTypes_h

#include "mozilla/a11y/Role.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::a11y::role>
  : public ContiguousEnumSerializerInclusive<mozilla::a11y::role,
                                             mozilla::a11y::role::NOTHING,
                                             mozilla::a11y::role::LAST_ROLE>
{
};
};

/**
 * Since IPDL does not support preprocessing, this header file allows us to
 * define types used by PDocAccessible differently depending on platform.
 */

#if defined(XP_WIN) && defined(ACCESSIBILITY)

// So that we don't include a bunch of other Windows junk.
#if !defined(COM_NO_WINDOWS_H)
#define COM_NO_WINDOWS_H
#endif // !defined(COM_NO_WINDOWS_H)

// COM headers pull in MSXML which conflicts with our own XMLDocument class.
// This define excludes those conflicting definitions.
#if !defined(__XMLDocument_FWD_DEFINED__)
#define __XMLDocument_FWD_DEFINED__
#endif // !defined(__XMLDocument_FWD_DEFINED__)

#include <combaseapi.h>

#include "mozilla/a11y/COMPtrTypes.h"

// This define in rpcndr.h messes up our code, so we must undefine it after
// COMPtrTypes.h has been included.
#if defined(small)
#undef small
#endif // defined(small)

#else

namespace mozilla {
namespace a11y {

typedef uint32_t IAccessibleHolder;
typedef uint32_t IDispatchHolder;
typedef uint32_t IHandlerControlHolder;

} // namespace a11y
} // namespace mozilla

#endif // defined(XP_WIN) && defined(ACCESSIBILITY)

#endif // mozilla_a11y_IPCTypes_h

