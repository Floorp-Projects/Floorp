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

JSType *Package_Type;
JSType *NamedArgument_Type;
JSArrayType *Array_Type;
JSType *Date_Type;
JSType *RegExp_Type;
JSType *Error_Type;
JSType *EvalError_Type;
JSType *RangeError_Type;
JSType *ReferenceError_Type;
JSType *SyntaxError_Type;
JSType *TypeError_Type;
JSType *UriError_Type;


Attribute *Context::executeAttributes(ExprNode *attr)
{
    ASSERT(attr);

    ByteCodeGen bcg(this, mScopeChain);
    ByteCodeModule *bcm = bcg.genCodeForExpression(attr);
//    stdOut << *bcm;
    js2val result = interpret(bcm, 0, NULL, JSValue::newObject(getGlobalObject()), NULL, 0);

    ASSERT(JSValue::isAttribute(result));
    return JSValue::attribute(result);
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
bool JSObject::hasOwnProperty(Context * /*cx*/, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
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

bool JSObject::hasProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    if (hasOwnProperty(cx, name, names, acc, p))
        return true;
    else
        if (!JSValue::isNull(mPrototype))
            return JSValue::getObjectValue(mPrototype)->hasProperty(cx, name, names, acc, p);
        else
            return false;
}

bool JSObject::deleteProperty(Context * /*cx*/, const String &name, NamespaceList *names)
{    
    PropertyIterator i = findNamespacedProperty(name, names);
    if (i != mProperties.end()) {
        if ((PROPERTY_ATTR(i) & Property::DontDelete) == 0) {
            mProperties.erase(i);
            return true;
        }
    }
    return false;
}


// get a property value
js2val JSObject::getPropertyValue(PropertyIterator &i)
{
    Property *prop = PROPERTY(i);
    ASSERT(prop->mFlag == ValuePointer);
    return prop->mData.vp;
}

Property *JSObject::insertNewProperty(const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const js2val v)
{
    Property *prop = new Property(v, type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}

void JSObject::defineGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator i;
    if (hasProperty(cx, name, names, Write, &i)) {
        ASSERT(PROPERTY_KIND(i) == FunctionPair);
        ASSERT(PROPERTY_GETTERF(i) == NULL);
        PROPERTY_GETTERF(i) = f;
    }
    else {
        const PropertyMap::value_type e(name, new NamespacedProperty(new Property(Function_Type, f, NULL, attrFlags), names));
        mProperties.insert(e);
    }
}
void JSObject::defineSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator i;
    if (hasProperty(cx, name, names, Read, &i)) {
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
    readRef = new NameReference(name, NULL, Read, Object_Type, 0);
    writeRef = new NameReference(name, NULL, Write, Object_Type, 0);
}


// add a property
Property *JSObject::defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it)) {
        // not a problem if neither are consts
        if ((attrFlags & Property::Const)
               || (PROPERTY_ATTR(it) & Property::Const)) {
            if (attr)
                cx->reportError(Exception::typeError, "Duplicate definition '{0}'", attr->pos, name);
            else
                cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
    }

    Property *prop = new Property(kUndefinedValue, type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}
Property *JSObject::defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type)
{
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it)) {
        // not a problem if neither are consts
        if (PROPERTY_ATTR(it) & Property::Const) {
            cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
    }

    Property *prop = new Property(kUndefinedValue, type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}

// add a property (with a value)
Property *JSObject::defineVariable(Context *cx, const String &name, AttributeStmtNode *attr, JSType *type, const js2val v)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it)) {
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
            PROPERTY_VALUEPOINTER(it) = v;
            return PROPERTY(it);
        }
    }

    Property *prop = new Property(v, type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}
Property *JSObject::defineVariable(Context *cx, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, const js2val v)
{
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it)) {
        if (PROPERTY_ATTR(it) & Property::Const) {
            cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
        }
    }

    Property *prop = new Property(v, type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}
Property *JSObject::defineAlias(Context * /*cx*/, const String &name, NamespaceList *names, PropertyAttribute attrFlags, JSType *type, js2val vp)
{
    Property *prop = new Property(vp, type, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}




Reference *JSObject::genReference(Context *cx, bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 /*depth*/)
{
    PropertyIterator i;
    if (hasProperty(cx, name, names, acc, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case ValuePointer:
            if (hasBase)
                return new PropertyReference(name, names, acc, prop->mType, prop->mAttributes);
            else
                return new NameReference(name, names, acc, prop->mType, prop->mAttributes);
        case FunctionPair:
            if (acc == Read)
                return new GetterFunctionReference(prop->mData.fPair.getterF, prop->mAttributes);
            else {
                JSFunction *f = prop->mData.fPair.setterF;
                return new SetterFunctionReference(f, f->getParameterType(0), prop->mAttributes);
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
    if (hasOwnProperty(cx, name, names, Read, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case ValuePointer:
            cx->pushValue(prop->mData.vp);
            break;
        case FunctionPair:
            cx->pushValue(cx->invokeFunction(prop->mData.fPair.getterF, JSValue::newObject(this), NULL, 0));
            break;
        default:
            ASSERT(false);  // XXX more to implement
            break;
        }
    }
    else {
        if (!JSValue::isNull(mPrototype))
            JSValue::getObjectValue(mPrototype)->getProperty(cx, name, names);
        else
            cx->pushValue(kUndefinedValue);        
    }
}


void JSType::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    PropertyIterator i;
    if (hasOwnProperty(cx, name, names, Read, &i))
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
    if (hasOwnProperty(cx, name, names, Read, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case Slot:
            cx->pushValue(mInstanceValues[prop->mData.index]);
            break;
        case ValuePointer:
            cx->pushValue(prop->mData.vp);
            break;
        case FunctionPair:
            cx->pushValue(cx->invokeFunction(prop->mData.fPair.getterF, JSValue::newObject(this), NULL, 0));
            break;
        case Constructor:
        case Method:
            cx->pushValue(JSValue::newFunction(mType->mMethods[prop->mData.index]));
            break;
        case IndexPair:
            cx->pushValue(cx->invokeFunction(mType->mMethods[prop->mData.iPair.getterI], JSValue::newObject(this), NULL, 0));
            break;
        default:
            ASSERT(false);  // XXX more to implement
            break;
        }
    }
    else
/*
XXX this path allows an instance to access static fields of it's type
        if (mType->hasOwnProperty(cx, name, names, Read, &i))
            mType->getProperty(cx, name, names);
        else
*/
            JSObject::getProperty(cx, name, names);
}

void JSObject::setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v)
{
    PropertyIterator i;
    if (hasOwnProperty(cx, name, names, Write, &i)) {
        if (PROPERTY_ATTR(i) & Property::ReadOnly) return;
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case ValuePointer:
            if (name.compare(cx->UnderbarPrototype_StringAtom) == 0)
                mPrototype = v;
            prop->mData.vp = v;
            break;
        case FunctionPair:
            {
                js2val argv = v;
                cx->invokeFunction(prop->mData.fPair.setterF, JSValue::newObject(this), &argv, 1);
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

void JSType::setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v)
{
    PropertyIterator i;
    if (hasOwnProperty(cx, name, names, Read, &i))
        JSObject::setProperty(cx, name, names, v);
    else
        if (mSuperType)
            mSuperType->setProperty(cx, name, names, v);
        else
            JSObject::setProperty(cx, name, names, v);
}

void JSInstance::setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v)
{
    PropertyIterator i;
    if (hasOwnProperty(cx, name, names, Write, &i)) {
        if (PROPERTY_ATTR(i) & Property::ReadOnly) return;
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case Slot:
            mInstanceValues[prop->mData.index] = v;
            break;
        case ValuePointer:
            prop->mData.vp = v;
            break;
        case FunctionPair: 
            {
                js2val argv = v;
                cx->invokeFunction(prop->mData.fPair.setterF, JSValue::newObject(this), &argv, 1);
            }
            break;
        case IndexPair: 
            {
                js2val argv = v;
                cx->invokeFunction(mType->mMethods[prop->mData.iPair.setterI], JSValue::newObject(this), &argv, 1);
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
        if (mType->hasOwnProperty(cx, name, names, Write, &i))
            mType->setProperty(cx, name, names, v);
        else
            defineVariable(cx, name, names, Property::Enumerable, Object_Type, v);
    }
}

void JSArrayInstance::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        cx->pushValue(JSValue::newNumber((float64)mLength));
    else
        JSInstance::getProperty(cx, name, names);
}

void JSArrayInstance::setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v)
{
    if (name.compare(cx->Length_StringAtom) == 0) {
        uint32 newLength = (uint32)JSValue::f64(JSValue::toUInt32(cx, v));
        if (newLength != JSValue::f64(JSValue::toNumber(cx, v)))
            cx->reportError(Exception::rangeError, "out of range value for length"); 
        
        for (uint32 i = newLength; i < mLength; i++) {
            const String *id = numberToString(i);
            if (findNamespacedProperty(*id, NULL) != mProperties.end())
                deleteProperty(cx, *id, NULL);
            delete id;
        }

        mLength = newLength;
    }
    else {
        PropertyIterator it = findNamespacedProperty(name, names);
        if (it == mProperties.end())
            insertNewProperty(name, names, Property::Enumerable, Object_Type, v);
        else {
            Property *prop = PROPERTY(it);
            ASSERT(prop->mFlag == ValuePointer);
            prop->mData.vp = v;
        }
        js2val v = JSValue::newString(&name);
        js2val v_int = JSValue::toUInt32(cx, v);
        if ((JSValue::f64(v_int) != two32minus1) && (JSValue::string(JSValue::toString(cx, v_int))->compare(name) == 0)) {
            if (JSValue::f64(v_int) >= mLength)
                mLength = (uint32)(JSValue::f64(v_int)) + 1;
        }
    }
}

bool JSArrayInstance::hasOwnProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        return true;
    else
        return JSInstance::hasOwnProperty(cx, name, names, acc, p);
}

bool JSArrayInstance::deleteProperty(Context *cx, const String &name, NamespaceList *names)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        return false;
    else
        return JSInstance::deleteProperty(cx, name, names);
}


// get a named property from a string instance, but intercept
// 'length' by returning the known value
void JSStringInstance::getProperty(Context *cx, const String &name, NamespaceList *names)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        cx->pushValue(JSValue::newNumber((float64)(mValue->size())));
    else
        JSInstance::getProperty(cx, name, names);
}

void JSStringInstance::setProperty(Context *cx, const String &name, NamespaceList *names, const js2val v)
{
    if (name.compare(cx->Length_StringAtom) == 0) {
    }
    else
        JSInstance::setProperty(cx, name, names, v);
}

bool JSStringInstance::hasOwnProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        return true;
    else
        return JSInstance::hasOwnProperty(cx, name, names, acc, p);
}

bool JSStringInstance::deleteProperty(Context *cx, const String &name, NamespaceList *names)
{
    if (name.compare(cx->Length_StringAtom) == 0)
        return false;
    else
        return JSInstance::deleteProperty(cx, name, names);
}


// construct an instance of a type
// - allocate memory for the slots, load the instance variable names into the 
// property map - in order to provide dynamic access to those properties.
void JSInstance::initInstance(Context *cx, JSType *type)
{
    if (type->mVariableCount)
        mInstanceValues = new js2val[type->mVariableCount];

    // copy the instance variable names into the property map 
    for (PropertyIterator pi = type->mProperties.begin(), 
                end = type->mProperties.end();
                (pi != end); pi++) {
        if (PROPERTY_KIND(pi) == Slot) {
            const PropertyMap::value_type e(PROPERTY_NAME(pi), NAMESPACED_PROPERTY(pi));
            mProperties.insert(e);
        }
    }

    // and then do the same for the super types
    JSType *t = type->mSuperType;
    while (t) {
        for (PropertyIterator i = t->mProperties.begin(), 
                    end = t->mProperties.end();
                    (i != end); i++) {            
            if (PROPERTY_KIND(i) == Slot) {
                const PropertyMap::value_type e(PROPERTY_NAME(i), NAMESPACED_PROPERTY(i));
                mProperties.insert(e);
            }
        }
        if (t->mInstanceInitializer)
            cx->invokeFunction(t->mInstanceInitializer, JSValue::newObject(this), NULL, 0);
        t = t->mSuperType;
    }

    // run the initializer
    if (type->mInstanceInitializer) {
        cx->invokeFunction(type->mInstanceInitializer, JSValue::newObject(this), NULL, 0);
    }

    mType = type;
}

// Create a new (empty) instance of this class. The prototype
// link for this new instance is established from the type's
// prototype object.
js2val JSType::newInstance(Context *cx)
{
    JSInstance *result = new JSInstance(cx, this);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

js2val JSObjectType::newInstance(Context *cx)
{
    JSObject *result = new JSObject();
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newObject(result);
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
        cx->interpret(f->getByteCode(), 0, f->getScopeChain(), JSValue::newObject(this), NULL, 0);
}

Property *JSType::defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it)) {
/*
XXX error for classes, right? but not for local variables under what circumstances (hoisting impact?)
        if (attr)
            cx->reportError(Exception::typeError, "Duplicate definition '{0}'", attr->pos, name);
        else
            cx->reportError(Exception::typeError, "Duplicate definition '{0}'", name);
*/
    }
    Property *prop = new Property(mVariableCount++, type, Slot, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}

js2val JSArrayType::newInstance(Context *cx)
{
    JSInstance *result = new JSArrayInstance(cx);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

js2val JSStringType::newInstance(Context *cx)
{
    JSInstance *result = new JSStringInstance(cx);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

js2val JSBooleanType::newInstance(Context *cx)
{
    JSInstance *result = new JSBooleanInstance(cx);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

js2val JSRegExpType::newInstance(Context *cx)
{
    JSInstance *result = new JSRegExpInstance(cx);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

js2val JSDateType::newInstance(Context *cx)
{
    JSInstance *result = new JSDateInstance(cx);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

js2val JSNumberType::newInstance(Context *cx)
{
    JSInstance *result = new JSNumberInstance(cx);
    result->mPrototype = mPrototypeObject;
    result->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototypeObject);
    return JSValue::newInstance(result);
}

// don't add to instances etc., climb all the way down (likely to the global object)
// and add the property there.
void ScopeChain::setNameValue(Context *cx, const String& name, NamespaceList *names)
{
    js2val v = cx->topValue();
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(cx, name, names, Write, &i)) {
            PropertyFlag flag = PROPERTY_KIND(i);
            switch (flag) {
            case ValuePointer:
                PROPERTY_VALUEPOINTER(i) = v;
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
    cx->getGlobalObject()->defineVariable(cx, name, names, 0, Object_Type, v);
}

bool ScopeChain::deleteName(Context *cx, const String& name, NamespaceList *names)
{
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++)
    {
        PropertyIterator i;
        if ((*s)->hasOwnProperty(cx, name, names, Read, &i))
            return (*s)->deleteProperty(cx, name, names);
    }
    return true;
}

inline char narrow(char16 ch) { return char(ch); }

JSObject *ScopeChain::getNameValue(Context *cx, const String& name, NamespaceList *names)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(cx, name, names, Read, &i)) {
            PropertyFlag flag = PROPERTY_KIND(i);
            switch (flag) {
            case ValuePointer:
                cx->pushValue(PROPERTY_VALUEPOINTER(i));
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

// it'd be much better if the property iterator returned by hasProperty could be
// used by the genReference call
Reference *ScopeChain::getName(Context *cx, const String& name, NamespaceList *names, Access acc)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(cx, name, names, acc, &i))
            return (*s)->genReference(cx, false, name, names, acc, depth);
        else
            if ((*s)->isDynamic())
                return NULL;

    }
    return NULL;
}

bool ScopeChain::hasNameValue(Context *cx, const String& name, NamespaceList *names)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(cx, name, names, Read, &i))
            return true;
    }
    return false;
}

// a compile time request to get the value for a name
// (i.e. we're accessing a constant value)
js2val ScopeChain::getCompileTimeValue(Context *cx, const String& name, NamespaceList *names)
{
    uint32 depth = 0;
    for (ScopeScanner s = mScopeStack.rbegin(), end = mScopeStack.rend(); (s != end); s++, depth++)
    {
        PropertyIterator i;
        if ((*s)->hasProperty(cx, name, names, Read, &i)) 
            return (*s)->getPropertyValue(i);
    }
    return kUndefinedValue;
}


// in the case of duplicate parameter names, pick the last one
// XXX does the namespace handling make any sense here? Can parameters be in a namespace?
Reference *ParameterBarrel::genReference(Context * /* cx */, bool /* hasBase */, const String& name, NamespaceList *names, Access acc, uint32 /*depth*/)
{
    Property *selectedProp = NULL;
    for (PropertyIterator i = mProperties.lower_bound(name),
                    end = mProperties.upper_bound(name); (i != end); i++) {
        NamespaceList *propNames = PROPERTY_NAMESPACELIST(i);
        if (names) {
            if (propNames == NULL)
                continue;       // a namespace list was specified, no match
            while (names) {
                NamespaceList *propNameEntry = propNames;
                while (propNameEntry) {
                    if (names->mName == propNameEntry->mName) {
                        Property *prop = PROPERTY(i);
                        ASSERT(prop->mFlag == Slot);
                        if (selectedProp == NULL)
                            selectedProp = prop;
                        else {
                            if (PROPERTY_INDEX(i) > selectedProp->mData.index)
                                selectedProp = prop;
                        }
                        break;
                    }
                    propNameEntry = propNameEntry->mNext;
                }
                names = names->mNext;
            }
        }
        else {
            if (propNames)  // entry is in a namespace, but none called for, no match
                continue;
            Property *prop = PROPERTY(i);
            ASSERT(prop->mFlag == Slot);
            if (selectedProp == NULL)
                selectedProp = prop;
            else {
                if (PROPERTY_INDEX(i) > selectedProp->mData.index)
                    selectedProp = prop;
            }
        }
    }
    ASSERT(selectedProp);
    return new ParameterReference(selectedProp->mData.index, acc, selectedProp->mType, selectedProp->mAttributes);
}

js2val ParameterBarrel::getSlotValue(Context *cx, uint32 slotIndex)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        return prev->mArgumentBase[slotIndex];        
    }
    return cx->mArgumentBase[slotIndex];
}

void ParameterBarrel::setSlotValue(Context *cx, uint32 slotIndex, js2val v)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        prev->mArgumentBase[slotIndex] = v;        
    }
    else
        cx->mArgumentBase[slotIndex] = v;
}

Property *ParameterBarrel::defineVariable(Context *cx, const String& name, AttributeStmtNode *attr, JSType *type)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyAttribute attrFlags = (attr) ? attr->attributeValue->mTrueFlags : 0;
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it)) {
        // XXX duplicate parameter name, ok for all functions, or just unchecked ones?
    }
    Property *prop = new Property(mVariableCount++, type, Slot, attrFlags);
    const PropertyMap::value_type e(name, new NamespacedProperty(prop, names));
    mProperties.insert(e);
    return prop;
}



js2val Activation::getSlotValue(Context *cx, uint32 slotIndex)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        return prev->mLocals[slotIndex];        
    }
    return cx->mLocals[slotIndex];
}

void Activation::setSlotValue(Context *cx, uint32 slotIndex, js2val v)
{
    // find the appropriate activation object:
    if (cx->mArgumentBase == NULL) {// then must be in eval code, 
        Activation *prev = cx->mActivationStack.top();
        prev->mLocals[slotIndex] = v;
    }
    else
        cx->mLocals[slotIndex] =v;
}







JSType *ScopeChain::findType(Context *cx, const StringAtom& typeName, size_t pos) 
{
    js2val v = getCompileTimeValue(cx, typeName, NULL);
    if (!JSValue::isUndefined(v)) {
        if (JSValue::isType(v))
            return JSValue::type(v);
        else {
            // Allow finding a function that has the same name as it's containing class
            // i.e. the default constructor.
            FunctionName *fnName = JSValue::function(v)->getFunctionName();
            if ((fnName->prefix == FunctionName::normal)
                    && JSValue::isFunction(v) && JSValue::function(v)->getClass() 
                        && (JSValue::function(v)->getClass()->mClassName->compare(*fnName->name) == 0))
                return JSValue::function(v)->getClass();
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
                type = findType(m_cx, typeExpr->name, t->pos);
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
                    type = new JSArrayType(m_cx, element, NULL, Object_Type, kNullValue, kNullValue);    // XXX or is this a descendant of Array[Object]?
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


// Iterates over the linked list of statements, p.
// 1. Adds 'symbol table' entries for each class, var & function by defining

//      them in the object at the top of the scope chain
// 2. Using information from pass 1, evaluate types (and XXX later XXX other
//      compile-time constants) to complete the definitions
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
bool ScopeChain::isPossibleUncheckedFunction(FunctionDefinition &f)
{
    bool result = false;
    if ((f.resultType == NULL)
            && (f.optParameters == NULL)
            && (f.prefix == FunctionName::normal)
            && (topClass() == NULL)) {
        result = true;
        VariableBinding *b = f.parameters;
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



/*

    Build a name for the package from the identifier list

*/
String ScopeChain::getPackageName(IdentifierList *packageIdList)
{
    String packagePath;
    IdentifierList *idList = packageIdList;
    while (idList) {
	packagePath += idList->name;
	idList = idList->next;
	if (idList)
	    packagePath += '/'; // XXX how to get path separator for OS?
    }
    return packagePath;
}

// counts the number of pigs that can fit in a small wicker basket
void JSFunction::countParameters(Context *cx, FunctionDefinition &f)
{
    uint32 requiredParameterCount = 0;
    uint32 optionalParameterCount = 0;
    uint32 namedParameterCount = 0;

    VariableBinding *b = f.parameters;
    while (b != f.optParameters) {
        requiredParameterCount++;
        b = b->next;
    }
    while (b != f.restParameter) {
        optionalParameterCount++;
        b = b->next;
    }
    b = f.namedParameters;
    while (b) {
        namedParameterCount++;
        b = b->next;
    }
    setParameterCounts(cx, requiredParameterCount, optionalParameterCount, namedParameterCount, f.restParameter != f.namedParameters);
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
            JSType *thisClass = new JSType(m_cx, name, NULL, kNullValue, kNullValue);

            m_cx->setAttributeValue(classStmt, 0);              // XXX default attribute for a class?

            PropertyIterator it;
            if (hasProperty(m_cx, *name, NULL, Read, &it))
                m_cx->reportError(Exception::referenceError, "Duplicate class definition", p->pos);

            defineVariable(m_cx, *name, classStmt, Type_Type, JSValue::newType(thisClass));
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
    case StmtNode::group:
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
    case StmtNode::With:
    case StmtNode::If:
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
            collectNames(t->stmt);
            if (t->catches) {
                CatchClause *c = t->catches;
                while (c) {
                    collectNames(c->stmt);
                    c->prop = defineVariable(m_cx, c->name, NULL, NULL);
                    c = c->next;
                }
            }
            if (t->finally) collectNames(t->finally);
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
                vs->attributeValue->mNamespaceList = new NamespaceList(*theClass->mPrivateNamespace, vs->attributeValue->mNamespaceList);
            }

            while (v)  {
                if (isStatic)
                    v->prop = defineStaticVariable(m_cx, *v->name, vs, NULL);
                else
                    v->prop = defineVariable(m_cx, *v->name, vs, NULL);
                v->scope = mScopeStack.back();
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
                    && isPossibleUncheckedFunction(f->function)) {
                isPrototype = true;
                fnc->setIsUnchecked();
            }

            fnc->setIsPrototype(isPrototype);
            fnc->setIsConstructor(isConstructor);
            fnc->setFunctionName(f->function);
            f->mFunction = fnc;

            fnc->countParameters(m_cx, f->function);

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
                                if (!isPossibleUncheckedFunction(f->function))
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
    case StmtNode::Import:
        {
            ImportStmtNode *i = checked_cast<ImportStmtNode *>(p);
	    String packageName;
	    if (i->packageIdList)
		packageName = getPackageName(i->packageIdList);
            else
                packageName = *i->packageString;

            if (!m_cx->checkForPackage(packageName))
                m_cx->loadPackage(packageName, packageName + ".js");

	    js2val packageValue = getCompileTimeValue(m_cx, packageName, NULL);
            ASSERT(JSValue::isPackage(packageValue));
            Package *package = JSValue::package(packageValue);
            
            if (i->varName)
                defineVariable(m_cx, *i->varName, NULL, Package_Type, JSValue::newPackage(package));
            
            for (PropertyIterator it = package->mProperties.begin(), end = package->mProperties.end();
                        (it != end); it++)
            {
                ASSERT(PROPERTY_KIND(it) == ValuePointer);
                bool makeAlias = true;
                if (i->includeExclude) {
                    makeAlias = i->exclude;
                    IdentifierList *idList = i->includeExclude;
                    while (idList) {
                        if (idList->name.compare(PROPERTY_NAME(it)) == 0) {
                            makeAlias = !makeAlias;
                            break;
                        }
                        idList = idList->next;
                    }
                }
                if (makeAlias)
                    defineAlias(m_cx, PROPERTY_NAME(it), PROPERTY_NAMESPACELIST(it), PROPERTY_ATTR(it), PROPERTY_TYPE(it), PROPERTY_VALUEPOINTER(it));
            }

        }
        break;
    case StmtNode::Namespace:
        {
            NamespaceStmtNode *n = checked_cast<NamespaceStmtNode *>(p);
            Attribute *x = new Attribute(0, 0);
            x->mNamespaceList = new NamespaceList(n->name, x->mNamespaceList);
            m_cx->getGlobalObject()->defineVariable(m_cx, n->name, (NamespaceList *)(NULL), Property::NoAttribute, Attribute_Type, JSValue::newAttribute(x));            
        }
        break;
    case StmtNode::Package:
        {
            PackageStmtNode *ps = checked_cast<PackageStmtNode *>(p);
	    String packageName = getPackageName(ps->packageIdList);
	    Package *package = new Package(packageName);
            ps->scope = package;
            defineVariable(m_cx, packageName, NULL, Package_Type, JSValue::newPackage(package));
	    m_cx->mPackages.push_back(package);

	    addScope(ps->scope);
            collectNames(ps->body);
	    popScope();
            package->mStatus = Package::InHand;
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

void JSType::defineMethod(Context *cx, const String& name, AttributeStmtNode *attr, JSFunction *f)
{
    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;
    PropertyIterator it;
    if (hasOwnProperty(cx, name, names, Read, &it))
        cx->reportError(Exception::typeError, "Duplicate method definition", attr->pos);

    // now check if the method exists in the supertype
    if (mSuperType && mSuperType->hasOwnProperty(cx, name, names, Read, &it)) {
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

void JSType::defineGetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    PropertyIterator i;
    uint32 vTableIndex = mMethods.size();
    mMethods.push_back(f);

    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;

    if (hasProperty(cx, name, names, Write, &i)) {
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

void JSType::defineSetterMethod(Context *cx, const String &name, AttributeStmtNode *attr, JSFunction *f)
{
    PropertyIterator i;
    uint32 vTableIndex = mMethods.size();
    mMethods.push_back(f);

    NamespaceList *names = (attr) ? attr->attributeValue->mNamespaceList : NULL;

    if (hasProperty(cx, name, names, Read, &i)) {
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

js2val JSType::getPropertyValue(PropertyIterator &i)
{
    Property *prop = PROPERTY(i);
    switch (prop->mFlag) {
    case ValuePointer:
        return prop->mData.vp;
    case Constructor:
        return JSValue::newFunction(mMethods[prop->mData.index]);
    default:
        return kUndefinedValue;
    }
}

bool JSType::hasProperty(Context *cx, const String &name, NamespaceList *names, Access acc, PropertyIterator *p)
{
    if (hasOwnProperty(cx, name, names, acc, p))
        return true;
    else
        if (mSuperType)
            return mSuperType->hasProperty(cx, name, names, acc, p);
        else
            return false;
}

Reference *JSType::genReference(Context *cx, bool hasBase, const String& name, NamespaceList *names, Access acc, uint32 depth)
{
    PropertyIterator i;
    if (hasOwnProperty(cx, name, names, acc, &i)) {
        Property *prop = PROPERTY(i);
        switch (prop->mFlag) {
        case FunctionPair:
            if (acc == Read)
                return new GetterFunctionReference(prop->mData.fPair.getterF, prop->mAttributes);
            else {
                JSFunction *f = prop->mData.fPair.setterF;
                return new SetterFunctionReference(f, f->getParameterType(0), prop->mAttributes);
            }
        case ValuePointer:
            return new StaticFieldReference(name, acc, this, prop->mType, prop->mAttributes);

        case IndexPair:
            if (acc == Read)
                return new GetterMethodReference(prop->mData.iPair.getterI, this, prop->mType, prop->mAttributes);
            else {
                JSFunction *f = mMethods[prop->mData.iPair.setterI];
                return new SetterMethodReference(prop->mData.iPair.setterI, this, f->getParameterType(0), prop->mAttributes);
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
        return mSuperType->genReference(cx, hasBase, name, names, acc, depth);
    return NULL;
}

// Construct a type object, hook up the prototype value (mPrototypeObject) and the __proto__ value
// (mPrototype). Special handling throughout for handling the initialization of Object_Type which has
// no super type. (Note though that it's prototype link __proto__ is the Type_Type object and it has
// a prototype object - whose __proto__ is null)
JSType::JSType(Context *cx, const StringAtom *name, JSType *super, js2val protoObj, js2val typeProto) 
            : JSInstance(cx, Type_Type),
                    mSuperType(super), 
                    mVariableCount(0),
                    mInstanceInitializer(NULL),
                    mDefaultConstructor(NULL),
                    mTypeCast(NULL),
                    mClassName(name),
                    mIsDynamic(false),
                    mUninitializedValue(kNullValue),
                    mPrototypeObject(kNullValue)
{
    if (mClassName)
        mPrivateNamespace = &cx->mWorld.identifiers[*mClassName + " private"];
    else
        mPrivateNamespace = &cx->mWorld.identifiers["unique id needed? private"]; // XXX. No, really?

    // every class gets a prototype object (used to set the __proto__ value of new instances)
    if (!JSValue::isNull(protoObj))
        mPrototypeObject = protoObj;
    else {
        JSObject *protoObj = new JSObject();
        mPrototypeObject = JSValue::newObject(protoObj);
        // and that object is prototype-linked to the super-type's prototype object
        if (mSuperType)
            protoObj->mPrototype = mSuperType->mPrototypeObject;
        protoObj->defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, this, protoObj->mPrototype);
    }

    if (mSuperType)
        defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, Property::ReadOnly | Property::DontDelete, Object_Type, mPrototypeObject);
    else  // must be Object_Type being initialized
        defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, Property::ReadOnly | Property::DontDelete, this, mPrototypeObject);

    if (!JSValue::isNull(typeProto))
        mPrototype = typeProto;
    else {
        // the __proto__ of a type is the super-type's prototype object, or for the 
        // 'class Object' type object it's the Object's prototype object
        if (mSuperType)
            mPrototype = mSuperType->mPrototypeObject;
        else // must be Object_Type being initialized
            mPrototype = mPrototypeObject;
    }
    if (mSuperType) {
        defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, Object_Type, mPrototype);
        JSValue::getObjectValue(mPrototypeObject)->defineVariable(cx, cx->Constructor_StringAtom, (NamespaceList *)NULL, 0, Object_Type, JSValue::newInstance(this));
    }
    else { // must be Object_Type being initialized
        defineVariable(cx, cx->UnderbarPrototype_StringAtom, NULL, 0, this, mPrototype);
        JSValue::getObjectValue(mPrototypeObject)->defineVariable(cx, cx->Constructor_StringAtom, (NamespaceList *)NULL, 0, this, JSValue::newInstance(this));
    }
}

// Establish the super class - connects the prototype's prototype
// and accounts for the super class's instance fields & methods
void JSType::setSuperType(JSType *super)
{
    mSuperType = super;
    if (mSuperType) {
        JSValue::getObjectValue(mPrototypeObject)->mPrototype = mSuperType->mPrototypeObject;
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

Reference *Activation::genReference(Context * cx, bool /* hasBase */, const String& name, NamespaceList *names, Access acc, uint32 depth)
{
    PropertyIterator i;
    if (hasProperty(cx, name, names, acc, &i)) {
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
            return new NameReference(name, names, acc, prop->mType, prop->mAttributes);

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
    case StmtNode::group:
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
            buildRuntimeForStmt(t->stmt);
            if (t->catches) {
                CatchClause *c = t->catches;
                while (c) {
                    buildRuntimeForStmt(c->stmt);
                    c->prop->mType = mScopeChain->extractType(c->type);
                    c = c->next;
                }
            }
            if (t->finally) buildRuntimeForStmt(t->finally);
        }
        break;
    case StmtNode::With:
    case StmtNode::If:
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
            JSFunction::ParameterFlag flag = JSFunction::RequiredParameter;
            while (v) {
                // XXX if no type is specified for the rest parameter - is it Array?
                if (v == f->function.optParameters)
                    flag = JSFunction::OptionalParameter;
                else
                    if (v == f->function.restParameter)
                        flag = JSFunction::RestParameter;
                    else
                        if (v == f->function.namedParameters)
                            flag = JSFunction::NamedParameter;
                fnc->setParameter(parameterCount++, v->name, mScopeChain->extractType(v->type), flag);
                v = v->next;
            }

            if (isOperator) {
                if (f->function.prefix != FunctionName::op) {
                    NOT_REACHED("***** Implement me -- signal an error here because the user entered an unquoted operator name");
                }
                ASSERT(f->function.name);
                const StringAtom& name = *f->function.name;
                Operator op = getOperator(parameterCount, name);
                // Operators added to the Context's operator table.
                // The indexing operators are considered unary since
                // they only dispatch on the base type.
                if ((parameterCount == 1)
                        || (op == Index)
                        || (op == IndexEqual)
                        || (op == DeleteIndex))
                    defineOperator(op, mScopeChain->topClass(), fnc);
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
                superClass = mScopeChain->findType(this, superClassExpr->name, superClassExpr->pos);
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
    case StmtNode::Package:
        {
            PackageStmtNode *ps = checked_cast<PackageStmtNode *>(p);
	    mScopeChain->addScope(ps->scope);
            buildRuntimeForStmt(ps->body);
	    mScopeChain->popScope();
        }
        break;
    default:
        break;
    }

}


static js2val Object_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val thatValue = thisValue;  // incoming 'new this' potentially supplied by constructor sequence

    if (argc != 0) {
        if (JSValue::isInstance(argv[0]) || JSValue::isObject(argv[0]) || JSValue::isFunction(argv[0]) || JSValue::isType(argv[0]))
            thatValue = argv[0];
        else
        if (JSValue::isString(argv[0]) || JSValue::isBool(argv[0]) || JSValue::isNumber(argv[0]))
            thatValue = JSValue::toObject(cx, argv[0]);
        else {
            if (JSValue::isNull(thatValue))
                thatValue = Object_Type->newInstance(cx);
        }
    }
    else {
        if (JSValue::isNull(thatValue))
            thatValue = Object_Type->newInstance(cx);
    }
    return thatValue;
}

static js2val Object_toString(Context * /* cx */, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::isObject(thisValue) || JSValue::isInstance(thisValue))
        return JSValue::newString(new String(widenCString("[object ") + *JSValue::getType(thisValue)->mClassName + widenCString("]")));
    else
    if (JSValue::isType(thisValue)) // XXX why wouldn't this get handled by the above?
        return JSValue::newString(new String(widenCString("[object ") + widenCString("Type") + widenCString("]")));
    else
    if (JSValue::isFunction(thisValue))
        return JSValue::newString(new String(widenCString("[object ") + widenCString("Function") + widenCString("]")));
    else {
        NOT_REACHED("Object.prototype.toString on non-object");
        return kUndefinedValue;
    }
}

static js2val Object_valueOf(Context * /* cx */, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    return thisValue;
}

class JSIteratorInstance : public JSInstance {
public:
    JSIteratorInstance(Context *cx) : JSInstance(cx, NULL) { mType = (JSType *)Object_Type; }
    virtual ~JSIteratorInstance() { } // keeping gcc happy
#ifdef DEBUG
    void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("JSIteratorInstance", s, t); return t; }
    void operator delete(void* t)   { trace_release("JSIteratorInstance", t); STD::free(t); }
#endif
    JSObject *obj;
    PropertyIterator it;
};

static js2val Object_forin(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    JSObject *obj = JSValue::getObjectValue(thisValue);

    JSIteratorInstance *itInst = new JSIteratorInstance(cx);    
    itInst->obj = obj;
    itInst->it = obj->mProperties.begin();
    while (true) {
        while (itInst->it == itInst->obj->mProperties.end()) {
            if (JSValue::isNull(itInst->obj->mPrototype))
                return kNullValue;
            itInst->obj = JSValue::getObjectValue(itInst->obj->mPrototype);
            itInst->it = itInst->obj->mProperties.begin();
        }
        if (PROPERTY_ATTR(itInst->it) & Property::Enumerable)
            break;
        itInst->it++;
    }

    js2val v = JSValue::newString(&PROPERTY_NAME(itInst->it));
    itInst->setProperty(cx, cx->mWorld.identifiers["value"], 0, v);
    return JSValue::newInstance(itInst);
}

static js2val Object_next(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 /*argc*/)
{
    js2val iteratorValue = argv[0];
    ASSERT(JSValue::isInstance(iteratorValue));
    JSIteratorInstance *itInst = checked_cast<JSIteratorInstance *>(JSValue::instance(iteratorValue));

    itInst->it++;
    while (true) {
        while (itInst->it == itInst->obj->mProperties.end()) {
            if (JSValue::isNull(itInst->obj->mPrototype))
                return kNullValue;
            itInst->obj = JSValue::getObjectValue(itInst->obj->mPrototype);
            itInst->it = itInst->obj->mProperties.begin();
        }
        if (PROPERTY_ATTR(itInst->it) & Property::Enumerable)
            break;
        itInst->it++;
    }
    js2val v = JSValue::newString(&PROPERTY_NAME(itInst->it));
    itInst->setProperty(cx, cx->mWorld.identifiers["value"], 0, v);
    return iteratorValue;
}

static js2val Object_done(Context *, const js2val /*thisValue*/, js2val * /*argv*/, uint32 /*argc*/)
{
    return kUndefinedValue;
}




static js2val Function_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val v = thisValue;
    if (JSValue::isNull(v))
        v = Function_Type->newInstance(cx);
    ASSERT(JSValue::isInstance(v));

    String s;
    if (argc == 0)
        s = widenCString("() { }");
    else {
        if (argc == 1)
            s = widenCString("() {") + *JSValue::string(JSValue::toString(cx, argv[0])) + "}";
        else {
            s = widenCString("(");         // ')'
            for (uint32 i = 0; i < (argc - 1); i++) {
                s += *JSValue::string(JSValue::toString(cx, argv[i]));
                if (i < (argc - 2))
                    s += widenCString(", ");
            }
/* ( */     s += ") {" + *JSValue::string(JSValue::toString(cx, argv[argc - 1])) + "}";
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
        fnc->setResultType(Object_Type);

        fnc->countParameters(cx, f->function);    


        if (cx->mScopeChain->isPossibleUncheckedFunction(f->function)) {
            fnc->setIsPrototype(true);
            fnc->setIsUnchecked();
        }
        cx->buildRuntimeForFunction(f->function, fnc);
        ByteCodeGen bcg(cx, cx->mScopeChain);
        bcg.genCodeForFunction(f->function, f->pos, fnc, false, NULL);
        cx->setReader(oldReader);
    }
    /***************************************************************/

    js2val fncPrototype = Object_Type->newInstance(cx);
    ASSERT(JSValue::isObject(fncPrototype));
    v = JSValue::newFunction(fnc);
    JSValue::object(fncPrototype)->defineVariable(cx, cx->Constructor_StringAtom, (NamespaceList *)NULL, Property::Enumerable, Object_Type, v);
    fnc->defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, Property::Enumerable, Object_Type, fncPrototype);
    return v;
}

static js2val Function_toString(Context *, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ASSERT(JSValue::getType(thisValue) == Function_Type);
    return JSValue::newString(new String(widenCString("function () { }")));
}

static js2val Function_hasInstance(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    ASSERT(argc == 1);
    js2val v = argv[0];
    if (!JSValue::isObject(v))
        return kFalseValue;

    ASSERT(JSValue::isFunction(thisValue));
    JSValue::function(thisValue)->getProperty(cx, cx->Prototype_StringAtom, CURRENT_ATTR);
    js2val p = cx->popValue();

    if (!JSValue::isObject(p))
        cx->reportError(Exception::typeError, "HasInstance: Function has non-object prototype");

    js2val V = JSValue::object(v)->mPrototype;
    while (!JSValue::isNull(V)) {
        if (JSValue::getObjectValue(V) == JSValue::object(p))
            return kTrueValue;
        V = JSValue::getObjectValue(V)->mPrototype;
    }
    return kFalseValue;
}

static js2val Function_call(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (!JSValue::isFunction(thisValue))
        cx->reportError(Exception::typeError, "Non-callable object for Function.call");

    js2val thisArg;
    if (argc == 0)
        thisArg = JSValue::newObject(cx->getGlobalObject());
    else {
        if (JSValue::isUndefined(argv[0]) || JSValue::isNull(argv[0]))
            thisArg = JSValue::newObject(cx->getGlobalObject());
        else
            thisArg = argv[0];
        --argc;
        ++argv;
    }
    return cx->invokeFunction(JSValue::function(thisValue), thisArg, argv, argc);
}

static js2val Function_apply(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (!JSValue::isFunction(thisValue))
        cx->reportError(Exception::typeError, "Non-callable object for Function.call");

    ContextStackReplacement csr(cx);

    js2val thisArg;
    if (argc == 0)
        thisArg = JSValue::newObject(cx->getGlobalObject());
    else {
        if (JSValue::isUndefined(argv[0]) || JSValue::isNull(argv[0]))
            thisArg = JSValue::newObject(cx->getGlobalObject());
        else
            thisArg = argv[0];
    }
    if (argc <= 1) {
        argv = NULL;
        argc = 0;
    }
    else {
        if (JSValue::getType(argv[1]) != Array_Type)
            cx->reportError(Exception::typeError, "Function.apply must have Array type argument list");

        ASSERT(JSValue::isObject(argv[1]));
        JSObject *argsObj = JSValue::object(argv[1]);
        argsObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
        js2val result = cx->popValue();
        argc = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));    

        argv = new js2val[argc];
        for (uint32 i = 0; i < argc; i++) {
            const String *id = numberToString(i);
            argsObj->getProperty(cx, *id, CURRENT_ATTR);
            argv[i] = cx->popValue();
            delete id;
        }
    }

    return cx->invokeFunction(JSValue::function(thisValue), thisArg, argv, argc);
}


static js2val Number_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val v = thisValue;
    if (JSValue::isNull(v))
        v = Number_Type->newInstance(cx);
    ASSERT(JSValue::isInstance(v));
    JSNumberInstance *numInst = checked_cast<JSNumberInstance *>(JSValue::instance(v));
    if (argc > 0)
        numInst->mValue = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    else
        numInst->mValue = 0.0;
    return v;
}

static js2val Number_TypeCast(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kPositiveZero;
    else
        return JSValue::toNumber(cx, argv[0]);
}

static js2val Number_toString(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::getType(thisValue) != Number_Type)
        cx->reportError(Exception::typeError, "Number.toString called on something other than a Number object");
    JSNumberInstance *numInst = checked_cast<JSNumberInstance *>(JSValue::instance(thisValue));
    return JSValue::newString(numberToString(numInst->mValue));
}

static js2val Number_valueOf(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::getType(thisValue) != Number_Type)
        cx->reportError(Exception::typeError, "Number.valueOf called on something other than a Number object");
    JSNumberInstance *numInst = checked_cast<JSNumberInstance *>(JSValue::instance(thisValue));
    return JSValue::newString(numberToString(numInst->mValue));
}

static js2val Integer_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val v = thisValue;
    if (JSValue::isNull(v))
        v = Integer_Type->newInstance(cx);
    ASSERT(JSValue::isInstance(v));
    JSNumberInstance *numInst = checked_cast<JSNumberInstance *>(JSValue::instance(v));
    if (argc > 0) {
        float64 d = JSValue::f64(JSValue::toNumber(cx, argv[0]));
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        d = neg ? -d : d;
        numInst->mValue = d;
    }
    else
        numInst->mValue = 0.0;
    return v;
}

static js2val Integer_toString(Context *, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    JSNumberInstance *numInst = checked_cast<JSNumberInstance *>(JSValue::instance(thisValue));
    return JSValue::newString(numberToString(numInst->mValue));
}



static js2val Boolean_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val v = thisValue;
    if (JSValue::isNull(v))
        v = Boolean_Type->newInstance(cx);
    ASSERT(JSValue::isInstance(v));
    JSBooleanInstance *thisObj = checked_cast<JSBooleanInstance *>(JSValue::instance(v));
    if (argc > 0)
        thisObj->mValue = JSValue::boolean(JSValue::toBoolean(cx, argv[0]));
    else
        thisObj->mValue = false;
    return v;
}

static js2val Boolean_TypeCast(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kFalseValue;
    else
        return JSValue::boolean(JSValue::toBoolean(cx, argv[0]));
}

static js2val Boolean_toString(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::getType(thisValue) != Boolean_Type)
        cx->reportError(Exception::typeError, "Boolean.toString can only be applied to Boolean objects");
    ASSERT(JSValue::isInstance(thisValue));
    JSBooleanInstance *thisObj = checked_cast<JSBooleanInstance *>(JSValue::instance(thisValue));
    if (thisObj->mValue)
        return JSValue::newString(&cx->True_StringAtom);
    else
        return JSValue::newString(&cx->False_StringAtom);
}

static js2val Boolean_valueOf(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::getType(thisValue) != Boolean_Type)
        cx->reportError(Exception::typeError, "Boolean.valueOf can only be applied to Boolean objects");
    ASSERT(JSValue::isInstance(thisValue));
    JSBooleanInstance *thisObj = checked_cast<JSBooleanInstance *>(JSValue::instance(thisValue));
    if (thisObj->mValue)
        return kTrueValue;
    else
        return kFalseValue;
}

static js2val GenericError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc, JSType *errType)
{
    js2val v = thisValue;
    if (JSValue::isNull(v))
        v = errType->newInstance(cx);
    ASSERT(JSValue::isInstance(v));
    JSObject *thisInst = JSValue::instance(v);
    js2val msg;
    if (argc > 0)
        msg = JSValue::toString(cx, argv[0]);
    else
        msg = JSValue::newString(&cx->Empty_StringAtom);
    thisInst->defineVariable(cx, cx->Message_StringAtom, NULL, Property::NoAttribute, String_Type, msg);
    thisInst->defineVariable(cx, cx->Name_StringAtom, NULL, Property::NoAttribute, String_Type, JSValue::newString(errType->mClassName));
    return v;
}

js2val RegExp_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val thatValue = thisValue;
    if (JSValue::isNull(thatValue))
        thatValue = RegExp_Type->newInstance(cx);
    ASSERT(JSValue::isInstance(thatValue));
    JSRegExpInstance *thisInst = checked_cast<JSRegExpInstance *>(JSValue::instance(thatValue));
    REuint32 flags = 0;

    const String *regexpStr = &cx->Empty_StringAtom;
    const String *flagStr = &cx->Empty_StringAtom;
    if (argc > 0) {
        if (JSValue::getType(argv[0]) == RegExp_Type) {
            ASSERT(JSValue::isInstance(argv[0]));
            if ((argc == 1) || JSValue::isUndefined(argv[1])) {
                ContextStackReplacement csr(cx);
                JSValue::instance(argv[0])->getProperty(cx, cx->Source_StringAtom, CURRENT_ATTR);
                js2val src = cx->popValue();
                ASSERT(JSValue::isString(src));
                regexpStr = JSValue::string(src);
                REState *other = (checked_cast<JSRegExpInstance *>(JSValue::instance(argv[0])))->mRegExp;
                flags = other->flags;
            }
            else
                cx->reportError(Exception::typeError, "Illegal RegExp constructor args");
        }
        else
            regexpStr = JSValue::string(JSValue::toString(cx, argv[0]));
        if ((argc > 1) && !JSValue::isUndefined(argv[1])) {
            flagStr = JSValue::string(JSValue::toString(cx, argv[1]));
            if (parseFlags(flagStr->begin(), (int32)flagStr->length(), &flags) != RE_NO_ERROR) {
                cx->reportError(Exception::syntaxError, "Failed to parse RegExp : '{0}'", *regexpStr + "/" + *flagStr);  // XXX error message?
            }
        }
    }
    REState *pState = REParse(regexpStr->begin(), (int32)regexpStr->length(), flags, RE_VERSION_1);
    if (pState) {
        thisInst->mRegExp = pState;
// XXX ECMA spec says these are DONTENUM, but SpiderMonkey and test suite disagree
/*
        thisInst->defineVariable(cx, cx->Source_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly), String_Type, JSValue(regexpStr));
        thisInst->defineVariable(cx, cx->Global_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly), Boolean_Type, (pState->flags & GLOBAL) ? kTrueValue : kFalseValue);
        thisInst->defineVariable(cx, cx->IgnoreCase_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly), Boolean_Type, (pState->flags & IGNORECASE) ? kTrueValue : kFalseValue);
        thisInst->defineVariable(cx, cx->Multiline_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly), Boolean_Type, (pState->flags & MULTILINE) ? kTrueValue : kFalseValue);
        thisInst->defineVariable(cx, cx->LastIndex_StringAtom, NULL, Property::DontDelete, Number_Type, kPositiveZero);
*/
        thisInst->defineVariable(cx, cx->Source_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly | Property::Enumerable), String_Type, JSValue::newString(regexpStr));
        thisInst->defineVariable(cx, cx->Global_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly | Property::Enumerable), Boolean_Type, (pState->flags & RE_GLOBAL) ? kTrueValue : kFalseValue);
        thisInst->defineVariable(cx, cx->IgnoreCase_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly | Property::Enumerable), Boolean_Type, (pState->flags & RE_IGNORECASE) ? kTrueValue : kFalseValue);
        thisInst->defineVariable(cx, cx->Multiline_StringAtom, NULL, (Property::DontDelete | Property::ReadOnly | Property::Enumerable), Boolean_Type, (pState->flags & RE_MULTILINE) ? kTrueValue : kFalseValue);
        thisInst->defineVariable(cx, cx->LastIndex_StringAtom, NULL, (Property::DontDelete | Property::Enumerable), Number_Type, kPositiveZero);
    }
    else {
        cx->reportError(Exception::syntaxError, "Failed to parse RegExp : '{0}'", "/" + *regexpStr + "/" + *flagStr);  // XXX error message?
    }
    return thatValue;
}

static js2val RegExp_TypeCast(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (argc > 0) {
	if ((JSValue::getType(argv[0]) == RegExp_Type)
		&& ((argc == 1) || JSValue::isUndefined(argv[1])))
	    return argv[0];
    }
    return RegExp_Constructor(cx, thisValue, argv, argc);
}

static js2val RegExp_toString(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ContextStackReplacement csr(cx);
    if (JSValue::getType(thisValue) != RegExp_Type)
        cx->reportError(Exception::typeError, "RegExp.toString can only be applied to RegExp objects");
    ASSERT(JSValue::isInstance(thisValue));
    JSRegExpInstance *thisInst = checked_cast<JSRegExpInstance *>(JSValue::instance(thisValue));
    thisInst->getProperty(cx, cx->Source_StringAtom, CURRENT_ATTR);
    js2val src = cx->popValue();
    
    // XXX not ever expecting this except in the one case of RegExp.prototype, which isn't a fully formed
    // RegExp instance, but has the appropriate type pointer.
    if (JSValue::isUndefined(src)) {
        ASSERT(thisInst == JSValue::instance(RegExp_Type->mPrototypeObject));
        return JSValue::newString(&cx->Empty_StringAtom);
    }

    String *result = new String("/" + *JSValue::string(JSValue::toString(cx, src)) + "/");
    REState *state = (REState *)thisInst->mRegExp;
    if (state->flags & RE_GLOBAL) *result += "g";
    if (state->flags & RE_IGNORECASE) *result += "i";
    if (state->flags & RE_MULTILINE) *result += "m";

    return JSValue::newString(result);
}

js2val RegExp_exec(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (JSValue::getType(thisValue) != RegExp_Type)
        cx->reportError(Exception::typeError, "RegExp.exec can only be applied to RegExp objects");
    ASSERT(JSValue::isInstance(thisValue));
    JSRegExpInstance *thisInst = checked_cast<JSRegExpInstance *>(JSValue::instance(thisValue));
   
    js2val result = kNullValue;
    if (argc > 0) {
        ContextStackReplacement csr(cx);
        int32 index = 0;

        const String *str = JSValue::string(JSValue::toString(cx, argv[0]));

        RegExp_Type->getProperty(cx, cx->Multiline_StringAtom, CURRENT_ATTR);
        js2val globalMultiline = cx->popValue();

        if (thisInst->mRegExp->flags & RE_GLOBAL) {
            // XXX implement lastIndex as a setter/getter pair instead ???
            thisInst->getProperty(cx, cx->LastIndex_StringAtom, CURRENT_ATTR);
            js2val lastIndex = cx->popValue();
            index = (int32)JSValue::f64(JSValue::toInteger(cx, lastIndex));            
        }

        REMatchState *match = REExecute(thisInst->mRegExp, str->begin(), index, (int32)str->length(), JSValue::boolean(JSValue::toBoolean(cx, globalMultiline)));
        if (match) {
            result = Array_Type->newInstance(cx);
            String *matchStr = new String(str->substr((uint32)match->startIndex, (uint32)match->endIndex - match->startIndex));
            JSValue::instance(result)->setProperty(cx, *numberToString(0), NULL, JSValue::newString(matchStr));
            String *parenStr = &cx->Empty_StringAtom;
            for (int32 i = 0; i < match->parenCount; i++) {
                if (match->parens[i].index != -1) {
                    String *parenStr = new String(str->substr((uint32)(match->parens[i].index), (uint32)(match->parens[i].length)));
                    JSValue::instance(result)->setProperty(cx, *numberToString(i + 1), NULL, JSValue::newString(parenStr));
                }
                else
                    JSValue::instance(result)->setProperty(cx, *numberToString(i + 1), NULL, kUndefinedValue);
            }
            // XXX SpiderMonkey also adds 'index' and 'input' properties to the result
            JSValue::instance(result)->setProperty(cx, cx->Index_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)(match->startIndex)));
            JSValue::instance(result)->setProperty(cx, cx->Input_StringAtom, CURRENT_ATTR, JSValue::newString(str));

            // XXX Set up the SpiderMonkey 'RegExp statics'
            RegExp_Type->setProperty(cx, cx->LastMatch_StringAtom, CURRENT_ATTR, JSValue::newString(matchStr));
            RegExp_Type->setProperty(cx, cx->LastParen_StringAtom, CURRENT_ATTR, JSValue::newString(parenStr));            
            String *contextStr = new String(str->substr(0, (uint32)match->startIndex));
            RegExp_Type->setProperty(cx, cx->LeftContext_StringAtom, CURRENT_ATTR, JSValue::newString(contextStr));
            contextStr = new String(str->substr((uint32)match->endIndex, (uint32)str->length() - match->endIndex));
            RegExp_Type->setProperty(cx, cx->RightContext_StringAtom, CURRENT_ATTR, JSValue::newString(contextStr));

            if (thisInst->mRegExp->flags & RE_GLOBAL) {
                index = match->endIndex;
                thisInst->setProperty(cx, cx->LastIndex_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)index));
            }
            free(match);
        }

    }
    return result;
}

static js2val RegExp_test(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (JSValue::getType(thisValue) != RegExp_Type)
        cx->reportError(Exception::typeError, "RegExp.test can only be applied to RegExp objects");
    ASSERT(JSValue::isInstance(thisValue));
    JSRegExpInstance *thisInst = checked_cast<JSRegExpInstance *>(JSValue::instance(thisValue));
    if (argc > 0) {
        ContextStackReplacement csr(cx);
        const String *str = JSValue::string(JSValue::toString(cx, argv[0]));
        RegExp_Type->getProperty(cx, cx->Multiline_StringAtom, CURRENT_ATTR);
        js2val globalMultiline = cx->popValue();
        REMatchState *match = REExecute(thisInst->mRegExp, str->begin(), 0, (int32)str->length(), JSValue::boolean(JSValue::toBoolean(cx, globalMultiline)));
        if (match) {
            free(match);
            return kTrueValue;
        }
    }
    return kFalseValue;
}


js2val Error_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, Error_Type);
}

static js2val Error_toString(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ContextStackReplacement csr(cx);

    if ((JSValue::getType(thisValue) != Error_Type) && !JSValue::getType(thisValue)->derivesFrom(Error_Type))
        cx->reportError(Exception::typeError, "Error.toString can only be applied to Error objects");
    ASSERT(JSValue::isInstance(thisValue));
    JSInstance *thisInstance = JSValue::instance(thisValue);
    thisInstance->getProperty(cx, cx->Message_StringAtom, CURRENT_ATTR);
    js2val msg = cx->popValue();
    thisInstance->getProperty(cx, cx->Name_StringAtom, CURRENT_ATTR);
    js2val name = cx->popValue();

    String *result = new String(*JSValue::string(JSValue::toString(cx, name)) + ":" + *JSValue::string(JSValue::toString(cx, msg)));
    return JSValue::newString(result);
}

js2val EvalError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, EvalError_Type);
}

js2val RangeError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, RangeError_Type);
}

js2val ReferenceError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, ReferenceError_Type);
}

js2val SyntaxError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, SyntaxError_Type);
}

js2val TypeError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, TypeError_Type);
}

js2val UriError_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    return GenericError_Constructor(cx, thisValue, argv, argc, UriError_Type);
}


static js2val ExtendAttribute_Invoke(Context * /*cx*/, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    ASSERT(argc == 1);

    Attribute *x = new Attribute(Property::Extend, Property::Extend | Property::Virtual);
    ASSERT(JSValue::isType(argv[0]));
    x->mExtendArgument = (JSType *)(JSValue::type(argv[0]));

    return JSValue::newAttribute(x);
}

static js2val GlobalObject_Eval(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc > 0) {
        if (!JSValue::isString(argv[0]))
            return argv[0];
        const String *sourceStr = JSValue::string(JSValue::toString(cx, argv[0]));

        Activation *prev = cx->mActivationStack.top();

        return cx->readEvalString(*sourceStr, widenCString("eval source"), cx->mScopeChain, prev->mThis);
    }
    return kUndefinedValue;
}

static js2val GlobalObject_ParseInt(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    ASSERT(argc >= 1);
    int radix = 0;      // default for stringToInteger
    if (argc == 2)
        radix = (int)JSValue::f64(JSValue::toInt32(cx, argv[1]));
    if ((radix < 0) || (radix > 36))
        return kNaNValue;

    const String *string = JSValue::string(JSValue::toString(cx, argv[0]));
    const char16 *numEnd;
    const char16 *sBegin = string->begin();
    float64 f = 0.0;
    if (sBegin)
        f = stringToInteger(sBegin, string->end(), numEnd, (uint)radix);
    return JSValue::newNumber(f);
}

static js2val GlobalObject_ParseFloat(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    ASSERT(argc == 1);

    const String *string = JSValue::string(JSValue::toString(cx, argv[0]));
    const char16 *numEnd;
    const char16 *sBegin = string->begin();
    float64 f = 0.0;
    if (sBegin)
        f = stringToDouble(sBegin, string->end(), numEnd);
    return JSValue::newNumber(f);
}

static js2val GlobalObject_isNaN(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 /*argc*/)
{
    float64 f = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    if (JSDOUBLE_IS_NaN(f))
        return kTrueValue;
    else
        return kFalseValue;
}

static js2val GlobalObject_isFinite(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 /*argc*/)
{
    float64 f = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    if (JSDOUBLE_IS_FINITE(f))
        return kTrueValue;
    else
        return kFalseValue;
}

/*
 * Stuff to emulate the old libmocha escape, which took a second argument
 * giving the type of escape to perform.  Retained for compatibility, and
 * copied here to avoid reliance on net.h, mkparse.c/NET_EscapeBytes.
 */
#define URL_XALPHAS     ((uint8) 1)
#define URL_XPALPHAS    ((uint8) 2)
#define URL_PATH        ((uint8) 4)

static const uint8 urlCharType[256] =
/*      Bit 0           xalpha          -- the alphas
 *      Bit 1           xpalpha         -- as xalpha but
 *                             converts spaces to plus and plus to %20
 *      Bit 2 ...       path            -- as xalphas but doesn't escape '/'
 */
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 0x */
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 1x */
         0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,       /* 2x   !"#$%&'()*+,-./  */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,       /* 3x  0123456789:;<=>?  */
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 4x  @ABCDEFGHIJKLMNO  */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,       /* 5X  PQRSTUVWXYZ[\]^_  */
         0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 6x  `abcdefghijklmno  */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,       /* 7X  pqrstuvwxyz{\}~  DEL */
         0, };

/* This matches the ECMA escape set when mask is 7 (default.) */

#define IS_OK(C, mask) (urlCharType[((uint8) (C))] & (mask))

/* See ECMA-262 15.1.2.4. */
static js2val GlobalObject_escape(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc > 0) {
        uint32 i, ni, length, newlength;
        char16 ch;

        const char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

        uint32 mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
        if (argc > 1) {
            float64 d = JSValue::f64(JSValue::toNumber(cx, argv[1]));
            if (!JSDOUBLE_IS_FINITE(d) || (mask = (uint32)d) != d || mask & ~(URL_XALPHAS | URL_XPALPHAS | URL_PATH))
                cx->reportError(Exception::runtimeError, "Bad string mask for escape");
        }

        const String *str = JSValue::string(JSValue::toString(cx, argv[0]));

        const char16 *chars = str->begin();
        length = newlength = str->size();

        /* Take a first pass and see how big the result string will need to be. */
        for (i = 0; i < length; i++) {
            if ((ch = chars[i]) < 128 && IS_OK(ch, mask))
                continue;
            if (ch < 256) {
                if (mask == URL_XPALPHAS && ch == ' ')
                    continue;   /* The character will be encoded as '+' */
                newlength += 2; /* The character will be encoded as %XX */
            } else {
                newlength += 5; /* The character will be encoded as %uXXXX */
            }
        }

        char16 *newchars = new char16[newlength + 1];
        for (i = 0, ni = 0; i < length; i++) {
            if ((ch = chars[i]) < 128 && IS_OK(ch, mask)) {
                newchars[ni++] = ch;
            } else if (ch < 256) {
                if (mask == URL_XPALPHAS && ch == ' ') {
                    newchars[ni++] = '+'; /* convert spaces to pluses */
                } else {
                    newchars[ni++] = '%';
                    newchars[ni++] = digits[ch >> 4];
                    newchars[ni++] = digits[ch & 0xF];
                }
            } else {
                newchars[ni++] = '%';
                newchars[ni++] = 'u';
                newchars[ni++] = digits[ch >> 12];
                newchars[ni++] = digits[(ch & 0xF00) >> 8];
                newchars[ni++] = digits[(ch & 0xF0) >> 4];
                newchars[ni++] = digits[ch & 0xF];
            }
        }
        ASSERT(ni == newlength);
        newchars[newlength] = 0;

        String *result = new String(newchars, newlength);
        delete[] newchars;
        return JSValue::newString(result);
    }
    return JSValue::newString(&cx->Undefined_StringAtom);
}
#undef IS_OK

/* See ECMA-262 15.1.2.5 */
static js2val GlobalObject_unescape(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc > 0) {
        uint32 i, ni;
        char16 ch;

        const String *str = JSValue::string(JSValue::toString(cx, argv[0]));

        const char16 *chars = str->begin();
        uint32 length = str->size();

        /* Don't bother allocating less space for the new string. */
        char16 *newchars = new char16[length + 1];
        ni = i = 0;
        while (i < length) {
            uint hexValue[4];
            ch = chars[i++];
            if (ch == '%') {
                if (i + 1 < length &&
                    isASCIIHexDigit(chars[i], hexValue[0]) && isASCIIHexDigit(chars[i + 1], hexValue[1]))
                {
                    ch = static_cast<char16>(hexValue[0] * 16 + hexValue[1]);
                    i += 2;
                } else if (i + 4 < length && chars[i] == 'u' &&
                           isASCIIHexDigit(chars[i + 1], hexValue[0]) && isASCIIHexDigit(chars[i + 2], hexValue[1]) &&
                           isASCIIHexDigit(chars[i + 3], hexValue[2]) && isASCIIHexDigit(chars[i + 4], hexValue[3]))
                {
                    ch = static_cast<char16>((((((hexValue[0] << 4)
                            + hexValue[1]) << 4)
                          + hexValue[2]) << 4)
                        + hexValue[3]);
                    i += 5;
                }
            }
            newchars[ni++] = ch;
        }
        newchars[ni] = 0;

        String *result = new String(newchars, ni);
        delete[] newchars;
        return JSValue::newString(result);
    }
    return JSValue::newString(&cx->Undefined_StringAtom);
}


JSFunction::JSFunction(Context *cx, JSType *resultType, ScopeChain *scopeChain) 
            : JSInstance(cx, Function_Type), 
                mParameterBarrel(NULL),
                mActivation(),
                mByteCode(NULL), 
                mNativeCode(NULL), 
                mResultType(resultType),
                mRequiredParameters(0),
                mOptionalParameters(0),
                mNamedParameters(0),
                mParameters(NULL),
                mScopeChain(NULL), 
                mIsPrototype(false),
                mIsConstructor(false),
                mIsChecked(true),
                mHasRestParameter(false),
                mClass(NULL),
                mFunctionName(NULL),
                mDispatch(NULL)
{
    if (scopeChain) {
        mScopeChain = new ScopeChain(*scopeChain);
    }
    if (Function_Type)    // protect against bootstrap
        mPrototype = Function_Type->mPrototypeObject;
    mActivation.mContainer = this;
    defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, 0, Object_Type, Object_Type->newInstance(cx));
    if (!JSValue::isNull(mPrototype))
        defineVariable(cx, cx->UnderbarPrototype_StringAtom, (NamespaceList *)NULL, 0, Object_Type, mPrototype);
}

JSFunction::JSFunction(Context *cx, NativeCode *code, JSType *resultType, NativeDispatch *dispatch) 
            : JSInstance(cx, Function_Type), 
                mParameterBarrel(NULL),
                mActivation(),
                mByteCode(NULL), 
                mNativeCode(code), 
                mResultType(resultType), 
                mRequiredParameters(0),
                mOptionalParameters(0),
                mNamedParameters(0),
                mParameters(NULL),
                mScopeChain(NULL),
                mIsPrototype(false),
                mIsConstructor(false),
                mIsChecked(false),          // native functions aren't checked (?)
                mHasRestParameter(false),
                mClass(NULL),
                mFunctionName(NULL),
                mDispatch(dispatch)
{
    if (Function_Type)    // protect against bootstrap
        mPrototype = Function_Type->mPrototypeObject;
    mActivation.mContainer = this;
    defineVariable(cx, cx->Prototype_StringAtom, (NamespaceList *)NULL, 0, Object_Type, Object_Type->newInstance(cx));
    defineVariable(cx, cx->UnderbarPrototype_StringAtom, (NamespaceList *)NULL, 0, Object_Type, mPrototype);
}
              
js2val JSFunction::runParameterInitializer(Context *cx, uint32 a, const js2val thisValue, js2val *argv, uint32 argc)
{
    ASSERT(mParameters && (a < maxParameterIndex()));
    return cx->interpret(getByteCode(), (int32)mParameters[a].mInitializer, getScopeChain(), thisValue, argv, argc);
}

void JSFunction::setParameterCounts(Context *cx, uint32 r, uint32 o, uint32 n, bool hasRest)
{
    mHasRestParameter = hasRest;
    mRequiredParameters = r;
    mOptionalParameters = o;
    mNamedParameters = n;
    mParameters = new ParameterData[mRequiredParameters + mOptionalParameters + + mNamedParameters + ((hasRest) ? 1 : 0)];
    defineVariable(cx, cx->Length_StringAtom, (NamespaceList *)NULL, Property::DontDelete | Property::ReadOnly, Number_Type, JSValue::newNumber((float64)mRequiredParameters)); 
}

js2val JSFunction::invokeNativeCode(Context *cx, const js2val thisValue, js2val argv[], uint32 argc)
{
    if (mDispatch)
        return (mDispatch)(mNativeCode, cx, thisValue, argv, argc);
    else
        return mNativeCode(cx, thisValue, argv, argc);
}


void Context::assureStackSpace(uint32 s)
{
    if ((mStackMax - mStackTop) < s) {
        js2val *newStack = new js2val[mStackMax + s];
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
    JSFunction *constructor = new JSFunction(this, cdef->defCon, Object_Type, NULL);
    constructor->setClass(type);
    constructor->setFunctionName(type->mClassName);
    type->setDefaultConstructor(this, constructor);

    // the prototype functions are defined in the prototype object...
    if (pdef) {
        for (uint32 i = 0; i < pdef->mCount; i++) {
            JSFunction *fun = new JSFunction(this, pdef->mDef[i].imp, pdef->mDef[i].result, NULL);
//            fun->setClass(type); don't do this, it makes the function a method
            StringAtom *name = &mWorld.identifiers[widenCString(pdef->mDef[i].name)];
            fun->setFunctionName(name);
            fun->setParameterCounts(this, pdef->mDef[i].length, 0, 0, false);
            JSValue::getObjectValue(type->mPrototypeObject)->defineVariable(this, *name,
                                                                       (NamespaceList *)(NULL), 
                                                                       Property::NoAttribute,
                                                                       pdef->mDef[i].result, 
                                                                       JSValue::newFunction(fun));
        }
    }
    type->completeClass(this, mScopeChain);
    type->setStaticInitializer(this, NULL);
    type->mUninitializedValue = *cdef->uninit;
    getGlobalObject()->defineVariable(this, widenCString(cdef->name), (NamespaceList *)(NULL), Property::NoAttribute, Type_Type, JSValue::newType(type));
    mScopeChain->popScope();
    if (pdef) delete pdef;
}

static js2val arrayMaker(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    ASSERT(argc == 2);
    ASSERT(JSValue::isType(argv[0]));
    JSType *baseType = JSValue::type(argv[0]);

    if ((baseType == Array_Type) && JSValue::isType(argv[1])) {
        JSType *elementType = JSValue::type(argv[1]);
        JSType *result = new JSArrayType(cx, elementType, NULL, Object_Type, kNullValue, kNullValue);
        result->setDefaultConstructor(cx, Array_Type->getDefaultConstructor());
        return JSValue::newType(result);
    }
    else {
        // then it's just <type>[] - an element reference
        JSValue::type(argv[0])->getProperty(cx, *JSValue::string(JSValue::toString(cx, argv[1])), NULL);
        return cx->popValue();
    }

}

void Context::initBuiltins()
{
    ClassDef builtInClasses[] =
    {
        { "Object",         Object_Constructor,         &kUndefinedValue     },
        { "Type",           NULL,                       &kNullValue          },
        { "Function",       Function_Constructor,       &kNullValue          },
        { "Number",         Number_Constructor,         &kPositiveZero       },
        { "Integer",        Integer_Constructor,        &kPositiveZero       },
        { "String",         String_Constructor,         &kNullValue          },
        { "Array",          Array_Constructor,          &kNullValue          },
        { "Boolean",        Boolean_Constructor,        &kFalseValue         },
        { "Void",           NULL,                       &kUndefinedValue     },
        { "Unit",           NULL,                       &kNullValue          },
        { "Attribute",      NULL,                       &kNullValue          },
        { "NamedArgument",  NULL,                       &kNullValue          },
        { "Date",           Date_Constructor,           &kPositiveZero       },
        { "Null",           NULL,                       &kNullValue          },
        { "Error",          Error_Constructor,          &kNullValue          },
        { "EvalError",      EvalError_Constructor,      &kNullValue          },
        { "RangeError",     RangeError_Constructor,     &kNullValue          },
        { "ReferenceError", ReferenceError_Constructor, &kNullValue          },
        { "SyntaxError",    SyntaxError_Constructor,    &kNullValue          },
        { "TypeError",      TypeError_Constructor,      &kNullValue          },
        { "UriError",       UriError_Constructor,       &kNullValue          },
        { "RegExp",         RegExp_Constructor,         &kNullValue          },

        { "Package",        NULL,                       &kNullValue          },

    };

    Object_Type  = new JSObjectType(this, &mWorld.identifiers[widenCString(builtInClasses[0].name)], NULL, kNullValue, kNullValue);
//    Object_Type->mPrototype->mType = Object_Type;
    Object_Type->mIsDynamic = true;                     // XXX aren't all the built-ins thus?

    Type_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[1].name)], Object_Type, kNullValue, kNullValue);
    Object_Type->mType  = Type_Type;
    
    // 
    // ECMA 1.5 says Function.prototype is a Function object, it's [[class]] property is "Function"
    // ECMA 2.0 says Function.prototype is an Object NOT an instance of the class
    // here we sort of manage that by having a JSFunction object as the prototype object, not a JSFunctionInstance
    // (which we don't actually have, hmm).
    // For String, etc. this same issue needs to be finessed

    js2val protoVal;

    JSFunction *funProto = new JSFunction(this, Object_Type, NULL);
    funProto->mPrototype = Object_Type->mPrototypeObject;
    funProto->defineVariable(this, UnderbarPrototype_StringAtom, NULL, 0, Object_Type, funProto->mPrototype);
    protoVal            = JSValue::newFunction(funProto);
    Function_Type       = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[2].name)], Object_Type, protoVal, kNullValue);
    Function_Type->mPrototype = Function_Type->mPrototypeObject;
    funProto->mType = Function_Type;

    // now we can bootstrap the Object prototype (__proto__) to be the Function prototype
    Object_Type->mPrototype = Function_Type->mPrototypeObject;
    Object_Type->setProperty(this, UnderbarPrototype_StringAtom, (NamespaceList *)NULL, Object_Type->mPrototype);

    
    JSNumberInstance *numProto  = new JSNumberInstance(this);
    protoVal            = JSValue::newInstance(numProto);
    Number_Type         = new JSNumberType(this, &mWorld.identifiers[widenCString(builtInClasses[3].name)], Object_Type, protoVal, Function_Type->mPrototypeObject);
    numProto->mType     = Number_Type;

    Integer_Type        = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[4].name)], Object_Type, kNullValue, kNullValue);

    JSStringInstance *strProto  = new JSStringInstance(this);
    protoVal            = JSValue::newInstance(strProto);
    String_Type         = new JSStringType(this, &mWorld.identifiers[widenCString(builtInClasses[5].name)], Object_Type, protoVal, Function_Type->mPrototypeObject);
    strProto->mValue    = &Empty_StringAtom;
    strProto->mPrototype = Function_Type->mPrototypeObject;
    strProto->mType     = String_Type;
    
    JSArrayInstance *arrayProto = new JSArrayInstance(this);
    protoVal            = JSValue::newInstance(arrayProto);
    Array_Type          = new JSArrayType(this, Object_Type, &mWorld.identifiers[widenCString(builtInClasses[6].name)], Object_Type, protoVal, Function_Type->mPrototypeObject);
    arrayProto->mType   = Array_Type;

    JSBooleanInstance *boolProto = new JSBooleanInstance(this);
    protoVal            = JSValue::newInstance(boolProto);
    Boolean_Type        = new JSBooleanType(this, &mWorld.identifiers[widenCString(builtInClasses[7].name)], Object_Type, protoVal, Function_Type->mPrototypeObject);
    boolProto->mValue   = false;
    boolProto->mType    = Boolean_Type;

    JSDateInstance *dateProto = new JSDateInstance(this);
    protoVal            = JSValue::newInstance(dateProto);
    Date_Type           = new JSDateType(this, &mWorld.identifiers[widenCString(builtInClasses[12].name)], Object_Type, protoVal, Function_Type->mPrototypeObject);
    dateProto->mType    = Date_Type;
    
    Void_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[8].name)], Object_Type, kNullValue, kNullValue);
    Unit_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[9].name)], Object_Type, kNullValue, kNullValue);
    Attribute_Type      = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[10].name)], Object_Type, kNullValue, kNullValue);
    NamedArgument_Type  = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[11].name)], Object_Type, kNullValue, kNullValue);
    Null_Type           = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[13].name)], Object_Type, kNullValue, kNullValue);
    Error_Type          = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[14].name)], Object_Type, kNullValue, kNullValue);
    EvalError_Type      = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[15].name)], Error_Type, kNullValue, kNullValue);
    RangeError_Type     = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[16].name)], Error_Type, kNullValue, kNullValue);
    ReferenceError_Type = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[17].name)], Error_Type, kNullValue, kNullValue);
    SyntaxError_Type    = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[18].name)], Error_Type, kNullValue, kNullValue);
    TypeError_Type      = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[19].name)], Error_Type, kNullValue, kNullValue);
    UriError_Type       = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[20].name)], Error_Type, kNullValue, kNullValue);
    
    // XXX RegExp.prototype is set to a RegExp instance, which isn't ECMA (it's supposed to be an Object instance) but
    // is SpiderMonkey compatible.
    JSRegExpInstance *regExpProto = new JSRegExpInstance(this);
    protoVal            = JSValue::newInstance(regExpProto);
    RegExp_Type         = new JSRegExpType(this, &mWorld.identifiers[widenCString(builtInClasses[21].name)], Object_Type, protoVal, Function_Type->mPrototypeObject);
    regExpProto->mPrototype = Object_Type->mPrototypeObject;
    regExpProto->mType = RegExp_Type;
    
    Package_Type        = new JSType(this, &mWorld.identifiers[widenCString(builtInClasses[22].name)], Object_Type, kNullValue, kNullValue);




    ProtoFunDef objectProtos[] = 
    {
        { "toString", String_Type, 0, Object_toString },
        { "toSource", String_Type, 0, Object_toString },
        { "forin",    Object_Type, 0, Object_forin    },
        { "next",     Object_Type, 0, Object_next     },
        { "done",     Object_Type, 0, Object_done     },
        { "valueOf",  Object_Type, 0, Object_valueOf  },
        { NULL }
    };
    ProtoFunDef functionProtos[] = 
    {
        { "toString",    String_Type, 0, Function_toString },
        { "toSource",    String_Type, 0, Function_toString },
        { "hasInstance", Boolean_Type, 1, Function_hasInstance },
        { "call",        Object_Type, 1, Function_call },
        { "apply",       Object_Type, 2, Function_apply },
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
    ProtoFunDef errorProtos[] = 
    {
        { "toString", String_Type, 0, Error_toString },
        { "toSource", String_Type, 0, Error_toString },
        { NULL }
    };
    ProtoFunDef regexpProtos[] = 
    {
        { "toString", String_Type, 0, RegExp_toString },
        { "toSource", String_Type, 0, RegExp_toString },
        { "exec",     Array_Type,  0, RegExp_exec },
        { "test",     Boolean_Type,0, RegExp_test },
        { NULL }
    };

    ASSERT(mGlobal);
    *mGlobal = JSValue::object(Object_Type->newInstance(this));
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
    initClass(Error_Type,           &builtInClasses[14], new PrototypeFunctions(&errorProtos[0]));
    initClass(EvalError_Type,       &builtInClasses[15], new PrototypeFunctions(&errorProtos[0]));  // XXX shouldn't these be inherited from prototype?
    initClass(RangeError_Type,      &builtInClasses[16], new PrototypeFunctions(&errorProtos[0]));
    initClass(ReferenceError_Type,  &builtInClasses[17], new PrototypeFunctions(&errorProtos[0]));
    initClass(SyntaxError_Type,     &builtInClasses[18], new PrototypeFunctions(&errorProtos[0]));
    initClass(TypeError_Type,       &builtInClasses[19], new PrototypeFunctions(&errorProtos[0]));
    initClass(UriError_Type,        &builtInClasses[20], new PrototypeFunctions(&errorProtos[0]));
    initClass(RegExp_Type,          &builtInClasses[21], new PrototypeFunctions(&regexpProtos[0]));

    defineOperator(Index, Type_Type, new JSFunction(this, arrayMaker, Type_Type, NULL));
    
    Object_Type->mTypeCast = new JSFunction(this, Object_Constructor, Object_Type, NULL);
    Function_Type->mTypeCast = new JSFunction(this, Function_Constructor, Object_Type, NULL);

    Number_Type->mTypeCast = new JSFunction(this, Number_TypeCast, Number_Type, NULL);

    defineOperator(Index, Array_Type, new JSFunction(this, Array_GetElement, Object_Type, NULL));
    defineOperator(IndexEqual, Array_Type, new JSFunction(this, Array_SetElement, Object_Type, NULL));
    Array_Type->mTypeCast = new JSFunction(this, Array_Constructor, Array_Type, NULL);
    Array_Type->defineVariable(this, Length_StringAtom, NULL, Property::NoAttribute, Number_Type, JSValue::newNumber(1.0));

    Boolean_Type->mTypeCast = new JSFunction(this, Boolean_TypeCast, Boolean_Type, NULL);
    Boolean_Type->defineVariable(this, Length_StringAtom, NULL, Property::NoAttribute, Number_Type, JSValue::newNumber(1.0));

    Date_Type->mTypeCast = new JSFunction(this, Date_TypeCast, String_Type, NULL);
    Date_Type->defineStaticMethod(this, widenCString("parse"), NULL, new JSFunction(this, Date_parse, Number_Type, NULL));
    Date_Type->defineStaticMethod(this, widenCString("UTC"), NULL, new JSFunction(this, Date_UTC, Number_Type, NULL));

    JSFunction *fromCharCode = new JSFunction(this, String_fromCharCode, String_Type, NULL);
    fromCharCode->defineVariable(this, Length_StringAtom, (NamespaceList *)NULL, Property::DontDelete | Property::ReadOnly, Number_Type, JSValue::newNumber(1.0)); 
    String_Type->defineVariable(this, FromCharCode_StringAtom, NULL, String_Type, JSValue::newFunction(fromCharCode));
    String_Type->mTypeCast = new JSFunction(this, String_TypeCast, String_Type, NULL);
    String_Type->defineVariable(this, Length_StringAtom, NULL, Property::NoAttribute, Number_Type, JSValue::newNumber(1.0));

    Error_Type->mTypeCast = new JSFunction(this, Error_Constructor, Error_Type, NULL);
    RegExp_Type->mTypeCast = new JSFunction(this, RegExp_TypeCast, Error_Type, NULL);

    // XXX these RegExp statics are not specified by ECMA, but implemented by SpiderMonkey
    RegExp_Type->defineVariable(this, Input_StringAtom, NULL, Property::NoAttribute, String_Type, JSValue::newString(&Empty_StringAtom));
    RegExp_Type->defineVariable(this, LastMatch_StringAtom, NULL, Property::NoAttribute, String_Type, JSValue::newString(&Empty_StringAtom));
    RegExp_Type->defineVariable(this, LastParen_StringAtom, NULL, Property::NoAttribute, String_Type, JSValue::newString(&Empty_StringAtom));
    RegExp_Type->defineVariable(this, LeftContext_StringAtom, NULL, Property::NoAttribute, String_Type, JSValue::newString(&Empty_StringAtom));
    RegExp_Type->defineVariable(this, RightContext_StringAtom, NULL, Property::NoAttribute, String_Type, JSValue::newString(&Empty_StringAtom));
    

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
      Message_StringAtom        (world.identifiers["message"]),
      Name_StringAtom           (world.identifiers["name"]),
      Error_StringAtom          (world.identifiers["Error"]),
      EvalError_StringAtom      (world.identifiers["EvalError"]),
      RangeError_StringAtom     (world.identifiers["RangeError"]),
      ReferenceError_StringAtom (world.identifiers["ReferenceError"]),
      SyntaxError_StringAtom    (world.identifiers["SyntaxError"]),
      TypeError_StringAtom      (world.identifiers["TypeError"]),
      UriError_StringAtom       (world.identifiers["UriError"]),
      Source_StringAtom         (world.identifiers["source"]),
      Global_StringAtom         (world.identifiers["global"]),
      IgnoreCase_StringAtom     (world.identifiers["ignoreCase"]),
      Multiline_StringAtom      (world.identifiers["multiline"]),
      Input_StringAtom          (world.identifiers["input"]),
      Index_StringAtom          (world.identifiers["index"]),
      LastIndex_StringAtom      (world.identifiers["lastIndex"]),
      LastMatch_StringAtom      (world.identifiers["lastMatch"]),
      LastParen_StringAtom      (world.identifiers["lastParen"]),
      LeftContext_StringAtom    (world.identifiers["leftContext"]),
      RightContext_StringAtom   (world.identifiers["rightContext"]),
      Dollar_StringAtom		(world.identifiers["$"]),
      UnderbarPrototype_StringAtom  (world.identifiers["__proto__"]),

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
      mNamespaceList(NULL),
      mLocals(NULL),
      mArgumentBase(NULL),
      mReader(NULL),
      mErrorReporter(NULL),
      argumentFormatMap(NULL),
      mGlobal(global)

{
    uint32 i;
    
    initOperatorTable();

    mScopeChain = new ScopeChain(this, mWorld);
    if (Object_Type == NULL) {                
        initBuiltins();
    }
    
    JSType *MathType = new JSType(this, &mWorld.identifiers[widenCString("Math")], Object_Type, Object_Type->mPrototypeObject, kNullValue);
    JSObject *mathObj = JSValue::instance(MathType->newInstance(this));

    getGlobalObject()->defineVariable(this, Math_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Object_Type, JSValue::newObject(mathObj));
    initMathObject(this, mathObj);
    initDateObject(this);
    
    Number_Type->defineVariable(this, widenCString("MAX_VALUE"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue::newNumber(maxValue));
    Number_Type->defineVariable(this, widenCString("MIN_VALUE"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue::newNumber(minValue));
    Number_Type->defineVariable(this, widenCString("NaN"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue::newNumber(nan));
    Number_Type->defineVariable(this, widenCString("POSITIVE_INFINITY"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue::newNumber(positiveInfinity));
    Number_Type->defineVariable(this, widenCString("NEGATIVE_INFINITY"), NULL, Property::ReadOnly | Property::DontDelete, Number_Type, JSValue::newNumber(negativeInfinity));

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
        getGlobalObject()->defineVariable(this, widenCString(attribute_init[i].name), (NamespaceList *)(NULL), Property::NoAttribute, Attribute_Type, JSValue::newAttribute(attr));
    }

    JSFunction *x = new JSFunction(this, ExtendAttribute_Invoke, Attribute_Type, NULL);
    getGlobalObject()->defineVariable(this, Extend_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Attribute_Type, JSValue::newFunction(x));


    
    
    ProtoFunDef globalObjectFunctions[] = {
        { "eval",           Object_Type,  1, GlobalObject_Eval },
        { "parseInt",       Number_Type,  2, GlobalObject_ParseInt },
        { "parseFloat",     Number_Type,  2, GlobalObject_ParseFloat },
        { "isNaN",          Boolean_Type, 2, GlobalObject_isNaN },
        { "isFinite",       Boolean_Type, 2, GlobalObject_isFinite },
        { "escape",         String_Type,  1, GlobalObject_escape },
        { "unescape",       String_Type,  1, GlobalObject_unescape },        
    };
    
    for (i = 0; i < (sizeof(globalObjectFunctions) / sizeof(ProtoFunDef)); i++) {
        x = new JSFunction(this, globalObjectFunctions[i].imp, globalObjectFunctions[i].result, NULL);
        x->setParameterCounts(this, globalObjectFunctions[i].length, 0, 0, false);
        x->setIsPrototype(true);
        getGlobalObject()->defineVariable(this, widenCString(globalObjectFunctions[i].name), (NamespaceList *)(NULL), Property::NoAttribute, globalObjectFunctions[i].result, JSValue::newFunction(x));    
    }

    getGlobalObject()->defineVariable(this, Undefined_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Void_Type, kUndefinedValue);
    getGlobalObject()->defineVariable(this, NaN_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Void_Type, kNaNValue);
    getGlobalObject()->defineVariable(this, Infinity_StringAtom, (NamespaceList *)(NULL), Property::NoAttribute, Void_Type, kPositiveInfinity);                

}


/*
    See if the specified package is already loaded - return true
    Throw an exception if the package is being loaded already
*/
bool Context::checkForPackage(const String &packageName)
{
    // XXX linear search 
    for (PackageList::iterator pi = mPackages.begin(), end = mPackages.end(); (pi != end); pi++) {
        if (PACKAGE_NAME(pi).compare(packageName) == 0) {
            if (PACKAGE_STATUS(pi) == Package::OnItsWay)
                reportError(Exception::referenceError, "Package circularity");
            else
                return true;
        }
    }
    return false;
}


/*
    Load the specified package from the file
*/
void Context::loadPackage(const String & /*packageName*/, const String &filename)
{
    // XXX need some rules for search path
    // XXX need to extract just the target package from the file
    readEvalFile(filename);
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

void JSValue::print(Formatter& f, const js2val value)
{
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        f << f64(value);
        break;
    case JS2VAL_OBJECT:
        if (isNull(value))
            f << "null";
        else if (isType(value)) {
            printFormat(f, "Type @ 0x%08X\n", JSValue::type(value));
            f << *JSValue::type(value);
        }
        else if (isFunction(value)) {
            if (!JSValue::function(value)->isNative()) {
                FunctionName *fnName = JSValue::function(value)->getFunctionName();
                if (fnName) {
                    StringFormatter s;
                    PrettyPrinter pp(s);
                    fnName->print(pp);
                    f << "function '" << s.getString() << "'\n" << *JSValue::function(value)->getByteCode();
                }
                else {
                    f << "function anonymous\n" << *JSValue::function(value)->getByteCode();
                }
            }
            else
                f << "function\n";
        }
        else {
            printFormat(f, "Object @ 0x%08X\n", object(value));
            f << *JSValue::object(value);
        }
        break;
    case JS2VAL_BOOLEAN:
        f << ((JSValue::boolean(value)) ? "true" : "false");
        break;
    case JS2VAL_STRING:
        f << *JSValue::string(value);
        break;
    case JS2VAL_INT:
        f << "undefined";
        break;
    default:
        NOT_REACHED("Bad tag");
    }
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
            js2val v = prop.mData.vp;
            f << "ValuePointer --> "; 
            if (JSValue::isObject(v))
                printFormat(f, "Object @ 0x%08X\n", JSValue::object(v));
            else
            if (JSValue::isType(v))
                printFormat(f, "Type @ 0x%08X\n", JSValue::type(v));
            else
            if (JSValue::isFunction(v))
                printFormat(f, "Function @ 0x%08X\n", JSValue::function(v));
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
        case ValuePointer : f << "Value --> " << prop->mData.vp; break;
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

