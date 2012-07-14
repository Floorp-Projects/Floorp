/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined(jsion_ion_gc_h__) && defined(JS_ION)
#define jsion_ion_gc_h__

#include "jscntxt.h"
#include "gc/Root.h"

namespace js {
namespace ion {

// Roots a read-only GCThing for the lifetime of a single compilation.
// Each root is maintained in a linked list that is walked over during tracing.
// The CompilerRoot must be heap-allocated and may not go out of scope.
template <typename T>
class CompilerRoot : public CompilerRootNode
{
  public:
    CompilerRoot()
      : CompilerRootNode(NULL)
    { }

    CompilerRoot(T ptr)
      : CompilerRootNode(NULL)
    {
        if (ptr)
            setRoot(ptr);
    }

  public:
    // Sets the pointer and inserts into root list. The pointer becomes read-only.
    void setRoot(T root) {
        JSRuntime *rt = root->compartment()->rt;

        JS_ASSERT(!ptr);
        ptr = root;
        next = rt->ionCompilerRootList;
        rt->ionCompilerRootList = this;
    }

  public:
    operator T () const { return static_cast<T>(ptr); }
    T operator ->() const { return static_cast<T>(ptr); }
};

typedef CompilerRoot<JSObject*>   CompilerRootObject;
typedef CompilerRoot<JSFunction*> CompilerRootFunction;
typedef CompilerRoot<PropertyName*> CompilerRootPropertyName;
typedef CompilerRoot<Value> CompilerRootValue;

// Automatically clears the compiler root list when compilation finishes.
class AutoCompilerRoots
{
    JSRuntime *rt_;

  public:
    AutoCompilerRoots(JSRuntime *rt)
      : rt_(rt)
    {
        JS_ASSERT(rt_->ionCompilerRootList == NULL);
    }

    ~AutoCompilerRoots()
    {
        rt_->ionCompilerRootList = NULL;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_ion_gc_h__

