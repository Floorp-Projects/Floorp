/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef js2runtime_h___
#define js2runtime_h___

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#endif

#include <vector>
#include <stack>
#include <map>

#include "systemtypes.h"
#include "strings.h"
#include "formatter.h"
#include "property.h"

#include "tracer.h"
#include "collector.h"
#include "regexp.h"

namespace JavaScript {
namespace JS2Runtime {

    class ByteCodeGen;
    class ByteCodeModule;

#ifdef IS_LITTLE_ENDIAN
#define JSDOUBLE_HI32(x)        (((uint32 *)&(x))[1])
#define JSDOUBLE_LO32(x)        (((uint32 *)&(x))[0])
#else
#define JSDOUBLE_HI32(x)        (((uint32 *)&(x))[0])
#define JSDOUBLE_LO32(x)        (((uint32 *)&(x))[1])
#endif

#define JSDOUBLE_HI32_SIGNBIT   0x80000000
#define JSDOUBLE_HI32_EXPMASK   0x7ff00000
#define JSDOUBLE_HI32_MANTMASK  0x000fffff

#define JSDOUBLE_IS_NaN(x)                                                    \
    ((JSDOUBLE_HI32(x) & JSDOUBLE_HI32_EXPMASK) == JSDOUBLE_HI32_EXPMASK &&   \
     (JSDOUBLE_LO32(x) || (JSDOUBLE_HI32(x) & JSDOUBLE_HI32_MANTMASK)))

#define JSDOUBLE_IS_INFINITE(x)                                               \
    ((JSDOUBLE_HI32(x) & ~JSDOUBLE_HI32_SIGNBIT) == JSDOUBLE_HI32_EXPMASK &&   \
     !JSDOUBLE_LO32(x))

#define JSDOUBLE_IS_FINITE(x)                                                 \
    ((JSDOUBLE_HI32(x) & JSDOUBLE_HI32_EXPMASK) != JSDOUBLE_HI32_EXPMASK)

#define JSDOUBLE_IS_POSZERO(d)  (JSDOUBLE_HI32(d) == 0 && JSDOUBLE_LO32(d) == 0)

#define JSDOUBLE_IS_NEGZERO(d)  (JSDOUBLE_HI32(d) == JSDOUBLE_HI32_SIGNBIT && \
                                 JSDOUBLE_LO32(d) == 0)


static const double two32minus1 = 4294967295.0;
static const double two32 = 4294967296.0;
static const double two16 = 65536.0;
static const double two31 = 2147483648.0;

#define NotABanana ((uint32)(-1))

    class JSObject;
    class JSFunction;
    class JSType;
    class JSArrayType;
    class JSStringType;
    class Context;
    class NamedArgument;
    class JSInstance;
    class Attribute;
    class Package;


    extern JSType *Object_Type;         // the base type for all types
    extern JSType *Number_Type;
    extern JSType *Integer_Type;
    extern JSStringType *String_Type;
    extern JSType *Type_Type;           // the type for variables that are types 
                                        // (e.g. the property 'C' from class C...
                                        // has this type).
    extern JSType *Boolean_Type;
    extern JSType *Void_Type;
    extern JSType *Null_Type;
    extern JSArrayType *Array_Type;
    extern JSType *Unit_Type;
    extern JSType *Function_Type;

    extern JSType *Attribute_Type;      // used to define 'prototype' 'static' etc & Namespace values
    extern JSType *Package_Type;

    extern JSType *Date_Type;
    extern JSType *RegExp_Type;


    const String *numberToString(float64 number);
    float64 stringToNumber(const String *string);


#define JS2_BIT(n)       ((uint32)1 << (n))
#define JS2_BITMASK(n)   (JS2_BIT(n) - 1)
/*
 * Type tags stored in the low bits of a js2val.
 */
#define JS2VAL_OBJECT            0x0     /* untagged reference to object */
#define JS2VAL_INT               0x1     /* tagged 31-bit integer value */
#define JS2VAL_DOUBLE            0x2     /* tagged reference to double */
#define JS2VAL_STRING            0x4     /* tagged reference to string */
#define JS2VAL_BOOLEAN           0x6     /* tagged boolean value */

/* Type tag bitfield length and derived macros. */
#define JS2VAL_TAGBITS           3
#define JS2VAL_TAGMASK           JS2_BITMASK(JS2VAL_TAGBITS)
#define JS2VAL_TAG(v)            ((v) & JS2VAL_TAGMASK)
#define JS2VAL_SETTAG(v,t)       ((v) | (t))
#define JS2VAL_CLRTAG(v)         ((v) & ~(js2val)JS2VAL_TAGMASK)
#define JS2VAL_ALIGN             JS2_BIT(JS2VAL_TAGBITS)

#define JS2VAL_INT_POW2(n)       ((js2val)1 << (n))
#define INT_TO_JS2VAL(i)         (((js2val)(i) << 1) | JS2VAL_INT)
#define JS2VAL_VOID              INT_TO_JS2VAL(0 - JS2VAL_INT_POW2(30))
#define JS2VAL_NULL              OBJECT_TO_JS2VAL(0)
#define JS2VAL_FALSE             BOOLEAN_TO_JS2VAL(false)
#define JS2VAL_TRUE              BOOLEAN_TO_JS2VAL(true)

/* Predicates for type testing. */
#define JS2VAL_IS_OBJECT(v)      (JS2VAL_TAG(v) == JS2VAL_OBJECT)
#define JS2VAL_IS_NUMBER(v)      (JS2VAL_IS_INT(v) || JS2VAL_IS_DOUBLE(v))
#define JS2VAL_IS_INT(v)         (((v) & JS2VAL_INT) && (v) != JS2VAL_VOID)
#define JS2VAL_IS_DOUBLE(v)      (JS2VAL_TAG(v) == JS2VAL_DOUBLE)
#define JS2VAL_IS_STRING(v)      (JS2VAL_TAG(v) == JS2VAL_STRING)
#define JS2VAL_IS_BOOLEAN(v)     (JS2VAL_TAG(v) == JS2VAL_BOOLEAN)
#define JS2VAL_IS_NULL(v)        ((v) == JS2VAL_NULL)
#define JS2VAL_IS_VOID(v)        ((v) == JS2VAL_VOID)
#define JS2VAL_IS_PRIMITIVE(v)   (!JS2VAL_IS_OBJECT(v) || JS2VAL_IS_NULL(v))

/* Objects, strings, and doubles are GC'ed. */
#define JS2VAL_IS_GCTHING(v)     (!((v) & JS2VAL_INT) && !JS2VAL_IS_BOOLEAN(v))
#define JS2VAL_TO_GCTHING(v)     ((void *)JS2VAL_CLRTAG(v))
#define JS2VAL_TO_OBJECT(v)      ((JSObject *)JS2VAL_TO_GCTHING(v))
#define JS2VAL_TO_DOUBLE(v)      ((float64 *)JS2VAL_TO_GCTHING(v))
#define JS2VAL_TO_STRING(v)      ((String *)JS2VAL_TO_GCTHING(v))
#define OBJECT_TO_JS2VAL(obj)    ((js2val)(obj))
#define DOUBLE_TO_JS2VAL(dp)     JS2VAL_SETTAG((js2val)(dp), JS2VAL_DOUBLE)
#define STRING_TO_JS2VAL(str)    JS2VAL_SETTAG((js2val)(str), JS2VAL_STRING)

/* Convert between boolean and jsval. */
#define JS2VAL_TO_BOOLEAN(v)     (((v) >> JS2VAL_TAGBITS) != 0)
#define BOOLEAN_TO_JS2VAL(b)     JS2VAL_SETTAG((js2val)(b) << JS2VAL_TAGBITS,     \
                                             JS2VAL_BOOLEAN)

    class JSValue {
    private:
        JSValue() { ASSERT(false); }

    public:        

        static uint32 tag(const js2val v)                    { return JS2VAL_TAG(v); }
        static float64 f64(const js2val v)                  { return *JS2VAL_TO_DOUBLE(v); }
        static JSObject *object(const js2val v)             { return JS2VAL_TO_OBJECT(v); }
        static JSInstance *instance(const js2val v)         { return (JSInstance *)JS2VAL_TO_OBJECT(v); }
        static JSFunction *function(const js2val v)         { return (JSFunction *)JS2VAL_TO_OBJECT(v); }
        static JSType *type(const js2val v)                 { return (JSType *)JS2VAL_TO_OBJECT(v); }
        static const String *string(const js2val v)         { return JS2VAL_TO_STRING(v); }
        static bool boolean(const js2val v)                 { return JS2VAL_TO_BOOLEAN(v); }
        static NamedArgument *namedArg(const js2val v)      { return (NamedArgument *)JS2VAL_TO_OBJECT(v); }
        static Attribute *attribute(const js2val v)         { return (Attribute *)JS2VAL_TO_OBJECT(v); }
        static Package *package(const js2val v)             { return (Package *)JS2VAL_TO_OBJECT(v); }

        static js2val newNumber(float64 f)                  { return DOUBLE_TO_JS2VAL(new double(f));}
        static js2val newObject(JSObject *object)           { return OBJECT_TO_JS2VAL(object); }
        static js2val newInstance(JSInstance *instance)     { return OBJECT_TO_JS2VAL(instance); }
        static js2val newFunction(JSFunction *function)     { return OBJECT_TO_JS2VAL(function); }
        static js2val newType(JSType *type)                 { return OBJECT_TO_JS2VAL(type); }
        static js2val newString(const String *string)       { return STRING_TO_JS2VAL(string); }
        static js2val newBoolean(bool boolean)              { return BOOLEAN_TO_JS2VAL(boolean); }

        static js2val newNamedArg(NamedArgument *arg)       { return OBJECT_TO_JS2VAL(arg); }
        static js2val newAttribute(Attribute *attr)         { return OBJECT_TO_JS2VAL(attr); }
        static js2val newPackage(Package *pkg)              { return OBJECT_TO_JS2VAL(pkg); }

        
        static bool isObject(const js2val v)                { return JS2VAL_IS_OBJECT(v); }
        static bool isInstance(const js2val v);
        static bool isNumber(js2val v)                      { return JS2VAL_IS_NUMBER(v); }
        static bool isBool(const js2val v)                  { return JS2VAL_IS_BOOLEAN(v); }
        static bool isType(const js2val v);
        static bool isFunction(const js2val v);
        static bool isString(const js2val v)                { return JS2VAL_IS_STRING(v); }
        static bool isPrimitive(const js2val v)             { return isNumber(v) || isBool(v) || isString(v) || isUndefined(v) || isNull(v); }
        static bool isNamedArg(const js2val v);
        static bool isAttribute(const js2val v);
        static bool isPackage(const js2val v);

        static bool isUndefined(const js2val v)             { return JS2VAL_IS_VOID(v); }
        static bool isNull(const js2val v)                  { return JS2VAL_IS_NULL(v); }
        static bool isNaN(const js2val v)                   { ASSERT(isNumber(v)); return JSDOUBLE_IS_NaN(*JS2VAL_TO_DOUBLE(v)); }
        static bool isNegativeInfinity(const js2val v)      { ASSERT(isNumber(v)); return (*JS2VAL_TO_DOUBLE(v) < 0) && JSDOUBLE_IS_INFINITE(*JS2VAL_TO_DOUBLE(v)); }
        static bool isPositiveInfinity(const js2val v)      { ASSERT(isNumber(v)); return (*JS2VAL_TO_DOUBLE(v) > 0) && JSDOUBLE_IS_INFINITE(*JS2VAL_TO_DOUBLE(v)); }
        static bool isNegativeZero(const js2val v)          { ASSERT(isNumber(v)); return JSDOUBLE_IS_NEGZERO(*JS2VAL_TO_DOUBLE(v)); }
        static bool isPositiveZero(const js2val v)          { ASSERT(isNumber(v)); return JSDOUBLE_IS_POSZERO(*JS2VAL_TO_DOUBLE(v)); }

        static bool isFalse(const js2val v)                 { return (v == JS2VAL_FALSE); }
        static bool isTrue(const js2val v)                  { return (v == JS2VAL_TRUE); }

        static JSType *getType(const js2val v);

        static js2val toString(Context *cx, const js2val v)        { return (isString(v) ? v : valueToString(cx, v)); }
        static js2val toNumber(Context *cx, const js2val v)        { return (isNumber(v) ? v : valueToNumber(cx, v)); }
        static js2val toInteger(Context *cx, const js2val v)       { return valueToInteger(cx, v); }
        static js2val toUInt32(Context *cx, const js2val v)        { return valueToUInt32(cx, v); }
        static js2val toUInt16(Context *cx, const js2val v)        { return valueToUInt16(cx, v); }
        static js2val toInt32(Context *cx, const js2val v)         { return valueToInt32(cx, v); }
        static js2val toObject(Context *cx, const js2val v)        { return ((isObject(v) || isType(v) || isFunction(v) || isInstance(v) || isPackage(v)) ?
                                                                                    v : valueToObject(cx, v)); }
        static js2val toBoolean(Context *cx, const js2val v)       { return (isBool(v) ? v : valueToBoolean(cx, v)); }

        static float64 getNumberValue(const js2val v);
        static const String *getStringValue(const js2val v);
        static bool getBoolValue(const js2val v);
        static JSObject *getObjectValue(const js2val v);

        static JSObject *toObjectValue(Context *cx, const js2val v);

        /* These are for use in 'toPrimitive' calls */
        enum Hint {
            NumberHint, StringHint, NoHint
        };
        static js2val toPrimitive(Context *cx, const js2val v, Hint hint = NoHint);
        
        static js2val valueToNumber(Context *cx, const js2val value);
        static js2val valueToInteger(Context *cx, const js2val value);
        static js2val valueToString(Context *cx, const js2val value);
        static js2val valueToObject(Context *cx, const js2val value);
        static js2val valueToUInt32(Context *cx, const js2val value);
        static js2val valueToUInt16(Context *cx, const js2val value);
        static js2val valueToInt32(Context *cx, const js2val value);
        static js2val valueToBoolean(Context *cx, const js2val value);
        

        static float64 float64ToInteger(float64 d);
        static int32 float64ToInt32(float64 d);
        static uint32 float64ToUInt32(float64 d);

        static void print(Formatter& f, const js2val value);


#ifdef NOT_SCARED_OF_GC_CODE    
	/**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* collector)
        {
            switch (tag) {
            case object_tag:	object = (JSObject*) collector->copy(object); break;
            case function_tag:	function = (JSFunction*) collector->copy(function); break;
            case type_tag:	type = (JSType*) collector->copy(type); break;
            default:		break;
            }
            return sizeof(JSValue);
        }

        void* operator new(size_t n, Collector& gc)
        {
            static Collector::InstanceOwner<JSValue> owner;
            return gc.allocateObject(n, &owner);
        }
#endif

#ifdef DEBUG
        void* operator new(size_t s)  { void *t = STD::malloc(s); trace_alloc("JSValue", s, t); return t; }
        void operator delete(void* t) { trace_release("JSValue", t); STD::free(t); }
#endif

    };

    extern js2val kUndefinedValue;
    extern js2val kNaNValue;
    extern js2val kTrueValue;
    extern js2val kFalseValue;
    extern js2val kNullValue;
    extern js2val kNegativeZero;
    extern js2val kPositiveZero;
    extern js2val kNegativeInfinity;
    extern js2val kPositiveInfinity;
    

    
    typedef enum { Read, Write } Access;
    Formatter& operator<<(Formatter& f, const Access& acc);
    
    
    typedef enum {
        None,
        Posate,
        Negate,
        Complement,
        Increment,
        Decrement,
        Call,
        New,
        Index,
        IndexEqual,
        DeleteIndex,
        Plus,
        Minus,
        Multiply,
        Divide,
        Remainder,
        ShiftLeft,
        ShiftRight,
        UShiftRight,
        Less,
        LessEqual,
        In,
        Equal,
        SpittingImage,
        BitAnd,
        BitXor,
        BitOr,
        OperatorCount
    } Operator;
    
/*
XXX ...couldn't get this to work...

    class OperatorEntry {
    public:
        OperatorEntry(const String &, JS2Runtime::Operator) {}
        OperatorEntry& operator=(const OperatorEntry &) { return *this; }

        const String mName;
        JS2Runtime::Operator op;

        bool operator == (const String &name) { return name == mName; }
    };

    typedef HashTable<OperatorEntry, const String> OperatorHashTable;
    extern OperatorHashTable operatorHashTable;
*/

    typedef std::map<const String, JS2Runtime::Operator> OperatorMap;
    extern OperatorMap operatorMap;
        
    class Reference {
    public:
        Reference(JSType *type, PropertyAttribute attr) : mType(type), mAttr(attr) { }
        JSType *mType;
        PropertyAttribute mAttr;

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("Reference", s, t); return t; }
        void operator delete(void* t)   { trace_release("Reference", t); STD::free(t); }
#endif

        // used by the invocation sequence to calculate
        // the stack depth and specify the 'this' flag
        virtual bool needsThis()                            { return false; }

        // issued as soon as the reference is generated to
        // establish any required base object.
        virtual void emitImplicitLoad(ByteCodeGen *)        { } 

        // acquires the invokable object
        virtual void emitInvokeSequence(ByteCodeGen *bcg)   { emitCodeSequence(bcg); }

        // issued before the rvalue is evaluated.
        // returns true if it pushes anything on the stack
        virtual bool emitPreAssignment(ByteCodeGen *)       { return false; }

        virtual void emitCodeSequence(ByteCodeGen *) 
                { throw Exception(Exception::internalError, "gen code for base ref"); }

        // returns the amount of stack used by the reference
        virtual uint16 baseExpressionDepth()                 { return 0; }

        // generate code sequence for typeof operator
        virtual void emitTypeOf(ByteCodeGen *bcg);

        // generate code sequence for delete operator
        virtual void emitDelete(ByteCodeGen *bcg);

        bool isConst()                                      { return ((mAttr & Property::Const) == Property::Const); }
    };

    // a getter/setter function from an activation
    // the function is known directly
    class AccessorReference : public Reference {
    public:
        AccessorReference(JSFunction *f, PropertyAttribute attr);
        JSFunction *mFunction;
        void emitCodeSequence(ByteCodeGen *bcg);
    };
    // a simple local variable reference - it's a slot
    // in the current activation
    class LocalVarReference : public Reference {
    public:
        LocalVarReference(uint32 index, Access acc, JSType *type, PropertyAttribute attr)
            : Reference(type, attr), mAccess(acc), mIndex(index) { }
        Access mAccess;
        uint32 mIndex;
        void emitCodeSequence(ByteCodeGen *bcg);
    };
    // a local variable 'n' activations up the
    // execution stack
    class ClosureVarReference : public LocalVarReference {
    public:
        ClosureVarReference(uint32 depth, uint32 index, Access acc, JSType *type, PropertyAttribute attr) 
                        : LocalVarReference(index, acc, type, attr), mDepth(depth) { }
        uint32 mDepth;
        void emitCodeSequence(ByteCodeGen *bcg);
    };
    // a member field in an instance
    class FieldReference : public Reference {
    public:
        FieldReference(uint32 index, Access acc, JSType *type, PropertyAttribute attr) 
            : Reference(type, attr), mAccess(acc), mIndex(index) { }
        Access mAccess;
        uint32 mIndex;
        void emitCodeSequence(ByteCodeGen *bcg);
        void emitImplicitLoad(ByteCodeGen *bcg);
        uint16 baseExpressionDepth() { return 1; }
    };
    // a static field
    class StaticFieldReference : public Reference {
    public:
        StaticFieldReference(const String& name, Access acc, JSType *baseClass, JSType *type, PropertyAttribute attr) 
            : Reference(type, attr), mAccess(acc), mName(name), mClass(baseClass) { }
        Access mAccess;
        const String& mName;
        JSType *mClass;
        void emitCodeSequence(ByteCodeGen *bcg);
        void emitInvokeSequence(ByteCodeGen *bcg);
        void emitImplicitLoad(ByteCodeGen *bcg);
        uint16 baseExpressionDepth() { return 1; }
        void emitDelete(ByteCodeGen *bcg);
    };
    // a member function in a vtable
    class MethodReference : public Reference {
    public:
        MethodReference(uint32 index, JSType *baseClass, JSType *type, PropertyAttribute attr) 
            : Reference(type, attr), mIndex(index), mClass(baseClass) { }
        uint32 mIndex;
        JSType *mClass;
        void emitCodeSequence(ByteCodeGen *bcg);
        virtual bool needsThis() { return true; }
        virtual void emitImplicitLoad(ByteCodeGen *bcg);
        virtual uint16 baseExpressionDepth() { return 1; }
        void emitInvokeSequence(ByteCodeGen *bcg);
    };
    class GetterMethodReference : public MethodReference {
    public:
        GetterMethodReference(uint32 index, JSType *baseClass, JSType *type, PropertyAttribute attr)
            : MethodReference(index, baseClass, type, attr) { }
        void emitCodeSequence(ByteCodeGen *bcg);
    };
    class SetterMethodReference : public MethodReference {
    public:
        SetterMethodReference(uint32 index, JSType *baseClass, JSType *type, PropertyAttribute attr)
            : MethodReference(index, baseClass, type, attr) { }
        void emitCodeSequence(ByteCodeGen *bcg);
        bool emitPreAssignment(ByteCodeGen *bcg);
    };

    // a function
    class FunctionReference : public Reference {
    public:
        FunctionReference(JSFunction *f, PropertyAttribute attr);
        JSFunction *mFunction;
        void emitCodeSequence(ByteCodeGen *bcg);
    };
    // a getter function
    class GetterFunctionReference : public Reference {
    public:
        GetterFunctionReference(JSFunction *f, PropertyAttribute attr);
        JSFunction *mFunction;
        void emitCodeSequence(ByteCodeGen *bcg);
    };
    // a setter function
    class SetterFunctionReference : public Reference {
    public:
        SetterFunctionReference(JSFunction *f, JSType *type, PropertyAttribute attr);
        JSFunction *mFunction;
        void emitCodeSequence(ByteCodeGen *bcg);
        void emitImplicitLoad(ByteCodeGen *bcg);
    };
    // Either an existing value property (dynamic) or
    // the "we don't know any field by that name".
    class PropertyReference : public Reference {
    public:
        PropertyReference(const String& name, NamespaceList *nameSpace, Access acc, JSType *type, PropertyAttribute attr)
            : Reference(type, attr), mAccess(acc), mName(name), mNameSpace(nameSpace) { }
        Access mAccess;
        const String& mName;
        NamespaceList *mNameSpace;
        void emitCodeSequence(ByteCodeGen *bcg);
        void emitInvokeSequence(ByteCodeGen *bcg);
        uint16 baseExpressionDepth() { return 1; }
        bool needsThis() { return true; }
        void emitImplicitLoad(ByteCodeGen *bcg);
        void emitDelete(ByteCodeGen *bcg);
    };
    // a parameter slot (they can't have getter/setter, right?)
    class ParameterReference : public Reference {
    public:
        ParameterReference(uint32 index, Access acc, JSType *type, PropertyAttribute attr) 
            : Reference(type, attr), mAccess(acc), mIndex(index) { }
        Access mAccess;
        uint32 mIndex;
        void emitCodeSequence(ByteCodeGen *bcg);
    };

    // the generic "we don't know anybody by that name" - not bound to a specific object
    // so it's a scope chain lookup at runtime
    class NameReference : public Reference {
    public:
        NameReference(const String& name, NamespaceList *nameSpace, Access acc)
            : Reference(Object_Type, 0), mAccess(acc), mName(name), mNameSpace(nameSpace) { }
        NameReference(const String& name, NamespaceList *nameSpace, Access acc, JSType *type, PropertyAttribute attr)
            : Reference(type, attr), mAccess(acc), mName(name), mNameSpace(nameSpace) { }
        Access mAccess;
        const String& mName;
        NamespaceList *mNameSpace;
        void emitCodeSequence(ByteCodeGen *bcg);
        void emitTypeOf(ByteCodeGen *bcg);
        void emitDelete(ByteCodeGen *bcg);
    };

    class ElementReference : public Reference {
    public:
        ElementReference(Access acc, uint16 depth)
            : Reference(Object_Type, 0), mAccess(acc), mDepth(depth) { }
        Access mAccess;
        uint16 mDepth;
        void emitCodeSequence(ByteCodeGen *bcg);
        uint16 baseExpressionDepth() { return (uint16)(mDepth + 1); }
        void emitDelete(ByteCodeGen *bcg);
    };



    
    
    
    
    
    
    class JSObject {
    public:
    // The generic Javascript object. Every JS2 object is one of these
        JSObject() : mPrototype(kNullValue) { }
        
        virtual ~JSObject() { } // keeping gcc happy
        
        // the property data is kept (or referenced from) here
        PropertyMap   mProperties;

        // Every JSObject (except the Ur-object) has a prototype
        js2val       mPrototype;

        virtual bool isDynamic() { return true; }


        // find a property by the given name, and then check to see if there's any
        // overlap between the supplied attribute list and the property's list.
        // ***** REWRITE ME -- matching attribute lists for inclusion is a bad idea.
        virtual PropertyIterator findNamespacedProperty(const String &name, NamespaceList *names);

        virtual bool deleteProperty(Context *cx, const String &name, NamespaceList *names);

        // see if the property exists by a specific kind of access
        virtual bool hasOwnProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p);
        
        virtual bool hasProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p);

        virtual js2val getPropertyValue(PropertyIterator &i);

        virtual void getProperty(Context *cx, const String &name, NamespaceList *names);

        virtual void setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v);



        // add a property
        virtual Property *defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type);
        virtual Property *defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type);

        // add a property/value into the map 
        // - assumes the map doesn't already have this property
        Property *insertNewProperty(const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const js2val v);

        virtual Property *defineStaticVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type)
        {
            return JSObject::defineVariable(cx, name, attr, type);   
            // XXX or error?  (Note that this implementation is invoked by JSType::defineStaticXXXX
            // - the question is, is static var X (etc) at global scope an error?
        }
        // add a method property
        virtual void defineMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            defineVariable(cx, name, attr, Function_Type, JSValue::newFunction(f));
        }
        virtual void defineStaticMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineVariable(cx, name, attr, Function_Type, JSValue::newFunction(f));    // XXX or error?
        }
        virtual void defineConstructor(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineVariable(cx, name, attr, Function_Type, JSValue::newFunction(f));    // XXX or error?
        }
        virtual void defineStaticGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineGetterMethod(cx, name, attr, f);    // XXX or error?
        }
        virtual void defineStaticSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineSetterMethod(cx, name, attr, f);    // XXX or error?
        }
        virtual void defineGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);
        virtual void defineSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        virtual Property *defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type, const js2val v);
        virtual Property *defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const js2val v);
        virtual Property *defineAlias(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, js2val vp);
        
        virtual Reference *genReference(Context *cx, bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        virtual JSType *topClass()                      { return NULL; }
        virtual bool isNestedFunction()                 { return false; }
        virtual JSFunction *getContainerFunction()      { return NULL; }
        
        virtual bool hasLocalVars()     { return false; }
        virtual uint32 localVarCount()  { return 0; }

        virtual void defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type);

        virtual js2val getSlotValue(Context * /*cx*/, uint32 /*slotIndex*/)    { ASSERT(false); return kUndefinedValue; }
        virtual void setSlotValue(Context * /*cx*/, uint32 /*slotIndex*/, js2val /*v*/)    { ASSERT(false); }

        // debug only        
        void printProperties(Formatter &f) const
        {
            for (PropertyMap::const_iterator i = mProperties.begin(), end = mProperties.end(); (i != end); i++) 
            {
                f << "[" << PROPERTY_NAME(i) << "] " << *PROPERTY(i);
            }
        }

    protected:
        typedef Collector::InstanceOwner<JSObject> JSObjectOwner;
        friend class Collector::InstanceOwner<JSObject>;
        /**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* /* collector */)
        {
            // enumerate property map elements.
            // scan mPrototype.
            return sizeof(JSObject);
        }

    public:
        void* operator new(size_t n, Collector& gc)
        {
            static JSObjectOwner owner;
            return gc.allocateObject(n, &owner);
        }

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSObject", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSObject", t); STD::free(t); }
#endif

        static uint32 tempVarCount;
    };

    Formatter& operator<<(Formatter& f, const JSObject& obj);
    


        
    
    
    
    
    

    
 


    class JSInstance : public JSObject {
    public:
        
        JSInstance(Context *cx, JSType *type) 
            : JSObject(), mType(type), mInstanceValues(NULL) { if (type) initInstance(cx, type); }

        virtual ~JSInstance() { } // keeping gcc happy

        void initInstance(Context *cx, JSType *type);

        void getProperty(Context *cx, const String &name, NamespaceList *names);
        void setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v);

        js2val getField(uint32 index)
        {
            return mInstanceValues[index];
        }

        void setField(uint32 index, js2val v)
        {
            mInstanceValues[index] = v;
        }

        JSType *getType() const { return mType; }
        
        bool isDynamic();

        // the class that created this instance
        JSType        *mType;

        js2val         *mInstanceValues;

    protected:
        typedef Collector::InstanceOwner<JSInstance> JSInstanceOwner;
        friend class Collector::InstanceOwner<JSInstance>;
        /**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* collector)
        {
            JSObject::scan(collector);
            mType = (JSType*) collector->copy(mType);
            // FIXME: need some kind of array operator new[] (gc) thing.
            // this will have to use an extra word to keep track of the
            // element count.
            mInstanceValues = (js2val*) collector->copy(mInstanceValues);
            return sizeof(JSInstance);
        }
    
    public:
        void* operator new(size_t n, Collector& gc)
        {
            static JSInstanceOwner owner;
            return gc.allocateObject(n, &owner);
        }

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSInstance", t); STD::free(t); }
#endif
    };
    Formatter& operator<<(Formatter& f, const JSInstance& obj);

    

    typedef std::vector<JSFunction *> MethodList;

    class ScopeChain;










    class JSType : public JSInstance {
    protected:
        // XXX these initializations are for ParameterBarrel & Activation which are 'types' only
        // because they take advantage of the slotted variable handling - maybe an interim class
        // for just that purpose would be better...
        JSType() : JSInstance(NULL, NULL), mSuperType(NULL), mVariableCount(0), mIsDynamic(false) { }

    public:        
        JSType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto);

        virtual ~JSType() { }       // keeping gcc happy



        void setSuperType(JSType *super);

        void setStaticInitializer(Context *cx, JSFunction *f);
        void setInstanceInitializer(Context *cx, JSFunction *f);

        virtual JSType *topClass()      { return this; }


        // construct a new (empty) instance of this class
        virtual js2val newInstance(Context *cx);
           

        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type);


   // XXX
   // XXX why doesn't the virtual function in JSObject get found?
   // XXX
        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type, js2val v)
        {
            return JSObject::defineVariable(cx, name, attr, type, v);
        }
        Property *defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const js2val v)
        {
            return JSObject::defineVariable(cx, name, names, attrFlags, type, v);
        }




        void defineMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f);
        
        void defineGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        void defineSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        void setDefaultConstructor(Context * /*cx*/, JSFunction *f)
        {
            mDefaultConstructor = f;
        }

        void addMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);
        void addStaticMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        // return true if 'other' is on the chain of supertypes
        bool derivesFrom(JSType *other);

        virtual void getProperty(Context *cx, const String &name, NamespaceList *names);
        virtual void setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v);

        virtual js2val getPropertyValue(PropertyIterator &i);

        virtual bool hasProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p);

        virtual Reference *genReference(Context *cx, bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        JSFunction *getDefaultConstructor() { return mDefaultConstructor; }
        JSFunction *getTypeCastFunction()   { return mTypeCast; }

        js2val getUninitializedValue()     { return mUninitializedValue; }

        // Generates defaultConstructor if one doesn't exist -
        // assumes that the super types have been completed already
        void completeClass(Context *cx, ScopeChain *scopeChain);

        virtual bool isDynamic()            { return mIsDynamic; }

        JSType          *mSuperType;                // NULL implies that this is the base Object

        uint32          mVariableCount;             // number of instance variables
        JSFunction      *mInstanceInitializer;
        JSFunction      *mDefaultConstructor;
        JSFunction      *mTypeCast;

        // the 'vtable'
        MethodList          mMethods;

        const StringAtom    *mClassName;
        const StringAtom    *mPrivateNamespace;

        bool            mIsDynamic;
        js2val         mUninitializedValue;            // the value for uninitialized vars

        js2val         mPrototypeObject;              // becomes the prototype for any instance

        // DEBUG
        void printSlotsNStuff(Formatter& f) const;

    protected:
        typedef Collector::InstanceOwner<JSType> JSTypeOwner;
        friend class Collector::InstanceOwner<JSType>;
        /**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* collector)
        {
            JSObject::scan(collector);
            mSuperType = (JSType*) collector->copy(mSuperType);
            mInstanceInitializer = (JSFunction*) collector->copy(mInstanceInitializer);
            mDefaultConstructor = (JSFunction*) collector->copy(mDefaultConstructor);
            mTypeCast = (JSFunction*) collector->copy(mTypeCast);
            // scan mMethods.
            // scan mClassName.
            // scan mPrivateNamespace.
            // scan mUninitializedValue.
            // scan mPrototypeObject.
            return sizeof(JSType);
        }

    public:
        void* operator new(size_t n, Collector& gc)
        {
            static JSTypeOwner owner;
            return gc.allocateObject(n, &owner);
        }

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSType", t); STD::free(t); }
#endif
    };

    Formatter& operator<<(Formatter& f, const JSType& obj);

    //
    // we have to have unique instance classes whenever the instance requires
    // extra data - otherwise where else does this data go?
    //
    // XXX could instead have dynamically constructed the various classes with
    // the appropriate number of instance slots and used the generic newInstance
    // mechanism. Then the extra data would just be instance->slot[0,1...]
    //
    // XXX maybe could have implemented length (for string) as a getter/setter pair
    // (would still require StringType, but the new instances would all get
    // the pair of methods for free)
    //
    class JSArrayInstance : public JSInstance {
    public:
        JSArrayInstance(Context *cx) : JSInstance(cx, NULL), mLength(0) { mType = (JSType *)Array_Type; mPrototype = Object_Type->mPrototypeObject; }
        virtual ~JSArrayInstance() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSArrayInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSArrayInstance", t); STD::free(t); }
#endif
        void setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v);
        void getProperty(Context *cx, const String &name, NamespaceList *names);

        bool hasOwnProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p);
        bool deleteProperty(Context *cx, const String &name, NamespaceList *names);

        uint32 mLength;
    };

    class JSArrayType : public JSType {
    public:
        JSArrayType(Context *cx, JSType *elementType, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto), mElementType(elementType)
        {
        }
        virtual ~JSArrayType() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSArrayType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSArrayType", t); STD::free(t); }
#endif

        js2val newInstance(Context *cx);
        JSType *mElementType;
    };

    class JSStringInstance : public JSInstance {
    public:
        JSStringInstance(Context *cx) : JSInstance(cx, NULL), mValue(NULL) { mType = (JSType *)String_Type; }
        virtual ~JSStringInstance() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSStringInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSStringInstance", t); STD::free(t); }
#endif
        void setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v);
        void getProperty(Context *cx, const String &name, NamespaceList *names);
        bool hasOwnProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p);
        bool deleteProperty(Context *cx, const String &name, NamespaceList *names);

        const String *mValue;
    };

    class JSStringType : public JSType {
    public:
        JSStringType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto)
        {
        }
        virtual ~JSStringType() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSStringType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSStringType", t); STD::free(t); }
#endif
        js2val newInstance(Context *cx);
    };

    class JSBooleanInstance : public JSInstance {
    public:
        JSBooleanInstance(Context *cx) : JSInstance(cx, NULL), mValue(false) { mType = (JSType *)Boolean_Type; }
        virtual ~JSBooleanInstance() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSBooleanInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSBooleanInstance", t); STD::free(t); }
#endif
        bool mValue;
    };

    class JSBooleanType : public JSType {
    public:
        JSBooleanType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto)
        {
        }
        virtual ~JSBooleanType() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSBooleanType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSBooleanType", t); STD::free(t); }
#endif
        js2val newInstance(Context *cx);
    };

    class JSNumberInstance : public JSInstance {
    public:
        JSNumberInstance(Context *cx) : JSInstance(cx, NULL), mValue(0.0) { mType = (JSType *)Number_Type; }
        virtual ~JSNumberInstance() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSNumberInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSNumberInstance", t); STD::free(t); }
#endif
        float64 mValue;
    };

    class JSNumberType : public JSType {
    public:
        JSNumberType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto)
        {
        }
        virtual ~JSNumberType() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSNumberType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSNumberType", t); STD::free(t); }
#endif
        js2val newInstance(Context *cx);
    };

    class JSDateInstance : public JSInstance {
    public:
        JSDateInstance(Context *cx) : JSInstance(cx, NULL), mValue(0.0) { mType = (JSType *)Date_Type; }
        virtual ~JSDateInstance() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSDateInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSDateInstance", t); STD::free(t); }
#endif
        float64 mValue;
    };

    class JSDateType : public JSType {
    public:
        JSDateType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto)
        {
        }
        virtual ~JSDateType() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSDateType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSDateType", t); STD::free(t); }
#endif
        js2val newInstance(Context *cx);
    };

    class JSRegExpInstance : public JSInstance {
    public:
        JSRegExpInstance(Context *cx) : JSInstance(cx, NULL), mLastIndex(0), mRegExp(NULL) { mType = (JSType *)RegExp_Type; }
        virtual ~JSRegExpInstance() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSRegExpInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSRegExpInstance", t); STD::free(t); }
#endif    
        uint32 mLastIndex;
        REState *mRegExp;
    };

    class JSRegExpType : public JSType {
    public:
        JSRegExpType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto)
        {
        }
        virtual ~JSRegExpType() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSBooleanType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSBooleanType", t); STD::free(t); }
#endif
        js2val newInstance(Context *cx);
    };

    class JSObjectType : public JSType {
    public:
        JSObjectType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSType(cx, name, super, protoObj, typeProto)
        {
        }
        virtual ~JSObjectType() { } // keeping gcc happy
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSObjectType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSObjectType", t); STD::free(t); }
#endif
        js2val newInstance(Context *cx);
    };


    // captures the Parameter names scope
    // it's a JSType simply because it's also a thing that
    // maps from names to slots.
    class ParameterBarrel : public JSType {
    public:

        ParameterBarrel() : JSType() 
        {
        }
        virtual ~ParameterBarrel() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("ParameterBarrel", s, t); return t; }
        void operator delete(void* t)   { trace_release("ParameterBarrel", t); STD::free(t); }
#endif

        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type);
        Reference *genReference(Context *cx, bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        js2val getSlotValue(Context *cx, uint32 slotIndex);
        void setSlotValue(Context *cx, uint32 slotIndex, js2val v);

    };







    // an Activation has two jobs:
    // 1. At compile time it handles the function/method being compiled and collects
    //      the local vars/consts being defined in that function. 
    // 2. At runtime it is the container for the values of those local vars
    //      (although it's only constructed as such when the function 
    //          either calls another function - so the activation represents
    //          the saved state, or when a closure object is constructed)

    class Activation : public JSType {
    public:

        Activation() 
            :	mLocals(NULL), 
                mStack(NULL),
                mStackTop(0),
                mPC(0), 
                mModule(NULL),
                mContainer(NULL),
                mNamespaceList(NULL)
        {}

        Activation(js2val *locals, 
                   js2val *stack, uint32 stackTop,
                   ScopeChain *scopeChain,
                   js2val *argBase, js2val curThis,
                   uint8 *pc, 
                   ByteCodeModule *module,
                   NamespaceList *namespaceList )
            : 	mLocals(locals), 
                mStack(stack), 
                mStackTop(stackTop),
                mScopeChain(scopeChain),
                mArgumentBase(argBase), 
                mThis(curThis), 
                mPC(pc), 
                mModule(module),
                mContainer(NULL),
                mNamespaceList(namespaceList)
        {}

        virtual ~Activation() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("Activation", s, t); return t; }
        void operator delete(void* t)   { trace_release("Activation", t); STD::free(t); }
#endif

        virtual bool isNestedFunction() { return true; }

        void defineMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineMethod(cx, name, attr, f);
        }

        JSType *topClass()              { return NULL;  }
        
        
        // saved values from a previous execution
        js2val *mLocals;
        js2val *mStack;
        uint32 mStackTop;           
        ScopeChain *mScopeChain;
        js2val *mArgumentBase;
        js2val mThis;
        uint8 *mPC;
        ByteCodeModule *mModule;
        JSFunction *mContainer;
        NamespaceList *mNamespaceList;

        virtual JSFunction *getContainerFunction()      { return mContainer; }


        bool hasLocalVars()             { return true; }
        virtual uint32 localVarCount()  { return mVariableCount; }

        void defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type);

        Reference *genReference(Context *cx, bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        js2val getSlotValue(Context *cx, uint32 slotIndex);
        void setSlotValue(Context *cx, uint32 slotIndex, js2val v);

    };


    class ScopeChain {
    public:

        ScopeChain(Context *cx, World &) :
              m_cx(cx)
        {
        }

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("ScopeChain", s, t); return t; }
        void operator delete(void* t)   { trace_release("ScopeChain", t); STD::free(t); }
#endif

        Context *m_cx;

        std::vector<JSObject *> mScopeStack;
        typedef std::vector<JSObject *>::reverse_iterator ScopeScanner;


        void addScope(JSObject *s)
        {
            mScopeStack.push_back(s);
        }

        void popScope()
        {
            ASSERT(mScopeStack.size());
            mScopeStack.pop_back();
        }

        // add a new name to the current scope
        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type)
        {
            JSObject *top = mScopeStack.back();
            return top->defineVariable(cx, name, attr, type);
        }
        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type, js2val v)
        {
            JSObject *top = mScopeStack.back();
            return top->defineVariable(cx, name, attr, type, v);
        }
        Property *defineAlias(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, js2val vp)
        {
            JSObject *top = mScopeStack.back();
            return top->defineAlias(cx, name, names, attrFlags, type, vp);
        }
        Property *defineStaticVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type)
        {
            JSObject *top = mScopeStack.back();
            ASSERT(dynamic_cast<JSType *>(top));
            return top->defineStaticVariable(cx, name, attr, type);
        }
        void defineMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            top->defineMethod(cx, name, attr, f);
        }   
        void defineStaticMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            ASSERT(dynamic_cast<JSType *>(top));
            top->defineStaticMethod(cx, name, attr, f);
        }   
        void defineConstructor(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            ASSERT(dynamic_cast<JSType *>(top));
            top->defineConstructor(cx, name, attr, f);
        }   
        void defineGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            top->defineGetterMethod(cx, name, attr, f);
        }
        void defineSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            top->defineSetterMethod(cx, name, attr, f);
        }
        void defineStaticGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            ASSERT(dynamic_cast<JSType *>(top));
            top->defineStaticGetterMethod(cx, name, attr, f);
        }
        void defineStaticSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            ASSERT(dynamic_cast<JSType *>(top));
            top->defineStaticSetterMethod(cx, name, attr, f);
        }
        void defineUnaryOperator(Context *cx, Operator which, JSFunction *f);

        // see if the current scope contains a name already
        bool hasProperty(Context *cx, const String& name, NamespaceList *names, Access acc, PropertyIterator *p)
        {
            JSObject *top = mScopeStack.back();
            return top->hasProperty(cx, name, names, acc, p);
        }

        bool deleteName(Context *cx, const String& name, NamespaceList *names);

        // delete a property from the top object (already know it's there)
        bool deleteProperty(Context *cx, const String &name, NamespaceList *names)
        {
            JSObject *top = mScopeStack.back();
            return top->deleteProperty(cx, name, names);
        }

        // generate a reference to the given name
        Reference *getName(Context *cx, const String& name, NamespaceList *names, Access acc);

        bool hasNameValue(Context *cx, const String& name, NamespaceList *names);

        // pushes the value of the name and returns it's container object
        JSObject *getNameValue(Context *cx, const String& name, NamespaceList *names);

        // return the class on the top of the stack (or NULL if there
        // isn't one there).
        JSType *topClass()
        {
            JSObject *obj = mScopeStack.back();
            return obj->topClass();
        }
        
        JSFunction *getContainerFunction()
        {
            JSObject *obj = mScopeStack.back();
            return (obj->getContainerFunction());
        }

        // return 'true' if the current top of scope stack is an
        // activation - which would make any function declarations
        // be local declarations.
        bool isNestedFunction()
        {
            JSObject *obj = mScopeStack.back();
            return obj->isNestedFunction();
        }

	bool isPossibleUncheckedFunction(FunctionDefinition &f);

        void defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type)
        {
            mScopeStack.back()->defineTempVariable(cx, readRef, writeRef, type);
        }

        // a compile time request to get the value for a name
        // (i.e. we're accessing a constant value)
        js2val getCompileTimeValue(Context *cx, const String& name, NamespaceList *names);



        void setNameValue(Context *cx, const String& name, NamespaceList *names);

        // return the number of local vars used by all the 
        // Activations on the top of the chain
        uint32 countVars()
        {
            uint32 result = 0;
            for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++)
            {
                if ((*s)->hasLocalVars())
                    result += (*s)->localVarCount();
                else
                    break;
            }
            return result;
        }

        uint32 countActivations()
        {
            uint32 result = 0;
            for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++)
            {
                if ((*s)->hasLocalVars())
                    result++;
            }
            return result;
        }

        void collectNames(StmtNode *p);

        // Lookup a name as a type in the chain
        JSType *findType(Context *cx, const StringAtom& typeName, size_t pos);

        // Get a type from an ExprNode 
        JSType *extractType(ExprNode *t);

	// concoct a package name from an id list
	String getPackageName(IdentifierList *packageIdList);

    };


    class JSFunction : public JSInstance {
    protected:
        JSFunction(Context *cx) : JSInstance(cx, NULL), mActivation() 
        { 
            mType = (JSType *)Function_Type; 
            mActivation.mContainer = this; 
            mPrototype = Function_Type->mPrototypeObject; 
        }        // for JSBoundFunction (XXX ask Patrick about this structure)
    public:

        typedef enum { Invalid, RequiredParameter, OptionalParameter, RestParameter, NamedParameter } ParameterFlag;

        class ParameterData {
        public:
            ParameterData() : mName(NULL), mType(NULL), mInitializer((uint32)(-1)), mFlag(Invalid) { }
            const String *mName;
            JSType *mType;
            uint32 mInitializer;
            ParameterFlag mFlag;
        };


        typedef js2val (NativeCode)(Context *cx, const js2val thisValue, js2val argv[], uint32 argc);
        typedef js2val (NativeDispatch)(NativeCode *target, Context *cx, const js2val thisValue, js2val argv[], uint32 argc);

        // XXX these should be Function_Type->newInstance() calls, no?

        JSFunction(Context *cx, JSType *resultType, ScopeChain *scopeChain);        
        JSFunction(Context *cx, NativeCode *code, JSType *resultType, NativeDispatch *dispatch = NULL);

        ~JSFunction() { }  // keeping gcc happy
        
#ifdef DEBUG
        uint32 maxParameterIndex()              { return mRequiredParameters + mOptionalParameters + mNamedParameters + ((mHasRestParameter) ? 1 : 0); } 
#endif
        void setByteCode(ByteCodeModule *b)     { ASSERT(!isNative()); mByteCode = b; }
        void setResultType(JSType *r)           { mResultType = r; }
        void setParameterCounts(Context *cx, uint32 r, uint32 o, uint32 n, bool hasRest);
        void setParameter(uint32 index, const String *n, JSType *t, ParameterFlag flag)
                                                { ASSERT(mParameters && (index < maxParameterIndex())); 
                                                  mParameters[index].mType = t; mParameters[index].mName = n; mParameters[index].mFlag = flag; }
        void setParameterInitializer(uint32 index, uint32 offset)
                                                { ASSERT(mParameters && (index < maxParameterIndex())); mParameters[index].mInitializer = offset; }

        void setIsPrototype(bool i)             { mIsPrototype = i; }
        void setIsConstructor(bool i)           { mIsConstructor = i; }
        void setIsUnchecked()                   { mIsChecked = false; }
        void setFunctionName(FunctionName &n)   { mFunctionName = new FunctionName(); mFunctionName->prefix = n.prefix; mFunctionName->name = n.name; }
        void setFunctionName(const StringAtom *n)
                                                { mFunctionName = new FunctionName(); mFunctionName->name = n; }
        void setClass(JSType *c)                { mClass = c; }

        virtual bool hasBoundThis()             { return false; }
        virtual bool isNative()                 { return (mNativeCode != NULL); }
        virtual bool isPrototype()              { return mIsPrototype; }
        virtual bool isConstructor()            { return mIsConstructor; }
        virtual bool isMethod()                 { return (mClass != NULL); }
        virtual ByteCodeModule *getByteCode()   { ASSERT(!isNative()); return mByteCode; }
//        virtual NativeCode *getNativeCode()     { ASSERT(isNative()); return mCode; }
        virtual js2val invokeNativeCode(Context *cx, const js2val thisValue, js2val argv[], uint32 argc);
        virtual ParameterBarrel *getParameterBarrel()
                                                { return mParameterBarrel; }
        virtual Activation *getActivation()     { return &mActivation; }

        virtual ScopeChain *getScopeChain()     { return mScopeChain; }
        virtual js2val getThisValue()          { return kNullValue; }         
        virtual JSType *getClass()              { return mClass; }
        virtual FunctionName *getFunctionName() { return mFunctionName; }

        virtual bool isChecked()                { return mIsChecked; }

        virtual JSType *getResultType()         { return mResultType; }
        virtual JSType *getParameterType(uint32 a)    
                                                { ASSERT(mParameters && (a < maxParameterIndex())); return mParameters[a].mType; }
        virtual bool parameterHasInitializer(uint32 a)
                                                { ASSERT(mParameters && (a < maxParameterIndex())); return (mParameters[a].mInitializer != (uint32)(-1)); }
        virtual js2val runParameterInitializer(Context *cx, uint32 a, const js2val thisValue, js2val *argv, uint32 argc);

        virtual uint32 getRequiredParameterCount()   
                                                { return mRequiredParameters; }
        virtual uint32 getOptionalParameterCount()   
                                                { return mOptionalParameters; }
        virtual uint32 getNamedParameterCount() { return mNamedParameters; }
        virtual bool hasOptionalParameters()    { return (mOptionalParameters > 0); }
        virtual bool parameterIsRequired(uint32 index)  
                                                { ASSERT(mParameters && (index < maxParameterIndex())); return (mParameters[index].mFlag == RequiredParameter); }
        virtual bool parameterIsOptional(uint32 index)  
                                                { ASSERT(mParameters && (index < maxParameterIndex())); return (mParameters[index].mFlag == OptionalParameter); }
        virtual bool parameterIsNamed(uint32 index)  
                                                { ASSERT(mParameters && (index < maxParameterIndex())); return (mParameters[index].mFlag == NamedParameter); }
        virtual uint32 getRestParameterIndex()  { ASSERT(mHasRestParameter); return (mRequiredParameters + mOptionalParameters); }
        
        virtual const String *getParameterName(uint32 index)
                                                { ASSERT(mParameters && (index < maxParameterIndex())); return mParameters[index].mName; }
        virtual bool hasRestParameter()         { return mHasRestParameter; }

        virtual JSFunction *getFunction()       { return this; }
        bool isEqual(JSFunction *f)             { return (getFunction() == f->getFunction()); }

        void countParameters(Context *cx, FunctionDefinition &f);
        
        
        ParameterBarrel *mParameterBarrel;
        Activation mActivation;                 // not used during execution  (XXX so maybe we should handle it differently, hmmm?)

    private:
        ByteCodeModule *mByteCode;
        NativeCode *mNativeCode;
        JSType *mResultType;
        uint32 mRequiredParameters;
        uint32 mOptionalParameters;
        uint32 mNamedParameters;
        ParameterData *mParameters;
        ScopeChain *mScopeChain;
        bool mIsPrototype;                      // set for functions with prototype attribute
        bool mIsConstructor;
        bool mIsChecked;
        bool mHasRestParameter;
        JSType *mClass;                         // pointer to owning class if this function is a method
        FunctionName *mFunctionName;
        NativeDispatch *mDispatch;

    protected:
        typedef Collector::InstanceOwner<JSFunction> JSFunctionOwner;
        friend class Collector::InstanceOwner<JSFunction>;
        /**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* collector)
        {
            JSObject::scan(collector);
            mParameterBarrel = (ParameterBarrel*) collector->copy(mParameterBarrel);
            mResultType = (JSType*) collector->copy(mResultType);
            mClass = (JSType*) collector->copy(mClass);
            // scan mPrototype.
            return sizeof(JSFunction);
        }

    public:
        void* operator new(size_t n, Collector& gc)
        {
            static JSFunctionOwner owner;
            return gc.allocateObject(n, &owner);
        }

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSFunction", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSFunction", t); STD::free(t); }
#endif
    };



    class JSBoundFunction : public JSFunction {
    private:
        JSFunction *mFunction;
        JSObject *mThis;
    public:
        JSBoundFunction(Context *cx, JSFunction *f, JSObject *thisObj)
            : JSFunction(cx), mFunction(NULL), mThis(thisObj) { if (f->hasBoundThis()) mFunction = f->getFunction(); else mFunction = f; }

        ~JSBoundFunction() { }  // keeping gcc happy

        bool hasBoundThis()             { return true; }
        bool isNative()                 { return mFunction->isNative(); }
        bool isPrototype()              { return mFunction->isPrototype(); }
        bool isConstructor()            { return mFunction->isConstructor(); }
        bool isMethod()                 { return mFunction->isMethod(); }
        ByteCodeModule *getByteCode()   { return mFunction->getByteCode(); }
        js2val invokeNativeCode(Context *cx, const js2val thisValue, js2val argv[], uint32 argc)
                                        { return mFunction->invokeNativeCode(cx, thisValue, argv, argc); }
        ParameterBarrel *getParameterBarrel()
                                        { return mFunction->mParameterBarrel; }
        Activation *getActivation()     { return &mFunction->mActivation; }
        JSType *getResultType()         { return mFunction->getResultType(); }
        JSType *getParameterType(uint32 a)    { return mFunction->getParameterType(a); }
        bool parameterHasInitializer(uint32 a){ return mFunction->parameterHasInitializer(a); }
        js2val runParameterInitializer(Context *cx, uint32 a, const js2val thisValue, js2val *argv, uint32 argc)
                                        { return mFunction->runParameterInitializer(cx, a, thisValue, argv, argc); }
        ScopeChain *getScopeChain()     { return mFunction->getScopeChain(); }
        js2val getThisValue()           { return (mThis) ? JSValue::newObject(mThis) : kNullValue; }         
        JSType *getClass()              { return mFunction->getClass(); }
        FunctionName *getFunctionName() { return mFunction->getFunctionName(); }

        uint32 getRequiredParameterCount()   
                                        { return mFunction->getRequiredParameterCount(); }
        uint32 getOptionalParameterCount()   
                                        { return mFunction->getOptionalParameterCount(); }
        uint32 getNamedParameterCount()  { return mFunction->getNamedParameterCount(); }
        virtual bool parameterIsRequired(uint32 index)  
                                        { return mFunction->parameterIsRequired(index); }
        virtual bool parameterIsOptional(uint32 index)  
                                        { return mFunction->parameterIsOptional(index); }
        virtual bool parameterIsNamed(uint32 index)  
                                        { return mFunction->parameterIsNamed(index); }
        virtual uint32 getRestParameterIndex()
                                        { return mFunction->getRestParameterIndex(); }
        virtual const String *getParameterName(uint32 index)
                                        { return mFunction->getParameterName(index); }
        bool isChecked()                { return mFunction->isChecked(); }
        bool hasRestParameter()         { return mFunction->hasRestParameter(); }

        void getProperty(Context *cx, const String &name, NamespaceList *names) 
                                        { mFunction->getProperty(cx, name, names); }
        void setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v)
                                        { mFunction->setProperty(cx, name, names, v); }
        bool hasProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
                                        { return mFunction->hasProperty(cx, name, names, acc, p); }
        bool hasOwnProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
                                        { return mFunction->hasOwnProperty(cx, name, names, acc, p); }
        PropertyIterator findNamespacedProperty(const String &name, NamespaceList *names)
                                        { return mFunction->findNamespacedProperty(name, names); }
        bool deleteProperty(Context *cx, const String &name, NamespaceList *names)
                                        { return mFunction->deleteProperty(cx, name, names); }

        JSFunction *getFunction()       { return mFunction; }

    protected:
        typedef Collector::InstanceOwner<JSBoundFunction> JBoundFunctionOwner;
        friend class Collector::InstanceOwner<JSBoundFunction>;
        /**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* collector)
        {
            JSFunction::scan(collector);
            // copy the appropriate members.
            mFunction = (JSFunction*) collector->copy(mFunction);
            return sizeof(JSBoundFunction);
        }

    public:
        void* operator new(size_t n, Collector& gc)
        {
            static JBoundFunctionOwner owner;
            return gc.allocateObject(n, &owner);
        }

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSBoundFunction", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSBoundFunction", t); STD::free(t); }
#endif
    };

    // This is for unary & binary operators, it collects together the operand
    // types and the function pointer for the given operand. See also
    // Context::initOperators where the default operators are set up.
    class OperatorDefinition {
    public:
        
        OperatorDefinition(JSType *type1, JSType *type2, JSFunction *imp)
            : mType1(type1), mType2(type2), mImp(imp) { ASSERT(mType1); ASSERT(mType2); }

        OperatorDefinition(JSType *type1, JSFunction *imp)
            : mType1(type1), mImp(imp) { ASSERT(mType1); }

        JSType *mType1;
        JSType *mType2;
        JSFunction *mImp;

        // see if this operator is applicable when 
        // being invoked by the given types
        bool isApplicable(JSType *tx, JSType *ty)
        {
            return ( ((tx == mType1) || tx->derivesFrom(mType1))
                        && 
                      ((ty == mType2) || ty->derivesFrom(mType2)) );
        }

        bool isApplicable(JSType *tx)
        {
            return ( (tx == mType1) || tx->derivesFrom(mType1) );
        }

    };



    // provide access to the Error object constructors so that runtime exceptions
    // can be constructed for Javascript catches.
    extern js2val Error_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    extern js2val EvalError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    extern js2val RangeError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    extern js2val ReferenceError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    extern js2val SyntaxError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    extern js2val TypeError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    extern js2val UriError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);

    // called by bytecodegen for RegExp literals
    extern js2val RegExp_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);
    
    // called directly by String.match
    extern js2val RegExp_exec(Context *cx, const js2val thisValue, js2val *argv, uint32 argc);


    class Package : public JSObject  {
    public:
        typedef enum { OnItsWay, InHand } PackageStatus;
        
        Package(const String &name) : JSObject(), mName(name), mStatus(OnItsWay) { }

        String mName;
        PackageStatus mStatus;
    };

    typedef std::vector<Package *> PackageList;

#define PACKAGE_NAME(pi) ((*pi)->mName)
#define PACKAGE_STATUS(pi) ((*pi)->mStatus)
    
    class Context {
    public:
        struct ProtoFunDef {
            char *name;
            JSType *result;
            uint32 length;
            JSFunction::NativeCode *imp;
        };
        class PrototypeFunctions {
        public:
            PrototypeFunctions(ProtoFunDef *p)
            {
                uint32 count = 0;
                mDef = NULL;
                if (p) {
                    while (p[count].name) count++;
                    mDef = new ProtoFunDef[count];
                    for (uint32 i = 0; i < count; i++)
                        mDef[i] = *p++;
                }
                mCount = count;
            }
            ~PrototypeFunctions()
            {
                if (mDef) delete mDef;
            }
            ProtoFunDef *mDef;
            uint32 mCount;
        };

        struct ClassDef {
            char *name;
            JSFunction::NativeCode *defCon;
            const js2val *uninit;
        };

        Context(JSObject **global, World &world, Arena &a, Pragma::Flags flags);

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("Context", s, t); return t; }
        void operator delete(void* t)   { trace_release("Context", t); STD::free(t); }
#endif
        //
        // Initialize a bunch of useful string atoms to save 
        // having to look them up during execution.
        //
        // Should these be in World instead?
        //
        StringAtom& Virtual_StringAtom; 
        StringAtom& Constructor_StringAtom; 
        StringAtom& Operator_StringAtom; 
        StringAtom& Fixed_StringAtom;
        StringAtom& Dynamic_StringAtom;
        StringAtom& Extend_StringAtom;
        StringAtom& Prototype_StringAtom;
        StringAtom& Forin_StringAtom;
        StringAtom& Value_StringAtom;
        StringAtom& Next_StringAtom;
        StringAtom& Done_StringAtom;
        StringAtom& Undefined_StringAtom;
        StringAtom& Object_StringAtom;
        StringAtom& Boolean_StringAtom;
        StringAtom& Number_StringAtom;
        StringAtom& String_StringAtom;
        StringAtom& Function_StringAtom;
        StringAtom& HasInstance_StringAtom;
        StringAtom& True_StringAtom;
        StringAtom& False_StringAtom;
        StringAtom& Null_StringAtom;
        StringAtom& ToString_StringAtom;
        StringAtom& ValueOf_StringAtom;
        StringAtom& Length_StringAtom;
        StringAtom& FromCharCode_StringAtom;
        StringAtom& Math_StringAtom;
        StringAtom& NaN_StringAtom;
        StringAtom& Eval_StringAtom;
        StringAtom& Infinity_StringAtom;
        StringAtom& Empty_StringAtom;
        StringAtom& Arguments_StringAtom;
        StringAtom& Message_StringAtom;
        StringAtom& Name_StringAtom;
        StringAtom& Error_StringAtom;
        StringAtom& EvalError_StringAtom;
        StringAtom& RangeError_StringAtom;
        StringAtom& ReferenceError_StringAtom;
        StringAtom& SyntaxError_StringAtom;
        StringAtom& TypeError_StringAtom;
        StringAtom& UriError_StringAtom;
        StringAtom& Source_StringAtom;
        StringAtom& Global_StringAtom;
        StringAtom& IgnoreCase_StringAtom;
        StringAtom& Multiline_StringAtom;
        StringAtom& Input_StringAtom;
        StringAtom& Index_StringAtom;
        StringAtom& LastIndex_StringAtom;
        StringAtom& LastMatch_StringAtom;
        StringAtom& LastParen_StringAtom;
        StringAtom& LeftContext_StringAtom;
        StringAtom& RightContext_StringAtom;
	StringAtom& Dollar_StringAtom;
        StringAtom& UnderbarPrototype_StringAtom;

        void initBuiltins();
        void initClass(JSType *type, ClassDef *cdef, PrototypeFunctions *pdef);
        void initOperators();
        void initAttributeValue(char *name, uint32 trueFlags, uint32 falseFlags);
        

        void defineOperator(Operator which, JSType *t1, JSType *t2, JSFunction *imp)
        {
            OperatorDefinition *op = new OperatorDefinition(t1, t2, imp);
            mBinaryOperatorTable[which].push_back(op);
        }

        void defineOperator(Operator which, JSType *t, JSFunction *imp)
        {
            OperatorDefinition *op = new OperatorDefinition(t, imp);
            mUnaryOperatorTable[which].push_back(op);
        }

        // Construct an array of argument values, updating argCount to
        // reflect the final size. Pulls incoming args from the top of
        // the stack.
        js2val *buildArgumentBlock(JSFunction *target, uint32 &argCount);

        
        // compiles attribute expression into an attribute object
        // which is stored back into the statement node.
        void setAttributeValue(AttributeStmtNode *s, PropertyAttribute defaultValue);
        Attribute *executeAttributes(ExprNode *attr);


        // Run the binary operator designated, after using the types to do the dispatch
        bool executeOperator(Operator op, JSType *t1, JSType *t2);
        js2val mapValueToType(js2val v, JSType *t);

        // Run the interpreter loop on the function
        js2val invokeFunction(JSFunction *target, const js2val thisValue, js2val *argv, uint32 argc);

        // This reader is used to generate source information
        // to go with exception messages.
        void setReader(Reader *r)           { mReader = r; }

        JSObject *getGlobalObject()         { ASSERT(mGlobal); return *mGlobal; }

        World &mWorld;
        ScopeChain *mScopeChain;
        Arena &mArena;
        Pragma::Flags mFlags;           // The flags to use for the next parse; updated by the parser
        bool mDebugFlag;

        // the currently executing 'function'
        ByteCodeModule *mCurModule;

        uint8 *mPC;

        js2val mThis;

        // this is the execution stack (for the current function)
        js2val *mStack;
        uint32 mStackTop;
        uint32 mStackMax;

        NamespaceList *mNamespaceList;

        void pushValue(const js2val v)
        {
            ASSERT(mStackTop < mStackMax);
            mStack[mStackTop++] = v;
        }

        js2val popValue()
        {
            ASSERT(mStackTop > 0);
            return mStack[--mStackTop];
        }

        js2val topValue()
        {
            return mStack[mStackTop - 1];
        }

        void resizeStack(uint32 n)
        {
            ASSERT(n <= mStackMax);
            mStackTop = n;
        }

        uint32 stackSize()
        {
            return mStackTop;
        }

        js2val getValue(uint32 n)
        {
            ASSERT(n < mStackTop);
            return mStack[n];
        }

        // put the value in at 'index', lifting everything above that up by one
        void insertValue(js2val v, uint32 index)
        {
            ASSERT(mStackTop < mStackMax);      // we're effectively pushing one entry
            for (uint32 i = mStackTop - 1; i >= index; i--)
                mStack[i + 1] = mStack[i];
            mStack[index] = v;
            mStackTop++;
        }

        js2val *getBase(uint32 n)
        {
            ASSERT(n <= mStackTop);     // s'ok to point beyond the end
            return &mStack[n];
        }

        void assureStackSpace(uint32 s);


        // the activation stack
        std::stack<Activation *> mActivationStack;

        struct HandlerData {
            HandlerData(uint8 *pc, uint32 stackSize, Activation *curAct) 
                : mPC(pc), mStackSize(stackSize), mActivation(curAct) { }

            uint8 *mPC;
            uint32 mStackSize;
            Activation *mActivation;
        };

        std::stack<HandlerData *> mTryStack;
        std::stack<uint8 *> mSubStack;

        // the locals for the current function (an array, constructed on function entry)
        js2val *mLocals;

        // the base of the incoming arguments for this function
        js2val *mArgumentBase;

        typedef std::vector<OperatorDefinition *> OperatorList;
            
        // XXX bigger than necessary...
        OperatorList mBinaryOperatorTable[OperatorCount];
        OperatorList mUnaryOperatorTable[OperatorCount];
        

        PackageList mPackages;  // the currently loaded packages, mPackages.back() is the current package
        bool checkForPackage(const String &packageName);    // return true if loaded, throw exception if loading
        void loadPackage(const String &packageName, const String &filename);  // load package from file

        js2val readEvalFile(const String& fileName);
        js2val readEvalString(const String &str, const String& fileName, ScopeChain *scopeChain, const js2val thisValue);

        void buildRuntime(StmtNode *p);
        void buildRuntimeForFunction(FunctionDefinition &f, JSFunction *fnc);
        void buildRuntimeForStmt(StmtNode *p);
        ByteCodeModule *genCode(StmtNode *p, const String &sourceName);

        js2val interpret(ByteCodeModule *bcm, int offset, ScopeChain *scopeChain, const js2val thisValue, js2val *argv, uint32 argc);
        js2val interpret(uint8 *pc, uint8 *endPC);
        
        void reportError(Exception::Kind kind, char *message, size_t pos, const char *arg = NULL);
        void reportError(Exception::Kind kind, char *message, const char *arg = NULL);
        void reportError(Exception::Kind kind, char *message, const String& name);
        void reportError(Exception::Kind kind, char *message, size_t pos, const String& name);

        JSFunction *getUnaryOperator(JSType *dispatchType, Operator which);

        
        /* utility routines */

        // Extract the operator from the string literal function name
        // - requires the paramter count in order to distinguish
        // between unary and binary operators.
        Operator getOperator(uint32 parameterCount, const String &name);

        // Get the type of the nth parameter.
        JSType *getParameterType(FunctionDefinition &function, int index);


        Reader *mReader;


        void *mErrorReporter;
        void *argumentFormatMap;

    private:
        JSObject **mGlobal;


    };

    /*
        (a local instance of) This class is used when a function in the
        interpreter execution codepath may need to re-invoke the interpreter
        (by calling an internal method that MAY have an override). The stack
        replacement simply inserts a stack big enough for whatever action is
        about to occur.
    */
    class ContextStackReplacement {
    public:
        enum { ReplacementStackSize = 4 };
        ContextStackReplacement(Context *cx) 
        { 
            m_cx = cx;
            mOldStack = cx->mStack; 
            mOldStackTop = cx->mStackTop; 
            mOldStackMax = cx->mStackMax; 
            cx->mStack = &mStack[0];
            cx->mStackTop = 0;
            cx->mStackMax = ReplacementStackSize;
        }

        ~ContextStackReplacement()
        {
            m_cx->mStack = mOldStack;
            m_cx->mStackTop = mOldStackTop;
            m_cx->mStackMax = mOldStackMax;
        }

        js2val mStack[ReplacementStackSize];

        Context *m_cx;

        js2val *mOldStack;
        uint32 mOldStackTop;
        uint32 mOldStackMax;

    };

    
    class NamedArgument : public JSObject {
    public:
        NamedArgument(js2val v, const String *n) : mValue(v), mName(n) { }

        js2val mValue;
        const String *mName;

    protected:
        typedef Collector::InstanceOwner<NamedArgument> NamedArgumentOwner;
        friend class Collector::InstanceOwner<NamedArgument>;
        /**
        * Scans through the object, and copies all references.
        */
#ifdef NOT_SCARED_OF_GC_CODE    
        Collector::size_type scan(Collector* collector)
        {
            mValue.scan(collector);
            return sizeof(NamedArgument);
        }
    public:
        void* operator new(size_t n, Collector& gc)
        {
            static NamedArgumentOwner owner;
            return gc.allocateObject(n, &owner);
        }
#endif

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("NamedArgument", s, t); return t; }
        void operator delete(void* t)   { trace_release("NamedArgument", t); STD::free(t); }
#endif
    };
    
    
    class Attribute : public JSObject {
    public:
        Attribute(PropertyAttribute t, PropertyAttribute f)
            : mTrueFlags(t), mFalseFlags(f), mExtendArgument(NULL), mNamespaceList(NULL) { }

        PropertyAttribute mTrueFlags;
        PropertyAttribute mFalseFlags;

        JSType *mExtendArgument;
        NamespaceList *mNamespaceList;

    protected:
        typedef Collector::InstanceOwner<Attribute> AttributeOwner;
        friend class Collector::InstanceOwner<Attribute>;
        /**
        * Scans through the object, and copies all references.
        */
        Collector::size_type scan(Collector* collector)
        {
            mExtendArgument = (JSType*) collector->copy(mExtendArgument);
            return sizeof(Attribute);
        }

    public:
        void* operator new(size_t n, Collector& gc)
        {
            static AttributeOwner owner;
            return gc.allocateObject(n, &owner);
        }

#ifdef DEBUG
    public:
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("Attribute", s, t); return t; }
        void operator delete(void* t)   { trace_release("Attribute", t); STD::free(t); }
#endif
    };
    
    
    
    
    
    inline bool JSValue::isInstance(const js2val v)              { return JS2VAL_IS_OBJECT(v) && dynamic_cast<JSInstance *>(JS2VAL_TO_OBJECT(v)); }
    inline bool JSValue::isType(const js2val v)                  { return JS2VAL_IS_OBJECT(v) && dynamic_cast<JSType *>(JS2VAL_TO_OBJECT(v)); }
    inline bool JSValue::isFunction(const js2val v)              { return JS2VAL_IS_OBJECT(v) && dynamic_cast<JSFunction *>(JS2VAL_TO_OBJECT(v)); }
    inline bool JSValue::isNamedArg(const js2val v)              { return JS2VAL_IS_OBJECT(v) && dynamic_cast<NamedArgument *>(JS2VAL_TO_OBJECT(v)); }
    inline bool JSValue::isAttribute(const js2val v)             { return JS2VAL_IS_OBJECT(v) && dynamic_cast<Attribute *>(JS2VAL_TO_OBJECT(v)); }
    inline bool JSValue::isPackage(const js2val v)               { return JS2VAL_IS_OBJECT(v) && dynamic_cast<Package *>(JS2VAL_TO_OBJECT(v)); }
    
    
    
    inline AccessorReference::AccessorReference(JSFunction *f, PropertyAttribute attr)
        : Reference(f->getResultType(), attr), mFunction(f) 
    {
    }
  
    inline FunctionReference::FunctionReference(JSFunction *f, PropertyAttribute attr) 
            : Reference(f->getResultType(), attr), mFunction(f)
    {
    }

    inline GetterFunctionReference::GetterFunctionReference(JSFunction *f, PropertyAttribute attr) 
            : Reference(f->getResultType(), attr), mFunction(f)
    {
    }

    inline SetterFunctionReference::SetterFunctionReference(JSFunction *f, JSType *type, PropertyAttribute attr) 
            : Reference(type, attr), mFunction(f)
    {
    }

    inline void JSType::addStaticMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
    {
        defineStaticMethod(cx, name, attr, f);
    }

    inline void JSType::addMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
    {
        defineMethod(cx, name, attr, f);
    }

    inline bool JSInstance::isDynamic()
    {
        return mType->isDynamic(); 
    }

    inline void ScopeChain::defineUnaryOperator(Context *cx, Operator which, JSFunction *f)
    {
        JSObject *top = mScopeStack.back();
        ASSERT(dynamic_cast<JSType *>(top));
        cx->defineOperator(which, (JSType *)top, f);
    }

}
}

#endif //js2runtime_h___
