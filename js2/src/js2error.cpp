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

js2val error_ConstructorCore(JS2Metadata *meta, JS2Class *errorClass, js2val arg)
{
    JS2Object *obj = new PrototypeInstance(errorClass->prototype, errorClass);
    js2val thatValue = OBJECT_TO_JS2VAL(obj);

    if (!JS2VAL_IS_VOID(arg)) {
        Multiname mn(&meta->world.identifiers["message"], meta->publicNamespace);
        meta->writeDynamicProperty(obj, &mn, true, meta->engine->allocString(meta->toString(arg)), RunPhase);
    }

    return thatValue;
}

js2val Error_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->errorClass, argv[0]);
}

js2val EvalError_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->evalErrorClass, argv[0]);
}

js2val RangeError_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->rangeErrorClass, argv[0]);
}

js2val ReferenceError_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->referenceErrorClass, argv[0]);
}

js2val SyntaxError_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->syntaxErrorClass, argv[0]);
}

js2val TypeError_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->typeErrorClass, argv[0]);
}

js2val UriError_Constructor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    return error_ConstructorCore(meta, meta->uriErrorClass, argv[0]);
}



js2val Error_toString(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    js2val result;
    LookupKind lookup(false, JS2VAL_NULL);
    Multiname mn(&meta->world.identifiers["message"], meta->publicNamespace);
    js2val a = thisValue;

    if (meta->readProperty(&a, &mn, &lookup, RunPhase, &result)) {
        if (JS2VAL_IS_STRING(result))
            return result;
        else
            return meta->engine->allocString(meta->toString(result));
    }
    else
        return JS2VAL_UNDEFINED;
}

static void initErrorClass(JS2Metadata *meta, JS2Class *c, Constructor *constructor)
{
// XXX Or make 'name' a static class member?
    c->construct = constructor;
    c->prototype = new PrototypeInstance(NULL, c);
    js2val nameVal = meta->engine->allocString(c->name);
    meta->writeDynamicProperty(c->prototype, new Multiname(&meta->world.identifiers["name"], meta->publicNamespace), true, nameVal, RunPhase);
}

void initErrorObject(JS2Metadata *meta)
{

    typedef struct {
        char *name;
        uint16 length;
        NativeCode *code;
    } PrototypeFunction;

    PrototypeFunction errorProtos[] = 
    {
        { "toString",           0, Error_toString },
        { NULL }
    };

    NamespaceList publicNamespaceList;
    publicNamespaceList.push_back(meta->publicNamespace);


    PrototypeFunction *pf = &errorProtos[0];
    while (pf->name) {
        SimpleInstance *fInst = new SimpleInstance(meta->functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code);
    
        InstanceMember *m = new InstanceMethod(fInst);
        meta->defineInstanceMember(meta->errorClass, &meta->cxt, &meta->world.identifiers[pf->name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, m, 0);
        pf++;
    }

    initErrorClass(meta, meta->errorClass, Error_Constructor);

    initErrorClass(meta, meta->evalErrorClass, EvalError_Constructor);
    initErrorClass(meta, meta->rangeErrorClass, RangeError_Constructor);
    initErrorClass(meta, meta->referenceErrorClass, ReferenceError_Constructor);
    initErrorClass(meta, meta->syntaxErrorClass, SyntaxError_Constructor);
    initErrorClass(meta, meta->typeErrorClass, TypeError_Constructor);
    initErrorClass(meta, meta->uriErrorClass, UriError_Constructor);


}

}
}
