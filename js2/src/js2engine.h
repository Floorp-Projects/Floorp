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

#ifndef js2engine_h___
#define js2engine_h___


#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

namespace JavaScript {
namespace MetaData {




enum JS2Op {
    ePlus,
    eTrue,
    eFalse,
    eNumber,
    eString,            // <string pointer>
    eObject,            // <named argument count>
    eLexicalRead,       // <multiname index>
    eLexicalWrite,      // <multiname index>
    eReturn,
    eReturnVoid,
    eNewObject          // <argCount:16>
};


class JS2Metadata;
class BytecodeContainer;

class JS2Engine {
public:

    JS2Engine(World &world);

    js2val interpret(JS2Metadata *metadata, Phase execPhase, BytecodeContainer *targetbCon);

    js2val interpreterLoop();

    void *gc_alloc_8();
    float64 *newDoubleValue(float64 x);

    void pushNumber(float64 x);

#define MAX_EXEC_STACK (20)

    void push(js2val x) { ASSERT(sp < (execStack + MAX_EXEC_STACK)); *sp++ = x; }
    js2val pop()        { ASSERT(sp > execStack); return *--sp; }

    String *convertValueToString(js2val x);
    js2val convertValueToPrimitive(js2val x);
    float64 convertValueToDouble(js2val x);

    String *toString(js2val x)      { if (JS2VAL_IS_STRING(x)) return JS2VAL_TO_STRING(x); else return convertValueToString(x); }
    js2val toPrimitive(js2val x)    { if (JS2VAL_IS_PRIMITIVE(x)) return x; else return convertValueToPrimitive(x); }
    float64 toNumber(js2val x)      { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else if (JS2VAL_IS_DOUBLE(x)) return *JS2VAL_TO_DOUBLE(x); else return convertValueToDouble(x); }

    uint8 *pc;
    BytecodeContainer *bCon;
    JS2Metadata *meta;
    Phase phase;
    World &world;

    float64 *nanValue;
    float64 *float64Table[256];

    StringAtom &true_StringAtom;
    StringAtom &false_StringAtom;
    StringAtom &null_StringAtom;
    StringAtom &undefined_StringAtom;
    StringAtom &public_StringAtom;
    StringAtom &object_StringAtom;
    
    js2val *execStack;
    js2val *sp;
    

    static int getStackEffect(JS2Op op);


};


String *numberToString(float64 *number);



}
}



#endif

