
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

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#ifndef bytecodecontainer_h___
#define bytecodecontainer_h___



namespace JavaScript {
namespace MetaData {


class Multiname;

class BytecodeContainer {
public:
    BytecodeContainer() : mBuffer(new CodeBuffer), mStackTop(0), mStackMax(0) { }
    BytecodeContainer::~BytecodeContainer() ;

    
    uint8 *getCodeStart()                   { return mBuffer->begin(); }


    void emitOp(JS2Op op)                   { adjustStack(op); addByte((uint8)op); }
    void emitOp(JS2Op op, int32 effect)     { adjustStack(op, effect); addByte((uint8)op); }

    void adjustStack(JS2Op op)              { adjustStack(op, JS2Engine::getStackEffect(op)); }
    void adjustStack(JS2Op op, int32 effect){ mStackTop += effect; if (mStackTop > mStackMax) mStackMax = mStackTop; ASSERT(mStackTop >= 0); }

    void addByte(uint8 v)                   { mBuffer->push_back(v); }
    
    void addPointer(const void *v)          { ASSERT(sizeof(void *) == sizeof(uint32)); addLong((uint32)(v)); }
    static void *getPointer(void *pc)       { return (void *)getLong(pc); }
    
    void addFloat64(float64 v)              { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(float64)); }
    static float64 getFloat64(void *pc)     { return *((float64 *)pc); }
   
    void addLong(const uint32 v)            { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint32)); }
    static uint32 getLong(void *pc)         { return *((uint32 *)pc); }

    void addShort(uint16 v)                 { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint16)); }
    static uint16 getShort(void *pc)        { return *((uint16 *)pc); }

    void addMultiname(Multiname *mn)        { mMultinameList.push_back(mn); addShort(mMultinameList.size() - 1); }
    static Multiname *getMultiname(void *pc){ return (Multiname *)getLong(pc); }

    void addString(const StringAtom &x)     { emitOp(eString); addPointer(&x); }
    void addString(String &x)               { emitOp(eString); addPointer(&x); }
    void addString(String *x)               { emitOp(eString); addPointer(x); }
    static String *getString(void *pc)      { return (String *)getPointer(pc); }
    // XXX We lose StringAtom here - is there anyway of stashing these in a bytecodecontainer?
    
    typedef std::vector<uint8> CodeBuffer;

    CodeBuffer *mBuffer;
    std::vector<Multiname *> mMultinameList;      // gc tracking 

    int32 mStackTop;                // keep these as signed so as to
    int32 mStackMax;                // track if they go negative.

};


}
}

#endif
