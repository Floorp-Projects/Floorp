/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_PropertyBagUtils_h
#define IPC_PropertyBagUtils_h

#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsIPropertyBag2.h"
#include "nsIVariant.h"

namespace mozilla::ipc {

/**
 * Limited nsIVariant support. Not all types are implemented and only
 * nsIURI is implemented with nsIVariant::GetAsInterface.
 */
template <>
struct IPDLParamTraits<nsIVariant*> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    nsIVariant* aParam);
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<nsIVariant>* aResult);
};

template <>
struct IPDLParamTraits<nsIPropertyBag2*> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    nsIPropertyBag2* aParam);
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<nsIPropertyBag2>* aResult);
};

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_URIUtils_h
