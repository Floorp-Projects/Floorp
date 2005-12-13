/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* ***** BEGIN LICENSE BLOCK *****
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

    case eBranchTrue:
        {
            a = pop();
            bool c = meta->toBoolean(a);
            if (c) {
                int32 offset = BytecodeContainer::getOffset(pc);
                pc += offset;
            }
            else 
                pc += sizeof(int32);
        }
        break;

    case eBranchFalse:
        {
            a = pop();
            bool c = meta->toBoolean(a);
            if (!c) {
                int32 offset = BytecodeContainer::getOffset(pc);
                pc += offset;
            }
            else 
                pc += sizeof(int32);
        }
        break;

    case eBranch:
        {
            int32 offset = BytecodeContainer::getOffset(pc);
            pc += offset;
        }
        break;

    case eBreak:
        {
            // don't update the pc in order to get the blockcount
            // since the branch calculation doesn't include that.
            int32 offset = BytecodeContainer::getOffset(pc);
            uint32 blockCount = BytecodeContainer::getShort(pc + sizeof(int32));
            for (uint32 i = 0; i < blockCount; i++)
                meta->env->removeTopFrame();
            pc += offset;
        }
        break;

    case ePop:
        {
            pop();
        }
        break;

    case eDup:
        {
            push(top());
        }
        break;

    case eSwap:        // go from [x][y] --> [y][x]
        {
            ASSERT((sp - execStack) > 2);
            a = sp[-2];
            sp[-2] = sp[-1];
            sp[-1] = a;
        }
        break;
    case eSwap2:        // go from [x][y][z] --> [y][z][x]
        {
            ASSERT((sp - execStack) > 3);
            a = sp[-3];
            sp[-3] = sp[-2];
            sp[-2] = sp[-1];
            sp[-1] = a;
        }
        break;

        
        // Get the first enumerable property from the object
        // and save it's 'state' on the stack. Push true/false
        // for whether that was successful.
    case eFirst:
        {
            a = pop();
            b = meta->toObject(a);
            ForIteratorObject *fi = new (meta) ForIteratorObject(JS2VAL_TO_OBJECT(b));
            push(OBJECT_TO_JS2VAL(fi));
            push(BOOLEAN_TO_JS2VAL(fi->first(this)));
        }
        break;

        // Increment the enumeration 'state' on the stack. 
        // Push true/false for whether that was successful.
    case eNext:
        {
            a = top();
            ASSERT(JS2VAL_IS_OBJECT(a));
            JS2Object *obj = JS2VAL_TO_OBJECT(a);
            ASSERT(obj->kind == ForIteratorKind);
            ForIteratorObject *fi = checked_cast<ForIteratorObject *>(obj);
            push(BOOLEAN_TO_JS2VAL(fi->next(this)));
        }
        break;

        // Extract the current value of the iterator
    case eForValue:
        {
            a = top();
            ASSERT(JS2VAL_IS_OBJECT(a));
            JS2Object *obj = JS2VAL_TO_OBJECT(a);
            ASSERT(obj->kind == ForIteratorKind);
            ForIteratorObject *fi = checked_cast<ForIteratorObject *>(obj);
            push(fi->value(this));
        }
        break;


    case eTry:
        {
            int32 finallyOffset = BytecodeContainer::getOffset(pc);
            if (finallyOffset != (int32)NotALabel)
                pushHandler(pc + finallyOffset);
            pc += sizeof(int32);
            int32 catchOffset = BytecodeContainer::getOffset(pc);
            if (catchOffset != (int32)NotALabel)
                pushHandler(pc + catchOffset);
            pc += sizeof(int32);
        }
        break;

    case eHandler:
        {
            popHandler();
        }
        break;

    case eThrow:
        {
            throw Exception(pop());
        }
        break;

    case eCallFinally:
        {
            int32 finallyOffset = BytecodeContainer::getOffset(pc);
            uint8 *tgt = pc + finallyOffset;
            pc += sizeof(int32);
            finallyStack.push(pc);
            pc = tgt;
        }
        break;

    case eReturnFinally:
        {
            pc = finallyStack.top();
            finallyStack.pop();
        }
        break;
    


