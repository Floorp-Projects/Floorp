/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Module_h
#define builtin_Module_h

#include "jsobj.h"

namespace js {

class Module : public JSObject {
  public:
    static Module *create(JSContext *cx, js::HandleAtom atom);

    JSAtom *atom() {
        return &getReservedSlot(ATOM_SLOT).toString()->asAtom();
    };

    JSScript *script() {
        return (JSScript *) getReservedSlot(SCRIPT_SLOT).toPrivate();
    }

    static Class class_;

  private:
    inline void setAtom(JSAtom *atom);
    inline void setScript(JSScript *script);

    static const uint32_t ATOM_SLOT = 0;
    static const uint32_t SCRIPT_SLOT = 1;
};

} // namespace js

#endif /* builtin_Module_h */
