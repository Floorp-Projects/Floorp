/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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

#include <algorithm>

#include "parser.h"
#include "numerics.h"
#include "js2runtime.h"

// this is the IdentifierList passed to the name lookup routines
#define CURRENT_ATTR    (NULL)

#include "jsarray.h"

namespace JavaScript {    
namespace JS2Runtime {

js2val Array_Constructor(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val thatValue = thisValue;
    if (JSValue::isNull(thatValue))
        thatValue = Array_Type->newInstance(cx);
    ASSERT(JSValue::isInstance(thatValue));
    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thatValue));
    if (argc > 0) {
        if (argc == 1) {
            if (JSValue::isNumber(argv[0])) {
                uint32 i = (uint32)(JSValue::f64(argv[0]));
                if (i == JSValue::f64(argv[0]))
                    arrInst->mLength = i;
                else
                    cx->reportError(Exception::rangeError, "Array length too large");
            }
            else {
                arrInst->mLength = 1;
                arrInst->defineVariable(cx, widenCString("0"), (NamespaceList *)(NULL), Property::Enumerable, Object_Type, argv[0]);
            }
        }
        else {
            arrInst->mLength = argc;
            for (uint32 i = 0; i < argc; i++) {
                const String *id = numberToString(i);
                arrInst->defineVariable(cx, *id, (NamespaceList *)(NULL), Property::Enumerable, Object_Type, argv[i]);
                delete id;
            }
        }
    }
    return thatValue;
}


static js2val Array_toString(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::getType(thisValue) != Array_Type)
        cx->reportError(Exception::typeError, "Array.prototype.toString called on a non Array");

    ASSERT(JSValue::isInstance(thisValue));
    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thisValue));

    ContextStackReplacement csr(cx);

    if (arrInst->mLength == 0)
        return JSValue::newString(&cx->Empty_StringAtom);
    else {
        String *s = new String();
        for (uint32 i = 0; i < arrInst->mLength; i++) {
            const String *id = numberToString(i);
            arrInst->getProperty(cx, *id, NULL);
            js2val result = cx->popValue();
            if (!JSValue::isUndefined(result) && !JSValue::isNull(result))
                s->append(*JSValue::string(JSValue::toString(cx, result)));
            if (i < (arrInst->mLength - 1))
                s->append(widenCString(","));
            delete id;
        }
        return JSValue::newString(s);
    }
    
}

static js2val Array_toSource(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (JSValue::getType(thisValue) != Array_Type)
        cx->reportError(Exception::typeError, "Array.prototype.toSource called on a non Array");

    ASSERT(JSValue::isInstance(thisValue));
    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thisValue));

    ContextStackReplacement csr(cx);

    if (arrInst->mLength == 0)
        return JSValue::newString(new String(widenCString("[]")));
    else {
        String *s = new String(widenCString("["));
        for (uint32 i = 0; i < arrInst->mLength; i++) {
            const String *id = numberToString(i);
            arrInst->getProperty(cx, *id, NULL);
            js2val result = cx->popValue();
            if (!JSValue::isUndefined(result))
                s->append(*JSValue::string(JSValue::toString(cx, result)));
            if (i < (arrInst->mLength - 1))
                s->append(widenCString(", "));
            delete id;
        }
        s->append(widenCString("]"));
        return JSValue::newString(s);
    }
    
}

static js2val Array_push(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);
    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));    

    for (uint32 i = 0; i < argc; i++) {
        const String *id = numberToString(i + length);
        thisObj->defineVariable(cx, *id, (NamespaceList *)(NULL), Property::NoAttribute, Object_Type, argv[i]);
        delete id;
    }
    length += argc;
    result = JSValue::newNumber((float64)length);
    thisObj->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, result);
    return result;
}
              
static js2val Array_pop(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);
    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));    

    if (length > 0) {
        const String *id = numberToString(length - 1);
        thisObj->getProperty(cx, *id, NULL);
        js2val result = cx->popValue();
        thisObj->deleteProperty(cx, *id, NULL);
        --length;
        thisObj->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)length));
        delete id;
        return result;
    }
    else
        return kUndefinedValue;
}

js2val Array_concat(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val E = thisValue;

    js2val result = Array_Type->newInstance(cx);
    JSArrayInstance *A = checked_cast<JSArrayInstance *>(JSValue::instance(result));
    uint32 n = 0;
    uint32 i = 0;

    ContextStackReplacement csr(cx);

    do {
        if (JSValue::getType(E) != Array_Type) {
            const String *id = numberToString(n++);
            A->setProperty(cx, *id, CURRENT_ATTR, E);
            delete id;
        }
        else {
            JSObject *arrObj = JSValue::getObjectValue(E);
            arrObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
            js2val result = cx->popValue();
            uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));    
            for (uint32 k = 0; k < length; k++) {
                const String *id = numberToString(k);
                arrObj->getProperty(cx, *id, NULL);
                delete id;
                id = numberToString(n++);
                js2val result = cx->popValue();
                A->setProperty(cx, *id, CURRENT_ATTR, result);
                delete id;
            }
        }
        E = argv[i++];
    } while (i <= argc);
    
    return result;
}

static js2val Array_join(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);

    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));   

/* XXX ECMA says that if separator is undefined we use ',', but SpiderMonkey and
  the test suite want 'undefined' in that case, and only use the ',' when the
  separator is actually is missing
*/

    const String *separator;
    if (argc == 0)
        separator = new String(widenCString(","));
    else
        separator = JSValue::string(JSValue::toString(cx, argv[0]));

    const String *id = numberToString(0);
    thisObj->getProperty(cx, *id, CURRENT_ATTR);
    delete id;
    result = cx->popValue();

    String *S = new String();

    for (uint32 k = 0; k < length; k++) {
        id = numberToString(k);
        thisObj->getProperty(cx, *id, CURRENT_ATTR);
        result = cx->popValue();
        if (!JSValue::isUndefined(result) && !JSValue::isNull(result))
            *S += *JSValue::string(JSValue::toString(cx, result));

        if (k < (length - 1))
            *S += *separator;
        delete id;
    }
    
    return JSValue::newString(S);
}

static js2val Array_reverse(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);

    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));   

    uint32 halfway = length / 2;

    for (uint32 k = 0; k < halfway; k++) {    
        const String *id1 = numberToString(k);
        const String *id2 = numberToString(length - k - 1);

        PropertyIterator it;
        if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
            if (thisObj->hasOwnProperty(cx, *id2, CURRENT_ATTR, Read, &it)) {
                thisObj->getProperty(cx, *id1, CURRENT_ATTR);
                thisObj->getProperty(cx, *id2, CURRENT_ATTR);
                thisObj->setProperty(cx, *id1, CURRENT_ATTR, cx->popValue());
                thisObj->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
            }
            else {
                thisObj->getProperty(cx, *id1, CURRENT_ATTR);
                thisObj->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
                thisObj->deleteProperty(cx, *id1, CURRENT_ATTR);
            }
        }
        else {
            if (thisObj->hasOwnProperty(cx, *id2, CURRENT_ATTR, Read, &it)) {
                thisObj->getProperty(cx, *id2, CURRENT_ATTR);
                thisObj->setProperty(cx, *id1, CURRENT_ATTR, cx->popValue());
                thisObj->deleteProperty(cx, *id2, CURRENT_ATTR);
            }
            else {
                thisObj->deleteProperty(cx, *id1, CURRENT_ATTR);
                thisObj->deleteProperty(cx, *id2, CURRENT_ATTR);
            }
        }
        delete id1;
        delete id2;
    }

    return thisValue;
}

static js2val Array_shift(Context *cx, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);

    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));   

    if (length == 0) {
        thisObj->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, result);
        return kUndefinedValue;
    }

    const String *id = numberToString(0);
    thisObj->getProperty(cx, *id, CURRENT_ATTR);
    result = cx->popValue();
    delete id;

    for (uint32 k = 1; k < length; k++) {
        const String *id1 = numberToString(k);
        const String *id2 = numberToString(k - 1);

        PropertyIterator it;
        if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
            thisObj->getProperty(cx, *id1, CURRENT_ATTR);
            thisObj->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
        }
        else
            thisObj->deleteProperty(cx, *id2, CURRENT_ATTR);        
        delete id1;
        delete id2;
    }

    id = numberToString(length - 1);
    thisObj->deleteProperty(cx, *id, CURRENT_ATTR);
    delete id;
    thisObj->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)(length - 1)) );

    return result;
}

static js2val Array_slice(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);

    js2val result = Array_Type->newInstance(cx);
    JSArrayInstance *A = checked_cast<JSArrayInstance *>(JSValue::instance(result));

    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val lengthValue = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, lengthValue));   

    uint32 start, end;
    if (argc < 1) 
        start = 0;
    else {
        int32 arg0 = (int32)JSValue::f64(JSValue::toInt32(cx, argv[0]));
        if (arg0 < 0) {
            arg0 += length; 
            if (arg0 < 0)
                start = 0;
            else
                start = toUInt32(arg0);
        }
        else {
            if (toUInt32(arg0) >= length)    // cast ok since > 0
                start = length;
            else
                start = toUInt32(arg0);
        }
    }

    if (argc < 2) 
        end = length;
    else {
        int32 arg1 = (int32)JSValue::f64(JSValue::toInt32(cx, argv[1]));
        if (arg1 < 0) {
            arg1 += length;
            if (arg1 < 0)
                end = 0;
            else
                end = toUInt32(arg1);
        }
        else {
            if (toUInt32(arg1) >= length)
                end = length;
            else
                end = toUInt32(arg1);
        }
    }
    
    uint32 n = 0;
    while (start < end) {
        const String *id1 = numberToString(start);
        PropertyIterator it;
        if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
            const String *id2 = numberToString(n);
            thisObj->getProperty(cx, *id1, CURRENT_ATTR);
            A->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
        }
        n++;
        start++;
        delete id1;
    }
    A->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)n) );
    return JSValue::newInstance(A);
}

typedef struct CompareArgs {
    Context     *context;
    JSFunction  *target;
} CompareArgs;

typedef struct QSortArgs {
    js2val      *vec;
    js2val      *pivot;
    CompareArgs  *arg;
} QSortArgs;

static int32 sort_compare(js2val *a, js2val *b, CompareArgs *arg);

static void
js_qsort_r(QSortArgs *qa, int lo, int hi)
{
    js2val *pivot, *vec, *a, *b;
    int i, j, lohi, hilo;

    CompareArgs *arg;

    pivot = qa->pivot;
    vec = qa->vec;
    arg = qa->arg;

    while (lo < hi) {
        i = lo;
        j = hi;
        a = vec + i;
        *pivot = *a;
        while (i < j) {
            b = vec + j;
            if (sort_compare(b, pivot, arg) >= 0) {
                j--;
                continue;
            }
            *a = *b;
            while (sort_compare(a, pivot, arg) <= 0) {
                i++;
                a = vec + i;
                if (i == j)
                    goto store_pivot;
            }
            *b = *a;
        }
        if (i > lo) {
      store_pivot:
            *a = *pivot;
        }
        if (i - lo < hi - i) {
            lohi = i - 1;
            if (lo < lohi)
                js_qsort_r(qa, lo, lohi);
            lo = i + 1;
        } else {
            hilo = i + 1;
            if (hilo < hi)
                js_qsort_r(qa, hilo, hi);
            hi = i - 1;
        }
    }
}

static void js_qsort(js2val *vec, size_t nel, CompareArgs *arg)
{
    js2val *pivot;
    QSortArgs qa;

    pivot = (js2val *)STD::malloc(nel);
    qa.vec = vec;
    qa.pivot = pivot;
    qa.arg = arg;
    js_qsort_r(&qa, 0, (int)(nel - 1));
    STD::free(pivot);
}

static int32 sort_compare(js2val *a, js2val *b, CompareArgs *arg)
{
    js2val av = *(const js2val *)a;
    js2val bv = *(const js2val *)b;
    CompareArgs *ca = (CompareArgs *) arg;
    Context *cx = ca->context;
    int32 result;

    if (ca->target == NULL) {
        if (JSValue::isUndefined(av) || JSValue::isUndefined(bv)) {
	    /* Put undefined properties at the end. */
	    result = (JSValue::isUndefined(av)) ? 1 : -1;
    	}
        else {
            const String *astr = JSValue::string(JSValue::toString(cx, av));
            const String *bstr = JSValue::string(JSValue::toString(cx, bv));
            result = astr->compare(*bstr);
        }
        return result;
    }
    else {
        js2val argv[2];
	argv[0] = av;
	argv[1] = bv;
        js2val v = cx->invokeFunction(ca->target, kNullValue, argv, 2);
        float64 f = JSValue::f64(JSValue::toNumber(cx, v));
        if (JSDOUBLE_IS_NaN(f) || (f == 0))
            result = 0;
        else
            if (f > 0)
                result = 1;
            else
                result = -1;
        return result;
    }
}


static js2val Array_sort(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    CompareArgs ca;
    ca.context = cx;
    ca.target = NULL;

    if (argc > 0) {
        if (!JSValue::isUndefined(argv[0])) {
            if (!JSValue::isFunction(argv[0]))
                cx->reportError(Exception::typeError, "sort needs a compare function");
            ca.target = JSValue::function(argv[0]);
        }
    }

    JSObject *thisObj = JSValue::getObjectValue(thisValue);
    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));

    if (length > 0) {
        uint32 i;
        js2val *vec = new js2val[length];

        for (i = 0; i < length; i++) {
            const String *id = numberToString(i);
            thisObj->getProperty(cx, *id, CURRENT_ATTR);
            vec[i] = cx->popValue();
            delete id;
        }

        js_qsort(vec, length, &ca);

        for (i = 0; i < length; i++) {
            const String *id = numberToString(i);
            thisObj->setProperty(cx, *id, CURRENT_ATTR, vec[i]);
            delete id;
        }
        delete[] vec;
    }
    return thisValue;
}

static js2val Array_splice(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (argc > 1) {
        uint32 k;
        ContextStackReplacement csr(cx);

        JSObject *thisObj = JSValue::getObjectValue(thisValue);
        thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
        js2val lengthValue = cx->popValue();
        uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, lengthValue));

        js2val result = Array_Type->newInstance(cx);
        JSArrayInstance *A = checked_cast<JSArrayInstance *>(JSValue::instance(result));

        int32 arg0 = (int32)JSValue::f64(JSValue::toInt32(cx, argv[0]));
        uint32 start;
        if (arg0 < 0) {
            arg0 += length;
            if (arg0 < 0)
                start = 0;
            else
                start = toUInt32(arg0);
        }
        else {
            if (toUInt32(arg0) >= length)
                start = length;
            else
                start = toUInt32(arg0);
        }

        uint32 deleteCount;
        int32 arg1 = (int32)JSValue::f64(JSValue::toInt32(cx, argv[1]));
        if (arg1 < 0)
            deleteCount = 0;
        else
            if (toUInt32(arg1) >= (length - start))
                deleteCount = length - start;
            else
                deleteCount = toUInt32(arg1);
        
        for (k = 0; k < deleteCount; k++) {
            const String *id1 = numberToString(start + k);
            PropertyIterator it;
            if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
                const String *id2 = numberToString(k);
                thisObj->getProperty(cx, *id1, CURRENT_ATTR);
                A->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
            }
            delete id1;
        }
        A->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)deleteCount) );

        uint32 newItemCount = argc - 2;
        if (newItemCount < deleteCount) {
            for (k = start; k < (length - deleteCount); k++) {
                const String *id1 = numberToString(k + deleteCount);
                const String *id2 = numberToString(k + newItemCount);
                PropertyIterator it;
                if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
                    thisObj->getProperty(cx, *id1, CURRENT_ATTR);
                    thisObj->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
                }
                else
                    thisObj->deleteProperty(cx, *id2, CURRENT_ATTR);
                delete id1;
                delete id2;
            }
            for (k = length; k > (length - deleteCount + newItemCount); k--) {
                const String *id1 = numberToString(k - 1);
                thisObj->deleteProperty(cx, *id1, CURRENT_ATTR);
            }
        }
        else {
            if (newItemCount > deleteCount) {
                for (k = length - deleteCount; k > start; k--) {
                    const String *id1 = numberToString(k + deleteCount - 1);
                    const String *id2 = numberToString(k + newItemCount - 1);
                    PropertyIterator it;
                    if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
                        thisObj->getProperty(cx, *id1, CURRENT_ATTR);
                        thisObj->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
                    }
                    else
                        thisObj->deleteProperty(cx, *id2, CURRENT_ATTR);
                    delete id1;
                    delete id2;
                }
            }
        }
        k = start;
        for (uint32 i = 2; i < argc; i++) {
            const String *id1 = numberToString(k++);
            thisObj->setProperty(cx, *id1, CURRENT_ATTR, argv[i]);
            delete id1;
        }
        thisObj->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)(length - deleteCount + newItemCount)) );

        return result;
    }
    return kUndefinedValue;
}

static js2val Array_unshift(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    ContextStackReplacement csr(cx);

    JSObject *thisObj = JSValue::getObjectValue(thisValue);
    thisObj->getProperty(cx, cx->Length_StringAtom, CURRENT_ATTR);
    js2val result = cx->popValue();
    uint32 length = (uint32)JSValue::f64(JSValue::toUInt32(cx, result));
    uint32 k;

    for (k = length; k > 0; k--) {
        const String *id1 = numberToString(k - 1);
        const String *id2 = numberToString(k + argc - 1);
        PropertyIterator it;
        if (thisObj->hasOwnProperty(cx, *id1, CURRENT_ATTR, Read, &it)) {
            thisObj->getProperty(cx, *id1, CURRENT_ATTR);
            thisObj->setProperty(cx, *id2, CURRENT_ATTR, cx->popValue());
        }
        else
            thisObj->deleteProperty(cx, *id2, CURRENT_ATTR);
        delete id1;
        delete id2;
    }

    for (k = 0; k < argc; k++) {
        const String *id1 = numberToString(k);
        thisObj->setProperty(cx, *id1, CURRENT_ATTR, argv[k]);
        delete id1;
    }
    thisObj->setProperty(cx, cx->Length_StringAtom, CURRENT_ATTR, JSValue::newNumber((float64)(length + argc)) );

    return thisValue;
}

// An index has to pass the test that :
//   ToString(ToUint32(ToString(index))) == ToString(index)     
//
//  the 'ToString(index)' operation is the default behaviour of '[]'
//
//  isArrayIndex() is called when we know that index is a Number
//
static bool isArrayIndex(Context *cx, js2val index, uint32 &intIndex)
{
    ASSERT(JSValue::isNumber(index));
    js2val v = JSValue::toUInt32(cx, index);
    if ((JSValue::f64(v) == JSValue::f64(index)) && (JSValue::f64(v) < two32minus1)) {
        intIndex = (uint32)JSValue::f64(v);
        return true;
    }
    return false;
}

js2val Array_GetElement(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    // the 'this' value is the first argument, then the index
    if (argc != 2)
        cx->reportError(Exception::referenceError, "[] only supports single dimension");

    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thisValue)); 

    js2val index = argv[1];
    const String *name = JSValue::string(JSValue::toString(cx, index));
    
    arrInst->getProperty(cx, *name, NULL);
    return cx->popValue();
}

js2val Array_SetElement(Context *cx, const js2val thisValue, js2val *argv, uint32 argc)
{
    // the 'this' value is the first argument, then the rValue, and finally the index
    if (argc != 3)
        cx->reportError(Exception::referenceError, "[]= only supports single dimension");

    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thisValue)); 

    js2val index = argv[2];
    const String *name = JSValue::string(JSValue::toString(cx, index));

    if (JSValue::isNumber(index)) {
        uint32 intIndex;
        if (isArrayIndex(cx, index, intIndex)) {
            PropertyIterator it = arrInst->findNamespacedProperty(*name, NULL);
            if (it == arrInst->mProperties.end())
                arrInst->insertNewProperty(*name, NULL, Property::Enumerable, Object_Type, argv[1]);
            else {
                Property *prop = PROPERTY(it);
                ASSERT(prop->mFlag == ValuePointer);
                prop->mData.vp = argv[1];
            }
            if (intIndex >= arrInst->mLength)
                arrInst->mLength = intIndex + 1;
        }
    }
    else {
        arrInst->setProperty(cx, *name, NULL, argv[1]);
    }
    return argv[0];
}



Context::PrototypeFunctions *getArrayProtos()
{
    Context::ProtoFunDef arrayProtos[] = 
    {
        { "toString",           String_Type, 0, Array_toString },
        { "toLocaleString",     String_Type, 0, Array_toString },       // XXX 
        { "toSource",           String_Type, 0, Array_toSource },
        { "push",               Number_Type, 1, Array_push },
        { "pop",                Object_Type, 0, Array_pop },
        { "concat",             Array_Type,  1, Array_concat },
        { "join",               String_Type, 1, Array_join },
        { "reverse",            Array_Type,  0, Array_reverse },
        { "shift",              Object_Type, 0, Array_shift },
        { "slice",              Array_Type,  2, Array_slice },
        { "sort",               Array_Type,  1, Array_sort },
        { "splice",             Array_Type,  2, Array_splice },
        { "unshift",            Number_Type, 1, Array_unshift },
        { NULL }
    };
    return new Context::PrototypeFunctions(&arrayProtos[0]);
}

}
}
