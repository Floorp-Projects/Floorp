// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_PORT_REF_H_
#define MOJO_CORE_PORTS_PORT_REF_H_

#include "mojo/core/ports/name.h"
#include "mozilla/RefPtr.h"

namespace mojo {
namespace core {
namespace ports {

class Port;
class PortLocker;

class PortRef {
 public:
  ~PortRef();
  PortRef();
  PortRef(const PortName& name, RefPtr<Port> port);

  PortRef(const PortRef& other);
  PortRef(PortRef&& other);

  PortRef& operator=(const PortRef& other);
  PortRef& operator=(PortRef&& other);

  const PortName& name() const { return name_; }

  bool is_valid() const { return !!port_; }

 private:
  friend class PortLocker;

  Port* port() const { return port_.get(); }

  PortName name_;
  RefPtr<Port> port_;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_PORT_REF_H_
