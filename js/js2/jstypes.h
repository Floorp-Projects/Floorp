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

#include <vector>
#include <map>
#include <stack>

#include "jstypes.h"
#include "gc_allocator.h"
#include "vmtypes.h"
#include "icodegenerator.h"

namespace JavaScript {
namespace JSTypes {
    using namespace VM;
    using namespace ICG;
    
    class JSObject;
    class JSArray;
    class JSFunction;
    
    /**
     * All JavaScript data types.
     */        
    union JSValue {
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
        
        JSValue() : f64(0.0) {}
            
        explicit JSValue(float64 f64) : f64(f64) {}
        explicit JSValue(JSObject* obj) : object(obj) {}
    };

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
     * Basic behavior of all JS objects, mapping a name to a value.
     * This is provided mainly to avoid having an awkward implementation
     * of JSObject & JSArray, which must each define its own
     * gc_allocator. This is all in flux.
     */
    class JSMap : public gc_base {
    protected:
        typedef std::map<String, JSValue, std::less<String>, gc_map_allocator> JSProperties;
        JSProperties mProperties;
        JSMap* mPrototype;
    public:
        JSValue& operator[](const String& name)
        {
            return mProperties[name];
        }
        
        const JSValue& getProperty(const String& name)
        {
        #ifdef XP_MAC
            JSProperties::const_iterator i = mProperties.find(name);
            if (i != mProperties.end())
                return i->second;
        #else
            // XXX should use map.find() to do this efficiently, but
            // unfortunately, find() returns an iterator that is different
            // on different STL implementations.
            if (mProperties.count(name))
                return mProperties[name];
        #endif
            if (mPrototype)
                return mPrototype->getProperty(name);
            return kUndefinedValue;
        }
        
        JSValue& setProperty(const String& name, const JSValue& value)
        {
            return (mProperties[name] = value);
        }
    };

    /**
     * Private representation of a JS function. This simply
     * holds a reference to the iCode module that is the
     * compiled code of the function.
     */
    class JSFunction : public JSMap {
        ICodeModule* mICode;
    public:
        JSFunction(ICodeModule* iCode) : mICode(iCode) {}
        ICodeModule* getICode() { return mICode; }
    };
        
    /**
     * Private representation of a JavaScript object.
     * This will change over time, so it is treated as an opaque
     * type everywhere else but here.
     */
    class JSObject : public JSMap {
    public:
        JSValue& defineProperty(const String& name, JSValue &v)
        {
            return (mProperties[name] = v);
        }
                
        // FIXME:  need to copy the ICodeModule's instruction stream.    
        JSValue& defineFunction(const String& name, ICodeModule* iCode)
        {
            JSValue value;
            value.function = new JSFunction(iCode);
            return mProperties[name] = value;
        }
    };
    
    /**
     * Private representation of a JavaScript array.
     */
    class JSArray : public JSMap {
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
        
    class JS_Exception : public gc_base {
    public:
        JS_Exception() { }
    };
        
} /* namespace JSTypes */    
} /* namespace JavaScript */


#endif /* jstypes_h */
