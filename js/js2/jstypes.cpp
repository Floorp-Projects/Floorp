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

#include "jstypes.h"
#include "jsclasses.h"
#include "numerics.h"
#include "icodegenerator.h"
#include "interpreter.h"

namespace JavaScript {
namespace JSTypes {

using namespace JSClasses;
using namespace Interpreter;

/********** Object Object Stuff **************************/

static JSValue object_toString(Context *, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSString *s = new JSString("[object ");
        JSValue theThis = argv[0];
        ASSERT(theThis.isObject());
        s->append(theThis.object->getClass());
        s->append("]");
        return JSValue(s);
    }
    return kUndefinedValue;
}

static JSValue object_constructor(Context *cx, const JSValues& argv)
{
    // argv[0] will be NULL
    
    return new JSObject();
}

struct ObjectFunctionEntry {
    char *name;
    JSNativeFunction::JSCode fn;
} ObjectFunctions[] = {
    { "toString",     object_toString },
};


JSObject* JSObject::ObjectPrototypeObject = JSObject::initJSObject();
JSString* JSObject::ObjectString = new JSString("Object");

// This establishes the ur-prototype, there's a timing issue
// here - the JSObject static initializers have to run before
// any other JSObject objects are constructed.
JSObject *JSObject::initJSObject()
{
    JSObject *result = new JSObject();

    for (int i = 0; i < sizeof(ObjectFunctions) / sizeof(ObjectFunctionEntry); i++)
        result->setProperty(widenCString(ObjectFunctions[i].name), JSValue(new JSNativeFunction(ObjectFunctions[i].fn) ) );

    return result;
}

void JSObject::initObjectObject(JSScope *g)
{
    // The ObjectPrototypeObject has already been constructed by static initialization.
    
//    JSNativeFunction *objCon = new JSNativeFunction(objectConstructor);
//    objCon->setProperty(widenCString("prototype"), JSValue(ObjectPrototypeObject));    
//    g->setProperty(*ObjectString, JSValue(objCon));
}



/********** Function Object Stuff **************************/

// An empty function that returns undefined
static JSValue functionPrototypeFunction(Context *, const JSValues &)
{
    return kUndefinedValue;
}


static JSValue function_constructor(Context *cx, const JSValues& argv)
{
    // build a function from the arguments
    // argv[0] will be NULL

    if (argv.size() >= 2) {        
        int32 parameterCount = argv.size() - 2;
        JSString source("function (");
        for (int32 i = 0; i < parameterCount; i++) {
            source.append((JSValue::valueToString(argv[i + 1]).string));
            if (i < (parameterCount - 1)) source.append(",");
        }
        source.append(") {");
        source.append(JSValue::valueToString(argv[argv.size() - 1]).string);
        source.append("}");
        
        JSFunction *f = new JSFunction(cx->compileFunction(source));
        f->setProperty(widenCString("length"), JSValue(parameterCount));
        JSObject *obj = new JSObject();
        f->setProperty(widenCString("prototype"), JSValue(obj)); 
        f->setProperty(widenCString("constructor"), JSValue(obj)); 
        return JSValue(f);
    }

    return kUndefinedValue;
}

static JSValue function_toString(Context *, const JSValues &)
{
    return JSValue(new JSString("function XXX() { }" ));
}
static JSValue function_apply(Context *, const JSValues &)
{
    // XXX
    return kUndefinedValue;
}
static JSValue function_call(Context *, const JSValues &)
{
    // XXX
    return kUndefinedValue;
}


JSString* JSFunction::FunctionString = new JSString("Function");
JSObject* JSFunction::FunctionPrototypeObject = NULL;   // the 'original Function prototype object'

struct FunctionFunctionEntry {
    char *name;
    JSNativeFunction::JSCode fn;
} FunctionFunctions[] = {
    { "constructor",    function_constructor },
    { "toString",       function_toString },
    { "apply",          function_apply },
    { "call",           function_call },
};

void JSFunction::initFunctionObject(JSScope *g)
{
    // first build the Function Prototype Object
    FunctionPrototypeObject = new JSNativeFunction(functionPrototypeFunction);
    for (int i = 0; i < sizeof(FunctionFunctions) / sizeof(FunctionFunctionEntry); i++)
        FunctionPrototypeObject->setProperty(widenCString(FunctionFunctions[i].name), JSValue(new JSNativeFunction(FunctionFunctions[i].fn) ) );

    ASSERT(g->getProperty(*FunctionString).isObject());
    JSObject *functionVariable = g->getProperty(*FunctionString).object;
    // there is actually no connection between the 'prototype' property of the function object
    // and the prototype set for each new function - the constructor has implicit access to
    // the 'original Function prototype object'
    functionVariable->setProperty(widenCString("prototype"), JSValue(FunctionPrototypeObject));   // should be DontEnum, DontDelete, ReadOnly
}

/********** Boolean Object Stuff **************************/

JSString* JSBoolean::BooleanString = new JSString("Boolean");

static JSValue boolean_constructor(Context *cx, const JSValues& argv)
{
    // argv[0] will be NULL
    if (argv.size() > 1)
        return JSValue(new JSBoolean(JSValue::valueToBoolean(argv[1]).boolean));
    else
        return JSValue(new JSBoolean(false));
}

static JSValue boolean_toString(Context *cx, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSValue theThis = argv[0];
        if (theThis.isObject()) {
            JSBoolean *b = dynamic_cast<JSBoolean *>(theThis.object);
            if (b)
                return JSValue(new JSString(b->getValue() ? "true" : "false"));
            else
                throw new JSException("TypeError : Boolean::toString called on non boolean object");
        }
        else
            throw new JSException("TypeError : Boolean::toString called on non object");
    }
    return kUndefinedValue;
}

static JSValue boolean_valueOf(Context *cx, const JSValues& argv)
{
    return kUndefinedValue;
}

JSObject *JSBoolean::BooleanPrototypeObject = NULL;

struct BooleanFunctionEntry {
    char *name;
    JSNativeFunction::JSCode fn;
} BooleanFunctions[] = {
    { "constructor",    boolean_constructor },
    { "toString",       boolean_toString },
    { "valueOf",        boolean_valueOf },
};

void JSBoolean::initBooleanObject(JSScope *g)
{
    BooleanPrototypeObject = new JSObject();
    BooleanPrototypeObject->setClass(new JSString(BooleanString));

    for (int i = 0; i < sizeof(BooleanFunctions) / sizeof(BooleanFunctionEntry); i++)
        BooleanPrototypeObject->setProperty(widenCString(BooleanFunctions[i].name), JSValue(new JSNativeFunction(BooleanFunctions[i].fn) ) );

    ASSERT(g->getProperty(*BooleanString).isObject());
    JSObject *booleanVariable = g->getProperty(*BooleanString).object;
    booleanVariable->setProperty(widenCString("prototype"), JSValue(BooleanPrototypeObject));   // should be DontEnum, DontDelete, ReadOnly
}

/********** Date Object Stuff **************************/

static JSValue date_constructor(Context *cx, const JSValues& argv)
{
    // return JSValue(new JSDate());
    return JSValue(new JSObject());
}

static JSValue date_invokor(Context *cx, const JSValues& argv)
{
    return JSValue(new JSString("now"));
}


/**************************************************************************************/

JSType Any_Type = JSType(widenCString("any"), NULL);
JSType Integer_Type = JSType(widenCString("Integer"), &Any_Type);
JSType Number_Type = JSType(widenCString("Number"), &Integer_Type);
JSType Character_Type = JSType(widenCString("Character"), &Any_Type);
JSType String_Type = JSType(widenCString("String"), &Character_Type);
JSType Function_Type = JSType(widenCString("Function"), &Any_Type, new JSNativeFunction(function_constructor), new JSNativeFunction(function_constructor));
JSType Array_Type = JSType(widenCString("Array"), &Any_Type);
JSType Type_Type = JSType(widenCString("Type"), &Any_Type);
JSType Boolean_Type = JSType(widenCString("Boolean"), &Any_Type, new JSNativeFunction(boolean_constructor), new JSNativeFunction(boolean_constructor));
JSType Null_Type = JSType(widenCString("Null"), &Any_Type);
JSType Void_Type = JSType(widenCString("void"), &Any_Type);
JSType None_Type = JSType(widenCString("none"), &Any_Type);


JSType Object_Type = JSType(widenCString("Object"), NULL, new JSNativeFunction(object_constructor));
JSType Date_Type = JSType(widenCString("Date"), NULL, new JSNativeFunction(date_constructor), new JSNativeFunction(date_invokor));


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

#define JSDOUBLE_IS_NEGZERO(d)  (JSDOUBLE_HI32(d) == JSDOUBLE_HI32_SIGNBIT && \
				 JSDOUBLE_LO32(d) == 0)


// the canonical undefined value, etc.
const JSValue kUndefinedValue;
const JSValue kNaNValue = JSValue(nan);
const JSValue kTrueValue = JSValue(true);
const JSValue kFalseValue = JSValue(false);
const JSValue kNullValue = JSValue((JSObject*)NULL);
const JSValue kNegativeZero = JSValue(-0.0);
const JSValue kPositiveZero = JSValue(0.0);
const JSValue kNegativeInfinity = JSValue(negativeInfinity);
const JSValue kPositiveInfinity = JSValue(positiveInfinity);


const JSType *JSValue::getType() const
{
    switch (tag) {
    case JSValue::i32_tag:
        return &Integer_Type;
    case JSValue::u32_tag:
        return &Integer_Type;
    case JSValue::integer_tag:
        return &Integer_Type;
    case JSValue::f64_tag:
        return &Number_Type;
    case JSValue::object_tag:
        {
            //
            // XXX why isn't there a class for Object? XXX
            //
            JSClass *clazz = dynamic_cast<JSClass *>(object->getType());
            if (clazz) 
                return clazz;
            else
                return &Any_Type;
        }
    case JSValue::array_tag:
        return &Array_Type;
    case JSValue::function_tag:
        return &Function_Type;
    case JSValue::string_tag:
        return &String_Type;
    case JSValue::boolean_tag:
        return &Boolean_Type;
    case JSValue::undefined_tag:
        return &Void_Type;
    case JSValue::type_tag:
        return &Type_Type;
    default:
        NOT_REACHED("Bad tag");
        return &None_Type;
    }
}

bool JSValue::isNaN() const
{
    ASSERT(isNumber());
    switch (tag) {
    case i32_tag:
    case u32_tag:
        return false;
    case integer_tag:
    case f64_tag:
        return JSDOUBLE_IS_NaN(f64);
    default:
        NOT_REACHED("Broken compiler?");
        return true;
    }
}
              
bool JSValue::isNegativeInfinity() const
{
    ASSERT(isNumber());
    switch (tag) {
    case i32_tag:
    case u32_tag:
        return false;
    case integer_tag:
    case f64_tag:
        return (f64 < 0) && JSDOUBLE_IS_INFINITE(f64);
    default:
        NOT_REACHED("Broken compiler?");
        return true;
    }
}
              
bool JSValue::isPositiveInfinity() const
{
    ASSERT(isNumber());
    switch (tag) {
    case i32_tag:
    case u32_tag:
        return false;
    case integer_tag:
    case f64_tag:
        return (f64 > 0) && JSDOUBLE_IS_INFINITE(f64);
    default:
        NOT_REACHED("Broken compiler?");
        return true;
    }
}
              
bool JSValue::isNegativeZero() const
{
    ASSERT(isNumber());
    switch (tag) {
    case i32_tag:
    case u32_tag:
        return false;
    case integer_tag:
    case f64_tag:
        return JSDOUBLE_IS_NEGZERO(f64);
    default:
        NOT_REACHED("Broken compiler?");
        return true;
    }
}
              
bool JSValue::isPositiveZero() const
{
    ASSERT(isNumber());
    switch (tag) {
    case i32_tag:
        return (i32 == 0);
    case u32_tag:
        return (u32 == 0);
    case integer_tag:
    case f64_tag:
        return (f64 == 0.0) && !JSDOUBLE_IS_NEGZERO(f64);
    default:
        NOT_REACHED("Broken compiler?");
        return true;
    }
}
              
int JSValue::operator==(const JSValue& value) const
{
    if (this->tag == value.tag) {
        #define CASE(T) case T##_tag: return (this->T == value.T)
        switch (tag) {
        CASE(i8); CASE(u8);
        CASE(i16); CASE(u16);
        CASE(i32); CASE(u32); CASE(f32);
        CASE(i64); CASE(u64); CASE(f64);
        CASE(object); CASE(array); CASE(function); CASE(string);
        CASE(type); CASE(boolean);
        #undef CASE
        case integer_tag : return (this->f64 == value.f64);
        // question:  are all undefined values equal to one another?
        case undefined_tag: return 1;
        }
    }
    return 0;
}

Formatter& operator<<(Formatter& f, JSObject& obj)
{
    obj.printProperties(f);
    return f;
}

void JSObject::printProperties(Formatter& f)
{
    for (JSProperties::const_iterator i = mProperties.begin(); i != mProperties.end(); i++) {
        f << (*i).first << " : " << (*i).second;
        f << "\n";
    }
}


String getRegisterValue(const JSValues& registerList, Register reg)
{
    StringFormatter sf;

    if (reg == NotARegister)
        sf << "<NaR>";
    else
        sf << "R" << reg << '=' << registerList[reg];
    return sf;
}

Formatter& operator<<(Formatter& f, const JSValue& value)
{
    switch (value.tag) {
    case JSValue::i32_tag:
        f << float64(value.i32);
        break;
    case JSValue::u32_tag:
        f << float64(value.u32);
        break;
    case JSValue::integer_tag:
    case JSValue::f64_tag:
        f << value.f64;
        break;
    case JSValue::object_tag:
        printFormat(f, "Object @ 0x%08X\n", value.object);
        f << *value.object;
        break;
    case JSValue::array_tag:
        printFormat(f, "Array @ 0x%08X", value.object);
        break;
    case JSValue::function_tag:
        printFormat(f, "Function @ 0x%08X", value.object);
        break;
    case JSValue::string_tag:
        f << "\"" << *value.string << "\"";
        break;
    case JSValue::boolean_tag:
        f << ((value.boolean) ? "true" : "false");
        break;
    case JSValue::undefined_tag:
        f << "undefined";
        break;
    case JSValue::type_tag:
        printFormat(f, "Type @ 0x%08X\n", value.type);
        f << *value.type;
        break;
    default:
        NOT_REACHED("Bad tag");
    }
    return f;
}

JSValue JSValue::toPrimitive(ECMA_type /*hint*/) const
{
    JSObject *obj;
    switch (tag) {
    case i32_tag:
    case u32_tag:
    case integer_tag:
    case f64_tag:
    case string_tag:
    case boolean_tag:
    case undefined_tag:
        return *this;

    case object_tag:
        obj = object;
        break;
    case array_tag:
        obj = array;
        break;
    case function_tag:
        obj = function;
        break;

    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }

    const JSValue &toString = obj->getProperty(widenCString("toString"));
    if (toString.isObject()) {
        if (toString.isFunction()) {
        }
        else    // right? The spec doesn't say
            throw new JSException("Runtime error from toPrimitive");    // XXX
    }

    const JSValue &valueOf = obj->getProperty(widenCString("valueOf"));
    if (!valueOf.isObject())
        throw new JSException("Runtime error from toPrimitive");    // XXX
    
    return kUndefinedValue;
    
}


JSValue JSValue::valueToString(const JSValue& value) // can assume value is not a string
{
    const char* chrp;
    char buf[dtosStandardBufferSize];
    switch (value.tag) {
    case i32_tag:
        chrp = doubleToStr(buf, dtosStandardBufferSize, value.i32, dtosStandard, 0);
        break;
    case u32_tag:
        chrp = doubleToStr(buf, dtosStandardBufferSize, value.u32, dtosStandard, 0);
        break;
    case integer_tag:
    case f64_tag:
        chrp = doubleToStr(buf, dtosStandardBufferSize, value.f64, dtosStandard, 0);
        break;
    case object_tag:
        chrp = "object";
        break;
    case array_tag:
        chrp = "array";
        break;
    case function_tag:
        chrp = "function";
        break;
    case string_tag:
        return value;
    case boolean_tag:
        chrp = (value.boolean) ? "true" : "false";
        break;
    case undefined_tag:
        chrp = "undefined";
        break;
    default:
        NOT_REACHED("Bad tag");
    }
    return JSValue(new JSString(chrp));
}

JSValue JSValue::valueToNumber(const JSValue& value) // can assume value is not a number
{
    switch (value.tag) {
    case i32_tag:
        return JSValue((float64)value.i32);
    case u32_tag:
        return JSValue((float64)value.u32);
    case integer_tag:
    case f64_tag:
        return value;
    case string_tag: 
        {
            JSString* str = value.string;
            const char16 *numEnd;
	        double d = stringToDouble(str->begin(), str->end(), numEnd);
            return JSValue(d);
        }
    case object_tag:
    case array_tag:
    case function_tag:
        // XXX more needed :
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    case boolean_tag:
        return JSValue((value.boolean) ? 1.0 : 0.0);
    case undefined_tag:
        return kNaNValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}

JSValue JSValue::valueToInteger(const JSValue& value)
{
    JSValue result = valueToNumber(value);
    ASSERT(result.tag == f64_tag);
    result.tag = i64_tag;
    bool neg = (result.f64 < 0);
    result.f64 = floor((neg) ? -result.f64 : result.f64);
    result.f64 = (neg) ? -result.f64 : result.f64;
    return result;
}


JSValue JSValue::valueToBoolean(const JSValue& value)
{
    switch (value.tag) {
    case i32_tag:
        return JSValue(value.i32 != 0);
    case u32_tag:
        return JSValue(value.u32 != 0);
    case integer_tag:
    case f64_tag:
        return JSValue(!(value.f64 == 0.0) || JSDOUBLE_IS_NaN(value.f64));
    case string_tag: 
        return JSValue(value.string->length() != 0);
    case boolean_tag:
        return value;
    case object_tag:
    case array_tag:
    case function_tag:
        return kTrueValue;
    case undefined_tag:
        return kFalseValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}


static const double two32 = 4294967296.0;
static const double two31 = 2147483648.0;

JSValue JSValue::valueToInt32(const JSValue& value)
{
    double d;
    switch (value.tag) {
    case i32_tag:
        return value;
    case u32_tag:
        d = value.u32;
        break;
    case integer_tag:
    case f64_tag:
        d = value.f64;
        break;
    case string_tag: 
        {
            JSString* str = value.string;
            const char16 *numEnd;
	        d = stringToDouble(str->begin(), str->end(), numEnd);
        }
        break;
    case boolean_tag:
        return JSValue((int32)((value.boolean) ? 1 : 0));
    case object_tag:
    case array_tag:
    case undefined_tag:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d) )
        return JSValue((int32)0);
    d = fmod(d, two32);
    d = (d >= 0) ? d : d + two32;
    if (d >= two31)
	    return JSValue((int32)(d - two32));
    else
	    return JSValue((int32)d);
    
}

JSValue JSValue::valueToUInt32(const JSValue& value)
{
    double d;
    switch (value.tag) {
    case i32_tag:
        return JSValue((uint32)value.i32);
    case u32_tag:
        return value;
    case f64_tag:
        d = value.f64;
        break;
    case string_tag: 
        {
            JSString* str = value.string;
            const char16 *numEnd;
	        d = stringToDouble(str->begin(), str->end(), numEnd);
        }
        break;
    case boolean_tag:
        return JSValue((uint32)((value.boolean) ? 1 : 0));
    case object_tag:
    case array_tag:
    case undefined_tag:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d))
        return JSValue((uint32)0);
    bool neg = (d < 0);
    d = floor(neg ? -d : d);
    d = neg ? -d : d;
    d = fmod(d, two32);
    d = (d >= 0) ? d : d + two32;
    return JSValue((uint32)d);
}


JSValue JSValue::convert(JSType *toType)
{
    if (toType == &Any_Type)    // yuck, something wrong with this
                                // maybe the base types should be 
                                // a family of classes, not just instances
                                // of JSType ???
        return *this;
    else if (toType == &Integer_Type)
        return valueToInteger(*this);
    else {
        JSClass *toClass = dynamic_cast<JSClass *>(toType);
        if (toClass) {
            if (tag == object_tag) {
                JSClass *fromClass = dynamic_cast<JSClass *>(object->getType());
                if (fromClass) {
                    while (fromClass != toClass) {
                        fromClass = fromClass->getSuperClass();
                        if (fromClass == NULL)
                            throw new JSException("Can't cast to unrelated class");
                    }
                    return *this;
                }
                else
                    throw new JSException("Can't cast a generic object to a class");
            }
            else
                throw new JSException("Can't cast a non-object to a class");
        }
    }
    return kUndefinedValue;
}


JSFunction::~JSFunction()
{
    delete mICode;
}

JSString::JSString(const String& str)
{
    size_t n = str.size();
    resize(n);
    traits_type::copy(begin(), str.begin(), n);
}

JSString::JSString(const String* str)
{
    size_t n = str->size();
    resize(n);
    traits_type::copy(begin(), str->begin(), n);
}

JSString::JSString(const char* str)
{
    size_t n = ::strlen(str);
    resize(n);
    std::transform(str, str + n, begin(), JavaScript::widen);
}

void JSString::append(const char* str)
{
    size_t n = ::strlen(str);
    size_t oldSize = size();
    resize(oldSize + n);
    std::transform(str, str + n, begin() + oldSize, JavaScript::widen);
}

void JSString::append(const JSStringBase* str)
{
    size_t n = str->size();
    size_t oldSize = size();
    resize(oldSize + n);
    traits_type::copy(begin() + oldSize, str->begin(), n);
}

JSString::operator String()
{
    return String(begin(), size());
}


// # of sub-type relationship between this type and the other type 
// (== MAX_INT if other is not a base type)

int32 JSType::distance(const JSType *other) const
{
    if (other == this) 
        return 0; 
    if (mBaseType == NULL)
        return NoRelation;
    int32 baseDistance = mBaseType->distance(other);
    if (baseDistance != NoRelation)
        ++baseDistance;
    return baseDistance;
}


} /* namespace JSTypes */    
} /* namespace JavaScript */
