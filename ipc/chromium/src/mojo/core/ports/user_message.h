// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_USER_MESSAGE_H_
#define MOJO_CORE_PORTS_USER_MESSAGE_H_

#include <stddef.h>

namespace mojo {
namespace core {
namespace ports {

class UserMessageEvent;

// Base type to use for any embedder-defined user message implementation. This
// class is intentionally empty.
//
// Provides a bit of type-safety help to subclasses since by design downcasting
// from this type is a common operation in embedders.
//
// Each subclass should define a static const instance of TypeInfo named
// |kUserMessageTypeInfo| and pass its address down to the UserMessage
// constructor. The type of a UserMessage can then be dynamically inspected by
// comparing |type_info()| to any subclass's |&kUserMessageTypeInfo|.
class UserMessage {
 public:
  struct TypeInfo {};

  explicit UserMessage(const TypeInfo* type_info);
  virtual ~UserMessage();

  UserMessage(const UserMessage&) = delete;
  void operator=(const UserMessage&) = delete;

  const TypeInfo* type_info() const { return type_info_; }

  // Invoked immediately before the system asks the embedder to forward this
  // message to an external node.
  //
  // The UserMessageEvent is passed in to allow ports and other such values to
  // be attached to the message before it is sent externally, in case late
  // serialization is performed.
  //
  // Returns |true| if the message is OK to route externally, or |false|
  // otherwise. Returning |false| implies an unrecoverable condition, and the
  // message event will be destroyed without further routing.
  virtual bool WillBeRoutedExternally(UserMessageEvent& event);

  // Returns the size in bytes of this message iff it's serialized. Zero
  // otherwise.
  virtual size_t GetSizeIfSerialized() const;

 private:
  const TypeInfo* const type_info_;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_USER_MESSAGE_H_
