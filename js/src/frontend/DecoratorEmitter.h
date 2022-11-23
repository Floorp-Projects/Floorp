/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_DecoratorEmitter_h
#define frontend_DecoratorEmitter_h

#include "mozilla/Attributes.h"

#include "frontend/ParseNode.h"

namespace js::frontend {

struct BytecodeEmitter;

class MOZ_STACK_CLASS DecoratorEmitter {
 private:
  BytecodeEmitter* bce_;

 public:
  enum Kind { Method, Getter, Setter, Field, Accessor, Class };

  explicit DecoratorEmitter(BytecodeEmitter* bce);

  [[nodiscard]] bool emitApplyDecoratorsToElementDefinition(
      Kind kind, ParseNode* key, ListNode* decorators, bool isStatic);

 private:
  [[nodiscard]] bool emitDecorationState();

  [[nodiscard]] bool emitUpdateDecorationState();

  [[nodiscard]] bool emitCreateDecoratorAccessObject();

  [[nodiscard]] bool emitCreateAddInitializerFunction();

  [[nodiscard]] bool emitCreateDecoratorContextObject(Kind kind, ParseNode* key,
                                                      bool isStatic,
                                                      TokenPos pos);
};

} /* namespace js::frontend */

#endif /* frontend_DecoratorEmitter_h */
