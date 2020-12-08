/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_PRESENTATION_MOCKEDSOCKETTRANSPORT_H_
#define DOM_PRESENTATION_MOCKEDSOCKETTRANSPORT_H_

#include "nsISocketTransport.h"

namespace mozilla {
namespace dom {

// For testing purposes, we need a mocked socket transport that doesn't do
// anything. It has to be implemented in C++ because nsISocketTransport is
// builtinclass because it contains nostdcall methods.

class MockedSocketTransport final : public nsISocketTransport {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORT
  NS_DECL_NSISOCKETTRANSPORT

  MockedSocketTransport() = default;

  static already_AddRefed<MockedSocketTransport> Create();

 protected:
  virtual ~MockedSocketTransport() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_PRESENTATION_MOCKEDSOCKETTRANSPORT_H_
