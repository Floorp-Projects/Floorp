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

#include <assert.h>
#include <ctype.h>

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
#include "jslong.h"
#include "fdlibm_ns.h"
#include "prmjtime.h"

namespace JavaScript {    
namespace MetaData {

    void RegExpInstance::setLastIndex(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->writePublic(meta, OBJECT_TO_JS2VAL(this), meta->regexpClass, meta->engine->allocStringPtr("lastIndex"), true, a);
    }
    void RegExpInstance::setGlobal(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->writePublic(meta, OBJECT_TO_JS2VAL(this), meta->regexpClass, meta->engine->allocStringPtr("global"), true, a);
    }
    void RegExpInstance::setMultiline(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->writePublic(meta, OBJECT_TO_JS2VAL(this), meta->regexpClass, meta->engine->allocStringPtr("multiline"), true, a);
    }
    void RegExpInstance::setIgnoreCase(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->writePublic(meta, OBJECT_TO_JS2VAL(this), meta->regexpClass, meta->engine->allocStringPtr("ignoreCase"), true, a);
    }
    void RegExpInstance::setSource(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->writePublic(meta, OBJECT_TO_JS2VAL(this), meta->regexpClass, meta->engine->allocStringPtr("source"), true, a);
    }

    js2val RegExpInstance::getLastIndex(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->readPublic(meta, &thisVal, meta->regexpClass, meta->engine->allocStringPtr("lastIndex"), RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getGlobal(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->readPublic(meta, &thisVal, meta->regexpClass, meta->engine->allocStringPtr("global"), RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getMultiline(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->readPublic(meta, &thisVal, meta->regexpClass, meta->engine->allocStringPtr("multiline"), RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getIgnoreCase(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->readPublic(meta, &thisVal, meta->regexpClass, meta->engine->allocStringPtr("ignoreCase"), RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getSource(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->readPublic(meta, &thisVal, meta->regexpClass, meta->engine->allocStringPtr("source"), RunPhase, &r))
            ASSERT(false);
        return r;
    }

    
    js2val RegExp_toString(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue))->type != meta->regexpClass))
            meta->reportError(Exception::typeError, "RegExp.toString can only be applied to RegExp objects", meta->engine->errorPos());
        RegExpInstance *thisInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(thisValue));
        js2val srcval = thisInst->getSource(meta);
        ASSERT(JS2VAL_IS_STRING(srcval));
        const String *s = JS2VAL_TO_STRING(srcval);
        String result = "/" + *s + "/";
        js2val flagVal = thisInst->getGlobal(meta);
        ASSERT(JS2VAL_IS_BOOLEAN(flagVal));
        if (JS2VAL_TO_BOOLEAN(flagVal))
            result += "g";
        flagVal = thisInst->getIgnoreCase(meta);
        ASSERT(JS2VAL_IS_BOOLEAN(flagVal));
        if (JS2VAL_TO_BOOLEAN(flagVal))
            result += "i";
        flagVal = thisInst->getMultiline(meta);
        ASSERT(JS2VAL_IS_BOOLEAN(flagVal));
        if (JS2VAL_TO_BOOLEAN(flagVal))
            result += "m";
        return meta->engine->allocString(result);
    }

    js2val RegExp_exec(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue))->type != meta->regexpClass))
            meta->reportError(Exception::typeError, "RegExp.exec can only be applied to RegExp objects", meta->engine->errorPos());
        RegExpInstance *thisInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(thisValue));
   
        js2val result = JS2VAL_NULL;
        if (argc > 0) {
            uint32 index = 0;

            const String *str = meta->toString(argv[0]);
            js2val globalMultiline = thisInst->getMultiline(meta);

            if (meta->toBoolean(thisInst->getGlobal(meta))) {
                js2val lastIndexVal = thisInst->getLastIndex(meta);
                float64 lastIndex = meta->toFloat64(lastIndexVal);
                if ((lastIndex < 0) || (lastIndex > str->length())) {
                    thisInst->setLastIndex(meta, meta->engine->allocNumber(0.0));
                    return result;
                }
                index = meta->engine->float64toUInt32(lastIndex);
            }
            REMatchResult *match = REExecute(meta, thisInst->mRegExp, str->begin(), index, toUInt32(str->length()), meta->toBoolean(globalMultiline));
            if (match) {
                ArrayInstance *A = new ArrayInstance(meta, meta->arrayClass->prototype, meta->arrayClass);
                DEFINE_ROOTKEEPER(rk, A);
                result = OBJECT_TO_JS2VAL(A);
                js2val matchStr = meta->engine->allocString(str->substr((uint32)match->startIndex, (uint32)match->endIndex - match->startIndex));
                meta->createDynamicProperty(A, meta->engine->numberToString((long)0), matchStr, ReadWriteAccess, false, true);
                for (int32 i = 0; i < match->parenCount; i++) {
                    if (match->parens[i].index != -1) {
                        js2val parenStr = meta->engine->allocString(str->substr((uint32)(match->parens[i].index), (uint32)(match->parens[i].length)));
                        meta->createDynamicProperty(A, meta->engine->numberToString(i + 1), parenStr, ReadWriteAccess, false, true);
                    }
		            else
                        meta->createDynamicProperty(A, meta->engine->numberToString(i + 1), JS2VAL_UNDEFINED, ReadWriteAccess, false, true);
                }
                setLength(meta, A, match->parenCount + 1);

                meta->createDynamicProperty(A, meta->engine->allocStringPtr("index"), meta->engine->allocNumber((float64)(match->startIndex)), ReadWriteAccess, false, true);
                meta->createDynamicProperty(A, meta->engine->allocStringPtr("input"), meta->engine->allocString(str), ReadWriteAccess, false, true);
                
                meta->stringClass->writePublic(meta, OBJECT_TO_JS2VAL(meta->regexpClass), meta->stringClass, meta->engine->allocStringPtr("lastMatch"), true, matchStr);
                js2val leftContextVal = meta->engine->allocString(str->substr(0, (uint32)match->startIndex));
                meta->stringClass->writePublic(meta, OBJECT_TO_JS2VAL(meta->regexpClass), meta->stringClass, meta->engine->allocStringPtr("leftContext"), true, matchStr);
                js2val rightContextVal = meta->engine->allocString(str->substr((uint32)match->endIndex, (uint32)str->length() - match->endIndex));
                meta->stringClass->writePublic(meta, OBJECT_TO_JS2VAL(meta->regexpClass), meta->stringClass, meta->engine->allocStringPtr("rightContext"), true, matchStr);
                
                if (meta->toBoolean(thisInst->getGlobal(meta))) {
                    index = match->endIndex;
                    thisInst->setLastIndex(meta, meta->engine->allocNumber((float64)index));
                }

            }

        }
        return result;
    }

    js2val RegExp_Call(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        if ((argc > 0)
                && (JS2VAL_IS_OBJECT(argv[0]) 
                && (JS2VAL_TO_OBJECT(argv[0])->kind == SimpleInstanceKind)
                && (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(argv[0]))->type == meta->regexpClass))
                && ((argc == 1) || JS2VAL_IS_UNDEFINED(argv[1])))
            return argv[0];
        else
            return RegExp_Constructor(meta, thisValue, argv, argc);
    }

    js2val RegExp_Constructor(JS2Metadata *meta, const js2val /* thisValue */, js2val *argv, uint32 argc)
    {
        RegExpInstance *thisInst = new RegExpInstance(meta, meta->regexpClass->prototype, meta->regexpClass);
        DEFINE_ROOTKEEPER(rk, thisInst);
        js2val thatValue = OBJECT_TO_JS2VAL(thisInst);
        uint32 flags = 0;

        const String *regexpStr = meta->engine->Empty_StringAtom;
        DEFINE_ROOTKEEPER(rk1, regexpStr);
        const String *flagStr = meta->engine->Empty_StringAtom;
        DEFINE_ROOTKEEPER(rk2, flagStr);
        if (argc > 0) {
            if (JS2VAL_IS_OBJECT(argv[0]) 
                    && (JS2VAL_TO_OBJECT(argv[0])->kind == SimpleInstanceKind)
                    && (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(argv[0]))->type == meta->regexpClass)) {
                if ((argc == 1) || JS2VAL_IS_UNDEFINED(argv[1])) {
                    RegExpInstance *otherInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(argv[0]));
                    js2val src  = otherInst->getSource(meta);
                    ASSERT(JS2VAL_IS_STRING(src));
                    regexpStr = JS2VAL_TO_STRING(src);
                    flags = otherInst->mRegExp->flags;
                }
                else
                    meta->reportError(Exception::typeError, "Illegal RegExp constructor args", meta->engine->errorPos());
            }
            else
                regexpStr = meta->toString(argv[0]);
            if ((argc > 1) && !JS2VAL_IS_UNDEFINED(argv[1])) {
                flagStr = meta->toString(argv[1]);
                if (!parseFlags(meta, flagStr->begin(), (int32)flagStr->length(), &flags))
                    meta->reportError(Exception::syntaxError, "Failed to parse RegExp : '{0}'", meta->engine->errorPos(), *regexpStr + "/" + *flagStr);  // XXX error message?
            }
        }
        JSRegExp *re = RECompile(meta, regexpStr->begin(), (int32)regexpStr->length(), flags);
        if (re) {
            thisInst->mRegExp = re;
            // XXX ECMA spec says these are DONTENUM
            thisInst->setSource(meta, STRING_TO_JS2VAL(regexpStr));
            thisInst->setGlobal(meta, BOOLEAN_TO_JS2VAL((re->flags & JSREG_GLOB) == JSREG_GLOB));
            thisInst->setIgnoreCase(meta, BOOLEAN_TO_JS2VAL((re->flags & JSREG_FOLD) == JSREG_FOLD));
            thisInst->setLastIndex(meta, INT_TO_JS2VAL(0));
            thisInst->setMultiline(meta, BOOLEAN_TO_JS2VAL((re->flags & JSREG_MULTILINE) == JSREG_MULTILINE));
        }
        else
            meta->reportError(Exception::syntaxError, "Failed to parse RegExp : '{0}'", meta->engine->errorPos(), "/" + *regexpStr + "/" + *flagStr);  // XXX what about the RE parser error message?
        return thatValue;
    }

    void initRegExpObject(JS2Metadata *meta)
    {

        FunctionData prototypeFunctions[] =
        {
            { "toString",            0, RegExp_toString },
            { "exec",                0, RegExp_exec  },
            { NULL }
        };

        meta->initBuiltinClass(meta->regexpClass, NULL, RegExp_Constructor, RegExp_Call);
        meta->env->addFrame(meta->regexpClass);
        {
            Variable *v = new Variable(meta->stringClass, meta->engine->allocString(""), false);
            meta->defineLocalMember(meta->env, &meta->world.identifiers["lastMatch"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
            v = new Variable(meta->stringClass, meta->engine->allocString(""), false);
            meta->defineLocalMember(meta->env, &meta->world.identifiers["leftContext"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
            v = new Variable(meta->stringClass, meta->engine->allocString(""), false);
            meta->defineLocalMember(meta->env, &meta->world.identifiers["rightContext"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
        }
        meta->env->removeTopFrame();
        
        NamespaceList publicNamespaceList;
        publicNamespaceList.push_back(meta->publicNamespace);

#define INSTANCE_VAR_COUNT (5)

        struct {
            char *name;
            JS2Class *type;
        } RegExpInstanceVars[INSTANCE_VAR_COUNT] = {
            { "source", meta->stringClass }, 
            { "global", meta->booleanClass }, 
            { "multiline", meta->booleanClass }, 
            { "ignoreCase", meta->booleanClass }, 
            { "lastIndex", meta->numberClass },         
        };

        for (uint32 i = 0; i < INSTANCE_VAR_COUNT; i++)
        {
            Multiname *mn = new Multiname(meta->engine->allocStringPtr(RegExpInstanceVars[i].name), &publicNamespaceList);
            InstanceMember *m = new InstanceVariable(mn, RegExpInstanceVars[i].type, false, true, true, meta->regexpClass->slotCount++);
            meta->defineInstanceMember(meta->regexpClass, &meta->cxt, mn->name, *mn->nsList, Attribute::NoOverride, false, m, 0);
        }

        RegExpInstance *reProto = new RegExpInstance(meta, OBJECT_TO_JS2VAL(meta->objectClass->prototype), meta->regexpClass);
        DEFINE_ROOTKEEPER(rk, reProto);
        meta->regexpClass->prototype = OBJECT_TO_JS2VAL(reProto);   
        meta->initBuiltinClassPrototype(meta->regexpClass, &prototypeFunctions[0]);

        reProto->setSource(meta, meta->engine->allocString(""));
        reProto->setGlobal(meta, JS2VAL_FALSE);
        reProto->setIgnoreCase(meta, JS2VAL_FALSE);
        reProto->setLastIndex(meta, INT_TO_JS2VAL(0));
        reProto->setMultiline(meta, JS2VAL_FALSE);
    
    }

}
}
