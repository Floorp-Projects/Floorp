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

#ifndef jstypes_h
#define jstypes_h

#include "utilities.h"
#include "gc_allocator.h"
#include "parser.h"
#include <vector>
#include <map>
#include <stack>


/* forward declare classes from JavaScript::ICG */
namespace JavaScript {
namespace ICG {
    class ICodeModule;
} /* namespace ICG */
namespace Interpreter {
    class Context;
} /* namespace Interpreter */
} /* namespace JavaScript */

namespace JavaScript {
namespace JSTypes {

    using ICG::ICodeModule;
    using Interpreter::Context;
    
    class JSObject;
    class JSArray;
    class JSString;
    class JSFunction;
    class JSScope;
    class JSType;
    class Context;
    
    typedef uint32 Register;

    /**
     * All JavaScript data types.
     */        
    struct JSValue {
        union {
            int8 i8;
            uint8 u8;
            int16 i16;
            uint16 u16;
            int32 i32;
            uint32 u32;
            int64 i64;
            uint64 u64;
            float32 f32;
            float64 f64;
            JSObject* object;
            JSArray* array;
            JSFunction *function;
            JSString *string;
            JSType *type;
            bool boolean;
        };
        
        /* These are the ECMA types, for use in 'toPrimitive' calls */
        enum ECMA_type {
            Undefined, Null, Boolean, Number, Object, String, 
            NoHint
        };

        typedef enum {
            i8_tag, u8_tag,
            i16_tag, u16_tag,
            i32_tag, u32_tag,
            i64_tag, u64_tag,
            f32_tag, f64_tag,
            integer_tag,
            object_tag, array_tag, function_tag, string_tag, boolean_tag, type_tag,
            undefined_tag,
            uninitialized_tag
        } Tag;
        Tag tag;
        
        JSValue() : f64(0.0), tag(undefined_tag) {}
        explicit JSValue(int32 i32) : i32(i32), tag(i32_tag) {}
        explicit JSValue(uint32 u32) : u32(u32), tag(u32_tag) {}
        explicit JSValue(float64 f64) : f64(f64), tag(f64_tag) {}
        explicit JSValue(JSObject* object) : object(object), tag(object_tag) {}
        explicit JSValue(JSArray* array) : array(array), tag(array_tag) {}
        explicit JSValue(JSFunction* function) : function(function), tag(function_tag) {}
        explicit JSValue(JSString* string) : string(string), tag(string_tag) {}
        explicit JSValue(bool boolean) : boolean(boolean), tag(boolean_tag) {}
        explicit JSValue(JSType* type) : type(type), tag(type_tag) {}
        explicit JSValue(Tag tag) : tag(tag) {}

        int32& operator=(int32 i32)                     { return (tag = i32_tag, this->i32 = i32); }
        uint32& operator=(uint32 u32)                   { return (tag = u32_tag, this->u32 = u32); }
        float64& operator=(float64 f64)                 { return (tag = f64_tag, this->f64 = f64); }
        JSObject*& operator=(JSObject* object)          { return (tag = object_tag, this->object = object); }
        JSArray*& operator=(JSArray* array)             { return (tag = array_tag, this->array = array); }
        JSFunction*& operator=(JSFunction* function)    { return (tag = function_tag, this->function = function); }
        JSString*& operator=(JSString* string)          { return (tag = string_tag, this->string = string); }
        bool& operator=(bool boolean)                   { return (tag = boolean_tag, this->boolean = boolean); }
        JSType*& operator=(JSType* type)                { return (tag = type_tag, this->type = type); }
        
        bool isFunction() const                         { return (tag == function_tag); }
        bool isObject() const                           { return ((tag == object_tag) || (tag == function_tag) || (tag == array_tag) || (tag == type_tag)); }
        bool isString() const                           { return (tag == string_tag); }
        bool isBoolean() const                          { return (tag == boolean_tag); }
        bool isNumber() const                           { return (tag == f64_tag) || (tag == integer_tag); }

                                                        /* this is correct wrt ECMA, The i32 & u32 kinds
                                                           will have to be converted (to doubles?) anyway because
                                                           we can't have overflow happening in generic arithmetic */

        bool isInitialized() const                      { return (tag != uninitialized_tag); }
        bool isUndefined() const                        { return (tag == undefined_tag); }
        bool isNull() const                             { return ((tag == object_tag) && (this->object == NULL)); }
        bool isNaN() const;
        bool isNegativeInfinity() const;
        bool isPositiveInfinity() const;
        bool isNegativeZero() const;
        bool isPositiveZero() const;
        bool isType() const                             { return (tag == type_tag); }

        JSValue toString() const                        { return (isString() ? *this : valueToString(*this)); }
        JSValue toNumber() const                        { return (isNumber() ? *this : valueToNumber(*this)); }
        JSValue toInt32() const                         { return ((tag == i32_tag) ? *this : valueToInt32(*this)); }
        JSValue toUInt32() const                        { return ((tag == u32_tag) ? *this : valueToUInt32(*this)); }
        JSValue toBoolean() const                       { return ((tag == boolean_tag) ? *this : valueToBoolean(*this)); }

        JSValue toPrimitive(ECMA_type hint = NoHint) const;

        JSValue convert(JSType *toType);

        
        static JSValue valueToString(const JSValue& value);
        static JSValue valueToNumber(const JSValue& value);
        static JSValue valueToInteger(const JSValue& value);
        static JSValue valueToInt32(const JSValue& value);
        static JSValue valueToUInt32(const JSValue& value);
        static JSValue valueToBoolean(const JSValue& value);


        const JSType *getType() const;                  // map from tag type to JS2 type

        int operator==(const JSValue& value) const;
    };

    Formatter& operator<<(Formatter& f, const JSValue& value);

#if defined(XP_MAC)
    // copied from default template parameters in map.
    typedef gc_allocator<std::pair<const String, JSValue> > gc_map_allocator;
#elif defined(XP_UNIX)
    // FIXME: in libg++, they assume the map's allocator is a byte allocator,
    // which is wrapped in a simple_allocator. this is crap.
    typedef char _Char[1];
    typedef gc_allocator<_Char> gc_map_allocator;
#elif defined(_WIN32)
    // FIXME: MSVC++'s notion. this is why we had to add _Charalloc().
    typedef gc_allocator<JSValue> gc_map_allocator;
#endif        
        
    /**
     * GC-scannable array of values.
     */
    typedef std::vector<JSValue, gc_allocator<JSValue> > JSValues;


    String getRegisterValue(const JSValues& registerList, Register reg);

    extern const JSValue kUndefinedValue;
    extern const JSValue kNaNValue;
    extern const JSValue kTrueValue;
    extern const JSValue kFalseValue;
    extern const JSValue kNullValue;
    extern const JSValue kNegativeZero;
    extern const JSValue kPositiveZero;
    extern const JSValue kNegativeInfinity;
    extern const JSValue kPositiveInfinity;

    // JS2 predefined types:
    extern JSType           Any_Type;
    extern JSType           Integer_Type;
    extern JSType           Number_Type;
    extern JSType           Character_Type;
    extern JSType           String_Type;
    extern JSType           Function_Type;
    extern JSType           Array_Type;
    extern JSType           Type_Type;
    extern JSType           Boolean_Type;
    extern JSType           Null_Type;
    extern JSType           Void_Type;
    extern JSType           None_Type;

    // JS1X heritage classes as types:
    extern JSType Object_Type;
    extern JSType Date_Type;

    typedef std::map<String, JSValue, std::less<String>, gc_map_allocator> JSProperties;

    /**
     * Basic behavior of all JS objects, mapping a name to a value,
     * with prototype-based inheritance.
     */
    class JSObject : public gc_base {
    protected:
        JSProperties mProperties;
        JSObject* mPrototype;
        JSType* mType;
        JSString* mClass;       // this is the internal [[Class]] property

        static JSObject *initJSObject();
        static JSString *ObjectString;
        static JSObject *ObjectPrototypeObject;

        void init(JSObject* prototype)  { mPrototype = prototype; mType = &Any_Type; mClass = ObjectString; }

    public:
        JSObject()                      { init(ObjectPrototypeObject); }
        JSObject(JSValue &constructor)  { init(constructor.object->getProperty(widenCString("prototype")).object); }
        JSObject(JSObject *prototype)   { init(prototype); }
    
        static void initObjectObject(JSScope *g);

        bool hasProperty(const String& name)
        {
            return (mProperties.count(name) != 0);
        }

        const JSValue& getProperty(const String& name)
        {
            JSProperties::const_iterator i = mProperties.find(name);
            if (i != mProperties.end())
                return i->second;
            if (mPrototype)
                return mPrototype->getProperty(name);
            return kUndefinedValue;
        }

        // return the property AND the object it's found in
        // (would rather return references, but couldn't get that to work)
        const JSValue getReference(JSValue &prop, const String& name)
        {
            JSProperties::const_iterator i = mProperties.find(name);
            if (i != mProperties.end()) {
                prop = i->second;
                return JSValue(this);
            }
            if (mPrototype)
                return mPrototype->getReference(prop, name);
            return kUndefinedValue;
        }
        
        JSValue& setProperty(const String& name, const JSValue& value)
        {
            return (mProperties[name] = value);
        }
        
        const JSValue& deleteProperty(const String& name)
        {
            JSProperties::iterator i = mProperties.find(name);
            if (i != mProperties.end()) {
                mProperties.erase(i);
                return kTrueValue;
            }
            if (mPrototype)
                return mPrototype->deleteProperty(name);
            return kFalseValue;
        }

        void setPrototype(JSObject* prototype)
        {
            mPrototype = prototype;
        }
        
        JSObject* getPrototype()
        {
            return mPrototype;
        }

        JSType* getType()
        {
            return mType;
        }

        JSString* getClass()
        {
            return mClass;
        }

        void setClass(JSString* s)
        {
            mClass = s;
        }

        virtual void printProperties(Formatter& f);
    };
    
    Formatter& operator<<(Formatter& f, JSObject& obj);

    /**
     * Private representation of a JavaScript array.
     */
    class JSArray : public JSObject {
        JSValues elements;
        uint32 top;
    public:
        JSArray() : elements(1) { top = 0; }
        JSArray(uint32 size) : elements(size) { top = size; }
        JSArray(const JSValues &v) : elements(v) {}

        uint32 length()
        {
            return top;
        }
            
        JSValue& operator[](const JSValue& index)
        {
            // for now, we can only handle f64 index values.
            uint32 n = (uint32)index.f64;
            // obviously, a sparse representation might be better.
            uint32 size = elements.size();
            if (n >= size) expand(n, size);
            markHiEnd(n);
            return elements[n];
        }
            
        JSValue& operator[](uint32 n)
        {
            // obviously, a sparse representation might be better.
            uint32 size = elements.size();
            if (n >= size) expand(n, size);
            markHiEnd(n);
            return elements[n];
        }
            
        void resize(uint32 size)
        {
            elements.resize(size);
            top = size;
        }
            
    private:

        void markHiEnd(uint32 index)
        {
            if ((index + 1) > top) top = index + 1;
        }

        void expand(uint32 n, uint32 size)
        {
            do {
                size *= 2;
            } while (n >= size);
            elements.resize(size);
        }
    };
        
#if defined(XP_UNIX)
    // bastring.cc defines a funky operator new that assumes a byte-allocator.
    typedef string_char_traits<char16> JSCharTraits;
    typedef gc_allocator<_Char> JSStringAllocator;
#else
    typedef std::char_traits<char16> JSCharTraits;
    typedef gc_allocator<char16> JSStringAllocator;
#endif

    typedef std::basic_string<char16, JSCharTraits, JSStringAllocator> JSStringBase;

    /**
     * Garbage collectable UNICODE string.
     */
    class JSString : public JSStringBase, public gc_base {
    public:
        JSString() {}
        explicit JSString(const JSStringBase& str) : JSStringBase(str) {}
        explicit JSString(const JSStringBase* str) : JSStringBase(*str) {}
        explicit JSString(const String& str);
        explicit JSString(const String* str);
        explicit JSString(const char* str);

        operator String();

        void append(const char* str);
        void append(const JSStringBase* str);
    };

    class JSException : public gc_base {
    public:
        JSException(char *mess) : value(JSValue(new JSString(mess))) { }
        JSException(JSValue v) : value(v) { }
        JSValue value;
    };
    
    inline Formatter& operator<<(Formatter& f, const JSString& str)
    {
        printString(f, str.begin(), str.end());
        return f;
    }

    /**
     * Private representation of a JS function. This simply
     * holds a reference to the iCode module that is the
     * compiled code of the function.
     */
    class JSFunction : public JSObject {
        static JSString* FunctionString;
        static JSObject* FunctionPrototypeObject;
        ICodeModule* mICode;
        
        uint32 mParameterCount;
        bool mParameterInfo;

    protected:
        JSFunction() : mICode(0), mParameterCount(0) {}

   	    typedef JavaScript::gc_traits_finalizable<JSFunction> traits;
	    typedef gc_allocator<JSFunction, traits> allocator;
		
    public:
        static void initFunctionObject(JSScope *g);

        JSFunction(ICodeModule* iCode, FunctionDefinition *def)
            : JSObject(FunctionPrototypeObject),
              mICode(iCode), mParameterCount(0), mParameterInfo(false)
        {
            setClass(FunctionString);
            if (def) {
                mParameterInfo = true;
                VariableBinding *p = def->parameters;
                while (p) { mParameterCount++; p = p->next; }
            }
        }

        uint32 getParameterCount()  { return mParameterCount; }
        bool hasParameterInfo()     { return mParameterInfo; }

        ~JSFunction();
    
        void* operator new(size_t) { return allocator::allocate(1); }

        ICodeModule* getICode()    { return mICode; }
        virtual bool isNative()    { return false; }
    };

    class JSNativeFunction : public JSFunction {
    public:
        typedef JSValue (*JSCode)(Context *cx, const JSValues& argv);
        JSCode mCode;
        JSNativeFunction(JSCode code) : mCode(code) {}
        virtual bool isNative()    { return true; }
    };
        
    class JSBinaryOperator : public JSFunction {
    public:
        typedef JSValue (*JSBinaryCode)(const JSValue& arg1, const JSValue& arg2);
        JSBinaryCode mCode;
        JSBinaryOperator(JSBinaryCode code) : mCode(code) {}
        virtual bool isNative()    { return true; }
    };

   /**
     * Provides a set of nested scopes. 
     */
    class JSScope : public JSObject {
    protected:
        JSScope* mParent;
        JSProperties mTypes;
    public:
        JSScope(JSScope* parent = 0, JSObject* prototype = 0)
            : mParent(parent)
        {
            if (prototype)
                setPrototype(prototype);
        }
        
        JSScope* getParent()
        {
            return mParent;
        }
    
        bool isDefined(const String& name)
        {
            if (hasProperty(name))
                return true;
            if (mParent)
                return mParent->isDefined(name);
            return false;
        }
        
        const JSValue& getVariable(const String& name)
        {
            const JSValue& ret = getProperty(name);
            if (ret.isUndefined() && mParent) 
                return mParent->getVariable(name);
            return ret;
        }
        
        JSValue& setVariable(const String& name, const JSValue& value)
        {
            return (mProperties[name] = value);
        }
        
        JSValue& defineVariable(const String& name, JSType* type, const JSValue& value)
        {
            if (type != &Any_Type)
                mTypes[name] = type;
            return (mProperties[name] = value);
        }

        JSValue& defineVariable(const String& name, JSType* type)
        {
            if (type != &Any_Type)
                mTypes[name] = type;
            return (mProperties[name] = kUndefinedValue);
        }

        void setType(const String& name, JSType* type)
        {
            // only type variables that are defined in this scope.
            JSProperties::iterator i = mProperties.find(name);
            if (i != mProperties.end())
                mTypes[name] = type;
            else
                if (mParent)
                    mParent->setType(name, type);
        }

        JSType* getType(const String& name)
        {
            JSType* result = &Any_Type;
            // only consider types for variables defined in this scope.
            JSProperties::const_iterator i = mProperties.find(name);
            if (i != mProperties.end()) {
                i = mTypes.find(name);
                if (i != mTypes.end())
                    result = i->second.type;
            } else {
                // see if variable is defined in parent scope.
                if (mParent)
                    result = mParent->getType(name);
            }
            return result;
        }
        
        JSValue& defineFunction(const String& name, ICodeModule* iCode, FunctionDefinition *def)
        {
            JSValue value(new JSFunction(iCode, def));
            return defineVariable(name, &Function_Type, value);
        }

        JSValue& defineNativeFunction(const String& name, JSNativeFunction::JSCode code)
        {
            JSValue value(new JSNativeFunction(code));
            return defineVariable(name, &Function_Type, value);
        }
    };

    class JSType : public JSObject {
    protected:
        String mName;
        JSType *mBaseType;

        // The constructor is an implementation of the [[Construct]] mechanism
        JSFunction *mConstructor;
        // The invokor is an implementation of the [[Call]] mechanism
        JSFunction *mInvokor;
    public:
        JSType(const String &name, JSType *baseType) : mName(name), mBaseType(baseType), mConstructor(NULL), mInvokor(NULL)
        {
            mType = &Type_Type;
        }
        
        JSType(const String &name, JSType *baseType, JSFunction *constructor, JSFunction *invokor = NULL) 
                    : mName(name), mBaseType(baseType), mConstructor(constructor), mInvokor(invokor)
        {
            mType = &Type_Type;
        }

        enum { NoRelation = 0x7FFFFFFF };

        const String& getName() const { return mName; }

        int32 distance(const JSType *other) const;

        JSFunction *getConstructor() const { return mConstructor; }
        JSFunction *getInvokor() const { return mInvokor; }
    };

    class JSBoolean : public JSObject {
    protected:
        bool mValue;
        static JSObject* BooleanPrototypeObject;
        static JSString* BooleanString;
    public:
        JSBoolean(bool value) : mValue(value), JSObject(BooleanPrototypeObject) { setClass(BooleanString); }

        static void initBooleanObject(JSScope *g);

        bool getValue() { return mValue; }
    };

} /* namespace JSTypes */    
} /* namespace JavaScript */


#endif /* jstypes_h */
