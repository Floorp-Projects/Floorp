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
#include <vector>
#include <map>
#include <stack>


/* forward declare classes from JavaScript::ICG */
namespace JavaScript {
namespace ICG {
    class ICodeModule;
} /* namespace ICG */
} /* namespace JavaScript */

namespace JavaScript {
namespace JSTypes {
    using ICG::ICodeModule;
    
    class JSObject;
    class JSArray;
    class JSFunction;
    class JSString;
    
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
        };
        
        enum {
            i8_tag, u8_tag,
            i16_tag, u16_tag,
            i32_tag, u32_tag,
            i64_tag, u64_tag,
            f32_tag, f64_tag,
            object_tag, array_tag, function_tag, string_tag,
            undefined_tag
        } tag;
        
        JSValue() : f64(0.0), tag(undefined_tag) {}
        explicit JSValue(int32 i32) : i32(i32), tag(i32_tag) {}
        explicit JSValue(float64 f64) : f64(f64), tag(f64_tag) {}
        explicit JSValue(JSObject* object) : object(object), tag(object_tag) {}
        explicit JSValue(JSArray* array) : array(array), tag(array_tag) {}
        explicit JSValue(JSFunction* function) : function(function), tag(function_tag) {}
        explicit JSValue(JSString* string) : string(string), tag(string_tag) {}
        
        int32& operator=(int32 i32)                     { return (tag = i32_tag, this->i32 = i32); }
        float64& operator=(float64 f64)                 { return (tag = f64_tag, this->f64 = f64); }
        JSObject*& operator=(JSObject* object)          { return (tag = object_tag, this->object = object); }
        JSArray*& operator=(JSArray* array)             { return (tag = array_tag, this->array = array); }
        JSFunction*& operator=(JSFunction* function)    { return (tag = function_tag, this->function = function); }
        JSString*& operator=(JSString* string)          { return (tag = string_tag, this->string = string); }
        
        bool isString() const                           { return (tag == string_tag); }
        bool isNumber() const                           { return ((tag == f64_tag) || (tag == i32_tag)); }

        JSValue toString()                              { return (isString() ? *this : valueToString(*this)); }
        JSValue toNumber()                              { return (isNumber() ? *this : valueToNumber(*this)); }

        static JSValue valueToString(const JSValue& value);
        static JSValue valueToNumber(const JSValue& value);

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

    extern const JSValue kUndefinedValue;

    /**
     * Basic behavior of all JS objects, mapping a name to a value,
     * with prototype-based inheritance.
     */
    class JSObject : public gc_base {
    protected:
        typedef std::map<String, JSValue, std::less<String>, gc_map_allocator> JSProperties;
        JSProperties mProperties;
        JSObject* mPrototype;
    public:
        JSObject() : mPrototype(0) {}
    
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
        
        JSValue& setProperty(const String& name, const JSValue& value)
        {
            return (mProperties[name] = value);
        }

        void setPrototype(JSObject* prototype)
        {
            mPrototype = prototype;
        }
        
        JSObject* getPrototype()
        {
            return mPrototype;
        }
    };
    
    /**
     * Private representation of a JavaScript array.
     */
    class JSArray : public JSObject {
        JSValues elements;
    public:
        JSArray() : elements(1) {}
        JSArray(uint32 size) : elements(size) {}
        JSArray(const JSValues &v) : elements(v) {}
            
        uint32 length()
        {
            return elements.size();
        }
            
        JSValue& operator[](const JSValue& index)
        {
            // for now, we can only handle f64 index values.
            uint32 n = (uint32)index.f64;
            // obviously, a sparse representation might be better.
            uint32 size = elements.size();
            if (n >= size) expand(n, size);
            return elements[n];
        }
            
        JSValue& operator[](uint32 n)
        {
            // obviously, a sparse representation might be better.
            uint32 size = elements.size();
            if (n >= size) expand(n, size);
            return elements[n];
        }
            
        void resize(uint32 size)
        {
            elements.resize(size);
        }
            
    private:
        void expand(uint32 n, uint32 size)
        {
            do {
                size *= 2;
            } while (n >= size);
            elements.resize(size);
        }
    };
        
    /**
     * Private representation of a JS function. This simply
     * holds a reference to the iCode module that is the
     * compiled code of the function.
     */
    class JSFunction : public JSObject {
        ICodeModule* mICode;
    protected:
        JSFunction() : mICode(0) {}

   	    typedef JavaScript::gc_traits_finalizable<JSFunction> traits;
	    typedef gc_allocator<JSFunction, traits> allocator;
		
    public:
        JSFunction(ICodeModule* iCode) : mICode(iCode) {}
        ~JSFunction();
    
        void* operator new(size_t) { return allocator::allocate(1); }

        ICodeModule* getICode()    { return mICode; }
        virtual bool isNative()    { return false; }
    };

    class JSNativeFunction : public JSFunction {
    public:
        typedef JSValue (*JSCode)(const JSValues& argv);
        JSCode mCode;
        JSNativeFunction(JSCode code) : mCode(code) {}
        virtual bool isNative()    { return true; }
    };
        
    class JSException : public gc_base {
    public:
        JSException(JSValue v) : value(v) { }
        JSValue value;
    };
    
#if defined(XP_UNIX)
    // bastring.cc defines a funky operator new that assumes a byte-allocator.
    typedef string_char_traits<char16> JSCharTraits;
    typedef gc_allocator<_Char> JSStringAllocator;
#else
    typedef std::char_traits<char16> JSCharTraits;
    typedef gc_allocator<char16> JSStringAllocator;
#endif

    /**
     * Garbage collectable UNICODE string.
     */
    class JSString : public std::basic_string<char16, JSCharTraits, JSStringAllocator>, public gc_base {
    public:
        JSString() {}
        explicit JSString(const String& str);
        explicit JSString(const String* str);
        explicit JSString(const char* str);
    };

    inline Formatter& operator<<(Formatter& f, const JSString& str)
    {
        printString(f, str.begin(), str.end());
        return f;
    }

    /**
     * Provides a set of nested scopes. 
     */
    class JSScope : public JSObject {
    protected:
        JSScope* mParent;
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
            return getProperty(name);
        }
        
        JSValue& setVariable(const String& name, const JSValue& value)
        {
            return setProperty(name, value);
        }
        
        JSValue& defineVariable(const String& name, const JSValue& value)
        {
            return setProperty(name, value);
        }
        
        JSValue& defineFunction(const String& name, ICodeModule* iCode)
        {
            JSValue value(new JSFunction(iCode));
            return defineVariable(name, value);
        }

        JSValue& defineNativeFunction(const String& name, JSNativeFunction::JSCode code)
        {
            JSValue value(new JSNativeFunction(code));
            return defineVariable(name, value);
        }
    };
    
} /* namespace JSTypes */    
} /* namespace JavaScript */


#endif /* jstypes_h */
