/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 *   Brian Hackett <bhackett@mozilla.com>
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

#ifndef jsion_caches_h__
#define jsion_caches_h__

#include "IonCode.h"
#include "TypeOracle.h"
#include "IonRegisters.h"

struct JSFunction;
struct JSScript;

namespace js {
namespace ion {

class IonCacheGetProperty;
class IonCacheSetProperty;

// Common structure encoding the state of a polymorphic inline cache contained
// in the code for an IonScript. IonCaches are used for polymorphic operations
// where multiple implementations may be required.
//
// The cache is initially compiled as a patchable jump to an out of line
// fragment which invokes a cache function to perform the operation. The cache
// function may generate a stub to perform the operation in certain cases
// (e.g. a particular shape for an input object), patch the cache's jump to
// that stub and patch any failure conditions in the stub to jump back to the
// cache fragment. When those failure conditions are hit, the cache function
// may attach new stubs, forming a daisy chain of tests for how to perform the
// operation in different circumstances.
//
// Eventually, if too many stubs are generated the cache function may disable
// the cache, by generating a stub to make a call and perform the operation
// within the VM.
//
// While calls may be made to the cache function and other VM functions, the
// cache may still be treated as pure during optimization passes, such that
// LICM and GVN may be performed on operations around the cache as if the
// operation cannot reenter scripted code through an Invoke() or otherwise have
// unexpected behavior. This restricts the sorts of stubs which the cache can
// generate or the behaviors which called functions can have, and if a called
// function performs a possibly impure operation then the operation will be
// marked as such and the calling script will be recompiled.
//
// Similarly, despite the presence of functions and multiple stubs generated
// for a cache, the cache itself may be marked as idempotent and become hoisted
// or coalesced by LICM or GVN. This also constrains the stubs which can be
// generated for the cache.

struct TypedOrValueRegisterSpace
{
    AlignedStorage2<TypedOrValueRegister> data_;
    TypedOrValueRegister &data() {
        return *data_.addr();
    }
    const TypedOrValueRegister &data() const {
        return *data_.addr();
    }
};

struct ConstantOrRegisterSpace
{
    AlignedStorage2<ConstantOrRegister> data_;
    ConstantOrRegister &data() {
        return *data_.addr();
    }
    const ConstantOrRegister &data() const {
        return *data_.addr();
    }
};

class IonCache
{
  protected:

    enum Kind {
        Invalid = 0,
        GetProperty,
        SetProperty
    } kind : 8;

    bool pure_ : 1;
    bool idempotent_ : 1;
    size_t stubCount_ : 6;

    CodeLocationJump initialJump_;
    CodeLocationJump lastJump_;
    CodeLocationLabel cacheLabel_;

    // Offset from the initial jump to the rejoin label.
    static const size_t REJOIN_LABEL_OFFSET = 0;

    union {
        struct {
            Register object;
            JSAtom *atom;
            TypedOrValueRegisterSpace output;
        } getprop;
        struct {
            Register object;
            JSAtom *atom;
            ConstantOrRegisterSpace value;
            bool strict;
        } setprop;
    } u;

    // Registers live after the cache, excluding output registers. The initial
    // value of these registers must be preserved by the cache.
    RegisterSet liveRegs;

    // Location of this operation, NULL for idempotent caches.
    JSScript *script;
    jsbytecode *pc;

    void init(Kind kind, RegisterSet liveRegs,
              CodeOffsetJump initialJump,
              CodeOffsetLabel rejoinLabel,
              CodeOffsetLabel cacheLabel) {
        this->kind = kind;
        this->liveRegs = liveRegs;
        this->initialJump_ = initialJump;
        this->lastJump_ = initialJump;
        this->cacheLabel_ = cacheLabel;

        JS_ASSERT(rejoinLabel.offset() == initialJump.offset() + REJOIN_LABEL_OFFSET);
    }

  public:

    IonCache() { PodZero(this); }

    void updateBaseAddress(IonCode *code) {
        initialJump_.repoint(code);
        lastJump_.repoint(code);
        cacheLabel_.repoint(code);
    }

    CodeLocationJump lastJump() const { return lastJump_; }
    CodeLocationLabel cacheLabel() const { return cacheLabel_; }

    CodeLocationLabel rejoinLabel() const {
        return CodeLocationLabel(lastJump().raw() + REJOIN_LABEL_OFFSET);
    }

    bool pure() { return pure_; }
    bool idempotent() { return idempotent_; }

    void updateLastJump(CodeLocationJump jump) { lastJump_ = jump; }

    size_t stubCount() const { return stubCount_; }
    void incrementStubCount() {
        // The IC should stop generating stubs before wrapping stubCount.
        stubCount_++;
        JS_ASSERT(stubCount_);
    }

    IonCacheGetProperty &toGetProperty() {
        JS_ASSERT(kind == GetProperty);
        return * (IonCacheGetProperty *) this;
    }

    IonCacheSetProperty &toSetProperty() {
        JS_ASSERT(kind == SetProperty);
        return * (IonCacheSetProperty *) this;
    }

    void setScriptedLocation(JSScript *script, jsbytecode *pc) {
        this->script = script;
        this->pc = pc;
    }

    void getScriptedLocation(JSScript **pscript, jsbytecode **ppc) {
        *pscript = script;
        *ppc = pc;
    }
};

inline IonCache &
IonScript::getCache(size_t index) {
    JS_ASSERT(index < numCaches());
    return cacheList()[index];
}

// Subclasses of IonCache for the various kinds of caches. These do not define
// new data members; all caches must be of the same size.

class IonCacheGetProperty : public IonCache
{
  public:
    IonCacheGetProperty(CodeOffsetJump initialJump,
                        CodeOffsetLabel rejoinLabel,
                        CodeOffsetLabel cacheLabel,
                        RegisterSet liveRegs,
                        Register object, JSAtom *atom,
                        TypedOrValueRegister output)
    {
        init(GetProperty, liveRegs, initialJump, rejoinLabel, cacheLabel);
        u.getprop.object = object;
        u.getprop.atom = atom;
        u.getprop.output.data() = output;
    }

    Register object() const { return u.getprop.object; }
    JSAtom *atom() const { return u.getprop.atom; }
    TypedOrValueRegister output() const { return u.getprop.output.data(); }

    bool attachNative(JSContext *cx, JSObject *obj, const Shape *shape);
};

class IonCacheSetProperty : public IonCache
{
  public:
    IonCacheSetProperty(CodeOffsetJump initialJump,
                        CodeOffsetLabel rejoinLabel,
                        CodeOffsetLabel cacheLabel,
                        RegisterSet liveRegs,
                        Register object, JSAtom *atom,
                        ConstantOrRegister value,
                        bool strict)
    {
        init(SetProperty, liveRegs, initialJump, rejoinLabel, cacheLabel);
        u.setprop.object = object;
        u.setprop.atom = atom;
        u.setprop.value.data() = value;
        u.setprop.strict = strict;
    }

    Register object() const { return u.setprop.object; }
    JSAtom *atom() const { return u.setprop.atom; }
    ConstantOrRegister value() const { return u.setprop.value.data(); }
    bool strict() const { return u.setprop.strict; }

    bool attachNativeExisting(JSContext *cx, JSObject *obj, const Shape *shape);
};

bool
GetPropertyCache(JSContext *cx, size_t cacheIndex, JSObject *obj, Value *vp);

bool
SetPropertyCache(JSContext *cx, size_t cacheIndex, JSObject *obj, Value value);

} // namespace ion
} // namespace js

#endif // jsion_caches_h__
