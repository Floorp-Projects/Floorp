/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ModuleObject_h
#define builtin_ModuleObject_h

#include "vm/NativeObject.h"

namespace js {

class ModuleObject : public NativeObject
{
  public:
    enum
    {
        ScriptSlot = 0,
        SlotCount
    };

    static const Class class_;

    static ModuleObject* create(ExclusiveContext* cx);
    void init(HandleScript script);

    JSScript* script() const;

  private:
    static void trace(JSTracer* trc, JSObject* obj);
};

typedef Rooted<ModuleObject*> RootedModuleObject;
typedef Handle<ModuleObject*> HandleModuleObject;

} // namespace js

#endif /* builtin_ModuleObject_h */
