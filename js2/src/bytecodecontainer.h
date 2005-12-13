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

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#ifndef bytecodecontainer_h___
#define bytecodecontainer_h___



namespace JavaScript {
namespace MetaData {

class BytecodeContainer;
    
class Label {
public:
    
    typedef enum { InternalLabel, NamedLabel, BreakLabel, ContinueLabel } LabelKind;

    Label() : mKind(InternalLabel), mHasLocation(false) { }
    Label(LabelStmtNode *lbl) : mKind(NamedLabel), mHasLocation(false), mLabelStmt(lbl) { }
    Label(LabelKind kind) : mKind(kind), mHasLocation(false) { }

    bool matches(const StringAtom *name)
    {
        return ((mKind == NamedLabel) && (mLabelStmt->name.compare(*name) == 0));
    }

    bool matches(LabelKind kind)
    {
        return (mKind == kind);
    }

    void addFixup(BytecodeContainer *bcg, uint32 branchLocation);        
    void setLocation(BytecodeContainer *bcg, uint32 location);

    std::vector<uint32> mFixupList;

    LabelKind mKind;
    bool mHasLocation;
    LabelStmtNode *mLabelStmt;

    uint32 mLocation;
};

#define NotALabel (BytecodeContainer::LabelID)(-1)

class Multiname;
class JS2Object;
class Frame;
class RegExpInstance;

class StashedMultiname {
public:
    uint16 stringIndex;     // the name
    uint16 namespaceCount;  //
    uint16 *namespaceList;  // if null and count is 1, it's the public namespace.
};

class BytecodeContainer {
public:
    BytecodeContainer() : mStackTop(0), mStackMax(0) 
#ifdef DEBUG
    , fName(widenCString("<unknown>"))
#endif
                            { }
    
    BytecodeContainer::~BytecodeContainer();


    void mark();
    
    uint8 *getCodeStart()                   { return mBuffer.begin(); }
    uint8 *getCodeEnd()                     { return mBuffer.begin() + mBuffer.size(); }

    uint32 getMaxStack()                    { return mStackMax; }

    typedef std::pair<uint16, size_t> MapEntry;
    std::vector<MapEntry> pcMap;
    typedef uint32 LabelID;

    size_t getPosition(uint16 pc);

    void emitOp(JS2Op op, size_t pos);
    void emitOp(JS2Op op, size_t pos, int32 effect);

    void emitBranch(JS2Op op, LabelID tgt, size_t pos)
                                            { emitOp(op, pos); addFixup(tgt); }

    void adjustStack(JS2Op op)              { adjustStack(op, getStackEffect(op)); }
    void adjustStack(JS2Op op, int32 effect);

    void addByte(uint8 v)                   { mBuffer.push_back(v); }
    
    void addPointer(const void *v)          { ASSERT(sizeof(void *) == sizeof(uint32)); addUInt32((uint32)(v)); }
    static void *getPointer(void *pc)       { return (void *)getUInt32(pc); }
    
    // These insert the opcodes...
    void addFloat64(float64 v, size_t pos)  { emitOp(eNumber, pos); mBuffer.insert(mBuffer.end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(float64)); }
    static float64 getFloat64(void *pc)     { return *((float64 *)pc); }
   
    void addUInt64(const uint64 v, size_t pos) { emitOp(eUInt64, pos); mBuffer.insert(mBuffer.end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint64)); }
    static uint64 getUInt64(void *pc)       { return *((uint64 *)pc); }

    void addInt64(const int64 v, size_t pos){ emitOp(eInt64, pos); mBuffer.insert(mBuffer.end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(int64)); }
    static int64 getInt64(void *pc)         { return *((int64 *)pc); }

    void addInteger(int32 i, size_t pos)    { emitOp(eInteger, pos); addInt32(i); }
   
    // These don't insert opcodes...
    void addUInt32(const uint32 v)          { mBuffer.insert(mBuffer.end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint32)); }
    static uint32 getUInt32(void *pc)       { return *((uint32 *)pc); }

    void addShort(uint16 v)                 { mBuffer.insert(mBuffer.end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint16)); }
    static uint16 getShort(void *pc)        { return *((uint16 *)pc); }

    void addInt32(int32 v)                  { addOffset(v); }
    static int32 getInt32(void *pc)         { return *((int32 *)pc); }


    // Maintain list of associated pointers, so as to keep the objects safe across gc's
//    void addMultiname(uint16 index)         { addShort(index); }
    uint16 saveMultiname(Multiname *mn)     { mMultinameList.push_back(mn); return (uint16)(mMultinameList.size() - 1); }

    void addFrame(Frame *f);
    void saveFrame(Frame *f);
    void addRegExp(RegExpInstance *x, size_t pos);
	void addObject(JS2Object *b)            { mObjectList.push_back(b); addShort((uint16)(mObjectList.size() - 1)); }
    void saveObject(JS2Object *b)           { mObjectList.push_back(b); }

    void addOffset(int32 v)                 { mBuffer.insert(mBuffer.end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(int32)); }
    void setOffset(uint32 index, int32 v)   { *((int32 *)(mBuffer.begin() + index)) = v; }
    static int32 getOffset(void *pc)        { return *((int32 *)pc); }
    
    void addString(const StringAtom *x, size_t pos)     { emitOp(eString, pos); mStringList.push_back(String(*x)); addShort((uint16)(mStringList.size() - 1)); }
    void addString(String &x, size_t pos)               { emitOp(eString, pos); mStringList.push_back(String(x)); addShort((uint16)(mStringList.size() - 1)); }
    void addString(String *x, size_t pos)               { emitOp(eString, pos); mStringList.push_back(String(*x)); addShort((uint16)(mStringList.size() - 1)); }
    // XXX We lose StringAtom here (and is it safe to stash the address of a StringAtom?)
    // - is there any way of keeping StringAtoms themselves in a bytecodeContainer?
    
    void addType(JS2Class *c)               { addPointer(c); }  // assume that classes are rooted already
    static JS2Class *getType(void *pc)      { return (JS2Class *)(getPointer(pc)); }



    typedef std::vector<uint8> CodeBuffer;

    CodeBuffer mBuffer;
    std::vector<Multiname *> mMultinameList;      // gc tracking 
    std::vector<JS2Object *> mObjectList;         // gc tracking 

    std::vector<String> mStringList;

    int32 mStackTop;                // keep these as signed so as to...
    int32 mStackMax;                // ...track if they go negative.

    std::vector<Label> mLabelList;

    LabelID getLabel();
    void addFixup(LabelID label);
    void setLabel(LabelID label);

    String mSource;                     // for error reporting
    String mSourceLocation;             // for error reporting
    String fName;                       // relevant function name

};


}
}

#endif
