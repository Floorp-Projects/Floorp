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


#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "parser.h"
#include "numerics.h"
#include "js2runtime.h"
#include "bytecodegen.h"

#include "jsstring.h"
#include "jsarray.h"
#include "jsmath.h"
#include "jsdate.h"

#include "fdlibm_ns.h"

// this is the AttributeList passed to the name lookup routines
#define CURRENT_ATTR    (NULL)

namespace JavaScript {    
namespace JS2Runtime {


//
// XXX don't these belong in the context? But don't
// they need to compare equal across contexts?
//
JSType *Object_Type = NULL;
JSType *Number_Type;
JSType *Integer_Type;
JSType *Function_Type;
JSStringType *String_Type;
JSType *Boolean_Type;
JSType *Type_Type;
JSType *Void_Type;
JSType *Null_Type;
JSType *Unit_Type;
JSType *Attribute_Type;
JSType *NamedArgument_Type;
JSArrayType *Array_Type;
JSType *Date_Type;


Attribute *Context::executeAttributes(ExprNode *attr)
{
    ASSERT(attr);

    ByteCodeGen bcg(this, mScopeChain);
    ByteCodeModule *bcm = bcg.genCodeForExpression(attr);
//    stdOut << *bcm;
    JSValue result = interpret(bcm, 0, NULL, JSValue(getGlobalObject()), NULL, 0);

    ASSERT(result.isObject() && (result.object->getType() == Attribute_Type));
    return static_cast<Attribute *>(result.object);
}


// Find a property with the given name, but make sure it's in
// the supplied namespace. XXX speed up! XXX
//
PropertyIterator JSObject::findNamespacedProperty(const String &name, NamespaceList *names)
{
    for (PropertyIterator i = mProperties.lower_bound(name), 
                    end = mProperties.upper_bound(name); (i != end); i++) {
        NamespaceList *propNames = PROPERTY_NAMESPACELIST(i);
        if (names) {
            if (propNames == NULL)
                continue;       // a namespace list was specified, no match
            while (names) {
                NamespaceList *propNameEntry = propNames;
                while (propNameEntry) {
                    if (names->mName == propNameEntry->mName)
                        return i;
                    propNameEntry = propNameEntry->mNext;
                }
                names = names->mNext;
            }
        }
        else {
            if (propNames)  // entry is in a namespace, but none called for, no match
                continue;
            return i;
        }
    }
    return mProperties.end();
}



/*---------------------------------------------------------------------------------------------*/











// see if the property exists by a specific kind of access
bool JSObject::hasOwnProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    *p = findNamespacedProperty(name, names);
    if (*p != mProperties.end()) {
        Property *prop = PROPERTY(*p);
        if (prop->mFlag == FunctionPair)
            return (acc == Read) ? (prop->mData.fPair.getterF != NULL)
                                 : (prop->mData.fPair.setterF != NULL);
        else
            if (prop->mFlag == IndexPair)
                return (acc == Read) ? (prop->mData.iPair.getterI != toUInt32(-1))
                                     : (prop->mData.iPair.setterI != toUInt32(-1));
            else
                return true;
    }
    else
        return false;
}

bool JSObject::hasProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    if (hasOwnProperty(name, names, acc, p))
        return true;
    else
        if (mPrototype)
            return mPrototype->hasProperty(name, names, acc, p);
        else
            return false;
}

bool JSObject::deleteProperty(const String &name, NamespaceList *names)
{    
    PropertyIterator i = findNamespacedProperty(name, names);
    if ((PROPERTY_ATTR(i) & Property::DontDelete) == 0) {
        mProperties.erase(i);
        return true;
    }
    return false;
}


// get a property value
JSValue JSObject::getPropertyValue(PropertyIterator &i)
{
    Property *prop = PROPERTY(i);
    ASSERT(prop->mFlag == ValuePointer);
    return *prop->mData.vp;
}

Property *JSObject::insertNewProperty(const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const JSValue &v)
{
    Property *prop = new Property(new JSValue(v), type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}

void JSObject::defineGetterMethod(Context * /*cx*/, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator i;
    if (hasProperty(name, names, Write, &i)) {
        ASSERT(PROPERTY_KIND(i) == FunctionPair);
        ASSERT(PROPERTY_GETTERF(i) == NULL);
        PROPERTY_GETTERF(i) = f;
    }
    else {
        const PropertyMap::value_type e(name, new NamespacedProperty(new Property(Function_Type, f, NULL, attrFlags), names));
        mProperties.insert(e);
    }
}
void JSObject::defineSetterMethod(Context * /*cx*/, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator i;
    if (hasProperty(name, names, Read, &i)) {
        ASSERT(PROPERTY_KIND(i) == FunctionPair);
        ASSERT(PROPERTY_SETTERF(i) == NULL);
        PROPERTY_SETTERF(i) = f;
    }
    else {
        const PropertyMap::value_type e(name, new NamespacedProperty(new Property(Function_Type, NULL, f, attrFlags), names));
        mProperties.insert(e);
    }
}

uint32 JSObject::tempVarCount = 0;

void JSObject::defineTempVariable(Context *cx, Reference *&readRef, Reference *&writeRef, JSType *type)
{
    char buf[32];
    sprintf(buf, "%%tempvar%%_%d", tempVarCount++);
    const String &name = cx->mWorld.identifiers[buf];
    /* Property *prop = */defineVariable(cx, name, (NamespaceList *)NULL, Property::NoAttribute, type);
    readRef = new NameReference(name, Read, Object_Type, 0);
    writeRef = new NameReference(name, Write, Object_Type, 0);
}


// add a property
Property *JSObject::defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator it;
    if (hasOwnProperty(name, names, Read, &it)) {
        // not a problem if neither are consts
        if ((attrFlags & Property::Const)
               || (PROPERTY_ATTR(it) & Property::Const)) {
            if (attr)
                cx->reportError(Exception::typeError, "Duplicate definition '{0}'", attr->pos, name);
            else
                cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
    }

    Property *prop = new Property(new JSValue(), type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}
Property *JSObject::defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type)
{
    PropertyIterator it;
    if (hasOwnProperty(name, names, Read, &it)) {
        // not a problem if neither are consts
        if (PROPERTY_ATTR(it) & Property::Const) {
            cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
    }

    Property *prop = new Property(new JSValue(), type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}

// add a property (with a value)
Property *JSObject::defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type, const JSValue v)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator it;
    if (hasOwnProperty(name, names, Read, &it)) {
        // not a problem if neither are consts
        if ((attrFlags & Property::Const)
               || (PROPERTY_ATTR(it) & Property::Const)) {
            if (attr)
                cx->reportError(Exception::typeError, "Duplicate definition '{0}'", attr->pos, name);
            else
                cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
        else {
            // override the existing value
            PROPERTY_VALUEPOINTER(it) = new JSValue(v);
            return PROPERTY(it);
        }
    }

    Property *prop = new Property(new JSValue(v), type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}
Property *JSObject::defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const JSValue v)
{
    PropertyIterator it;
    if (hasOwnProperty(name, names, Read, &it)) {
        if (PROPERTY_ATTR(it) & Property::Const) {
            cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
    }

    Property *prop = new Property(new JSValue(v), type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}



Reference *JSObject::genReference(bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 /*depth*/)
{
    PropertyIterator i;
    if (hasProperty(name, names, acc, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case ValuePointer:
            if (hasBase)
                return new PropertyReference(name, acc, prop->mType, prop->mAttributes);
            else
                return new NameReference(name, acc, prop->mType, prop->mAttributes);
        case FunctionPair:
            if (acc == Read)
                return new GetterFunctionReference(prop->mData.fPair.getterF, prop->mAttributes);
            else {
                JSFunction *f = prop->mData.fPair.setterF;
                return new SetterFunctionReference(f, f->getArgType(0), prop->mAttributes);
            }
        default:
            NOT_REACHED("bad storage kind");
            return NULL;
        }
    }
    NOT_REACHED("bad genRef call");
    return NULL;
}


void JSObject::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    PropertyIterator i;
    if (hasOwnProperty(name, names, Read, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case ValuePointer:
            cx->pushValue(*prop->mData.vp);
            break;
        case FunctionPair:
             cx->pushValue(cx->invokeFunction(prop->mData.fPair.getterF, JSValue(this), NULL, 0));
            break;
        case Constructor:
        case Method:
            cx->pushValue(JSValue(mType->mMethods[prop->mData.index]));
            break;
        default:
            ASSERT(false);  // XXX more to implement
            break;
        }
    }
    else {
        if (mPrototype)
            mPrototype->getProperty(cx, name, names);
        else
            cx->pushValue(kUndefinedValue);        
    }
}


void JSType::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    PropertyIterator i;
    if (hasOwnProperty(name, names, Read, &i))
        JSObject::getProperty(cx, name, names);
    else
        if (mSuperType)
            mSuperType->getProperty(cx, name, names);
        else
            JSObject::getProperty(cx, name, names);
}

void JSInstance::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    PropertyIterator i;
    if (hasOwnProperty(name, names, Read, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case Slot:
            cx->pushValue(mInstanceValues[prop->mData.index]);
            break;
        case ValuePointer:
            cx->pushValue(*prop->mData.vp);
            break;
        case FunctionPair:
            cx->pushValue(cx->invokeFunction(prop->mData.fPair.getterF, JSValue(this), NULL, 0));
            break;
        case Constructor:
        case Method:
            cx->pushValue(JSValue(mType->mMethods[prop->mData.index]));
            break;
        case IndexPair:
            cx->pushValue(cx->invokeFunction(mType->mMethods[prop->mData.iPair.getterI], JSValue(this), NULL, 0));
            break;
        default:
            ASSERT(false);  // XXX more to implement
            break;
        }
    }
    else
        if (mType->hasOwnProperty(name, names, Read, &i))
            mType->getProperty(cx, name, names);
        else
            JSObject::getProperty(cx, name, names);
}

void JSObject::setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v)
{
    PropertyIterator i;
    if (hasProperty(name, names, Write, &i)) {
        if (PROPERTY_ATTR(i) & Property::ReadOnly) return;
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case ValuePointer:
            *prop->mData.vp = v;
            break;
        case FunctionPair:
            {
                JSValue argv = v;
                cx->invokeFunction(prop->mData.fPair.setterF, JSValue(this), &argv, 1);
            }
            break;
        default:
            ASSERT(false);  // XXX more to implement ?
            break;
        }
    }
    else
        defineVariable(cx, name, names, Property::Enumerable, Object_Type, v);
}

void JSType::setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v)
{
    PropertyIterator i;
    if (hasOwnProperty(name, names, Read, &i))
        JSObject::setProperty(cx, name, names, v);
    else
        if (mSuperType)
            mSuperType->setProperty(cx, name, names, v);
        else
            JSObject::setProperty(cx, name, names, v);
}

void JSInstance::setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v)
{
    PropertyIterator i;
    if (hasOwnProperty(name, names, Write, &i)) {
        if (PROPERTY_ATTR(i) & Property::ReadOnly) return;
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case Slot:
            mInstanceValues[prop->mData.index] = v;
            break;
        case ValuePointer:
            *prop->mData.vp = v;
            break;
        case FunctionPair: 
            {
                JSValue argv = v;
                cx->invokeFunction(prop->mData.fPair.setterF, JSValue(this), &argv, 1);
            }
            break;
        case IndexPair: 
            {
                JSValue argv = v;
                cx->invokeFunction(mType->mMethods[prop->mData.iPair.setterI], JSValue(this), &argv, 1);
            }
            break;
        default:
            ASSERT(false);  // XXX more to implement ?
            break;
        }
    }
    else {
        // the instance doesn't have this property, see if
        // the type does...
        if (mType->hasOwnProperty(name, names, Write, &i))
            mType->setProperty(cx, name, names, v);
        else
            defineVariable(cx, name, names, Property::Enumerable, Object_Type, v);
    }
}

void JSArrayInstance::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        cx->pushValue(JSValue((float64)mLength));
    else
        JSInstance::getProperty(cx, name, names);
}

void JSArrayInstance::setProperty(Context *cx, const String &name, NamespaceList *names, const JSValue &v)
{
    if (name.compare(cx->Length_StringAtom) == 0) {
        uint32 newLength = (uint32)(v.toUInt32(cx).f64);
        if (newLength != v.toNumber(cx).f64)
            cx->reportError(Exception::rangeError, "out of range value for length"); 
        
        for (uint32 i = newLength; i < mLength; i++) {
            const String *id = numberToString(i);
            if (findNamespacedProperty(*id, NULL) != mProperties.end())
                deleteProperty(*id, NULL);
            delete id;
        }

        mLength = newLength;
    }
    else {
        PropertyIterator it = findNamespacedProperty(name, names);
        if (it == mProperties.end())
            insertNewProperty(name, names, 0, Object_Type, v);
        else {
            Property *prop = PROPERTY(it);
            ASSERT(prop->mFlag == ValuePointer);
            *prop->mData.vp = v;
        }
        JSValue v = JSValue(&name);
        JSValue v_int = v.toUInt32(cx);
        if ((v_int.f64 != two32minus1) && (v_int.toString(cx).string->compare(name) == 0)) {
            if (v_int.f64 >= mLength)
                mLength = (uint32)(v_int.f64) + 1;
        }
    }
}

// get a named property from a string instance, but intercept
// 'length' by returning the known value
void JSStringInstance::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    if (name.compare(cx->Length_StringAtom) == 0) {
        cx->pushValue(JSValue((float64)mLength));
    }
    else
        JSInstance::getProperty(cx, name, names);
}

// construct an instance of a type
// - allocate memory for the slots, load the instance variable names into the 
// property map. 
void JSInstance::initInstance(Context *cx, JSType *type)
{
    if (type->mVariableCount)
        mInstanceValues = new JSValue[type->mVariableCount];

    // copy the instance variable names into the property map
    for (PropertyIterator pi = type->mProperties.begin(), 
                end = type->mProperties.end();
                (pi != end); pi++) {            
        const PropertyMap::value_type e(PROPERTY_NAME(pi), NAMESPACED_PROPERTY(pi));
        mProperties.insert(e);
    }

    // and then do the same for the super types
    JSType *t = type->mSuperType;
    while (t) {
        for (PropertyIterator i = t->mProperties.begin(), 
                    end = t->mProperties.end();
                    (i != end); i++) {            
            const PropertyMap::value_type e(PROPERTY_NAME(i), NAMESPACED_PROPERTY(i));
            mProperties.insert(e);            
        }
        if (t->mInstanceInitializer)
            cx->invokeFunction(t->mInstanceInitializer, JSValue(this), NULL, 0);
        t = t->mSuperType;
    }

    // run the initializer
    if (type->mInstanceInitializer) {
        cx->invokeFunction(type->mInstanceInitializer, JSValue(this), NULL, 0);
    }

    mType = type;
}

// Create a new (empty) instance of this class. The prototype
// link for this new instance is established from the type's
// prototype object.
JSInstance *JSType::newInstance(Context *cx)
{
    JSInstance *result = new JSInstance(cx, this);
    result->mPrototype = mPrototypeObject;
    return result;
}

// the function 'f' gets executed each time a new instance is created
void JSType::setInstanceInitializer(Context * /*cx*/, JSFunction *f)
{
    mInstanceInitializer = f;
}

// Run the static initializer against this type
void JSType::setStaticInitializer(Context *cx, JSFunction *f)
{
    if (f)
        cx->interpret(f->getByteCode(), 0, f->getScopeChain(), JSValue(this), NULL, 0);
}

Property *JSType::defineVariable(Context * /*cx*/, const String& name, AttributeStmtNode *attr, JSType *type)
{
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    Property *prop = new Property(mVariableCount++, type, Slot, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, (attr) ? attr->attributeValue->mNamespaceList : NULL));
    mProperties.insert(e);
    return prop;
}

JSInstance *JSArrayType::newInstance(Context *cx)
{
    JSInstance *result = new JSArrayInstance(cx, this);
    result->mPrototype = mPrototypeObject;
    return result;
}

JSInstance *JSStringType::newInstance(Context *cx)
{
    JSInstance *result = new JSStringInstance(cx, this);
    result->mPrototype = mPrototypeObject;
    return result;
}

// don't add to instances etc., climb all the way down (likely to the global object)
// and add the property there.
void ScopeChain::setNameValue(Context *cx, const String& name, AttributeStmtNode *attr)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    JSValue v = cx->topValue();
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(name, names, Write, &i)) {
            PropertyFlag flag = PROPERTY_KIND(i);
            switch (flag) {
            case ValuePointer:
                *PROPERTY_VALUEPOINTER(i) = v;
                break;
            case Slot:
                (*s)->setSlotValue(cx, PROPERTY_INDEX(i), v);
                break;
            default:
                ASSERT(false);      // what else needs to be implemented ?
            }
            return;
        }
    }
    cx->getGlobalObject()->defineVariable(cx, name, attr, Object_Type, v);
}

inline char narrow(char16 ch) { return char(ch); }

JSObject *ScopeChain::getNameValue(Context *cx, const String& name, AttributeStmtNode *attr)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(name, names, Read, &i)) {
            PropertyFlag flag = PROPERTY_KIND(i);
            switch (flag) {
            case ValuePointer:
                cx->pushValue(*PROPERTY_VALUEPOINTER(i));
                break;
            case Slot:
                cx->pushValue((*s)->getSlotValue(cx, PROPERTY_INDEX(i)));
                break;
            default:
                ASSERT(false);      // what else needs to be implemented ?
            }
            return *s;
        }
    }    
    m_cx->reportError(Exception::referenceError, "'{0}' not defined", name );
    return NULL;
}

Reference *ScopeChain::getName(const String& name, NamespaceList *names, Access acc)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(name, names, acc, &i))
            return (*s)->genReference(false, name, names, acc, depth);
        else
            if ((*s)->isDynamic())
                return NULL;

    }
    return NULL;
}

bool ScopeChain::hasNameValue(const String& name, NamespaceList *names)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(name, names, Read, &i))
            return true;
    }
    return false;
}

// a compile time request to get the value for a name
// (i.e. we're accessing a constant value)
JSValue ScopeChain::getCompileTimeValue(const String& name, NamespaceList *names)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(name, names, Read, &i)) 
            return (*s)->getPropertyValue(i);
    }
    return kUndefinedValue;
}



Reference *ParameterBarrel::genReference(bool /* hasBase */, const String& name, NamespaceList *names, Access acc, uint32 /*depth*/)
{
    PropertyIterator i;
    if (hasProperty(name, names, acc, &i)) {
        Property *prop = PROPERTY(i);
        ASSERT(prop->mFlag == Slot);
        return new ParameterReference(prop->mData.index, acc, prop->mType, prop->mAttributes);
    }
    NOT_REACHED("bad genRef call");
    return NULL;
}

JSValue ParameterBarrel::getSlotValue(Context *cx, uint32 slotIndex)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        return prev->mArgumentBase[slotIndex];        
    }
    return cx->mArgumentBase[slotIndex];
}

void ParameterBarrel::setSlotValue(Context *cx, uint32 slotIndex, JSValue &v)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        prev->mArgumentBase[slotIndex] = v;        
    }
    else
        cx->mArgumentBase[slotIndex] = v;
}



JSValue Activation::getSlotValue(Context *cx, uint32 slotIndex)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        return prev->mLocals[slotIndex];        
    }
    return cx->mLocals[slotIndex];
}

void Activation::setSlotValue(Context *cx, uint32 slotIndex, JSValue &v)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        prev->mLocals[slotIndex] = v;
    }
    else
        cx->mLocals[slotIndex] =v;
}







JSType *ScopeChain::findType(const StringAtom& typeName, size_t pos) 
{
    JSValue v = getCompileTimeValue(typeName, NULL);
    if (!v.isUndefined()) {
        if (v.isType())
            return v.type;
        else {
            // Allow finding a function that has the same name as it's containing class
            // i.e. the default constructor.
            FunctionName *fnName = v.function->getFunctionName();
            if ((fnName->prefix == FunctionName::normal)
                    && v.isFunction() && v.function->getClass() 
                            && (v.function->getClass()->mClassName->compare(*fnName->name) == 0))
                return v.function->getClass();
            m_cx->reportError(Exception::semanticError, "Unknown type", pos);
            return NULL;
        }
    }
    return NULL;
}

// Take the specified type in 't' and see if we have a compile-time
// type value for it. FindType will throw an error if a type by
// that name doesn't exist.
JSType *ScopeChain::extractType(ExprNode *t)
{
    JSType *type = Object_Type;
    if (t) {
        switch (t->getKind()) {
        case ExprNode::identifier:
            {
                IdentifierExprNode* typeExpr = checked_cast<IdentifierExprNode *>(t);
                type = findType(typeExpr->name, t->pos);
            }
            break;
        case ExprNode::index:
            // array type
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(t);
                JSType *base = extractType(i->op);
                JSType *element = Object_Type;
                ExprPairList *p = i->pairs;
                if (p != NULL) {
                    element = extractType(p->value);
                    ASSERT(p->next == NULL);
                }
                ASSERT(base == Array_Type);
                if (element == Object_Type)
                    type = Array_Type;
                else {
                    type = new JSArrayType(m_cx, element, NULL, Object_Type);    // XXX or is this a descendat of Array[Object]?
                    type->setDefaultConstructor(m_cx, Array_Type->getDefaultConstructor());
                }
            }
            break;
        default:
            NOT_REACHED("implement me - more complex types");
            break;
        }
    }
    return type;
}

// return the type of the index'th parameter in function
JSType *Context::getParameterType(FunctionDefinition &function, int index)
{
    VariableBinding *v = function.parameters;
    while (v) {
        if (index-- == 0)
            return mScopeChain->extractType(v->type);
        else
            v = v->next;
    }
    return NULL;
}

// counts the number of pigs that can fit in small wicker basket
uint32 Context::getParameterCount(FunctionDefinition &function)
{
    uint32 count = 0;
    VariableBinding *v = function.parameters;
    while (v) {
        count++;
        v = v->next;
    }
    return count;
}


// Iterates over the linked list of statements, p.
// 1. Adds 'symbol table' entries for each class, var & function
// 2. Using information from pass 1, evaluate types (and XXX later XXX other
//      compile-time constants)
void Context::buildRuntime(StmtNode *p)
{
    ContextStackReplacement csr(this);

    mScopeChain->addScope(getGlobalObject());
    while (p) {
        mScopeChain->collectNames(p);         // adds declarations for each top-level entity in p
        buildRuntimeForStmt(p);               // adds definitions as they exist for ditto
        p = p->next;
    }
    mScopeChain->popScope();
}

// Generate bytecode for the linked list of statements in p
JS2Runtime::ByteCodeModule *Context::genCode(StmtNode *p, const String &/*sourceName*/)
{
    mScopeChain->addScope(getGlobalObject());
    JS2Runtime::ByteCodeGen bcg(this, mScopeChain);
    JS2Runtime::ByteCodeModule *result = bcg.genCodeForScript(p);
    mScopeChain->popScope();
    return result;
}

/*  Make sure that:
     the function is not a class or interface member; 
     the function has no optional, named, or rest parameters; 
     none of the function's parameters has a declared type; 
     the function does not have a declared return type; 
     the function is not a getter or setter. 
*/
bool ScopeChain::isPossibleUncheckedFunction(FunctionDefinition *f)
{
    bool result = false;
    if ((f->resultType == NULL)
            && (f->restParameter == NULL)
            && (f->optParameters == NULL)
            && (f->prefix == FunctionName::normal)
            && (topClass() == NULL)) {
        result = true;
        VariableBinding *b = f->parameters;
        while (b) {
            if (b->type != NULL) {
                    result = false;
                    break;
            }
            b = b->next;
        }
    }
    return result;
}

//  The first pass over the tree - it just installs the names of each declaration
void ScopeChain::collectNames(StmtNode *p)
{
    switch (p->getKind()) {
        // XXX - other statements, execute them (assuming they have constant control values) ?
        // or simply visit the contained blocks and process any references that need to be hoisted
    case StmtNode::Class:
        {
            ClassStmtNode *classStmt = checked_cast<ClassStmtNode *>(p);
            const StringAtom *name = &classStmt->name;
            JSType *thisClass = new JSType(m_cx, name, NULL);

            m_cx->setAttributeValue(classStmt, 0);              // XXX default attribute for a class?

            PropertyIterator it;
            if (hasProperty(*name, NULL, Read, &it))
                m_cx->reportError(Exception::referenceError, "Duplicate class definition", p->pos);

            defineVariable(m_cx, *name, classStmt, Type_Type, JSValue(thisClass));
            classStmt->mType = thisClass;
        }
        break;
    case StmtNode::label:
        {
            LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
            collectNames(l->stmt);
        }
        break;
    case StmtNode::block:
        {
            // should push a new Activation scope here?
            BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
            StmtNode *s = b->statements;
            while (s) {
                collectNames(s);
                s = s->next;
            }            
        }
        break;
    case StmtNode::If:
    case StmtNode::With:
    case StmtNode::DoWhile:
    case StmtNode::While:
        {
            UnaryStmtNode *u = checked_cast<UnaryStmtNode *>(p);
            collectNames(u->stmt);
        }
        break;
    case StmtNode::IfElse:
        {
            BinaryStmtNode *b = checked_cast<BinaryStmtNode *>(p);
            collectNames(b->stmt);
            collectNames(b->stmt2);
        }
        break;
    case StmtNode::Try:
        {
            TryStmtNode *t = checked_cast<TryStmtNode *>(p);
            if (t->catches) {
                CatchClause *c = t->catches;
                while (c) {
                    c->prop = defineVariable(m_cx, c->name, NULL, NULL);
                    c = c->next;
                }
            }
        }
        break;
    case StmtNode::For:
    case StmtNode::ForIn:
        {
            ForStmtNode *f = checked_cast<ForStmtNode *>(p);
            if (f->initializer) collectNames(f->initializer);
            collectNames(f->stmt);
        }
        break;
    case StmtNode::Const:
    case StmtNode::Var:
        {
            VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);
            VariableBinding *v = vs->bindings;
            m_cx->setAttributeValue(vs, Property::Final);
            if (p->getKind() == StmtNode::Const)
                vs->attributeValue->mTrueFlags |= Property::Const;
            bool isStatic = (vs->attributeValue->mTrueFlags & Property::Static) == Property::Static;
            if ((vs->attributeValue->mTrueFlags & Property::Private) == Property::Private) {
                JSType *theClass = topClass();
                if (theClass == NULL)
                    m_cx->reportError(Exception::typeError, "Private can only be used inside a class");
                vs->attributeValue->mNamespaceList = new NamespaceList(theClass->mPrivateNamespace, vs->attributeValue->mNamespaceList);
            }

            while (v)  {
                if (isStatic)
                    v->prop = defineStaticVariable(m_cx, *v->name, vs, NULL);
                else
                    v->prop = defineVariable(m_cx, *v->name, vs, NULL);
                v = v->next;
            }
        }
        break;
    case StmtNode::Function:
        {
            FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
            m_cx->setAttributeValue(f, Property::Virtual);

            bool isStatic = (f->attributeValue->mTrueFlags & Property::Static) == Property::Static;
            bool isConstructor = (f->attributeValue->mTrueFlags & Property::Constructor) == Property::Constructor;
            bool isOperator = (f->attributeValue->mTrueFlags & Property::Operator) == Property::Operator;
            bool isPrototype = (f->attributeValue->mTrueFlags & Property::Prototype) == Property::Prototype;
            
            JSFunction *fnc = new JSFunction(m_cx, NULL, this);

            /* Determine whether a function is unchecked, which is the case if -
                 XXX strict mode is disabled at the point of the function definition; 
                 the function is not a class or interface member; 
                 the function has no optional, named, or rest parameters; 
                 none of the function's parameters has a declared type; 
                 the function does not have a declared return type; 
                 the function is not a getter or setter. 
            */
            if (!isPrototype
                    && (!isOperator)
		    && isPossibleUncheckedFunction(&f->function)) {
                isPrototype = true;
                fnc->setIsUnchecked();
            }

            fnc->setIsPrototype(isPrototype);
            fnc->setIsConstructor(isConstructor);
            fnc->setFunctionName(&f->function);
            f->mFunction = fnc;

            uint32 reqArgCount = 0;
            uint32 optArgCount = 0;

            VariableBinding *b = f->function.parameters;
            while ((b != f->function.optParameters) && (b != f->function.restParameter)) {
                reqArgCount++;
                b = b->next;
            }
            while (b != f->function.restParameter) {
                optArgCount++;
                b = b->next;
            }
            fnc->setArgCounts(m_cx, reqArgCount, optArgCount, (f->function.restParameter != NULL));

            if (isOperator) {
                // no need to do anything yet, all operators are 'pre-declared'
            }
            else {
                const StringAtom& name = *f->function.name;
                if (topClass())
                    fnc->setClass(topClass());

                if ((f->attributeValue->mTrueFlags & Property::Extend) == Property::Extend) {

                    JSType *extendedClass = f->attributeValue->mExtendArgument;
                
                    // sort of want to fall into the code below, but use 'extendedClass' instead
                    // of whatever the topClass will turn out to be.
                    if (extendedClass->mClassName->compare(name) == 0) {
                        isConstructor = true;       // can you add constructors?
                        fnc->setIsConstructor(true);
                    }
                    if (isConstructor)
                        extendedClass->defineConstructor(m_cx, name, f, fnc);
                    else {
                        switch (f->function.prefix) {
                        case FunctionName::Get:
                            if (isStatic)
                                extendedClass->defineStaticGetterMethod(m_cx, name, f, fnc);
                            else
                                extendedClass->defineGetterMethod(m_cx, name, f, fnc);
                            break;
                        case FunctionName::Set:
                            if (isStatic)
                                extendedClass->defineStaticSetterMethod(m_cx, name, f, fnc);
                            else
                                extendedClass->defineSetterMethod(m_cx, name, f, fnc);
                            break;
                        case FunctionName::normal:
                            f->attributeValue->mTrueFlags |= Property::Const;
                            if (isStatic)
                                extendedClass->defineStaticMethod(m_cx, name, f, fnc);
                            else
                                extendedClass->defineMethod(m_cx, name, f, fnc);
                            break;
                        default:
                            NOT_REACHED("***** implement me -- throw an error because the user passed a quoted function name");
                            break;
                        }
                    }                    
                }
                else {
                    bool isDefaultConstructor = false;
                    if (topClass() && (topClass()->mClassName->compare(name) == 0)) {
                        isConstructor = true;
                        fnc->setIsConstructor(true);
                        isDefaultConstructor = true;
                    }
                    if (!isNestedFunction()) {
                        if (isConstructor) {
                            defineConstructor(m_cx, name, f, fnc);
                            if (isDefaultConstructor)
                                topClass()->setDefaultConstructor(m_cx, fnc);
                        }
                        else {
                            switch (f->function.prefix) {
                            case FunctionName::Get:
                                if (isStatic)
                                    defineStaticGetterMethod(m_cx, name, f, fnc);
                                else
                                    defineGetterMethod(m_cx, name, f, fnc);
                                break;
                            case FunctionName::Set:
                                if (isStatic)
                                    defineStaticSetterMethod(m_cx, name, f, fnc);
                                else
                                    defineSetterMethod(m_cx, name, f, fnc);
                                break;
                            case FunctionName::normal:
                                // make a function into a const declaration, but only if any types
                                // have been specified - otherwise it's a 1.5 atyle definition and
                                // duplicates are allowed
                                if (!isPossibleUncheckedFunction(&f->function))
                                    f->attributeValue->mTrueFlags |= Property::Const;
                                if (isStatic)
                                    defineStaticMethod(m_cx, name, f, fnc);
                                else
                                    defineMethod(m_cx, name, f, fnc);
                                break;
                            default:
                                NOT_REACHED("***** implement me -- throw an error because the user passed a quoted function name");
                                break;
                            }
                        }
                    }
                }
            }
        }
        break;
    case StmtNode::Namespace:
        {
            NamespaceStmtNode *n = checked_cast<NamespaceStmtNode *>(p);
            Attribute *x = new Attribute(0, 0);
            x->mNamespaceList = new NamespaceList(&n->name, x->mNamespaceList);
            m_cx->getGlobalObject()->defineVariable(m_cx, n->name, (NamespaceList *)(NULL), Property::NoAttribute, Attribute_Type, JSValue(x));            
        }
        break;
    default:
        break;
    }
}

// Make sure there's a default constructor. XXX anything else?
void JSType::completeClass(Context *cx, ScopeChain *scopeChain)
{    
    // if none exists, build a default constructor that calls 'super()'
    if (getDefaultConstructor() == NULL) {
        JSFunction *fnc = new JSFunction(cx, Object_Type, scopeChain);
        fnc->setIsConstructor(true);
        fnc->setFunctionName(mClassName);
        fnc->setClass(this);

        ByteCodeGen bcg(cx, scopeChain);

        if (mSuperType && mSuperType->getDefaultConstructor()) {
            bcg.addOp(LoadTypeOp);
            bcg.addPointer(this);
            bcg.addOp(NewThisOp);
            bcg.addOp(LoadThisOp);
            bcg.addOp(LoadFunctionOp);
            bcg.addPointer(mSuperType->getDefaultConstructor());
            bcg.addOpAdjustDepth(InvokeOp, -1);
            bcg.addLong(0);
            bcg.addByte(Explicit);
            bcg.addOp(PopOp);
        }
        bcg.addOp(LoadThisOp);
        ASSERT(bcg.mStackTop == 1);
        bcg.addOpSetDepth(ReturnOp, 0);
        ByteCodeModule *bcm = new JS2Runtime::ByteCodeModule(&bcg, fnc);
        if (cx->mReader)
            bcm->setSource(cx->mReader->source, cx->mReader->sourceLocation);
        fnc->setByteCode(bcm);        

        scopeChain->defineConstructor(cx, *mClassName, NULL, fnc);   // XXX attributes?
        setDefaultConstructor(cx, fnc);
    }
}

/*
// constructor functions are added as static methods
// XXX is it worth just having a default constructor 
// pointer in the class?
JSFunction *JSType::getDefaultConstructor()
{
    PropertyIterator it;
    if (hasOwnProperty(*mClassName, NULL, Read, &it)) {
        ASSERT(PROPERTY_KIND(it) == ValuePointer);
        JSValue c = *PROPERTY_VALUEPOINTER(it);
        ASSERT(c.isFunction());
        return c.function;
    }
    return NULL;
}
*/
void JSType::defineMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyIterator it;
    if (hasOwnProperty(name, names, Read, &it))
        cx->reportError(Exception::typeError, "Duplicate method definition", attr->pos);

    // now check if the method exists in the supertype
    if (mSuperType && mSuperType->hasOwnProperty(name, names, Read, &it)) {
        // if it does, it must have been overridable:
        PropertyAttribute superAttr = PROPERTY_ATTR(it);
        if (superAttr & Property::Final)
            cx->reportError(Exception::typeError, "Attempting to override a final method", attr->pos);
        // if it was marked as virtual, then the new one must specifiy 'override' or 'mayoverride'
        if (superAttr & Property::Virtual) {
            if ((attr->attributeValue->mTrueFlags & (Property::Override | Property::MayOverride)) == 0)
                cx->reportError(Exception::typeError, "Must specify 'override' or 'mayOverride'", attr->pos);
        }
    }

    uint32 vTableIndex = mMethods.size();
    mMethods.push_back(f);

    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    const PropertyMap::value_type e(name, new NamespacedProperty(new Property(vTableIndex, Function_Type, Method, attrFlags), names));
    mProperties.insert(e);
}

void JSType::defineGetterMethod(Context * /*cx*/, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    PropertyIterator i;
    uint32 vTableIndex = mMethods.size();
    mMethods.push_back(f);

    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;

    if (hasProperty(name, names, Write, &i)) {
        ASSERT(PROPERTY_KIND(i) == IndexPair);
        ASSERT(PROPERTY_GETTERI(i) == 0);
        PROPERTY_GETTERI(i) = vTableIndex;
    }
    else {
        PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
        const PropertyMap::value_type e(name, new NamespacedProperty(new Property(vTableIndex, 0, Function_Type, attrFlags), names));
        mProperties.insert(e);
    }
}

void JSType::defineSetterMethod(Context * /*cx*/, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    PropertyIterator i;
    uint32 vTableIndex = mMethods.size();
    mMethods.push_back(f);

    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;

    if (hasProperty(name, names, Read, &i)) {
        ASSERT(PROPERTY_KIND(i) == IndexPair);
        ASSERT(PROPERTY_SETTERI(i) == 0);
        PROPERTY_SETTERI(i) = vTableIndex;
    }
    else {
        PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
        const PropertyMap::value_type e(name, new NamespacedProperty(new Property(0, vTableIndex, Function_Type, attrFlags), names));
        mProperties.insert(e);
    }
}

bool JSType::derivesFrom(JSType *other)
{
    if (mSuperType == other)
        return true;
    else
        if (mSuperType)
            return mSuperType->derivesFrom(other);
        else
            return false;
}

JSValue JSType::getPropertyValue(PropertyIterator &i)
{
    Property *prop = PROPERTY(i);
    switch (prop->mFlag) {
    case ValuePointer:
        return *prop->mData.vp;
    case Constructor:
        return JSValue(mMethods[prop->mData.index]);
    default:
        return kUndefinedValue;
    }
}

bool JSType::hasProperty(const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    if (hasOwnProperty(name, names, acc, p))
        return true;
    else
        if (mSuperType)
            return mSuperType->hasProperty(name, names, acc, p);
        else
            return false;
}

Reference *JSType::genReference(bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth)
{
    PropertyIterator i;
    if (hasOwnProperty(name, names, acc, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case FunctionPair:
            if (acc == Read)
                return new GetterFunctionReference(prop->mData.fPair.getterF, prop->mAttributes);
            else {
                JSFunction *f = prop->mData.fPair.setterF;
                return new SetterFunctionReference(f, f->getArgType(0), prop->mAttributes);
            }
        case ValuePointer:
            return new StaticFieldReference(name, acc, this, prop->mType, prop->mAttributes);

        case IndexPair:
            if (acc == Read)
                return new GetterMethodReference(prop->mData.iPair.getterI, this, prop->mType, prop->mAttributes);
            else {
                JSFunction *f = mMethods[prop->mData.iPair.setterI];
                return new SetterMethodReference(prop->mData.iPair.setterI, this, f->getArgType(0), prop->mAttributes);
            }
        case Slot:
            return new FieldReference(prop->mData.index, acc, prop->mType, prop->mAttributes);
        case Method:
            return new MethodReference(prop->mData.index, this, prop->mType, prop->mAttributes);
        default:
            NOT_REACHED("bad storage kind");
            return NULL;
        }
    }
    // walk the supertype chain
    if (mSuperType)
        return mSuperType->genReference(hasBase, name, names, acc, depth);
    return NULL;
}

JSType::JSType(Context *cx, const StringAtom *name, JSType *super, JSObject *protoObj) 
            : JSObject(Type_Type),
                    mSuperType(super), 
                    mVariableCount(0),
                    mInstanceInitializer(NULL),
                    mDefaultConstructor(NULL),
                    mTypeCast(NULL),
                    mClassName(name),
                    mIsDynamic(false),
                    mUninitializedValue(kNullValue),
                    mPrototypeObject(NULL)
{
    if (mClassName)
        mPrivateNamespace = &cx->mWorld.identifiers[*mClassName + " private"];
    else
        mPrivateNamespace = &cx->mWorld.identifiers["unique id needed? private"]; // XXX. No, really?

    for (uint32 i = 0; i < OperatorCount; i++)
        mUnaryOperators[i] = NULL;

    // every class gets a prototype object
    if (protoObj)
        mPrototypeObject = protoObj;
    else
        mPrototypeObject = new JSObject(this);
    // and that object is prototype-linked to the super-type's prototype object
    if (mSuperType)
        mPrototypeObject->mPrototype = mSuperType->mPrototypeObject;

    if (mSuperType)
        defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, Property::ReadOnly | Property::DontDelete, Object_Type, JSValue(mPrototypeObject));
    else  // must be Object_Type
        defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, Property::ReadOnly | Property::DontDelete, this, JSValue(mPrototypeObject));

    if (mSuperType)
        mPrototype = mSuperType->mPrototypeObject;
    else // must be Object_Type
        mPrototype = mPrototypeObject;
}

JSType::JSType(JSType *xClass)     // used for constructing the static component type
            : JSObject(Type_Type),
                    mSuperType(xClass), 
                    mVariableCount(0),
                    mInstanceInitializer(NULL),
                    mDefaultConstructor(NULL),
                    mTypeCast(NULL),
                    mClassName(NULL),
                    mPrivateNamespace(NULL),
                    mIsDynamic(false),
                    mUninitializedValue(kNullValue),
                    mPrototypeObject(NULL)
{
    for (uint32 i = 0; i < OperatorCount; i++)
        mUnaryOperators[i] = NULL;
}

// Establish the super class - connects the prototype's prototype
// and accounts for the super class's instance fields & methods
void JSType::setSuperType(JSType *super)
{
    mSuperType = super;
    if (mSuperType) {
        mPrototypeObject->mPrototype = mSuperType->mPrototypeObject;
        // inherit supertype instance field and vtable slots
        mVariableCount = mSuperType->mVariableCount;
        mMethods.insert(mMethods.begin(), 
                            mSuperType->mMethods.begin(), 
                            mSuperType->mMethods.end());
    }
}

















void Activation::defineTempVariable(Context * /*cx*/, Reference *&readRef, Reference *&writeRef, JSType *type)
{
    readRef = new LocalVarReference(mVariableCount, Read, type, Property::NoAttribute);
    writeRef = new LocalVarReference(mVariableCount, Write, type, Property::NoAttribute);
    mVariableCount++;
}

Reference *Activation::genReference(bool /* hasBase */, const String& name, NamespaceList *names, Access acc, uint32 depth)
{
    PropertyIterator i;
    if (hasProperty(name, names, acc, &i)) {
        Property *prop = PROPERTY(i);
        ASSERT((prop->mFlag == ValuePointer) || (prop->mFlag == Slot) || (prop->mFlag == FunctionPair)); 

        switch (prop->mFlag) {
        case FunctionPair:
            return (acc == Read) ? new AccessorReference(prop->mData.fPair.getterF, prop->mAttributes)
                                 : new AccessorReference(prop->mData.fPair.setterF, prop->mAttributes);
        case Slot:
            if (depth)
                return new ClosureVarReference(depth, prop->mData.index, acc, prop->mType, prop->mAttributes);
            else
                return new LocalVarReference(prop->mData.index, acc, prop->mType, prop->mAttributes);

        case ValuePointer:
            return new NameReference(name, acc, prop->mType, prop->mAttributes);

        default:            
            NOT_REACHED("bad genRef call");
            break;
        }
    }
    NOT_REACHED("bad genRef call");
    return NULL;
}





/*
Process the statements in the function body, handling parameters and local 
variables to collect names & types.
*/
void Context::buildRuntimeForFunction(FunctionDefinition &f, JSFunction *fnc)
{
    fnc->mParameterBarrel = new ParameterBarrel();
    mScopeChain->addScope(fnc->mParameterBarrel);
    VariableBinding *v = f.parameters;
    while (v) {
        if (v->name) {
            JSType *pType = mScopeChain->extractType(v->type);              // XXX already extracted for argument Data
            mScopeChain->defineVariable(this, *v->name, NULL, pType);       // XXX attributes?
        }
        v = v->next;
    }
    if (f.body) {
        mScopeChain->addScope(&fnc->mActivation);
        mScopeChain->collectNames(f.body);
        buildRuntimeForStmt(f.body);
        mScopeChain->popScope();
    }
    mScopeChain->popScope();
}

/*
The incoming AttributeStmtNode has a list of attributes - evaluate those and return
the resultant Attribute value. If there are no attributes, return the default value.
*/
void Context::setAttributeValue(AttributeStmtNode *s, PropertyAttribute defaultValue)
{
    Attribute *attributeValue = NULL;
    if (s->attributes == NULL)
        attributeValue = new Attribute(defaultValue, 0);
    else
        attributeValue = executeAttributes(s->attributes);
    s->attributeValue = attributeValue;
}


// Second pass, collect type information and finish 
// off the definitions made in pass 1
void Context::buildRuntimeForStmt(StmtNode *p)
{
    switch (p->getKind()) {
    case StmtNode::block:
        {
            BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
            StmtNode *s = b->statements;
            while (s) {
                buildRuntimeForStmt(s);
                s = s->next;
            }            
        }
        break;
    case StmtNode::label:
        {
            LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
            buildRuntimeForStmt(l->stmt);
        }
        break;
    case StmtNode::Try:
        {
            TryStmtNode *t = checked_cast<TryStmtNode *>(p);
            if (t->catches) {
                CatchClause *c = t->catches;
                while (c) {
                    c->prop->mType = mScopeChain->extractType(c->type);
                    c = c->next;
                }
            }
        }
        break;
    case StmtNode::If:
    case StmtNode::With:
    case StmtNode::DoWhile:
    case StmtNode::While:
        {
            UnaryStmtNode *u = checked_cast<UnaryStmtNode *>(p);
            buildRuntimeForStmt(u->stmt);
        }
        break;
    case StmtNode::IfElse:
        {
            BinaryStmtNode *b = checked_cast<BinaryStmtNode *>(p);
            buildRuntimeForStmt(b->stmt);
            buildRuntimeForStmt(b->stmt2);
        }
        break;
    case StmtNode::For:
    case StmtNode::ForIn:
        {
            ForStmtNode *f = checked_cast<ForStmtNode *>(p);
            if (f->initializer) buildRuntimeForStmt(f->initializer);
            buildRuntimeForStmt(f->stmt);
        }
        break;
    case StmtNode::Var:
    case StmtNode::Const:
        {
            VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);
            VariableBinding *v = vs->bindings;
            while (v)  {
                JSType *type = mScopeChain->extractType(v->type);
                v->prop->mType = type;
                v = v->next;
            }
        }
        break;
    case StmtNode::Function:
        {
            FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
            
            bool isOperator = (f->attributeValue->mTrueFlags & Property::Operator) == Property::Operator;

            JSType *resultType = mScopeChain->extractType(f->function.resultType);
            JSFunction *fnc = f->mFunction;
            fnc->setResultType(resultType);
            
            VariableBinding *v = f->function.parameters;
            uint32 parameterCount = 0;
            while (v) {
                // XXX if no type is specified for the rest parameter - is it Array?
                fnc->setArgument(parameterCount++, v->name, mScopeChain->extractType(v->type));
                v = v->next;
            }

            if (isOperator) {
                if (f->function.prefix != FunctionName::op) {
                    NOT_REACHED("***** Implement me -- signal an error here because the user entered an unquoted operator name");
                }
                ASSERT(f->function.name);
                const StringAtom& name = *f->function.name;
                Operator op = getOperator(parameterCount, name);
                // if it's a unary operator, it just gets added 
                // as a method with a special name. Binary operators
                // get added to the Context's operator table.
                // The indexing operators are considered unary since
                // they only dispatch on the base type.
                if ((parameterCount == 1)
                        || (op == Index)
                        || (op == IndexEqual)
                        || (op == DeleteIndex))
                    mScopeChain->defineUnaryOperator(op, fnc);
                else
                    defineOperator(op, getParameterType(f->function, 0), 
                                   getParameterType(f->function, 1), fnc);
            }

            // if it's an extending function, rediscover the extended class
            // and push the class scope onto the scope chain


/*
            bool isExtender = false;                
            if (hasAttribute(f->attributes, ExtendKeyWord)) {
                JSType *extendedClass = mScopeChain->extractType( <extend attribute argument> );
                mScopeChain->addScope(extendedClass->mStatics);
                mScopeChain->addScope(extendedClass);
            }
*/            

            buildRuntimeForFunction(f->function, fnc);
/*
            if (isExtender) {   // blow off the extended class's scope
                mScopeChain->popScope();
                mScopeChain->popScope();
            }
*/


        }
        break;
    case StmtNode::Class:
        {     
            ClassStmtNode *classStmt = checked_cast<ClassStmtNode *>(p);
            JSType *superClass = Object_Type;
            if (classStmt->superclass) {
                ASSERT(classStmt->superclass->getKind() == ExprNode::identifier);   // XXX
                IdentifierExprNode *superClassExpr = checked_cast<IdentifierExprNode *>(classStmt->superclass);
                superClass = mScopeChain->findType(superClassExpr->name, superClassExpr->pos);
            }
            JSType *thisClass = classStmt->mType;
            thisClass->setSuperType(superClass);
            mScopeChain->addScope(thisClass);
            if (classStmt->body) {
                StmtNode* s = classStmt->body->statements;
                while (s) {
                    mScopeChain->collectNames(s);
                    s = s->next;
                }
                s = classStmt->body->statements;
                while (s) {
                    buildRuntimeForStmt(s);
                    s = s->next;
                }
            }
            thisClass->completeClass(this, mScopeChain);

            mScopeChain->popScope();
        }        
        break;
    case StmtNode::Namespace:
        {
            // do anything ?
        }
        break;
    default:
        break;
    }

}


static JSValue Object_Constructor(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    JSValue thatValue = thisValue;  // incoming 'new this' potentially supplied by constructor sequence

    if (argc != 0) {
        if (argv[0].isObject())
            thatValue = argv[0];
        else
        if (argv[0].isString() || argv[0].isBool() || argv[0].isNumber())
            thatValue = argv[0].toObject(cx);
        else {
            if (thatValue.isNull())
                thatValue = Object_Type->newInstance(cx);
        }
    }
    else {
        if (thatValue.isNull())
            thatValue = Object_Type->newInstance(cx);
    }
    return thatValue;
}

static JSValue Object_toString(Context *, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    if (thisValue.isObject())
        return JSValue(new String(widenCString("[object ") + *thisValue.object->getType()->mClassName + widenCString("]")));
    else
        if (thisValue.isType())
            return JSValue(new String(widenCString("[object ") + widenCString("Type") + widenCString("]")));
        else
            if (thisValue.isFunction())
                return JSValue(new String(widenCString("[object ") + widenCString("Function") + widenCString("]")));
            else {
                NOT_REACHED("Object.prototype.toString on non-object");
                return kUndefinedValue;
            }
}

struct IteratorDongle {
    JSObject *obj;
    PropertyIterator it;
};

static JSValue Object_forin(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    JSObject *obj = NULL;
    if (thisValue.isType())
        obj = thisValue.type;
    else
    if (thisValue.isFunction())
        obj = thisValue.function;
    else
    if (thisValue.isObject())
        obj = thisValue.object;
    else
        NOT_REACHED("Non object for for..in");

    JSObject *iteratorObject = Object_Type->newInstance(cx);    
    IteratorDongle *itDude = new IteratorDongle();
    itDude->obj = obj;
    itDude->it = obj->mProperties.begin();
    while (true) {
        while (itDude->it == itDude->obj->mProperties.end()) {
            itDude->obj = itDude->obj->mPrototype;
            if (itDude->obj == NULL)
                return kNullValue;
            itDude->it = itDude->obj->mProperties.begin();
        }
        if (PROPERTY_ATTR(itDude->it) & Property::Enumerable)
            break;
        itDude->it++;
    }

    JSValue v(&PROPERTY_NAME(itDude->it));
    iteratorObject->setProperty(cx, cx->mWorld.identifiers["value"], 0, v);
    iteratorObject->mPrivate = itDude;
    return JSValue(iteratorObject);
}

static JSValue Object_next(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue iteratorValue = argv[0];
    ASSERT(iteratorValue.isObject());
    JSObject *iteratorObject = iteratorValue.object;

    IteratorDongle *itDude = (IteratorDongle *)(iteratorObject->mPrivate);
    itDude->it++;
    while (true) {
        while (itDude->it == itDude->obj->mProperties.end()) {
            itDude->obj = itDude->obj->mPrototype;
            if (itDude->obj == NULL)
                return kNullValue;
            itDude->it = itDude->obj->mProperties.begin();
        }
        if (PROPERTY_ATTR(itDude->it) & Property::Enumerable)
            break;
        itDude->it++;
    }
    JSValue v(&PROPERTY_NAME(itDude->it));
    iteratorObject->setProperty(cx, cx->mWorld.identifiers["value"], 0, v);
    return iteratorValue;
}

static JSValue Object_done(Context *, const JSValue& /*thisValue*/, JSValue * /*argv*/, uint32 /*argc*/)
{
    return kUndefinedValue;
}




static JSValue Function_Constructor(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    JSValue v = thisValue;
    if (v.isNull())
        v = Function_Type->newInstance(cx);
    ASSERT(v.isObject());

    String s;
    if (argc == 0)
        s = widenCString("() { }");
    else {
        if (argc == 1)
            s = widenCString("() {") + *argv[0].toString(cx).string + "}";
        else {
            s = widenCString("(");         // ')'
            for (uint32 i = 0; i < (argc - 1); i++) {
                s += *argv[i].toString(cx).string;
                if (i < (argc - 2))
                    s += widenCString(", ");
            }
/* ( */     s += ") {" + *argv[argc - 1].toString(cx).string + "}";
        }
    }

    JSFunction *fnc = NULL;
    /***************************************************************/
    {
        Arena a;
        Parser p(cx->mWorld, a, cx->mFlags, s, widenCString("function constructor"));
        Reader *oldReader = cx->mReader;
        cx->mReader = &p.lexer.reader;

        FunctionExprNode *f = p.parseFunctionExpression(0);
        if (!p.lexer.peek(true).hasKind(Token::end)) 
            cx->reportError(Exception::syntaxError, "Unexpected stuff after the function body");

        fnc = new JSFunction(cx, NULL, cx->mScopeChain);

        uint32 reqArgCount = 0;
        uint32 optArgCount = 0;

        VariableBinding *b = f->function.parameters;
        while ((b != f->function.optParameters) && (b != f->function.restParameter)) {
            reqArgCount++;
            b = b->next;
        }
        while (b != f->function.restParameter) {
            optArgCount++;
            b = b->next;
        }
        fnc->setArgCounts(cx, reqArgCount, optArgCount, (f->function.restParameter != NULL));

        if (cx->mScopeChain->isPossibleUncheckedFunction(&f->function)) {
	    fnc->setIsPrototype(true);
            fnc->setIsUnchecked();
        }
        cx->buildRuntimeForFunction(f->function, fnc);
        ByteCodeGen bcg(cx, cx->mScopeChain);
        bcg.genCodeForFunction(f->function, f->pos, fnc, false, NULL);
        cx->setReader(oldReader);
    }
    /***************************************************************/

    JSObject *fncPrototype = Object_Type->newInstance(cx);
    fncPrototype->defineVariable(cx, cx->Constructor_StringAtom, (NamespaceList *)NULL, Property::Enumerable, Object_Type, JSValue(fnc));
    fnc->defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, Property::Enumerable, Object_Type, JSValue(fncPrototype));
    v = JSValue(fnc);
    return v;
}

static JSValue Function_toString(Context *, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isFunction() || (thisValue.isObject() && (thisValue.object->getType() == Function_Type)) );
    return JSValue(new String(widenCString("function () { }")));
}

static JSValue Function_hasInstance(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(argc == 1);
    JSValue v = argv[0];
    if (!v.isObject())
        return kFalseValue;

    ASSERT(thisValue.isFunction());
    thisValue.function->getProperty(cx, cx->Prototype_StringAtom, CURRENT_ATTR);
    JSValue p = cx->popValue();

    if (!p.isObject())
        cx->reportError(Exception::typeError, "HasInstance: Function has non-object prototype");

    JSObject *V = v.object->mPrototype;
    while (V) {
        if (V == p.object)
            return kTrueValue;
        V = V->mPrototype;
    }
    return kFalseValue;
}

static JSValue Number_Constructor(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    JSValue v = thisValue;
    if (v.isNull())
        v = Number_Type->newInstance(cx);
    ASSERT(v.isObject());
    JSObject *thisObj = v.object;
    if (argc > 0)
        thisObj->mPrivate = (void *)(new float64(argv[0].toNumber(cx).f64));
    else
        thisObj->mPrivate = (void *)(new float64(0.0));
    return v;
}

static JSValue Number_toString(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    if (!thisValue.isObject() || (thisValue.getType() != Number_Type))
        cx->reportError(Exception::typeError, "Number.toString called on something other than a Number object");
    JSObject *thisObj = thisValue.object;
    return JSValue(numberToString(*((float64 *)(thisObj->mPrivate))));
}

static JSValue Number_valueOf(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    if (!thisValue.isObject() || (thisValue.getType() != Number_Type))
        cx->reportError(Exception::typeError, "Number.valueOf called on something other than a Number object");
    JSObject *thisObj = thisValue.object;
    return JSValue(*((float64 *)(thisObj->mPrivate)));
}

static JSValue Integer_Constructor(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    JSValue v = thisValue;
    if (v.isNull())
        v = Integer_Type->newInstance(cx);
    ASSERT(v.isObject());
    JSObject *thisObj = v.object;
    if (argc > 0) {
        float64 d = argv[0].toNumber(cx).f64;
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        d = neg ? -d : d;
        thisObj->mPrivate = (void *)(new double(d));
    }
    else
        thisObj->mPrivate = (void *)(new double(0.0));
    return v;
}

static JSValue Integer_toString(Context *, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject());
    JSObject *thisObj = thisValue.object;
    return JSValue(numberToString(*((double *)(thisObj->mPrivate))));
}



static JSValue Boolean_Constructor(Context *cx, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    JSValue v = thisValue;
    if (v.isNull())
        v = Boolean_Type->newInstance(cx);
    ASSERT(v.isObject());
    JSObject *thisObj = v.object;
    if (argc > 0)
        thisObj->mPrivate = (void *)(argv[0].toBoolean(cx).boolean);
    else
        thisObj->mPrivate = (void *)(false);
    return v;
}

static JSValue Boolean_toString(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject());
    JSObject *thisObj = thisValue.object;
    if (thisObj->mType != Boolean_Type)
        cx->reportError(Exception::typeError, "Boolean.toString can only be applied to Boolean objects");
    if (thisObj->mPrivate != 0)
        return JSValue(&cx->True_StringAtom);
    else
        return JSValue(&cx->False_StringAtom);
}

static JSValue Boolean_valueOf(Context *cx, const JSValue& thisValue, JSValue * /*argv*/, uint32 /*argc*/)
{
    ASSERT(thisValue.isObject());
    JSObject *thisObj = thisValue.object;
    if (thisObj->mType != Boolean_Type)
        cx->reportError(Exception::typeError, "Boolean.valueOf can only be applied to Boolean objects");
    if (thisObj->mPrivate != 0)
        return kTrueValue;
    else
        return kFalseValue;
}

static JSValue ExtendAttribute_Invoke(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    ASSERT(argc == 1);

    Attribute *x = new Attribute(Property::Extend, Property::Extend | Property::Virtual);
    ASSERT(argv[0].isType());
    x->mExtendArgument = (JSType *)(argv[0].type);

    return JSValue(x);
}

static JSValue GlobalObject_Eval(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    if (!argv[0].isString())
        return argv[0];
    const String *sourceStr = argv[0].toString(cx).string;

    Activation *prev = cx->mActivationStack.top();

    return cx->readEvalString(*sourceStr, widenCString("eval source"), cx->mScopeChain, prev->mThis);
}

static JSValue GlobalObject_ParseInt(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    ASSERT(argc >= 1);
    int radix = 0;      // default for stringToInteger
    if (argc == 2)
        radix = (int)(argv[1].toInt32(cx).f64);
    if ((radix < 0) || (radix > 36))
        return kNaNValue;

    const String *string = argv[0].toString(cx).string;
    const char16 *numEnd;
    const char16 *sBegin = string->begin();
    float64 f = 0.0;
    if (sBegin)
        f = stringToInteger(sBegin, string->end(), numEnd, (uint)radix);
    return JSValue(f);
}

static JSValue GlobalObject_ParseFloat(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    ASSERT(argc == 1);

    const String *string = argv[0].toString(cx).string;
    const char16 *numEnd;
    const char16 *sBegin = string->begin();
    float64 f = 0.0;
    if (sBegin)
        f = stringToDouble(sBegin, string->end(), numEnd);
    return JSValue(f);
}

static JSValue GlobalObject_isNaN(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    float64 f = argv[0].toNumber(cx).f64;
    if (JSDOUBLE_IS_NaN(f))
        return kTrueValue;
    else
        return kFalseValue;
}

static JSValue GlobalObject_isFinite(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    float64 f = argv[0].toNumber(cx).f64;
    if (JSDOUBLE_IS_FINITE(f))
        return kTrueValue;
    else
        return kFalseValue;
}


JSFunction::JSFunction(Context *cx, JSType *resultType, ScopeChain *scopeChain) 
            : JSObject(Function_Type), 
                mParameterBarrel(NULL),
                mActivation(),
                mByteCode(NULL), 
                mCode(NULL), 
                mResultType(resultType),
                mRequiredArgs(0),
                mOptionalArgs(0),
                mArguments(NULL),
                mScopeChain(NULL), 
                mIsPrototype(false),
                mIsConstructor(false),
                mIsChecked(true),
                mHasRestParameter(false),
                mRestParameterName(NULL),
                mClass(NULL),
                mFunctionName(NULL)
{
    if (scopeChain) {
        mScopeChain = new ScopeChain(*scopeChain);
    }
    if (Function_Type)    // protect against bootstrap
        mPrototype = Function_Type->mPrototypeObject;
    mActivation.mContainer = this;
}

JSFunction::JSFunction(Context *cx, NativeCode *code, JSType *resultType) 
            : JSObject(Function_Type), 
                mParameterBarrel(NULL),
                mActivation(),
                mByteCode(NULL), 
                mCode(code), 
                mResultType(resultType), 
                mRequiredArgs(0),
                mOptionalArgs(0),
                mArguments(NULL),
                mScopeChain(NULL),
                mIsPrototype(false),
                mIsConstructor(false),
                mIsChecked(false),          // native functions aren't checked (?)
                mHasRestParameter(false),
                mRestParameterName(NULL),
                mClass(NULL),
                mFunctionName(NULL)
{
    if (Function_Type)    // protect against bootstrap
        mPrototype = Function_Type->mPrototypeObject;
    mActivation.mContainer = this;
}
              
JSValue JSFunction::runArgInitializer(Context *cx, uint32 a, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    ASSERT(mArguments && (a < (mRequiredArgs + mOptionalArgs)));
    return cx->interpret(getByteCode(), (int32)mArguments[a].mInitializer, getScopeChain(), thisValue, argv, argc);    
}

void JSFunction::setArgCounts(Context *cx, uint32 r, uint32 o, bool hasRest)   
{
    mHasRestParameter = hasRest; 
    mRequiredArgs = r; 
    mOptionalArgs = o; 
    mArguments = new ArgumentData[mRequiredArgs + mOptionalArgs + ((hasRest) ? 1 : 0)]; 
    defineVariable(cx, cx->Length_StringAtom, (NamespaceList *)NULL, Property::DontDelete | Property::ReadOnly, Number_Type, JSValue((float64)mRequiredArgs)); 
}


uint32 JSFunction::findParameterName(const String *name)
{
    uint32 maxArgs = mRequiredArgs + mOptionalArgs + ((mHasRestParameter) ? 1 : 0);
    ASSERT(mArguments || (maxArgs == 0));
    for (uint32 i = 0; i < maxArgs; i++) {
        if (mArguments[i].mName->compare(*name) == 0)
            return i;
    }
    return NotABanana;
}

void Context::assureStackSpace(uint32 s)
{
    if ((mStackMax - mStackTop) < s) {
        JSValue *newStack = new JSValue[mStackMax + s];
        for (uint32 i = 0; i < mStackTop; i++)
            newStack[i] = mStack[i];
        delete[] mStack;
        mStack = newStack;
        mStackMax += s;
    }
}





// Initialize a built-in class - setting the functions into the prototype object
void Context::initClass(JSType *type, ClassDef *cdef, PrototypeFunctions *pdef)
{
    mScopeChain->addScope(type);
    JSFunction *constructor = new JSFunction(this, cdef->defCon, Object_Type);
    constructor->setClass(type);
    constructor->setFunctionName(type->mClassName);
    type->setDefaultConstructor(this, constructor);

    // the prototype functions are defined in the prototype object...
    if (pdef) {
        for (uint32 i = 0; i < pdef->mCount; i++) {
            JSFunction *fun = new JSFunction(this, pdef->mDef[i].imp, pdef->mDef[i].result);
//            fun->setClass(type); don't do this, it makes the function a method
            StringAtom *name = &mWorld.identifiers[widenCString(pdef->mDef[i].name)];
            fun->setFunctionName(name);
            fun->setArgCounts(this, pdef->mDef[i].length, 0, false);
            type->mPrototypeObject->defineVariable(this, *name,
                                               (NamespaceList *)(NULL), 
                                               Property::NoAttribute,
                                               pdef->mDef[i].result, 
                                               JSValue(fun));
        }
    }
    type->completeClass(this, mScopeChain);
    type->setStaticInitializer(this, NULL);
    type->mUninitializedValue = *cdef->uninit;
    getGlobalObject()->defineVariable(this, widenCString(cdef->name), (NamespaceList *)(NULL), Property::NoAttribute, Type_Type, JSValue(type));
    mScopeChain->popScope();
    if (pdef) delete pdef;
}

static JSValue arrayMaker(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    ASSERT(argc == 2);
    ASSERT(argv[0].isType());
    JSType *baseType = argv[0].type;

    if ((baseType == Array_Type) && argv[1].isType()) {
        JSType *elementType = argv[1].type;
        JSType *result = new JSArrayType(cx, elementType, NULL, Object_Type);
        result->setDefaultConstructor(cx, Array_Type->getDefaultConstructor());
        return JSValue(result);
    }
    else {
        // then it's just <type>[] - an element reference
        argv[0].type->getProperty(cx, *argv[1].toString(cx).string, NULL);
        return cx->popValue();
    }

}

void Context::initBuiltins()
{
    ClassDef builtInClasses[] =
    {
        { "Object",         Object_Constructor,    &kUndefinedValue     },
        { "Type",           NULL,                  &kNullValue          },
        { "Function",       Function_Constructor,  &kNullValue          },
        { "Number",         Number_Constructor,    &kPositiveZero       },
        { "Integer",        Integer_Constructor,   &kPositiveZero       },
        { "String",         String_Constructor,    &kNullValue          },
        { "Array",          Array_Constructor,     &kNullValue          },
        { "Boolean",        Boolean_Constructor,   &kFalseValue         },
        { "Void",           NULL,                  &kUndefinedValue     },
        { "Unit",           NULL,                  &kNullValue          },
        { "Attribute",      NULL,                  &kNullValue          },
        { "NamedArgument",  NULL,                  &kNullValue          },
        { "Date",           Date_Constructor,      &kPositiveZero       },
        { "Null",           NULL,                  &kNullValue          },
    };

    Object_Type  = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[0].name)], NULL);
    Object_Type->mPrototype->mType = Object_Type;
    Object_Type->mIsDynamic = true;
    // XXX aren't all the built-ins thus?

    Type_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[1].name)], Object_Type);
    Object_Type->mType  = Type_Type;
    
    // 
    // ECMA 1.5 says Function.prototype is a Function object, it's [[class]] property is "Function"
    // ECMA 2.0 says Function.prototype is an Object NOT an instance of the class
    // here we sort of manage that by having a JSFunction object as the prototype object, not a JSFunctionInstance
    // (which we don't actually have, hmm).
    // For String, etc. this same issue needs to be finessed

    JSFunction *funProto = new JSFunction(this, Object_Type, NULL);
    Function_Type       = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[2].name)], Object_Type, funProto);
    funProto->mType = Function_Type;
    
    JSObject *numProto  = new JSObject();
    Number_Type         = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[3].name)], Object_Type, numProto);
    numProto->mType = Number_Type;
    numProto->mPrivate = (void *)(new float64(0.0));

    Integer_Type        = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[4].name)], Object_Type);

    JSObject *strProto  = new JSStringInstance(this, NULL);
    String_Type         = new JSStringType(this, &mWorld.identifiers[widenCString(builtInClasses[5].name)], Object_Type, strProto);
    strProto->mType = String_Type;
    strProto->mPrivate = (void *)(&Empty_StringAtom);
    
    JSArrayInstance *arrayProto = new JSArrayInstance(this, NULL);
    Array_Type          = new JSArrayType(this, Object_Type, &mWorld.identifiers[widenCString(builtInClasses[6].name)], Object_Type, arrayProto);
    arrayProto->mType = Array_Type;

    Boolean_Type        = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[7].name)], Object_Type);
    Void_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[8].name)], Object_Type);
    Unit_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[9].name)], Object_Type);
    Attribute_Type      = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[10].name)], Object_Type);
    NamedArgument_Type  = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[11].name)], Object_Type);
    Date_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[12].name)], Object_Type);
    Null_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[13].name)], Object_Type);

    String_Type->defineVariable(this, FromCharCode_StringAtom, NULL, String_Type, JSValue(new JSFunction(this, String_fromCharCode, String_Type)));


    ProtoFunDef objectProtos[] = 
    {
        { "toString", String_Type, 0, Object_toString },
        { "toSource", String_Type, 0, Object_toString },
        { "forin",    Object_Type, 0, Object_forin    },
        { "next",     Object_Type, 0, Object_next     },
        { "done",     Object_Type, 0, Object_done     },
        { NULL }
    };
    ProtoFunDef functionProtos[] = 
    {
        { "toString",    String_Type, 0, Function_toString },
        { "toSource",    String_Type, 0, Function_toString },
        { "hasInstance", Boolean_Type, 1, Function_hasInstance },
        { NULL }
    };
    ProtoFunDef numberProtos[] = 
    {
        { "toString", String_Type, 0, Number_toString },
        { "toSource", String_Type, 0, Number_toString },
        { "valueOf",  Number_Type, 0, Number_valueOf },
        { NULL }
    };
    ProtoFunDef integerProtos[] = 
    {
        { "toString", String_Type, 0, Number_toString },
        { "toSource", String_Type, 0, Number_toString },
        { NULL }
    };
    ProtoFunDef booleanProtos[] = 
    {
        { "toString", String_Type, 0, Boolean_toString },
        { "toSource", String_Type, 0, Boolean_toString },
        { "valueOf",  Number_Type, 0, Boolean_valueOf },
        { NULL }
    };

    ASSERT(mGlobal);
    *mGlobal = Object_Type->newInstance(this);
    initClass(Object_Type,          &builtInClasses[0],  new PrototypeFunctions(&objectProtos[0]) );
    
    initClass(Type_Type,            &builtInClasses[1],  NULL );
    initClass(Function_Type,        &builtInClasses[2],  new PrototypeFunctions(&functionProtos[0]) );
    initClass(Number_Type,          &builtInClasses[3],  new PrototypeFunctions(&numberProtos[0]) );
    initClass(Integer_Type,         &builtInClasses[4],  new PrototypeFunctions(&integerProtos[0]) );
    initClass(String_Type,          &builtInClasses[5],  getStringProtos() );
    initClass(Array_Type,           &builtInClasses[6],  getArrayProtos() );
    initClass(Boolean_Type,         &builtInClasses[7],  new PrototypeFunctions(&booleanProtos[0]) );
    initClass(Void_Type,            &builtInClasses[8],  NULL);
    initClass(Unit_Type,            &builtInClasses[9],  NULL);
    initClass(Attribute_Type,       &builtInClasses[10], NULL);
    initClass(NamedArgument_Type,   &builtInClasses[11], NULL);
    initClass(Date_Type,            &builtInClasses[12], getDateProtos() );
    initClass(Null_Type,            &builtInClasses[13], NULL);

    Type_Type->defineUnaryOperator(Index, new JSFunction(this, arrayMaker, Type_Type));

    Function_Type->mTypeCast = new JSFunction(this, Function_Constructor, Object_Type);

    Array_Type->defineUnaryOperator(Index, new JSFunction(this, Array_GetElement, Object_Type));
    Array_Type->defineUnaryOperator(IndexEqual, new JSFunction(this, Array_SetElement, Object_Type));

    Date_Type->mTypeCast = new JSFunction(this, Date_TypeCast, String_Type);
    Date_Type->defineStaticMethod(this, widenCString("parse"), NULL, new JSFunction(this, Date_parse, Number_Type));
    Date_Type->defineStaticMethod(this, widenCString("UTC"), NULL, new JSFunction(this, Date_UTC, Number_Type));

    String_Type->mTypeCast = new JSFunction(this, String_Constructor, String_Type);

}


OperatorMap operatorMap;
//OperatorHashTable operatorHashTable;

struct OperatorInitData {
    char *operatorString;
    JS2Runtime::Operator op;
} operatorInitData[] = 
{
    { "~",            JS2Runtime::Complement,       },
    { "++",           JS2Runtime::Increment,        },
    { "--",           JS2Runtime::Decrement,        },
    { "()",           JS2Runtime::Call,             },
    { "new",          JS2Runtime::New,              },
    { "[]",           JS2Runtime::Index,            },
    { "[]=",          JS2Runtime::IndexEqual,       },
    { "delete[]",     JS2Runtime::DeleteIndex,      },
    { "+",            JS2Runtime::Plus,             },
    { "-",            JS2Runtime::Minus,            },
    { "*",            JS2Runtime::Multiply,         },
    { "/",            JS2Runtime::Divide,           },
    { "%",            JS2Runtime::Remainder,        },
    { "<<",           JS2Runtime::ShiftLeft,        },
    { ">>",           JS2Runtime::ShiftRight,       },
    { ">>>",          JS2Runtime::UShiftRight,      },
    { "<",            JS2Runtime::Less,             },
    { "<=",           JS2Runtime::LessEqual,        },
    { "in",           JS2Runtime::In,               },
    { "==",           JS2Runtime::Equal,            },
    { "===",          JS2Runtime::SpittingImage,    },
    { "&",            JS2Runtime::BitAnd,           },
    { "^",            JS2Runtime::BitXor,           },
    { "|",            JS2Runtime::BitOr,            },
};

static void initOperatorTable()
{
    static bool mapped = false;
    if (!mapped) {
        for (uint32 i = 0; (i < sizeof(operatorInitData) / sizeof(OperatorInitData)); i++) {
            const String& name = widenCString(operatorInitData[i].operatorString);
            operatorMap[name] = operatorInitData[i].op;
/*
            operatorHashTable.insert(name, OperatorEntry(name, operatorInitData[i].op)  );
*/
        }
        mapped = true;
    }
}

JS2Runtime::Operator Context::getOperator(uint32 parameterCount, const String &name)
{
    OperatorMap::iterator it = operatorMap.find(name);
    if (it == operatorMap.end())
        return JS2Runtime::None;

    if ((it->second == JS2Runtime::Plus) && (parameterCount == 1))
        return JS2Runtime::Posate;
    if ((it->second == JS2Runtime::Minus) && (parameterCount == 1))
        return JS2Runtime::Negate;

    return it->second;
}


Context::Context(JSObject **global, World &world, Arena &a, Pragma::Flags flags) 

    : Virtual_StringAtom        (world.identifiers["virtual"]),
      Constructor_StringAtom    (world.identifiers["constructor"]),
      Operator_StringAtom       (world.identifiers["operator"]),
      Fixed_StringAtom          (world.identifiers["fixed"]),
      Dynamic_StringAtom        (world.identifiers["dynamic"]),
      Extend_StringAtom         (world.identifiers["extend"]),
      Prototype_StringAtom      (world.identifiers["prototype"]),
      Forin_StringAtom          (world.identifiers["forin"]),
      Value_StringAtom          (world.identifiers["value"]),
      Next_StringAtom           (world.identifiers["next"]),
      Done_StringAtom           (world.identifiers["done"]),
      Undefined_StringAtom      (world.identifiers["undefined"]),
      Object_StringAtom         (world.identifiers["object"]),
      Boolean_StringAtom        (world.identifiers["boolean"]),
      Number_StringAtom         (world.identifiers["number"]),
      String_StringAtom         (world.identifiers["string"]),
      Function_StringAtom       (world.identifiers["function"]),
      HasInstance_StringAtom    (world.identifiers["hasInstance"]),
      True_StringAtom           (world.identifiers["true"]),
      False_StringAtom          (world.identifiers["false"]),
      Null_StringAtom           (world.identifiers["null"]),
      ToString_StringAtom       (world.identifiers["toString"]),
      ValueOf_StringAtom        (world.identifiers["valueOf"]),
      Length_StringAtom         (world.identifiers["length"]),
      FromCharCode_StringAtom   (world.identifiers["fromCharCode"]),
      Math_StringAtom           (world.identifiers["Math"]),
      NaN_StringAtom            (world.identifiers["NaN"]),
      Eval_StringAtom           (world.identifiers["eval"]),
      Infinity_StringAtom       (world.identifiers["Infinity"]),
      Empty_StringAtom          (world.identifiers[""]),
      Arguments_StringAtom      (world.identifiers["arguments"]),

      mWorld(world),
      mScopeChain(NULL),
      mArena(a),
      mFlags(flags),
      mDebugFlag(false),
      mCurModule(NULL),
      mPC(NULL),
      mThis(kNullValue),
      mStack(NULL),
      mStackTop(0),
      mStackMax(0),
      mLocals(NULL),
      mArgumentBase(NULL),
      mReader(NULL),
      mGlobal(global) 

{
    uint32 i;
    
    initOperatorTable();

    mScopeChain = new ScopeChain(this, mWorld);
    if (Object_Type == NULL) {                
        initBuiltins();
    }
    
    JSType *MathType = new JSType(this, &mWorld.identifiers[widenCString("Math")], Object_Type);
    JSObject *mathObj = MathType->newInstance(this);

    getGlobalObject()->defineVariable(this, Math_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Object_Type, JSValue(mathObj));
    initMathObject(this, mathObj);
    initDateObject(this);
    
    Number_Type->defineVariable(this, widenCString("MAX_VALUE"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue(maxValue));
    Number_Type->defineVariable(this, widenCString("MIN_VALUE"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue(minValue));
    Number_Type->defineVariable(this, widenCString("NaN"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue(nan));
    Number_Type->defineVariable(this, widenCString("POSITIVE_INFINITY"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue(positiveInfinity));
    Number_Type->defineVariable(this, widenCString("NEGATIVE_INFINITY"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue(negativeInfinity));

    initOperators();
    
    struct Attribute_Init {
        char *name;
        uint32 trueFlags;
        uint32 falseFlags;
    } attribute_init[] = 
    {
                                                        // XXX these false flags are WAY NOT COMPLETE!!
                                                        //
        { "indexable",      Property::Indexable,        0 },
        { "enumerable",     Property::Enumerable,       0 },
        { "virtual",        Property::Virtual,          Property::Static | Property::Constructor },
        { "constructor",    Property::Constructor,      Property::Virtual },
        { "operator",       Property::Operator,         Property::Virtual | Property::Constructor },
        { "dynamic",        Property::Dynamic,          0 },
        { "fixed",          0,                          Property::Dynamic },
        { "prototype",      Property::Prototype,        0 },
        { "static",         Property::Static,           Property::Virtual },
        { "abstract",       Property::Abstract,         Property::Static },
        { "override",       Property::Override,         Property::Static },
        { "mayOverride",    Property::MayOverride,      Property::Static },
        { "true",           Property::True,             0 },
        { "false",          0,                          Property::True },
        { "public",         Property::Public,           Property::Private },
        { "private",        Property::Private,          Property::Public },
        { "final",          Property::Final,            Property::Virtual | Property::Abstract },
        { "const",          Property::Const,            0 },
    };
    
    for (i = 0; i < (sizeof(attribute_init) / sizeof(Attribute_Init)); i++) {
        Attribute *attr = new Attribute(attribute_init[i].trueFlags, attribute_init[i].falseFlags);
        getGlobalObject()->defineVariable(this, widenCString(attribute_init[i].name), (NamespaceList *)(NULL), Property::NoAttribute, Attribute_Type, JSValue(attr));
    }

    JSFunction *x = new JSFunction(this, ExtendAttribute_Invoke, Attribute_Type);
    getGlobalObject()->defineVariable(this, Extend_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Attribute_Type, JSValue(x));


    
    
    ProtoFunDef globalObjectFunctions[] = {
        { "eval",           Object_Type,  1, GlobalObject_Eval },
        { "parseInt",       Number_Type,  2, GlobalObject_ParseInt },
        { "parseFloat",     Number_Type,  2, GlobalObject_ParseFloat },
        { "isNaN",          Boolean_Type, 2, GlobalObject_isNaN },
        { "isFinite",       Boolean_Type, 2, GlobalObject_isFinite },
    };
    
    for (i = 0; i < (sizeof(globalObjectFunctions) / sizeof(ProtoFunDef)); i++) {
        x = new JSFunction(this, globalObjectFunctions[i].imp, globalObjectFunctions[i].result);
        x->setArgCounts(this, globalObjectFunctions[i].length, 0, false);
        x->setIsPrototype(true);
        getGlobalObject()->defineVariable(this, widenCString(globalObjectFunctions[i].name), (NamespaceList *)(NULL), Property::NoAttribute, globalObjectFunctions[i].result, JSValue(x));    
    }

/*    
    x = new JSFunction(GlobalObject_Eval, Object_Type);
    x->setArgCounts(1, 0, false);
    x->setIsPrototype(true);
    getGlobalObject()->defineVariable(this, Eval_StringAtom, (NamespaceList *)(NULL), Object_Type, JSValue(x));

    x = new JSFunction(GlobalObject_ParseInt, Number_Type);
    x->setArgCounts(2, 0, false);
    x->setIsPrototype(true);
    getGlobalObject()->defineVariable(this, widenCString("parseInt"), (NamespaceList *)(NULL), Object_Type, JSValue(x));
*/

    getGlobalObject()->defineVariable(this, Undefined_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Void_Type, kUndefinedValue);
    getGlobalObject()->defineVariable(this, NaN_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Void_Type, kNaNValue);
    getGlobalObject()->defineVariable(this, Infinity_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Void_Type, kPositiveInfinity);                

}

void Context::reportError(Exception::Kind kind, char *message, size_t pos, const char *arg)
{
    const char16 *lineBegin;
    const char16 *lineEnd;
    String x = widenCString(message);
    if (arg) {
        uint32 a = x.find(widenCString("{0}"));
        x.replace(a, 3, widenCString(arg));
    }
    if (mReader) {
        uint32 lineNum = mReader->posToLineNum(pos);
        size_t linePos = mReader->getLine(lineNum, lineBegin, lineEnd);
        ASSERT(lineBegin && lineEnd && linePos <= pos);
        throw Exception(kind, x, 
                            mReader->sourceLocation, 
                            lineNum, pos - linePos, pos, lineBegin, lineEnd);
    }
    else {
        if (mCurModule) {
            Reader reader(mCurModule->mSource, mCurModule->mSourceLocation);
            reader.fillLineStartsTable();
            uint32 lineNum = reader.posToLineNum(pos);
            size_t linePos = reader.getLine(lineNum, lineBegin, lineEnd);
            ASSERT(lineBegin && lineEnd && linePos <= pos);
            throw Exception(kind, x, 
                                reader.sourceLocation, 
                                lineNum, pos - linePos, pos, lineBegin, lineEnd);
        }
    }
    
    throw Exception(kind, x); 
}

// assumes mPC has been set inside the interpreter loop prior 
// to dispatch to whatever routine invoked this error reporter
void Context::reportError(Exception::Kind kind, char *message, const char *arg)
{
    size_t pos = 0;
    if (mCurModule)
        pos = mCurModule->getPositionForPC(toUInt32(mPC - mCurModule->mCodeBase));
    reportError(kind, message, pos, arg);
}

void Context::reportError(Exception::Kind kind, char *message, const String& name)
{
    std::string str(name.length(), char());
    std::transform(name.begin(), name.end(), str.begin(), narrow);
    reportError(kind, message, str.c_str() );
}

void Context::reportError(Exception::Kind kind, char *message, size_t pos, const String& name)
{
    std::string str(name.length(), char());
    std::transform(name.begin(), name.end(), str.begin(), narrow);
    reportError(kind, message, pos, str.c_str());
}

Formatter& operator<<(Formatter& f, const JSValue& value)
{
    switch (value.tag) {
    case JSValue::f64_tag:
        f << value.f64;
        break;
    case JSValue::object_tag:
        printFormat(f, "Object @ 0x%08X\n", value.object);
        f << *value.object;
        break;
    case JSValue::type_tag:
        printFormat(f, "Type @ 0x%08X\n", value.type);
        f << *value.type;
        break;
    case JSValue::boolean_tag:
        f << ((value.boolean) ? "true" : "false");
        break;
    case JSValue::string_tag:
        f << *value.string;
        break;
    case JSValue::undefined_tag:
        f << "undefined";
        break;
    case JSValue::null_tag:
        f << "null";
        break;
    case JSValue::function_tag:
        if (!value.function->isNative()) {
            FunctionName *fnName = value.function->getFunctionName();
            if (fnName) {
                StringFormatter s;
                PrettyPrinter pp(s);
                fnName->print(pp);
                f << "function '" << s.getString() << "'\n" << *value.function->getByteCode();
            }
            else {
                f << "function anonymous\n" << *value.function->getByteCode();
            }
        }
        else
            f << "function\n";
        break;
    default:
        NOT_REACHED("Bad tag");
    }
    return f;
}

void JSType::printSlotsNStuff(Formatter& f) const
{
    f << "var. count = " << mVariableCount << "\n";
    f << "method count = " << (uint32)(mMethods.size()) << "\n";
    uint32 index = 0;
    for (MethodList::const_iterator i = mMethods.begin(), end = mMethods.end(); (i != end); i++) {
        f << "[#" << index++ << "]";
        if (*i == NULL)
            f << "NULL\n";
        else
            if (!(*i)->isNative()) {
                ByteCodeModule *m = (*i)->getByteCode();
                if (m)
                    f << *m;
            }
    }
//    if (mStatics)
//        f << "Statics :\n" << *mStatics;
}

Formatter& operator<<(Formatter& f, const JSObject& obj)
{
    obj.printProperties(f);
    return f;
}
Formatter& operator<<(Formatter& f, const JSType& obj)
{
    printFormat(f, "super @ 0x%08X\n", obj.mSuperType);
    f << "properties\n";
    obj.printProperties(f);
    f << "slotsnstuff\n";
    obj.printSlotsNStuff(f);
    f << "done with type\n";
    return f;
}
Formatter& operator<<(Formatter& f, const Access& slot)
{
    switch (slot) {
    case Read : f << "Read\n"; break;
    case Write : f << "Write\n"; break;
    }
    return f;
}
Formatter& operator<<(Formatter& f, const Property& prop)
{
    switch (prop.mFlag) {
    case ValuePointer : 
        {
            JSValue v = *prop.mData.vp;
            f << "ValuePointer --> "; 
            if (v.isObject())
                printFormat(f, "Object @ 0x%08X\n", v.object);
            else
            if (v.isType())
                printFormat(f, "Type @ 0x%08X\n", v.type);
            else
            if (v.isFunction())
                printFormat(f, "Function @ 0x%08X\n", v.function);
            else
                f << v << "\n";
        }
        break;
    case FunctionPair : f << "FunctionPair\n"; break;
    case IndexPair : f << "IndexPair\n"; break;
    case Slot : f << "Slot\n"; break;
    case Constructor : f << "Constructor\n"; break;
    case Method : f << "Method\n"; break;
    }
    return f;
}
Formatter& operator<<(Formatter& f, const JSInstance& obj)
{
    for (PropertyMap::const_iterator i = obj.mProperties.begin(), end = obj.mProperties.end(); (i != end); i++) {
        const Property *prop = PROPERTY(i);
        f << "[" << PROPERTY_NAME(i) << "] ";
        switch (prop->mFlag) {
        case ValuePointer : f << "ValuePointer --> " << *prop->mData.vp; break;
        case FunctionPair : f << "FunctionPair\n"; break;
        case IndexPair : f << "IndexPair\n"; break;
        case Slot : f << "Slot #" << prop->mData.index 
                         << " --> " << obj.mInstanceValues[prop->mData.index] << "\n"; break;
        case Method : f << "Method #" << prop->mData.index << "\n"; break;
        case Constructor : f << "Constructor #" << prop->mData.index << "\n"; break;
        }
    }
    return f;
}




}
}

