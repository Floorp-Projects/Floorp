/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

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
    extern JSType *NamedArgument_Type;

    extern JSType *Date_Type;


    const String *numberToString(float64 number);
    float64 stringToNumber(const String *string);


    class JSValue {
    public:
        union {
            float64 f64;
            JSObject *object;
            JSFunction *function;
            const String *string;
            JSType *type;
            bool boolean;
        };
        
        typedef enum {
            undefined_tag, 
            f64_tag,
            object_tag,
            function_tag,
            type_tag,
            boolean_tag,
            string_tag,
            null_tag
        } Tag;
        Tag tag;
        
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSValue", s, t); return t; }
        void operator delete(void* t) { trace_release("JSValue", t); STD::free(t); }
#endif

        JSValue() : f64(0.0), tag(undefined_tag) {}
        explicit JSValue(float64 f64) : f64(f64), tag(f64_tag) {}
        explicit JSValue(JSObject *object) : object(object), tag(object_tag) {}
        explicit JSValue(JSFunction *function) : function(function), tag(function_tag) {}
        explicit JSValue(JSType *type) : type(type), tag(type_tag) {}
        explicit JSValue(const String *string) : string(string), tag(string_tag) {}
        explicit JSValue(bool boolean) : boolean(boolean), tag(boolean_tag) {}
        explicit JSValue(Tag tag) : tag(tag) {}

        float64& operator=(float64 f64)                 { return (tag = f64_tag, this->f64 = f64); }
        JSObject*& operator=(JSObject* object)          { return (tag = object_tag, this->object = object); }
        JSType*& operator=(JSType* type)                { return (tag = type_tag, this->type = type); }
        JSFunction*& operator=(JSFunction* function)    { return (tag = function_tag, this->function = function); }
        bool& operator=(bool boolean)                   { return (tag = boolean_tag, this->boolean = boolean); }
        
        bool isObject() const                           { return (tag == object_tag); }
        bool isNumber() const                           { return (tag == f64_tag); }
        bool isBool() const                             { return (tag == boolean_tag); }
        bool isType() const                             { return (tag == type_tag); }
        bool isFunction() const                         { return (tag == function_tag); }
        bool isString() const                           { return (tag == string_tag); }
        bool isPrimitive() const                        { return (tag != object_tag) && (tag != type_tag) && (tag != function_tag); }

        bool isUndefined() const                        { return (tag == undefined_tag); }
        bool isNull() const                             { return (tag == null_tag); }
        bool isNaN() const                              { ASSERT(isNumber()); return JSDOUBLE_IS_NaN(f64); }
        bool isNegativeInfinity() const                 { ASSERT(isNumber()); return (f64 < 0) && JSDOUBLE_IS_INFINITE(f64); }
        bool isPositiveInfinity() const                 { ASSERT(isNumber()); return (f64 > 0) && JSDOUBLE_IS_INFINITE(f64); }
        bool isNegativeZero() const                     { ASSERT(isNumber()); return JSDOUBLE_IS_NEGZERO(f64); }
        bool isPositiveZero() const                     { ASSERT(isNumber()); return JSDOUBLE_IS_POSZERO(f64); }

        bool isTrue() const                             { ASSERT(isBool()); return boolean; }
        bool isFalse() const                            { ASSERT(isBool()); return !boolean; }

        JSType *getType() const;

        JSValue toString(Context *cx) const             { return (isString() ? *this : valueToString(cx, *this)); }
        JSValue toNumber(Context *cx) const             { return (isNumber() ? *this : valueToNumber(cx, *this)); }
        JSValue toInteger(Context *cx) const            { return valueToInteger(cx, *this); }
        JSValue toUInt32(Context *cx) const             { return valueToUInt32(cx, *this); }
        JSValue toUInt16(Context *cx) const             { return valueToUInt16(cx, *this); }
        JSValue toInt32(Context *cx) const              { return valueToInt32(cx, *this); }
        JSValue toObject(Context *cx) const             { return ((isObject() || isType() || isFunction()) ?
                                                                *this : valueToObject(cx, *this)); }
        JSValue toBoolean(Context *cx) const            { return (isBool() ? *this : valueToBoolean(cx, *this)); }

        float64 getNumberValue() const;
        const String *getStringValue() const;
        bool getBoolValue() const;

        /* These are for use in 'toPrimitive' calls */
        enum Hint {
            NumberHint, StringHint, NoHint
        };
        JSValue toPrimitive(Context *cx, Hint hint = NoHint) const;
        
        static JSValue valueToNumber(Context *cx, const JSValue& value);
        static JSValue valueToInteger(Context *cx, const JSValue& value);
        static JSValue valueToString(Context *cx, const JSValue& value);
        static JSValue valueToObject(Context *cx, const JSValue& value);
        static JSValue valueToUInt32(Context *cx, const JSValue& value);
        static JSValue valueToUInt16(Context *cx, const JSValue& value);
        static JSValue valueToInt32(Context *cx, const JSValue& value);
        static JSValue valueToBoolean(Context *cx, const JSValue& value);
        

        static float64 float64ToInteger(float64 d);

        int operator==(const JSValue& value) const;

    };
    Formatter& operator<<(Formatter& f, const JSValue& value);

    extern JSValue kUndefinedValue;
    extern JSValue kNaNValue;
    extern JSValue kTrueValue;
    extern JSValue kFalseValue;
    extern JSValue kNullValue;
    extern JSValue kNegativeZero;
    extern JSValue kPositiveZero;
    extern JSValue kNegativeInfinity;
    extern JSValue kPositiveInfinity;
    

    
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
        PropertyReference(const String& name, Access acc, JSType *type, PropertyAttribute attr)
            : Reference(type, attr), mAccess(acc), mName(name) { }
        Access mAccess;
        const String& mName;
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
        NameReference(const String& name, Access acc)
            : Reference(Object_Type, 0), mAccess(acc), mName(name) { }
        NameReference(const String& name, Access acc, JSType *type, PropertyAttribute attr)
            : Reference(type, attr), mAccess(acc), mName(name) { }
        Access mAccess;
        const String& mName;
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
        JSObject(JSType *type = Object_Type) : mType(type), mPrivate(NULL), mPrototype(NULL) { }
        
        virtual ~JSObject() { } // keeping gcc happy
        
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSObject", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSObject", t); STD::free(t); }
#endif

        // every object has a type
        JSType        *mType;

        // the property data is kept (or referenced from) here
        PropertyMap   mProperties;

        // Every JSObject has a private part
        void          *mPrivate;

        // Every JSObject (except the Ur-object) has a prototype
        JSObject      *mPrototype;

        JSType *getType() const { return mType; }

        virtual bool isDynamic() { return true; }


        // find a property by the given name, and then check to see if there's any
        // overlap between the supplied attribute list and the property's list.
        // ***** REWRITE ME -- matching attribute lists for inclusion is a bad idea.
        PropertyIterator findNamespacedProperty(const String &name, NamespaceList *names);

        void deleteProperty(const String &name, NamespaceList *names)
        {
            PropertyIterator i = findNamespacedProperty(name, names);
            mProperties.erase(i);
        }

        // see if the property exists by a specific kind of access
        bool hasOwnProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p);
        
        virtual bool hasProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p);

        virtual JSValue getPropertyValue(PropertyIterator &i);

        virtual void getProperty(Context *cx, const String &name, NamespaceList *names);

        virtual void setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v);



        // add a property
        virtual Property *defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type);
        virtual Property *defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type);

        // add a property/value into the map 
        // - assumes the map doesn't already have this property
        Property *insertNewProperty(const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const JSValue &v);

        virtual Property *defineStaticVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type)
        {
            return JSObject::defineVariable(cx, name, attr, type);   
            // XXX or error?  (Note that this implementation is invoked by JSType::defineStaticXXXX
            // - the question is, is static var X (etc) at global scope an error?
        }
        // add a method property
        virtual void defineMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            defineVariable(cx, name, attr, Function_Type, JSValue(f));
        }
        virtual void defineStaticMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineVariable(cx, name, attr, Function_Type, JSValue(f));    // XXX or error?
        }
        virtual void defineConstructor(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
        {
            JSObject::defineVariable(cx, name, attr, Function_Type, JSValue(f));    // XXX or error?
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

        virtual Property *defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type, const JSValue v);
        virtual Property *defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const JSValue v);
        
        virtual Reference *genReference(bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        virtual JSType *topClass()                      { return NULL; }
        virtual bool isNestedFunction()                 { return false; }
        virtual JSFunction *getContainerFunction()      { return NULL; }
        
        virtual bool hasLocalVars()     { return false; }
        virtual uint32 localVarCount()  { return 0; }

        virtual void defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type);

        virtual JSValue getSlotValue(Context * /*cx*/, uint32 /*slotIndex*/)    { ASSERT(false); return kUndefinedValue; }

        // debug only        
        void printProperties(Formatter &f) const
        {
            for (PropertyMap::const_iterator i = mProperties.begin(), end = mProperties.end(); (i != end); i++) 
            {
                f << "[" << PROPERTY_NAME(i) << "] " << *PROPERTY(i);
            }
        }

        static uint32 tempVarCount;
    };

    Formatter& operator<<(Formatter& f, const JSObject& obj);
    


        
    
    
    
    
    

    
 


    class JSInstance : public JSObject {
    public:
        
        JSInstance(Context *cx, JSType *type) 
            : JSObject(type), mInstanceValues(NULL) { if (type) initInstance(cx, type); }

        virtual ~JSInstance() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSInstance", t); STD::free(t); }
#endif

        void initInstance(Context *cx, JSType *type);

        void getProperty(Context *cx, const String &name, NamespaceList *names);
        void setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v);

        JSValue getField(uint32 index)
        {
            return mInstanceValues[index];
        }

        void setField(uint32 index, JSValue v)
        {
            mInstanceValues[index] = v;
        }

        virtual bool isDynamic();


        JSValue         *mInstanceValues;
    };
    Formatter& operator<<(Formatter& f, const JSInstance& obj);

    

    typedef std::vector<JSFunction *> MethodList;

    class ScopeChain;










    class JSType : public JSObject {
    public:        
        JSType(Context *cx, const StringAtom *name, JSType *super, JSObject *protoObj = NULL);
        JSType(JSType *xClass);     // used for constructing the static component type

        virtual ~JSType() { }       // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSType", t); STD::free(t); }
#endif


        void setSuperType(JSType *super);

        void setStaticInitializer(Context *cx, JSFunction *f);
        void setInstanceInitializer(Context *cx, JSFunction *f);

        virtual JSType *topClass()      { return this; }


        // construct a new (empty) instance of this class
        virtual JSInstance *newInstance(Context *cx);
           

        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type);


   // XXX
   // XXX why doesn't the virtual function in JSObject get found?
   // XXX
        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type, JSValue v)
        {
            return JSObject::defineVariable(cx, name, attr, type, v);
        }




        void defineMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f);
        
        void defineGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        void defineSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        void defineUnaryOperator(Operator which, JSFunction *f)
        {
            mUnaryOperators[which] = f;
        }

        JSFunction *getUnaryOperator(Operator which)
        {
            return mUnaryOperators[which];      // XXX Umm, aren't these also getting inherited? 
        }

        void setDefaultConstructor(Context * /*cx*/, JSFunction *f)
        {
            mDefaultConstructor = f;
        }

        void addMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);
        void addStaticMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f);

        // return true if 'other' is on the chain of supertypes
        bool derivesFrom(JSType *other);

        virtual void getProperty(Context *cx, const String &name, NamespaceList *names);
        virtual void setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v);

        virtual JSValue getPropertyValue(PropertyIterator &i);

        virtual bool hasProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p);

        virtual Reference *genReference(bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        JSFunction *getDefaultConstructor() { return mDefaultConstructor; }
        JSFunction *getTypeCastFunction()   { return mTypeCast; }

        JSValue getUninitializedValue()    { return mUninitializedValue; }

        // assumes that the super types have been completed already
        void completeClass(Context *cx, ScopeChain *scopeChain);

        virtual bool isDynamic() { return mIsDynamic; }

        JSType          *mSuperType;        // NULL implies that this is the base Object

        uint32          mVariableCount;
        JSFunction      *mInstanceInitializer;
        JSFunction      *mDefaultConstructor;
        JSFunction      *mTypeCast;

        // the 'vtable'
        MethodList          mMethods;
        const StringAtom    *mClassName;
        const StringAtom    *mPrivateNamespace;

        JSFunction      *mUnaryOperators[OperatorCount];    // XXX too wasteful

        bool            mIsDynamic;
        JSValue         mUninitializedValue;            // the value for uninitialized vars

        JSObject        *mPrototypeObject;              // becomes the prototype for any instance


        void printSlotsNStuff(Formatter& f) const;

    };

    Formatter& operator<<(Formatter& f, const JSType& obj);

    class JSArrayInstance : public JSInstance {
    public:
        JSArrayInstance(Context *cx, JSType * /*type*/) : JSInstance(cx, NULL), mLength(0) { mType = (JSType *)Array_Type; }
        virtual ~JSArrayInstance() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSArrayInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSArrayInstance", t); STD::free(t); }
#endif

        // XXX maybe could have implemented length as a getter/setter pair?
        void setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v);
        void getProperty(Context *cx, const String &name, NamespaceList *names);


        uint32 mLength;

    };

    class JSArrayType : public JSType {
    public:
        JSArrayType(Context *cx, JSType *elementType, const StringAtom *name, JSType *super, JSObject *protoObj = NULL) 
            : JSType(cx, name, super, protoObj), mElementType(elementType)
        {
        }
        virtual ~JSArrayType() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSArrayType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSArrayType", t); STD::free(t); }
#endif

        JSInstance *newInstance(Context *cx);

        JSType *mElementType;

    };

    class JSStringInstance : public JSInstance {
    public:
        JSStringInstance(Context *cx, JSType * /*type*/) : JSInstance(cx, NULL), mLength(0) { mType = (JSType *)String_Type; }
        virtual ~JSStringInstance() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSStringInstance", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSStringInstance", t); STD::free(t); }
#endif

        void getProperty(Context *cx, const String &name, NamespaceList *names);


        uint32 mLength;

    };

    class JSStringType : public JSType {
    public:
        JSStringType(Context *cx, const StringAtom *name, JSType *super) 
            : JSType(cx, name, super)
        {
        }
        virtual ~JSStringType() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSStringType", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSStringType", t); STD::free(t); }
#endif

        JSInstance *newInstance(Context *cx);

    };





    // captures the Parameter names scope
    // it's a JSType simply because it's also a thing that
    // maps from names to slots.
    class ParameterBarrel : public JSType {
    public:

        ParameterBarrel() : JSType(NULL) 
        {
        }
        virtual ~ParameterBarrel() { } // keeping gcc happy

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("ParameterBarrel", s, t); return t; }
        void operator delete(void* t)   { trace_release("ParameterBarrel", t); STD::free(t); }
#endif

        Reference *genReference(bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

        JSValue getSlotValue(Context *cx, uint32 slotIndex);

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
                    : JSType(NULL), 
                        mLocals(NULL), 
                        mStack(NULL),
                        mStackTop(0),
                        mPC(0), 
                        mModule(NULL),
                        mContainer(NULL) { }

        Activation(JSValue *locals, 
                        JSValue *stack, uint32 stackTop,
                        ScopeChain *scopeChain,
                        JSValue *argBase, JSValue curThis,
                        uint8 *pc, 
                        ByteCodeModule *module )
                    : JSType(NULL), 
                        mLocals(locals), 
                        mStack(stack), 
                        mStackTop(stackTop),
                        mScopeChain(scopeChain),
                        mArgumentBase(argBase), 
                        mThis(curThis), 
                        mPC(pc), 
                        mModule(module),
                        mContainer(NULL)       { }

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
        JSValue *mLocals;
        JSValue *mStack;
        uint32 mStackTop;           
        ScopeChain *mScopeChain;
        JSValue *mArgumentBase;
        JSValue mThis;
        uint8 *mPC;
        ByteCodeModule *mModule;
        JSFunction *mContainer;

        virtual JSFunction *getContainerFunction()      { return mContainer; }


        bool hasLocalVars()             { return true; }
        virtual uint32 localVarCount()  { return mVariableCount; }

        void defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type);

        Reference *genReference(bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth);

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
        Property *defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type, JSValue v)
        {
            JSObject *top = mScopeStack.back();
            return top->defineVariable(cx, name, attr, type, v);
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
        void defineUnaryOperator(Operator which, JSFunction *f)
        {
            JSObject *top = mScopeStack.back();
            ASSERT(dynamic_cast<JSType *>(top));
            ((JSType *)top)->defineUnaryOperator(which, f);
        }

        // see if the current scope contains a name already
        bool hasProperty(const String& name, NamespaceList *names, Access acc, PropertyIterator *p)
        {
            JSObject *top = mScopeStack.back();
            return top->hasProperty(name, names, acc, p);
        }

        bool hasOwnProperty(const String& name, NamespaceList *names, Access acc, PropertyIterator *p)
        {
            JSObject *top = mScopeStack.back();
            return top->hasOwnProperty(name, names, acc, p);
        }

        // delete a property from the top object (already know it's there)
        void deleteProperty(const String &name, NamespaceList *names)
        {
            JSObject *top = mScopeStack.back();
            top->deleteProperty(name, names);
        }

        // generate a reference to the given name
        Reference *getName(const String& name, NamespaceList *names, Access acc);

        bool hasNameValue(const String& name, NamespaceList *names);

        // pushes the value of the name and returns it's container object
        JSObject *getNameValue(Context *cx, const String& name, AttributeStmtNode *attr);

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

	bool isPossibleUncheckedFunction(FunctionDefinition *f);

        void defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type)
        {
            mScopeStack.back()->defineTempVariable(cx, readRef, writeRef, type);
        }

        // a compile time request to get the value for a name
        // (i.e. we're accessing a constant value)
        JSValue getCompileTimeValue(const String& name, NamespaceList *names);



        void setNameValue(Context *cx, const String& name, AttributeStmtNode *attr);

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
        JSType *findType(const StringAtom& typeName, size_t pos);

        // Get a type from an ExprNode 
        JSType *extractType(ExprNode *t);


    };


    class JSFunction : public JSObject {
    protected:
        JSFunction() : JSObject(Function_Type), mActivation() { mActivation.mContainer = this; mPrototype = Function_Type->mPrototypeObject; }        // for JSBoundFunction (XXX ask Patrick about this structure)
    public:

        class ArgumentData {
        public:
            ArgumentData() : mName(NULL), mType(NULL), mInitializer((uint32)(-1)) { }
            const String *mName;
            JSType *mType;
            uint32 mInitializer;
        };


        typedef JSValue (NativeCode)(Context *cx, const JSValue &thisValue, JSValue argv[], uint32 argc);

        // XXX these should be Function_Type->newInstance() calls, no?

        JSFunction(Context *cx, JSType *resultType, ScopeChain *scopeChain);        
        JSFunction(Context *cx, NativeCode *code, JSType *resultType);

        ~JSFunction() { }  // keeping gcc happy
        
#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSFunction", s, t); return t; }
        void operator delete(void* t)   { trace_release("JSFunction", t); STD::free(t); }
#endif

        void setByteCode(ByteCodeModule *b)     { ASSERT(!isNative()); mByteCode = b; }
        void setResultType(JSType *r)           { mResultType = r; }
        void setArgCounts(Context *cx, uint32 r, uint32 o, bool hasRest);
        void setArgument(uint32 index, const String *n, JSType *t)
                                                { ASSERT(mArguments && (index < (mRequiredArgs + mOptionalArgs + ((mHasRestParameter) ? 1 : 0) ))); mArguments[index].mType = t; mArguments[index].mName = n; }
        void setArgumentInitializer(uint32 index, uint32 offset)
                                                { ASSERT(mArguments && (index < (mRequiredArgs + mOptionalArgs + ((mHasRestParameter) ? 1 : 0) ))); mArguments[index].mInitializer = offset; }

        void setIsPrototype(bool i)             { mIsPrototype = i; }
        void setIsConstructor(bool i)           { mIsConstructor = i; }
        void setIsUnchecked()                   { mIsChecked = false; }
        void setFunctionName(FunctionName *n)   { mFunctionName = new FunctionName(); mFunctionName->prefix = n->prefix; mFunctionName->name = n->name; }
        void setFunctionName(const StringAtom *n)
                                                { mFunctionName = new FunctionName(); mFunctionName->name = n; }
        void setClass(JSType *c)                { mClass = c; }

        virtual bool hasBoundThis()             { return false; }
        virtual bool isNative()                 { return (mCode != NULL); }
        virtual bool isPrototype()              { return mIsPrototype; }
        virtual bool isConstructor()            { return mIsConstructor; }
        bool isMethod()                         { return (mClass != NULL); }
        virtual ByteCodeModule *getByteCode()   { ASSERT(!isNative()); return mByteCode; }
        virtual NativeCode *getNativeCode()     { ASSERT(isNative()); return mCode; }
        virtual ParameterBarrel *getParameterBarrel()
                                                { return mParameterBarrel; }

        virtual JSType *getResultType()         { return mResultType; }
        virtual JSType *getArgType(uint32 a)    { ASSERT(mArguments && (a < (mRequiredArgs + mOptionalArgs))); return mArguments[a].mType; }
        virtual bool argHasInitializer(uint32 a){ ASSERT(mArguments && (a < (mRequiredArgs + mOptionalArgs))); return (mArguments[a].mInitializer != (uint32)(-1)); }
        virtual JSValue runArgInitializer(Context *cx, uint32 a, const JSValue& thisValue, JSValue *argv, uint32 argc);
        virtual ScopeChain *getScopeChain()     { return mScopeChain; }
        virtual JSValue getThisValue()          { return kNullValue; }         
        virtual JSType *getClass()              { return mClass; }
        virtual FunctionName *getFunctionName() { return mFunctionName; }
        virtual uint32 findParameterName(const String *name);
        virtual uint32 getRequiredArgumentCount()   
                                                { return mRequiredArgs; }
        virtual uint32 getOptionalArgumentCount()   
                                                { return mOptionalArgs; }
        virtual bool hasOptionalArguments()     { return (mOptionalArgs > 0); }
        virtual bool isChecked()                { return mIsChecked; }
        virtual bool hasRestParameter()         { return mHasRestParameter; }
        virtual const String *getRestParameterName()  
                                                { ASSERT(mHasRestParameter); return mArguments[(mRequiredArgs + mOptionalArgs)].mName; }
        virtual JSType *getRestParameterType()  { ASSERT(mHasRestParameter); return mArguments[(mRequiredArgs + mOptionalArgs)].mType; }

        virtual JSFunction *getFunction()       { return this; }
        bool isEqual(JSFunction *f)             { return (getFunction() == f->getFunction()); }

        ParameterBarrel *mParameterBarrel;
        Activation mActivation;                 // not used during execution  (XXX so maybe we should handle it differently, hmmm?)

    private:
        ByteCodeModule *mByteCode;
        NativeCode *mCode;
        JSType *mResultType;
        uint32 mRequiredArgs;                   // total # parameters
        uint32 mOptionalArgs;
        ArgumentData *mArguments;
        ScopeChain *mScopeChain;
        bool mIsPrototype;                      // set for functions with prototype attribute
        bool mIsConstructor;
        bool mIsChecked;
        bool mHasRestParameter;
        const String *mRestParameterName;
        JSType *mClass;                         // pointer to owning class if this function is a method
        FunctionName *mFunctionName;
    };

    class JSBoundFunction : public JSFunction {
    private:
        JSFunction *mFunction;
        JSObject *mThis;
    public:
        JSBoundFunction(JSFunction *f, JSObject *thisObj)
            : mFunction(NULL), mThis(thisObj) { if (f->hasBoundThis()) mFunction = f->getFunction(); else mFunction = f; }

        ~JSBoundFunction() { }  // keeping gcc happy

        bool hasBoundThis()             { return true; }
        bool isNative()                 { return mFunction->isNative(); }
        bool isPrototype()              { return mFunction->isPrototype(); }
        bool isConstructor()            { return mFunction->isConstructor(); }
        ByteCodeModule *getByteCode()   { return mFunction->getByteCode(); }
        NativeCode *getNativeCode()     { return mFunction->getNativeCode(); }
        ParameterBarrel *getParameterBarrel()
                                        { return mFunction->mParameterBarrel; }
        JSType *getResultType()         { return mFunction->getResultType(); }
        JSType *getArgType(uint32 a)    { return mFunction->getArgType(a); }
        bool argHasInitializer(uint32 a){ return mFunction->argHasInitializer(a); }
        JSValue runArgInitializer(Context *cx, uint32 a, const JSValue& thisValue, JSValue *argv, uint32 argc)
                                        { return mFunction->runArgInitializer(cx, a, thisValue, argv, argc); }
        ScopeChain *getScopeChain()     { return mFunction->getScopeChain(); }
        JSValue getThisValue()          { return (mThis) ? JSValue(mThis) : kNullValue; }         
        JSType *getClass()              { return mFunction->getClass(); }
        FunctionName *getFunctionName() { return mFunction->getFunctionName(); }
        uint32 findParameterName(const String *name)
                                        { return mFunction->findParameterName(name); } 
        uint32 getRequiredArgumentCount()   
                                        { return mFunction->getRequiredArgumentCount(); }
        uint32 getOptionalArgumentCount()   
                                        { return mFunction->getOptionalArgumentCount(); }
        bool hasOptionalArguments()     { return mFunction->hasOptionalArguments(); }
        bool isChecked()                { return mFunction->isChecked(); }
        bool hasRestParameter()         { return mFunction->hasRestParameter(); }
        const String *getRestParameterName()  
                                        { return mFunction->getRestParameterName(); }
        JSType *getRestParameterType()  { return mFunction->getRestParameterType(); }

        void getProperty(Context *cx, const String &name, NamespaceList *names) 
                                        { mFunction->getProperty(cx, name, names); }
        void setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v)
                                        { mFunction->setProperty(cx, name, names, v); }
        bool hasProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
                                        { return mFunction->hasProperty(name, names, acc, p); }

        JSFunction *getFunction()       { return mFunction; }

    };

    // This is for binary operators, it collects together the operand
    // types and the function pointer for the given operand. See also
    // Context::initOperators where the default operators are set up.
    class OperatorDefinition {
    public:
        
        OperatorDefinition(JSType *type1, JSType *type2, JSFunction *imp)
            : mType1(type1), mType2(type2), mImp(imp) { ASSERT(mType1); ASSERT(mType2); }


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


    };





    class Attribute;

    
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
            const JSValue *uninit;
        };

        Context(JSObject **global, World &world, Arena &a, Pragma::Flags flags);

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("Context", s, t); return t; }
        void operator delete(void* t)   { trace_release("Context", t); STD::free(t); }
#endif

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

        void initBuiltins();
        void initClass(JSType *type, ClassDef *cdef, PrototypeFunctions *pdef);
        void initOperators();
        void initAttributeValue(char *name, uint32 trueFlags, uint32 falseFlags);
        

        void defineOperator(Operator which, JSType *t1, JSType *t2, JSFunction *imp)
        {
            OperatorDefinition *op = new OperatorDefinition(t1, t2, imp);
            mOperatorTable[which].push_back(op);
        }

        JSValue *buildArgumentBlock(JSFunction *target, uint32 &argCount);

        
        // compiles attribute expression into an attribute object
        // which is stored back into the statement node.
        void setAttributeValue(AttributeStmtNode *s, PropertyAttribute defaultValue);
        Attribute *executeAttributes(ExprNode *attr);



        bool executeOperator(Operator op, JSType *t1, JSType *t2);
        JSValue mapValueToType(JSValue v, JSType *t);

        JSValue invokeFunction(JSFunction *target, const JSValue& thisValue, JSValue *argv, uint32 argc);

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

        JSValue mThis;

        // this is the execution stack (for the current function)
        JSValue *mStack;
        uint32 mStackTop;
        uint32 mStackMax;

        void pushValue(const JSValue &v)
        {
            ASSERT(mStackTop < mStackMax);
            mStack[mStackTop++] = v;
        }

        JSValue popValue()
        {
            ASSERT(mStackTop > 0);
            return mStack[--mStackTop];
        }

        JSValue topValue()
        {
            return mStack[mStackTop - 1];
        }

        void resizeStack(uint32 n)
        {
            ASSERT(n < mStackMax);
            mStackTop = n;
        }

        uint32 stackSize()
        {
            return mStackTop;
        }

        JSValue getValue(uint32 n)
        {
            ASSERT(n < mStackTop);
            return mStack[n];
        }

        // put the value in at 'index', lifting everything above that up by one
        void insertValue(JSValue v, uint32 index)
        {
            ASSERT(mStackTop < mStackMax);      // we're effectively pushing one entry
            for (uint32 i = mStackTop - 1; i >= index; i--)
                mStack[i + 1] = mStack[i];
            mStack[index] = v;
            mStackTop++;
        }
/*
        void setValue(uint32 n, JSValue v)
        {
            ASSERT(n < mStackTop);
            mStack[n] = v;
        }
*/
        JSValue *getBase(uint32 n)
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
        JSValue *mLocals;

        // the base of the incoming arguments for this function
        JSValue *mArgumentBase;

        typedef std::vector<OperatorDefinition *> OperatorList;
            
        OperatorList mOperatorTable[OperatorCount];
        

        JSValue readEvalFile(const String& fileName);
        JSValue readEvalString(const String &str, const String& fileName, ScopeChain *scopeChain, const JSValue& thisValue);

        void buildRuntime(StmtNode *p);
        void buildRuntimeForFunction(FunctionDefinition &f, JSFunction *fnc);
        void buildRuntimeForStmt(StmtNode *p);
        ByteCodeModule *genCode(StmtNode *p, const String &sourceName);

        JSValue interpret(ByteCodeModule *bcm, int offset, ScopeChain *scopeChain, const JSValue& thisValue, JSValue *argv, uint32 argc);
        JSValue interpret(uint8 *pc, uint8 *endPC);
        
        void reportError(Exception::Kind kind, char *message, size_t pos, const char *arg = NULL);
        void reportError(Exception::Kind kind, char *message, const char *arg = NULL);
        void reportError(Exception::Kind kind, char *message, const String& name);
        void reportError(Exception::Kind kind, char *message, size_t pos, const String& name);

        
        /* utility routines */

        // Extract the operator from the string literal function name
        // - requires the paramter count in order to distinguish
        // between unary and binary operators.
        Operator getOperator(uint32 parameterCount, const String &name);

        // Get the type of the nth parameter.
        JSType *getParameterType(FunctionDefinition &function, int index);

        // Get the number of parameters.
        uint32 getParameterCount(FunctionDefinition &function);

        Reader *mReader;

    private:
        JSObject **mGlobal;


    };

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

        JSValue mStack[ReplacementStackSize];

        Context *m_cx;

        JSValue *mOldStack;
        uint32 mOldStackTop;
        uint32 mOldStackMax;

    };

    
    class NamedArgument : public JSObject {
    public:
        NamedArgument(JSValue &v, const String *n) : JSObject(NamedArgument_Type), mValue(v), mName(n) { }

        JSValue mValue;
        const String *mName;
    };
    
    
    class Attribute : public JSObject {
    public:
        Attribute(PropertyAttribute t, PropertyAttribute f)
            : JSObject(Attribute_Type), mTrueFlags(t), mFalseFlags(f), mExtendArgument(NULL), mNamespaceList(NULL) { }

        PropertyAttribute mTrueFlags;
        PropertyAttribute mFalseFlags;

        JSType *mExtendArgument;
        NamespaceList *mNamespaceList;
    };
    
    
    
    
    
    
    
    
    
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


    inline bool JSInstance::isDynamic() { return mType->isDynamic(); }

}
}

#endif //js2runtime_h___
