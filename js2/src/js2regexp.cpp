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
        meta->regexpClass->WritePublic(meta, OBJECT_TO_JS2VAL(this), meta->world.identifiers["lastIndex"], true, a);
    }
    void RegExpInstance::setGlobal(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->WritePublic(meta, OBJECT_TO_JS2VAL(this), meta->world.identifiers["global"], true, a);
    }
    void RegExpInstance::setMultiline(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->WritePublic(meta, OBJECT_TO_JS2VAL(this), meta->world.identifiers["multiline"], true, a);
    }
    void RegExpInstance::setIgnoreCase(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->WritePublic(meta, OBJECT_TO_JS2VAL(this), meta->world.identifiers["ignoreCase"], true, a);
    }
    void RegExpInstance::setSource(JS2Metadata *meta, js2val a)
    {
        meta->regexpClass->WritePublic(meta, OBJECT_TO_JS2VAL(this), meta->world.identifiers["source"], true, a);
    }

    js2val RegExpInstance::getLastIndex(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->ReadPublic(meta, &thisVal, meta->world.identifiers["lastIndex"], RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getGlobal(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->ReadPublic(meta, &thisVal, meta->world.identifiers["global"], RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getMultiline(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->ReadPublic(meta, &thisVal, meta->world.identifiers["multiline"], RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getIgnoreCase(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->ReadPublic(meta, &thisVal, meta->world.identifiers["ignoreCase"], RunPhase, &r))
            ASSERT(false);
        return r;
    }
    js2val RegExpInstance::getSource(JS2Metadata *meta)
    {
        js2val r;
        js2val thisVal = OBJECT_TO_JS2VAL(this);
        if (!meta->regexpClass->ReadPublic(meta, &thisVal, meta->world.identifiers["source"], RunPhase, &r))
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

    js2val RegExp_exec_sub(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc, bool test)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue))->type != meta->regexpClass))
            meta->reportError(Exception::typeError, "RegExp.exec can only be applied to RegExp objects", meta->engine->errorPos());
        RegExpInstance *thisInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(thisValue));
   
        const String *str = NULL;
        DEFINE_ROOTKEEPER(meta, rk0, str);

        js2val regexpClassVal = OBJECT_TO_JS2VAL(meta->regexpClass);
        js2val result = JS2VAL_NULL;
        if (argc == 0) {
            js2val inputVal;
            if (!meta->classClass->ReadPublic(meta, &regexpClassVal, meta->world.identifiers["input"], RunPhase, &inputVal))
                ASSERT(false);
            str = meta->toString(inputVal);
        }
        else
            str = meta->toString(argv[0]);

        uint32 index = 0;
        js2val globalMultiline;
        if (!meta->classClass->ReadPublic(meta, &regexpClassVal, meta->world.identifiers["multiline"], RunPhase, &globalMultiline))
            ASSERT(false);

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
            if (meta->toBoolean(thisInst->getGlobal(meta))) {
                index = match->endIndex;
                thisInst->setLastIndex(meta, meta->engine->allocNumber((float64)index));
            }
// construct the result array and set $1.. in RegExp statics
            ArrayInstance *A = NULL;
            DEFINE_ROOTKEEPER(meta, rk, A);
            if (test)
                result = JS2VAL_TRUE;
            else {
                A = new (meta) ArrayInstance(meta, meta->arrayClass->prototype, meta->arrayClass);
                result = OBJECT_TO_JS2VAL(A);
            }
            js2val matchStr = meta->engine->allocString(str->substr((uint32)match->startIndex, (uint32)match->endIndex - match->startIndex));
            DEFINE_ROOTKEEPER(meta, rk1, matchStr);
            js2val inputStr = meta->engine->allocString(str);
            DEFINE_ROOTKEEPER(meta, rk2, inputStr);
            if (!test)
                meta->createDynamicProperty(A, meta->engine->numberToStringAtom((uint32)0), matchStr, ReadWriteAccess, false, true);
            js2val parenStr = JS2VAL_VOID;
            DEFINE_ROOTKEEPER(meta, rk3, parenStr);
            if (match->parenCount == 0) // arrange to set the lastParen to "", not undefined (it's a non-ecma 1.2 thing)
                parenStr = meta->engine->allocString("");
            for (uint32 i = 0; i < match->parenCount; i++) {
                if (match->parens[i].index != -1) {
                    parenStr = meta->engine->allocString(str->substr((uint32)(match->parens[i].index), (uint32)(match->parens[i].length)));
                    if (!test)
                        meta->createDynamicProperty(A, meta->engine->numberToStringAtom(toUInt32(i + 1)), parenStr, ReadWriteAccess, false, true);
                    if (i < 9) { // 0-->8 maps to $1 thru $9
                        char name[3] = "$0";
                        name[1] = '1' + i;
                        String staticName(widenCString(name));
                        meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers[staticName], true, parenStr);
                    }
                }
                else {
                    if (!test)
                        meta->createDynamicProperty(A, meta->engine->numberToStringAtom(toUInt32(i + 1)), JS2VAL_UNDEFINED, ReadWriteAccess, false, true);
                    if (i < 9) { // 0-->8 maps to $1 thru $9
                        char name[3] = "$0";
                        name[1] = '1' + i;
                        String staticName(widenCString(name));
                        meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers[staticName], true, JS2VAL_UNDEFINED);
                    }
                }
            }
            if (!test) {
                setLength(meta, A, match->parenCount + 1);                
        
// add 'index' and 'input' properties to the result array
                meta->createDynamicProperty(A, meta->world.identifiers["index"], meta->engine->allocNumber((float64)(match->startIndex)), ReadWriteAccess, false, true);
                meta->createDynamicProperty(A, meta->world.identifiers["input"], inputStr, ReadWriteAccess, false, true);
            }                
// set other RegExp statics
            meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers["input"], true, inputStr);
            meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers["lastMatch"], true, matchStr);
            meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers["lastParen"], true, parenStr);
            js2val leftContextVal = meta->engine->allocString(str->substr(0, (uint32)match->startIndex));
            DEFINE_ROOTKEEPER(meta, rk4, leftContextVal);
            meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers["leftContext"], true, leftContextVal);
            js2val rightContextVal = meta->engine->allocString(str->substr((uint32)match->endIndex, (uint32)str->length() - match->endIndex));
            DEFINE_ROOTKEEPER(meta, rk5, rightContextVal);
            meta->classClass->WritePublic(meta, regexpClassVal, meta->world.identifiers["rightContext"], true, rightContextVal);
            free(match);
        }
        return result;
    }
    js2val RegExp_exec(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        return RegExp_exec_sub(meta, thisValue, argv, argc, false);
    }

    js2val RegExp_test(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        js2val result = RegExp_exec_sub(meta, thisValue, argv, argc, true);
        if (result != JS2VAL_TRUE)
            result = JS2VAL_FALSE;
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

    js2val RegExp_compile_sub(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc, bool flat)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue))->type != meta->regexpClass))
            meta->reportError(Exception::typeError, "RegExp.compile can only be applied to RegExp objects", meta->engine->errorPos());
        RegExpInstance *thisInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(thisValue));
        uint32 flags = 0;

        const String *regexpStr = meta->engine->allocStringPtr("");
        DEFINE_ROOTKEEPER(meta, rk1, regexpStr);
        const String *flagStr = meta->engine->allocStringPtr("");
        DEFINE_ROOTKEEPER(meta, rk2, flagStr);
        if (argc > 0) {
            if (JS2VAL_IS_OBJECT(argv[0]) && !JS2VAL_IS_NULL(argv[0])
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
        JS2RegExp *re = RECompile(meta, regexpStr->begin(), (int32)regexpStr->length(), flags, flat);
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
        return thisValue;
    }

    js2val RegExp_compile(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        return RegExp_compile_sub(meta, thisValue, argv, argc, false);
    }

    js2val RegExp_ConstructorOpt(JS2Metadata *meta, const js2val /* thisValue */, js2val *argv, uint32 argc, bool flat)
    {
        RegExpInstance *thisInst = new (meta) RegExpInstance(meta, meta->regexpClass->prototype, meta->regexpClass);
        DEFINE_ROOTKEEPER(meta, rk, thisInst);
        js2val thatValue = OBJECT_TO_JS2VAL(thisInst);
        return RegExp_compile_sub(meta, thatValue, argv, argc, flat);
    }

    js2val RegExp_Constructor(JS2Metadata *meta, const js2val /* thisValue */, js2val *argv, uint32 argc)
    {
        RegExpInstance *thisInst = new (meta) RegExpInstance(meta, meta->regexpClass->prototype, meta->regexpClass);
        DEFINE_ROOTKEEPER(meta, rk, thisInst);
        js2val thatValue = OBJECT_TO_JS2VAL(thisInst);
		return RegExp_compile_sub(meta, thatValue, argv, argc, false);
    }

    void initRegExpObject(JS2Metadata *meta)
    {
		uint32 i;

        FunctionData prototypeFunctions[] =
        {
            { "toString",            0, RegExp_toString },
            { "exec",                0, RegExp_exec  },
            { "test",                0, RegExp_test  },
            { "compile",             0, RegExp_compile  },
            { NULL }
        };


#define STATIC_VAR_COUNT (15)

        struct {
            char *name;
            char *aliasName;
            JS2Class *type;
        } RegExpStaticVars[STATIC_VAR_COUNT] = {
            { "input", "$_", meta->stringClass }, 
            { "multiline", "$*", meta->booleanClass }, 
            { "lastMatch", "$&", meta->stringClass },         
            { "lastParen", "$+", meta->objectClass },         
            { "leftContext", "$`", meta->stringClass },         
            { "rightContext", "$'", meta->stringClass },  
            
            { "$1", NULL, meta->objectClass },         
            { "$2", NULL, meta->objectClass },         
            { "$3", NULL, meta->objectClass },         
            { "$4", NULL, meta->objectClass },         
            { "$5", NULL, meta->objectClass },         
            { "$6", NULL, meta->objectClass },         
            { "$7", NULL, meta->objectClass },         
            { "$8", NULL, meta->objectClass },         
            { "$9", NULL, meta->objectClass },         
        };

        meta->initBuiltinClass(meta->regexpClass, NULL, RegExp_Constructor, RegExp_Call);
        meta->env->addFrame(meta->regexpClass);
        for (i = 0; i < STATIC_VAR_COUNT; i++)
        {
            Variable *v = new Variable();
            v->type = RegExpStaticVars[i].type;
            if (RegExpStaticVars[i].type == meta->stringClass)
                v->value = meta->engine->allocString("");
            else
                if (RegExpStaticVars[i].type == meta->booleanClass)
                    v->value = JS2VAL_FALSE;
            meta->defineLocalMember(meta->env, meta->world.identifiers[RegExpStaticVars[i].name], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
            if (RegExpStaticVars[i].aliasName)
                meta->defineLocalMember(meta->env, meta->world.identifiers[RegExpStaticVars[i].aliasName], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
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

        for (i = 0; i < INSTANCE_VAR_COUNT; i++)
        {
            Multiname *mn = new (meta) Multiname(meta->world.identifiers[RegExpInstanceVars[i].name], &publicNamespaceList);
            InstanceMember *m = new InstanceVariable(mn, RegExpInstanceVars[i].type, false, true, true, meta->regexpClass->slotCount++);
            meta->defineInstanceMember(meta->regexpClass, &meta->cxt, mn->name, *mn->nsList, Attribute::NoOverride, false, m, 0);
        }

        RegExpInstance *reProto = new (meta) RegExpInstance(meta, OBJECT_TO_JS2VAL(meta->objectClass->prototype), meta->regexpClass);
        DEFINE_ROOTKEEPER(meta, rk, reProto);
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
