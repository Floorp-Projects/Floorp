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

// This code is a port to NS Mork from public domain Mithril C++ sources.
// Note many code comments here come verbatim from cut-and-pasted Mithril.
// In many places, code is identical; Mithril versions stay public domain.
// Changes in porting are mainly class type and scalar type name changes.

#ifndef _MORKPROBEMAP_
#define _MORKPROBEMAP_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class morkMapScratch { // utility class used by map subclasses
public:
  nsIMdbHeap*  sMapScratch_Heap;     // cached sMap_Heap
  mork_count   sMapScratch_Slots;    // cached sMap_Slots
  
  mork_u1*     sMapScratch_Keys;     // cached sMap_Keys
  mork_u1*     sMapScratch_Vals;     // cached sMap_Vals
  
public:
  void halt_map_scratch(morkEnv* ev);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kProbeMap   0x7072 /* ascii 'pr' */
#define morkProbeMap_kTag     0x70724D50 /* ascii 'prMP' */

#define morkProbeMap_kLazyClearOnAdd ((mork_u1) 'c')
 
class morkProbeMap: public morkNode {

protected:

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

protected:
  // { begin morkMap slots
  nsIMdbHeap* sMap_Heap; // strong ref to heap allocating all space
    
  mork_u1*    sMap_Keys;
  mork_u1*    sMap_Vals;
  
  mork_count  sMap_Seed;   // change count of members or structure
    
  mork_count  sMap_Slots;  // count of slots in the hash table
  mork_fill   sMap_Fill;   // number of used slots in the hash table

  mork_size   sMap_KeySize; // size of each key (cannot be zero)
  mork_size   sMap_ValSize; // size of each val (zero allowed)
  
  mork_bool   sMap_KeyIsIP;     // sMap_KeySize == sizeof(mork_ip)
  mork_bool   sMap_ValIsIP;     // sMap_ValSize == sizeof(mork_ip)
  mork_u1     sMap_Pad[ 2 ];    // for u4 alignment
  // } end morkMap slots
    
  friend class morkProbeMapIter; // for access to protected slots

public: // getters
  mork_count  MapSeed() const { return sMap_Seed; }
    
  mork_count  MapSlots() const { return sMap_Slots; }
  mork_fill   MapFill() const { return sMap_Fill; }

  mork_size   MapKeySize() const { return sMap_KeySize; }
  mork_size   MapValSize() const { return sMap_ValSize; }
  
  mork_bool   MapKeyIsIP() const { return sMap_KeyIsIP; }
  mork_bool   MapValIsIP() const { return sMap_ValIsIP; }

protected: // slots
  // { begin morkProbeMap slots
   
  mork_fill   sProbeMap_MaxFill; // max sMap_Fill before map must grow
  
  mork_u1     sProbeMap_LazyClearOnAdd; // true if kLazyClearOnAdd
  mork_bool   sProbeMap_ZeroIsClearKey; // zero is adequate to clear keys
  mork_u1     sProbeMap_Pad[ 2 ]; // for u4 alignment
  
  mork_u4     sProbeMap_Tag; 
 
  // } end morkProbeMap slots
    
public: // lazy clear on add

  mork_bool need_lazy_init() const 
  { return sProbeMap_LazyClearOnAdd == morkProbeMap_kLazyClearOnAdd; }

public: // typing
  mork_bool   GoodProbeMap() const
  { return sProbeMap_Tag == morkProbeMap_kTag; }
    
protected: // utilities

  void* clear_alloc(morkEnv* ev, mork_size inSize);

  mork_u1*    map_new_vals(morkEnv* ev, mork_num inSlots);
  mork_u1*    map_new_keys(morkEnv* ev, mork_num inSlots);

  void clear_probe_map(morkEnv* ev, nsIMdbHeap* ioMapHeap);
  void init_probe_map(morkEnv* ev, mork_size inSlots);
  void probe_map_lazy_init(morkEnv* ev);

  mork_bool new_slots(morkEnv* ev, morkMapScratch* old, mork_num inSlots);
  
  mork_test find_key_pos(morkEnv* ev, const void* inAppKey,
    mork_u4 inHash, mork_pos* outPos) const;
  
  void put_probe_kv(morkEnv* ev,
    const void* inAppKey, const void* inAppVal, mork_pos inPos);
  void get_probe_kv(morkEnv* ev,
    void* outAppKey, void* outAppVal, mork_pos inPos) const;
    
  mork_bool grow_probe_map(morkEnv* ev);
  void      rehash_old_map(morkEnv* ev, morkMapScratch* ioScratch);
  void      revert_map(morkEnv* ev, morkMapScratch* ioScratch);

public: // errors
  void ProbeMapBadTagError(morkEnv* ev) const;
  void WrapWithNoVoidSlotError(morkEnv* ev) const;
  void GrowFailsMaxFillError(morkEnv* ev) const;
  void MapKeyIsNotIPError(morkEnv* ev) const;
  void MapValIsNotIPError(morkEnv* ev) const;

  void MapNilKeysError(morkEnv* ev);
  void MapZeroKeySizeError(morkEnv* ev);

  void MapSeedOutOfSyncError(morkEnv* ev);
  void MapFillUnderflowWarning(morkEnv* ev);

  static void ProbeMapCutError(morkEnv* ev);

  // { ===== begin morkMap methods =====
public:

  virtual mork_test // hit(a,b) implies hash(a) == hash(b)
  MapTest(morkEnv* ev, const void* inMapKey, const void* inAppKey) const;
    // Note inMapKey is always a key already stored in the map, while inAppKey
    //   is always a method argument parameter from a client method call.
    //   This matters the most in morkProbeMap subclasses, which have the
    //   responsibility of putting 'app' keys into slots for 'map' keys, and
    //   the bit pattern representation might be different in such cases.
    // morkTest_kHit means that inMapKey equals inAppKey (and this had better
    //   also imply that hash(inMapKey) == hash(inAppKey)).
    // morkTest_kMiss means that inMapKey does NOT equal inAppKey (but this
    //   implies nothing at all about hash(inMapKey) and hash(inAppKey)).
    // morkTest_kVoid means that inMapKey is not a valid key bit pattern,
    //   which means that key slot in the map is not being used.  Note that
    //   kVoid is only expected as a return value in morkProbeMap subclasses,
    //   because morkProbeMap must ask whether a key slot is used or not. 
    //   morkChainMap however, always knows when a key slot is used, so only
    //   key slots expected to have valid bit patterns will be presented to
    //   the MapTest() methods for morkChainMap subclasses.
    //
    // NOTE: it is very important that subclasses correctly return the value
    // morkTest_kVoid whenever the slot for inMapKey contains a bit pattern
    // that means the slot is not being used, because this is the only way a
    // probe map can terminate an unsuccessful search for a key in the map.

  virtual mork_u4 // hit(a,b) implies hash(a) == hash(b)
  MapHash(morkEnv* ev, const void* inAppKey) const;

  virtual mork_bool
  MapAtPut(morkEnv* ev, const void* inAppKey, const void* inAppVal,
    void* outAppKey, void* outAppVal);
    
  virtual mork_bool
  MapAt(morkEnv* ev, const void* inAppKey, void* outAppKey, void* outAppVal);
    
  virtual mork_num
  MapCutAll(morkEnv* ev);
  // } ===== end morkMap methods =====

    
  // { ===== begin morkProbeMap methods =====
public:

  virtual mork_u4
  ProbeMapHashMapKey(morkEnv* ev, const void* inMapKey) const;
    // ProbeMapHashMapKey() does logically the same thing as MapHash(), and
    // the default implementation actually calls virtual MapHash().  However,
    // Subclasses must override this method whenever the formats of keys in
    // the map differ from app keys outside the map, because MapHash() only
    // works on keys in 'app' format, while ProbeMapHashMapKey() only works
    // on keys in 'map' format.  This method is called in order to rehash all
    // map keys when a map is grown, and this causes all old map members to
    // move into new slot locations.
    //
    // Note it is absolutely imperative that a hash for a key in 'map' format
    // be exactly the same the hash of the same key in 'app' format, or else
    // maps will seem corrupt later when keys in 'app' format cannot be found.

  virtual mork_bool
  ProbeMapIsKeyNil(morkEnv* ev, void* ioMapKey);
    // ProbeMapIsKeyNil() must say whether the representation of logical 'nil'
    // is currently found inside the key at ioMapKey, for a key found within
    // the map.  The the map iterator uses this method to find map keys that
    // are actually being used for valid map associations; otherwise the
    // iterator cannot determine which map slots actually denote used keys.
    // The default method version returns true if all the bits equal zero.

  virtual void
  ProbeMapClearKey(morkEnv* ev, // put 'nil' alls keys inside map
    void* ioMapKey, mork_count inKeyCount); // array of keys inside map
    // ProbeMapClearKey() must put some representation of logical 'nil' into
    // every key slot in the map, such that MapTest() will later recognize
    // that this bit pattern shows each key slot is not actually being used.
    //
    // This method is typically called whenever the map is either created or
    // grown into a larger size, where ioMapKey is a pointer to an array of
    // inKeyCount keys, where each key is this->MapKeySize() bytes in size.
    // Note that keys are assumed immediately adjacent with no padding, so
    // if any alignment requirements must be met, then subclasses should have
    // already accounted for this when specifying a key size in the map.
    //
    // Since this method will be called when a map is being grown in size,
    // nothing should be assumed about the state slots of the map, since the
    // ioMapKey array might not yet live in sMap_Keys, and the array length
    // inKeyCount might not yet live in sMap_Slots.  However, the value kept
    // in sMap_KeySize never changes, so this->MapKeySize() is always correct.

  virtual void
  ProbeMapPushIn(morkEnv* ev, // move (key,val) into the map
    const void* inAppKey, const void* inAppVal, // (key,val) outside map
    void* outMapKey, void* outMapVal);      // (key,val) inside map
    // This method actually puts keys and vals in the map in suitable format.
    //
    // ProbeMapPushIn() must copy a caller key and value in 'app' format
    // into the map slots provided, which are in 'map' format.  When the
    // 'app' and 'map' formats are identical, then this is just a bitwise
    // copy of this->MapKeySize() key bytes and this->MapValSize() val bytes,
    // and this is exactly what the default implementation performs.  However,
    // if 'app' and 'map' formats are different, and MapTest() depends on this
    // difference in format, then subclasses must override this method to do
    // whatever is necessary to store the input app key in output map format.
    //
    // Do NOT write more than this->MapKeySize() bytes of a map key, or more
    // than this->MapValSize() bytes of a map val, or corruption might ensue.
    //
    // The inAppKey and inAppVal parameters are the same ones passed into a
    // call to MapAtPut(), and the outMapKey and outMapVal parameters are ones
    // determined by how the map currently positions key inAppKey in the map.
    //
    // Note any key or val parameter can be a null pointer, in which case
    // this method must do nothing with those parameters.  In particular, do
    // no key move at all when either inAppKey or outMapKey is nil, and do
    // no val move at all when either inAppVal or outMapVal is nil.  Note that
    // outMapVal should always be nil when this->MapValSize() is nil.

  virtual void
  ProbeMapPullOut(morkEnv* ev, // move (key,val) out from the map
    const void* inMapKey, const void* inMapVal, // (key,val) inside map
    void* outAppKey, void* outAppVal) const;    // (key,val) outside map
    // This method actually gets keys and vals from the map in suitable format.
    //
    // ProbeMapPullOut() must copy a key and val in 'map' format into the
    // caller key and val slots provided, which are in 'app' format.  When the
    // 'app' and 'map' formats are identical, then this is just a bitwise
    // copy of this->MapKeySize() key bytes and this->MapValSize() val bytes,
    // and this is exactly what the default implementation performs.  However,
    // if 'app' and 'map' formats are different, and MapTest() depends on this
    // difference in format, then subclasses must override this method to do
    // whatever is necessary to store the input map key in output app format.
    //
    // The outAppKey and outAppVal parameters are the same ones passed into a
    // call to either MapAtPut() or MapAt(), while inMapKey and inMapVal are
    // determined by how the map currently positions the target key in the map.
    //
    // Note any key or val parameter can be a null pointer, in which case
    // this method must do nothing with those parameters.  In particular, do
    // no key move at all when either inMapKey or outAppKey is nil, and do
    // no val move at all when either inMapVal or outAppVal is nil.  Note that
    // inMapVal should always be nil when this->MapValSize() is nil.
  
  // } ===== end morkProbeMap methods =====
    
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseProbeMap() only if open
  virtual ~morkProbeMap(); // assert that CloseProbeMap() executed earlier
  
public: // morkProbeMap construction & destruction
  morkProbeMap(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioNodeHeap,
  mork_size inKeySize, mork_size inValSize,
  nsIMdbHeap* ioMapHeap, mork_size inSlots,
  mork_bool inZeroIsClearKey);
  
  void CloseProbeMap(morkEnv* ev); // called by 
  
public: // dynamic type identification
  mork_bool IsProbeMap() const
  { return IsNode() && mNode_Derived == morkDerived_kProbeMap; }
// } ===== end morkNode methods =====

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakMap(morkMap* me,
    morkEnv* ev, morkMap** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongMap(morkMap* me,
    morkEnv* ev, morkMap** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

/*============================================================================*/
/* morkProbeMapIter */

#define morkProbeMapIter_kBeforeIx ((mork_i4) -1) /* before first member */
#define morkProbeMapIter_kAfterIx  ((mork_i4) -2) /* after last member */

class morkProbeMapIter {

protected:
  morkProbeMap* sProbeMapIter_Map;      // nonref
  mork_num      sProbeMapIter_Seed;     // iter's cached copy of map's seed
  
  mork_i4       sProbeMapIter_HereIx;
  
  mork_change   sProbeMapIter_Change;   // morkMapIter API simulation dummy
  mork_u1       sProbeMapIter_Pad[ 3 ]; // for u4 alignment
  
public:
  morkProbeMapIter(morkEnv* ev, morkProbeMap* ioMap);
  void CloseMapIter(morkEnv* ev);
 
  morkProbeMapIter( ); // zero most slots; caller must call InitProbeMapIter()

protected: // protected so subclasses must provide suitable typesafe inlines:

  void InitProbeMapIter(morkEnv* ev, morkProbeMap* ioMap);
   
  void InitMapIter(morkEnv* ev, morkProbeMap* ioMap) // morkMapIter compatibility
  { this->InitProbeMapIter(ev, ioMap); }
   
  mork_bool IterFirst(morkEnv* ev, void* outKey, void* outVal);
  mork_bool IterNext(morkEnv* ev, void* outKey, void* outVal);
  mork_bool IterHere(morkEnv* ev, void* outKey, void* outVal);
   
  // NOTE: the following methods ONLY work for sMap_ValIsIP pointer values.
  // (Note the implied assumption that zero is never a good value pattern.)
  
  void*     IterFirstVal(morkEnv* ev, void* outKey);
  // equivalent to { void* v=0; this->IterFirst(ev, outKey, &v); return v; }
  
  void*     IterNextVal(morkEnv* ev, void* outKey);
  // equivalent to { void* v=0; this->IterNext(ev, outKey, &v); return v; }

  void*     IterHereVal(morkEnv* ev, void* outKey);
  // equivalent to { void* v=0; this->IterHere(ev, outKey, &v); return v; }

  // NOTE: the following methods ONLY work for sMap_KeyIsIP pointer values.
  // (Note the implied assumption that zero is never a good key pattern.)
  
  void*     IterFirstKey(morkEnv* ev);
  // equivalent to { void* k=0; this->IterFirst(ev, &k, 0); return k; }
  
  void*     IterNextKey(morkEnv* ev);
  // equivalent to { void* k=0; this->IterNext(ev, &k, 0); return k; }

  void*     IterHereKey(morkEnv* ev);
  // equivalent to { void* k=0; this->IterHere(ev, &k, 0); return k; }

public: // simulation of the morkMapIter API for morkMap compatibility:  
  mork_change* First(morkEnv* ev, void* outKey, void* outVal);
  mork_change* Next(morkEnv* ev, void* outKey, void* outVal);
  mork_change* Here(morkEnv* ev, void* outKey, void* outVal);
  
  mork_change* CutHere(morkEnv* ev, void* outKey, void* outVal);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKPROBEMAP_ */
