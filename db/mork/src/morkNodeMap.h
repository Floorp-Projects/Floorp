/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef _MORKNODEMAP_
#define _MORKNODEMAP_ 1

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

#define morkDerived_kNodeMap  /*i*/ 0x6E4D /* ascii 'nM' */

#define morkNodeMap_kStartSlotCount 512

/*| morkNodeMap: maps mork_token -> morkNode
|*/
class morkNodeMap : public morkIntMap { // for mapping tokens to nodes

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseNodeMap() only if open
  virtual ~morkNodeMap(); // assert that CloseNodeMap() executed earlier
  
public: // morkMap construction & destruction
  morkNodeMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void CloseNodeMap(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsNodeMap() const
  { return IsNode() && mNode_Derived == morkDerived_kNodeMap; }
// } ===== end morkNode methods =====

// { ===== begin morkMap poly interface =====
  // use the Equal() and Hash() for mork_u4 inherited from morkIntMap
// } ===== end morkMap poly interface =====

protected: // we want all subclasses to provide typesafe wrappers:

  mork_bool  AddNode(morkEnv* ev, mork_token inToken, morkNode* ioNode);
  // the AddNode() boolean return equals ev->Good().

  mork_bool  CutNode(morkEnv* ev, mork_token inToken);
  // The CutNode() boolean return indicates whether removal happened. 
  
  morkNode*  GetNode(morkEnv* ev, mork_token inToken);
  // Note the returned node does NOT have an increase in refcount for this.

  mork_num CutAllNodes(morkEnv* ev);
  // CutAllNodes() releases all the reference node values.
};

class morkNodeMapIter: public morkMapIter{ // typesafe wrapper class

public:
  morkNodeMapIter(morkEnv* ev, morkNodeMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkNodeMapIter( ) : morkMapIter()  { }
  void InitNodeMapIter(morkEnv* ev, morkNodeMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstNode(morkEnv* ev, mork_token* outToken, morkNode** outNode)
  { return this->First(ev, outToken, outNode); }
  
  mork_change*
  NextNode(morkEnv* ev, mork_token* outToken, morkNode** outNode)
  { return this->Next(ev, outToken, outNode); }
  
  mork_change*
  HereNode(morkEnv* ev, mork_token* outToken, morkNode** outNode)
  { return this->Here(ev, outToken, outNode); }
  
  mork_change*
  CutHereNode(morkEnv* ev, mork_token* outToken, morkNode** outNode)
  { return this->CutHere(ev, outToken, outNode); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKNODEMAP_ */
