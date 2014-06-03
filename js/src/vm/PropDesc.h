/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PropDesc_h
#define vm_PropDesc_h

#include "jsapi.h"
#include "NamespaceImports.h"

namespace js {

class Debugger;

static inline JSPropertyOp
CastAsPropertyOp(JSObject *object)
{
    return JS_DATA_TO_FUNC_PTR(JSPropertyOp, object);
}

static inline JSStrictPropertyOp
CastAsStrictPropertyOp(JSObject *object)
{
    return JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, object);
}

/*
 * A representation of ECMA-262 ed. 5's internal Property Descriptor data
 * structure.
 */
struct PropDesc {
  private:
    /*
     * Original object from which this descriptor derives, passed through for
     * the benefit of proxies.
     */
    Value pd_;

    Value value_, get_, set_;

    /* Property descriptor boolean fields. */
    uint8_t attrs;

    /* Bits indicating which values are set. */
    bool hasGet_ : 1;
    bool hasSet_ : 1;
    bool hasValue_ : 1;
    bool hasWritable_ : 1;
    bool hasEnumerable_ : 1;
    bool hasConfigurable_ : 1;

    /* Or maybe this represents a property's absence, and it's undefined. */
    bool isUndefined_ : 1;

    explicit PropDesc(const Value &v)
      : pd_(UndefinedValue()),
        value_(v),
        get_(UndefinedValue()), set_(UndefinedValue()),
        attrs(0),
        hasGet_(false), hasSet_(false),
        hasValue_(true), hasWritable_(false), hasEnumerable_(false), hasConfigurable_(false),
        isUndefined_(false)
    {
    }

  public:
    friend class AutoPropDescRooter;
    friend void JS::AutoGCRooter::trace(JSTracer *trc);

    enum Enumerability { Enumerable = true, NonEnumerable = false };
    enum Configurability { Configurable = true, NonConfigurable = false };
    enum Writability { Writable = true, NonWritable = false };

    PropDesc();

    static PropDesc undefined() { return PropDesc(); }
    static PropDesc valueOnly(const Value &v) { return PropDesc(v); }

    PropDesc(const Value &v, Writability writable,
             Enumerability enumerable, Configurability configurable)
      : pd_(UndefinedValue()),
        value_(v),
        get_(UndefinedValue()), set_(UndefinedValue()),
        attrs((writable ? 0 : JSPROP_READONLY) |
              (enumerable ? JSPROP_ENUMERATE : 0) |
              (configurable ? 0 : JSPROP_PERMANENT)),
        hasGet_(false), hasSet_(false),
        hasValue_(true), hasWritable_(true), hasEnumerable_(true), hasConfigurable_(true),
        isUndefined_(false)
    {}

    inline PropDesc(const Value &getter, const Value &setter,
                    Enumerability enumerable, Configurability configurable);

    /*
     * 8.10.5 ToPropertyDescriptor(Obj)
     *
     * If checkAccessors is false, skip steps 7.b and 8.b, which throw a
     * TypeError if .get or .set is neither a callable object nor undefined.
     *
     * (DebuggerObject_defineProperty uses this: the .get and .set properties
     * are expected to be Debugger.Object wrappers of functions, which are not
     * themselves callable.)
     */
    bool initialize(JSContext *cx, const Value &v, bool checkAccessors = true);

    /*
     * If IsGenericDescriptor(desc) or IsDataDescriptor(desc) is true, then if
     * the value of an attribute field of desc, considered as a data
     * descriptor, is absent, set it to its default value. Else if the value of
     * an attribute field of desc, considered as an attribute descriptor, is
     * absent, set it to its default value.
     */
    void complete();

    /*
     * 8.10.4 FromPropertyDescriptor(Desc)
     *
     * initFromPropertyDescriptor sets pd to undefined and populates all the
     * other fields of this PropDesc from desc.
     *
     * makeObject populates pd based on the other fields of *this, creating a
     * new property descriptor JSObject and defining properties on it.
     */
    void initFromPropertyDescriptor(Handle<JSPropertyDescriptor> desc);
    bool makeObject(JSContext *cx);

    void setUndefined() { isUndefined_ = true; }

    bool isUndefined() const { return isUndefined_; }

    bool hasGet() const { MOZ_ASSERT(!isUndefined()); return hasGet_; }
    bool hasSet() const { MOZ_ASSERT(!isUndefined()); return hasSet_; }
    bool hasValue() const { MOZ_ASSERT(!isUndefined()); return hasValue_; }
    bool hasWritable() const { MOZ_ASSERT(!isUndefined()); return hasWritable_; }
    bool hasEnumerable() const { MOZ_ASSERT(!isUndefined()); return hasEnumerable_; }
    bool hasConfigurable() const { MOZ_ASSERT(!isUndefined()); return hasConfigurable_; }

    Value pd() const { MOZ_ASSERT(!isUndefined()); return pd_; }
    void clearPd() { pd_ = UndefinedValue(); }

    uint8_t attributes() const { MOZ_ASSERT(!isUndefined()); return attrs; }

    /* 8.10.1 IsAccessorDescriptor(desc) */
    bool isAccessorDescriptor() const {
        return !isUndefined() && (hasGet() || hasSet());
    }

    /* 8.10.2 IsDataDescriptor(desc) */
    bool isDataDescriptor() const {
        return !isUndefined() && (hasValue() || hasWritable());
    }

    /* 8.10.3 IsGenericDescriptor(desc) */
    bool isGenericDescriptor() const {
        return !isUndefined() && !isAccessorDescriptor() && !isDataDescriptor();
    }

    bool configurable() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasConfigurable());
        return (attrs & JSPROP_PERMANENT) == 0;
    }

    bool enumerable() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasEnumerable());
        return (attrs & JSPROP_ENUMERATE) != 0;
    }

    bool writable() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasWritable());
        return (attrs & JSPROP_READONLY) == 0;
    }

    HandleValue value() const {
        MOZ_ASSERT(hasValue());
        return HandleValue::fromMarkedLocation(&value_);
    }

    JSObject * getterObject() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasGet());
        return get_.isUndefined() ? nullptr : &get_.toObject();
    }
    JSObject * setterObject() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasSet());
        return set_.isUndefined() ? nullptr : &set_.toObject();
    }

    HandleValue getterValue() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasGet());
        return HandleValue::fromMarkedLocation(&get_);
    }
    HandleValue setterValue() const {
        MOZ_ASSERT(!isUndefined());
        MOZ_ASSERT(hasSet());
        return HandleValue::fromMarkedLocation(&set_);
    }

    /*
     * Unfortunately the values produced by these methods are used such that
     * we can't assert anything here.  :-(
     */
    JSPropertyOp getter() const {
        return CastAsPropertyOp(get_.isUndefined() ? nullptr : &get_.toObject());
    }
    JSStrictPropertyOp setter() const {
        return CastAsStrictPropertyOp(set_.isUndefined() ? nullptr : &set_.toObject());
    }

    /*
     * Throw a TypeError if a getter/setter is present and is neither callable
     * nor undefined. These methods do exactly the type checks that are skipped
     * by passing false as the checkAccessors parameter of initialize.
     */
    bool checkGetter(JSContext *cx);
    bool checkSetter(JSContext *cx);

    bool unwrapDebuggerObjectsInto(JSContext *cx, Debugger *dbg, HandleObject obj,
                                   PropDesc *unwrapped) const;

    bool wrapInto(JSContext *cx, HandleObject obj, const jsid &id, jsid *wrappedId,
                  PropDesc *wrappedDesc) const;
};

class AutoPropDescRooter : private JS::CustomAutoRooter
{
  public:
    explicit AutoPropDescRooter(JSContext *cx
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : CustomAutoRooter(cx)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    PropDesc& getPropDesc() { return propDesc; }

    void initFromPropertyDescriptor(Handle<JSPropertyDescriptor> desc) {
        propDesc.initFromPropertyDescriptor(desc);
    }

    bool makeObject(JSContext *cx) {
        return propDesc.makeObject(cx);
    }

    void setUndefined() { propDesc.setUndefined(); }
    bool isUndefined() const { return propDesc.isUndefined(); }

    bool hasGet() const { return propDesc.hasGet(); }
    bool hasSet() const { return propDesc.hasSet(); }
    bool hasValue() const { return propDesc.hasValue(); }
    bool hasWritable() const { return propDesc.hasWritable(); }
    bool hasEnumerable() const { return propDesc.hasEnumerable(); }
    bool hasConfigurable() const { return propDesc.hasConfigurable(); }

    Value pd() const { return propDesc.pd(); }
    void clearPd() { propDesc.clearPd(); }

    uint8_t attributes() const { return propDesc.attributes(); }

    bool isAccessorDescriptor() const { return propDesc.isAccessorDescriptor(); }
    bool isDataDescriptor() const { return propDesc.isDataDescriptor(); }
    bool isGenericDescriptor() const { return propDesc.isGenericDescriptor(); }
    bool configurable() const { return propDesc.configurable(); }
    bool enumerable() const { return propDesc.enumerable(); }
    bool writable() const { return propDesc.writable(); }

    HandleValue value() const { return propDesc.value(); }
    JSObject *getterObject() const { return propDesc.getterObject(); }
    JSObject *setterObject() const { return propDesc.setterObject(); }
    HandleValue getterValue() const { return propDesc.getterValue(); }
    HandleValue setterValue() const { return propDesc.setterValue(); }

    JSPropertyOp getter() const { return propDesc.getter(); }
    JSStrictPropertyOp setter() const { return propDesc.setter(); }

  private:
    virtual void trace(JSTracer *trc);

    PropDesc propDesc;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

#endif /* vm_PropDesc_h */
