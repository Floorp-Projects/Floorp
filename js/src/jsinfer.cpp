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
#include "jsgc.h"
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

#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"

#include "jsatominlines.h"
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

#ifdef DEBUG
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
// Logging
/////////////////////////////////////////////////////////////////////

static bool InferSpewActive(SpewChannel channel)
{
    static bool active[SPEW_COUNT];
    static bool checked = false;
    if (!checked) {
        checked = true;
        PodArrayZero(active);
        const char *env = getenv("INFERFLAGS");
        if (!env)
            return false;
        if (strstr(env, "dynamic"))
            active[ISpewDynamic] = true;
        if (strstr(env, "ops"))
            active[ISpewOps] = true;
        if (strstr(env, "result"))
            active[ISpewResult] = true;
        if (strstr(env, "full")) {
            for (unsigned i = 0; i < SPEW_COUNT; i++)
                active[i] = true;
        }
    }
    return active[channel];
}

#ifdef DEBUG

const char *
TypeString(jstype type)
{
    switch (type) {
      case TYPE_UNDEFINED:
        return "void";
      case TYPE_NULL:
        return "null";
      case TYPE_BOOLEAN:
        return "bool";
      case TYPE_INT32:
        return "int";
      case TYPE_DOUBLE:
        return "float";
      case TYPE_STRING:
        return "string";
      case TYPE_UNKNOWN:
        return "unknown";
      default: {
        JS_ASSERT(TypeIsObject(type));
        TypeObject *object = (TypeObject *) type;
        return object->name();
      }
    }
}

void InferSpew(SpewChannel channel, const char *fmt, ...)
{
    if (!InferSpewActive(channel))
        return;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[infer] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

#endif

void TypeFailure(JSContext *cx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[infer failure] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);

    cx->compartment->types.finish(cx, cx->compartment);

    fflush(stdout);
    *((int*)NULL) = 0;  /* Type warnings */
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    InferSpew(ISpewOps, "addConstraint: T%u C%u %s",
              id(), constraint->id(), constraint->kind());

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
TypeSet::print(JSContext *cx)
{
    if (typeFlags == 0) {
        printf(" missing");
        return;
    }

    if (typeFlags & TYPE_FLAG_UNKNOWN)
        printf(" unknown");

    if (typeFlags & TYPE_FLAG_UNDEFINED)
        printf(" void");
    if (typeFlags & TYPE_FLAG_NULL)
        printf(" null");
    if (typeFlags & TYPE_FLAG_BOOLEAN)
        printf(" bool");
    if (typeFlags & TYPE_FLAG_INT32)
        printf(" int");
    if (typeFlags & TYPE_FLAG_DOUBLE)
        printf(" float");
    if (typeFlags & TYPE_FLAG_STRING)
        printf(" string");

    if (typeFlags & TYPE_FLAG_OBJECT) {
        printf(" object[%u]", objectCount);

        if (objectCount >= 2) {
            unsigned objectCapacity = HashSetCapacity(objectCount);
            for (unsigned i = 0; i < objectCapacity; i++) {
                TypeObject *object = objectSet[i];
                if (object)
                    printf(" %s", object->name());
            }
        } else if (objectCount == 1) {
            TypeObject *object = (TypeObject*) objectSet;
            printf(" %s", object->name());
        }
    }
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

/* Constraints for determining the 'this' object at sites invoked using 'new'. */
class TypeConstraintNewObject : public TypeConstraint
{
    TypeFunction *fun;
    TypeSet *target;

  public:
    TypeConstraintNewObject(TypeFunction *fun, TypeSet *target)
        : TypeConstraint("newObject"), fun(fun), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addNewObject(JSContext *cx, TypeFunction *fun, TypeSet *target)
{
    JS_ASSERT(this->pool == fun->pool);
    add(cx, ArenaNew<TypeConstraintNewObject>(*fun->pool, fun, target));
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
    analyze::Bytecode *code;
    TypeSet *target;

    TypeConstraintTransformThis(analyze::Bytecode *code, TypeSet *target)
        : TypeConstraint("transformthis"), code(code), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addTransformThis(JSContext *cx, analyze::Bytecode *code, TypeSet *target)
{
    JS_ASSERT(this->pool == &code->pool());
    add(cx, ArenaNew<TypeConstraintTransformThis>(code->pool(), code, target));
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

/*
 * Type constraint which marks the result of 'for in' loops as unknown if the
 * iterated value could be a generator.
 */
class TypeConstraintGenerator : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintGenerator(TypeSet *target)
        : TypeConstraint("generator"), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

/* Update types with the possible values bound by the for loop in code. */
static inline void
SetForTypes(JSContext *cx, const analyze::Script::AnalyzeState &state,
            analyze::Bytecode *code, TypeSet *types)
{
    JS_ASSERT(code->stackDepth == state.stackDepth);
    if (state.popped(0).isForEach)
        types->addType(cx, TYPE_UNKNOWN);
    else
        types->addType(cx, TYPE_STRING);

    code->popped(0)->add(cx, ArenaNew<TypeConstraintGenerator>(code->pool(), types));
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
GetPropertyObject(JSContext *cx, analyze::Script *script, jstype type)
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
        return script->getTypeNewObject(cx, JSProto_Number);

      case TYPE_BOOLEAN:
        return script->getTypeNewObject(cx, JSProto_Boolean);

      case TYPE_STRING:
        return script->getTypeNewObject(cx, JSProto_String);

      default:
        /* undefined and null do not have properties. */
        return NULL;
    }
}

/*
 * Handle a property access on a specific object. All property accesses go through
 * here, whether via x.f, x[f], or global name accesses.
 */
static inline void
PropertyAccess(JSContext *cx, analyze::Bytecode *code, TypeObject *object,
               bool assign, TypeSet *target, jsid id)
{
    JS_ASSERT_IF(!target, assign);

    /* Reads from objects with unknown properties are unknown, writes to such objects are ignored. */
    if (object->unknownProperties) {
        if (!assign)
            target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    /* Monitor assigns on the 'prototype' property. */
    if (assign && id == id_prototype(cx)) {
        cx->compartment->types.monitorBytecode(cx, code);
        return;
    }

    /* Monitor accesses on other properties with special behavior we don't keep track of. */
    if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx)) {
        if (assign)
            cx->compartment->types.monitorBytecode(cx, code);
        else
            target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    /* Mark arrays with possible non-integer properties as not dense. */
    if (assign && !JSID_IS_VOID(id))
        cx->markTypeArrayNotPacked(object, true, false);

    /* Capture the effects of a standard property access. */
    if (target) {
        TypeSet *types = object->getProperty(cx, id, assign);
        if (assign)
            target->addSubset(cx, code->pool(), types);
        else
            types->addMonitorRead(cx, *object->pool, code, target);
    } else {
        TypeSet *readTypes = object->getProperty(cx, id, false);
        TypeSet *writeTypes = object->getProperty(cx, id, true);
        if (code->hasIncDecOverflow)
            writeTypes->addType(cx, TYPE_DOUBLE);
        readTypes->addArith(cx, *object->pool, code, writeTypes);
    }
}

void
TypeConstraintProp::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN ||
        (!TypeIsObject(type) && !code->script->getScript()->compileAndGo)) {
        /*
         * Access on an unknown object.  Reads produce an unknown result, writes
         * need to be monitored.  Note: this isn't a problem for handling overflows
         * on inc/dec below, as these go through a slow path which must call
         * addTypeProperty.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, code);
        else
            target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, code->script, type);
    if (object)
        PropertyAccess(cx, code, object, assign, target, id);
}

void
TypeConstraintElem::newType(JSContext *cx, TypeSet *source, jstype type)
{
    switch (type) {
      case TYPE_UNDEFINED:
      case TYPE_BOOLEAN:
      case TYPE_NULL:
      case TYPE_INT32:
      case TYPE_DOUBLE:
        /*
         * Integer index access, these are all covered by the JSID_VOID property.
         * We are optimistically treating non-number accesses as not actually occurring,
         * and double accesses as getting an integer property. This must be checked
         * at runtime.
         */
        if (assign)
            object->addSetProperty(cx, code, target, JSID_VOID);
        else
            object->addGetProperty(cx, code, target, JSID_VOID);
        break;
      default:
        /*
         * Access to a potentially arbitrary element. Monitor assignments to unknown
         * elements, and treat reads of unknown elements as unknown.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, code);
        else
            target->addType(cx, TYPE_UNKNOWN);
    }
};

void
TypeConstraintNewObject::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject *) type;
        TypeSet *newTypes = object->getProperty(cx, JSID_EMPTY, true);
        newTypes->addSubset(cx, *object->pool, target);
    } else if (!fun->script) {
        /*
         * This constraint should only be used for native constructors with
         * immutable non-primitive prototypes. Disregard primitives here.
         */
    } else if (!fun->script->compileAndGo) {
        target->addType(cx, TYPE_UNKNOWN);
    } else {
        target->addType(cx, (jstype) fun->script->analysis->getTypeNewObject(cx, JSProto_Object));
    }
}

void
TypeConstraintCall::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        /* Monitor calls on unknown functions. */
        cx->compartment->types.monitorBytecode(cx, callsite->code);
        return;
    }

    /* Get the function being invoked. */
    TypeFunction *function = NULL;
    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject*) type;
        if (object->isFunction) {
            function = (TypeFunction*) object;
        } else {
            /* Unknown return value for calls on non-function objects. */
            cx->compartment->types.monitorBytecode(cx, callsite->code);
        }
    }
    if (!function)
        return;

    JSArenaPool &pool = callsite->pool();

    if (!function->script) {
        JS_ASSERT(function->handler);

        if (function->handler == JS_TypeHandlerDynamic) {
            /* Unknown result. */
            if (callsite->returnTypes)
                callsite->returnTypes->addType(cx, TYPE_UNKNOWN);
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
            callsite->argumentTypes[0]->addTransformThis(cx, callsite->code, thisTypes);

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
            TypeSet *types = script->getVariable(cx, id);
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
    for (unsigned i = callsite->argumentCount; i < script->argCount(); i++) {
        jsid id = script->getArgumentId(i);
        if (!JSID_IS_VOID(id)) {
            TypeSet *types = script->getVariable(cx, id);
            types->addType(cx, TYPE_UNDEFINED);
        }
    }

    /* Add a binding for the receiver object of the call. */
    if (callsite->isNew) {
        /* The receiver object is the 'new' object for the function's prototype. */
        if (function->unknownProperties) {
            script->thisTypes.addType(cx, TYPE_UNKNOWN);
        } else {
            TypeSet *prototypeTypes = function->getProperty(cx, id_prototype(cx), false);
            prototypeTypes->addNewObject(cx, function, &script->thisTypes);
        }

        /*
         * If the script does not return a value then the pushed value is the new
         * object (typical case).
         */
        if (callsite->returnTypes) {
            script->thisTypes.addSubset(cx, script->pool, callsite->returnTypes);
            function->returnTypes.addFilterPrimitives(cx, *function->pool,
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
            function->returnTypes.addSubset(cx, *function->pool, callsite->returnTypes);
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
         *   {int,bool} x {int,bool} -> int
         *   float x {int,bool,float} -> float
         *   string x any -> string
         */
        switch (type) {
          case TYPE_UNDEFINED:
          case TYPE_NULL:
          case TYPE_INT32:
          case TYPE_BOOLEAN:
            /* Note: need to account for overflows from, e.g. int + void */
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
            target->addType(cx, TYPE_UNKNOWN);
            break;
        }
    } else {
        /* Note: same issues with undefined as addition. */
        switch (type) {
          case TYPE_UNDEFINED:
          case TYPE_NULL:
          case TYPE_INT32:
          case TYPE_BOOLEAN:
            target->addType(cx, TYPE_INT32);
            break;
          case TYPE_DOUBLE:
            target->addType(cx, TYPE_DOUBLE);
            break;
          default:
            target->addType(cx, TYPE_UNKNOWN);
            break;
        }
    }
}

void
TypeConstraintTransformThis::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN || TypeIsObject(type) || code->script->getScript()->strictModeCode) {
        target->addType(cx, type);
        return;
    }

    if (!code->script->getScript()->compileAndGo) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeObject *object = NULL;
    switch (type) {
      case TYPE_NULL:
      case TYPE_UNDEFINED:
        object = code->script->getGlobalType();
        break;
      case TYPE_INT32:
      case TYPE_DOUBLE:
        object = code->script->getTypeNewObject(cx, JSProto_Number);
        break;
      case TYPE_BOOLEAN:
        object = code->script->getTypeNewObject(cx, JSProto_Boolean);
        break;
      case TYPE_STRING:
        object = code->script->getTypeNewObject(cx, JSProto_String);
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
    } else if (type != TYPE_UNKNOWN && TypeIsPrimitive(type)) {
        return;
    }

    target->addType(cx, type);
}

void
TypeConstraintMonitorRead::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == (jstype) cx->compartment->types.typeGetSet) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    target->addType(cx, type);
}

void
TypeConstraintGenerator::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    if (TypeIsPrimitive(type))
        return;

    /*
     * Watch for 'for in' loops which could produce values other than strings.
     * This includes loops on Iterator and Generator objects, and objects with
     * a custom __iterator__ property that is marked as typeGetSet (see GetCustomIterator).
     */
    TypeObject *object = (TypeObject *) type;
    if (object == cx->compartment->types.typeGetSet)
        target->addType(cx, TYPE_UNKNOWN);
    if (object->proto) {
        Class *clasp = object->proto->getClass();
        if (clasp == &js_IteratorClass || clasp == &js_GeneratorClass)
            target->addType(cx, TYPE_UNKNOWN);
    }
}

/* Constraint marking incoming arrays as possibly packed. */
class TypeConstraintPossiblyPacked : public TypeConstraint
{
public:
    TypeConstraintPossiblyPacked() : TypeConstraint("possiblyPacked") {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (type != TYPE_UNKNOWN && TypeIsObject(type)) {
            TypeObject *object = (TypeObject *) type;
            object->possiblePackedArray = true;
        }
    }
};

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

/*
 * Constraint which triggers recompilation of a script if a possible new JSValueType
 * tag is realized for a type set.
 */
class TypeConstraintFreezeTypeTag : public TypeConstraint
{
public:
    JSScript *script;

    /*
     * Whether the type tag has been marked unknown due to a type change which
     * occurred after this constraint was generated (and which triggered recompilation).
     */
    bool typeUnknown;

    TypeConstraintFreezeTypeTag(JSScript *script)
        : TypeConstraint("freezeTypeTag"),
          script(script), typeUnknown(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (typeUnknown)
            return;

        if (type != TYPE_UNKNOWN && TypeIsObject(type)) {
            /* Ignore new objects when the type set already has other objects. */
            if (source->objectCount >= 2) {
                JS_ASSERT(source->typeFlags == TYPE_FLAG_OBJECT);
                return;
            }
        }

        typeUnknown = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

static inline JSValueType
GetValueTypeFromTypeFlags(TypeFlags flags)
{
    switch (flags) {
      case TYPE_FLAG_UNDEFINED:
        return JSVAL_TYPE_UNDEFINED;
      case TYPE_FLAG_NULL:
        return JSVAL_TYPE_NULL;
      case TYPE_FLAG_BOOLEAN:
        return JSVAL_TYPE_BOOLEAN;
      case TYPE_FLAG_INT32:
        return JSVAL_TYPE_INT32;
      case (TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE):
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      case TYPE_FLAG_OBJECT:
        return JSVAL_TYPE_OBJECT;
      default:
        return JSVAL_TYPE_UNKNOWN;
    }
}

JSValueType
TypeSet::getKnownTypeTag(JSContext *cx, JSScript *script)
{
    JSValueType type = GetValueTypeFromTypeFlags(typeFlags);

    if (script && type != JSVAL_TYPE_UNKNOWN) {
        JS_ASSERT(this->pool == &script->analysis->pool);
        add(cx, ArenaNew<TypeConstraintFreezeTypeTag>(script->analysis->pool, script), false);
    }

    return type;
}

/* Compute the meet of kind with the kind of object, per the ObjectKind lattice. */
static inline ObjectKind
CombineObjectKind(TypeObject *object, ObjectKind kind)
{
    /*
     * Our initial guess is that all arrays are packed, but if the array is
     * created through [], Array() or Array(N) and we don't see later code
     * which looks to be filling it in starting at zero, consider it not packed.
     * All requests for the kind of an object go through here, so there are
     * no FreezeObjectKind constraints to update if we unset isPackedArray here.
     */
    if (object->isPackedArray && !object->possiblePackedArray) {
        InferSpew(ISpewDynamic, "Possible unpacked array: %s", object->name());
        object->isPackedArray = false;
    }

    ObjectKind nkind;
    if (object->isFunction)
        nkind = object->asFunction()->script ? OBJECT_SCRIPTED_FUNCTION : OBJECT_NATIVE_FUNCTION;
    else if (object->isPackedArray)
        nkind = OBJECT_PACKED_ARRAY;
    else if (object->isDenseArray)
        nkind = OBJECT_DENSE_ARRAY;
    else
        nkind = OBJECT_UNKNOWN;

    if (kind == OBJECT_UNKNOWN || nkind == OBJECT_UNKNOWN)
        return OBJECT_UNKNOWN;

    if (kind == nkind || kind == OBJECT_NONE)
        return nkind;

    if ((kind == OBJECT_PACKED_ARRAY && nkind == OBJECT_DENSE_ARRAY) ||
        (kind == OBJECT_DENSE_ARRAY && nkind == OBJECT_PACKED_ARRAY)) {
        return OBJECT_DENSE_ARRAY;
    }

    return OBJECT_UNKNOWN;
}

/* Constraint which triggers recompilation if an array becomes not-packed or not-dense. */
class TypeConstraintFreezeArray : public TypeConstraint
{
public:
    /*
     * Array kind being specialized on by the FreezeObjectConstraint.  This may have
     * become OBJECT_UNKNOWN due to subsequent type changes and recompilation.
     */
    ObjectKind *pkind;

    JSScript *script;

    TypeConstraintFreezeArray(ObjectKind *pkind, JSScript *script)
        : TypeConstraint("freezeArray"),
          pkind(pkind), script(script)
    {
        JS_ASSERT(*pkind == OBJECT_PACKED_ARRAY || *pkind == OBJECT_DENSE_ARRAY);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type) {}

    void arrayNotPacked(JSContext *cx, bool notDense)
    {
        if (*pkind == OBJECT_UNKNOWN) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        JS_ASSERT(*pkind == OBJECT_PACKED_ARRAY || *pkind == OBJECT_DENSE_ARRAY);

        if (!notDense && *pkind == OBJECT_DENSE_ARRAY) {
            /* Marking an array as not packed, but we were already accounting for this. */
            return;
        }

        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

/*
 * Constraint which triggers recompilation if objects of a different kind are
 * added to a type set.
 */
class TypeConstraintFreezeObjectKind : public TypeConstraint
{
public:
    ObjectKind kind;
    JSScript *script;

    TypeConstraintFreezeObjectKind(ObjectKind kind, JSScript *script)
        : TypeConstraint("freezeObjectKind"),
          kind(kind), script(script)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (kind == OBJECT_UNKNOWN) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        if (type == TYPE_UNKNOWN) {
            kind = OBJECT_UNKNOWN;
        } else if (TypeIsObject(type)) {
            TypeObject *object = (TypeObject *) type;
            ObjectKind nkind = CombineObjectKind(object, kind);

            if (nkind != OBJECT_UNKNOWN &&
                (kind == OBJECT_PACKED_ARRAY || kind == OBJECT_DENSE_ARRAY)) {
                /*
                 * Arrays can become not-packed or not-dense dynamically.
                 * Add a constraint on the element type of the object to pick
                 * up such changes.
                 */
                TypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
                elementTypes->add(cx,
                    ArenaNew<TypeConstraintFreezeArray>(*object->pool, &kind, script), false);
            }

            if (nkind == kind) {
                /* New object with the same kind we are interested in. */
                return;
            }
            kind = nkind;

            cx->compartment->types.addPendingRecompile(cx, script);
        }
    }
};

ObjectKind
TypeSet::getKnownObjectKind(JSContext *cx, JSScript *script)
{
    JS_ASSERT(this->pool == &script->analysis->pool);

    ObjectKind kind = OBJECT_NONE;

    if (objectCount >= 2) {
        unsigned objectCapacity = HashSetCapacity(objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = objectSet[i];
            if (object)
                kind = CombineObjectKind(object, kind);
        }
    } else if (objectCount == 1) {
        kind = CombineObjectKind((TypeObject *) objectSet, kind);
    }

    if (kind != OBJECT_UNKNOWN) {
        /*
         * Watch for new objects of different kind, and re-traverse existing types
         * in this set to add any needed FreezeArray constraints.
         */
        add(cx, ArenaNew<TypeConstraintFreezeObjectKind>(script->analysis->pool,
                                                         kind, script), true);
    }

    return kind;
}

/* Constraint which triggers recompilation if any type is added to a type set. */
class TypeConstraintFreezeNonEmpty : public TypeConstraint
{
public:
    JSScript *script;
    bool hasType;

    TypeConstraintFreezeNonEmpty(JSScript *script)
        : TypeConstraint("freezeNonEmpty"),
          script(script), hasType(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (hasType)
            return;

        hasType = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

bool
TypeSet::knownNonEmpty(JSContext *cx, JSScript *script)
{
    if (typeFlags != 0)
        return true;

    add(cx, ArenaNew<TypeConstraintFreezeNonEmpty>(script->analysis->pool, script), false);

    return false;
}

static bool
HasUnknownObject(TypeSet *types)
{
    if (types->objectCount >= 2) {
        unsigned objectCapacity = HashSetCapacity(types->objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = types->objectSet[i];
            if (object && object->unknownProperties)
                return true;
        }
    } else if (types->objectCount == 1 && ((TypeObject*)types->objectSet)->unknownProperties) {
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

void 
TypeCompartment::growPendingArray()
{
    pendingCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *oldArray = pendingArray;
    pendingArray = ArenaArray<PendingWork>(pool, pendingCapacity);
    memcpy(pendingArray, oldArray, pendingCount * sizeof(PendingWork));
}

void
TypeCompartment::addDynamicType(JSContext *cx, TypeSet *types, jstype type)
{
    JS_ASSERT_IF(type == TYPE_UNKNOWN, !types->unknown());
    JS_ASSERT_IF(type != TYPE_UNKNOWN, !types->hasType(type));

    interpreting = false;
    uint64_t startTime = currentTime();

    types->addType(cx, type);

    uint64_t endTime = currentTime();
    analysisTime += (endTime - startTime);
    interpreting = true;

    if (hasPendingRecompiles())
        processPendingRecompiles(cx);
}

void
TypeCompartment::addDynamicPush(JSContext *cx, analyze::Bytecode &code,
                                unsigned index, jstype type)
{
    js::types::TypeSet *types = code.pushed(index);
    JS_ASSERT(!types->hasType(type));

    InferSpew(ISpewDynamic, "MonitorResult: #%u:%05u %u: %s",
              code.script->id, code.offset, index, TypeString(type));

    interpreting = false;
    uint64_t startTime = currentTime();

    types->addType(cx, type);

    /*
     * For inc/dec ops, we need to go back and reanalyze the affected opcode
     * taking the overflow into account.  We won't see an explicit adjustment
     * of the type of the thing being inc/dec'ed, nor will adding the type to
     * the pushed value affect the type.
     */
    JSOp op = JSOp(code.script->getScript()->code[code.offset]);
    const JSCodeSpec *cs = &js_CodeSpec[op];
    if (cs->format & (JOF_INC | JOF_DEC)) {
        JS_ASSERT(!code.hasIncDecOverflow);
        code.hasIncDecOverflow = true;

        /* Inc/dec ops do not use the temporary analysis state. */
        analyze::Script::AnalyzeState state;
        code.script->analyzeTypes(cx, &code, state);
    }

    uint64_t endTime = currentTime();
    analysisTime += (endTime - startTime);
    interpreting = true;

    if (hasPendingRecompiles())
        processPendingRecompiles(cx);
}

void
TypeCompartment::processPendingRecompiles(JSContext *cx)
{
    /*
     * :FIXME: The case where recompilation fails needs to be handled (along with OOM
     * checks pretty much everywhere else in this file).  This function should return
     * false, and the caller must return and throw a JS exception *before* performing
     * the side effect which triggered the recompilation.
     */
    JS_ASSERT(pendingRecompiles);
    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
#ifdef JS_METHODJIT
        JSScript *script = (*pendingRecompiles)[i];
        mjit::Recompiler recompiler(cx, script);
        if (!recompiler.recompile())
            JS_NOT_REACHED("Recompilation failed!");
#endif
    }
    delete pendingRecompiles;
    pendingRecompiles = NULL;
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
    if (!script->jitNormal && !script->jitCtor) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }

    if (!pendingRecompiles)
        pendingRecompiles = new Vector<JSScript*>(ContextAllocPolicy(cx));

    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
        if (script == (*pendingRecompiles)[i])
            return;
    }

    recompilations++;
    pendingRecompiles->append(script);
}

void
TypeCompartment::dynamicAssign(JSContext *cx, JSObject *obj, jsid id, const Value &rval)
{
    if (obj->isWith())
        obj = js_UnwrapWithObject(cx, obj);

    jstype rvtype = GetValueType(cx, rval);
    TypeObject *object = obj->getType();

    TypeSet *assignTypes;

    /*
     * :XXX: Does this need to be moved to a more general place? We aren't considering
     * call objects in, e.g. addTypeProperty, but call objects might not be able to
     * flow there as they do not escape to scripts.
     */
    if (obj->isCall() || obj->isBlock()) {
        /* Local variable, let variable or argument assignment. */
        while (!obj->isCall())
            obj = obj->getParent();
        analyze::Script *script = obj->getCallObjCalleeFunction()->script()->analysis;
        JS_ASSERT(!script->isEval());

        assignTypes = script->getVariable(cx, id);
    } else if (object->unknownProperties) {
        return;
    } else {
        id = MakeTypeId(cx, id);

        /*
         * Mark as unknown any object which has had dynamic assignments to __proto__,
         * and any object which has had dynamic assignments to string properties through SETELEM.
         * The latter avoids making large numbers of type properties for hashmap-style objects.
         * :FIXME: this is too aggressive for things like prototype library initialization.
         */
        JSOp op = JSOp(*cx->regs->pc);
        if (id == id___proto__(cx) || (op == JSOP_SETELEM && !JSID_IS_VOID(id))) {
            cx->markTypeObjectUnknownProperties(object);
            if (hasPendingRecompiles())
                processPendingRecompiles(cx);
            return;
        }

        assignTypes = object->getProperty(cx, id, true);
    }

    if (assignTypes->hasType(rvtype))
        return;

    InferSpew(ISpewDynamic, "MonitorAssign: %s %s: %s",
              object->name(), TypeIdString(id), TypeString(rvtype));
    addDynamicType(cx, assignTypes, rvtype);
}

void
TypeCompartment::monitorBytecode(JSContext *cx, analyze::Bytecode *code)
{
    if (code->monitorNeeded)
        return;

    /*
     * Make sure monitoring is limited to property sets and calls where the
     * target of the set/call could be statically unknown, and mark the bytecode
     * results as unknown.
     */
    JSOp op = JSOp(code->script->getScript()->code[code->offset]);
    switch (op) {
      case JSOP_SETNAME:
      case JSOP_SETGNAME:
      case JSOP_SETXMLNAME:
      case JSOP_SETELEM:
      case JSOP_SETPROP:
      case JSOP_SETMETHOD:
      case JSOP_INITPROP:
      case JSOP_INITMETHOD:
      case JSOP_FORPROP:
      case JSOP_FORNAME:
      case JSOP_ENUMELEM:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
        break;
      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC:
      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC:
      case JSOP_INCELEM:
      case JSOP_DECELEM:
      case JSOP_ELEMINC:
      case JSOP_ELEMDEC:
      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_PROPINC:
      case JSOP_PROPDEC:
      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW:
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;
      default:
        TypeFailure(cx, "Monitoring unknown bytecode: %s", js_CodeNameTwo[op]);
    }

    InferSpew(ISpewOps, "addMonitorNeeded: #%u:%05u", code->script->id, code->offset);

    code->monitorNeeded = true;

    JSScript *script = code->script->getScript();
    if (script->hasJITCode())
        cx->compartment->types.addPendingRecompile(cx, script);
}

void
TypeCompartment::finish(JSContext *cx, JSCompartment *compartment)
{
    JS_ASSERT(this == &compartment->types);

    if (!InferSpewActive(ISpewResult) || JS_CLIST_IS_EMPTY(&compartment->scripts))
        return;

    for (JSScript *script = (JSScript *)compartment->scripts.next;
         &script->links != &compartment->scripts;
         script = (JSScript *)script->links.next) {
        if (script->analysis)
            script->analysis->finish(cx);
    }

#ifdef DEBUG
    TypeObject *object = objects;
    while (object) {
        object->print(cx);
        object = object->next;
    }
#endif

    double millis = analysisTime / 1000.0;

    printf("Counts: ");
    for (unsigned count = 0; count < TYPE_COUNT_LIMIT; count++) {
        if (count)
            printf("/");
        printf("%u", typeCounts[count]);
    }
    printf(" (%u over)\n", typeCountOver);

    printf("Recompilations: %u\n", recompilations);
    printf("Time: %.2f ms\n", millis);
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

    /* Check if the classes are already the same. */
    if (one == two)
        return;

    /* There should not be any types or constraints added to the stack types. */
    JS_ASSERT(one->types.typeFlags == 0 && one->types.constraintList == NULL);
    JS_ASSERT(two->types.typeFlags == 0 && two->types.constraintList == NULL);

    /* Merge any inner portions of the stack for the two nodes. */
    if (one->innerStack)
        merge(cx, one->innerStack, two->innerStack);

    InferSpew(ISpewOps, "merge: T%u T%u", one->types.id(), two->types.id());

    /* one has now been merged into two, do the actual join. */
    PodZero(one);
    one->mergedGroup = two;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

void
TypeObject::storeToInstances(JSContext *cx, Property *base)
{
    TypeObject *object = instanceList;
    while (object) {
        Property *p =
            HashSetLookup<jsid,Property,Property>(object->propertySet, object->propertyCount, base->id);
        if (p)
            base->ownTypes.addSubset(cx, *pool, &p->types);
        if (object->instanceList)
            object->storeToInstances(cx, base);
        object = object->instanceNext;
    }
}

void
TypeObject::addProperty(JSContext *cx, jsid id, Property *&base)
{
    JS_ASSERT(!base);
    base = ArenaNew<Property>(*pool, pool, id);

    InferSpew(ISpewOps, "addProperty: %s %s T%u own T%u",
              name(), TypeIdString(id), base->types.id(), base->ownTypes.id());

    base->ownTypes.addSubset(cx, *pool, &base->types);

    if (unknownProperties) {
        /*
         * Immediately mark the variable as unknown. Ideally we won't be doing this
         * too often, but we don't assert !unknownProperties to avoid extra complexity
         * in other code accessing object properties.
         */
        base->ownTypes.addType(cx, TYPE_UNKNOWN);
    }

    /* Check all transitive instances for this property. */
    if (instanceList)
        storeToInstances(cx, base);

    /* Pull in this property from all prototypes up the chain. */
    JSObject *obj = proto;
    while (obj) {
        TypeObject *object = obj->getType();
        Property *p =
            HashSetLookup<jsid,Property,Property>(object->propertySet, object->propertyCount, id);
        if (p)
            p->ownTypes.addSubset(cx, *object->pool, &base->types);
        obj = obj->getProto();
    }
}

void
TypeObject::markUnknown(JSContext *cx)
{
    JS_ASSERT(!unknownProperties);
    unknownProperties = true;

    cx->markTypeArrayNotPacked(this, true, false);

    /*
     * Existing constraints may have already been added to this object, which we need
     * to do the right thing for.  We can't ensure that we will mark all unknown
     * objects before they have been accessed, as the __proto__ of a known object
     * could be dynamically set to an unknown object, and we can decide to ignore
     * properties of an object during analysis (i.e. hashmaps). Adding unknown for
     * any properties accessed already accounts for possible values read from them.
     */

    if (propertyCount >= 2) {
        unsigned capacity = HashSetCapacity(propertyCount);
        for (unsigned i = 0; i < capacity; i++) {
            Property *prop = propertySet[i];
            if (prop)
                prop->ownTypes.addType(cx, TYPE_UNKNOWN);
        }
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        prop->ownTypes.addType(cx, TYPE_UNKNOWN);
    }

    /* Mark existing instances as unknown. */

    TypeObject *instance = instanceList;
    while (instance) {
        if (!instance->unknownProperties)
            instance->markUnknown(cx);
        instance = instance->instanceNext;
    }
}

void
TypeObject::print(JSContext *cx)
{
    printf("%s : %s", name(), proto ? proto->getType()->name() : "(null)");

    if (unknownProperties)
        printf(" unknown");

    if (propertyCount == 0) {
        printf(" {}\n");
        return;
    }

    printf(" {");

    if (propertyCount >= 2) {
        unsigned capacity = HashSetCapacity(propertyCount);
        for (unsigned i = 0; i < capacity; i++) {
            Property *prop = propertySet[i];
            if (prop) {
                printf("\n    %s:", TypeIdString(prop->id));
                prop->ownTypes.print(cx);
            }
        }
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        printf("\n    %s:", TypeIdString(prop->id));
        prop->ownTypes.print(cx);
    }

    printf("\n}\n");
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
    if (analysis->failed())
        return;

    js::analyze::Bytecode &code = analysis->getCode(pc);
    JS_ASSERT(code.analyzed);

    int useCount = js::analyze::GetUseCount(this, code.offset);
    JSOp op = (JSOp) *pc;

    if (!useCount)
        return;

    js::types::TypeStack *stack = code.inStack->group();
    for (int i = 0; i < useCount; i++) {
        const js::Value &val = sp[-1 - i];
        js::types::TypeSet *types = &stack->types;
        bool ignore = val.isMagic() || stack->ignoreTypeTag || IgnorePopped(op, i);

        stack = stack->innerStack ? stack->innerStack->group() : NULL;

        if (ignore)
            continue;

        js::types::jstype type = js::types::GetValueType(cx, val);

        /*
         * If this is a type for an object with unknown properties, match any object
         * in the type set which also has unknown properties. This avoids failure
         * on objects whose prototype (and thus type) changes dynamically, which will
         * mark the old and new type objects as unknown.
         */
        if (js::types::TypeIsObject(type) && ((js::types::TypeObject*)type)->unknownProperties &&
            js::types::HasUnknownObject(types)) {
            continue;
        }

        if (!types->hasType(type)) {
            js::types::TypeFailure(cx, "Missing type at #%u:%05u popped %u: %s",
                                   analysis->id, code.offset, i, js::types::TypeString(type));
        }

        if (js::types::TypeIsObject(type)) {
            JS_ASSERT(val.isObject());
            JSObject *obj = &val.toObject();
            js::types::TypeObject *object = (js::types::TypeObject *) type;

            if (object->unknownProperties) {
                JS_ASSERT(!object->isDenseArray);
                continue;
            }

            /* Make sure information about the array status of this object is right. */
            JS_ASSERT_IF(object->isPackedArray, object->isDenseArray);
            if (object->isDenseArray) {
                if (!obj->isDenseArray() ||
                    (object->isPackedArray && !obj->isPackedDenseArray())) {
                    js::types::TypeFailure(cx, "Object not %s array at #%u:%05u popped %u: %s",
                        object->isPackedArray ? "packed" : "dense",
                        analysis->id, code.offset, i, object->name());
                }
            }
        }
    }
}

namespace js {
namespace analyze {

using namespace types;

void
Script::addVariable(JSContext *cx, jsid id, types::Variable *&var)
{
    JS_ASSERT(!var);
    var = ArenaNew<types::Variable>(pool, &pool, id);

    /* Augment with builtin types for the 'arguments' variable. */
    if (fun && id == id_arguments(cx)) {
        TypeSet *types = &var->types;
        if (script->compileAndGo)
            types->addType(cx, (jstype) getTypeNewObject(cx, JSProto_Object));
        else
            types->addType(cx, TYPE_UNKNOWN);
    }

    InferSpew(ISpewOps, "addVariable: #%lu %s T%u",
              this->id, TypeIdString(id), var->types.id());
}

inline Bytecode*
Script::parentCode()
{
    return parent ? &parent->analysis->getCode(parentpc) : NULL;
}

inline Script*
Script::evalParent()
{
    Script *script = this;
    while (script->parent && !script->fun)
        script = script->parent->analysis;
    return script;
}

void
Script::setFunction(JSContext *cx, JSFunction *fun)
{
    JS_ASSERT(!this->fun);
    this->fun = fun;

    /* Add the return type for the empty script, which we do not explicitly analyze. */
    if (script->isEmpty())
        function()->returnTypes.addType(cx, TYPE_UNDEFINED);

    /*
     * Construct the arguments and locals of this function, and mark them as
     * definitely declared for scope lookups.  Note that we don't do this for the
     * global script (don't need to, everything not in another scope is global),
     * nor for eval scripts --- if an eval declares a variable the declaration
     * will be merged with any declaration in the context the eval occurred in,
     * and definitions information will be cleared for any scripts that could use
     * the declared variable.
     */
    if (fun->hasLocalNames())
        localNames = fun->getLocalNameArray(cx, &pool);

    /* Make a local variable for the function. */
    if (fun->atom) {
        TypeSet *var = getVariable(cx, ATOM_TO_JSID(fun->atom));
        if (script->compileAndGo)
            var->addType(cx, (jstype) function());
        else
            var->addType(cx, TYPE_UNKNOWN);
    }
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

/*
 * Get the variable set which declares id, either the local variables of a script
 * or the properties of the global object.  NULL if the scope is ambiguous due
 * to a 'with' or non-compileAndGo code, SCOPE_GLOBAL if the scope is definitely global.
 */
static Script * const SCOPE_GLOBAL = (Script *) 0x1;
static Script *
SearchScope(JSContext *cx, Script *script, TypeStack *stack, jsid id)
{
    /* Search up until we find a local variable with the specified name. */
    while (true) {
        /* Search the stack for any 'with' objects or 'let' variables. */
        while (stack) {
            stack = stack->group();
            if (stack->boundWith) {
                /* Enclosed within a 'with', ambiguous scope. */
                return NULL;
            }
            if (stack->letVariable == id) {
                /* The variable is definitely bound by this scope. */
                return script->evalParent();
            }
            stack = stack->innerStack;
        }

        if (script->isEval()) {
            /* eval scripts have no local variables to consider (they may have 'let' vars). */
            JS_ASSERT(!script->variableCount);
            stack = script->parentCode()->inStack;
            script = script->parent->analysis;
            continue;
        }

        if (!script->parent)
            break;

        /* Function scripts have 'arguments' local variables. */
        if (id == id_arguments(cx) && script->fun) {
            TypeSet *types = script->getVariable(cx, id);
            if (script->getScript()->compileAndGo)
                types->addType(cx, (jstype) script->getTypeNewObject(cx, JSProto_Object));
            else
                types->addType(cx, TYPE_UNKNOWN);
            return script;
        }

        /* Function scripts with names have local variables of that name. */
        if (script->fun && id == ATOM_TO_JSID(script->fun->atom)) {
            TypeSet *types = script->getVariable(cx, id);
            types->addType(cx, (jstype) script->function());
            return script;
        }

        unsigned nargs = script->argCount();
        for (unsigned i = 0; i < nargs; i++) {
            if (id == script->getArgumentId(i))
                return script;
        }

        unsigned nfixed = script->getScript()->nfixed;
        for (unsigned i = 0; i < nfixed; i++) {
            if (id == script->getLocalId(i, NULL))
                return script;
        }

        stack = script->parentCode()->inStack;
        script = script->parent->analysis;
    }

    JS_ASSERT(script);
    return script->getScript()->compileAndGo ? SCOPE_GLOBAL : NULL;
}

/* Mark the specified variable as undefined in any scope it could refer to. */
void
TrashScope(JSContext *cx, Script *script, jsid id)
{
    while (true) {
        if (!script->isEval()) {
            TypeSet *types = script->getVariable(cx, id);
            types->addType(cx, TYPE_UNKNOWN);
        }
        if (!script->parent)
            break;
        script = script->parent->analysis;
    }
    if (script->getScript()->compileAndGo) {
        TypeSet *types = script->getGlobalType()->getProperty(cx, id, true);
        types->addType(cx, TYPE_UNKNOWN);
    }
}

static inline jsid
GetAtomId(JSContext *cx, Script *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script->getScript(), (jsbytecode*) pc, offset);
    return MakeTypeId(cx, ATOM_TO_JSID(script->getScript()->getAtom(index)));
}

static inline jsid
GetGlobalId(JSContext *cx, Script *script, const jsbytecode *pc)
{
    unsigned index = GET_SLOTNO(pc);
    return MakeTypeId(cx, ATOM_TO_JSID(script->getScript()->getGlobalAtom(index)));
}

static inline JSObject *
GetScriptObject(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, offset);
    return script->getObject(index);
}

static inline const Value &
GetScriptConst(JSContext *cx, JSScript *script, const jsbytecode *pc)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, 0);
    return script->getConst(index);
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
    if (!newScript->fun)
        *id = newScript->getLocalId(newScript->getScript()->nfixed + slot, newCode);
    else if (slot < newScript->argCount())
        *id = newScript->getArgumentId(slot);
    else if (slot == UpvarCookie::CALLEE_SLOT)
        *id = ATOM_TO_JSID(newScript->fun->atom);
    else
        *id = newScript->getLocalId(slot - newScript->argCount(), newCode);

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

void
Script::analyzeTypes(JSContext *cx, Bytecode *code, AnalyzeState &state)
{
    unsigned offset = code->offset;

    JS_ASSERT(code->analyzed);
    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

    InferSpew(ISpewOps, "analyze: #%u:%05u", id, offset);

    if (code->stackDepth > state.stackDepth && state.stack) {
#ifdef DEBUG
        /*
         * Check that we aren't destroying any useful information. This should only
         * occur around exception handling bytecode.
         */
        for (unsigned i = state.stackDepth; i < code->stackDepth; i++) {
            JS_ASSERT(!state.stack[i].isForEach);
            JS_ASSERT(!state.stack[i].hasDouble);
            JS_ASSERT(!state.stack[i].scope);
        }
#endif
        unsigned ndefs = code->stackDepth - state.stackDepth;
        memset(&state.stack[state.stackDepth], 0, ndefs * sizeof(AnalyzeStateStack));
    }
    state.stackDepth = code->stackDepth;

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_POP:
      case JSOP_NOP:
      case JSOP_TRACE:
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_IFEQ:
      case JSOP_IFEQX:
      case JSOP_IFNE:
      case JSOP_IFNEX:
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
      case JSOP_DEBUGGER:
      case JSOP_SETCALL:
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
      case JSOP_URSH:
        /* :TODO: Add heuristics for guessing URSH which can overflow. */
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
      case JSOP_REGEXP:
        if (script->compileAndGo)
            code->setFixed(cx, 0, (jstype) getTypeNewObject(cx, JSProto_RegExp));
        else
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      case JSOP_STOP:
        /* If a stop is reachable then the return type may be void. */
        if (fun)
            function()->returnTypes.addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        /* OR/AND push whichever operand determined the result. */
        code->popped(0)->addSubset(cx, pool, code->pushed(0));
        break;

      case JSOP_DUP:
        MergePushed(cx, pool, code, 0, code->popped(0));
        MergePushed(cx, pool, code, 1, code->popped(0));
        break;

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
        Script *scope;

        switch (op) {
          case JSOP_GETGLOBAL:
          case JSOP_CALLGLOBAL:
            id = GetGlobalId(cx, this, pc);
            scope = SCOPE_GLOBAL;
            break;
          default:
            /*
             * Search the scope for GNAME to mirror the interpreter's behavior,
             * and to workaround cases where GNAME is broken, :FIXME: bug 605200.
             */
            id = GetAtomId(cx, this, pc, 0);
            scope = SearchScope(cx, this, code->inStack, id);
            break;
        }

        if (scope == SCOPE_GLOBAL) {
            /*
             * This might be a lazily loaded property of the global object.
             * Resolve it now. Subtract this from the total analysis time.
             */
            uint64_t startTime = cx->compartment->types.currentTime();
            JSObject *obj;
            JSProperty *prop;
            js_LookupPropertyWithFlags(cx, getGlobal(), id,
                                       JSRESOLVE_QUALIFIED, &obj, &prop);
            uint64_t endTime = cx->compartment->types.currentTime();
            cx->compartment->types.analysisTime -= (endTime - startTime);

            /* Handle as a property access. */
            PropertyAccess(cx, code, getGlobalType(), false, code->pushed(0), id);
        } else if (scope) {
            /* Definitely a local variable. */
            TypeSet *types = scope->getVariable(cx, id);
            types->addSubset(cx, scope->pool, code->pushed(0));
        } else {
            /* Ambiguous access, unknown result. */
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        }

        if (op == JSOP_CALLGLOBAL || op == JSOP_CALLGNAME || op == JSOP_CALLNAME)
            code->setFixed(cx, 1, scope ? TYPE_UNDEFINED : TYPE_UNKNOWN);
        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
        /* Handled below. */
        break;

      case JSOP_SETGNAME:
      case JSOP_SETNAME: {
        jsid id = GetAtomId(cx, this, pc, 0);

        const AnalyzeStateStack &stack = state.popped(1);
        if (stack.scope == SCOPE_GLOBAL) {
            PropertyAccess(cx, code, getGlobalType(), true, code->popped(0), id);
        } else if (stack.scope) {
            TypeSet *types = stack.scope->getVariable(cx, id);
            code->popped(0)->addSubset(cx, pool, types);
        } else {
            cx->compartment->types.monitorBytecode(cx, code);
        }

        MergePushed(cx, pool, code, 0, code->popped(0));
        break;
      }

      case JSOP_GETXPROP: {
        jsid id = GetAtomId(cx, this, pc, 0);

        const AnalyzeStateStack &stack = state.popped(0);
        if (stack.scope == SCOPE_GLOBAL) {
            PropertyAccess(cx, code, getGlobalType(), false, code->pushed(0), id);
        } else if (stack.scope) {
            TypeSet *types = stack.scope->getVariable(cx, id);
            types->addSubset(cx, stack.scope->pool, code->pushed(0));
        } else {
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        }

        break;
      }

      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC:
      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC: {
        /* Same issue for searching scope as JSOP_GNAME/CALLGNAME. :FIXME: bug 605200 */
        jsid id = GetAtomId(cx, this, pc, 0);

        Script *scope = SearchScope(cx, this, code->inStack, id);
        if (scope == SCOPE_GLOBAL) {
            PropertyAccess(cx, code, getGlobalType(), true, NULL, id);
            PropertyAccess(cx, code, getGlobalType(), false, code->pushed(0), id);
        } else if (scope) {
            TypeSet *types = scope->getVariable(cx, id);
            types->addSubset(cx, scope->pool, code->pushed(0));
            types->addArith(cx, scope->pool, code, types);
            if (code->hasIncDecOverflow)
                types->addType(cx, TYPE_DOUBLE);
        } else {
            cx->compartment->types.monitorBytecode(cx, code);
        }

        break;
      }

      case JSOP_SETGLOBAL:
      case JSOP_SETCONST: {
        /*
         * Even though they are on the global object, GLOBAL accesses do not run into
         * the issues which require monitoring that other property accesses do:
         * __proto__ is still emitted as a SETGNAME even if there is a 'var __proto__',
         * and there will be no getter/setter in a prototype, and 'constructor',
         * 'prototype' and 'caller' do not have special meaning on the global object.
         */
        jsid id = (op == JSOP_SETGLOBAL) ? GetGlobalId(cx, this, pc) : GetAtomId(cx, this, pc, 0);
        TypeSet *types = getGlobalType()->getProperty(cx, id, true);
        code->popped(0)->addSubset(cx, pool, types);
        MergePushed(cx, pool, code, 0, code->popped(0));
        break;
      }

      case JSOP_INCGLOBAL:
      case JSOP_DECGLOBAL:
      case JSOP_GLOBALINC:
      case JSOP_GLOBALDEC: {
        jsid id = GetGlobalId(cx, this, pc);
        TypeSet *types = getGlobalType()->getProperty(cx, id, true);
        types->addArith(cx, cx->compartment->types.pool, code, types);
        MergePushed(cx, cx->compartment->types.pool, code, 0, types);
        if (code->hasIncDecOverflow)
            types->addType(cx, TYPE_DOUBLE);
        break;
      }

      case JSOP_GETUPVAR:
      case JSOP_CALLUPVAR:
      case JSOP_GETFCSLOT:
      case JSOP_CALLFCSLOT: {
        unsigned index = GET_UINT16(pc);

        jsid id = JSID_VOID;
        Script *newScript = GetUpvarVariable(cx, code, index, &id);
        TypeSet *types = newScript->getVariable(cx, id);

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
            TypeSet *types = getVariable(cx, id);

            MergePushed(cx, pool, code, 0, types);
            if (op == JSOP_SETARG)
                code->popped(0)->addSubset(cx, pool, types);
        } else {
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        }

        if (op == JSOP_CALLARG)
            code->setFixed(cx, 1, TYPE_UNDEFINED);
        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        TypeSet *types = getVariable(cx, id);

        types->addArith(cx, pool, code, types);
        MergePushed(cx, pool, code, 0, types);
        if (code->hasIncDecOverflow)
            types->addType(cx, TYPE_DOUBLE);
        break;
      }

      case JSOP_ARGSUB:
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      case JSOP_GETLOCAL:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP:
      case JSOP_CALLLOCAL: {
        uint32 local = GET_SLOTNO(pc);
        jsid id = getLocalId(local, code);

        TypeSet *types;
        JSArenaPool *pool;
        if (!JSID_IS_VOID(id)) {
            types = evalParent()->getVariable(cx, id);
            pool = &evalParent()->pool;
        } else {
            types = getStackTypes(GET_SLOTNO(pc), code);
            pool = &this->pool;
        }

        if (op != JSOP_SETLOCALPOP) {
            MergePushed(cx, *pool, code, 0, types);
            if (op == JSOP_CALLLOCAL)
                code->setFixed(cx, 1, TYPE_UNDEFINED);
        }

        if (op == JSOP_SETLOCAL || op == JSOP_SETLOCALPOP) {
            state.clearLocal(local);
            if (state.popped(0).isConstant)
                state.addConstLocal(local, state.popped(0).isZero);

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
        uint32 local = GET_SLOTNO(pc);
        jsid id = getLocalId(local, code);

        state.clearLocal(local);

        TypeSet *types = evalParent()->getVariable(cx, id);
        types->addArith(cx, evalParent()->pool, code, types);
        MergePushed(cx, evalParent()->pool, code, 0, types);

        if (code->hasIncDecOverflow)
            types->addType(cx, TYPE_DOUBLE);
        break;
      }

      case JSOP_ARGUMENTS: {
        TypeSet *types = getVariable(cx, id_arguments(cx));
        MergePushed(cx, pool, code, 0, types);
        break;
      }

      case JSOP_ARGCNT:
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

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
        thisTypes.addTransformThis(cx, code, newTypes);
        newTypes->addGetProperty(cx, code, code->pushed(0), id);

        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_GETARGPROP: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        TypeSet *types = getVariable(cx, id);

        jsid propid = GetAtomId(cx, this, pc, SLOTNO_LEN);
        types->addGetProperty(cx, code, code->pushed(0), propid);

        CheckNextTest(cx, code, pc);
        break;
      }

      case JSOP_GETLOCALPROP: {
        jsid id = getLocalId(GET_SLOTNO(pc), code);
        TypeSet *types = evalParent()->getVariable(cx, id);

        if (this != evalParent()) {
            TypeSet *newTypes = TypeSet::make(cx, pool, "localprop");
            types->addSubset(cx, evalParent()->pool, newTypes);
            types = newTypes;
        }

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
        if (state.popped(1).isZero) {
            /*
             * Initializing the array with what looks like it could be zero.
             * This is sensitive to the order in which bytecodes are emitted
             * for common loop forms: '(for i = 0;; i++) a[i] = ...' and
             * 'i = 0; while () { a[i] = ...; i++ }. In the bytecode the increment
             * will appear after the initialization, and we are looking for arrays
             * initialized between the two statements.
             */
            code->popped(2)->add(cx, ArenaNew<TypeConstraintPossiblyPacked>(pool));
        }

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
        thisTypes.addTransformThis(cx, code, code->pushed(0));
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
        if (fun)
            code->popped(0)->addSubset(cx, pool, &function()->returnTypes);
        break;

      case JSOP_ADD:
        code->popped(0)->addArith(cx, pool, code, code->pushed(0), code->popped(1));
        code->popped(1)->addArith(cx, pool, code, code->pushed(0), code->popped(0));
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_DIV:
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
        JSObject *obj = GetScriptObject(cx, script, pc, off);
        TypeFunction *function = obj->getType()->asFunction();

        /* Remember where this script was defined. */
        function->script->analysis->parent = script;
        function->script->analysis->parentpc = pc;

        TypeSet *res = NULL;

        if (op == JSOP_LAMBDA || op == JSOP_LAMBDA_FC) {
            res = code->pushed(0);
        } else if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) {
            jsid id = getLocalId(GET_SLOTNO(pc), code);
            res = evalParent()->getVariable(cx, id);
        } else {
            JSAtom *atom = obj->getFunctionPrivate()->atom;
            JS_ASSERT(atom);
            jsid id = ATOM_TO_JSID(atom);
            if (isGlobal() && script->compileAndGo) {
                /* Defined function at global scope. */
                res = getGlobalType()->getProperty(cx, id, true);
            } else {
                /* Defined function in a function eval() or ambiguous function scope. */
                TrashScope(cx, this, id);
                break;
            }
        }

        if (res) {
            if (script->compileAndGo)
                res->addType(cx, (jstype) function);
            else
                res->addType(cx, TYPE_UNKNOWN);
        } else {
            cx->compartment->types.monitorBytecode(cx, code);
        }
        break;
      }

      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
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

      case JSOP_NEWINIT:
      case JSOP_NEWARRAY:
      case JSOP_NEWOBJECT:
        if (script->compileAndGo) {
            TypeObject *object;
            if (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && pc[1] == JSProto_Array)) {
                object = code->initArray;
                jsbytecode *next = pc + GetBytecodeLength(pc);
                if (JSOp(*next) != JSOP_ENDINIT)
                    object->possiblePackedArray = true;
            } else {
                object = code->initObject;
            }
            code->pushed(0)->addType(cx, (jstype) object);
        } else {
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        }
        break;

      case JSOP_ENDINIT:
        break;

      case JSOP_INITELEM:
        if (script->compileAndGo) {
            TypeObject *object = code->initObject;
            JS_ASSERT(object);

            code->pushed(0)->addType(cx, (jstype) object);

            TypeSet *types;
            if (state.popped(1).hasDouble) {
                Value val = DoubleValue(state.popped(1).doubleValue);
                jsid id;
                if (!js_InternNonIntElementId(cx, NULL, val, &id))
                    JS_NOT_REACHED("Bad");
                types = object->getProperty(cx, id, true);
            } else {
                types = object->getProperty(cx, JSID_VOID, true);
            }

            if (state.hasGetSet)
                types->addType(cx, (jstype) cx->getTypeGetSet());
            else if (state.hasHole)
                cx->markTypeArrayNotPacked(object, false);
            else
                code->popped(0)->addSubset(cx, pool, types);
        } else {
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        }
        state.hasGetSet = false;
        state.hasHole = false;
        break;

      case JSOP_GETTER:
      case JSOP_SETTER:
        state.hasGetSet = true;
        break;

      case JSOP_HOLE:
        state.hasHole = true;
        break;

      case JSOP_INITPROP:
      case JSOP_INITMETHOD:
        if (script->compileAndGo) {
            TypeObject *object = code->initObject;
            JS_ASSERT(object);

            code->pushed(0)->addType(cx, (jstype) object);

            jsid id = GetAtomId(cx, this, pc, 0);
            TypeSet *types = object->getProperty(cx, id, true);

            if (id == id___proto__(cx) || id == id_prototype(cx))
                cx->compartment->types.monitorBytecode(cx, code);
            else if (state.hasGetSet)
                types->addType(cx, (jstype) cx->getTypeGetSet());
            else
                code->popped(0)->addSubset(cx, pool, types);
        } else {
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        }
        state.hasGetSet = false;
        JS_ASSERT(!state.hasHole);
        break;

      case JSOP_ENTERWITH:
        /*
         * Scope lookups can occur on the value being pushed here.  We don't track
         * the value or its properties, and just monitor all name opcodes contained
         * by the with.
         */
        code->pushedArray[0].group()->boundWith = true;
        break;

      case JSOP_ENTERBLOCK: {
        JSObject *obj = GetScriptObject(cx, script, pc, 0);
        unsigned defCount = GetDefCount(script, offset);

        const Shape *shape = obj->lastProperty();
        for (unsigned i = defCount - 1; i < defCount; i--) {
            code->pushedArray[i].group()->letVariable = shape->id;
            shape = shape->previous();
        }
        break;
      }

      case JSOP_ITER:
        /*
         * The actual pushed value is an iterator object, which we don't care about.
         * Propagate the target of the iteration itself so that we'll be able to detect
         * when an object of Iterator class flows to the JSOP_FOR* opcode, which could
         * be a generator that produces arbitrary values with 'for in' syntax.
         */
        MergePushed(cx, pool, code, 0, code->popped(0));
        code->pushedArray[0].group()->ignoreTypeTag = true;
        break;

      case JSOP_MOREITER:
        code->pushedArray[0].group()->ignoreTypeTag = true;
        MergePushed(cx, pool, code, 0, code->popped(0));
        code->setFixed(cx, 1, TYPE_BOOLEAN);
        break;

      case JSOP_FORNAME: {
        jsid id = GetAtomId(cx, this, pc, 0);
        Script *scope = SearchScope(cx, this, code->inStack, id);

        if (scope == SCOPE_GLOBAL)
            SetForTypes(cx, state, code, getGlobalType()->getProperty(cx, id, true));
        else if (scope)
            SetForTypes(cx, state, code, scope->getVariable(cx, id));
        else
            cx->compartment->types.monitorBytecode(cx, code);
        break;
      }

      case JSOP_FORGLOBAL: {
        jsid id = GetGlobalId(cx, this, pc);
        SetForTypes(cx, state, code, getGlobalType()->getProperty(cx, id, true));
        break;
      }

      case JSOP_FORLOCAL: {
        jsid id = getLocalId(GET_SLOTNO(pc), code);
        JS_ASSERT(!JSID_IS_VOID(id));

        SetForTypes(cx, state, code, evalParent()->getVariable(cx, id)); 
       break;
      }

      case JSOP_FORARG: {
        jsid id = getArgumentId(GET_ARGNO(pc));
        JS_ASSERT(!JSID_IS_VOID(id));

        SetForTypes(cx, state, code, getVariable(cx, id));
        break;
      }

      case JSOP_FORELEM:
        MergePushed(cx, pool, code, 0, code->popped(0));
        code->setFixed(cx, 1, TYPE_UNKNOWN);
        break;

      case JSOP_FORPROP:
      case JSOP_ENUMELEM:
        cx->compartment->types.monitorBytecode(cx, code);
        break;

      case JSOP_ARRAYPUSH: {
        TypeSet *types = getStackTypes(GET_SLOTNO(pc), code);
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
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      case JSOP_DEFVAR:
        /*
         * Trash known types going up the scope chain when a variable has
         * ambiguous scope or a non-global eval defines a variable.
         */
        if (!isGlobal()) {
            jsid id = GetAtomId(cx, this, pc, 0);
            TrashScope(cx, this, id);
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
        if (fun) {
            if (script->compileAndGo) {
                TypeObject *object = getTypeNewObject(cx, JSProto_Generator);
                function()->returnTypes.addType(cx, (jstype) object);
            } else {
                function()->returnTypes.addType(cx, TYPE_UNKNOWN);
            }
        }
        break;

      case JSOP_YIELD:
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      case JSOP_CALLXMLNAME:
        code->setFixed(cx, 1, TYPE_UNKNOWN);
        /* FALLTHROUGH */
      case JSOP_XMLNAME:
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      case JSOP_SETXMLNAME:
        cx->compartment->types.monitorBytecode(cx, code);
        MergePushed(cx, pool, code, 0, code->popped(0));
        break;

      case JSOP_BINDXMLNAME:
        break;

      case JSOP_TOXML:
      case JSOP_TOXMLLIST:
      case JSOP_XMLPI:
      case JSOP_XMLCDATA:
      case JSOP_XMLCOMMENT:
      case JSOP_DESCENDANTS:
      case JSOP_TOATTRNAME:
      case JSOP_QNAMECONST:
      case JSOP_QNAME:
      case JSOP_ANYNAME:
      case JSOP_GETFUNNS:
        code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      case JSOP_FILTER:
        /* Note: the second value pushed by filter is a hole, and not modelled. */
        MergePushed(cx, pool, code, 0, code->popped(0));
        code->pushedArray[0].group()->boundWith = true;
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

        TypeSet *types = evalParent()->getVariable(cx, id);
        value->addSubset(cx, pool, types);
        break;
      }

      case JSOP_USESHARP: {
        char name[24];
        JS_snprintf(name, sizeof(name), "#%d:%d",
                    GET_UINT16(pc), GET_UINT16(pc + UINT16_LEN));
        JSAtom *atom = js_Atomize(cx, name, strlen(name), ATOM_PINNED);
        jsid id = ATOM_TO_JSID(atom);

        TypeSet *types = evalParent()->getVariable(cx, id);
        MergePushed(cx, evalParent()->pool, code, 0, types);
        break;
      }

      case JSOP_CALLEE:
        if (script->compileAndGo)
            code->setFixed(cx, 0, (jstype) function());
        else
            code->setFixed(cx, 0, TYPE_UNKNOWN);
        break;

      default:
        TypeFailure(cx, "Unknown bytecode: %s", js_CodeNameTwo[op]);
    }

    /* Compute temporary analysis state after the bytecode. */

    if (!state.stack)
        return;

    if (op == JSOP_DUP) {
        state.stack[code->stackDepth] = state.stack[code->stackDepth - 1];
        state.stackDepth = code->stackDepth + 1;
    } else if (op == JSOP_DUP2) {
        state.stack[code->stackDepth]     = state.stack[code->stackDepth - 2];
        state.stack[code->stackDepth + 1] = state.stack[code->stackDepth - 1];
        state.stackDepth = code->stackDepth + 2;
    } else {
        unsigned nuses = GetUseCount(script, offset);
        unsigned ndefs = GetDefCount(script, offset);
        memset(&state.stack[code->stackDepth - nuses], 0, ndefs * sizeof(AnalyzeStateStack));
        state.stackDepth = code->stackDepth - nuses + ndefs;
    }

    switch (op) {
      case JSOP_BINDGNAME:
      case JSOP_BINDNAME: {
        /* Same issue for searching scope as JSOP_GNAME/JSOP_CALLGNAME. :FIXME: bug 605200 */
        jsid id = GetAtomId(cx, this, pc, 0);
        AnalyzeStateStack &stack = state.popped(0);
        stack.scope = SearchScope(cx, this, code->inStack, id);
        break;
      }

      case JSOP_ITER: {
        uintN flags = pc[1];
        if (flags & JSITER_FOREACH)
            state.popped(0).isForEach = true;
        break;
      }

      case JSOP_DOUBLE: {
        AnalyzeStateStack &stack = state.popped(0);
        stack.hasDouble = true;
        stack.doubleValue = GetScriptConst(cx, script, pc).toDouble();
        break;
      }

      case JSOP_ZERO:
        state.popped(0).isZero = true;
        /* FALLTHROUGH */
      case JSOP_ONE:
      case JSOP_INT8:
      case JSOP_INT32:
      case JSOP_UINT16:
      case JSOP_UINT24:
        state.popped(0).isConstant = true;
        break;

      case JSOP_GETLOCAL:
        if (state.maybeLocalConst(GET_SLOTNO(pc), false)) {
            state.popped(0).isConstant = true;
            if (state.maybeLocalConst(GET_SLOTNO(pc), true))
                state.popped(0).isZero = true;
        }
        break;

      default:;
    }
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG

void
Bytecode::print(JSContext *cx)
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
        printf("%s", name);
        break;

      case JOF_JUMP:
      case JOF_JUMPX: {
        ptrdiff_t off = GetJumpOffset(pc, pc);
        printf("%s %u", name, unsigned(offset + off));
        break;
      }

      case JOF_ATOM: {
        if (op == JSOP_DOUBLE) {
            printf("%s", name);
        } else {
            jsid id = GetAtomId(cx, script, pc, 0);
            if (JSID_IS_STRING(id))
                printf("%s %s", name, TypeIdString(id));
            else
                printf("%s (index)", name);
        }
        break;
      }

      case JOF_OBJECT:
        printf("%s (object)", name);
        break;

      case JOF_REGEXP:
        printf("%s (regexp)", name);
        break;

      case JOF_UINT16PAIR:
        printf("%s %d %d", name, GET_UINT16(pc), GET_UINT16(pc + UINT16_LEN));
        break;

      case JOF_UINT16:
        printf("%s %d", name, GET_UINT16(pc));
        break;

      case JOF_QARG: {
        jsid id = script->getArgumentId(GET_ARGNO(pc));
        printf("%s %s", name, TypeIdString(id));
        break;
      }

      case JOF_GLOBAL:
        printf("%s %s", name, TypeIdString(GetGlobalId(cx, script, pc)));
        break;

      case JOF_LOCAL:
        if ((op != JSOP_ARRAYPUSH) && (analyzed || (GET_SLOTNO(pc) < script->getScript()->nfixed))) {
            jsid id = script->getLocalId(GET_SLOTNO(pc), this);
            printf("%s %d %s", name, GET_SLOTNO(pc), TypeIdString(id));
        } else {
            printf("%s %d", name, GET_SLOTNO(pc));
        }
        break;

      case JOF_SLOTATOM: {
        jsid id = GetAtomId(cx, script, pc, SLOTNO_LEN);

        jsid slotid = JSID_VOID;
        if (op == JSOP_GETARGPROP)
            slotid = script->getArgumentId(GET_ARGNO(pc));
        if (op == JSOP_GETLOCALPROP && (analyzed || (GET_SLOTNO(pc) < script->getScript()->nfixed)))
            slotid = script->getLocalId(GET_SLOTNO(pc), this);

        printf("%s %u %s %s", name, GET_SLOTNO(pc), TypeIdString(slotid), TypeIdString(id));
        break;
      }

      case JOF_SLOTOBJECT:
        printf("%s %u (object)", name, GET_SLOTNO(pc));
        break;

      case JOF_UINT24:
        JS_ASSERT(op == JSOP_UINT24 || op == JSOP_NEWARRAY);
        printf("%s %d", name, (jsint)GET_UINT24(pc));
        break;

      case JOF_UINT8:
        printf("%s %d", name, (jsint)pc[1]);
        break;

      case JOF_INT8:
        printf("%s %d", name, (jsint)GET_INT8(pc));
        break;

      case JOF_INT32:
        printf("%s %d", name, (jsint)GET_INT32(pc));
        break;

      default:
        JS_NOT_REACHED("Unknown opcode type");
    }
}

#endif

void
Script::finish(JSContext *cx)
{
    if (failed() || !codeArray)
        return;

    TypeCompartment *compartment = &script->compartment->types;

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

#ifdef DEBUG

    if (parent) {
        if (fun)
            printf("Function");
        else
            printf("Eval");

        printf(" #%u @%u\n", id, parent->analysis->id);
    } else {
        printf("Main #%u:\n", id);
    }

    if (!codeArray) {
        printf("(unused)\n");
        return;
    }

    /* Print out points where variables became unconditionally defined. */
    printf("defines:");
    for (unsigned i = 0; i < localCount(); i++) {
        if (locals[i] != LOCAL_USE_BEFORE_DEF && locals[i] != LOCAL_CONDITIONALLY_DEFINED)
            printf(" %s@%u", TypeIdString(getLocalId(i, NULL)), locals[i]);
    }
    printf("\n");

    printf("locals:\n");
    printf("    this:");
    thisTypes.print(cx);

    if (variableCount >= 2) {
        unsigned capacity = HashSetCapacity(variableCount);
        for (unsigned i = 0; i < capacity; i++) {
            Variable *var = variableSet[i];
            if (var) {
                printf("\n    %s:", TypeIdString(var->id));
                var->types.print(cx);
            }
        }
    } else if (variableCount == 1) {
        Variable *var = (Variable *) variableSet;
        printf("\n    %s:", TypeIdString(var->id));
        var->types.print(cx);
    }
    printf("\n");

    int id_count = 0;

    for (unsigned offset = 0; offset < script->length; offset++) {
        Bytecode *code = codeArray[offset];
        if (!code)
            continue;

        printf("#%u:%05u:  ", id, offset);
        code->print(cx);
        printf("\n");

        if (code->defineCount) {
            printf("  defines:");
            for (unsigned i = 0; i < code->defineCount; i++) {
                uint32 local = code->defineArray[i];
                printf(" %s", TypeIdString(getLocalId(local, NULL)));
            }
            printf("\n");
        }

        TypeStack *stack;
        unsigned useCount = GetUseCount(script, offset);
        if (useCount) {
            printf("  use:");
            stack = code->inStack->group();
            for (unsigned i = 0; i < useCount; i++) {
                if (!stack->id)
                    stack->id = ++id_count;
                printf(" %d", stack->id);
                stack = stack->innerStack ? stack->innerStack->group() : NULL;
            }
            printf("\n");

            /* Watch for stack values without any types. */
            stack = code->inStack->group();
            for (unsigned i = 0; i < useCount; i++) {
                if (!IgnorePopped((JSOp)script->code[offset], i)) {
                    if (stack->types.typeFlags == 0)
                        printf("  missing stack: %d\n", stack->id);
                }
                stack = stack->innerStack ? stack->innerStack->group() : NULL;
            }
        }

        unsigned defCount = GetDefCount(script, offset);
        if (defCount) {
            printf("  def:");
            for (unsigned i = 0; i < defCount; i++) {
                stack = code->pushedArray[i].group();
                if (!stack->id)
                    stack->id = ++id_count;
                printf(" %d", stack->id);
            }
            printf("\n");
            for (unsigned i = 0; i < defCount; i++) {
                stack = code->pushedArray[i].group();
                printf("  type %d:", stack->id);
                stack->types.print(cx);
                printf("\n");
            }
        }

        if (code->monitorNeeded)
            printf("  monitored\n");
    }

    printf("\n");

    TypeObject *object = objects;
    while (object) {
        object->print(cx);
        object = object->next;
    }

#endif /* DEBUG */

}

} } /* namespace js::analyze */
