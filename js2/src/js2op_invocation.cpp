
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


        case eReturn: {
            retval = pop();
            return retval;
	}
        break;

        case eReturnVoid: {
            return retval;
	}
        break;

        case eNewObject: {
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
