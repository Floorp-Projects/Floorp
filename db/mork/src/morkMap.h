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

#ifndef _MORKMAP_
#define _MORKMAP_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* (These hash methods closely resemble those in public domain IronDoc.) */

/*| Equal: equal for hash table. Note equal(a,b) implies hash(a)==hash(b).
|*/
typedef mork_bool (* morkMap_mEqual)
(const morkMap* self, morkEnv* ev, const void* inKeyA, const void* inKeyB);

/*| Hash: hash for hash table. Note equal(a,b) implies hash(a)==hash(b).
|*/
typedef mork_u4 (* morkMap_mHash)
(const morkMap* self, morkEnv* ev, const void* inKey);

/*| IsNil: whether a key slot contains a "null" value denoting "no such key".
|*/
typedef mork_bool (* morkMap_mIsNil)
(const morkMap* self, morkEnv* ev, const void* inKey);

/*| Note: notify regarding a refcounting change for a key or a value.
|*/
//typedef void (* morkMap_mNote)
//(morkMap* self, morkEnv* ev, void* inKeyOrVal);

/*| morkMapForm: slots need to initialize a new dict.  (This is very similar
**| to the config object for public domain IronDoc hash tables.)
|*/
class morkMapForm { // a struct of callback method pointers for morkMap
public:
  
  // const void*     mMapForm_NilKey;  // externally defined 'nil' bit pattern

  // void*           mMapForm_NilBuf[ 8 ]; // potential place to put NilKey
  // If keys are no larger than 8*sizeof(void*), NilKey can be put in NilBuf.
  // Note this should be true for all Mork subclasses, and we plan usage so.
  
  // These three methods must always be provided, so zero will cause errors:
  
  // morkMap_mEqual  mMapForm_Equal;   // for comparing two keys for identity
  // morkMap_mHash   mMapForm_Hash;    // deterministic key to hash method
  // morkMap_mIsNil  mMapForm_IsNil;   // to query whether a key equals 'nil'
  
  // If any of these method slots are nonzero, then morkMap will call the
  // appropriate one to notify dict users when a key or value is added or cut.
  // Presumably a caller wants to know this in order to perform refcounting or
  // some other kind of memory management.  These methods are definitely only
  // called when references to keys or values are inserted or removed, and are
  // never called when the actual number of references does not change (such
  // as when added keys are already present or cut keys are alreading missing).
  // 
  // morkMap_mNote   mMapForm_AddKey;  // if nonzero, notify about add key
  // morkMap_mNote   mMapForm_CutKey;  // if nonzero, notify about cut key
  // morkMap_mNote   mMapForm_AddVal;  // if nonzero, notify about add val
  // morkMap_mNote   mMapForm_CutVal;  // if nonzero, notify about cut val
  //
  // These note methods have been removed because it seems difficult to
  // guarantee suitable alignment of objects passed to notification methods.
  
  // Note dict clients should pick key and val sizes that provide whatever
  // alignment will be required for an array of such keys and values.
  mork_size       mMapForm_KeySize; // size of every key (cannot be zero)
  mork_size       mMapForm_ValSize; // size of every val (can indeed be zero)
  
  mork_bool       mMapForm_HoldChanges; // support changes array in the map
  mork_change     mMapForm_DummyChange; // change used for false HoldChanges
  mork_bool       mMapForm_KeyIsIP;     // key is mork_ip sized
  mork_bool       mMapForm_ValIsIP;     // key is mork_ip sized
};

/*| morkAssoc: a canonical association slot in a morkMap.  A single assoc
**| instance does nothing except point to the next assoc in the same bucket
**| of a hash table.  Each assoc has only two interesting attributes: 1) the
**| address of the assoc, and 2) the next assoc in a bucket's list.  The assoc
**| address is interesting because in the context of an array of such assocs,
**| one can determine the index of a particular assoc in the array by address
**| arithmetic, subtracting the array address from the assoc address.  And the
**| index of each assoc is the same index as the associated key and val slots
**| in the associated arrays
**|
**|| Think of an assoc instance as really also containing a key slot and a val
**| slot, where each key is mMap_Form.mMapForm_KeySize bytes in size, and
**| each val is mMap_Form.mMapForm_ValSize in size.  But the key and val
**| slots are stored in separate arrays with indexes that are parallel to the
**| indexes in the array of morkAssoc instances.  We have taken the variable
**| sized slots out of the morkAssoc structure, and put them into parallel
**| arrays associated with each morkAssoc by array index.  And this leaves us
**| with only the link field to the next assoc in each assoc instance.
|*/
class morkAssoc {
public:
  morkAssoc*   mAssoc_Next;
};


#define morkDerived_kMap     /*i*/ 0x4D70 /* ascii 'Mp' */

#define morkMap_kTag     /*i*/ 0x6D4D6150 /* ascii 'mMaP' */

/*| morkMap: a hash table based on the public domain IronDoc hash table
**| (which is in turn rather like a similar OpenDoc hash table).
|*/
class morkMap : public morkNode {

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*       mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

public: // state is public because the entire Mork system is private

  nsIMdbHeap*       mMap_Heap; // strong ref to heap allocating all space
  mork_u4           mMap_Tag; // must equal morkMap_kTag

  // When a morkMap instance is constructed, the dict form slots must be
  // provided in order to properly configure a dict with all runtime needs:
  
  morkMapForm       mMap_Form; // construction time parameterization
  
  // Whenever the dict changes structure in a way that would affect any
  // iteration of the dict associations, the seed increments to show this:
  
  mork_seed         mMap_Seed;   // counter for member and structural changes
  
  // The current total assoc capacity of the dict is mMap_Slots, where
  // mMap_Fill of these slots are actually holding content, so mMap_Fill
  // is the actual membership count, and mMap_Slots is how larger membership
  // can become before the hash table must grow the buffers being used. 
  
  mork_count        mMap_Slots;  // count of slots in the hash table
  mork_fill         mMap_Fill;   // number of used slots in the hash table
  
  // Key and value slots are bound to corresponding mMap_Assocs array slots.
  // Instead of having a single array like this: {key,val,next}[ mMap_Slots ]
  // we have instead three parallel arrays with essentially the same meaning:
  // {key}[ mMap_Slots ], {val}[ mMap_Slots ], {assocs}[ mMap_Slots ]
  
  mork_u1*          mMap_Keys;  // mMap_Slots * mMapForm_KeySize buffer
  mork_u1*          mMap_Vals;  // mMap_Slots * mMapForm_ValSize buffer

  // An assoc is "used" when it appears in a bucket's linked list of assocs.
  // Until an assoc is used, it appears in the FreeList linked list.  Every
  // assoc that becomes used goes into the bucket determined by hashing the
  // key associated with a new assoc.  The key associated with a new assoc
  // goes in to the slot in mMap_Keys which occupies exactly the same array
  // index as the array index of the used assoc in the mMap_Assocs array.
  
  morkAssoc*        mMap_Assocs;   // mMap_Slots * sizeof(morkAssoc) buffer
  
  // The changes array is only needed when the

  mork_change*      mMap_Changes;  // mMap_Slots * sizeof(mork_change) buffer
  
  // The Buckets array need not be the same length as the Assocs array, but we
  // usually do it that way so the average bucket depth is no more than one.
  // (We could pick a different policy, or make it parameterizable, but that's
  // tuning we can do some other time.)
  
  morkAssoc**       mMap_Buckets;  // mMap_Slots * sizeof(morkAssoc*) buffer
  
  // The length of the mMap_FreeList should equal (mMap_Slots - mMap_Fill).
  // We need a free list instead of a simpler representation because assocs
  // can be cut and returned to availability in any kind of unknown pattern.
  // (However, when assocs are first allocated, or when the dict is grown, we
  // know all new assocs are contiguous and can chain together adjacently.)
  
  morkAssoc*        mMap_FreeList; // list of unused mMap_Assocs array slots 

public: // getters (morkProbeMap compatibility)
  mork_fill        MapFill() const { return mMap_Fill; }
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseMap() only if open
  virtual ~morkMap(); // assert that CloseMap() executed earlier
  
public: // morkMap construction & destruction
  morkMap(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioNodeHeap, 
    mork_size inKeySize, mork_size inValSize,
    mork_size inSlots, nsIMdbHeap* ioSlotHeap, mork_bool inHoldChanges);
  
  void CloseMap(morkEnv* ev); // called by 
  
public: // dynamic type identification
  mork_bool IsMap() const
  { return IsNode() && mNode_Derived == morkDerived_kMap; }
// } ===== end morkNode methods =====

public: // poly map hash table methods

// { ===== begin morkMap poly interface =====
  virtual mork_bool // note: equal(a,b) implies hash(a) == hash(b)
  Equal(morkEnv* ev, const void* inKeyA, const void* inKeyB) const = 0;

  virtual mork_u4 // note: equal(a,b) implies hash(a) == hash(b)
  Hash(morkEnv* ev, const void* inKey) const = 0;
// } ===== end morkMap poly interface =====

public: // open utitity methods

  mork_bool GoodMapTag() const { return mMap_Tag == morkMap_kTag; }
  mork_bool GoodMap() const
  { return ( IsNode() && GoodMapTag() ); }
  
  void NewIterOutOfSyncError(morkEnv* ev);
  void NewBadMapError(morkEnv* ev);
  void NewSlotsUnderflowWarning(morkEnv* ev);
  void InitMap(morkEnv* ev, mork_size inSlots);

protected: // internal utitity methods

  friend class morkMapIter;
  void clear_map(morkEnv* ev, nsIMdbHeap* ioHeap);

  void* alloc(morkEnv* ev, mork_size inSize);
  void* clear_alloc(morkEnv* ev, mork_size inSize);
  
  void push_free_assoc(morkAssoc* ioAssoc)
  {
    ioAssoc->mAssoc_Next = mMap_FreeList;
    mMap_FreeList = ioAssoc;
  }
  
  morkAssoc* pop_free_assoc()
  {
    morkAssoc* assoc = mMap_FreeList;
    if ( assoc )
      mMap_FreeList = assoc->mAssoc_Next;
    return assoc;
  }

  morkAssoc** find(morkEnv* ev, const void* inKey, mork_u4 inHash) const;
  
  mork_u1* new_keys(morkEnv* ev, mork_num inSlots);
  mork_u1* new_values(morkEnv* ev, mork_num inSlots);
  mork_change* new_changes(morkEnv* ev, mork_num inSlots);
  morkAssoc** new_buckets(morkEnv* ev, mork_num inSlots);
  morkAssoc* new_assocs(morkEnv* ev, mork_num inSlots);
  mork_bool new_arrays(morkEnv* ev, morkHashArrays* old, mork_num inSlots);
  
  mork_bool grow(morkEnv* ev);

  void get_assoc(void* outKey, void* outVal, mork_pos inPos) const;
  void put_assoc(const void* inKey, const void* inVal, mork_pos inPos) const;
  
public: // inlines to form slots
  // const void*     FormNilKey() const { return mMap_Form.mMapForm_NilKey; }
  
  // morkMap_mEqual  FormEqual() const { return mMap_Form.mMapForm_Equal; }
  // morkMap_mHash   FormHash() const { return mMap_Form.mMapForm_Hash; }
  // orkMap_mIsNil  FormIsNil() const { return mMap_Form.mMapForm_IsNil; } 
    
  // morkMap_mNote   FormAddKey() const { return mMap_Form.mMapForm_AddKey; }
  // morkMap_mNote   FormCutKey() const { return mMap_Form.mMapForm_CutKey; }
  // morkMap_mNote   FormAddVal() const { return mMap_Form.mMapForm_AddVal; }
  // morkMap_mNote   FormCutVal() const { return mMap_Form.mMapForm_CutVal; }
  
  mork_size       FormKeySize() const { return mMap_Form.mMapForm_KeySize; }
  mork_size       FormValSize() const { return mMap_Form.mMapForm_ValSize; }

  mork_bool       FormKeyIsIP() const { return mMap_Form.mMapForm_KeyIsIP; }
  mork_bool       FormValIsIP() const { return mMap_Form.mMapForm_ValIsIP; }

  mork_bool       FormHoldChanges() const
  { return mMap_Form.mMapForm_HoldChanges; }
  
  mork_change*    FormDummyChange()
  { return &mMap_Form.mMapForm_DummyChange; }

public: // other map methods
 
  mork_bool Put(morkEnv* ev, const void* inKey, const void* inVal,
    void* outKey, void* outVal, mork_change** outChange);
    
  mork_bool Cut(morkEnv* ev, const void* inKey,
    void* outKey, void* outVal, mork_change** outChange);
    
  mork_bool Get(morkEnv* ev, const void* inKey, 
    void* outKey, void* outVal, mork_change** outChange);
    
  mork_num CutAll(morkEnv* ev);

private: // copying is not allowed
  morkMap(const morkMap& other);
  morkMap& operator=(const morkMap& other);


public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakMap(morkMap* me,
    morkEnv* ev, morkMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongMap(morkMap* me,
    morkEnv* ev, morkMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

/*| morkMapIter: an iterator for morkMap and subclasses.  This is not a node,
**| and expected usage is as a member of some other node subclass, such as in
**| a cursor subclass or a thumb subclass.  Also, iters might be as temp stack
**| objects when scanning the content of a map.
|*/
class morkMapIter{  // iterator for hash table map

protected:
  morkMap*    mMapIter_Map;      // map to iterate, NOT refcounted
  mork_seed   mMapIter_Seed;     // cached copy of map's seed
  
  morkAssoc** mMapIter_Bucket;   // one bucket in mMap_Buckets array
  morkAssoc** mMapIter_AssocRef; // usually *AtRef equals Here
  morkAssoc*  mMapIter_Assoc;    // the current assoc in an iteration
  morkAssoc*  mMapIter_Next;     // mMapIter_Assoc->mAssoc_Next */

public:
  morkMapIter(morkEnv* ev, morkMap* ioMap);
  void CloseMapIter(morkEnv* ev);
 
  morkMapIter( ); // everything set to zero -- need to call InitMapIter()

protected: // we want all subclasses to provide typesafe wrappers:

  void InitMapIter(morkEnv* ev, morkMap* ioMap);
 
  // The morkAssoc returned below is always either mork_change* or
  // else nil (when there is no such assoc).  We return a pointer to
  // the change rather than a simple bool, because callers might
  // want to access change info associated with an assoc.
  
  mork_change* First(morkEnv* ev, void* outKey, void* outVal);
  mork_change* Next(morkEnv* ev, void* outKey, void* outVal);
  mork_change* Here(morkEnv* ev, void* outKey, void* outVal);
  
  mork_change* CutHere(morkEnv* ev, void* outKey, void* outVal);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKMAP_ */
