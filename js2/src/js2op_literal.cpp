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


    case eNumber: 
        {
            pushNumber(BytecodeContainer::getFloat64(pc));
            pc += sizeof(float64);
        }
        break;

    case eInteger: 
        {
            push(INT_TO_JS2VAL(BytecodeContainer::getInt32(pc)));
            pc += sizeof(int32);
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
            uint16 index = BytecodeContainer::getShort(pc);
            push(STRING_TO_JS2VAL(allocString(&bCon->mStringList[index])));
            pc += sizeof(short);
        }
        break;

    case eRegExp: 
        {
            RegExpInstance *x = checked_cast<RegExpInstance *>(bCon->mObjectList[BytecodeContainer::getShort(pc)]);
            push(OBJECT_TO_JS2VAL(x));
            pc += sizeof(short);
        }
        break;

    case eFunction: 
        {
            FunctionInstance *x = checked_cast<FunctionInstance *>(bCon->mObjectList[BytecodeContainer::getShort(pc)]);
            push(OBJECT_TO_JS2VAL(x));
            pc += sizeof(short);
        }
        break;

    case eClosure: 
        {
            FunctionInstance *x = checked_cast<FunctionInstance *>(bCon->mObjectList[BytecodeContainer::getShort(pc)]);
            pc += sizeof(short);

            // For each active plural frame in the function definition environment, we need
            // to find it's current singular counterpart and use that as the dohickey
            FrameListIterator closure_fi = x->fWrap->env->getBegin();
            FrameListIterator current_fi = meta->env->getBegin();
            while (true) {
                Frame *closure_fr = *closure_fi;
                Frame *current_fr = *current_fi;
                ASSERT(closure_fr->kind == current_fr->kind);
                if ((closure_fr->kind == ClassKind) || (closure_fr->kind == PackageKind) || (closure_fr->kind == SystemKind))
                    break;
				closure_fi++;
				current_fi++;
            }
            x->fWrap->env = new (meta) Environment(meta->env);
        }
        break;

    case eNull: 
        {
            push(JS2VAL_NULL);
        }
        break;

    case eUndefined: 
        {
            push(JS2VAL_UNDEFINED);
        }
        break;

    case eThis: // XXX literal?
        {
            pFrame = meta->env->getEnclosingParameterFrame(&a);
            if ((pFrame == NULL) || JS2VAL_IS_INACCESSIBLE(a) || JS2VAL_IS_NULL(a))
                a = OBJECT_TO_JS2VAL(meta->env->getPackageFrame());
//                meta->reportError(Exception::compileExpressionError, "'this' not available", errorPos());
//            else
//                a = pFrame->thisObject;
            push(a);
            pFrame = NULL;
        }
        break;

    case eSuper: // XXX literal?
        {
            pFrame = meta->env->getEnclosingParameterFrame(&a);
            ASSERT(pFrame);
//            a = pFrame->thisObject;
            if (JS2VAL_IS_INACCESSIBLE(a))
                meta->reportError(Exception::compileExpressionError, "'this' not available for 'super'", errorPos());
makeLimitedInstance:
            {
                JS2Class *limit = meta->env->getEnclosingClass()->super;
                ASSERT(limit);
                a = limit->ImplicitCoerce(meta, a);
                ASSERT(JS2VAL_IS_OBJECT(a));
                if (JS2VAL_IS_NULL(a))
                    push(JS2VAL_NULL);
                else
                    push(OBJECT_TO_JS2VAL(new (meta) LimitedInstance(JS2VAL_TO_OBJECT(a), limit)));
            }
            pFrame = NULL;
        }
        break;

    case eSuperExpr: // XXX literal?
        {
            a = pop();
            goto makeLimitedInstance;
        }
        break;

    case eNewObject:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            SimpleInstance *sInst = new (meta) SimpleInstance(meta, OBJECT_TO_JS2VAL(meta->objectClass->prototype), meta->objectClass);
            baseVal = OBJECT_TO_JS2VAL(sInst);
            for (uint16 i = 0; i < argCount; i++) {
                a = pop();
                ASSERT(JS2VAL_IS_STRING(a));
                astr = JS2VAL_TO_STRING(a);
                b = pop();
                meta->createDynamicProperty(sInst, meta->world.identifiers[*astr], b, ReadWriteAccess, false, true);
            }
            push(baseVal);
            baseVal = JS2VAL_VOID;
            astr = NULL;
        }
        break;

    case eNewArray:
        {
            uint16 argCount = BytecodeContainer::getShort(pc);
            pc += sizeof(uint16);
            ArrayInstance *aInst = new (meta) ArrayInstance(meta, OBJECT_TO_JS2VAL(meta->arrayClass->prototype), meta->arrayClass);
            baseVal = OBJECT_TO_JS2VAL(aInst);
            for (uint16 i = 0; i < argCount; i++) {
                b = pop();
                meta->createDynamicProperty(aInst, numberToStringAtom(toUInt32((argCount - 1) - i)), b, ReadWriteAccess, false, true);
            }
            setLength(meta, aInst, argCount);
            push(baseVal);
            baseVal = JS2VAL_VOID;
        }
        break;

