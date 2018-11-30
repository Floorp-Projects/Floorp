/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ModuleSharedContext_h
#define frontend_ModuleSharedContext_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_STACK_CLASS

#include "builtin/ModuleObject.h"    // js::Module{Builder,Object}
#include "frontend/SharedContext.h"  // js::frontend::SharedContext
#include "js/RootingAPI.h"           // JS::Handle, JS::Rooted
#include "vm/Scope.h"                // js::{Module,}Scope

namespace js {
namespace frontend {

class MOZ_STACK_CLASS ModuleSharedContext : public SharedContext {
  JS::Rooted<ModuleObject*> module_;
  JS::Rooted<Scope*> enclosingScope_;

 public:
  JS::Rooted<ModuleScope::Data*> bindings;
  ModuleBuilder& builder;

  ModuleSharedContext(JSContext* cx, ModuleObject* module,
                      Scope* enclosingScope, ModuleBuilder& builder);

  JS::Handle<ModuleObject*> module() const { return module_; }
  Scope* compilationEnclosingScope() const override { return enclosingScope_; }
};

inline ModuleSharedContext* SharedContext::asModuleContext() {
  MOZ_ASSERT(isModuleContext());
  return static_cast<ModuleSharedContext*>(this);
}

}  // namespace frontend
}  // namespace js

#endif /* frontend_ModuleSharedContext_h */
