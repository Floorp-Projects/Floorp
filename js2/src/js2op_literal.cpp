
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


    case eNumber: 
        {
            pushNumber(BytecodeContainer::getFloat64(pc));
            pc += sizeof(float64);
        }
        break;

    case eUInt64: 
        {
            pushULong(BytecodeContainer::getUInt64(pc));
            pc += sizeof(uint64);
        }
        break;

    case eInt64: 
        {
            pushLong(BytecodeContainer::getInt64(pc));
            pc += sizeof(int64);
        }
        break;

    case eTrue: 
        {
	    push(JS2VAL_TRUE);
	}
	break;

    case eFalse: 
        {
	    push(JS2VAL_FALSE);
	}
	break;

    case eString: 
        {
            push(STRING_TO_JS2VAL(allocString(BytecodeContainer::getString(pc))));
            pc += sizeof(String *);
        }
        break;

    case eRegExp: 
        {
            RegExpInstance *x = bCon->mRegExpList[BytecodeContainer::getShort(pc)];
            push(OBJECT_TO_JS2VAL(x));
            pc += sizeof(short);
        }
        break;

    case eNull: 
        {
	    push(JS2VAL_NULL);
	}
	break;

    case eThis: // XXX literal?
        {
            a = meta->env.findThis(true);
            if (JS2VAL_IS_INACCESSIBLE(a))
                meta->reportError(Exception::compileExpressionError, "'this' not available", errorPos());
            push(a);
        }
        break;

    case eNewObject:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            PrototypeInstance *pInst = new PrototypeInstance(meta->objectClass->prototype, meta->objectClass);
            baseVal = OBJECT_TO_JS2VAL(pInst);
            for (uint16 i = 0; i < argCount; i++) {
                a = pop();
                ASSERT(JS2VAL_IS_STRING(a));
                String *name = JS2VAL_TO_STRING(a);
                const StringAtom &nameAtom = meta->world.identifiers[*name];
                b = pop();
                const DynamicPropertyMap::value_type e(nameAtom, b);
                pInst->dynamicProperties.insert(e);
            }
            push(baseVal);
            baseVal = JS2VAL_VOID;
        }
        break;

    case eNewArray:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            ArrayInstance *aInst = new ArrayInstance(meta->arrayClass);
            baseVal = OBJECT_TO_JS2VAL(aInst);
            for (uint16 i = 0; i < argCount; i++) {
                b = pop();
                const DynamicPropertyMap::value_type e(*numberToString((argCount - 1) - i), b);
                aInst->dynamicProperties.insert(e);
            }
            setLength(meta, aInst, argCount);
            push(baseVal);
            baseVal = JS2VAL_VOID;
        }
        break;

