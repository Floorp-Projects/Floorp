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

        // Get the first enumerable property from the object
        // and save it's 'state' on the stack. Push true/false
        // for whether that was succesful.
    case eFirst:
        {
            a = pop();
            b = meta->toObject(a);
            ForIteratorObject *fi = new ForIteratorObject(JS2VAL_TO_OBJECT(b));
            push(OBJECT_TO_JS2VAL(fi));
            push(BOOLEAN_TO_JS2VAL(fi->first()));
        }
        break;

        // Increment the enumeration 'state' on the stack. 
        // Push true/false for whether that was succesful.
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
            if (finallyOffset != NotALabel)
                pushHandler(pc + finallyOffset);
            pc += sizeof(int32);
            int32 catchOffset = BytecodeContainer::getOffset(pc);
            if (catchOffset != NotALabel)
                pushHandler(pc + catchOffset);
            pc += sizeof(int32);
        }
        break;

    case eHandler:
        {
            popHandler();
        }
        break;
