
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
            PrototypeInstance *pInst = new PrototypeInstance(NULL); // XXX Object prototype object
            for (uint16 i = 0; i < argCount; i++) {
                js2val nameVal = pop();
                ASSERT(JS2VAL_IS_STRING(nameVal));
                String *name = JS2VAL_TO_STRING(nameVal);
                const StringAtom &nameAtom = world.identifiers[*name];
                js2val fieldVal = pop();
                const DynamicPropertyMap::value_type e(nameAtom, fieldVal);
                pInst->dynamicProperties.insert(e);
            }
            push(OBJECT_TO_JS2VAL(pInst));
        }
        break;
/*
    case eToBoolean:
        {
            js2val v = pop();
            bool b = toBoolean(v);
            retval = BOOLEAN_TO_JS2VAL(b);
            push(retval);
        }
        break;
*/
    case eNew:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            js2val v = top();
            ASSERT(JS2VAL_IS_OBJECT(v) && !JS2VAL_IS_NULL(v));
            JS2Object *obj = JS2VAL_TO_OBJECT(v);
            ASSERT(obj->kind == ClassKind);
            JS2Class *c = checked_cast<JS2Class *>(obj);
            push(OBJECT_TO_JS2VAL(c->construct(this)));
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
                Frame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                meta->env.addFrame(runtimeFrame);
                runtimeFrame->instantiate(&meta->env);
//                assignArguments(runtimeFrame, fWrap->compileFrame->signature);
                
                jsr(fWrap->bCon);

            }
            else
                ASSERT(false);
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
            if (activationStackEmpty()) 
                return retval;
            else
                rts();
	}
        break;

    case eReturnVoid: 
        {
            if (activationStackEmpty()) 
                return retval;
            else
                rts();
	}
        break;

