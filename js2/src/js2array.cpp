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
#include "msvc_pragma.h"
#endif


#include <algorithm>
#include <list>
#include <map>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {    
namespace MetaData {

uint32 getLength(JS2Metadata *meta, JS2Object *obj)
{
    Multiname mn(meta->engine->length_StringAtom, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);
    uint32 length = 0;
    js2val result;
    if (meta->readDynamicProperty(obj, &mn, &lookup, RunPhase, &result))
        length = toUInt32(meta->toInteger(result));
    return length;
}

js2val setLength(JS2Metadata *meta, JS2Object *obj, uint32 length)
{
    Multiname mn(meta->engine->length_StringAtom, meta->publicNamespace);
    js2val result = meta->engine->allocNumber(length);
    meta->writeDynamicProperty(obj, &mn, true, result, RunPhase);
    return result;
}

js2val Array_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    js2val thatValue = OBJECT_TO_JS2VAL(new ArrayInstance(meta->arrayClass));
    ArrayInstance *arrInst = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(thatValue));
    if (argc > 0) {
        if (argc == 1) {
            if (JS2VAL_IS_NUMBER(argv[0])) {
                uint32 i = (uint32)(meta->toFloat64(argv[0]));
                if (i == meta->toFloat64(argv[0]))
                    setLength(meta, arrInst, i);
                else
                    meta->reportError(Exception::rangeError, "Array length too large", meta->engine->errorPos());
            }
            else {
                setLength(meta, arrInst, 1);
                Multiname mn(meta->toString((int32)0), meta->publicNamespace);
                meta->writeDynamicProperty(arrInst, &mn, true, argv[0], RunPhase);
                delete mn.name;
            }
        }
        else {
            Multiname mn(NULL, meta->publicNamespace);
            setLength(meta, arrInst, argc);
            for (uint32 i = 0; i < argc; i++) {
                mn.name = meta->toString(i);
                meta->writeDynamicProperty(arrInst, &mn, true, argv[i], RunPhase);
                delete mn.name;
            }
        }
    }
    return thatValue;
}

static js2val Array_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (meta->objectType(thisValue) != meta->arrayClass)
        meta->reportError(Exception::typeError, "Array.prototype.toString called on a non Array", meta->engine->errorPos());

    ArrayInstance *arrInst = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(thisValue));
    uint32 length = getLength(meta, arrInst);

    if (length == 0)
        return STRING_TO_JS2VAL(meta->engine->allocString(meta->engine->Empty_StringAtom));
    else {
        Multiname mn(NULL, meta->publicNamespace);
        LookupKind lookup(false, JS2VAL_NULL);
        js2val result;
        String *s = new String();
        for (uint32 i = 0; i < length; i++) {
            mn.name = numberToString(i);
            if (meta->readDynamicProperty(arrInst, &mn, &lookup, RunPhase, &result)
                    && !JS2VAL_IS_UNDEFINED(result))
                s->append(*meta->toString(result));
            if (i < (length - 1))
                s->append(widenCString(","));
            delete mn.name;
        }
        result = meta->engine->allocString(s);
        delete s;
        return result;
    }
    
}

static js2val Array_toSource(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    if (meta->objectType(thisValue) != meta->arrayClass)
        meta->reportError(Exception::typeError, "Array.prototype.toString called on a non Array", meta->engine->errorPos());

    ArrayInstance *arrInst = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(thisValue));
    uint32 length = getLength(meta, arrInst);

    if (length == 0)
        return meta->engine->allocString("[]");
    else {
        Multiname mn(NULL, meta->publicNamespace);
        LookupKind lookup(false, JS2VAL_NULL);
        js2val result;
        String *s = new String();
        for (uint32 i = 0; i < length; i++) {
            mn.name = numberToString(i);
            if (meta->readDynamicProperty(arrInst, &mn, &lookup, RunPhase, &result)
                    && !JS2VAL_IS_UNDEFINED(result))
                s->append(*meta->toString(result));
            if (i < (length - 1))
                s->append(widenCString(","));
            delete mn.name;
        }
        s->append(widenCString("]"));
        result = meta->engine->allocString(s);
        delete s;
        return result;
    }
    
}

static js2val Array_push(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
    uint32 length = getLength(meta, thisObj);

    Multiname mn(NULL, meta->publicNamespace);
    for (uint32 i = 0; i < argc; i++) {
        mn.name = numberToString(i + length);
        meta->writeDynamicProperty(thisObj, &mn, true, argv[i], RunPhase);
        delete mn.name;
    }
    return setLength(meta, thisObj, length + argc);
}
              
static js2val Array_pop(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
    uint32 length = getLength(meta, thisObj);

    if (length > 0) {
        Multiname mn(numberToString(length - 1), meta->publicNamespace);
        LookupKind lookup(false, JS2VAL_NULL);
        js2val result = JS2VAL_UNDEFINED;
        bool deleteResult;
        meta->readDynamicProperty(thisObj, &mn, &lookup, RunPhase, &result);
        meta->deleteProperty(thisValue, &mn, &lookup, RunPhase, &deleteResult);
        setLength(meta, thisObj, length - 1);
        delete mn.name;
        return result;
    }
    else
        return JS2VAL_UNDEFINED;
}

js2val Array_concat(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val E = thisValue;

    js2val result = OBJECT_TO_JS2VAL(new ArrayInstance(meta->arrayClass));
    ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));
    uint32 n = 0;
    uint32 i = 0;
    Multiname mn(NULL, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);

    do {
        if (meta->objectType(E) != meta->arrayClass) {
            mn.name = numberToString(n++);
            meta->writeDynamicProperty(A, &mn, true, E, RunPhase);
            delete mn.name;
        }
        else {
            ASSERT(JS2VAL_IS_OBJECT(thisValue));
            JS2Object *arrObj = JS2VAL_TO_OBJECT(E);
            uint32 length = getLength(meta, arrObj);
            for (uint32 k = 0; k < length; k++) {
                mn.name = numberToString(k);
                js2val rval = JS2VAL_UNDEFINED;
                meta->readDynamicProperty(arrObj, &mn, &lookup, RunPhase, &rval);
                delete mn.name;
                mn.name = numberToString(n++);
                meta->writeDynamicProperty(A, &mn, true, rval, RunPhase);
                delete mn.name;
            }
        }
        E = argv[i++];
    } while (i <= argc);
    
    return result;
}

static js2val Array_join(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
    uint32 length = getLength(meta, thisObj);

    const String *separator;
    if ((argc == 0) || JS2VAL_IS_UNDEFINED(argv[0]))
        separator = new String(widenCString(","));
    else
        separator = meta->toString(argv[0]);

    String *S = new String();
    Multiname mn(NULL, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);

    for (uint32 k = 0; k < length; k++) {
        js2val result = JS2VAL_UNDEFINED;
        mn.name = numberToString(k);
        meta->readDynamicProperty(thisObj, &mn, &lookup, RunPhase, &result);
        if (!JS2VAL_IS_UNDEFINED(result) && !JS2VAL_IS_NULL(result))
            *S += *meta->toString(result);

        if (k < (length - 1))
            *S += *separator;
        delete mn.name;
    }
    
    return meta->engine->allocString(S);
}

static js2val Array_reverse(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
    uint32 length = getLength(meta, thisObj);

    Multiname mn1(NULL, meta->publicNamespace);
    Multiname mn2(NULL, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);

    uint32 halfway = length / 2;

    for (uint32 k = 0; k < halfway; k++) {
        bool deleteResult;
        js2val result1 = JS2VAL_UNDEFINED;
        js2val result2 = JS2VAL_UNDEFINED;
        mn1.name = numberToString(k);
        mn2.name = numberToString(length - k - 1);

        if (meta->hasOwnProperty(thisObj, mn1.name)) {
            if (meta->hasOwnProperty(thisObj, mn2.name)) {
                meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &result1);
                meta->readDynamicProperty(thisObj, &mn2, &lookup, RunPhase, &result2);
                meta->writeDynamicProperty(thisObj, &mn1, true, result2, RunPhase);
                meta->writeDynamicProperty(thisObj, &mn2, true, result1, RunPhase);
            }
            else {
                meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &result1);
                meta->writeDynamicProperty(thisObj, &mn2, true, result1, RunPhase);
                meta->deleteProperty(thisValue, &mn1, &lookup, RunPhase, &deleteResult);
            }
        }
        else {
            if (meta->hasOwnProperty(thisObj, mn2.name)) {
                meta->readDynamicProperty(thisObj, &mn2, &lookup, RunPhase, &result2);
                meta->writeDynamicProperty(thisObj, &mn1, true, result2, RunPhase);
                meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
            }
            else {
                meta->deleteProperty(thisValue, &mn1, &lookup, RunPhase, &deleteResult);
                meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
            }
        }
        delete mn1.name;
        delete mn2.name;
    }

    return thisValue;
}

static js2val Array_shift(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
    uint32 length = getLength(meta, thisObj);

    if (length == 0) {
        setLength(meta, thisObj, 0);
        return JS2VAL_UNDEFINED;
    }

    Multiname mn1(NULL, meta->publicNamespace);
    Multiname mn2(NULL, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);
    js2val result;
    bool deleteResult;
    mn1.name = numberToString((int32)0);
    meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &result);
    delete mn1.name;

    for (uint32 k = 1; k < length; k++) {
        mn1.name = numberToString(k);
        mn2.name = numberToString(k - 1);

        if (meta->hasOwnProperty(thisObj, mn1.name)) {
            meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &result);
            meta->writeDynamicProperty(thisObj, &mn2, true, result, RunPhase);
        }
        else
            meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
        delete mn1.name;
        delete mn2.name;
    }

    mn1.name = numberToString(length - 1);
    meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
    delete mn1.name;
    setLength(meta, thisObj, length - 1);
    return result;
}

static js2val Array_slice(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);

    js2val result = OBJECT_TO_JS2VAL(new ArrayInstance(meta->arrayClass));
    ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));

    uint32 length = getLength(meta, thisObj);

    uint32 start, end;
    if (argc < 1) 
        start = 0;
    else {
        int32 arg0 = meta->toInteger(argv[0]);
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
        int32 arg1 = meta->toInteger(argv[1]);
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
    
    Multiname mn1(NULL, meta->publicNamespace);
    Multiname mn2(NULL, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);
    uint32 n = 0;
    while (start < end) {
        mn1.name = numberToString(start);
        if (meta->hasOwnProperty(thisObj, mn1.name)) {
            js2val rval;
            mn2.name = numberToString(n);
            meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &rval);
            meta->writeDynamicProperty(A, &mn2, true, rval, RunPhase);
            delete mn2.name;
        }
        n++;
        start++;
        delete mn1.name;
    }
    setLength(meta, A, n);
    return result;
}

typedef struct CompareArgs {
    JS2Metadata *meta;
    JS2Object   *target;
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
    JS2Metadata *meta = ca->meta;
    int32 result;

    if (ca->target == NULL) {
        if (JS2VAL_IS_UNDEFINED(av) || JS2VAL_IS_UNDEFINED(bv)) {
	    /* Put undefined properties at the end. */
	    result = (JS2VAL_IS_UNDEFINED(av)) ? 1 : -1;
    	}
        else {
            const String *astr = meta->toString(av);
            const String *bstr = meta->toString(bv);
            result = astr->compare(*bstr);
        }
        return result;
    }
    else {
        js2val argv[2];
	argv[0] = av;
	argv[1] = bv;
        js2val v = JS2VAL_UNDEFINED;// XXX = cx->invokeFunction(ca->target, kNullValue, argv, 2);
        float64 f = meta->toFloat64(v);
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


static js2val Array_sort(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    CompareArgs ca;
    ca.meta = meta;
    ca.target = NULL;

    if (argc > 0) {
        if (!JS2VAL_IS_UNDEFINED(argv[0])) {
            if (meta->objectType(argv[0]) != meta->functionClass)
                meta->reportError(Exception::typeError, "sort needs a compare function", meta->engine->errorPos());
            ca.target = JS2VAL_TO_OBJECT(argv[0]);
        }
    }

    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
    uint32 length = getLength(meta, thisObj);

    if (length > 0) {
        uint32 i;
        js2val *vec = new js2val[length];
        
        Multiname mn(NULL, meta->publicNamespace);
        LookupKind lookup(false, JS2VAL_NULL);
        for (i = 0; i < length; i++) {
            mn.name = numberToString(i);
            meta->readDynamicProperty(thisObj, &mn, &lookup, RunPhase, &vec[i]);
            delete mn.name;
        }

        js_qsort(vec, length, &ca);

        for (i = 0; i < length; i++) {
            mn.name = numberToString(i);
            meta->writeDynamicProperty(thisObj, &mn, true, vec[i], RunPhase);
            delete mn.name;
        }
        delete[] mn.name;
    }
    return thisValue;
}

static js2val Array_splice(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (argc > 1) {
        uint32 k;

        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);
        uint32 length = getLength(meta, thisObj);

        js2val result = OBJECT_TO_JS2VAL(new ArrayInstance(meta->arrayClass));
        ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));

        int32 arg0 = meta->toInteger(argv[0]);
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
        int32 arg1 = meta->toInteger(argv[1]);
        if (arg1 < 0)
            deleteCount = 0;
        else
            if (toUInt32(arg1) >= (length - start))
                deleteCount = length - start;
            else
                deleteCount = toUInt32(arg1);
        
        Multiname mn1(NULL, meta->publicNamespace);
        Multiname mn2(NULL, meta->publicNamespace);
        LookupKind lookup(false, JS2VAL_NULL);
        for (k = 0; k < deleteCount; k++) {
            mn1.name = numberToString(start + k);
            if (meta->hasOwnProperty(thisObj, mn1.name)) {
                js2val rval;
                mn2.name = numberToString(k);
                meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &rval);
                meta->writeDynamicProperty(A, &mn2, true, rval, RunPhase);
                delete mn2.name;
            }
            delete mn1.name;
        }
        setLength(meta, A, deleteCount);

        uint32 newItemCount = argc - 2;
        if (newItemCount < deleteCount) {
            bool deleteResult;
            for (k = start; k < (length - deleteCount); k++) {
                mn1.name = numberToString(k + deleteCount);
                mn2.name = numberToString(k + newItemCount);
                if (meta->hasOwnProperty(thisObj, mn1.name)) {
                    js2val rval;
                    meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &rval);
                    meta->writeDynamicProperty(A, &mn2, true, rval, RunPhase);
                }
                else
                    meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
                delete mn1.name;
                delete mn2.name;
            }
            for (k = length; k > (length - deleteCount + newItemCount); k--) {
                mn1.name = numberToString(k - 1);
                meta->deleteProperty(thisValue, &mn1, &lookup, RunPhase, &deleteResult);
                delete mn1.name;
            }
        }
        else {
            if (newItemCount > deleteCount) {
                for (k = length - deleteCount; k > start; k--) {
                    bool deleteResult;
                    mn1.name = numberToString(k + deleteCount - 1);
                    mn1.name = numberToString(k + newItemCount - 1);
                    if (meta->hasOwnProperty(thisObj, mn1.name)) {
                        js2val rval;
                        meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &rval);
                        meta->writeDynamicProperty(A, &mn2, true, rval, RunPhase);
                    }
                    else
                        meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
                    delete mn1.name;
                    delete mn2.name;
                }
            }
        }
        k = start;
        for (uint32 i = 2; i < argc; i++) {
            mn1.name = numberToString(k++);
            meta->writeDynamicProperty(A, &mn2, true, argv[i], RunPhase);
            delete mn1.name;
        }
        setLength(meta, thisObj, (length - deleteCount + newItemCount));

        return result;
    }
    return JS2VAL_UNDEFINED;
}

static js2val Array_unshift(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    ASSERT(JS2VAL_IS_OBJECT(thisValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thisValue);

    uint32 length = getLength(meta, thisObj);
    uint32 k;

    Multiname mn1(NULL, meta->publicNamespace);
    Multiname mn2(NULL, meta->publicNamespace);
    LookupKind lookup(false, JS2VAL_NULL);

    for (k = length; k > 0; k--) {
        bool deleteResult;
        mn1.name = numberToString(k - 1);
        mn2.name = numberToString(k + argc - 1);
        if (meta->hasOwnProperty(thisObj, mn1.name)) {
            js2val rval;
            meta->readDynamicProperty(thisObj, &mn1, &lookup, RunPhase, &rval);
            meta->writeDynamicProperty(thisObj, &mn2, true, rval, RunPhase);
        }
        else
            meta->deleteProperty(thisValue, &mn2, &lookup, RunPhase, &deleteResult);
        delete mn1.name;
        delete mn2.name;
    }

    for (k = 0; k < argc; k++) {
        mn1.name = numberToString(k);
        meta->writeDynamicProperty(thisObj, &mn1, true, argv[k], RunPhase);
        delete mn1.name;
    }
    setLength(meta, thisObj, (length + argc));

    return thisValue;
}

#if 0
// An index has to pass the test that :
//   toString(ToUint32(toString(index))) == toString(index)     
//
//  the 'toString(index)' operation is the default behaviour of '[]'
//
//  isArrayIndex() is called when we know that index is a Number
//
static bool isArrayIndex(JS2Metadata *meta, js2val index, uint32 &intIndex)
{
    ASSERT(JS2VAL_IS_NUMBER(index));
    js2val v = JSValue::toUInt32(cx, index);
    if ((JSValue::f64(v) == JSValue::f64(index)) && (JSValue::f64(v) < two32minus1)) {
        intIndex = (uint32)JSValue::f64(v);
        return true;
    }
    return false;
}

js2val Array_GetElement(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    // the 'this' value is the first argument, then the index
    if (argc != 2)
        cx->reportError(Exception::referenceError, "[] only supports single dimension");

    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thisValue)); 

    js2val index = argv[1];
    const String *name = JSValue::string(JSValue::meta->toString(cx, index));
    
    arrInst->getProperty(cx, *name, NULL);
    return cx->popValue();
}

js2val Array_SetElement(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    // the 'this' value is the first argument, then the rValue, and finally the index
    if (argc != 3)
        cx->reportError(Exception::referenceError, "[]= only supports single dimension");

    JSArrayInstance *arrInst = checked_cast<JSArrayInstance *>(JSValue::instance(thisValue)); 

    js2val index = argv[2];
    const String *name = JSValue::string(JSValue::meta->toString(cx, index));

    if (JS2VAL_IS_NUMBER(index)) {
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
#endif


void initArrayObject(JS2Metadata *meta)
{

    typedef struct {
        char *name;
        uint16 length;
        NativeCode *code;
    } PrototypeFunction;

    PrototypeFunction arrayProtos[] = 
    {
        { "toString",           0, Array_toString },
        { "toLocaleString",     0, Array_toString },       // XXX 
        { "toSource",           0, Array_toSource },
        { "push",               1, Array_push },
        { "pop",                0, Array_pop },
        { "concat",             1, Array_concat },
        { "join",               1, Array_join },
        { "reverse",            0, Array_reverse },
        { "shift",              0, Array_shift },
        { "slice",              2, Array_slice },
        { "sort",               1, Array_sort },
        { "splice",             2, Array_splice },
        { "unshift",            1, Array_unshift },
        { NULL }
    };

    NamespaceList publicNamespaceList;
    publicNamespaceList.push_back(meta->publicNamespace);

    meta->arrayClass->construct = Array_Constructor;

    PrototypeFunction *pf = &arrayProtos[0];
    while (pf->name) {
        FixedInstance *fInst = new FixedInstance(meta->functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code);
    
        InstanceMember *m = new InstanceMethod(fInst);
        meta->defineInstanceMember(meta->arrayClass, &meta->cxt, &meta->world.identifiers[pf->name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, m, 0);
        pf++;
    }
}

}
}
