/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _MORKTHUMB_
#define _MORKTHUMB_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


#define morkThumb_kMagic_OpenFilePort               1 /* factory method */
#define morkThumb_kMagic_OpenFileStore              2 /* factory method */
#define morkThumb_kMagic_ExportToFormat             3 /* port method */
#define morkThumb_kMagic_ImportContent              4 /* store method */
#define morkThumb_kMagic_LargeCommit                5 /* store method */
#define morkThumb_kMagic_SessionCommit              6 /* store method */
#define morkThumb_kMagic_CompressCommit             7 /* store method */
#define morkThumb_kMagic_SearchManyColumns          8 /* table method */
#define morkThumb_kMagic_NewSortColumn              9 /* table metho) */
#define morkThumb_kMagic_NewSortColumnWithCompare  10 /* table method */
#define morkThumb_kMagic_CloneSortColumn           11 /* table method */
#define morkThumb_kMagic_AddIndex                  12 /* table method */
#define morkThumb_kMagic_CutIndex                  13 /* table method */

#define morkDerived_kThumb     /*i*/ 0x5468 /* ascii 'Th' */

/*| morkThumb: 
|*/
class morkThumb : public morkObject {

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

public: // state is public because the entire Mork system is private
  // might as well include all the return values here:
  
  mork_magic   mThumb_Magic;   // magic sig different in each thumb type
  mork_count   mThumb_Total;
  mork_count   mThumb_Current;

  mork_bool    mThumb_Done;
  mork_bool    mThumb_Broken;
  mork_u2      mThumb_Seed;  // optional seed for u4 alignment padding
  
  morkStore*   mThumb_Store; // weak ref to created store
  nsIMdbFile*  mThumb_File;  // strong ref to file (store, import, export)
  morkWriter*  mThumb_Writer;  // strong ref to writer (for commit)
  morkBuilder* mThumb_Builder;  // strong ref to builder (for store open)
  morkPort*    mThumb_SourcePort;  // strong ref to port for import
  
  mork_bool    mThumb_DoCollect; // influence whether a collect happens
  mork_bool    mThumb_Pad[ 3 ]; // padding for u4 alignment
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseThumb() only if open
  virtual ~morkThumb(); // assert that CloseThumb() executed earlier
  
public: // morkThumb construction & destruction
  morkThumb(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap, mork_magic inMagic);
  void CloseThumb(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkThumb(const morkThumb& other);
  morkThumb& operator=(const morkThumb& other);

public: // dynamic type identification
  mork_bool IsThumb() const
  { return IsNode() && mNode_Derived == morkDerived_kThumb; }
// } ===== end morkNode methods =====

public: // typing
  static void NonThumbTypeError(morkEnv* ev);
  static void UnsupportedThumbMagicError(morkEnv* ev);

  static void NilThumbStoreError(morkEnv* ev);
  static void NilThumbFileError(morkEnv* ev);
  static void NilThumbWriterError(morkEnv* ev);
  static void NilThumbBuilderError(morkEnv* ev);
  static void NilThumbSourcePortError(morkEnv* ev);

public: // 'do more' methods

  void DoMore_OpenFilePort(morkEnv* ev);
  void DoMore_OpenFileStore(morkEnv* ev);
  void DoMore_ExportToFormat(morkEnv* ev);
  void DoMore_ImportContent(morkEnv* ev);
  void DoMore_LargeCommit(morkEnv* ev);
  void DoMore_SessionCommit(morkEnv* ev);
  void DoMore_CompressCommit(morkEnv* ev);
  void DoMore_Commit(morkEnv* ev);
  void DoMore_SearchManyColumns(morkEnv* ev);
  void DoMore_NewSortColumn(morkEnv* ev);
  void DoMore_NewSortColumnWithCompare(morkEnv* ev);
  void DoMore_CloneSortColumn(morkEnv* ev);
  void DoMore_AddIndex(morkEnv* ev);
  void DoMore_CutIndex(morkEnv* ev);

public: // other thumb methods

  nsIMdbThumb* AcquireThumbHandle(morkEnv* ev); // mObject_Handle

  morkStore* ThumbToOpenStore(morkEnv* ev);
  // for orkinFactory::ThumbToOpenStore() after OpenFileStore()

public: // assorted thumb constructors

  static morkThumb* Make_OpenFileStore(morkEnv* ev, 
    nsIMdbHeap* ioHeap, morkStore* ioStore);

  static morkThumb* Make_CompressCommit(morkEnv* ev, 
    nsIMdbHeap* ioHeap, morkStore* ioStore, mork_bool inDoCollect);

  static morkThumb* Make_LargeCommit(morkEnv* ev, 
    nsIMdbHeap* ioHeap, morkStore* ioStore);

// { ===== begin non-poly methods imitating nsIMdbThumb =====
  void GetProgress(morkEnv* ev, mdb_count* outTotal,
    mdb_count* outCurrent, mdb_bool* outDone, mdb_bool* outBroken);
  
  void DoMore(morkEnv* ev, mdb_count* outTotal,
    mdb_count* outCurrent, mdb_bool* outDone, mdb_bool* outBroken);
  
  void CancelAndBreakThumb(morkEnv* ev);
// } ===== end non-poly methods imitating nsIMdbThumb =====



public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakThumb(morkThumb* me,
    morkEnv* ev, morkThumb** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongThumb(morkThumb* me,
    morkEnv* ev, morkThumb** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKTHUMB_ */
