/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef _MORKATOMMAP_
#define _MORKATOMMAP_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKINTMAP_
#include "morkIntMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kAtomAidMap  /*i*/ 0x6141 /* ascii 'aA' */

#define morkAtomAidMap_kStartSlotCount 512

/*| morkAtomAidMap: keys of morkBookAtom organized by atom ID
|*/
class morkAtomAidMap : public morkMap { // for mapping tokens to maps

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseAtomAidMap() only if open
  virtual ~morkAtomAidMap(); // assert that CloseAtomAidMap() executed earlier
  
public: // morkMap construction & destruction
  morkAtomAidMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void CloseAtomAidMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsAtomAidMap() const
  { return IsNode() && mNode_Derived == morkDerived_kAtomAidMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  virtual mork_bool // note: equal(a,b) implies hash(a) == hash(b)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;
  // implemented using morkBookAtom::HashAid()

  virtual mork_u4 // note: equal(a,b) implies hash(a) == hash(b)
  Hash(morkEnv* ev, const void* inKey) const;
  // implemented using morkBookAtom::EqualAid()
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool      AddAtom(morkEnv* ev, morkBookAtom* ioAtom);
  // AddAtom() returns ev->Good()

  morkBookAtom*  CutAtom(morkEnv* ev, const morkBookAtom* inAtom);
  // CutAtom() returns the atom removed equal to inAtom, if there was one
  
  morkBookAtom*  GetAtom(morkEnv* ev, const morkBookAtom* inAtom);
  // GetAtom() returns the atom equal to inAtom, or else nil

  morkBookAtom*  GetAid(morkEnv* ev, mork_aid inAid);
  // GetAid() returns the atom equal to inAid, or else nil

  // note the atoms are owned elsewhere, usuall by morkAtomSpace

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakAtomAidMap(morkAtomAidMap* me,
    morkEnv* ev, morkAtomAidMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongAtomAidMap(morkAtomAidMap* me,
    morkEnv* ev, morkAtomAidMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};


class morkAtomAidMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkAtomAidMapIter(morkEnv* ev, morkAtomAidMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkAtomAidMapIter( ) : morkMapIter()  { }
  void InitAtomAidMapIter(morkEnv* ev, morkAtomAidMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change* FirstAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->First(ev, outAtomPtr, /*val*/ (void*) 0); }
  
  mork_change* NextAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->Next(ev, outAtomPtr, /*val*/ (void*) 0); }
  
  mork_change* HereAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->Here(ev, outAtomPtr, /*val*/ (void*) 0); }
  
  mork_change* CutHereAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->CutHere(ev, outAtomPtr, /*val*/ (void*) 0); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kAtomBodyMap  /*i*/ 0x6142 /* ascii 'aB' */

#define morkAtomBodyMap_kStartSlotCount 512

/*| morkAtomBodyMap: keys of morkBookAtom organized by body bytes
|*/
class morkAtomBodyMap : public morkMap { // for mapping tokens to maps

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseAtomBodyMap() only if open
  virtual ~morkAtomBodyMap(); // assert CloseAtomBodyMap() executed earlier
  
public: // morkMap construction & destruction
  morkAtomBodyMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void CloseAtomBodyMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsAtomBodyMap() const
  { return IsNode() && mNode_Derived == morkDerived_kAtomBodyMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  virtual mork_bool // note: equal(a,b) implies hash(a) == hash(b)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const;
  // implemented using morkBookAtom::EqualFormAndBody()

  virtual mork_u4 // note: equal(a,b) implies hash(a) == hash(b)
  Hash(morkEnv* ev, const void* inKey) const;
  // implemented using morkBookAtom::HashFormAndBody()
// } ===== end morkMap poly interface =====

public: // other map methods

  mork_bool      AddAtom(morkEnv* ev, morkBookAtom* ioAtom);
  // AddAtom() returns ev->Good()

  morkBookAtom*  CutAtom(morkEnv* ev, const morkBookAtom* inAtom);
  // CutAtom() returns the atom removed equal to inAtom, if there was one
  
  morkBookAtom*  GetAtom(morkEnv* ev, const morkBookAtom* inAtom);
  // GetAtom() returns the atom equal to inAtom, or else nil

  // note the atoms are owned elsewhere, usuall by morkAtomSpace

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakAtomBodyMap(morkAtomBodyMap* me,
    morkEnv* ev, morkAtomBodyMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongAtomBodyMap(morkAtomBodyMap* me,
    morkEnv* ev, morkAtomBodyMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

class morkAtomBodyMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkAtomBodyMapIter(morkEnv* ev, morkAtomBodyMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkAtomBodyMapIter( ) : morkMapIter()  { }
  void InitAtomBodyMapIter(morkEnv* ev, morkAtomBodyMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change* FirstAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->First(ev, outAtomPtr, /*val*/ (void*) 0); }
  
  mork_change* NextAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->Next(ev, outAtomPtr, /*val*/ (void*) 0); }
  
  mork_change* HereAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->Here(ev, outAtomPtr, /*val*/ (void*) 0); }
  
  mork_change* CutHereAtom(morkEnv* ev, morkBookAtom** outAtomPtr)
  { return this->CutHere(ev, outAtomPtr, /*val*/ (void*) 0); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kAtomRowMap  /*i*/ 0x6152 /* ascii 'aR' */

/*| morkAtomRowMap: maps morkAtom* -> morkRow*
|*/
class morkAtomRowMap : public morkIntMap { // for mapping atoms to rows

public:
  mork_column mAtomRowMap_IndexColumn; // row column being indexed

public:

  virtual ~morkAtomRowMap();
  morkAtomRowMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_column inIndexColumn);

public: // adding and cutting from morkRow instance candidate

  void  AddRow(morkEnv* ev, morkRow* ioRow);
  // add ioRow only if it contains a cell in mAtomRowMap_IndexColumn. 

  void  CutRow(morkEnv* ev, morkRow* ioRow);
  // cut ioRow only if it contains a cell in mAtomRowMap_IndexColumn. 

public: // other map methods

  mork_bool  AddAid(morkEnv* ev, mork_aid inAid, morkRow* ioRow)
  { return this->AddInt(ev, inAid, ioRow); }
  // the AddAid() boolean return equals ev->Good().

  mork_bool  CutAid(morkEnv* ev, mork_aid inAid)
  { return this->CutInt(ev, inAid); }
  // The CutAid() boolean return indicates whether removal happened. 
  
  morkRow*   GetAid(morkEnv* ev, mork_aid inAid)
  { return (morkRow*) this->GetInt(ev, inAid); }
  // Note the returned space does NOT have an increase in refcount for this.
  
public: // dynamic type identification
  mork_bool IsAtomRowMap() const
  { return IsNode() && mNode_Derived == morkDerived_kAtomRowMap; }

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakAtomRowMap(morkAtomRowMap* me,
    morkEnv* ev, morkAtomRowMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongAtomRowMap(morkAtomRowMap* me,
    morkEnv* ev, morkAtomRowMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }

};

class morkAtomRowMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkAtomRowMapIter(morkEnv* ev, morkAtomRowMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkAtomRowMapIter( ) : morkMapIter()  { }
  void InitAtomRowMapIter(morkEnv* ev, morkAtomRowMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstAtomAndRow(morkEnv* ev, morkAtom** outAtom, morkRow** outRow)
  { return this->First(ev, outAtom, outRow); }
  
  mork_change*
  NextAtomAndRow(morkEnv* ev, morkAtom** outAtom, morkRow** outRow)
  { return this->Next(ev, outAtom, outRow); }
  
  mork_change*
  HereAtomAndRow(morkEnv* ev, morkAtom** outAtom, morkRow** outRow)
  { return this->Here(ev, outAtom, outRow); }
  
  mork_change*
  CutHereAtomAndRow(morkEnv* ev, morkAtom** outAtom, morkRow** outRow)
  { return this->CutHere(ev, outAtom, outRow); }
};


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKATOMMAP_ */
