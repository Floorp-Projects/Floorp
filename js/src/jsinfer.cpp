/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey bytecode type inference
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "jsapi.h"
#include "jsautooplen.h"
#include "jsbit.h"
#include "jsbool.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsinfer.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jscntxt.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstl.h"
#include "jsiter.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include <zlib.h>

#ifdef JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

static inline jsid
id_prototype(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
}

static inline jsid
id_arguments(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
}

static inline jsid
id_length(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
}

static inline jsid
id___proto__(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.protoAtom);
}

static inline jsid
id_constructor(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.constructorAtom);
}

static inline jsid
id_caller(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.callerAtom);
}

static inline jsid
id_toString(JSContext *cx)
{
    return ATOM_TO_JSID(cx->runtime->atomState.toStringAtom);
}

static inline jsid
id_toSource(JSContext *cx)
{
    return ATOM_TO_JSID(cx->runtime->atomState.toSourceAtom);
}

namespace js {
namespace types {

#ifdef JS_TYPES_DEBUG_SPEW
unsigned TypeSet::typesetCount = 0;
unsigned TypeConstraint::constraintCount = 0;
#endif

static const char *js_CodeNameTwo[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    name,
#include "jsopcode.tbl"
#undef OPDEF
};

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
#ifdef JS_TYPES_DEBUG_SPEW
    JS_ASSERT(id);
    fprintf(cx->typeOut(), "addConstraint: T%u C%u %s\n",
            id, constraint->id, constraint->kind);
#endif

    JS_ASSERT(constraint->next == NULL);
    constraint->next = constraintList;
    constraintList = constraint;

    if (!callExisting)
        return;

    if (typeFlags & TYPE_FLAG_UNKNOWN) {
        cx->compartment->types.addPending(cx, constraint, this, TYPE_UNKNOWN);
        cx->compartment->types.resolvePending(cx);
        return;
    }

    for (jstype type = TYPE_UNDEFINED; type <= TYPE_STRING; type++) {
        if (typeFlags & (1 << type))
            cx->compartment->types.addPending(cx, constraint, this, type);
    }

    if (objectCount >= 2) {
        unsigned objectCapacity = HashSetCapacity(objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = objectSet[i];
            if (object)
                cx->compartment->types.addPending(cx, constraint, this, (jstype) object);
        }
    } else if (objectCount == 1) {
        TypeObject *object = (TypeObject*) objectSet;
        cx->compartment->types.addPending(cx, constraint, this, (jstype) object);
    }

    cx->compartment->types.resolvePending(cx);
}

void
TypeSet::print(JSContext *cx, FILE *out)
{
    if (typeFlags == 0) {
        fputs(" missing", out);
        return;
    }

    if (typeFlags & TYPE_FLAG_UNKNOWN)
        fputs(" unknown", out);

    if (typeFlags & TYPE_FLAG_UNDEFINED)
        fputs(" void", out);
    if (typeFlags & TYPE_FLAG_NULL)
        fputs(" null", out);
    if (typeFlags & TYPE_FLAG_BOOLEAN)
        fputs(" bool", out);
    if (typeFlags & TYPE_FLAG_INT32)
        fputs(" int", out);
    if (typeFlags & TYPE_FLAG_DOUBLE)
        fputs(" float", out);
    if (typeFlags & TYPE_FLAG_STRING)
        fputs(" string", out);

    if (typeFlags & TYPE_FLAG_OBJECT) {
        fprintf(out, " object[%u]", objectCount);

        if (objectCount >= 2) {
            unsigned objectCapacity = HashSetCapacity(objectCount);
            for (unsigned i = 0; i < objectCapacity; i++) {
                TypeObject *object = objectSet[i];
                if (object)
                    fprintf(out, " %s", cx->getTypeId(object->name));
            }
        } else if (objectCount == 1) {
            TypeObject *object = (TypeObject*) objectSet;
            fprintf(out, " %s", cx->getTypeId(object->name));
        }
    }
}

void
PrintType(JSContext *cx, jstype type, bool newline)
{
    JS_ASSERT(type);

    TypeSet types(&cx->compartment->types.pool);
    PodZero(&types);

    if (type == TYPE_UNKNOWN) {
        types.typeFlags = TYPE_FLAG_UNKNOWN;
    } else if (TypeIsPrimitive(type)) {
        types.typeFlags = 1 << type;
    } else {
        types.typeFlags = TYPE_FLAG_OBJECT;
        types.objectSet = (TypeObject**) type;
        types.objectCount = 1;
    }

    types.print(cx, cx->typeOut());

    if (newline)
        fputs("\n", cx->typeOut());
}

/* Standard subset constraint, propagate all types from one set to another. */
class TypeConstraintSubset : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintSubset(TypeSet *target)
        : TypeConstraint("subset"), target(target)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addSubset(JSContext *cx, JSArenaPool &pool, TypeSet *target)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintSubset>(pool, target));
}

/* Constraints for reads/writes on object properties. */
class TypeConstraintProp : public TypeConstraint
{
public:
    /* Bytecode this access occurs at. */
    analyze::Bytecode *code;

    /*
     * If assign is true, the target is used to update a property of the object.
     * If assign is false, the target is assigned the value of the property.
     */
    bool assign;
    TypeSet *target;

    /* Property being accessed. */
    jsid id;

    TypeConstraintProp(analyze::Bytecode *code, TypeSet *target, jsid id, bool assign)
        : TypeConstraint("prop"), code(code), assign(assign), target(target), id(id)
    {
        JS_ASSERT(code);
        JS_ASSERT(id == MakeTypeId(id));

        /* If the target is NULL, this is as an inc/dec on the property. */
        JS_ASSERT_IF(!target, assign);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addGetProperty(JSContext *cx, analyze::Bytecode *code, TypeSet *target, jsid id)
{
    JS_ASSERT(this->pool == &code->pool());
    add(cx, ArenaNew<TypeConstraintProp>(code->pool(), code, target, id, false));
}

void
TypeSet::addSetProperty(JSContext *cx, analyze::Bytecode *code, TypeSet *target, jsid id)
{
    JS_ASSERT(this->pool == &code->pool());
    add(cx, ArenaNew<TypeConstraintProp>(code->pool(), code, target, id, true));
}

/*
 * Constraints for reads/writes on object elements, which may either be integer
 * element accesses or arbitrary accesses depending on the index type.
 */
class TypeConstraintElem : public TypeConstraint
{
public:
    /* Bytecode performing the element access. */
    analyze::Bytecode *code;

    /* Types of the object being accessed. */
    TypeSet *object;

    /* Target to use for the ConstraintProp. */
    TypeSet *target;

    /* As for ConstraintProp. */
    bool assign;

    TypeConstraintElem(analyze::Bytecode *code, TypeSet *object, TypeSet *target, bool assign)
        : TypeConstraint("elem"), code(code), object(object), target(target), assign(assign)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addGetElem(JSContext *cx, analyze::Bytecode *code, TypeSet *object, TypeSet *target)
{
    JS_ASSERT(this->pool == &code->pool());
    add(cx, ArenaNew<TypeConstraintElem>(code->pool(), code, object, target, false));
}

void
TypeSet::addSetElem(JSContext *cx, analyze::Bytecode *code, TypeSet *object, TypeSet *target)
{
    JS_ASSERT(this->pool == &code->pool());
    add(cx, ArenaNew<TypeConstraintElem>(code->pool(), code, object, target, true));
}

/*
 * Constraints for watching call edges as they are discovered and invoking native
 * function handlers, adding constraints for arguments, receiver objects and the
 * return value, and updating script foundOffsets.
 */
class TypeConstraintCall : public TypeConstraint
{
public:
    /* Call site being tracked. */
    TypeCallsite *callsite;

    TypeConstraintCall(TypeCallsite *callsite)
        : TypeConstraint("call"), callsite(callsite)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addCall(JSContext *cx, TypeCallsite *site)
{
    JS_ASSERT(this->pool == &site->pool());
    add(cx, ArenaNew<TypeConstraintCall>(site->pool(), site));
}

/* Constraints for arithmetic operations. */
class TypeConstraintArith : public TypeConstraint
{
public:
    /* Bytecode the operation occurs at. */
    analyze::Bytecode *code;

    /* Type set receiving the result of the arithmetic. */
    TypeSet *target;

    /* For addition operations, the other operand. */
    TypeSet *other;

    TypeConstraintArith(analyze::Bytecode *code, TypeSet *target, TypeSet *other)
        : TypeConstraint("arith"), code(code), target(target), other(other)
    {
        JS_ASSERT(code);
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addArith(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code,
                  TypeSet *target, TypeSet *other)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintArith>(pool, code, target, other));
}

/* Subset constraint which transforms primitive values into appropriate objects. */
class TypeConstraintTransformThis : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintTransformThis(TypeSet *target)
        : TypeConstraint("transformthis"), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addTransformThis(JSContext *cx, JSArenaPool &pool, TypeSet *target)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintTransformThis>(pool, target));
}

/* Subset constraint which filters out primitive types. */
class TypeConstraintFilterPrimitive : public TypeConstraint
{
public:
    TypeSet *target;

    /* Primitive types other than null and undefined are passed through. */
    bool onlyNullVoid;

    TypeConstraintFilterPrimitive(TypeSet *target, bool onlyNullVoid)
        : TypeConstraint("filter"), target(target), onlyNullVoid(onlyNullVoid)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addFilterPrimitives(JSContext *cx, JSArenaPool &pool, TypeSet *target, bool onlyNullVoid)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintFilterPrimitive>(pool, target, onlyNullVoid));
}

/*
 * Subset constraint for property reads which monitors accesses on properties
 * with scripted getters and polymorphic types.
 */
class TypeConstraintMonitorRead : public TypeConstraint
{
public:
    analyze::Bytecode *code;
    TypeSet *target;

    TypeConstraintMonitorRead(analyze::Bytecode *code, TypeSet *target)
        : TypeConstraint("monitorRead"), code(code), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addMonitorRead(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code, TypeSet *target)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintMonitorRead>(pool, code, target));
}

/////////////////////////////////////////////////////////////////////
// TypeConstraint
/////////////////////////////////////////////////////////////////////

void
TypeConstraintSubset::newType(JSContext *cx, TypeSet *source, jstype type)
{
    /* Basic subset constraint, move all types to the target. */
    target->addType(cx, type);
}

/* Get the object to use for a property access on type. */
static inline TypeObject *
GetPropertyObject(JSContext *cx, jstype type)
{
    if (TypeIsObject(type))
        return (TypeObject*) type;

    /*
     * Handle properties attached to primitive types, treating this access as a
     * read on the primitive's new object.
     */
    switch (type) {

      case TYPE_INT32:
      case TYPE_DOUBLE:
        if (!cx->globalObject->getReservedSlot(JSProto_Number).isObject())
            js_InitNumberClass(cx, cx->globalObject);
        return cx->getFixedTypeObject(TYPE_OBJECT_NEW_NUMBER);

      case TYPE_BOOLEAN:
        if (!cx->globalObject->getReservedSlot(JSProto_Boolean).isObject())
            js_InitBooleanClass(cx, cx->globalObject);
        return cx->getFixedTypeObject(TYPE_OBJECT_NEW_BOOLEAN);

      case TYPE_STRING:
        if (!cx->globalObject->getReservedSlot(JSProto_String).isObject())
            js_InitStringClass(cx, cx->globalObject);
        return cx->getFixedTypeObject(TYPE_OBJECT_NEW_STRING);

      default:
        /* undefined and null do not have properties. */
        return NULL;
    }
}

void
TypeConstraintProp::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        /* Access on an unknown object. this bytecode needs to be monitored. */
        cx->compartment->types.monitorBytecode(code);
        return;
    }

    /* Monitor any bytecodes reading from objects which need to be monitored. */
    if (!assign && TypeIsObject(type)) {
        TypeObject *object = (TypeObject*)type;
        if (object->monitored)
            cx->compartment->types.monitorBytecode(code);
    }

    /* Monitor assigns on the 'prototype' property. */
    if (assign && id == id_prototype(cx)) {
        cx->compartment->types.monitorBytecode(code);
        return;
    }

    /* Monitor accesses on other properties with special behavior we don't keep track of. */
    if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx)) {
        cx->compartment->types.monitorBytecode(code);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, type);
    if (!object)
        return;

    TypeSet *types = object->properties(cx).getVariable(cx, id);

    /* Capture the effects of a standard property access. */
    if (target) {
        if (assign)
            target->addSubset(cx, code->pool(), types);
        else
            types->addMonitorRead(cx, object->pool(), code, target);
    } else {
        types->addArith(cx, object->pool(), code, types);
    }
}

void
TypeConstraintElem::newType(JSContext *cx, TypeSet *source, jstype type)
{
    switch (type) {
      case TYPE_INT32:
      case TYPE_DOUBLE:
        /* Integer index access, these are all covered by the JSID_VOID property. */
        if (assign)
            object->addSetProperty(cx, code, target, JSID_VOID);
        else
            object->addGetProperty(cx, code, target, JSID_VOID);

        /*
         * Optimistically treat double accesses as getting an integer property,
         * This needs to be checked at runtime.
         */
        if (type == TYPE_DOUBLE)
            cx->compartment->types.monitorBytecode(code);
        break;
      default:
        /*
         * Access to a potentially arbitrary element.  Monitor assignments to unknown
         * elements, and treat reads of unknown elements as unknown.  TODO: we need to
         * identify hashmap uses either ahead of time or on the fly, as currently we
         * will make separate entries in the variable set of the properties for every
         * element added.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(code);
        else
            target->addType(cx, TYPE_UNKNOWN);
    }
};

void
TypeConstraintCall::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        /* Monitor calls on unknown functions. */
        cx->compartment->types.monitorBytecode(callsite->code);
        return;
    }

    /* Monitor the return values of calls on non-function objects. */
    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject*)type;
        if (!object->isFunction)
            cx->compartment->types.monitorBytecode(callsite->code);
    }

    /* Get the function being invoked. */
    TypeFunction *function = NULL;
    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject*) type;
        if (object->isFunction)
            function = (TypeFunction*) object;
    }
    if (!function)
        return;

    JSArenaPool &pool = callsite->pool();

    if (!function->script) {
        JS_ASSERT(function->handler);

        if (function->handler == JS_TypeHandlerDynamic) {
            /* Calls to functions with dynamic handlers need to be monitored. */
            cx->compartment->types.monitorBytecode(callsite->code);

            /* Add any guesses about the return types of this function to the callsite. */
            if (callsite->returnTypes)
                function->returnTypes.addSubset(cx, function->pool(), callsite->returnTypes);
        } else if (function->isGeneric) {
            if (callsite->argumentCount == 0) {
                /* Generic methods called with zero arguments generate runtime errors. */
                return;
            }

            /*
             * Make a new callsite transforming the arguments appropriately, as is
             * done by the generic native dispatchers. watch out for cases where the
             * first argument is null, which will transform to the global object.
             */

            TypeSet *thisTypes = TypeSet::make(cx, pool, "genericthis");
            callsite->argumentTypes[0]->addTransformThis(cx, pool, thisTypes);

            TypeCallsite *newSite = ArenaNew<TypeCallsite>(pool, callsite->code, callsite->isNew,
                                                           callsite->argumentCount - 1);
            newSite->thisTypes = thisTypes;
            newSite->returnTypes = callsite->returnTypes;
            for (unsigned i = 0; i < callsite->argumentCount - 1; i++)
                newSite->argumentTypes[i] = callsite->argumentTypes[i + 1];

            function->handler(cx, (JSTypeFunction*)function, (JSTypeCallsite*)newSite);
        } else {
            /* Model the function's effects directly. */
            function->handler(cx, (JSTypeFunction*)function, (JSTypeCallsite*)callsite);
        }
        return;
    }

    analyze::Script *script = function->script->analysis;

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount; i++) {
        TypeSet *argTypes = callsite->argumentTypes[i];
        jsid id = script->getArgumentId(i);

        if (!JSID_IS_VOID(id)) {
            TypeSet *types = script->localTypes.getVariable(cx, id);
            argTypes->addSubset(cx, pool, types);
        } else {
            /*
             * This argument exceeds the number of formals. ignore the binding,
             * the value can only be accessed through the arguments object,
             * which is monitored.
             */
        }
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < script->argCount; i++) {
        jsid id = script->getArgumentId(i);
        TypeSet *types = script->localTypes.getVariable(cx, id);
        types->addType(cx, TYPE_UNDEFINED);
    }

    /* Add a binding for the receiver object of the call. */
    if (callsite->isNew) {
        /* The receiver object is the 'new' object for the function. */
        TypeObject *object = function->getNewObject(cx);
        script->thisTypes.addType(cx, (jstype) object);

        /*
         * If the script does not return a value then the pushed value is the new
         * object (typical case).
         */
        if (callsite->returnTypes) {
            callsite->returnTypes->addType(cx, (jstype) object);
            function->returnTypes.addFilterPrimitives(cx, function->pool(),
                                                      callsite->returnTypes, false);
        }
    } else {
        if (callsite->thisTypes) {
            /* Add a binding for the receiver object of the call. */
            callsite->thisTypes->addSubset(cx, pool, &script->thisTypes);
        } else {
            JS_ASSERT(callsite->thisType != TYPE_NULL);
            script->thisTypes.addType(cx, callsite->thisType);
        }

        /* Add a binding for the return value of the call. */
        if (callsite->returnTypes)
            function->returnTypes.addSubset(cx, function->pool(), callsite->returnTypes);
    }

    /* Analyze the function if we have not already done so. */
    if (!script->hasAnalyzed())
        script->analyze(cx);
}

void
TypeConstraintArith::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (other) {
        /*
         * Addition operation, consider these cases:
         *   int x {int,bool} -> int
         *   bool x bool -> int
         *   float x {int,bool,float} -> float
         *   string x any -> string
         */
        switch (type) {
          case TYPE_UNDEFINED:
          case TYPE_INT32:
          case TYPE_BOOLEAN:
            /* Note: treating int + void as causing an overflow. */
            if (other->typeFlags & (TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                    TYPE_FLAG_INT32 | TYPE_FLAG_BOOLEAN))
                target->addType(cx, TYPE_INT32);
            if (other->typeFlags & TYPE_FLAG_DOUBLE)
                target->addType(cx, TYPE_DOUBLE);
            break;
          case TYPE_DOUBLE:
            if (other->typeFlags & (TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                    TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE | TYPE_FLAG_BOOLEAN))
                target->addType(cx, TYPE_DOUBLE);
            break;
          case TYPE_STRING:
            target->addType(cx, TYPE_STRING);
            break;
          default:
            /*
             * Don't try to model arithmetic on objects, this can invoke valueOf,
             * operate on XML objects, etc.
             */
            cx->compartment->types.monitorBytecode(code);
            break;
        }
    } else {
        /* Monitor any arithmetic operation applied to a type other than int/float. */
        if (type == TYPE_INT32 || type == TYPE_DOUBLE)
            target->addType(cx, type);
        else
            cx->compartment->types.monitorBytecode(code);
    }
}

void
TypeConstraintTransformThis::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN || TypeIsObject(type)) {
        target->addType(cx, type);
        return;
    }

    /* TODO: handle strict mode code correctly. */
    TypeObject *object = NULL;
    switch (type) {
      case TYPE_NULL:
      case TYPE_UNDEFINED:
        object = cx->getGlobalTypeObject();
        break;
      case TYPE_INT32:
      case TYPE_DOUBLE:
        object = cx->getFixedTypeObject(TYPE_OBJECT_NEW_NUMBER);
        break;
      case TYPE_BOOLEAN:
        object = cx->getFixedTypeObject(TYPE_OBJECT_NEW_BOOLEAN);
        break;
      case TYPE_STRING:
        object = cx->getFixedTypeObject(TYPE_OBJECT_NEW_STRING);
        break;
      default:
        JS_NOT_REACHED("Bad type");
    }

    target->addType(cx, (jstype) object);
}

void
TypeConstraintFilterPrimitive::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (onlyNullVoid) {
        if (type == TYPE_NULL || type == TYPE_UNDEFINED)
            return;
    } else if (TypeIsPrimitive(type)) {
        return;
    }

    target->addType(cx, type);
}

void
TypeConstraintMonitorRead::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        cx->compartment->types.monitorBytecode(code);
        return;
    }

    if (type == (jstype) cx->getFixedTypeObject(TYPE_OBJECT_GETSET)) {
        cx->compartment->types.monitorBytecode(code);
        return;
    }

    target->addType(cx, type);
}

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

/*
 * Constraint which logs changes which occur after a script has been frozen,
 * at a bytecode granularity.
 */
class TypeConstraintFreeze : public TypeConstraint
{
public:
    analyze::Bytecode *code;

    TypeConstraintFreeze(analyze::Bytecode *_code)
        : TypeConstraint("freeze"), code(_code)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (type != TYPE_UNKNOWN && TypeIsObject(type)) {
            /*
             * Ignore new objects when either (a) this is a non-function object
             * and there are already other known objects, or (b) this is a function
             * object and there were already at least two objects of any kind
             * (could not treat as a direct call).  This can underapproximate the
             * number of recompilations actually required.
             */
            TypeObject *obj = (TypeObject*) type;
            if (obj->isFunction) {
                if (source->objectCount >= 3)
                    return;
            } else if (source->objectCount >= 2) {
                return;
            }
        }

        cx->compartment->types.recompileScript(code);
    }
};

void
TypeSet::addFreeze(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintFreeze>(pool, code), false);
}

/*
 * Constraint which logs changes to the types of a property of any object which
 * a type set can refer to.
 */
class TypeConstraintFreezeProp : public TypeConstraint
{
public:
    analyze::Bytecode *code;
    jsid id;

    TypeConstraintFreezeProp(analyze::Bytecode *_code, jsid _id)
        : TypeConstraint("freezeprop"), code(_code), id(_id)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (type == TYPE_UNKNOWN)
            return;

        TypeObject *object = GetPropertyObject(cx, type);
        if (!object)
            return;

        TypeSet *types = object->properties(cx).getVariable(cx, id);
        types->addFreeze(cx, object->pool(), code);
    }
};

void
TypeSet::addFreezeProp(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code, jsid id)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintFreezeProp>(pool, code, id), false);
}

/*
 * Constraint which logs changes to element accesses on an object, depending on
 * the type of the access.
 */
class TypeConstraintFreezeElem : public TypeConstraint
{
public:
    analyze::Bytecode *code;

    /* Fields about the element access, as for TypeConstraintElem. */
    TypeSet *object;

    TypeConstraintFreezeElem(analyze::Bytecode *code, TypeSet *object)
        : TypeConstraint("freezeelem"), code(code), object(object)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (type == TYPE_INT32) {
            object->addFreezeProp(cx, code->pool(), code, JSID_VOID);
        } else {
            /*
             * Accesses at this bytecode are monitored already, don't need
             * any freeze constraints.
             */
            JS_ASSERT(code->monitorNeeded);
        }
    }
};

void
TypeSet::addFreezeElem(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code, TypeSet *object)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintFreezeElem>(pool, code, object), false);
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

/*
 * These names need to be consistent with those of the function, prototype and new
 * objects produced by js_InitClass where objects have propagation other than from
 * Array.prototype/Object.prototype.  Since names are unique identifiers for type
 * objects within a compartment, this ensures that the propagation performed by
 * js_InitClass affects the objects later accessed via getFixedTypeObject.  This
 * design is pretty goofy and fragile but keeps js_InitClass simple.
 */
const char * const fixedTypeObjectNames[] = {
    "Object",
    "Function",
    "Array",
    "Function:prototype",
    "Object:prototype",
    "Array:prototype",
    "Boolean:new",
    "Date:new",
    "Error:new",
    "Iterator:new",
    "Number:new",
    "String:new",
    "Proxy:new",
    "RegExp:new",
    "ArrayBuffer:new",
    "#Magic",
    "#GetSet",
    "XML:new",
    "QName:new",
    "Namespace:new",
    "#Arguments",
    "#NoSuchMethod",
    "#NoSuchMethodArguments",
    "#PropertyDescriptor",
    "#KeyValuePair",
    "#RegExpMatchArray",
    "#StringSplitArray",
    "#UnknownArray",
    "#CloneArray",
    "#PropertyArray",
    "#XMLNamespaceArray",
    "#JSONArray",
    "#ReflectArray",
    "#UnknownObject",
    "#CloneObject",
    "#JSONStringify",
    "#JSONRevive",
    "#JSONObject",
    "#ReflectObject",
    "#XMLSettings",
    "#RegExpStatics",
    "#Call",
    "#DeclEnv",
    "#SharpArray",
    "#With",
    "#Block",
    "#NullClosure",
    "#PropertyIterator",
    "#Script"
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(fixedTypeObjectNames) == TYPE_OBJECT_FIXED_LIMIT);

void
TypeCompartment::init()
{
    PodZero(this);

    JS_InitArenaPool(&pool, "typeinfer", 512, 8, NULL);

    /*
    char buf[50];
    char prefix[10];
    prefix[0] = 0;
    strcpy(prefix, ".XXXXXX");
    mktemp(prefix);
    JS_snprintf(buf, sizeof(buf), "infer%s.txt", prefix);
    out = fopen(buf, "w");
    */

    static FILE *file = NULL;
    if (!file)
        file = fopen("infer.txt", "w");
    out = file;

    objectNameTable = new ObjectNameTable();
    bool success = objectNameTable->init();
    JS_ASSERT(success);
}

TypeCompartment::~TypeCompartment()
{
    /* fclose(out); */
    JS_FinishArenaPool(&pool);

    delete objectNameTable;
}

void 
TypeCompartment::growPendingArray()
{
    pendingCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *oldArray = pendingArray;
    pendingArray = ArenaArray<PendingWork>(pool, pendingCapacity);
    memcpy(pendingArray, oldArray, pendingCount * sizeof(PendingWork));
}

TypeObject *
TypeCompartment::makeFixedTypeObject(JSContext *cx, FixedTypeObjectName which)
{
    JS_ASSERT(which < TYPE_OBJECT_FIXED_LIMIT);
    JS_ASSERT(!fixedTypeObjects[which]);

    TypeObject *type = cx->getTypeObject(fixedTypeObjectNames[which], which <= TYPE_OBJECT_FUNCTION_LAST);
    fixedTypeObjects[which] = type;

    if (which <= TYPE_OBJECT_BASE_LAST) {
        /* Nothing */
    } else if (which <= TYPE_OBJECT_MONITOR_LAST) {
        type->setMonitored(cx);
    } else if (which <= TYPE_OBJECT_ARRAY_LAST) {
        cx->addTypePrototype(type, cx->getFixedTypeObject(TYPE_OBJECT_ARRAY_PROTOTYPE));
    } else {
        cx->addTypePrototype(type, cx->getFixedTypeObject(TYPE_OBJECT_OBJECT_PROTOTYPE));
    }

    return type;
}

TypeObject *
TypeCompartment::getTypeObject(JSContext *cx, analyze::Script *script, const char *name, bool isFunction)
{
#ifdef JS_TYPE_INFERENCE
    jsid id = ATOM_TO_JSID(js_Atomize(cx, name, strlen(name), 0));

    JSArenaPool &pool = script ? script->pool : this->pool;

    /*
     * Check for an existing object with the same name first.  If we have one
     * then reuse it.
     */
    ObjectNameTable::AddPtr p = objectNameTable->lookupForAdd(id);
    if (p) {
        JS_ASSERT(p->value->isFunction == isFunction);
        JS_ASSERT(&p->value->pool() == &pool);
        return p->value;
    }

    js::types::TypeObject *object;
    if (isFunction)
        object = ArenaNew<TypeFunction>(pool, cx, &pool, id);
    else
        object = ArenaNew<TypeObject>(pool, cx, &pool, id);

    TypeObject *&objects = script ? script->objects : this->objects;
    object->next = objects;
    objects = object;

    objectNameTable->add(p, id, object);
    return object;
#else
    return NULL;
#endif
}

void
TypeCompartment::addDynamicType(JSContext *cx, TypeSet *types, jstype type,
                                const char *format, ...)
{
    JS_ASSERT(!types->hasType(type));

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    PrintType(cx, type);

    interpreting = false;

    uint64_t startTime = currentTime();

    types->addType(cx, type);

    uint64_t endTime = currentTime();
    analysisTime += (endTime - startTime);

    interpreting = true;
}

void
TypeCompartment::print(JSContext *cx, JSCompartment *compartment)
{
    JS_ASSERT(this == &compartment->types);

#ifdef DEBUG
    for (JSScript *script = (JSScript *)compartment->scripts.next;
         &script->links != &compartment->scripts;
         script = (JSScript *)script->links.next) {
        if (script->analysis)
            script->analysis->print(cx);
    }

    TypeObject *object = objects;
    while (object) {
        object->print(cx, out);
        object = object->next;
    }
#endif // DEBUG

    if (ignoreWarnings)
        warnings = false;

    double millis = analysisTime / 1000.0;

    fprintf(out, "\nWarnings: %s\n", warnings ? "yes" : "no");

    fprintf(out, "Counts: ");
    for (unsigned count = 0; count < TYPE_COUNT_LIMIT; count++) {
        if (count)
            fputs("/", out);
        fprintf(out, "%u", typeCounts[count]);
    }
    fprintf(out, " (%u over)\n", typeCountOver);

    if (recompilations)
        fprintf(out, "Recompilations: %u\n", recompilations);
    fprintf(out, "Time: %.2f ms\n", millis);

    // for debugging regressions.
    if (warnings) {
        fclose(out);
        printf("Type warnings generated, bailing out...\n");
        fflush(stdout);
        *((int*)NULL) = 0;  /* Type warnings */
    }
}

/////////////////////////////////////////////////////////////////////
// TypeStack
/////////////////////////////////////////////////////////////////////

void
TypeStack::merge(JSContext *cx, TypeStack *one, TypeStack *two)
{
    JS_ASSERT((one == NULL) == (two == NULL));

    if (!one)
        return;

    one = one->group();
    two = two->group();

#ifdef JS_TYPES_DEBUG_SPEW
    JS_ASSERT(one->types.id && two->types.id);
#endif

    /* Check if the classes are already the same. */
    if (one == two)
        return;

    /* There should not be any types or constraints added to the stack types. */
    JS_ASSERT(one->types.typeFlags == 0 && one->types.constraintList == NULL);
    JS_ASSERT(two->types.typeFlags == 0 && two->types.constraintList == NULL);

    /* The nodes in a class must have consistent depth. */
    JS_ASSERT(one->stackDepth == two->stackDepth);

    /* Merge any inner portions of the stack for the two nodes. */
    if (one->innerStack)
        merge(cx, one->innerStack, two->innerStack);

    /* Check that additional information is consistent between the nodes. */
    JS_ASSERT(one->boundWith == two->boundWith);
    JS_ASSERT(one->isForEach == two->isForEach);
    JS_ASSERT(one->letVariable == two->letVariable);
    JS_ASSERT(one->scopeVars == two->scopeVars);
    JS_ASSERT(one->scopeScript == two->scopeScript);

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(cx->typeOut(), "merge: T%u T%u\n", one->types.id, two->types.id);
#endif

    /* one has now been merged into two, do the actual join. */
    PodZero(one);
    one->mergedGroup = two;
    two->hasMerged = true;
}

/////////////////////////////////////////////////////////////////////
// VariableSet
/////////////////////////////////////////////////////////////////////

bool
VariableSet::addPropagate(JSContext *cx, VariableSet *target,
                          bool excludePrototype)
{
    if (!HashSetInsert(cx, propagateSet, propagateCount, target))
        return false;

    /* Push all existing variables into the target, except (optionally) 'prototype'. */
    Variable *var = variables;
    while (var) {
        bool skip = (excludePrototype && var->id == id_prototype(cx));

        /*
         * Also exclude certain properties propagated into the Function prototype
         * from the Object prototype.
         */
        TypeObject *funcProto = cx->getFixedTypeObject(TYPE_OBJECT_FUNCTION_PROTOTYPE);
        if (target == &funcProto->properties(cx)) {
            if (var->id == id_toString(cx) || var->id == id_toSource(cx))
                skip = true;
        }

        if (!skip) {
            TypeSet *targetTypes = target->getVariable(cx, var->id);
            var->types.addSubset(cx, *pool, targetTypes);
        }

        var = var->next;
    }

    return true;
}

void
VariableSet::print(JSContext *cx, FILE *out)
{
    if (variables == NULL) {
        fputs(" {}\n", out);
        return;
    }

    fprintf(out, " {");

    Variable *var = variables;
    while (var) {
        fprintf(out, "\n    %s:", cx->getTypeId(var->id));
        var->types.print(cx, out);
        var = var->next;
    }

    fprintf(out, "\n}\n");
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

TypeObject::TypeObject(JSContext *cx, JSArenaPool *pool, jsid name)
    : name(name), isFunction(false), monitored(false),
      propertySet(pool), propertiesFilled(false), next(NULL),
      hasObjectPropagation(false), hasArrayPropagation(false), isInitObject(false)
{
#ifdef JS_TYPES_DEBUG_SPEW
    propertySet.name = name;
    fprintf(cx->typeOut(), "newObject: %s\n", cx->getTypeId(name));
#endif
}

bool
TypeObject::addPropagate(JSContext *cx, TypeObject *target,
                         bool excludePrototype)
{
    bool added = properties(cx).addPropagate(cx, &target->properties(cx), excludePrototype);

    /* Remember the basic properties we have pushed into a given object. */
    if (hasObjectPropagation)
        target->hasObjectPropagation = true;
    if (hasArrayPropagation)
        target->hasArrayPropagation = true;

    return added;
}

void
TypeFunction::fillProperties(JSContext *cx)
{
    /*
     * The function has an arguments property accessing the arguments of the most
     * recent call stack invocation, or null.
     */
    TypeSet *argumentTypes = propertySet.getVariable(cx, id_arguments(cx));
    argumentTypes->addType(cx, TYPE_NULL);
    argumentTypes->addType(cx, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_ARGUMENTS));
    if (script) {
        /* This property also receives any changes to the local variable 'arguments' */
        TypeSet *types = script->analysis->localTypes.getVariable(cx, id_arguments(cx));
        types->addSubset(cx, script->analysis->pool, argumentTypes);
    }

    /* Propagation will be performed separately for builtin class functions. */
    if (isBuiltin)
        return;

    JS_ASSERT(!prototypeObject);

    /*
     * The function itself inherits properties from Function.prototype, and
     * transitively from Object.prototype.
     */
    TypeObject *funcProto = cx->getFixedTypeObject(TYPE_OBJECT_FUNCTION_PROTOTYPE);
    funcProto->addPropagate(cx, this);

    const char *baseName = cx->getTypeId(name);
    unsigned len = strlen(baseName) + 15;
    char *prototypeName = (char *)alloca(len);
    JS_snprintf(prototypeName, len, "%s:prototype", baseName);
    prototypeObject = cx->getTypeObject(prototypeName, false);

    TypeSet *prototypeTypes = propertySet.getVariable(cx, id_prototype(cx));
    prototypeTypes->addType(cx, (jstype) prototypeObject);

    /* The prototype inherits properties from Object.prototype. */
    TypeObject *objectProto = cx->getFixedTypeObject(TYPE_OBJECT_OBJECT_PROTOTYPE);
    objectProto->addPropagate(cx, prototypeObject);
}

void
TypeObject::setMonitored(JSContext *cx)
{
    if (monitored)
        return;

    /*
     * Existing property constraints may have already been added to this object,
     * which we need to do the right thing for.  We can't ensure that we will
     * mark all monitored objects before they have been accessed, as the __proto__
     * of a non-monitored object could be dynamically set to a monitored object.
     * Adding unknown for any properties accessed already accounts for possible
     * values read from them.
     */

    Variable *var = properties(cx).variables;
    while (var) {
        var->types.addType(cx, TYPE_UNKNOWN);
        var = var->next;
    }

    monitored = true;
}

void
TypeObject::print(JSContext *cx, FILE *out)
{
    fputs(cx->getTypeId(name), out);

    if (isFunction && !propertiesFilled)
        fputs("\n", out);
    else
        properties(cx).print(cx, out);
    fputs("\n", out);
}

/////////////////////////////////////////////////////////////////////
// TypeFunction
/////////////////////////////////////////////////////////////////////

TypeFunction::TypeFunction(JSContext *cx, JSArenaPool *pool, jsid name)
    : TypeObject(cx, pool, name), handler(NULL), script(NULL),
      prototypeObject(NULL), newObject(NULL), returnTypes(pool),
      isBuiltin(false), isGeneric(false)
{
    isFunction = true;

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(cx->typeOut(), "newFunction: %s return T%u\n",
            cx->getTypeId(name), returnTypes.id);
#endif
}

/////////////////////////////////////////////////////////////////////
// TypeScript
/////////////////////////////////////////////////////////////////////

} } /* namespace js::types */

/*
 * Returns true if we don't expect to compute the correct types for some value
 * popped by the specified bytecode.
 */
static inline bool
IgnorePopped(JSOp op, unsigned index)
{
    switch (op) {
      case JSOP_LEAVEBLOCK:
      case JSOP_LEAVEBLOCKEXPR:
         /*
          * We don't model 'let' variables as stack operands, the values popped
          * when the 'let' finishes will be missing.
          */
        return true;

      case JSOP_FORNAME:
      case JSOP_FORLOCAL:
      case JSOP_FORGLOBAL:
      case JSOP_FORARG:
      case JSOP_FORPROP:
      case JSOP_FORELEM:
      case JSOP_MOREITER:
      case JSOP_ENDITER:
        /* We don't keep track of the iteration state for 'for in' or 'for each in' loops. */
        return true;
      case JSOP_ENUMELEM:
        return (index == 1 || index == 2);

      /* We keep track of the scopes pushed by BINDNAME separately. */
      case JSOP_SETNAME:
      case JSOP_SETGNAME:
        return (index == 1);
      case JSOP_GETXPROP:
      case JSOP_DUP:
        return true;
      case JSOP_SETXMLNAME:
        return (index == 1 || index == 2);

      case JSOP_ENDFILTER:
        /* We don't keep track of XML filter state. */
        return (index == 0);

      case JSOP_LEAVEWITH:
        /* We don't keep track of objects bound by a 'with'. */
        return true;

      case JSOP_RETSUB:
      case JSOP_POPN:
        /* We don't keep track of state indicating whether there is a pending exception. */
        return true;

      default:
        return false;
    }
}

void
JSScript::typeCheckBytecode(JSContext *cx, const jsbytecode *pc, const js::Value *sp)
{
    js::analyze::Bytecode &code = analysis->getCode(pc);
    JS_ASSERT(code.analyzed);

    int useCount = js::analyze::GetUseCount(this, code.offset);
    JSOp op = (JSOp) *pc;

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(cx->typeOut(), "Execute: #%u:%05u:  ", analysis->id, code.offset);
    code.print(cx, cx->typeOut());

    for (int i = 0; i < useCount; i++) {
        const js::Value &val = sp[-1 - i];
        js::types::jstype type = js::types::GetValueType(cx, val);

        fprintf(cx->typeOut(), " %u:", i);
        js::types::PrintType(cx, type, false);
    }

    fputs("\n", cx->typeOut());
#endif

    if (!code.script->compiled)
        analysis->freezeAllTypes(cx);

    if (code.script->recompileNeeded) {
        fprintf(cx->typeOut(), "Recompile: #%u\n", analysis->id);
        code.script->recompileNeeded = false;
        cx->compartment->types.recompilations++;
    }

    if (!useCount || code.missingTypes)
        return;

    js::types::TypeStack *stack = code.inStack->group();
    for (int i = 0; i < useCount; i++) {
        const js::Value &val = sp[-1 - i];
        js::types::jstype type = js::types::GetValueType(cx, val);

        bool matches = IgnorePopped(op, i) || stack->types.hasType(type);

        stack = stack->innerStack ? stack->innerStack->group() : NULL;

        if (!matches) {
            fprintf(cx->typeOut(), "warning: Missing type at #%u:%05u popped %u: ",
                    analysis->id, code.offset, i);
            js::types::PrintType(cx, type);

            cx->compartment->types.warnings = true;
            code.missingTypes = true;
            return;
        }
    }
}

namespace js {
namespace analyze {

using namespace types;

inline Bytecode*
Script::parentCode()
{
    return parent ? &parent->analysis->getCode(parentpc) : NULL;
}

inline Script*
Script::evalParent()
{
    Script *script = this;
    while (script->parent && !script->function)
        script = script->parent->analysis;
    return script;
}

void
Script::setFunction(JSContext *cx, JSFunction *fun)
{
    JS_ASSERT(!function);
    function = fun->getTypeObject()->asFunction();

    /* Add the return type for the empty script, which we do not explicitly analyze. */
    if (script->isEmpty())
        function->returnTypes.addType(cx, TYPE_UNDEFINED);

    /*
     * Construct the arguments and locals of this function, and mark them as
     * definitely declared for scope lookups.  Note that we don't do this for the
     * global script (don't need to, everything not in another scope is global),
     * nor for eval scripts --- if an eval declares a variable the declaration
     * will be merged with any declaration in the context the eval occurred in,
     * and definitions information will be cleared for any scripts that could use
     * the declared variable.
     */
    if (fun->hasLocalNames()) {
        localNames = fun->getLocalNameArray(cx, &pool);
        argCount = fun->nargs;
    }

    /* Remember any name the function has, this is a local variable of the script. */
    if (fun->atom)
        thisName = ATOM_TO_JSID(fun->atom);
}

static inline ptrdiff_t
GetJumpOffset(jsbytecode *pc, jsbytecode *pc2)
{
    uint32 type = JOF_OPTYPE(*pc);
    if (JOF_TYPE_IS_EXTENDED_JUMP(type))
        return GET_JUMPX_OFFSET(pc2);
    return GET_JUMP_OFFSET(pc2);
}

/* Return whether op bytecodes do not fallthrough (they may do a jump). */
static inline bool
BytecodeNoFallThrough(JSOp op)
{
    switch (op) {
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_DEFAULT:
      case JSOP_DEFAULTX:
      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
      case JSOP_THROW:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_FILTER:
        return true;
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
        /* These fall through indirectly, after executing a 'finally'. */
        return false;
      default:
        return false;
    }
}

static inline VariableSet *
GetGlobalProperties(JSContext *cx)
{
    return &cx->getGlobalTypeObject()->properties(cx);
}

/*
 * Get the variable set which declares id, either the local variables of a script
 * or the properties of the global object.  NULL if the scope is ambiguous due
 * to a 'with'.  Even if the scope is ambiguous, if the variable can refer to
 * a script's local or 'let' variable then scopeScript is set to that script.
 */
static VariableSet *
SearchScope(JSContext *cx, Script *script, TypeStack *stack, jsid id,
            Script **scopeScript = NULL)
{
    bool foundWith = false;

    /* Search up until we find a local variable with the specified name. */
    while (script->parent) {
        /* Search the stack for any 'with' objects or 'let' variables. */
        while (stack) {
            stack = stack->group();
            if (stack->boundWith) {
                /* Enclosed within a 'with', ambiguous scope. */
                foundWith = true;
            }
            if (stack->letVariable == id) {
                /* The variable is definitely bound by this scope. */
                goto found;
            }
            stack = stack->innerStack;
        }

        if (script->isEval()) {
            /* eval scripts have no local variables to consider (they may have 'let' vars). */
            JS_ASSERT(!script->localTypes.variables);
            stack = script->parentCode()->inStack;
            script = script->parent->analysis;
            continue;
        }

        /* Function scripts have 'arguments' local variables. */
        if (id == id_arguments(cx) && script->function) {
            TypeSet *types = script->localTypes.getVariable(cx, id);
            types->addType(cx, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_ARGUMENTS));
            goto found;
        }

        /* Function scripts with names have local variables of that name. */
        if (id == script->thisName) {
            JS_ASSERT(script->function);
            TypeSet *types = script->localTypes.getVariable(cx, id);
            types->addType(cx, (jstype) script->function);
            goto found;
        }

        unsigned nargs = script->argCount;
        for (unsigned i = 0; i < nargs; i++) {
            if (id == script->getArgumentId(i))
                goto found;
        }

        unsigned nfixed = script->getScript()->nfixed;
        for (unsigned i = 0; i < nfixed; i++) {
            if (id == script->getLocalId(i, NULL))
                goto found;
        }

        stack = script->parentCode()->inStack;
        script = script->parent->analysis;
    }

  found:
    JS_ASSERT(script);
    script = script->evalParent();

    if (script->function) {
        if (scopeScript)
            *scopeScript = script;
        return foundWith ? NULL : &script->localTypes;
    }
    if (scopeScript)
        *scopeScript = NULL;
    return foundWith ? NULL : GetGlobalProperties(cx);
}

static inline jsid
GetAtomId(JSContext *cx, Script *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script->getScript(), (jsbytecode*) pc, offset);
    return MakeTypeId(ATOM_TO_JSID(script->getScript()->getAtom(index)));
}

static inline jsid
GetGlobalId(JSContext *cx, Script *script, const jsbytecode *pc)
{
    unsigned index = GET_SLOTNO(pc);
    return MakeTypeId(ATOM_TO_JSID(script->getScript()->getGlobalAtom(index)));
}

static inline JSObject *
GetScriptObject(JSContext *cx, Script *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script->getScript(), (jsbytecode*) pc, offset);
    return script->getScript()->getObject(index);
}

/* Get the script and id of a variable referred to by an UPVAR opcode. */
static inline Script *
GetUpvarVariable(JSContext *cx, Bytecode *code, unsigned index, jsid *id)
{
    JSUpvarArray *uva = code->script->getScript()->upvars();

    JS_ASSERT(index < uva->length);
    js::UpvarCookie cookie = uva->vector[index];
    uint16 level = code->script->getScript()->staticLevel - cookie.level();
    uint16 slot = cookie.slot();

    /* Find the script with the static level we're searching for. */
    Bytecode *newCode = code;
    while (newCode->script->getScript()->staticLevel != level)
        newCode = newCode->script->parentCode();

    Script *newScript = newCode->script;

    /*
     * Get the name of the variable being referenced.  It is either an argument,
     * a local or the function itself.
     */
    if (!newScript->function)
        *id = newScript->getLocalId(newScript->getScript()->nfixed + slot, newCode->inStack);
    else if (slot < newScript->argCount)
        *id = newScript->getArgumentId(slot);
    else if (slot == UpvarCookie::CALLEE_SLOT)
        *id = newScript->thisName;
    else
        *id = newScript->getLocalId(slot - newScript->argCount, newCode->inStack);

    JS_ASSERT(!JSID_IS_VOID(*id));
    return newScript->evalParent();
}

/* Constraint which preserves primitives and converts objects to strings. */
class TypeConstraintToString : public TypeConstraint
{
  public:
    TypeSet *target;

    TypeConstraintToString(TypeSet *_target)
        : TypeConstraint("tostring"), target(_target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (TypeIsObject(type))
            target->addType(cx, TYPE_STRING);
        else
            target->addType(cx, type);
    }
};

/*
 * If the bytecode immediately following code/pc is a test of the value
 * pushed by code, mark that value as possibly void.
 */
static inline void
CheckNextTest(JSContext *cx, Bytecode *code, jsbytecode *pc)
{
    jsbytecode *next = pc + GetBytecodeLength(pc);
    switch ((JSOp)*next) {
      case JSOP_IFEQ:
      case JSOP_IFNE:
      case JSOP_NOT:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        code->pushed(0)->addType(cx, TYPE_UNDEFINED);
        break;
      default:
        break;
    }
}

/* Propagate the specified types into the Nth value pushed by an instruction. */
static inline void
MergePushed(JSContext *cx, JSArenaPool &pool, Bytecode *code, unsigned num, TypeSet *types)
{
    types->addSubset(cx, pool, code->pushed(num));
}

/*
 * Update types with the possible values bound by the for loop in code.
 * TODO: generators not handled yet.
 */
void
SetForTypes(JSContext *cx, Bytecode *code, TypeSet *types)
{
    if (code->pushedArray[0].group()->isForEach)
        types->addType(cx, TYPE_UNKNOWN);
    else
        types->addType(cx, TYPE_STRING);
}

void
Script::analyzeTypes(JSContext *cx, Bytecode *code)
{
    unsigned offset = code->offset;

    JS_ASSERT(code->analyzed);
    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(cx->typeOut(), "analyze: #%u:%05u\n", id, offset);
#endif

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_NOP:
      case JSOP_TRACE:
      case JSOP_POP:
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_IFEQ:
      case JSOP_IFEQX:
      case JSOP_IFNE:
      case JSOP_IFNEX:
      case JSOP_ENDINIT:
      case JSOP_LINENO:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_DEFCONST:
      case JSOP_LEAVEWITH:
      case JSOP_LEAVEBLOCK:
      case JSOP_RETRVAL:
      case JSOP_ENDITER:
      case JSOP_TRY:
      case JSOP_THROWING:
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
      case JSOP_RETSUB:
      case JSOP_CONDSWITCH:
      case JSOP_DEFAULT:
      case JSOP_POPN:
      case JSOP_UNBRANDTHIS:
      case JSOP_STARTXML:
      case JSOP_STARTXMLEXPR:
      case JSOP_DEFXMLNS:
      case JSOP_SHARPINIT:
      case JSOP_INDEXBASE:
      case JSOP_INDEXBASE1:
      case JSOP_INDEXBASE2:
      case JSOP_INDEXBASE3:
      case JSOP_RESETBASE:
      case JSOP_RESETBASE0:
      case JSOP_BLOCKCHAIN:
      case JSOP_NULLBLOCKCHAIN:
      case JSOP_POPV:
        break;

        /* Bytecodes pushing values of known type. */
      case JSOP_VOID:
      case JSOP_PUSH:
        code->setFixed(cx, 0, TYPE_UNDEFINED);
        break;
      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_INT8:
      case JSOP_INT32:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_BITAND:
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITNOT:
      case JSOP_RSH:
      case JSOP_LSH:
        code->setFixed(cx, 0, TYPE_INT32);
        break;
      case JSOP_FALSE:
      case JSOP_TRUE:
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_NOT:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
      case JSOP_IN:
      case JSOP_INSTANCEOF:
      case JSOP_DELDESC:
        code->setFixed(cx, 0, TYPE_BOOLEAN);
        break;
      case JSOP_DOUBLE:
      case JSOP_DIV:
        code->setFixed(cx, 0, TYPE_DOUBLE);
        break;
      case JSOP_STRING:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
      case JSOP_QNAMEPART:
      case JSOP_XMLTAGEXPR:
      case JSOP_TOATTRVAL:
      case JSOP_ADDATTRNAME:
      case JSOP_ADDATTRVAL:
      case JSOP_XMLELTEXPR:
        code->setFixed(cx, 0, TYPE_STRING);
        break;
      case JSOP_NULL:
        code->setFixed(cx, 0, TYPE_NULL);
        break;
      case JSOP_HOLE:
        code->setFixed(cx, 0, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_MAGIC));
        break;
      case JSOP_REGEXP:
        code->setFixed(cx, 0, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_NEW_REGEXP));
        break;

      case JSOP_STOP:
        /* If a stop is reachable then the return type may be void. */
        if (function)
            function->returnTypes.addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        /* OR/AND push whichever operand determined the result. */
        code->popped(0)->addSubset(cx, pool, code->pushed(0));
        break;

      case JSOP_DUP: {
        MergePushed(cx, pool, code, 0, code->popped(0));
        MergePushed(cx, pool, code, 1, code->popped(0));

        /* Propagate any scope information on the stack value.  blech. */
        TypeStack *stack = code->inStack->group();
        if (stack->scopeVars || stack->scopeScript) {
            code->pushedArray[0].group()->scopeVars = stack->scopeVars;
            code->pushedArray[1].group()->scopeVars = stack->scopeVars;
            code->pushedArray[0].group()->scopeScript = stack->scopeScript;
            code->pushedArray[1].group()->scopeScript = stack->scopeScript;
        }
        break;
      }

      case JSOP_DUP2:
        MergePushed(cx, pool, code, 0, code->popped(1));
        MergePushed(cx, pool, code, 1, code->popped(0));
        MergePushed(cx, pool, code, 2, code->popped(1));
        MergePushed(cx, pool, code, 3, code->popped(0));
        break;

      case JSOP_GETGLOBAL:
      case JSOP_CALLGLOBAL:
      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
      case JSOP_NAME:
      case JSOP_CALLNAME: {
        /* Get the type set being updated, if we can determine it. */
        jsid id;
        VariableSet *vars;

        switch (op) {
          case JSOP_GETGLOBAL:
          case JSOP_CALLGLOBAL:
            id = GetGlobalId(cx, this, pc);
            vars = GetGlobalProperties(cx);
            break;
          case JSOP_GETGNAME:
          case JSOP_CALLGNAME:
            id = GetAtomId(cx, this, pc, 0);
            vars = GetGlobalProperties(cx);
            break;
          default:
            id = GetAtomId(cx, this, pc, 0);
            vars = SearchScope(cx, this, code->inStack, id);
            break;
        }

        if (vars == GetGlobalProperties(cx)) {
            /*
             * This might be a lazily loaded property of the global object.
             * Resolve it now.
             */
            JSObject *obj;
            JSProperty *prop;
            js_LookupPropertyWithFlags(cx, cx->globalObject, id,
                                       JSRESOLVE_QUALIFIED, &obj, &prop);
        }

        if (vars && id != id___proto__(cx) && id != id_prototype(cx)) {
            TypeSet *types = vars->getVariable(cx, id);
            types->addMonitorRead(cx, *vars->pool, code, code->pushed(0));
            if (op == JSOP_CALLGLOBAL || op == JSOP_CALLGNAME || op == JSOP_CALLNAME)
                code->setFixed(cx, 1, TYPE_UNDEFINED);
        } else {
            /* Ambiguous access, we need to monitor this bytecode. */
            cx->compartment->types.monitorBytecode(code);
        }

        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_BINDGNAME: {
        jsid id = GetAtomId(cx, this, pc, 0);
        TypeStack *stack = code->pushedArray[0].group();
        stack->scopeVars = GetGlobalProperties(cx);
        break;
      }

      case JSOP_BINDNAME: {
        jsid id = GetAtomId(cx, this, pc, 0);
        TypeStack *stack = code->pushedArray[0].group();
        stack->scopeVars = SearchScope(cx, this, code->inStack, id, &stack->scopeScript);
        break;
      }

      case JSOP_SETGNAME:
      case JSOP_SETNAME: {
        jsid id = GetAtomId(cx, this, pc, 0);

        TypeStack *stack = code->inStack->group()->innerStack->group();
        if (stack->scopeVars && id != id___proto__(cx)) {
            TypeSet *types = stack->scopeVars->getVariable(cx, id);
            code->popped(0)->addSubset(cx, pool, types);
        } else {
            if (stack->scopeScript) {
                /*
                 * Even if the scope is ambiguous, we need to add constraints for
                 * any identified script which could be referred to by the setname.
                 * during monitoring we can't capture assignments to function locals
                 * or let variables.
                 */
                TypeSet *types = stack->scopeScript->localTypes.getVariable(cx, id);
                code->popped(0)->addSubset(cx, pool, types);
            }
            cx->compartment->types.monitorBytecode(code);
        }

        MergePushed(cx, pool, code, 0, code->popped(0));
        break;
      }

      case JSOP_GETXPROP: {
        jsid id = GetAtomId(cx, this, pc, 0);

        VariableSet *vars = code->inStack->group()->scopeVars;
        if (vars) {
            TypeSet *types = vars->getVariable(cx, id);
            types->addSubset(cx, pool, code->pushed(0));
        } else {
            cx->compartment->types.monitorBytecode(code);
        }

        break;
      }

      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC: {
        jsid id = GetAtomId(cx, this, pc, 0);

        VariableSet *vars = SearchScope(cx, this, code->inStack, id);
        if (vars) {
            TypeSet *types = vars->getVariable(cx, id);
            types->addSubset(cx, *vars->pool, code->pushed(0));
            types->addArith(cx, *vars->pool, code, types);
        } else {
            cx->compartment->types.monitorBytecode(code);
        }

        break;
      }

      case JSOP_SETGLOBAL:
      case JSOP_SETCONST: {
        jsid id = (op == JSOP_SETGLOBAL) ? GetGlobalId(cx, this, pc) : GetAtomId(cx, this, pc, 0);
        TypeSet *types = GetGlobalProperties(cx)->getVariable(cx, id);

        code->popped(0)->addSubset(cx, pool, types);
        MergePushed(cx, pool, code, 0, code->popped(0));
        break;
      }

      case JSOP_INCGLOBAL:
      case JSOP_DECGLOBAL:
      case JSOP_GLOBALINC:
      case JSOP_GLOBALDEC: {
        jsid id = GetGlobalId(cx, this, pc);
        TypeSet *types = GetGlobalProperties(cx)->getVariable(cx, id);

        types->addArith(cx, cx->compartment->types.pool, code, types);
        MergePushed(cx, cx->compartment->types.pool, code, 0, types);
        break;
      }

      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC: {
        jsid id = GetAtomId(cx, this, pc, 0);
        TypeSet *types = GetGlobalProperties(cx)->getVariable(cx, id);

        types->addArith(cx, cx->compartment->types.pool, code, types);
        MergePushed(cx, cx->compartment->types.pool, code, 0, types);
        break;
      }

      case JSOP_GETUPVAR:
      case JSOP_CALLUPVAR:
      case JSOP_GETFCSLOT:
      case JSOP_CALLFCSLOT: {
        unsigned index = GET_UINT16(pc);

        jsid id = JSID_VOID;
        Script *newScript = GetUpvarVariable(cx, code, index, &id);
        TypeSet *types = newScript->localTypes.getVariable(cx, id);

        MergePushed(cx, newScript->pool, code, 0, types);
        if (op == JSOP_CALLUPVAR || op == JSOP_CALLFCSLOT)
            code->setFixed(cx, 1, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETARG:
      case JSOP_SETARG:
      case JSOP_CALLARG: {
        jsid id = getArgumentId(GET_ARGNO(pc));

        if (!JSID_IS_VOID(id)) {
            TypeSet *types = localTypes.getVariable(cx, id);

            MergePushed(cx, pool, code, 0, types);
            if (op == JSOP_SETARG)
                code->popped(0)->addSubset(cx, pool, types);
            if (op == JSOP_CALLARG)
                code->setFixed(cx, 1, TYPE_UNDEFINED);
        } else {
            cx->compartment->types.monitorBytecode(code);
        }

        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        TypeSet *types = localTypes.getVariable(cx, id);

        types->addArith(cx, pool, code, types);
        MergePushed(cx, pool, code, 0, types);
        break;
      }

      case JSOP_ARGSUB:
        // TODO: is this the right way to handle arguments[...] accesses?
        cx->compartment->types.monitorBytecode(code);
        break;

      case JSOP_GETLOCAL:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP:
      case JSOP_CALLLOCAL: {
        uint32 local = GET_SLOTNO(pc);
        jsid id = getLocalId(local, code->inStack);

        TypeSet *types;
        JSArenaPool *pool;
        if (!JSID_IS_VOID(id)) {
            types = evalParent()->localTypes.getVariable(cx, id);
            pool = &evalParent()->pool;
        } else {
            types = getStackTypes(GET_SLOTNO(pc), code->inStack);
            pool = &this->pool;
        }

        if (op != JSOP_SETLOCALPOP) {
            MergePushed(cx, *pool, code, 0, types);
            if (op == JSOP_CALLLOCAL)
                code->setFixed(cx, 1, TYPE_UNDEFINED);
        }

        if (op == JSOP_SETLOCAL || op == JSOP_SETLOCALPOP) {
            code->popped(0)->addSubset(cx, this->pool, types);
        } else {
            /*
             * Add void type if the variable might be undefined.  TODO: monitor for
             * undefined read instead?  localDefined returns false for
             * variables which could have a legitimate use-before-def, for let
             * variables and variables exceeding the LOCAL_LIMIT threshold.
             */
            if (localHasUseBeforeDef(local) || !localDefined(local, pc))
                code->pushed(0)->addType(cx, TYPE_UNDEFINED);
        }

        break;
      }

      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        jsid id = getLocalId(GET_SLOTNO(pc), code->inStack);
        TypeSet *types = evalParent()->localTypes.getVariable(cx, id);

        types->addArith(cx, evalParent()->pool, code, types);
        MergePushed(cx, evalParent()->pool, code, 0, types);
        break;
      }

      case JSOP_ARGUMENTS: {
        /*
         * This can show up either when the arguments array is being accessed
         * or when there is a local variable named arguments.
         */
        JS_ASSERT(function);

        TypeSet *types = localTypes.getVariable(cx, id_arguments(cx));
        types->addType(cx, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_ARGUMENTS));
        MergePushed(cx, pool, code, 0, types);
        break;
      }

      case JSOP_ARGCNT: {
        JS_ASSERT(function);
        TypeSet *types = localTypes.getVariable(cx, id_arguments(cx));
        types->addType(cx, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_ARGUMENTS));

        types->addGetProperty(cx, code, code->pushed(0), id_length(cx));
        break;
      }

      case JSOP_SETPROP:
      case JSOP_SETMETHOD: {
        jsid id = GetAtomId(cx, this, pc, 0);
        code->popped(1)->addSetProperty(cx, code, code->popped(0), id);

        MergePushed(cx, pool, code, 0, code->popped(0));
        break;
      }

      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        jsid id = GetAtomId(cx, this, pc, 0);
        code->popped(0)->addGetProperty(cx, code, code->pushed(0), id);

        if (op == JSOP_CALLPROP)
            code->popped(0)->addFilterPrimitives(cx, pool, code->pushed(1), true);
        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_PROPINC:
      case JSOP_PROPDEC: {
        jsid id = GetAtomId(cx, this, pc, 0);
        code->popped(0)->addGetProperty(cx, code, code->pushed(0), id);
        code->popped(0)->addSetProperty(cx, code, NULL, id);
        break;
      }

      case JSOP_GETTHISPROP: {
        jsid id = GetAtomId(cx, this, pc, 0);

        /* Need a new type set to model conversion of NULL to the global object. */
        TypeSet *newTypes = TypeSet::make(cx, pool, "thisprop");
        thisTypes.addTransformThis(cx, pool, newTypes);
        newTypes->addGetProperty(cx, code, code->pushed(0), id);

        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_GETARGPROP: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        TypeSet *types = localTypes.getVariable(cx, id);

        jsid propid = GetAtomId(cx, this, pc, SLOTNO_LEN);
        types->addGetProperty(cx, code, code->pushed(0), propid);

        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_GETLOCALPROP: {
        jsid id = getLocalId(GET_SLOTNO(pc), code->inStack);
        TypeSet *types = evalParent()->localTypes.getVariable(cx, id);

        jsid propid = GetAtomId(cx, this, pc, SLOTNO_LEN);
        types->addGetProperty(cx, code, code->pushed(0), propid);

        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_GETELEM:
      case JSOP_CALLELEM:
        code->popped(0)->addGetElem(cx, code, code->popped(1), code->pushed(0));

        CheckNextTest(cx, code, pc);

        if (op == JSOP_CALLELEM)
            code->popped(1)->addFilterPrimitives(cx, pool, code->pushed(1), true);
        break;

      case JSOP_SETELEM:
        code->popped(1)->addSetElem(cx, code, code->popped(2), code->popped(0));
        MergePushed(cx, pool, code, 0, code->popped(0));
        break;

      case JSOP_INCELEM:
      case JSOP_DECELEM:
      case JSOP_ELEMINC:
      case JSOP_ELEMDEC:
        code->popped(0)->addGetElem(cx, code, code->popped(1), code->pushed(0));
        code->popped(0)->addSetElem(cx, code, code->popped(1), NULL);
        break;

      case JSOP_LENGTH:
        /* Treat this as an access to the length property. */
        code->popped(0)->addGetProperty(cx, code, code->pushed(0), id_length(cx));
        break;

      case JSOP_THIS:
        thisTypes.addTransformThis(cx, pool, code->pushed(0));

        /* 'this' refers to the parent global/scope object in non-function scripts. */
        if (!function)
            code->setFixed(cx, 0, (jstype) cx->getGlobalTypeObject());
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
        if (function)
            code->popped(0)->addSubset(cx, pool, &function->returnTypes);
        break;

      case JSOP_ADD:
        code->popped(0)->addArith(cx, pool, code, code->pushed(0), code->popped(1));
        code->popped(1)->addArith(cx, pool, code, code->pushed(0), code->popped(0));
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_URSH:
        code->popped(0)->addArith(cx, pool, code, code->pushed(0));
        code->popped(1)->addArith(cx, pool, code, code->pushed(0));
        break;

      case JSOP_NEG:
      case JSOP_POS:
        code->popped(0)->addArith(cx, pool, code, code->pushed(0));
        break;

      case JSOP_LAMBDA:
      case JSOP_LAMBDA_FC:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC: {
        unsigned funOffset = 0;
        if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC)
            funOffset = SLOTNO_LEN;

        unsigned off = (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) ? SLOTNO_LEN : 0;
        JSObject *obj = GetScriptObject(cx, this, pc, off);
        TypeFunction *function = obj->getTypeObject()->asFunction();

        /* Remember where this script was defined. */
        function->script->analysis->parent = script;
        function->script->analysis->parentpc = pc;

        TypeSet *res = NULL;

        if (op == JSOP_LAMBDA || op == JSOP_LAMBDA_FC) {
            res = code->pushed(0);
        } else if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) {
            jsid id = getLocalId(GET_SLOTNO(pc), code->inStack);
            res = evalParent()->localTypes.getVariable(cx, id);
        } else {
            /* Watch for functions defined at the top level of an eval, see DEFVAR below. */
            JSAtom *atom = obj->getFunctionPrivate()->atom;
            JS_ASSERT(atom);
            jsid id = ATOM_TO_JSID(atom);
            if (parent) {
                if (this->function) {
                    /*
                     * Defined function as a script local variable.  TODO: Fix this case
                     * once the frontend decides what the semantics are here.
                     */
                    res = localTypes.getVariable(cx, id);
                } else {
                    /* Defined function in an eval. */
                    VariableSet *vars = SearchScope(cx, parent->analysis, parentCode()->inStack, id);
                    if (vars) {
                        res = vars->getVariable(cx, id);
                        res->addType(cx, TYPE_UNDEFINED);
                    }
                }
            } else {
                /* Defined function at global scope. */
                res = GetGlobalProperties(cx)->getVariable(cx, id);
            }
        }

        if (res)
            res->addType(cx, (jstype) function);
        else
            cx->compartment->types.monitorBytecode(code);
        break;
      }

      case JSOP_CALL:
      case JSOP_SETCALL:
      case JSOP_EVAL:
      case JSOP_APPLY:
      case JSOP_NEW: {
        /* Construct the base call information about this site. */
        unsigned argCount = GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = ArenaNew<TypeCallsite>(pool, code, op == JSOP_NEW, argCount);
        callsite->thisTypes = code->popped(argCount);
        callsite->returnTypes = code->pushed(0);

        for (unsigned i = 0; i < argCount; i++) {
            TypeSet *argTypes = code->popped(argCount - 1 - i);
            callsite->argumentTypes[i] = argTypes;
        }

        code->popped(argCount + 1)->addCall(cx, callsite);
        break;
      }

      case JSOP_NEWARRAY: {
        TypeObject *object = code->initObject;
        JS_ASSERT(object);
        code->pushed(0)->addType(cx, (jstype) object);

        TypeSet *types = object->indexTypes(cx);

        /* Propagate types of the initializer elements to the element type set. */
        unsigned useCount = GetUseCount(script, offset);
        for (unsigned i = 0; i < useCount; i++)
            code->popped(i)->addSubset(cx, pool, types);

        break;
      }

      case JSOP_NEWINIT: {
        TypeObject *object = code->initObject;
        JS_ASSERT(object);
        code->pushed(0)->addType(cx, (jstype) object);
        break;
      }

      case JSOP_INITELEM: {
        TypeObject *object = code->initObject;
        JS_ASSERT(object);

        /* TODO: broken for float indexes? */
        code->popped(0)->addSubset(cx, pool, object->indexTypes(cx));
        code->pushed(0)->addType(cx, (jstype) object);
        break;
      }

      case JSOP_INITPROP:
      case JSOP_INITMETHOD: {
        TypeObject *object = code->initObject;
        JS_ASSERT(object);

        code->pushed(0)->addType(cx, (jstype) object);

        jsid id = GetAtomId(cx, this, pc, 0);
        TypeSet *types = object->properties(cx).getVariable(cx, id);

        if (id == id___proto__(cx) || id == id_prototype(cx))
            cx->compartment->types.monitorBytecode(code);
        else
            code->popped(0)->addSubset(cx, pool, types);
        break;
      }

      case JSOP_ENTERWITH:
        /*
         * Scope lookups can occur on the value being pushed here.  We don't track
         * the value or its properties, and just monitor all name opcodes contained
         * by the with.
         */
        code->pushedArray[0].group()->boundWith = true;
        break;

      case JSOP_ENTERBLOCK: {
        JSObject *obj = GetScriptObject(cx, this, pc, 0);
        unsigned defCount = GetDefCount(script, offset);

        const Shape *shape = obj->lastProperty();
        for (unsigned i = 0; i < defCount; i++) {
            code->pushedArray[i].group()->letVariable = shape->id;
            shape = shape->previous();
        }
        break;
      }

      case JSOP_ITER: {
        uintN flags = pc[1];
        if (flags & JSITER_FOREACH)
            code->pushedArray[0].group()->isForEach = true;
        break;
      }

      case JSOP_MOREITER:
        code->setFixed(cx, 1, TYPE_BOOLEAN);
        break;

      case JSOP_FORNAME: {
        jsid id = GetAtomId(cx, this, pc, 0);
        Script *scopeScript;
        VariableSet *vars = SearchScope(cx, this, code->inStack, id, &scopeScript);

        if (vars) {
            SetForTypes(cx, code, vars->getVariable(cx, id));
        } else {
            cx->compartment->types.monitorBytecode(code);

            /*
             * We can't effectively monitor assigns to locals, so add the type to
             * any script which the variable *could* refer to.
             */
            if (scopeScript)
                SetForTypes(cx, code, scopeScript->localTypes.getVariable(cx, id));
        }
        break;
      }

      case JSOP_FORGLOBAL: {
        jsid id = GetGlobalId(cx, this, pc);
        SetForTypes(cx, code, GetGlobalProperties(cx)->getVariable(cx, id));
        break;
      }

      case JSOP_FORLOCAL: {
        jsid id = getLocalId(GET_SLOTNO(pc), code->inStack);
        JS_ASSERT(!JSID_IS_VOID(id));

        SetForTypes(cx, code, evalParent()->localTypes.getVariable(cx, id));
        break;
      }

      case JSOP_FORARG: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        JS_ASSERT(!JSID_IS_VOID(id));

        SetForTypes(cx, code, localTypes.getVariable(cx, id));
        break;
      }

      case JSOP_FORPROP:
      case JSOP_FORELEM:
      case JSOP_ENUMELEM:
        cx->compartment->types.monitorBytecode(code);
        break;

      case JSOP_ARRAYPUSH: {
        TypeSet *types = getStackTypes(GET_SLOTNO(pc), code->inStack);
        types->addSetProperty(cx, code, code->popped(0), JSID_VOID);
        break;
      }

      case JSOP_THROW:
        /* There will be a monitor on the bytecode catching the exception. */
        break;

      case JSOP_FINALLY:
        /* Pushes information about whether an exception was thrown. */
        break;

      case JSOP_EXCEPTION:
        /* Always monitor the values pushed by generated exceptions. */
        cx->compartment->types.monitorBytecode(code);
        break;

      case JSOP_DEFVAR:
        /*
         * Watch for variable declarations within an 'eval'.  We will effectively
         * ignore this declaration, merging references to it into the innermost
         * containing scope which declares a variable with the same name.
         * Get that scope and add void to the possible var types.
         */
        if (parent && !function) {
            jsid id = GetAtomId(cx, this, pc, 0);
            VariableSet *vars = SearchScope(cx, parent->analysis, parentCode()->inStack, id);
            if (vars) {
                TypeSet *types = vars->getVariable(cx, id);
                types->addType(cx, TYPE_UNDEFINED);
            }
        }
        break;

      case JSOP_DELPROP:
      case JSOP_DELELEM:
      case JSOP_DELNAME:
        /* TODO: watch for deletes on the global object. */
        code->setFixed(cx, 0, TYPE_BOOLEAN);
        break;

      case JSOP_LEAVEBLOCKEXPR:
        MergePushed(cx, pool, code, 0, code->popped(0));
        break;

      case JSOP_CASE:
        MergePushed(cx, pool, code, 0, code->popped(1));
        break;

      case JSOP_UNBRAND:
        MergePushed(cx, pool, code, 0, code->popped(0));
        break;

      case JSOP_GENERATOR:
        if (function) {
            TypeObject *object = cx->getFixedTypeObject(TYPE_OBJECT_NEW_ITERATOR);
            function->returnTypes.addType(cx, (jstype) object);
        }
        break;

      case JSOP_YIELD:
        cx->compartment->types.monitorBytecode(code);
        break;

      case JSOP_XMLNAME:
      case JSOP_CALLXMLNAME:
      case JSOP_SETXMLNAME:
        cx->compartment->types.monitorBytecode(code);
        break;

      case JSOP_BINDXMLNAME:
        break;

      case JSOP_TOXML:
      case JSOP_TOXMLLIST:
      case JSOP_XMLPI:
      case JSOP_XMLCDATA:
      case JSOP_XMLCOMMENT:
      case JSOP_DESCENDANTS:
        code->setFixed(cx, 0, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_NEW_XML));
        break;

      case JSOP_TOATTRNAME:
      case JSOP_QNAMECONST:
      case JSOP_QNAME:
        code->setFixed(cx, 0, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_NEW_QNAME));
        break;

      case JSOP_ANYNAME:
      case JSOP_GETFUNNS:
        code->setFixed(cx, 0, (jstype) cx->getFixedTypeObject(TYPE_OBJECT_NEW_NAMESPACE));
        break;

      case JSOP_FILTER:
        /* Note: the second value pushed by filter is a hole, and not modelled. */
        MergePushed(cx, pool, code, 0, code->popped(0));
        code->pushedArray[0].group()->boundWith = true;

        /* Name binding inside filters is currently broken :FIXME: bug 605200. */
        cx->compartment->types.ignoreWarnings = true;
        break;

      case JSOP_ENDFILTER:
        MergePushed(cx, pool, code, 0, code->popped(1));
        break;

      case JSOP_DEFSHARP: {
        /*
         * This bytecode uses the value at the top of the stack, though this is
         * not specified in the opcode table.
         */
        JS_ASSERT(code->inStack);
        TypeSet *value = &code->inStack->group()->types;

        /* Model sharp values as local variables. */
        char name[24];
        JS_snprintf(name, sizeof(name), "#%d:%d",
                    GET_UINT16(pc), GET_UINT16(pc + UINT16_LEN));
        JSAtom *atom = js_Atomize(cx, name, strlen(name), ATOM_PINNED);
        jsid id = ATOM_TO_JSID(atom);

        TypeSet *types = evalParent()->localTypes.getVariable(cx, id);
        value->addSubset(cx, pool, types);
        break;
      }

      case JSOP_USESHARP: {
        char name[24];
        JS_snprintf(name, sizeof(name), "#%d:%d",
                    GET_UINT16(pc), GET_UINT16(pc + UINT16_LEN));
        JSAtom *atom = js_Atomize(cx, name, strlen(name), ATOM_PINNED);
        jsid id = ATOM_TO_JSID(atom);

        TypeSet *types = evalParent()->localTypes.getVariable(cx, id);
        MergePushed(cx, evalParent()->pool, code, 0, types);
        break;
      }

      case JSOP_CALLEE:
        JS_ASSERT(function);
        code->setFixed(cx, 0, (jstype) function);
        break;

      default:
        cx->compartment->types.warnings = true;
        fprintf(cx->typeOut(), "warning: Unknown bytecode: %s\n", js_CodeNameTwo[op]);
    }
}

void
Script::freezeAllTypes(JSContext *cx)
{
    JS_ASSERT(!compiled);
    compiled = true;

    unsigned offset = 0;
    while (offset < script->length) {
        Bytecode *code = codeArray[offset];

        if (code && code->analyzed)
            freezeTypes(cx, code);

        offset += GetBytecodeLength(script->code + offset);
    }
}

void
Script::freezeTypes(JSContext *cx, Bytecode *code)
{
    unsigned offset = code->offset;
    unsigned useCount = GetUseCount(script, offset);

    JS_ASSERT(code->analyzed);
    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

    switch (op) {

      case JSOP_IFEQ:
      case JSOP_IFEQX:
      case JSOP_IFNE:
      case JSOP_IFNEX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LSH:
      case JSOP_RSH:
      case JSOP_URSH:
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITAND:
      case JSOP_BITNOT:
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_NOT:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
      case JSOP_IN:
      case JSOP_DIV:
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
      case JSOP_GETPROP:
      case JSOP_GETXPROP:
      case JSOP_CALLPROP:
      case JSOP_GETELEM:
      case JSOP_CALLELEM:
      case JSOP_LENGTH:
      case JSOP_ADD:
      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_NEG:
        for (unsigned i = 0; i < useCount; i++)
            code->popped(i)->addFreeze(cx, pool, code);
        break;

      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC: {
        jsid id = GetAtomId(cx, this, pc, 0);

        VariableSet *vars = SearchScope(cx, this, code->inStack, id);
        if (vars) {
            TypeSet *types = vars->getVariable(cx, id);
            types->addFreeze(cx, *vars->pool, code);
        }
        break;
      }

      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC: {
        jsid id = GetAtomId(cx, this, pc, 0);
        TypeSet *types = GetGlobalProperties(cx)->getVariable(cx, id);

        types->addFreeze(cx, cx->compartment->types.pool, code);
        break;
      }

      case JSOP_INCGLOBAL:
      case JSOP_DECGLOBAL:
      case JSOP_GLOBALINC:
      case JSOP_GLOBALDEC: {
        jsid id = GetGlobalId(cx, this, pc);
        TypeSet *types = GetGlobalProperties(cx)->getVariable(cx, id);

        types->addFreeze(cx, cx->compartment->types.pool, code);
        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_GETARGPROP: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        TypeSet *types = localTypes.getVariable(cx, id);

        types->addFreeze(cx, pool, code);
        break;
      }

      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
      case JSOP_GETLOCALPROP: {
        jsid id = getLocalId(GET_SLOTNO(pc), code->inStack);
        TypeSet *types = evalParent()->localTypes.getVariable(cx, id);

        types->addFreeze(cx, evalParent()->pool, code);
        break;
      }

      case JSOP_SETPROP:
      case JSOP_SETMETHOD:
        code->popped(1)->addFreeze(cx, pool, code);
        break;

      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_PROPINC:
      case JSOP_PROPDEC: {
        jsid id = GetAtomId(cx, this, pc, 0);
        code->popped(0)->addFreeze(cx, pool, code);
        code->popped(0)->addFreezeProp(cx, pool, code, id);
        break;
      }

      case JSOP_GETTHISPROP:
        thisTypes.addFreeze(cx, pool, code);
        break;

      case JSOP_SETELEM:
        code->popped(1)->addFreeze(cx, pool, code);
        code->popped(2)->addFreeze(cx, pool, code);
        break;

      case JSOP_INCELEM:
      case JSOP_DECELEM:
      case JSOP_ELEMINC:
      case JSOP_ELEMDEC:
        code->popped(0)->addFreeze(cx, pool, code);
        code->popped(1)->addFreeze(cx, pool, code);
        code->popped(0)->addFreezeElem(cx, pool, code, code->popped(1));
        break;

      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_APPLY:
      case JSOP_NEW: {
        /*
         * The only value affecting the behavior of a call is the callee,
         * everything else is just copied around.
         */
        TypeSet *funTypes = code->popped(useCount - 1);
        funTypes->addFreeze(cx, pool, code);
        break;
      }

      default:
        break;
    }
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

void
Bytecode::print(JSContext *cx, FILE *out)
{
    jsbytecode *pc = script->getScript()->code + offset;

    JSOp op = (JSOp)*pc;
    JS_ASSERT(op < JSOP_LIMIT);

    const JSCodeSpec *cs = &js_CodeSpec[op];
    const char *name = js_CodeNameTwo[op];

    uint32 type = JOF_TYPE(cs->format);
    switch (type) {
      case JOF_BYTE:
      case JOF_TABLESWITCH:
      case JOF_TABLESWITCHX:
      case JOF_LOOKUPSWITCH:
      case JOF_LOOKUPSWITCHX:
        fprintf(out, "%s", name);
        break;

      case JOF_JUMP:
      case JOF_JUMPX: {
        ptrdiff_t off = GetJumpOffset(pc, pc);
        fprintf(out, "%s %u", name, unsigned(offset + off));
        break;
      }

      case JOF_ATOM: {
        if (op == JSOP_DOUBLE) {
            fprintf(out, "%s", name);
        } else {
            jsid id = GetAtomId(cx, script, pc, 0);
            if (JSID_IS_STRING(id))
                fprintf(out, "%s %s", name, cx->getTypeId(id));
            else
                fprintf(out, "%s (index)", name);
        }
        break;
      }

      case JOF_OBJECT:
        fprintf(out, "%s (object)", name);
        break;

      case JOF_REGEXP:
        fprintf(out, "%s (regexp)", name);
        break;

      case JOF_UINT16PAIR:
        fprintf(out, "%s %d %d", name, GET_UINT16(pc), GET_UINT16(pc + UINT16_LEN));
        break;

      case JOF_UINT16:
        fprintf(out, "%s %d", name, GET_UINT16(pc));
        break;

      case JOF_QARG: {
        jsid id = script->getArgumentId(GET_ARGNO(pc));
        fprintf(out, "%s %s", name, cx->getTypeId(id));
        break;
      }

      case JOF_GLOBAL:
        fprintf(out, "%s %s", name, cx->getTypeId(GetGlobalId(cx, script, pc)));
        break;

      case JOF_LOCAL:
        if ((op != JSOP_ARRAYPUSH) && (analyzed || (GET_SLOTNO(pc) < script->getScript()->nfixed))) {
            jsid id = script->getLocalId(GET_SLOTNO(pc), inStack);
            fprintf(out, "%s %d %s", name, GET_SLOTNO(pc), cx->getTypeId(id));
        } else {
            fprintf(out, "%s %d", name, GET_SLOTNO(pc));
        }
        break;

      case JOF_SLOTATOM: {
        jsid id = GetAtomId(cx, script, pc, SLOTNO_LEN);

        jsid slotid = JSID_VOID;
        if (op == JSOP_GETARGPROP)
            slotid = script->getArgumentId(GET_ARGNO(pc));
        if (op == JSOP_GETLOCALPROP && (analyzed || (GET_SLOTNO(pc) < script->getScript()->nfixed)))
            slotid = script->getLocalId(GET_SLOTNO(pc), inStack);

        fprintf(out, "%s %u %s %s", name, GET_SLOTNO(pc), cx->getTypeId(slotid), cx->getTypeId(id));
        break;
      }

      case JOF_SLOTOBJECT:
        fprintf(out, "%s %u (object)", name, GET_SLOTNO(pc));
        break;

      case JOF_UINT24:
        JS_ASSERT(op == JSOP_UINT24 || op == JSOP_NEWARRAY);
        fprintf(out, "%s %d", name, (jsint)GET_UINT24(pc));
        break;

      case JOF_UINT8:
        fprintf(out, "%s %d", name, (jsint)pc[1]);
        break;

      case JOF_INT8:
        fprintf(out, "%s %d", name, (jsint)GET_INT8(pc));
        break;

      case JOF_INT32:
        fprintf(out, "%s %d", name, (jsint)GET_INT32(pc));
        break;

      default:
        JS_NOT_REACHED("Unknown opcode type");
    }
}

void
Script::print(JSContext *cx)
{
    if (!codeArray)
        return;

    TypeCompartment *compartment = &script->compartment->types;
    FILE *out = compartment->out;

    /*
     * Check if there are warnings for used values with unknown types, and build
     * statistics about the size of type sets found for stack values.
     */
    for (unsigned offset = 0; offset < script->length; offset++) {
        Bytecode *code = codeArray[offset];
        if (!code || !code->analyzed)
            continue;

        unsigned useCount = GetUseCount(script, offset);
        if (!useCount)
            continue;

        TypeStack *stack = code->inStack->group();
        for (unsigned i = 0; i < useCount; i++) {
            TypeSet *types = &stack->types;

            /* TODO: distinguish direct and indirect call sites. */
            unsigned typeCount = types->objectCount ? 1 : 0;
            for (jstype type = TYPE_UNDEFINED; type <= TYPE_STRING; type++) {
                if (types->typeFlags & (1 << type))
                    typeCount++;
            }

            /*
             * Adjust the type counts for floats: values marked as floats
             * are also marked as ints by the inference, but for counting
             * we don't consider these to be separate types.
             */
            if (types->typeFlags & TYPE_FLAG_DOUBLE) {
                JS_ASSERT(types->typeFlags & TYPE_FLAG_INT32);
                typeCount--;
            }

            if ((types->typeFlags & TYPE_FLAG_UNKNOWN) ||
                typeCount > TypeCompartment::TYPE_COUNT_LIMIT) {
                compartment->typeCountOver++;
            } else if (typeCount == 0) {
                /* Ignore values without types, this may be unreached code. */
            } else {
                compartment->typeCounts[typeCount-1]++;
            }

            stack = stack->innerStack ? stack->innerStack->group() : NULL;
        }
    }

    if (parent) {
        if (function)
            fputs("Function", out);
        else
            fputs("Eval", out);

        fprintf(out, " #%u @%u\n", id, parent->analysis->id);
    } else {
        fprintf(out, "Main #%u:\n", id);
    }

    if (!codeArray) {
        fputs("(unused)\n", out);
        return;
    }

    /* Print out points where variables became unconditionally defined. */
    fputs("defines:", out);
    for (unsigned i = 0; i < localCount(); i++) {
        if (locals[i] != LOCAL_USE_BEFORE_DEF && locals[i] != LOCAL_CONDITIONALLY_DEFINED)
            fprintf(out, " %s@%u", cx->getTypeId(getLocalId(i, NULL)), locals[i]);
    }
    fputs("\n", out);

    fputs("locals:", out);
    localTypes.print(cx, out);

    int id_count = 0;

    for (unsigned offset = 0; offset < script->length; offset++) {
        Bytecode *code = codeArray[offset];
        if (!code)
            continue;

        fprintf(out, "#%u:%05u:  ", id, offset);
        code->print(cx, out);
        fputs("\n", out);

        if (code->defineCount) {
            fputs("  defines:", out);
            for (unsigned i = 0; i < code->defineCount; i++) {
                uint32 local = code->defineArray[i];
                fprintf(out, " %s", cx->getTypeId(getLocalId(local, NULL)));
            }
            fputs("\n", out);
        }

        TypeStack *stack;
        unsigned useCount = GetUseCount(script, offset);
        if (useCount) {
            fputs("  use:", out);
            stack = code->inStack->group();
            for (unsigned i = 0; i < useCount; i++) {
                if (!stack->id)
                    stack->id = ++id_count;
                fprintf(out, " %d", stack->id);
                stack = stack->innerStack ? stack->innerStack->group() : NULL;
            }
            fputs("\n", out);

            /* Watch for stack values without any types. */
            stack = code->inStack->group();
            for (unsigned i = 0; i < useCount; i++) {
                if (!IgnorePopped((JSOp)script->code[offset], i)) {
                    if (stack->types.typeFlags == 0)
                        fprintf(out, "  missing stack: %d\n", stack->id);
                }
                stack = stack->innerStack ? stack->innerStack->group() : NULL;
            }
        }

        unsigned defCount = GetDefCount(script, offset);
        if (defCount) {
            fputs("  def:", out);
            for (unsigned i = 0; i < defCount; i++) {
                stack = code->pushedArray[i].group();
                if (!stack->id)
                    stack->id = ++id_count;
                fprintf(out, " %d", stack->id);
            }
            fputs("\n", out);
            for (unsigned i = 0; i < defCount; i++) {
                stack = code->pushedArray[i].group();
                fprintf(out, "  type %d:", stack->id);
                stack->types.print(cx, out);
                fputs("\n", out);
            }
        }

        if (code->monitorNeeded)
            fprintf(out, "  monitored\n");
    }

    fputs("\n", out);

    TypeObject *object = objects;
    while (object) {
        object->print(cx, out);
        object = object->next;
    }
}

} } /* namespace js::analyze */
