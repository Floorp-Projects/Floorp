
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
            a = top();
            ASSERT(JS2VAL_IS_OBJECT(a) && !JS2VAL_IS_NULL(a));
            JS2Object *obj = JS2VAL_TO_OBJECT(a);
            if (obj->kind == ClassKind) {
                JS2Class *c = checked_cast<JS2Class *>(obj);
                a = c->construct(meta, JS2VAL_NULL, NULL, argCount);
                pop(argCount + 1);
                push(a);
            }
            else {
                if (obj->kind == FixedInstanceKind) {
                    // XXX 
                    js2val protoVal;
                    JS2Object *protoObj = meta->objectClass->prototype;
                    Multiname mn(prototype_StringAtom);
                    LookupKind lookup(false, NULL);
                    if (meta->readProperty(a, &mn, &lookup, RunPhase, &protoVal)) {
                        if (!JS2VAL_IS_OBJECT(protoVal))
                            meta->reportError(Exception::badValueError, "Non-object prototype value", errorPos());
                        protoObj = JS2VAL_TO_OBJECT(protoVal);
                    }

                    FixedInstance *fInst = checked_cast<FixedInstance *>(obj);
                    FunctionWrapper *fWrap = fInst->fWrap;
                    ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                    runtimeFrame->instantiate(&meta->env);
                    PrototypeInstance *pInst = new PrototypeInstance(protoObj, meta->objectClass);
                    baseVal = OBJECT_TO_JS2VAL(pInst);
                    runtimeFrame->thisObject = baseVal;
//                      assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                    if (!fWrap->code)
                        jsr(phase, fWrap->bCon);   // seems out of order, but we need to catch the current top frame 
                    meta->env.addFrame(runtimeFrame);
                    if (fWrap->code) {  // native code, pass pointer to argument base
                        a = fWrap->code(meta, a, base(argCount), argCount);
                        meta->env.removeTopFrame();
                        pop(argCount + 1);
                        push(a);
                    }
                    else {
                        insert(baseVal, argCount + 1);
                    }
                }
                else
                    ASSERT(false);
            }
        }
        break;

    case eCall:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            a = top(argCount + 2);                  // 'this'
            b = top(argCount + 1);                      // target function
            if (JS2VAL_IS_PRIMITIVE(b))
                meta->reportError(Exception::badValueError, "Can't call on primitive value", errorPos());
            JS2Object *fObj = JS2VAL_TO_OBJECT(b);
            if (fObj->kind == FixedInstanceKind) {
                FixedInstance *fInst = checked_cast<FixedInstance *>(fObj);
                FunctionWrapper *fWrap = fInst->fWrap;
                js2val compileThis = fWrap->compileFrame->thisObject;
                if (JS2VAL_IS_VOID(compileThis))
                    a = JS2VAL_VOID;
                else {
                    if (JS2VAL_IS_INACCESSIBLE(compileThis)) {
                        Frame *g = meta->env.getPackageOrGlobalFrame();
                        if (fWrap->compileFrame->prototype && (JS2VAL_IS_NULL(a) || JS2VAL_IS_VOID(a)) && (g->kind == GlobalObjectKind))
                            a = OBJECT_TO_JS2VAL(g);
                    }
                }
                ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                runtimeFrame->instantiate(&meta->env);
                runtimeFrame->thisObject = a;
//                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                if (!fWrap->code)
                    jsr(phase, fWrap->bCon);   // seems out of order, but we need to catch the current top frame 
                meta->env.addFrame(runtimeFrame);
                if (fWrap->code) {  // native code, pass pointer to argument base
                    a = fWrap->code(meta, a, base(argCount), argCount);
                    meta->env.removeTopFrame();
                    pop(argCount + 2);
                    push(a);
                }
            }
            else
            if (fObj->kind == MethodClosureKind) {  // XXX I made this up (particularly the frame push of the objectType)
                MethodClosure *mc = checked_cast<MethodClosure *>(fObj);
                FixedInstance *fInst = mc->method->fInst;
                FunctionWrapper *fWrap = fInst->fWrap;
                ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                runtimeFrame->instantiate(&meta->env);
                runtimeFrame->thisObject = mc->thisObject;
//                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                if (!fWrap->code)
                    jsr(phase, fWrap->bCon);   // seems out of order, but we need to catch the current top frame 
                meta->env.addFrame(meta->objectType(mc->thisObject));
                meta->env.addFrame(runtimeFrame);
                if (fWrap->code) {
                    a = fWrap->code(meta, mc->thisObject, base(argCount), argCount);
                    meta->env.removeTopFrame();
                    meta->env.removeTopFrame();
                    pop(argCount + 2);
                    push(a);
                }
            }
            // XXX Remove the arguments from the stack, (native calls have done this already)
            // (important that they're tracked for gc in any mechanism)
        }
        break;

    case ePopv:
        {
            retval = pop();
        }
        break;

    case eReturn: 
        {
            retval = pop();
            rts();
            if (pc == NULL) 
                return pop();
	}
        break;

    case eReturnVoid: 
        {
            rts();
            if (pc == NULL) 
                return retval;
	}
        break;

    case ePushFrame: 
        {
            Frame *f = bCon->mFrameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            meta->env.addFrame(f);
            f->instantiate(&meta->env);
        }
        break;

    case ePopFrame: 
        {
            meta->env.removeTopFrame();
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
                    a = STRING_TO_JS2VAL(function_StringAtom); 
                    break;
                case PrototypeInstanceKind:
                case PackageKind:
                case GlobalObjectKind:
                    a = STRING_TO_JS2VAL(object_StringAtom);
                    break;
                case FixedInstanceKind:
                    a = STRING_TO_JS2VAL(checked_cast<FixedInstance *>(obj)->typeofString);
                    break;
                case DynamicInstanceKind:
                    a = STRING_TO_JS2VAL(checked_cast<DynamicInstance *>(obj)->typeofString);
                    break;
                }
            }
            push(a);
        }
        break;