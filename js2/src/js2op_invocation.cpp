
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
* Communications Corporation.   Portions created by Netscape are
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


    case eNew:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            a = top(argCount + 1);
            ASSERT(JS2VAL_IS_OBJECT(a) && !JS2VAL_IS_NULL(a));
            JS2Object *obj = JS2VAL_TO_OBJECT(a);
            if (obj->kind == ClassKind) {
                JS2Class *c = checked_cast<JS2Class *>(obj);
                a = c->construct(meta, JS2VAL_NULL, base(argCount), argCount);
                pop(argCount + 1);
                push(a);
            }
            else {
                FunctionWrapper *fWrap = NULL;
                if ((obj->kind == SimpleInstanceKind)
                            && (meta->objectType(a) == meta->functionClass)) {
                    fWrap = (checked_cast<SimpleInstance *>(obj))->fWrap;
                }
                else
                    if ((obj->kind == PrototypeInstanceKind)
                            && ((checked_cast<PrototypeInstance *>(obj))->type == meta->functionClass)) {
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
                        js2val protoVal;
                        JS2Object *protoObj = meta->objectClass->prototype;
                        Multiname mn(prototype_StringAtom);     // gc safe because the content is rooted elsewhere
                        LookupKind lookup(true, JS2VAL_NULL);   // make it a lexical lookup since we want it to
                                                                // fail if 'prototype' hasn't been defined
                                                                // XXX (prototype should always exist for functions)
                        if (meta->readProperty(&a, &mn, &lookup, RunPhase, &protoVal)) {
                            if (!JS2VAL_IS_OBJECT(protoVal))
                                meta->reportError(Exception::badValueError, "Non-object prototype value", errorPos());
                            protoObj = JS2VAL_TO_OBJECT(protoVal);
                        }

                        if (fWrap->code) {  // native code, pass pointer to argument base
                            a = fWrap->code(meta, a, base(argCount), argCount);
                            meta->env->removeTopFrame();
                            pop(argCount + 1);
                            push(a);
                        }
                        else {
                            ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                            runtimeFrame->instantiate(meta->env);
                            PrototypeInstance *pInst = new PrototypeInstance(protoObj, meta->objectClass);
                            baseVal = OBJECT_TO_JS2VAL(pInst);
                            runtimeFrame->thisObject = baseVal;
                            runtimeFrame->assignArguments(base(argCount), argCount);
                            jsr(phase, fWrap->bCon, base(argCount + 1), baseVal);   // seems out of order, but we need to catch the current top frame 
                            meta->env->addFrame(runtimeFrame);
                        }
                    }
                    else
                        meta->reportError(Exception::typeError, "object is not a constructor", errorPos());
//                }
//                else
//                    meta->reportError(Exception::typeError, "object is not a constructor", errorPos());
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
                meta->reportError(Exception::badValueError, "Can't call on primitive value", errorPos());
            JS2Object *fObj = JS2VAL_TO_OBJECT(b);
            FunctionWrapper *fWrap = NULL;
            if ((fObj->kind == SimpleInstanceKind)
                        && (meta->objectType(b) == meta->functionClass)) {
                fWrap = (checked_cast<SimpleInstance *>(fObj))->fWrap;
            }
            else
                if ((fObj->kind == PrototypeInstanceKind)
                        && ((checked_cast<PrototypeInstance *>(fObj))->type == meta->functionClass)) {
                    fWrap = (checked_cast<FunctionInstance *>(fObj))->fWrap;
                }
            if (fWrap) {

                if (fWrap->compileFrame->prototype) {
                    if (JS2VAL_IS_VOID(a) || JS2VAL_IS_NULL(a)) {
                        Frame *g = meta->env->getPackageOrGlobalFrame();
                        if (g->kind == GlobalObjectKind)
                            a = OBJECT_TO_JS2VAL(g);
                    }
                }

                if (fWrap->code) {  // native code
                    a = fWrap->code(meta, a, base(argCount), argCount);
                    pop(argCount + 2);
                    push(a);
                }
                else {
                    ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                    runtimeFrame->instantiate(meta->env);
                    runtimeFrame->thisObject = a;
    //                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                    // XXX
                    runtimeFrame->assignArguments(base(argCount), argCount);
                    jsr(phase, fWrap->bCon, base(argCount + 2), JS2VAL_VOID);   // seems out of order, but we need to catch the current top frame 
                    meta->env->addFrame(runtimeFrame);
                }
            }
            else
            if (fObj->kind == MethodClosureKind) {  // XXX I made this up (particularly the frame push of the objectType)
                MethodClosure *mc = checked_cast<MethodClosure *>(fObj);
                SimpleInstance *fInst = mc->method->fInst;
                FunctionWrapper *fWrap = fInst->fWrap;

                if (fWrap->code) {
                    a = fWrap->code(meta, mc->thisObject, base(argCount), argCount);
                    pop(argCount + 2);
                    push(a);
                }
                else {
                    ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                    runtimeFrame->instantiate(meta->env);
                    runtimeFrame->thisObject = mc->thisObject;
//                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                    jsr(phase, fWrap->bCon, base(argCount + 2), JS2VAL_VOID);   // seems out of order, but we need to catch the current top frame 
                    meta->env->addFrame(meta->objectType(mc->thisObject));
                    meta->env->addFrame(runtimeFrame);
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
                meta->reportError(Exception::badValueError, "Un-callable object", errorPos());
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
            Frame *f = bCon->mFrameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            meta->env->addFrame(f);
            f->instantiate(meta->env);
        }
        break;

    case ePopFrame: 
        {
            meta->env->removeTopFrame();
        }
        break;

    case eIs:
        {
            a = pop();  // catch variable type
            b = pop();  // exception object
            JS2Class *c = meta->objectType(b);
            if (!JS2VAL_IS_OBJECT(a))
                meta->reportError(Exception::badValueError, "Type expected", errorPos());
            JS2Object *obj = JS2VAL_TO_OBJECT(a);
            if (obj->kind != ClassKind)
                 meta->reportError(Exception::badValueError, "Type expected", errorPos());
            JS2Class *isClass = checked_cast<JS2Class *>(obj);
            push(c == isClass);
        }
        break;

    case eTypeof:
        {
            a = pop();
            if (JS2VAL_IS_UNDEFINED(a))
                a = STRING_TO_JS2VAL(undefined_StringAtom);
            else
            if (JS2VAL_IS_BOOLEAN(a))
                a = allocString("boolean");
            else
            if (JS2VAL_IS_NUMBER(a))
                a = allocString("number");
            else
            if (JS2VAL_IS_STRING(a))
                a = allocString("string");
            else {
                ASSERT(JS2VAL_IS_OBJECT(a));
                if (JS2VAL_IS_NULL(a))
                    a = STRING_TO_JS2VAL(object_StringAtom);
                JS2Object *obj = JS2VAL_TO_OBJECT(a);
                switch (obj->kind) {
                case MultinameKind:
                    a = allocString("namespace"); 
                    break;
                case AttributeObjectKind:
                    a = allocString("attribute"); 
                    break;
                case ClassKind:
                case MethodClosureKind:
                    a = STRING_TO_JS2VAL(Function_StringAtom); 
                    break;
                case PrototypeInstanceKind:
                    a = STRING_TO_JS2VAL(checked_cast<PrototypeInstance *>(obj)->type->getName());
                    break;
                case PackageKind:
                case GlobalObjectKind:
                    a = STRING_TO_JS2VAL(object_StringAtom);
                    break;
                case SimpleInstanceKind:
                    a = STRING_TO_JS2VAL(checked_cast<SimpleInstance *>(obj)->type->getName());
                    break;
                }
            }
            push(a);
        }
        break;

    case eCoerce:
        {
            JS2Class *c = BytecodeContainer::getType(pc);
            pc += sizeof(JS2Class *);
            a = pop();
            push(c->implicitCoerce(meta, a));
        }
        break;
