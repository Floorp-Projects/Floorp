
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



    case eAdd: 
        {
	    js2val a = pop();
	    js2val b = pop();
	    a = toPrimitive(a);
	    b = toPrimitive(b);
	    if (JS2VAL_IS_STRING(a) || JS2VAL_IS_STRING(b)) {
	        String *astr = toString(a);
	        String *bstr = toString(b);
                String *c = new String(*astr);
                *c += *bstr;
                retval = STRING_TO_JS2VAL(c);
	        push(retval);
	    }
	    else {
                float64 anum = toNumber(a);
                float64 bnum = toNumber(b);
                retval = pushNumber(anum + bnum);
	    } 
        }
        break;

    case eSubtract: 
        {
	    js2val a = pop();
	    js2val b = pop();
            float64 anum = toNumber(a);
            float64 bnum = toNumber(b);
            retval = pushNumber(anum - bnum);
        }
        break;
