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
    uint32 length = 0;
    if ((obj->kind == SimpleInstanceKind)
            && (checked_cast<SimpleInstance *>(obj)->type == meta->arrayClass)) {
        length = meta->valToUInt32(Array_lengthGet(meta, OBJECT_TO_JS2VAL(obj), NULL, 0));
    }
    else {
        js2val result;
        JS2Class *c = meta->objectType(obj);
        js2val val = OBJECT_TO_JS2VAL(obj);
        if (c->ReadPublic(meta, &val, meta->engine->length_StringAtom, RunPhase, &result))
            length = meta->valToUInt32(result);
    }
    return length;
}

js2val setLength(JS2Metadata *meta, JS2Object *obj, uint32 newLength)
{
    js2val result = meta->engine->allocNumber(newLength);
    if ((obj->kind == SimpleInstanceKind)
            && (checked_cast<SimpleInstance *>(obj)->type == meta->arrayClass)) {
        Array_lengthSet(meta, OBJECT_TO_JS2VAL(obj), &result, 1);
    }
    else {
        JS2Class *c = meta->objectType(obj);
        c->WritePublic(meta, OBJECT_TO_JS2VAL(obj), meta->engine->length_StringAtom, true, result);
    }
    return result;
}

js2val Array_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    js2val thatValue = OBJECT_TO_JS2VAL(new (meta) ArrayInstance(meta, meta->arrayClass->prototype, meta->arrayClass));
    ArrayInstance *arrInst = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(thatValue));
    DEFINE_ROOTKEEPER(meta, rk, arrInst);
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
                meta->createDynamicProperty(arrInst, meta->engine->numberToStringAtom((int32)0), argv[0], ReadWriteAccess, false, true);
                setLength(meta, arrInst, 1);
            }
        }
        else {
            uint32 i;
            for (i = 0; i < argc; i++) {
                meta->createDynamicProperty(arrInst, meta->engine->numberToStringAtom(i), argv[i], ReadWriteAccess, false, true);
            }
            setLength(meta, arrInst, i);
        }
    }
    else
        setLength(meta, arrInst, 0);
    return thatValue;
}

static js2val Array_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val thatValue = thisValue;
    if (!JS2VAL_IS_OBJECT(thatValue) 
            || (JS2VAL_TO_OBJECT(thatValue)->kind != SimpleInstanceKind)
            || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thatValue)))->type != meta->arrayClass))
        meta->reportError(Exception::typeError, "Array.prototype.toString called on a non Array", meta->engine->errorPos());

    ArrayInstance *arrInst = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(thatValue));
    uint32 length = getLength(meta, arrInst);

    if (length == 0)
		if (meta->version == JS2VERSION_1_2)
	        return STRING_TO_JS2VAL(meta->engine->allocString("[]"));
		else
		    return STRING_TO_JS2VAL(meta->engine->allocString(meta->engine->Empty_StringAtom));
    else {
        js2val result;
        String *s;
		if (meta->version == JS2VERSION_1_2)
			 s = new String(widenCString("["));
		else
			s = new String();
        for (uint32 i = 0; i < length; i++) {
            if (meta->arrayClass->ReadPublic(meta, &thatValue, meta->engine->numberToStringAtom(i), RunPhase, &result)
                    && !JS2VAL_IS_UNDEFINED(result)
                    && !JS2VAL_IS_NULL(result) )
                s->append(*meta->toString(result));
            if (i < (length - 1))
		        if (meta->version == JS2VERSION_1_2)
                    s->append(widenCString(", "));
                else
                    s->append(widenCString(","));
        }
		if (meta->version == JS2VERSION_1_2)
			s->append(widenCString("]"));
        result = meta->engine->allocString(s);
        delete s;
        return result;
    }
    
}

static js2val Array_toSource(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val thatValue = thisValue;
    if (!JS2VAL_IS_OBJECT(thatValue) 
            || (JS2VAL_TO_OBJECT(thatValue)->kind != SimpleInstanceKind)
            || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thatValue)))->type != meta->arrayClass))
        meta->reportError(Exception::typeError, "Array.prototype.toString called on a non Array", meta->engine->errorPos());

    ArrayInstance *arrInst = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(thatValue));
    uint32 length = getLength(meta, arrInst);

    if (length == 0)
        return meta->engine->allocString("[]");
    else {
        js2val result;
        String *s = new String(widenCString("["));
        for (uint32 i = 0; i < length; i++) {
            if (meta->arrayClass->ReadPublic(meta, &thatValue, meta->engine->numberToStringAtom(i), RunPhase, &result)
                    && !JS2VAL_IS_UNDEFINED(result))
                s->append(*meta->toString(result));
            if (i < (length - 1))
                s->append(widenCString(","));
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

    JS2Class *c = meta->objectType(thisObj);
    for (uint32 i = 0; i < argc; i++) {
        c->WritePublic(meta, thisValue, meta->engine->numberToStringAtom(i + length), true, argv[i]);
    }
    return setLength(meta, thisObj, length + argc);
}
              
static js2val Array_pop(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val thatValue = thisValue;
    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);
    uint32 length = getLength(meta, thisObj);

    if (length > 0) {
        js2val result = JS2VAL_UNDEFINED;
        bool deleteResult;
        JS2Class *c = meta->objectType(thisObj);
        c->ReadPublic(meta, &thatValue, meta->engine->numberToStringAtom(length - 1), RunPhase, &result);
        c->DeletePublic(meta, thatValue, meta->engine->numberToStringAtom(length - 1), &deleteResult);
        setLength(meta, thisObj, length - 1);
        return result;
    }
    else
        return JS2VAL_UNDEFINED;
}

js2val Array_concat(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val E = thisValue;

    js2val result = OBJECT_TO_JS2VAL(new (meta) ArrayInstance(meta, meta->arrayClass->prototype, meta->arrayClass));
    ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));
    DEFINE_ROOTKEEPER(meta, rk, A);
    uint32 n = 0;
    uint32 i = 0;

    do {
        if (meta->objectType(E) != meta->arrayClass) {
            meta->arrayClass->WritePublic(meta, result, meta->engine->numberToStringAtom(n++), true, E);
        }
        else {
            ASSERT(JS2VAL_IS_OBJECT(thisValue));
            JS2Object *arrObj = JS2VAL_TO_OBJECT(E);
            uint32 length = getLength(meta, arrObj);
            JS2Class *c = meta->objectType(arrObj);
            for (uint32 k = 0; k < length; k++) {
                js2val rval = JS2VAL_UNDEFINED;
                c->ReadPublic(meta, &E, meta->engine->numberToStringAtom(k), RunPhase, &rval);                
                meta->arrayClass->WritePublic(meta, result, meta->engine->numberToStringAtom(n++), true, rval);
            }
        }
        E = argv[i++];
    } while (i <= argc);
    
    return result;
}

static js2val Array_join(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val thatValue = thisValue;
    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);
    uint32 length = getLength(meta, thisObj);

    const String *separator = NULL;
    DEFINE_ROOTKEEPER(meta, rk1, separator);
    if ((argc == 0) || JS2VAL_IS_UNDEFINED(argv[0]))
        separator = meta->engine->allocStringPtr(",");
    else
        separator = meta->toString(argv[0]);

    String *S = new String();   // not engine allocated, doesn't need rooting
    JS2Class *c = meta->objectType(thisObj);

    for (uint32 k = 0; k < length; k++) {
        js2val result = JS2VAL_UNDEFINED;
        c->ReadPublic(meta, &thatValue, meta->engine->numberToStringAtom(k), RunPhase, &result);                
        if (!JS2VAL_IS_UNDEFINED(result) && !JS2VAL_IS_NULL(result))
            *S += *meta->toString(result);

        if (k < (length - 1))
            *S += *separator;
    }
    
    return meta->engine->allocString(S);
}

static js2val Array_reverse(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val thatValue = thisValue;
    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);
    uint32 length = getLength(meta, thisObj);
    uint32 halfway = length / 2;

    JS2Class *c = meta->objectType(thisObj);

    for (uint32 k = 0; k < halfway; k++) {
        bool deleteResult;
        js2val result1 = JS2VAL_UNDEFINED;
        js2val result2 = JS2VAL_UNDEFINED;
        const StringAtom &nm1 = meta->engine->numberToStringAtom(k);
        const StringAtom &nm2 = meta->engine->numberToStringAtom(length - k - 1);

        if (meta->hasOwnProperty(thisObj, nm1)) {
            if (meta->hasOwnProperty(thisObj, nm2)) {
                c->ReadPublic(meta, &thatValue, nm1, RunPhase, &result1);                
                c->ReadPublic(meta, &thatValue, nm2, RunPhase, &result2);                
                c->WritePublic(meta, thatValue, nm1, true, result2);                
                c->WritePublic(meta, thatValue, nm2, true, result1);                
            }
            else {
                c->ReadPublic(meta, &thatValue, nm1, RunPhase, &result1);                
                c->WritePublic(meta, thatValue, nm2, true, result1);
                c->DeletePublic(meta, thatValue, nm1, &deleteResult);
            }
        }
        else {
            if (meta->hasOwnProperty(thisObj, nm2)) {
                c->ReadPublic(meta, &thatValue, nm2, RunPhase, &result2);                
                c->WritePublic(meta, thatValue, nm1, true, result2);
                c->DeletePublic(meta, thatValue, nm2, &deleteResult);
            }
            else {
                c->DeletePublic(meta, thatValue, nm1, &deleteResult);
                c->DeletePublic(meta, thatValue, nm2, &deleteResult);
            }
        }
    }
    
    return thatValue;
}

static js2val Array_shift(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    js2val thatValue = thisValue;
    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);
    uint32 length = getLength(meta, thisObj);

    if (length == 0) {
        setLength(meta, thisObj, 0);
        return JS2VAL_UNDEFINED;
    }

    JS2Class *c = meta->objectType(thisObj);

    js2val result;
    bool deleteResult;
    const StringAtom &nm1 = meta->engine->numberToStringAtom((int32)0);
    c->ReadPublic(meta, &thatValue, nm1, RunPhase, &result);                

    for (uint32 k = 1; k < length; k++) {
        const StringAtom &nm1 = meta->engine->numberToStringAtom(k);
        const StringAtom &nm2 = meta->engine->numberToStringAtom(k - 1);

        if (meta->hasOwnProperty(thisObj, nm1)) {
            c->ReadPublic(meta, &thatValue, nm1, RunPhase, &result);                
            c->WritePublic(meta, thatValue, nm2, true, result);
        }
        else
            c->DeletePublic(meta, thatValue, nm2, &deleteResult);
    }

    const StringAtom &nm2 = meta->engine->numberToStringAtom(length - 1);
    c->DeletePublic(meta, thatValue, nm2, &deleteResult);
    setLength(meta, thisObj, length - 1);
    return result;
}

static js2val Array_slice(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val thatValue = thisValue;
    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);

    js2val result = OBJECT_TO_JS2VAL(new (meta) ArrayInstance(meta, meta->arrayClass->prototype, meta->arrayClass));
    ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));
    DEFINE_ROOTKEEPER(meta, rk, A);

    uint32 length = getLength(meta, thisObj);

    uint32 start, end;
    if (argc < 1) 
        start = 0;
    else {
        int32 arg0 = meta->valToInt32(argv[0]);
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
        int32 arg1 = meta->valToInt32(argv[1]);
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
    
    JS2Class *c = meta->objectType(thisObj);
    uint32 n = 0;
    while (start < end) {
        const StringAtom &nm1 = meta->engine->numberToStringAtom(start);
        if (meta->hasOwnProperty(thisObj, nm1)) {
            js2val rval;
            const StringAtom &nm2 = meta->engine->numberToStringAtom(n);
            c->ReadPublic(meta, &thatValue, nm1, RunPhase, &rval);                
            meta->arrayClass->WritePublic(meta, result, nm2, true, rval);
        }
        n++;
        start++;
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

    pivot = (js2val *)STD::malloc(nel * sizeof(js2val));
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
            DEFINE_ROOTKEEPER(meta, rk1, astr);
            const String *bstr = meta->toString(bv);
            DEFINE_ROOTKEEPER(meta, rk2, bstr);
            result = astr->compare(*bstr);
        }
        return result;
    }
    else {
        js2val argv[2];
        argv[0] = av;
        argv[1] = bv;
        js2val v = meta->invokeFunction(ca->target, JS2VAL_NULL, argv, 2, NULL);
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
    js2val thatValue = thisValue;
    CompareArgs ca;
    ca.meta = meta;
    ca.target = NULL;

    if (argc > 0) {
        if (!JS2VAL_IS_UNDEFINED(argv[0])) {
                if ((JS2VAL_TO_OBJECT(argv[0])->kind != SimpleInstanceKind)
                        || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(argv[0])))->type != meta->functionClass))
                meta->reportError(Exception::typeError, "sort needs a compare function", meta->engine->errorPos());
            ca.target = JS2VAL_TO_OBJECT(argv[0]);
        }
    }

    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);
    uint32 length = getLength(meta, thisObj);

    uint32 i;
    // XXX bogus! new[] was supposed to behave itself, not just assert fail or crash
    if (length > 0x1000000)
        meta->reportError(Exception::internalError, "out of memory", meta->engine->errorPos());

    js2val *vec = new js2val[length];
    DEFINE_ARRAYROOTKEEPER(meta, rk, vec, length);

    JS2Class *c = meta->objectType(thisObj);
    for (i = 0; i < length; i++) {
        c->ReadPublic(meta, &thatValue, meta->engine->numberToStringAtom(i), RunPhase, &vec[i]);                
    }

    js_qsort(vec, length, &ca);

    for (i = 0; i < length; i++) {
        c->WritePublic(meta, thatValue, meta->engine->numberToStringAtom(i), true, vec[i]);
    }
    return thatValue;
}

static js2val Array_splice(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (argc > 1) {
        uint32 k;

        js2val thatValue = thisValue;
        ASSERT(JS2VAL_IS_OBJECT(thatValue));
        JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);
        uint32 length = getLength(meta, thisObj);

        js2val result = OBJECT_TO_JS2VAL(new (meta) ArrayInstance(meta, meta->arrayClass->prototype, meta->arrayClass));
        ArrayInstance *A = checked_cast<ArrayInstance *>(JS2VAL_TO_OBJECT(result));
        DEFINE_ROOTKEEPER(meta, rk, A);

        int32 arg0 = meta->valToInt32(argv[0]);
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
        int32 arg1 = meta->valToInt32(argv[1]);
        if (arg1 < 0)
            deleteCount = 0;
        else
            if (toUInt32(arg1) >= (length - start))
                deleteCount = length - start;
            else
                deleteCount = toUInt32(arg1);
        
        JS2Class *c = meta->objectType(thisObj);

        for (k = 0; k < deleteCount; k++) {
            const StringAtom &nm1 = meta->engine->numberToStringAtom(start + k);
            if (meta->hasOwnProperty(thisObj, nm1)) {
                js2val rval;
                const StringAtom &nm2 = meta->engine->numberToStringAtom(k);
                c->ReadPublic(meta, &thatValue, nm1, RunPhase, &rval);                
                meta->arrayClass->WritePublic(meta, result, nm2, true, rval);
            }
        }
        setLength(meta, A, deleteCount);

        uint32 newItemCount = argc - 2;
        if (newItemCount < deleteCount) {
            bool deleteResult;
            for (k = start; k < (length - deleteCount); k++) {
                const StringAtom &nm1 = meta->engine->numberToStringAtom(k + deleteCount);
                const StringAtom &nm2 = meta->engine->numberToStringAtom(k + newItemCount);
                if (meta->hasOwnProperty(thisObj, nm1)) {
                    js2val rval;
                    c->ReadPublic(meta, &thatValue, nm1, RunPhase, &rval);                
                    meta->arrayClass->WritePublic(meta, result, nm2, true, rval);
                }
                else
                    c->DeletePublic(meta, thatValue, nm2, &deleteResult);                
            }
            for (k = length; k > (length - deleteCount + newItemCount); k--) {
                const StringAtom &nm1 = meta->engine->numberToStringAtom(k - 1);
                c->DeletePublic(meta, thatValue, nm1, &deleteResult);                
            }
        }
        else {
            if (newItemCount > deleteCount) {
                for (k = length - deleteCount; k > start; k--) {
                    bool deleteResult;
                    const StringAtom &nm1 = meta->engine->numberToStringAtom(k + deleteCount - 1);
                    const StringAtom &nm2 = meta->engine->numberToStringAtom(k + newItemCount - 1);
                    if (meta->hasOwnProperty(thisObj, nm1)) {
                        js2val rval;
                        c->ReadPublic(meta, &thatValue, nm1, RunPhase, &rval);                
                        meta->arrayClass->WritePublic(meta, result, nm2, true, rval);
                    }
                    else
                        c->DeletePublic(meta, thatValue, nm2, &deleteResult);                
                }
            }
        }
        k = start;
        for (uint32 i = 2; i < argc; i++) {
            const StringAtom &nm2 = meta->engine->numberToStringAtom(k++);
            meta->arrayClass->WritePublic(meta, result, nm2, true, argv[i]);
        }
        setLength(meta, thisObj, (length - deleteCount + newItemCount));
        return result;
    }
    return JS2VAL_UNDEFINED;
}

static js2val Array_unshift(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val thatValue = thisValue;
    ASSERT(JS2VAL_IS_OBJECT(thatValue));
    JS2Object *thisObj = JS2VAL_TO_OBJECT(thatValue);

    uint32 length = getLength(meta, thisObj);
    uint32 k;

    JS2Class *c = meta->objectType(thisObj);

    for (k = length; k > 0; k--) {
        bool deleteResult;
        const StringAtom &nm1 = meta->engine->numberToStringAtom(k - 1);
        const StringAtom &nm2 = meta->engine->numberToStringAtom(k + argc - 1);
        if (meta->hasOwnProperty(thisObj, nm1)) {
            js2val rval;
            c->ReadPublic(meta, &thatValue, nm1, RunPhase, &rval);                
            c->WritePublic(meta, thatValue, nm2, true, rval);
        }
        else
            c->DeletePublic(meta, thatValue, nm2, &deleteResult);
    }

    for (k = 0; k < argc; k++) {
        const StringAtom &nm1 = meta->engine->numberToStringAtom(k);
        c->WritePublic(meta, thatValue, nm1, true, argv[k]);
    }
    setLength(meta, thisObj, (length + argc));

    return thatValue;
}



void initArrayObject(JS2Metadata *meta)
{
    FunctionData prototypeFunctions[] = 
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

    meta->initBuiltinClass(meta->arrayClass, NULL, Array_Constructor, Array_Constructor);
    meta->arrayClass->prototype = OBJECT_TO_JS2VAL(new (meta) ArrayInstance(meta, OBJECT_TO_JS2VAL(meta->objectClass->prototype), meta->arrayClass));
    meta->initBuiltinClassPrototype(meta->arrayClass, &prototypeFunctions[0]);
}

}
}
