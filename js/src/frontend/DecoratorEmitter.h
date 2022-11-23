/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_DecoratorEmitter_h
#define frontend_DecoratorEmitter_h

#include "mozilla/Attributes.h"

namespace js::frontend {

struct BytecodeEmitter;

class MOZ_STACK_CLASS DecoratorEmitter {
 public:
  explicit DecoratorEmitter(BytecodeEmitter* bce);
};

} /* namespace js::frontend */

#endif /* frontend_DecoratorEmitter_h */
