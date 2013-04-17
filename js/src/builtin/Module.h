/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Module_h___
#define Module_h___

#include "jsobj.h"

namespace js {

class Module : public JSObject {
  public:
    JSAtom *atom() {
        return &getReservedSlot(ATOM_SLOT).toString()->asAtom();
    };

    void setAtom(JSAtom *atom) {
        setReservedSlot(ATOM_SLOT, StringValue(atom));
    };

    JSScript *script() {
        return (JSScript *) getReservedSlot(SCRIPT_SLOT).toPrivate();
    }

    void setScript(JSScript *script) {
        setReservedSlot(SCRIPT_SLOT, PrivateValue(script));
    }

  private:
    static const uint32_t ATOM_SLOT = 0;
    static const uint32_t SCRIPT_SLOT = 1;
};

} // namespace js

js::Module *
js_NewModule(JSContext *cx, js::HandleAtom atom);

inline js::Module &
JSObject::asModule()
{
    JS_ASSERT(isModule());
    return *static_cast<js::Module *>(this);
}

#endif // Module_h___
