/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Message handlers
 *
 * This file contains base classes for message handling.
 */

#ifndef mozilla_ipc_DaemonSocketMessageHandlers_h
#define mozilla_ipc_DaemonSocketMessageHandlers_h

#include "nsISupportsImpl.h" // for ref-counting

namespace mozilla {
namespace ipc {

/**
 * |DaemonSocketResultHandler| is the base class for all protocol-specific
 * result handlers. It currently only manages the reference counting.
 */
class DaemonSocketResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DaemonSocketResultHandler);

  virtual ~DaemonSocketResultHandler()
  { }

protected:
  DaemonSocketResultHandler()
  { }
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_DaemonSocketMessageHandlers_h
