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

#ifndef jsclasses_h
#define jsclasses_h

#include "jstypes.h"

namespace JavaScript {
namespace JSClasses {

    using JSTypes::JSValue;
    using JSTypes::JSObject;
    using JSTypes::JSType;
    using JSTypes::JSScope;
    using JSTypes::JSFunction;
    using JSTypes::FunctionMap;
    using ICG::ICodeModule;
    

    struct JSSlot {
        typedef enum { kNoFlag = 0, kIsConstructor = 0x01 } SlotFlags;   // <-- readonly, enumerable etc

        // a slot may have a getter or a setter or both, but NOT either if mActual
        JSType* mType;
        uint32 mIndex;
        SlotFlags mFlags;
        JSFunction* mGetter;
        JSFunction* mSetter;
        bool mActual;
        
        JSSlot() : mType(0), mFlags(kNoFlag), mGetter(0), mSetter(0), mActual(true)
        {
        }

        bool isConstructor() const          { return (mFlags & kIsConstructor) != 0; }
    };

    typedef gc_map_allocator(JSSlot) gc_slot_allocator;
    typedef std::map<String, JSSlot, std::less<const String>, gc_slot_allocator> JSSlots;


    typedef std::pair<String, JSFunction*> MethodEntry;
    typedef std::vector<MethodEntry> JSMethods;

    /**
     * Represents a class in the JavaScript 2 (ECMA 4) language.
     * Since a class defines a scope, and is defined in a scope,
     * a new scope is created whose parent scope is the scope of
     * class definition.
     */
    class JSClass : public JSType {
    protected:
        JSScope* mScope;        
        uint32 mSlotCount;
        JSSlots mSlots;
        uint32 mStaticCount;
        JSSlots mStaticSlots;
        JSValue* mStaticData;
        JSMethods mMethods;
        bool mHasGetters;           // tracks whether any getters/setters get assigned
        bool mHasSetters;
        JSFunction **mGetters;       // allocated at 'complete()' time
        JSFunction **mSetters;
    public:
        JSClass(JSScope* scope, const String& name, JSClass* superClass = 0)
            :   JSType(name, superClass),
                mScope(new JSScope(scope)),
                mSlotCount(superClass ? superClass->mSlotCount : 0),
                mStaticCount(0),
                mStaticData(0),
                mHasGetters(false), mHasSetters(false), mGetters(0), mSetters(0)
        {
            if (superClass) {
                   // inherit superclass methods
                JSMethods::iterator end = superClass->mMethods.end();
                for (JSMethods::iterator i = superClass->mMethods.begin(); i != end; i++)
                    mMethods.push_back(*i);
            }    
        }
        
        JSClass* getSuperClass()
        {
            return static_cast<JSClass*>(mBaseType);
        }
        
        JSScope* getScope()
        {
            return mScope;
        }

        const JSSlot& defineSlot(const String& name, JSType* type, JSFunction* getter = 0, JSFunction* setter = 0)
        {
            JSSlot& slot = mSlots[name];
            ASSERT(slot.mType == 0);
            slot.mType = type;
            slot.mIndex = mSlotCount++; // starts at 0.
            slot.mSetter = setter;
            slot.mGetter = getter;
            if (setter || getter)
                slot.mActual = false;
            return slot;
        }

        void setGetter(const String& name, JSFunction *getter, JSType* type)
        {
            JSSlots::iterator slti = mSlots.find(name);
            if (slti == mSlots.end())
                defineSlot(name, type, getter, 0);
            else {
                ASSERT(!slti->second.mActual);
                ASSERT(slti->second.mGetter == 0);
                slti->second.mGetter = getter;
            }
            mHasGetters = true;
        }
        
        void setSetter(const String& name, JSFunction *setter, JSType* type)
        {
            JSSlots::iterator slti = mSlots.find(name);
            if (slti == mSlots.end())
                defineSlot(name, type, 0, setter);
            else {
                ASSERT(!slti->second.mActual);
                ASSERT(slti->second.mSetter == 0);
                slti->second.mSetter = setter;
            }
            mHasSetters = true;
        }
        
        bool hasGetter(const String& name)
        {
            JSSlots::iterator slti = mSlots.find(name);
            return ((slti != mSlots.end()) && slti->second.mGetter);
        }
        
        bool hasSetter(const String& name)
        {
            JSSlots::iterator slti = mSlots.find(name);
            return ((slti != mSlots.end()) && slti->second.mSetter);
        }
        
        bool hasGetter(uint32 index)
        {
            return (mGetters && mGetters[index]);
        }
        
        bool hasSetter(uint32 index)
        {
            return (mSetters && mSetters[index]);
        }
        
        JSFunction* getter(uint32 index)
        {
            return mGetters[index];
        }
        
        JSFunction* setter(uint32 index)
        {
            return mSetters[index];
        }
        

        const JSSlot& getSlot(const String& name)
        {
            return mSlots[name];
        }
        
        bool hasSlot(const String& name)
        {
            return (mSlots.find(name) != mSlots.end());
        }
        
        bool hasGetterOrSetter(const String& name)
        {
            return (mSlots.find(name) != mSlots.end());
        }
        
        JSSlots& getSlots()
        {
            return mSlots;
        }
        
        uint32 getSlotCount()
        {
            return mSlotCount;
        }
        
        /**
         * Define a static/class variable.
         */
        const JSSlot& defineStatic(const String& name, JSType* type)
        {
            JSSlot& slot = mStaticSlots[name];
            ASSERT(slot.mType == 0);
            slot.mType = type;
            slot.mIndex = mStaticCount++;
            return slot;
        }

        const JSSlot& defineConstructor(const String& name)
        {
            JSSlot& slot = mStaticSlots[name];
            ASSERT(slot.mType == 0);
            slot.mType = &JSTypes::Function_Type;
            slot.mIndex = mStaticCount++;
            slot.mFlags = JSSlot::kIsConstructor;
            return slot;
        }
        
        const JSSlot& getStatic(const String& name)
        {
            return mStaticSlots[name];
        }
        
        bool hasStatic(const String& name, JSType*& type, bool &isConstructor)
        {
            JSSlots::const_iterator i = mStaticSlots.find(name);
            if (i != mStaticSlots.end()) {
                type = i->second.mType;
                isConstructor = i->second.isConstructor();
                return true;
            }
            return false;
        }

        bool hasStatic(const String& name)
        {
            return (mStaticSlots.find(name) != mStaticSlots.end());
        }

        bool complete()
        {
            if (mHasGetters || mHasSetters) {
                if (mHasGetters) mGetters = new JSFunction*[mSlotCount];
                if (mHasSetters) mSetters = new JSFunction*[mSlotCount];
                JSSlots::iterator end = mSlots.end();
                for (JSSlots::iterator i = mSlots.begin(); i != end; i++) {
                    if (mHasGetters) mGetters[i->second.mIndex] = i->second.mGetter;
                    if (mHasSetters) mSetters[i->second.mIndex] = i->second.mSetter;
                }
            }
            mStaticData = new JSValue[mStaticCount];
            return (mStaticData != 0);
        }

        JSValue& operator[] (uint32 index)
        {
            return mStaticData[index];
        }
        
        virtual void printProperties(Formatter& f)
        {
            f << "Properties:\n";
            JSObject::printProperties(f);
            f << "Statics:\n";
            printStatics(f);
        }

        void printStatics(Formatter& f)
        {
            JSClass* superClass = getSuperClass();
            if (superClass) superClass->printStatics(f);
            for (JSSlots::iterator i = mStaticSlots.begin(), end = mStaticSlots.end(); i != end; ++i) {
                f << i->first << " : " << mStaticData[i->second.mIndex]  << "\n";
            }
        }

        void defineMethod(const String& name, JSFunction *f)
        {
            uint32 slot;
            if (hasMethod(name, slot))
                mMethods[slot] = MethodEntry(name, f);
            else
                mMethods.push_back(MethodEntry(name, f));
        }

        bool hasMethod(const String& name, uint32& index)
        {
            JSMethods::iterator end = mMethods.end();
            for (JSMethods::iterator i = mMethods.begin(); i != end; i++) {
                if (i->first == name) {
                    index = static_cast<uint32>(i - mMethods.begin());
                    return true;
                }
            }
            return false;
        }

        JSFunction* getMethod(uint32 index)
        {
            return mMethods[index].second;
        }
    };

    /**
     * Represents an instance of a JSClass.
     */
    class JSInstance : public JSObject {
    protected:
        JSValue mSlots[1];
    public:
        void* operator new(size_t n, JSClass* thisClass)
        {
            uint32 slotCount = thisClass->getSlotCount();
            if (slotCount > 0) n += sizeof(JSValue) * (slotCount - 1);
            return gc_base::operator new(n);
        }
        
#if !defined(XP_MAC)
        void operator delete(void* /*ptr*/, JSClass* /*thisClass*/) {}
#endif
        
        JSInstance(JSClass* thisClass)
        {
            mType = thisClass;
            // initialize extra slots with undefined.
            uint32 slotCount = thisClass->getSlotCount();
            if (slotCount > 0) {
                std::uninitialized_fill(&mSlots[1], &mSlots[1] + (slotCount - 1),
                                        JSTypes::kUndefinedValue);
            }
            // for grins, use the prototype link to access methods.
            // setPrototype(thisClass->getScope());
        }
        
        JSFunction* getMethod(uint32 index)
        {
            return getClass()->getMethod(index);
        }

        JSClass* getClass()
        {
            return static_cast<JSClass*>(mType);
        }
        
        JSValue& operator[] (uint32 index)
        {
            return mSlots[index];
        }

        virtual void printProperties(Formatter& f)
        {
            f << "Properties:\n";
            JSObject::printProperties(f);
            f << "Slots:\n";
            printSlots(f, getClass());
        }

        bool hasGetter(uint32 index)
        {
            return getClass()->hasGetter(index);
        }
        
        bool hasSetter(uint32 index)
        {
            return getClass()->hasSetter(index);
        }
        
        JSFunction* getter(uint32 index)
        {
            return getClass()->getter(index);
        }

        JSFunction* setter(uint32 index)
        {
            return getClass()->setter(index);
        }


    private:
        void printSlots(Formatter& f, JSClass* thisClass)
        {
            JSClass* superClass = thisClass->getSuperClass();
            if (superClass) printSlots(f, superClass);
            JSSlots& slots = thisClass->getSlots();
            for (JSSlots::iterator i = slots.begin(), end = slots.end(); i != end; ++i) {
                f << i->first << " : " <<  mSlots[i->second.mIndex]  << "\n";
            }
        }
    };
    
} /* namespace JSClasses */
} /* namespace JavaScript */

#endif /* jsclasses_h */
