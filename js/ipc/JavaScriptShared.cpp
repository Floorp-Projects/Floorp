/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptShared.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/CPOWManagerGetter.h"
#include "mozilla/dom/TabChild.h"
#include "jsfriendapi.h"
#include "xpcprivate.h"
#include "WrapperFactory.h"
#include "mozilla/Preferences.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;

IdToObjectMap::IdToObjectMap()
  : table_(SystemAllocPolicy())
{
}

bool
IdToObjectMap::init()
{
    if (table_.initialized())
        return true;
    return table_.init(32);
}

void
IdToObjectMap::trace(JSTracer* trc)
{
    for (Table::Range r(table_.all()); !r.empty(); r.popFront()) {
        DebugOnly<JSObject*> prior = r.front().value().get();
        JS_CallObjectTracer(trc, &r.front().value(), "ipc-object");
    }
}

void
IdToObjectMap::sweep()
{
    for (Table::Enum e(table_); !e.empty(); e.popFront()) {
        JS::Heap<JSObject*>* objp = &e.front().value();
        JS_UpdateWeakPointerAfterGC(objp);
        if (!*objp)
            e.removeFront();
    }
}

JSObject*
IdToObjectMap::find(ObjectId id)
{
    Table::Ptr p = table_.lookup(id);
    if (!p)
        return nullptr;
    return p->value();
}

bool
IdToObjectMap::add(ObjectId id, JSObject* obj)
{
    return table_.put(id, obj);
}

void
IdToObjectMap::remove(ObjectId id)
{
    table_.remove(id);
}

void
IdToObjectMap::clear()
{
    table_.clear();
}

bool
IdToObjectMap::empty() const
{
    return table_.empty();
}

ObjectToIdMap::ObjectToIdMap(JSRuntime* rt)
  : rt_(rt)
{
}

ObjectToIdMap::~ObjectToIdMap()
{
    JS_ClearAllPostBarrierCallbacks(rt_);
}

bool
ObjectToIdMap::init()
{
    return table_.initialized() || table_.init(32);
}

void
ObjectToIdMap::trace(JSTracer* trc)
{
    for (Table::Enum e(table_); !e.empty(); e.popFront()) {
        JSObject* obj = e.front().key();
        JS_CallUnbarrieredObjectTracer(trc, &obj, "ipc-object");
        if (obj != e.front().key())
            e.rekeyFront(obj);
    }
}

void
ObjectToIdMap::sweep()
{
    for (Table::Enum e(table_); !e.empty(); e.popFront()) {
        JSObject* obj = e.front().key();
        JS_UpdateWeakPointerAfterGCUnbarriered(&obj);
        if (!obj)
            e.removeFront();
        else if (obj != e.front().key())
            e.rekeyFront(obj);
    }
}

ObjectId
ObjectToIdMap::find(JSObject* obj)
{
    Table::Ptr p = table_.lookup(obj);
    if (!p)
        return ObjectId::nullId();
    return p->value();
}

bool
ObjectToIdMap::add(JSContext* cx, JSObject* obj, ObjectId id)
{
    if (!table_.put(obj, id))
        return false;
    JS_StoreObjectPostBarrierCallback(cx, keyMarkCallback, obj, &table_);
    return true;
}

/*
 * This function is called during minor GCs for each key in the HashMap that has
 * been moved.
 */
/* static */ void
ObjectToIdMap::keyMarkCallback(JSTracer* trc, JSObject* key, void* data)
{
    Table* table = static_cast<Table*>(data);
    JSObject* prior = key;
    JS_CallUnbarrieredObjectTracer(trc, &key, "ObjectIdCache::table_ key");
    table->rekeyIfMoved(prior, key);
}

void
ObjectToIdMap::remove(JSObject* obj)
{
    table_.remove(obj);
}

void
ObjectToIdMap::clear()
{
    table_.clear();
}

bool JavaScriptShared::sLoggingInitialized;
bool JavaScriptShared::sLoggingEnabled;
bool JavaScriptShared::sStackLoggingEnabled;

JavaScriptShared::JavaScriptShared(JSRuntime* rt)
  : rt_(rt),
    refcount_(1),
    nextSerialNumber_(1),
    unwaivedObjectIds_(rt),
    waivedObjectIds_(rt)
{
    if (!sLoggingInitialized) {
        sLoggingInitialized = true;

        if (PR_GetEnv("MOZ_CPOW_LOG")) {
            sLoggingEnabled = true;
            sStackLoggingEnabled = strstr(PR_GetEnv("MOZ_CPOW_LOG"), "stacks");
        } else {
            Preferences::AddBoolVarCache(&sLoggingEnabled,
                                         "dom.ipc.cpows.log.enabled", false);
            Preferences::AddBoolVarCache(&sStackLoggingEnabled,
                                         "dom.ipc.cpows.log.stack", false);
        }
    }
}

JavaScriptShared::~JavaScriptShared()
{
    MOZ_RELEASE_ASSERT(cpows_.empty());
}

bool
JavaScriptShared::init()
{
    if (!objects_.init())
        return false;
    if (!cpows_.init())
        return false;
    if (!unwaivedObjectIds_.init())
        return false;
    if (!waivedObjectIds_.init())
        return false;

    return true;
}

void
JavaScriptShared::decref()
{
    refcount_--;
    if (!refcount_)
        delete this;
}

void
JavaScriptShared::incref()
{
    refcount_++;
}

bool
JavaScriptShared::convertIdToGeckoString(JSContext* cx, JS::HandleId id, nsString* to)
{
    RootedValue idval(cx);
    if (!JS_IdToValue(cx, id, &idval))
        return false;

    RootedString str(cx, ToString(cx, idval));
    if (!str)
        return false;

    return AssignJSString(cx, *to, str);
}

bool
JavaScriptShared::convertGeckoStringToId(JSContext* cx, const nsString& from, JS::MutableHandleId to)
{
    RootedString str(cx, JS_NewUCStringCopyN(cx, from.BeginReading(), from.Length()));
    if (!str)
        return false;

    return JS_StringToId(cx, str, to);
}

bool
JavaScriptShared::toVariant(JSContext* cx, JS::HandleValue from, JSVariant* to)
{
    switch (JS_TypeOfValue(cx, from)) {
      case JSTYPE_VOID:
        *to = UndefinedVariant();
        return true;

      case JSTYPE_OBJECT:
      case JSTYPE_FUNCTION:
      {
        RootedObject obj(cx, from.toObjectOrNull());
        if (!obj) {
            MOZ_ASSERT(from.isNull());
            *to = NullVariant();
            return true;
        }

        if (xpc_JSObjectIsID(cx, obj)) {
            JSIID iid;
            const nsID* id = xpc_JSObjectToID(cx, obj);
            ConvertID(*id, &iid);
            *to = iid;
            return true;
        }

        ObjectVariant objVar;
        if (!toObjectVariant(cx, obj, &objVar))
            return false;
        *to = objVar;
        return true;
      }

      case JSTYPE_SYMBOL:
      {
        RootedSymbol sym(cx, from.toSymbol());

        SymbolVariant symVar;
        if (!toSymbolVariant(cx, sym, &symVar))
            return false;
        *to = symVar;
        return true;
      }

      case JSTYPE_STRING:
      {
        nsAutoJSString autoStr;
        if (!autoStr.init(cx, from))
            return false;
        *to = autoStr;
        return true;
      }

      case JSTYPE_NUMBER:
        if (from.isInt32())
            *to = double(from.toInt32());
        else
            *to = from.toDouble();
        return true;

      case JSTYPE_BOOLEAN:
        *to = from.toBoolean();
        return true;

      default:
        MOZ_ASSERT(false);
        return false;
    }
}

bool
JavaScriptShared::fromVariant(JSContext* cx, const JSVariant& from, MutableHandleValue to)
{
    switch (from.type()) {
        case JSVariant::TUndefinedVariant:
          to.set(UndefinedValue());
          return true;

        case JSVariant::TNullVariant:
          to.set(NullValue());
          return true;

        case JSVariant::TObjectVariant:
        {
          JSObject* obj = fromObjectVariant(cx, from.get_ObjectVariant());
          if (!obj)
              return false;
          to.set(ObjectValue(*obj));
          return true;
        }

        case JSVariant::TSymbolVariant:
        {
          Symbol* sym = fromSymbolVariant(cx, from.get_SymbolVariant());
          if (!sym)
              return false;
          to.setSymbol(sym);
          return true;
        }

        case JSVariant::Tdouble:
          to.set(JS_NumberValue(from.get_double()));
          return true;

        case JSVariant::Tbool:
          to.setBoolean(from.get_bool());
          return true;

        case JSVariant::TnsString:
        {
          const nsString& old = from.get_nsString();
          JSString* str = JS_NewUCStringCopyN(cx, old.BeginReading(), old.Length());
          if (!str)
              return false;
          to.set(StringValue(str));
          return true;
        }

        case JSVariant::TJSIID:
        {
          nsID iid;
          const JSIID& id = from.get_JSIID();
          ConvertID(id, &iid);

          JSCompartment* compartment = GetContextCompartment(cx);
          RootedObject global(cx, JS_GetGlobalForCompartmentOrNull(cx, compartment));
          JSObject* obj = xpc_NewIDObject(cx, global, iid);
          if (!obj)
              return false;
          to.set(ObjectValue(*obj));
          return true;
        }

        default:
          MOZ_CRASH("NYI");
          return false;
    }
}

bool
JavaScriptShared::toJSIDVariant(JSContext* cx, HandleId from, JSIDVariant* to)
{
    if (JSID_IS_STRING(from)) {
        nsAutoJSString autoStr;
        if (!autoStr.init(cx, JSID_TO_STRING(from)))
            return false;
        *to = autoStr;
        return true;
    }
    if (JSID_IS_INT(from)) {
        *to = JSID_TO_INT(from);
        return true;
    }
    if (JSID_IS_SYMBOL(from)) {
        SymbolVariant symVar;
        if (!toSymbolVariant(cx, JSID_TO_SYMBOL(from), &symVar))
            return false;
        *to = symVar;
        return true;
    }
    MOZ_ASSERT(false);
    return false;
}

bool
JavaScriptShared::fromJSIDVariant(JSContext* cx, const JSIDVariant& from, MutableHandleId to)
{
    switch (from.type()) {
      case JSIDVariant::TSymbolVariant: {
        Symbol* sym = fromSymbolVariant(cx, from.get_SymbolVariant());
        if (!sym)
            return false;
        to.set(SYMBOL_TO_JSID(sym));
        return true;
      }

      case JSIDVariant::TnsString:
        return convertGeckoStringToId(cx, from.get_nsString(), to);

      case JSIDVariant::Tint32_t:
        to.set(INT_TO_JSID(from.get_int32_t()));
        return true;

      default:
        return false;
    }
}

bool
JavaScriptShared::toSymbolVariant(JSContext* cx, JS::Symbol* symArg, SymbolVariant* symVarp)
{
    RootedSymbol sym(cx, symArg);
    MOZ_ASSERT(sym);

    SymbolCode code = GetSymbolCode(sym);
    if (static_cast<uint32_t>(code) < WellKnownSymbolLimit) {
        *symVarp = WellKnownSymbol(static_cast<uint32_t>(code));
        return true;
    }
    if (code == SymbolCode::InSymbolRegistry) {
        nsAutoJSString autoStr;
        if (!autoStr.init(cx, GetSymbolDescription(sym)))
            return false;
        *symVarp = RegisteredSymbol(autoStr);
        return true;
    }

    JS_ReportError(cx, "unique symbol can't be used with CPOW");
    return false;
}

JS::Symbol*
JavaScriptShared::fromSymbolVariant(JSContext* cx, SymbolVariant symVar)
{
    switch (symVar.type()) {
      case SymbolVariant::TWellKnownSymbol: {
        uint32_t which = symVar.get_WellKnownSymbol().which();
        if (which < WellKnownSymbolLimit)
            return GetWellKnownSymbol(cx, static_cast<SymbolCode>(which));
        MOZ_ASSERT(false, "bad child data");
        return nullptr;
      }

      case SymbolVariant::TRegisteredSymbol: {
        nsString key = symVar.get_RegisteredSymbol().key();
        RootedString str(cx, JS_NewUCStringCopyN(cx, key.get(), key.Length()));
        if (!str)
            return nullptr;
        return GetSymbolFor(cx, str);
      }

      default:
        return nullptr;
    }
}

/* static */ void
JavaScriptShared::ConvertID(const nsID& from, JSIID* to)
{
    to->m0() = from.m0;
    to->m1() = from.m1;
    to->m2() = from.m2;
    to->m3_0() = from.m3[0];
    to->m3_1() = from.m3[1];
    to->m3_2() = from.m3[2];
    to->m3_3() = from.m3[3];
    to->m3_4() = from.m3[4];
    to->m3_5() = from.m3[5];
    to->m3_6() = from.m3[6];
    to->m3_7() = from.m3[7];
}

/* static */ void
JavaScriptShared::ConvertID(const JSIID& from, nsID* to)
{
    to->m0 = from.m0();
    to->m1 = from.m1();
    to->m2 = from.m2();
    to->m3[0] = from.m3_0();
    to->m3[1] = from.m3_1();
    to->m3[2] = from.m3_2();
    to->m3[3] = from.m3_3();
    to->m3[4] = from.m3_4();
    to->m3[5] = from.m3_5();
    to->m3[6] = from.m3_6();
    to->m3[7] = from.m3_7();
}

JSObject*
JavaScriptShared::findObjectById(JSContext* cx, const ObjectId& objId)
{
    RootedObject obj(cx, objects_.find(objId));
    if (!obj) {
        JS_ReportError(cx, "operation not possible on dead CPOW");
        return nullptr;
    }

    // Each process has a dedicated compartment for CPOW targets. All CPOWs
    // from the other process point to objects in this scope. From there, they
    // can access objects in other compartments using cross-compartment
    // wrappers.
    JSAutoCompartment ac(cx, scopeForTargetObjects());
    if (objId.hasXrayWaiver()) {
        {
            JSAutoCompartment ac2(cx, obj);
            obj = JS_ObjectToOuterObject(cx, obj);
            if (!obj)
                return nullptr;
        }
        if (!xpc::WrapperFactory::WaiveXrayAndWrap(cx, &obj))
            return nullptr;
    } else {
        if (!JS_WrapObject(cx, &obj))
            return nullptr;
    }
    return obj;
}

static const uint64_t UnknownPropertyOp = 1;

bool
JavaScriptShared::fromDescriptor(JSContext* cx, Handle<JSPropertyDescriptor> desc,
                                 PPropertyDescriptor* out)
{
    out->attrs() = desc.attributes();
    if (!toVariant(cx, desc.value(), &out->value()))
        return false;

    if (!toObjectOrNullVariant(cx, desc.object(), &out->obj()))
        return false;

    if (!desc.getter()) {
        out->getter() = 0;
    } else if (desc.hasGetterObject()) {
        JSObject* getter = desc.getterObject();
        ObjectVariant objVar;
        if (!toObjectVariant(cx, getter, &objVar))
            return false;
        out->getter() = objVar;
    } else {
        MOZ_ASSERT(desc.getter() != JS_PropertyStub);
        out->getter() = UnknownPropertyOp;
    }

    if (!desc.setter()) {
        out->setter() = 0;
    } else if (desc.hasSetterObject()) {
        JSObject* setter = desc.setterObject();
        ObjectVariant objVar;
        if (!toObjectVariant(cx, setter, &objVar))
            return false;
        out->setter() = objVar;
    } else {
        MOZ_ASSERT(desc.setter() != JS_StrictPropertyStub);
        out->setter() = UnknownPropertyOp;
    }

    return true;
}

bool
UnknownPropertyStub(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    JS_ReportError(cx, "getter could not be wrapped via CPOWs");
    return false;
}

bool
UnknownStrictPropertyStub(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp,
                          ObjectOpResult& result)
{
    JS_ReportError(cx, "setter could not be wrapped via CPOWs");
    return false;
}

bool
JavaScriptShared::toDescriptor(JSContext* cx, const PPropertyDescriptor& in,
                               MutableHandle<JSPropertyDescriptor> out)
{
    out.setAttributes(in.attrs());
    if (!fromVariant(cx, in.value(), out.value()))
        return false;
    out.object().set(fromObjectOrNullVariant(cx, in.obj()));

    if (in.getter().type() == GetterSetter::Tuint64_t && !in.getter().get_uint64_t()) {
        out.setGetter(nullptr);
    } else if (in.attrs() & JSPROP_GETTER) {
        Rooted<JSObject*> getter(cx);
        getter = fromObjectVariant(cx, in.getter().get_ObjectVariant());
        if (!getter)
            return false;
        out.setGetter(JS_DATA_TO_FUNC_PTR(JSGetterOp, getter.get()));
    } else {
        out.setGetter(UnknownPropertyStub);
    }

    if (in.setter().type() == GetterSetter::Tuint64_t && !in.setter().get_uint64_t()) {
        out.setSetter(nullptr);
    } else if (in.attrs() & JSPROP_SETTER) {
        Rooted<JSObject*> setter(cx);
        setter = fromObjectVariant(cx, in.setter().get_ObjectVariant());
        if (!setter)
            return false;
        out.setSetter(JS_DATA_TO_FUNC_PTR(JSSetterOp, setter.get()));
    } else {
        out.setSetter(UnknownStrictPropertyStub);
    }

    return true;
}

bool
JavaScriptShared::toObjectOrNullVariant(JSContext* cx, JSObject* obj, ObjectOrNullVariant* objVarp)
{
    if (!obj) {
        *objVarp = NullVariant();
        return true;
    }

    ObjectVariant objVar;
    if (!toObjectVariant(cx, obj, &objVar))
        return false;

    *objVarp = objVar;
    return true;
}

JSObject*
JavaScriptShared::fromObjectOrNullVariant(JSContext* cx, ObjectOrNullVariant objVar)
{
    if (objVar.type() == ObjectOrNullVariant::TNullVariant)
        return nullptr;

    return fromObjectVariant(cx, objVar.get_ObjectVariant());
}

CrossProcessCpowHolder::CrossProcessCpowHolder(dom::CPOWManagerGetter* managerGetter,
                                               const InfallibleTArray<CpowEntry>& cpows)
  : js_(nullptr),
    cpows_(cpows)
{
    // Only instantiate the CPOW manager if we might need it later.
    if (cpows.Length())
        js_ = managerGetter->GetCPOWManager();
}

bool
CrossProcessCpowHolder::ToObject(JSContext* cx, JS::MutableHandleObject objp)
{
    if (!cpows_.Length())
        return true;

    return js_->Unwrap(cx, cpows_, objp);
}

bool
JavaScriptShared::Unwrap(JSContext* cx, const InfallibleTArray<CpowEntry>& aCpows,
                         JS::MutableHandleObject objp)
{
    objp.set(nullptr);

    if (!aCpows.Length())
        return true;

    RootedObject obj(cx, JS_NewPlainObject(cx));
    if (!obj)
        return false;

    RootedValue v(cx);
    RootedString str(cx);
    for (size_t i = 0; i < aCpows.Length(); i++) {
        const nsString& name = aCpows[i].name();

        if (!fromVariant(cx, aCpows[i].value(), &v))
            return false;

        if (!JS_DefineUCProperty(cx,
                                 obj,
                                 name.BeginReading(),
                                 name.Length(),
                                 v,
                                 JSPROP_ENUMERATE))
        {
            return false;
        }
    }

    objp.set(obj);
    return true;
}

bool
JavaScriptShared::Wrap(JSContext* cx, HandleObject aObj, InfallibleTArray<CpowEntry>* outCpows)
{
    if (!aObj)
        return true;

    AutoIdArray ids(cx, JS_Enumerate(cx, aObj));
    if (!ids)
        return false;

    RootedId id(cx);
    RootedValue v(cx);
    for (size_t i = 0; i < ids.length(); i++) {
        id = ids[i];

        nsString str;
        if (!convertIdToGeckoString(cx, id, &str))
            return false;

        if (!JS_GetPropertyById(cx, aObj, id, &v))
            return false;

        JSVariant var;
        if (!toVariant(cx, v, &var))
            return false;

        outCpows->AppendElement(CpowEntry(str, var));
    }

    return true;
}

CPOWManager*
mozilla::jsipc::CPOWManagerFor(PJavaScriptParent* aParent)
{
    return static_cast<JavaScriptParent*>(aParent);
}

CPOWManager*
mozilla::jsipc::CPOWManagerFor(PJavaScriptChild* aChild)
{
    return static_cast<JavaScriptChild*>(aChild);
}
