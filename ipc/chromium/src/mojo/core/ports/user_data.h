// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_USER_DATA_H_
#define MOJO_CORE_PORTS_USER_DATA_H_

#include "nsISupportsImpl.h"

namespace mojo {
namespace core {
namespace ports {

class UserData {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UserData);

 protected:
  virtual ~UserData() = default;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_USER_DATA_H_
