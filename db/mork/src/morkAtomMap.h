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

#endif /* _MORKATOMMAP_ */
