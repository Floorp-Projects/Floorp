/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Declares IPC serializers for the visibility-related types in Visibility.h.
 * These are separated out to reduce the number of moz.build files that need to
 * include chromium IPC headers, since Visibility.h is included from nsIFrame.h
 * which is widely included.
 */

#ifndef mozilla_layout_generic_VisibilityIPC_h
#define mozilla_layout_generic_VisibilityIPC_h

#include "ipc/IPCMessageUtils.h"

#include "Visibility.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::VisibilityCounter>
{
  typedef mozilla::VisibilityCounter paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, uint8_t(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    uint8_t valueAsByte;
    if (ReadParam(aMsg, aIter, &valueAsByte)) {
      *aResult = paramType(valueAsByte);
      return true;
    }
    return false;
  }
};

} // namespace IPC

#endif // mozilla_layout_generic_VisibilityIPC_h
