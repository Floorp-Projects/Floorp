
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


    case eNewObject: // XXX in js2op_literal instead?
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            PrototypeInstance *pInst = new PrototypeInstance(meta->objectClass->prototype, meta->objectClass);
            for (uint16 i = 0; i < argCount; i++) {
                js2val nameVal = pop();
                ASSERT(JS2VAL_IS_STRING(nameVal));
                String *name = JS2VAL_TO_STRING(nameVal);
                const StringAtom &nameAtom = meta->world.identifiers[*name];
                js2val fieldVal = pop();
                const DynamicPropertyMap::value_type e(nameAtom, fieldVal);
                pInst->dynamicProperties.insert(e);
            }
            push(OBJECT_TO_JS2VAL(pInst));
        }
        break;

    case eToBoolean:
        {
            js2val v = pop();
            bool b = toBoolean(v);
            push(BOOLEAN_TO_JS2VAL(b));
        }
        break;

    case eNew:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            js2val v = top();
            ASSERT(JS2VAL_IS_OBJECT(v) && !JS2VAL_IS_NULL(v));
            JS2Object *obj = JS2VAL_TO_OBJECT(v);
            ASSERT(obj->kind == ClassKind);
            JS2Class *c = checked_cast<JS2Class *>(obj);
            push(c->construct(meta, JS2VAL_NULL, NULL, argCount));
        }
        break;

    case eCall:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            js2val thisVal = top(argCount + 1);
            js2val fVal = top(argCount);
            if (JS2VAL_IS_PRIMITIVE(fVal))
                meta->reportError(Exception::badValueError, "Can't call on primitive value", errorPos());
            JS2Object *fObj = JS2VAL_TO_OBJECT(fVal);
            if (fObj->kind == FixedInstanceKind) {
                FixedInstance *fInst = checked_cast<FixedInstance *>(fObj);
                FunctionWrapper *fWrap = fInst->fWrap;
                js2val compileThis = fWrap->compileFrame->thisObject;
                js2val runtimeThis;
                if (JS2VAL_IS_VOID(compileThis))
                    runtimeThis = JS2VAL_VOID;
                else {
                    if (JS2VAL_IS_INACCESSIBLE(compileThis)) {
                        runtimeThis = thisVal;
                        Frame *g = meta->env.getPackageOrGlobalFrame();
                        if (fWrap->compileFrame->prototype && (JS2VAL_IS_NULL(runtimeThis) || JS2VAL_IS_VOID(runtimeThis)) && (g->kind == GlobalObjectKind))
                            runtimeThis = OBJECT_TO_JS2VAL(g);
                    }
                }
                ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                runtimeFrame->instantiate(&meta->env);
                runtimeFrame->thisObject = runtimeThis;
//                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                if (!fWrap->code)
                    jsr(phase, fWrap->bCon);   // seems out of order, but we need to catch the current top frame 
                meta->env.addFrame(runtimeFrame);
                if (fWrap->code) {
                    push(fWrap->code(meta, runtimeThis, NULL, argCount));
                    meta->env.removeTopFrame();
                }
            }
            else
            if (fObj->kind == MethodClosureKind) {  // XXX I made this up (particularly the push of the objectType)
                MethodClosure *mc = checked_cast<MethodClosure *>(fObj);
                FixedInstance *fInst = mc->method->fInst;
                FunctionWrapper *fWrap = fInst->fWrap;
                ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                runtimeFrame->instantiate(&meta->env);
                runtimeFrame->thisObject = mc->thisObject;
//                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                jsr(phase, fWrap->bCon);   // seems out of order, but we need to catch the current top frame 
                meta->env.addFrame(meta->objectType(mc->thisObject));
                meta->env.addFrame(runtimeFrame);
            }
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
            js2val a = pop();
            js2val rval;
            if (JS2VAL_IS_UNDEFINED(a))
                rval = STRING_TO_JS2VAL(&undefined_StringAtom);
            else
            if (JS2VAL_IS_BOOLEAN(a))
                rval = STRING_TO_JS2VAL(&meta->world.identifiers["boolean"]);
            else
            if (JS2VAL_IS_NUMBER(a))
                rval = STRING_TO_JS2VAL(&meta->world.identifiers["number"]);
            else
            if (JS2VAL_IS_STRING(a))
                rval = STRING_TO_JS2VAL(&meta->world.identifiers["string"]);
            else {
                ASSERT(JS2VAL_IS_OBJECT(a));
                if (JS2VAL_IS_NULL(a))
                    rval = STRING_TO_JS2VAL(&object_StringAtom);
                JS2Object *obj = JS2VAL_TO_OBJECT(a);
                switch (obj->kind) {
                case MultinameKind:
                    rval = STRING_TO_JS2VAL(&meta->world.identifiers["namespace"]); 
                    break;
                case AttributeObjectKind:
                    rval = STRING_TO_JS2VAL(&meta->world.identifiers["attribute"]); 
                    break;
                case ClassKind:
                case MethodClosureKind:
                    rval = STRING_TO_JS2VAL(&function_StringAtom); 
                    break;
                case PrototypeInstanceKind:
                case PackageKind:
                case GlobalObjectKind:
                    rval = STRING_TO_JS2VAL(&object_StringAtom);
                    break;
                case FixedInstanceKind:
                    rval = STRING_TO_JS2VAL(&checked_cast<FixedInstance *>(obj)->typeofString);
                    break;
                case DynamicInstanceKind:
                    rval = STRING_TO_JS2VAL(&checked_cast<DynamicInstance *>(obj)->typeofString);
                    break;
                }
            }
            push(rval);
        }
        break;