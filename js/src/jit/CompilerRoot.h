/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompilerRoot_h
#define jit_CompilerRoot_h

#ifdef JS_ION

#include "jscntxt.h"

#include "jit/Ion.h"
#include "jit/IonAllocPolicy.h"
#include "js/RootingAPI.h"

namespace js {
namespace jit {

// Roots a read-only GCThing for the lifetime of a single compilation.
// Each root is maintained in a linked list that is walked over during tracing.
// The CompilerRoot must be heap-allocated and may not go out of scope.
template <typename T>
class CompilerRoot : public CompilerRootNode
{
  public:
    CompilerRoot(T ptr)
      : CompilerRootNode(nullptr)
    {
        if (ptr) {
            JS_ASSERT(!GetIonContext()->runtime->isInsideNursery(ptr));
            setRoot(ptr);
        }
    }

  public:
    // Sets the pointer and inserts into root list. The pointer becomes read-only.
    void setRoot(T root) {
        CompilerRootNode *&rootList = GetIonContext()->temp->rootList();

        JS_ASSERT(!ptr_);
        ptr_ = root;
        next = rootList;
        rootList = this;
    }

  public:
    operator T () const { return static_cast<T>(ptr_); }
    T operator ->() const { return static_cast<T>(ptr_); }

  private:
    CompilerRoot() MOZ_DELETE;
    CompilerRoot(const CompilerRoot<T> &) MOZ_DELETE;
    CompilerRoot<T> &operator =(const CompilerRoot<T> &) MOZ_DELETE;
};

typedef CompilerRoot<JSObject*> CompilerRootObject;
typedef CompilerRoot<JSFunction*> CompilerRootFunction;
typedef CompilerRoot<JSScript*> CompilerRootScript;
typedef CompilerRoot<PropertyName*> CompilerRootPropertyName;
typedef CompilerRoot<Shape*> CompilerRootShape;
typedef CompilerRoot<Value> CompilerRootValue;

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_CompilerRoot_h */
