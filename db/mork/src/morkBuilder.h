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

#ifndef _MORKBUILDER_
#define _MORKBUILDER_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKPARSER_
#include "morkParser.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
 
/*| kCellsVecSize: length of cell vector buffer inside morkBuilder
|*/
#define morkBuilder_kCellsVecSize 64

#define morkBuilder_kDefaultBytesPerParseSegment 512 /* plausible to big */

#define morkDerived_kBuilder     /*i*/ 0x4275 /* ascii 'Bu' */

class morkBuilder /*d*/ : public morkParser {

// public: // slots inherited from morkParser (meant to inform only)
  // nsIMdbHeap*       mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs


  // nsIMdbHeap*      mParser_Heap;   // refcounted heap used for allocation
  // morkStream*   mParser_Stream; // refcounted input stream
    
  // mork_u4       mParser_Tag; // must equal morkParser_kTag
  // mork_count    mParser_MoreGranularity; // constructor inBytesPerParseSegment

  // mork_u4       mParser_State; // state where parser should resume
 
  // after finding ends of group transactions, we can re-seek the start:
  // mork_pos      mParser_GroupContentStartPos; // start of this group
    
  // mork_gid      mParser_GroupId; // group ID if inside a group
  // mork_tid      mParser_TableId; // table ID if inside a table
  // mork_rid      mParser_RowId;   // row ID if inside a row
    
  // mork_bool     mParser_InPort;  // called OnNewPort but not OnPortEnd?
  // mork_bool     mParser_InDict;  // called OnNewDict but not OnDictEnd?
  // mork_bool     mParser_InCell;  // called OnNewCell but not OnCellEnd?
  // mork_bool     mParser_InMeta;  // called OnNewMeta but not OnMetaEnd?
    
  // morkAlias     mParser_Alias;   // current alias being parsed
  // note that mParser_Alias.mAlias_Buf points at mParser_ScopeSpool below:
    
  // blob spools allocated in mParser_Heap
  // morkSpool     mParser_ScopeSpool;   // place to accumulate ID scope blobs
  // morkSpool     mParser_ValueSpool;   // place to accumulate value blobs
  // morkSpool     mParser_ColumnSpool;  // place to accumulate column blobs
  // morkSpool     mParser_StringSpool;  // place to accumulate string blobs
    
  // morkSpoolSink mParser_ScopeSink;  // writes to mParser_ScopeSpool
  // morkSpoolSink mParser_ValueSink;  // writes to mParser_ValueSpool
  // morkSpoolSink mParser_ColumnSink; // writes to mParser_ColumnSpool
  // morkSpoolSink mParser_StringSink; // writes to mParser_StringSpool

  // yarns allocated in mParser_Heap
  // morkYarn      mParser_AliasYarn;   // place to receive from AliasToYarn()
  
  // span showing current ongoing file position status:
  // morkSpan      mParser_PortSpan; // span of current db port file
    
  // various spans denoting nested subspaces inside the file's port span:
  // morkSpan      mParser_GroupSpan; // span of current transaction group
  // morkSpan      mParser_DictSpan;
  // morkSpan      mParser_AliasSpan;
  // morkSpan      mParser_MetaDictSpan;
  // morkSpan      mParser_TableSpan;
  // morkSpan      mParser_MetaTableSpan;
  // morkSpan      mParser_RowSpan;
  // morkSpan      mParser_MetaRowSpan;
  // morkSpan      mParser_CellSpan;
  // morkSpan      mParser_ColumnSpan;
  // morkSpan      mParser_SlotSpan;

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected morkBuilder members
  
  // weak refs that do not prevent closure of referenced nodes:
  morkStore*       mBuilder_Store; // weak ref to builder's store
  
  // strong refs that do indeed prevent closure of referenced nodes:
  morkTable*       mBuilder_Table;    // current table being built (or nil)
  morkRow*         mBuilder_Row;      // current row being built (or nil)
  morkCell*        mBuilder_Cell;     // current cell within CellsVec (or nil)
  
  morkRowSpace*    mBuilder_RowSpace;  // space for mBuilder_CurrentRowScope
  morkAtomSpace*   mBuilder_AtomSpace; // space for mBuilder_CurrentAtomScope
  
  morkAtomSpace*   mBuilder_OidAtomSpace;   // ground atom space for oids
  morkAtomSpace*   mBuilder_ScopeAtomSpace; // ground atom space for scopes
  
  // scoped object ids for current objects under construction:
  mdbOid           mBuilder_TableOid; // full oid for current table
  mdbOid           mBuilder_RowOid;   // full oid for current row
  
  // standard tokens that we want to know about for this port:
  mork_cscode      mBuilder_iso_8859_1; // token for "iso-8859-1"
  mork_cscode      mBuilder_r;          // token for "r"
  mork_cscode      mBuilder_a;          // token for "a"
  mork_cscode      mBuilder_t;          // token for "t"
  
  // tokens that become set as the result of meta cells in port rows:
  mork_cscode      mBuilder_PortForm;       // default port charset format
  mork_scope       mBuilder_PortRowScope;   // port row scope
  mork_scope       mBuilder_PortAtomScope;  // port atom scope

  // tokens that become set as the result of meta cells in meta tables:
  mork_cscode      mBuilder_TableForm;       // default table charset format
  mork_scope       mBuilder_TableRowScope;   // table row scope
  mork_scope       mBuilder_TableAtomScope;  // table atom scope
  mork_kind        mBuilder_TableKind;       // table kind
  
  // tokens that become set as the result of meta cells in meta rows:
  mork_cscode      mBuilder_RowForm;       // default row charset format
  mork_scope       mBuilder_RowRowScope;   // row scope per row metainfo
  mork_scope       mBuilder_RowAtomScope;  // row atom scope

  // meta tokens currently in force, driven by meta info slots above:
  mork_cscode      mBuilder_CurrentForm;       // current charset format
  mork_scope       mBuilder_CurrentRowScope;   // current row scope
  mork_scope       mBuilder_CurrentAtomScope;  // current atom scope
  
  // If any of these 'cut' bools are true, it means a minus was seen in the
  // Mork source text to indicate removal of content from some container.
  // (Note there is no corresponding 'add' bool, since add is the default.)
  // CutRow implies the current row should be cut from the table.
  // CutCell implies the current column should be cut from the row.
  mork_bool        mBuilder_CutRow;    // row with kCut change
  mork_bool        mBuilder_CutCell;   // cell with kCut change
  mork_u1          mBuilder_Pad1;      // pad to u4 alignment
  mork_u1          mBuilder_Pad2;      // pad to u4 alignment
  
  morkCell         mBuilder_CellsVec[ morkBuilder_kCellsVecSize ];
  mork_fill        mBuilder_CellsVecFill; // count used in CellsVec
  // Note when mBuilder_CellsVecFill equals morkBuilder_kCellsVecSize, and 
  // another cell is added, this means all the cells in the vector above
  // must be flushed to the current row being built to create more room.
  
protected: // protected inlines

  mork_bool  CellVectorIsFull() const
  { return ( mBuilder_CellsVecFill == morkBuilder_kCellsVecSize ); };
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseBuilder() only if open
  virtual ~morkBuilder(); // assert that CloseBuilder() executed earlier
  
public: // morkYarn construction & destruction
  morkBuilder(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    morkStream* ioStream,  // the readonly stream for input bytes
    mdb_count inBytesPerParseSegment, // target for ParseMore()
    nsIMdbHeap* ioSlotHeap, morkStore* ioStore
    );
      
  void CloseBuilder(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkBuilder(const morkBuilder& other);
  morkBuilder& operator=(const morkBuilder& other);

public: // dynamic type identification
  mork_bool IsBuilder() const
  { return IsNode() && mNode_Derived == morkDerived_kBuilder; }
// } ===== end morkNode methods =====

public: // typing
  static void NonBuilderTypeError(morkEnv* ev);
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // in virtual morkParser methods, data flow subclass to parser

    virtual void AliasToYarn(morkEnv* ev,
      const morkAlias& inAlias,  // typically an alias to concat with strings
      mdbYarn* outYarn);
    // The parser might ask that some aliases be turned into yarns, so they
    // can be concatenated into longer blobs under some circumstances.  This
    // is an alternative to using a long and complex callback for many parts
    // for a single cell value.
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // out virtual morkParser methods, data flow parser to subclass

    virtual void OnNewPort(morkEnv* ev, const morkPlace& inPlace);
    virtual void OnPortGlitch(morkEnv* ev, const morkGlitch& inGlitch);  
    virtual void OnPortEnd(morkEnv* ev, const morkSpan& inSpan);  

    virtual void OnNewGroup(morkEnv* ev, const morkPlace& inPlace, mork_gid inGid);
    virtual void OnGroupGlitch(morkEnv* ev, const morkGlitch& inGlitch);  
    virtual void OnGroupCommitEnd(morkEnv* ev, const morkSpan& inSpan);  
    virtual void OnGroupAbortEnd(morkEnv* ev, const morkSpan& inSpan);  

    virtual void OnNewPortRow(morkEnv* ev, const morkPlace& inPlace, 
      const morkAlias& inAlias, mork_change inChange);
    virtual void OnPortRowGlitch(morkEnv* ev, const morkGlitch& inGlitch);  
    virtual void OnPortRowEnd(morkEnv* ev, const morkSpan& inSpan);  

    virtual void OnNewTable(morkEnv* ev, const morkPlace& inPlace,
      const morkAlias& inAlias, mork_change inChange);
    virtual void OnTableGlitch(morkEnv* ev, const morkGlitch& inGlitch);
    virtual void OnTableEnd(morkEnv* ev, const morkSpan& inSpan);
      
    virtual void OnNewMeta(morkEnv* ev, const morkPlace& inPlace);
    virtual void OnMetaGlitch(morkEnv* ev, const morkGlitch& inGlitch);
    virtual void OnMetaEnd(morkEnv* ev, const morkSpan& inSpan);

    virtual void OnNewRow(morkEnv* ev, const morkPlace& inPlace, 
      const morkAlias& inAlias, mork_change inChange);
    virtual void OnRowGlitch(morkEnv* ev, const morkGlitch& inGlitch);  
    virtual void OnRowEnd(morkEnv* ev, const morkSpan& inSpan);  

    virtual void OnNewDict(morkEnv* ev, const morkPlace& inPlace);
    virtual void OnDictGlitch(morkEnv* ev, const morkGlitch& inGlitch);  
    virtual void OnDictEnd(morkEnv* ev, const morkSpan& inSpan);  

    virtual void OnAlias(morkEnv* ev, const morkSpan& inSpan,
      const morkAlias& inAlias);

    virtual void OnAliasGlitch(morkEnv* ev, const morkGlitch& inGlitch);

    virtual void OnNewCell(morkEnv* ev, const morkPlace& inPlace,
      const morkAlias& inAlias, mork_change inChange);
    virtual void OnCellGlitch(morkEnv* ev, const morkGlitch& inGlitch);
    virtual void OnCellForm(morkEnv* ev, mork_cscode inCharsetFormat);
    virtual void OnCellEnd(morkEnv* ev, const morkSpan& inSpan);
      
    virtual void OnValue(morkEnv* ev, const morkSpan& inSpan,
      const morkBuf& inBuf);

    virtual void OnValueAlias(morkEnv* ev, const morkSpan& inSpan,
      const morkAlias& inAlias);

    virtual void OnRowAlias(morkEnv* ev, const morkSpan& inSpan,
      const morkAlias& inAlias);

    virtual void OnTableAlias(morkEnv* ev, const morkSpan& inSpan,
      const morkAlias& inAlias);
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly morkBuilder methods
  
  
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakBuilder(morkBuilder* me,
    morkEnv* ev, morkBuilder** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongBuilder(morkBuilder* me,
    morkEnv* ev, morkBuilder** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKBUILDER_ */
