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

        // Get a multiname literal and add the currently open namespaces from the context
        // Push the resulting multiname object
        case eMultiname: {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            mn->addNamespace(meta->cxt);
            push(OBJECT_TO_JS2VAL(mn));
        }
        break;

        // Get a multiname literal and pop a namespace value to add to it
        // Push the resulting multiname object
        case eQMultiname: {
            js2val nsVal = pop();
            if (!JS2VAL_IS_OBJECT(nsVal))
                meta->reportError(Exception::badValueError, "Expected a namespace", meta->errorPos);
            JS2Object *obj = JS2VAL_TO_OBJECT(nsVal);
            if ((obj->kind != AttributeObjectKind) || ((checked_cast<Attribute *>(obj))->attrKind != Attribute::NamespaceAttr))
                meta->reportError(Exception::badValueError, "Expected a namespace", meta->errorPos);            
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            mn->addNamespace(checked_cast<Namespace *>(obj));
            push(OBJECT_TO_JS2VAL(mn));
        }
        break;

        // Pop a multiname object and read it's value from the environment on to the stack.
        case eLexicalRead: {
            js2val mnVal = pop();
            ASSERT(JS2VAL_IS_OBJECT(mnVal));
            JS2Object *obj = JS2VAL_TO_OBJECT(mnVal);
            Multiname *mn = checked_cast<Multiname *>(obj);
            retval = meta->env.lexicalRead(meta, mn, phase);
            push(retval);
	}
        break;

        // Pop a value and a multiname. Write the value to the multiname in the environment, leave
        // the value on the stack top.
        case eLexicalWrite: {
            retval = pop();
            js2val mnVal = pop();
            ASSERT(JS2VAL_IS_OBJECT(mnVal));
            JS2Object *obj = JS2VAL_TO_OBJECT(mnVal);
            Multiname *mn = checked_cast<Multiname *>(obj);
            meta->env.lexicalWrite(meta, mn, retval, true, phase);
            push(retval);
	}
        break;

        // Pop a namespace object and add it to the list of currently open namespaces in the context
        case eUse: {
            js2val nsVal = pop();
            if (!JS2VAL_IS_OBJECT(nsVal))
                meta->reportError(Exception::badValueError, "Expected a namespace", meta->errorPos);
            JS2Object *obj = JS2VAL_TO_OBJECT(nsVal);
            if ((obj->kind != AttributeObjectKind) || ((checked_cast<Attribute *>(obj))->attrKind != Attribute::NamespaceAttr))
                meta->reportError(Exception::badValueError, "Expected a namespace", meta->errorPos);            

        }
        break;