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
    using ICG::ICodeModule;
    
    struct JSSlot {
        JSType* mType;
        uint32 mIndex;
        
        JSSlot() : mType(0)
        {
        }
    };

#if defined(XP_MAC)
    // copied from default template parameters in map.
    typedef gc_allocator<std::pair<const String, JSSlot> > gc_slot_allocator;
#elif defined(XP_UNIX)
    typedef JSTypes::gc_map_allocator gc_slot_allocator;
#elif defined(_WIN32)
    typedef gc_allocator<JSSlot> gc_slot_allocator;
#endif

     typedef std::map<String, JSSlot, std::less<const String>, gc_slot_allocator> JSSlots;

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
        ICodeModule* mConstructor;
        // typedef std::vector<ICodeModule*, gc_allocator<ICodeModule*> > JSMethods;
        // JSMethods mMethods;
    public:
        JSClass(JSScope* scope, const String& name, JSClass* superClass = 0)
            :   JSType(name, superClass),
                mScope(new JSScope(scope)),
                mSlotCount(superClass ? superClass->mSlotCount : 0),
                mStaticCount(0),
                mStaticData(0),
                mConstructor(0)
        {
            // to "inherit" superClass methods.
            if (superClass)
                mScope->setPrototype(superClass->mScope);
        }
        
        JSClass* getSuperClass()
        {
            return static_cast<JSClass*>(mBaseType);
        }
        
        JSScope* getScope()
        {
            return mScope;
        }

        void setConstructor(ICodeModule* ctor)
        {
            mConstructor = ctor;
        }
        
        ICodeModule* getConstructor()
        {
            return mConstructor;
        }
        
        const JSSlot& defineSlot(const String& name, JSType* type)
        {
            JSSlot& slot = mSlots[name];
            ASSERT(slot.mType == 0);
            slot.mType = type;
            slot.mIndex = mSlotCount++; // starts at 0.
            return slot;
        }
        
        const JSSlot& getSlot(const String& name)
        {
            return mSlots[name];
        }
        
        bool hasSlot(const String& name)
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
        
        const JSSlot& getStatic(const String& name)
        {
            return mStaticSlots[name];
        }
        
        bool hasStatic(const String& name)
        {
            return (mStaticSlots.find(name) != mStaticSlots.end());
        }

        bool complete()
        {
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
            setPrototype(thisClass->getScope());
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
