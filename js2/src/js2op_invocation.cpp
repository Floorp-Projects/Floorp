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


    case eNew:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            a = top(argCount + 1);
            if (!JS2VAL_IS_OBJECT(a) || JS2VAL_IS_NULL(a)) {
                meta->reportError(Exception::badValueError, "object is not a constructor", errorPos());
            }
            JS2Object *obj = JS2VAL_TO_OBJECT(a);
            if (obj->kind == ClassKind) {
                JS2Class *c = checked_cast<JS2Class *>(obj);
                a = c->construct(meta, a, base(argCount), argCount);
                pop(argCount + 1);
                push(a);
            }
            else {
                FunctionWrapper *fWrap = NULL;
                if ((obj->kind == SimpleInstanceKind)
                            && (meta->objectType(a) == meta->functionClass)) {
                    fWrap = (checked_cast<FunctionInstance *>(obj))->fWrap;
                }
                if (fWrap) {
                    // XXX - I made this stuff up - extract the 'prototype' property from
                    // the function being invoked (defaulting to Object.prototype). Then 
                    // construct a new prototypeInstance, setting the acquired prototype
                    // parent. Finally invoke the function, but insert the constructed
                    // object at the bottom of the stack to be the 'return' value. 
                    // XXX this won't last - if a non-primitive is returned from the function,
                    // it's supposed to supplant the constructed object. XXX and I think the
                    // stack is out of balance anyway...
                    js2val protoVal = OBJECT_TO_JS2VAL(meta->objectClass->prototype);
                    Multiname mn(prototype_StringAtom);     // gc safe because the content is rooted elsewhere
                    JS2Class *limit = meta->objectType(a);
                    if (limit->Read(meta, &a, &mn, meta->env, RunPhase, &protoVal)) {
                        if (!JS2VAL_IS_OBJECT(protoVal))
                            meta->reportError(Exception::badValueError, "Non-object prototype value", errorPos());
                    }
                    uint32 length = fWrap->length;
                    if (fWrap->code) {  // native code, pass pointer to argument base
                        while (argCount < length) {
                            push(JS2VAL_UNDEFINED);
                            argCount++;
                        }
                        a = fWrap->code(meta, a, base(argCount), argCount);
                        pop(argCount + 1);
                        push(a);
                    }
                    else {
                        pFrame = fWrap->compileFrame;
                        baseVal = OBJECT_TO_JS2VAL(new (meta) SimpleInstance(meta, protoVal, meta->objectType(protoVal)));
                        pFrame->thisObject = baseVal;
                        pFrame->assignArguments(meta, obj, base(argCount), argCount);
                        jsr(phase, fWrap->bCon, base(argCount + 1) - execStack, baseVal, fWrap->env);
						parameterSlots = pFrame->argSlots;
						parameterCount = pFrame->argCount;
                        meta->env->addFrame(pFrame);
                        parameterFrame = pFrame;
                        pFrame = NULL;
                    }
                }
                else
                    meta->reportError(Exception::typeError, "object is not a constructor", errorPos());
            }
        }
        break;

    case eCall:
        {
            // XXX Remove the arguments from the stack, (native calls have done this already)
            // (important that they're tracked for gc in any mechanism)
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            a = top(argCount + 2);                  // 'this'
            b = top(argCount + 1);                  // target function
            if (JS2VAL_IS_PRIMITIVE(b))
                meta->reportError(Exception::badValueError, "{0} is not a function", errorPos(), JS2VAL_TO_STRING(typeofString(b)));
            JS2Object *fObj = JS2VAL_TO_OBJECT(b);
            FunctionWrapper *fWrap = NULL;
            if ((fObj->kind == SimpleInstanceKind)
                        && (meta->objectType(b) == meta->functionClass)) {
doCall:
                FunctionInstance *fInst = checked_cast<FunctionInstance *>(fObj);
                fWrap = fInst->fWrap;
                if (fInst->isMethodClosure) {
                    a = fInst->thisObject;
                }
                if (fWrap->compileFrame->prototype) {
                    if (JS2VAL_IS_VOID(a) || JS2VAL_IS_NULL(a)) {
                        Frame *g = meta->env->getPackageFrame();
                        a = OBJECT_TO_JS2VAL(g);
                    }
                }
                // XXX ok to not use getLength(meta, fObj) ?
                uint32 length = fWrap->length;
                if (fWrap->code || fWrap->alien) {  // native code
                    uint16 argc = argCount;
                    while (argCount < length) {
                        push(JS2VAL_UNDEFINED);
                        argCount++;
                    }
                    jsr(phase, NULL, base(argCount + 2) - execStack, JS2VAL_VOID, fWrap->env);
                    if (fWrap->alien)
                        a = fWrap->alien(meta, fInst, a, base(argCount), argc);
                    else
                        a = fWrap->code(meta, a, base(argCount), argc);
                    rts();
                    push(a);
                }
                else {
                    if (length || fInst->isMethodClosure || fWrap->compileFrame->buildArguments) {
                        pFrame = fWrap->compileFrame;
                        pFrame->thisObject = a;
                        // XXX (use fWrap->compileFrame->signature)
                        pFrame->assignArguments(meta, fObj, base(argCount), argCount);
                        jsr(phase, fWrap->bCon, base(argCount + 2) - execStack, JS2VAL_VOID, fWrap->env);
						parameterSlots = pFrame->argSlots;
						parameterCount = pFrame->argCount;
                        if (fInst->isMethodClosure)
                            meta->env->addFrame(meta->objectType(a));
                        meta->env->addFrame(pFrame);
                        parameterFrame = pFrame;
                        pFrame = NULL;
                    }
                    else {
                        jsr(phase, fWrap->bCon, base(argCount + 2) - execStack, JS2VAL_VOID, fWrap->env);
                        // need to mark the frame as a runtime frame (see stmtnode::return in validate)
                        pFrame = fWrap->compileFrame;
                        pFrame->pluralFrame = fWrap->compileFrame;
                        meta->env->addFrame(pFrame);
						parameterSlots = NULL;
						parameterCount = 0;
                        pFrame->thisObject = a;
                        parameterFrame = pFrame;
                        pFrame = NULL;
                    }
                }
            }
            else
            if (fObj->kind == ClassKind) {
                JS2Class *c = checked_cast<JS2Class *>(fObj);
                if (c->call)
                    a = c->call(meta, JS2VAL_NULL, base(argCount), argCount);
                else
                    a = JS2VAL_UNDEFINED;
                pop(argCount + 2);
                push(a);
            }
            else
            if ((fObj->kind == SimpleInstanceKind)
                        && (meta->objectType(b) == meta->regexpClass)) {
                // ECMA extension, call exec()
                js2val exec_fnVal;
                if (!meta->regexpClass->ReadPublic(meta, &b, meta->world.identifiers["exec"], RunPhase, &exec_fnVal))
                    ASSERT(false);
				a = b;
                ASSERT(JS2VAL_IS_OBJECT(exec_fnVal));
                fObj = JS2VAL_TO_OBJECT(exec_fnVal);
                goto doCall;
            }
            else
                meta->reportError(Exception::badValueError, "{0} is not a function", errorPos(), JS2VAL_TO_STRING(typeofString(b)));
        }
        break;

    case eSuperCall:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            ParameterFrame *pFrame = meta->env->getEnclosingParameterFrame(&a);
            ASSERT(pFrame && (pFrame->isConstructor));
            if (pFrame->superConstructorCalled)
                meta->reportError(Exception::referenceError, "The superconstructor cannot be called twice", errorPos());
            JS2Class *c = meta->env->getEnclosingClass();
            ASSERT(c);
            ASSERT(!JS2VAL_IS_VOID(a));
            meta->invokeInit(c->super, a, base(argCount), argCount);
            pFrame->superConstructorCalled = true;
        }
        break;

    case ePopv:
        {
            retval = pop();
        }
        break;

    case eVoid:
        {
            pop();
            push(JS2VAL_VOID);
        }
        break;

    case eReturn: 
        {
			retval = top();
			if (!JS2VAL_IS_VOID(activationStackTop[-1].retval))
				// we're in a constructor function, ignore the return value
				// if it's not an object
				if (JS2VAL_IS_OBJECT(retval))
					activationStackTop[-1].retval = retval;
            rts();
            if (pc == NULL) 
                return retval;
            push(retval);
        }
        break;

    case eReturnVoid: 
        {
            rts();
            if (pc == NULL) 
                return retval;
            push(retval);
        }
        break;

    case ePushFrame: 
        {
            Frame *f = checked_cast<Frame *>(bCon->mObjectList[BytecodeContainer::getShort(pc)]);
            pc += sizeof(short);
            if (meta->env->getTopFrame()->kind == ParameterFrameKind)
                localFrame = checked_cast<NonWithFrame *>(f);
            meta->env->addFrame(f);
            f->instantiate(meta->env);
        }
        break;

    case eWithin: 
        {
            a = pop();
            a = meta->toObject(a);
            meta->env->addFrame(new (meta) WithFrame(JS2VAL_TO_OBJECT(a)));
        }
        break;

    case ePopFrame: 
    case eWithout: 
        {
            meta->env->removeTopFrame();
        }
        break;

    case eIs:
        {
            b = pop();
            a = pop();      // doing 'a is b'
            if (!JS2VAL_IS_OBJECT(b))
                meta->reportError(Exception::badValueError, "Type expected", errorPos());
            JS2Object *obj = JS2VAL_TO_OBJECT(b);
            if (obj->kind != ClassKind)
                 meta->reportError(Exception::badValueError, "Type expected", errorPos());
            JS2Class *isClass = checked_cast<JS2Class *>(obj);
            push(isClass->Is(meta, a));
        }
        break;

    case eIn:
        {
            b = pop();
            a = pop();      // doing 'a in b'
            astr = meta->toString(a);
            Multiname mn(meta->world.identifiers[*astr]);
            JS2Class *c = meta->objectType(b);
            bool result = ( (meta->findBaseInstanceMember(c, &mn, ReadAccess) != NULL)
                            || (meta->findBaseInstanceMember(c, &mn, WriteAccess) != NULL)
                            || (meta->findCommonMember(&b, &mn, ReadAccess, false) != NULL)
                            || (meta->findCommonMember(&b, &mn, WriteAccess, false) != NULL));
            push(BOOLEAN_TO_JS2VAL(result));
            astr = NULL;
        }
        break;

    case eInstanceof:   // XXX prototype version
        {
            b = pop();
            a = pop();      // doing 'a instanceof b'
            if (!JS2VAL_IS_OBJECT(b))
                meta->reportError(Exception::typeError, "Object expected for instanceof", errorPos());
            JS2Object *obj = JS2VAL_TO_OBJECT(b);
            js2val a_protoVal;
            js2val b_protoVal;
			JS2Object *aObj = NULL;
            if ((obj->kind == SimpleInstanceKind)
                    && (checked_cast<SimpleInstance *>(obj)->type == meta->functionClass)) {
                // XXX this is [[hasInstance]] from ECMA3
                if (!JS2VAL_IS_OBJECT(a))
                    push(JS2VAL_FALSE);
                else {
                    aObj = JS2VAL_TO_OBJECT(a);
                    if (aObj->kind != SimpleInstanceKind)
                        meta->reportError(Exception::typeError, "Prototype instance expected for instanceof", errorPos());
                    a_protoVal = checked_cast<SimpleInstance *>(aObj)->super;
					{
						Multiname mn(prototype_StringAtom);     // gc safe because the content is rooted elsewhere
						JS2Class *limit = meta->objectType(b);
						if (limit->Read(meta, &b, &mn, meta->env, RunPhase, &b_protoVal)) {
							if (!JS2VAL_IS_OBJECT(b_protoVal))
								meta->reportError(Exception::typeError, "Non-object prototype value in instanceOf", errorPos());
						}
					}
doInstanceOfLoop:
                    bool result = false;
                    while (!JS2VAL_IS_NULL(a_protoVal) && !JS2VAL_IS_UNDEFINED(a_protoVal)) {
                        if (b_protoVal == a_protoVal) {
                            result = true;
                            break;
                        }
                        if (!JS2VAL_IS_OBJECT(a_protoVal))
                            meta->reportError(Exception::typeError, "Non-object prototype value in instanceOf", errorPos());
                        aObj = JS2VAL_TO_OBJECT(a_protoVal);
                        if (aObj->kind != SimpleInstanceKind)
                            meta->reportError(Exception::typeError, "Prototype instance expected for instanceof", errorPos());
                        a_protoVal = checked_cast<SimpleInstance *>(aObj)->super;
                    }
                    push(BOOLEAN_TO_JS2VAL(result));
                }
            }
            else {  // XXX also support a instanceof <<class>> since some of these are
                    // the ECMA3 builtins.
                if (obj->kind == ClassKind) {
                    if (!JS2VAL_IS_OBJECT(a))
                        push(JS2VAL_FALSE);
                    else {
                        aObj = JS2VAL_TO_OBJECT(a);
                        if (aObj->kind == SimpleInstanceKind) 
	                        a_protoVal = checked_cast<SimpleInstance *>(aObj)->super;
						else
                        if (aObj->kind == PackageKind) 
	                        a_protoVal = checked_cast<Package *>(aObj)->super;
						else
						if (aObj->kind == ClassKind)
							a_protoVal = checked_cast<JS2Class *>(aObj)->prototype;
						else
                            meta->reportError(Exception::typeError, "Prototype instance expected for instanceof", errorPos());
                        b_protoVal = checked_cast<JS2Class *>(obj)->prototype;
						goto doInstanceOfLoop;

                    }
                }
                else
                    meta->reportError(Exception::typeError, "Function or Class expected in instanceOf", errorPos());
            }
        }
        break;

    case eTypeof:
        {
            a = pop();
            push(typeofString(a));
        }
        break;

    case eCoerce:
        {
            JS2Class *c = BytecodeContainer::getType(pc);
            pc += sizeof(JS2Class *);
            a = pop();
            push(c->ImplicitCoerce(meta, a));
        }
        break;
