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

#ifndef _MORKPARSER_
#define _MORKPARSER_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

#ifndef _MORKSINK_
#include "morkSink.h"
#endif

#ifndef _MORKYARN_
#include "morkYarn.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
 
/*=============================================================================
 * morkPlace: stream byte position and stream line count
 */

class morkPlace {
public:
  mork_pos   mPlace_Pos;   // byte offset in an input stream
  mork_line  mPlace_Line;  // line count in an input stream
  
  void ClearPlace()
  {
    mPlace_Pos = 0; mPlace_Line = 0;
  }

  void SetPlace(mork_pos inPos, mork_line inLine)
  {
    mPlace_Pos = inPos; mPlace_Line = inLine;
  }

  morkPlace() { mPlace_Pos = 0; mPlace_Line = 0; }
  
  morkPlace(mork_pos inPos, mork_line inLine)
  { mPlace_Pos = inPos; mPlace_Line = inLine; }
  
  morkPlace(const morkPlace& inPlace)
  : mPlace_Pos(inPlace.mPlace_Pos), mPlace_Line(inPlace.mPlace_Line) { }
};

/*=============================================================================
 * morkGlitch: stream place and error comment describing a parsing error
 */

class morkGlitch {
public:
  morkPlace    mGlitch_Place;   // place in stream where problem happened
  const char*  mGlitch_Comment; // null-terminated ASCII C string

  morkGlitch() { mGlitch_Comment = 0; }
  
  morkGlitch(const morkPlace& inPlace, const char* inComment)
  : mGlitch_Place(inPlace), mGlitch_Comment(inComment) { }
};

/*=============================================================================
 * morkMid: all possible ways needed to express an alias ID in Mork syntax
 */

/*| morkMid: an abstraction of all the variations we might need to support
**| in order to present an ID through the parser interface most cheaply and
**| with minimum transformation away from the original text format.
**|
**|| An ID can have one of four forms:
**| 1) idHex            (mMid_Oid.mOid_Id <- idHex)
**| 2) idHex:^scopeHex  (mMid_Oid.mOid_Id <- idHex, mOid_Scope <- scopeHex)
**| 3) idHex:scopeName  (mMid_Oid.mOid_Id <- idHex, mMid_Buf <- scopeName)
**| 4) columnName       (mMid_Buf <- columnName, for columns in cells only)
**|
**|| Typically, mMid_Oid.mOid_Id will hold a nonzero integer value for
**| an ID, but we might have an optional scope specified by either an integer
**| in hex format, or a string name.  (Note that while the first ID can be
**| scoped variably, any integer ID for a scope is assumed always located in
**| the same scope, so the second ID need not be disambiguated.)
**|
**|| The only time mMid_Oid.mOid_Id is ever zero is when mMid_Buf alone
**| is nonzero, to indicate an explicit string instead of an alias appeared.
**| This case happens to make the representation of columns in cells somewhat
**| easier to represent, since columns can just appear as a string name; and
**| this unifies those interfaces with row and table APIs expecting IDs.
**|
**|| So when the parser passes an instance of morkMid to a subclass, the 
**| mMid_Oid.mOid_Id slot should usually be nonzero.  And the other two
**| slots, mMid_Oid.mOid_Scope and mMid_Buf, might both be zero, or at
**| most one of them will be nonzero to indicate an explicit scope; the
**| parser is responsible for ensuring at most one of these is nonzero.
|*/
class morkMid {
public:
  mdbOid          mMid_Oid;  // mOid_Scope is zero when not specified
  const morkBuf*  mMid_Buf;  // points to some specific buf subclass
  
  morkMid()
  { mMid_Oid.mOid_Scope = 0; mMid_Oid.mOid_Id = morkId_kMinusOne;
   mMid_Buf = 0; }
  
  void InitMidWithCoil(morkCoil* ioCoil)
  { mMid_Oid.mOid_Scope = 0; mMid_Oid.mOid_Id = morkId_kMinusOne;
   mMid_Buf = ioCoil; }
    
  void ClearMid()
  { mMid_Oid.mOid_Scope = 0; mMid_Oid.mOid_Id = morkId_kMinusOne;
   mMid_Buf = 0; }

  morkMid(const morkMid& other)
  : mMid_Oid(other.mMid_Oid), mMid_Buf(other.mMid_Buf) { }
  
  mork_bool HasNoId() const // ID is unspecified?
  { return ( mMid_Oid.mOid_Id == morkId_kMinusOne ); }
  
  mork_bool HasSomeId() const // ID is specified?
  { return ( mMid_Oid.mOid_Id != morkId_kMinusOne ); }
};

/*=============================================================================
 * morkSpan: start and end stream byte position and stream line count
 */

class morkSpan {
public:
  morkPlace   mSpan_Start;
  morkPlace   mSpan_End;

public: // methods
  
public: // inlines
  morkSpan() { } // use inline empty constructor for each place
  
  morkPlace* AsPlace() { return &mSpan_Start; }
  const morkPlace* AsConstPlace() const { return &mSpan_Start; }
  
  void SetSpan(mork_pos inFromPos, mork_line inFromLine,
    mork_pos inToPos, mork_line inToLine)
  {
    mSpan_Start.SetPlace(inFromPos, inFromLine);
    mSpan_End.SetPlace(inToPos,inToLine);
  }

  // setting end, useful to terminate a span using current port span end:
  void SetEndWithEnd(const morkSpan& inSpan) // end <- span.end
  { mSpan_End = inSpan.mSpan_End; }

  // setting start, useful to initiate a span using current port span end:
  void SetStartWithEnd(const morkSpan& inSpan) // start <- span.end
  { mSpan_Start = inSpan.mSpan_End; }
  
  void ClearSpan()
  {
    mSpan_Start.mPlace_Pos = 0; mSpan_Start.mPlace_Line = 0;
    mSpan_End.mPlace_Pos = 0; mSpan_End.mPlace_Line = 0;
  }

  morkSpan(mork_pos inFromPos, mork_line inFromLine,
    mork_pos inToPos, mork_line inToLine)
  : mSpan_Start(inFromPos, inFromLine), mSpan_End(inToPos, inToLine)
  {  /* empty implementation */ }
};

/*=============================================================================
 * morkParser: for parsing Mork text syntax
 */

#define morkParser_kMinGranularity 512 /* parse at least half 0.5K at once */
#define morkParser_kMaxGranularity (64 * 1024) /* parse at most 64 K at once */

#define morkDerived_kParser     /*i*/ 0x5073 /* ascii 'Ps' */
#define morkParser_kTag     /*i*/ 0x70417253 /* ascii 'pArS' */

// These are states for the simple parsing virtual machine.  Needless to say,
// these must be distinct, and preferrably in a contiguous integer range.
// Don't change these constants without looking at switch statements in code.
#define morkParser_kCellState      0 /* cell is tightest scope */
#define morkParser_kMetaState      1 /* meta is tightest scope */
#define morkParser_kRowState       2 /* row is tightest scope */
#define morkParser_kTableState     3 /* table is tightest scope */
#define morkParser_kDictState      4 /* dict is tightest scope */
#define morkParser_kPortState      5 /* port is tightest scope */

#define morkParser_kStartState     6 /* parsing has not yet begun */
#define morkParser_kDoneState      7 /* parsing is complete */
#define morkParser_kBrokenState    8 /* parsing is to broken to work */

class morkParser /*d*/ : public morkNode {

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

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected morkParser members
  
  nsIMdbHeap*   mParser_Heap;   // refcounted heap used for allocation
  morkStream*   mParser_Stream; // refcounted input stream

  mork_u4       mParser_Tag; // must equal morkParser_kTag
  mork_count    mParser_MoreGranularity; // constructor inBytesPerParseSegment

  mork_u4       mParser_State; // state where parser should resume

  // after finding ends of group transactions, we can re-seek the start:
  mork_pos      mParser_GroupContentStartPos; // start of this group

  morkMid       mParser_TableMid; // table mid if inside a table
  morkMid       mParser_RowMid;   // row mid if inside a row
  morkMid       mParser_CellMid;  // cell mid if inside a row
  mork_gid      mParser_GroupId;  // group ID if inside a group

  mork_bool     mParser_InPort;  // called OnNewPort but not OnPortEnd?
  mork_bool     mParser_InDict;  // called OnNewDict but not OnDictEnd?
  mork_bool     mParser_InCell;  // called OnNewCell but not OnCellEnd?
  mork_bool     mParser_InMeta;  // called OnNewMeta but not OnMetaEnd?

  mork_bool     mParser_InPortRow;  // called OnNewPortRow but not OnPortRowEnd?
  mork_bool     mParser_InRow;    // called OnNewRow but not OnNewRowEnd?
  mork_bool     mParser_InTable;  // called OnNewMeta but not OnMetaEnd?
  mork_bool     mParser_InGroup;  // called OnNewGroup but not OnGroupEnd?

  mork_change   mParser_AtomChange;  // driven by mParser_Change 
  mork_change   mParser_CellChange;  // driven by mParser_Change 
  mork_change   mParser_RowChange;   // driven by mParser_Change 
  mork_change   mParser_TableChange; // driven by mParser_Change 

  mork_change   mParser_Change;     // driven by modifier in text 
  mork_bool     mParser_IsBroken;   // has the parse become broken?
  mork_bool     mParser_IsDone;     // has the parse finished?
  mork_bool     mParser_DoMore;     // mParser_MoreGranularity not exhausted? 

  morkMid       mParser_Mid;   // current alias being parsed
  // note that mParser_Mid.mMid_Buf points at mParser_ScopeCoil below:

  // blob coils allocated in mParser_Heap
  morkCoil     mParser_ScopeCoil;   // place to accumulate ID scope blobs
  morkCoil     mParser_ValueCoil;   // place to accumulate value blobs
  morkCoil     mParser_ColumnCoil;  // place to accumulate column blobs
  morkCoil     mParser_StringCoil;  // place to accumulate string blobs

  morkSpool    mParser_ScopeSpool;  // writes to mParser_ScopeCoil
  morkSpool    mParser_ValueSpool;  // writes to mParser_ValueCoil
  morkSpool    mParser_ColumnSpool; // writes to mParser_ColumnCoil
  morkSpool    mParser_StringSpool; // writes to mParser_StringCoil

  // yarns allocated in mParser_Heap
  morkYarn      mParser_MidYarn;   // place to receive from MidToYarn()

  // span showing current ongoing file position status:
  morkSpan      mParser_PortSpan; // span of current db port file

  // various spans denoting nested subspaces inside the file's port span:
  morkSpan      mParser_GroupSpan; // span of current transaction group
  morkSpan      mParser_DictSpan;
  morkSpan      mParser_AliasSpan;
  morkSpan      mParser_MetaSpan;
  morkSpan      mParser_TableSpan;
  morkSpan      mParser_RowSpan;
  morkSpan      mParser_CellSpan;
  morkSpan      mParser_ColumnSpan;
  morkSpan      mParser_SlotSpan;

private: // convenience inlines

  mork_pos HerePos() const
  { return mParser_PortSpan.mSpan_End.mPlace_Pos; }

  void SetHerePos(mork_pos inPos)
  { mParser_PortSpan.mSpan_End.mPlace_Pos = inPos; }

  void CountLineBreak()
  { ++mParser_PortSpan.mSpan_End.mPlace_Line; }
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseParser() only if open
  virtual ~morkParser(); // assert that CloseParser() executed earlier
  
public: // morkYarn construction & destruction
  morkParser(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    morkStream* ioStream,  // the readonly stream for input bytes
    mdb_count inBytesPerParseSegment, // target for ParseMore()
    nsIMdbHeap* ioSlotHeap);
      
  void CloseParser(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkParser(const morkParser& other);
  morkParser& operator=(const morkParser& other);

public: // dynamic type identification
  mork_bool IsParser() const
  { return IsNode() && mNode_Derived == morkDerived_kParser; }
  
// } ===== end morkNode methods =====

public: // errors and warnings
  static void UnexpectedEofError(morkEnv* ev);
  static void EofInsteadOfHexError(morkEnv* ev);
  static void ExpectedEqualError(morkEnv* ev);
  static void ExpectedHexDigitError(morkEnv* ev, int c);
  static void NonParserTypeError(morkEnv* ev);
  static void UnexpectedByteInMetaWarning(morkEnv* ev);

public: // other type methods
  mork_bool GoodParserTag() const { return mParser_Tag == morkParser_kTag; }
  void NonGoodParserError(morkEnv* ev);
  void NonUsableParserError(morkEnv* ev);
  // call when IsNode() or GoodParserTag() is false
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // in virtual morkParser methods, data flow subclass to parser

    virtual void MidToYarn(morkEnv* ev,
      const morkMid& inMid,  // typically an alias to concat with strings
      mdbYarn* outYarn) = 0;
    // The parser might ask that some aliases be turned into yarns, so they
    // can be concatenated into longer blobs under some circumstances.  This
    // is an alternative to using a long and complex callback for many parts
    // for a single cell value.
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // out virtual morkParser methods, data flow parser to subclass

// The virtual methods below will be called in a pattern corresponding
// to the following grammar isomorphic to the Mork grammar.  There should
// be no exceptions, so subclasses can rely on seeing an appropriate "end"
// method whenever some "new" method has been seen earlier.  In the event
// that some error occurs that causes content to be flushed, or sudden early
// termination of a larger containing entity, we will always call a more
// enclosed "end" method before we call an "end" method with greater scope.

// Note the "mp" prefix stands for "Mork Parser":

// mp:Start     ::= OnNewPort mp:PortItem* OnPortEnd
// mp:PortItem  ::= mp:Content | mp:Group | OnPortGlitch
// mp:Group     ::= OnNewGroup mp:GroupItem* mp:GroupEnd
// mp:GroupItem ::= mp:Content | OnGroupGlitch
// mp:GroupEnd  ::= OnGroupCommitEnd | OnGroupAbortEnd
// mp:Content   ::= mp:PortRow | mp:Dict | mp:Table | mp:Row
// mp:PortRow   ::= OnNewPortRow mp:RowItem* OnPortRowEnd
// mp:Dict      ::= OnNewDict mp:DictItem* OnDictEnd
// mp:DictItem  ::= OnAlias | OnAliasGlitch | mp:Meta | OnDictGlitch
// mp:Table     ::= OnNewTable mp:TableItem* OnTableEnd
// mp:TableItem ::= mp:Row | mp:MetaTable | OnTableGlitch
// mp:MetaTable ::= OnNewMeta mp:MetaItem* mp:Row OnMetaEnd
// mp:Meta      ::= OnNewMeta mp:MetaItem* OnMetaEnd
// mp:MetaItem  ::= mp:Cell | OnMetaGlitch
// mp:Row       ::= OnNewRow mp:RowItem* OnRowEnd
// mp:RowItem   ::= mp:Cell | mp:Meta | OnRowGlitch
// mp:Cell      ::= OnNewCell mp:CellItem? OnCellEnd
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid


  // Note that in interfaces below, mork_change parameters kAdd and kNil
  // both mean about the same thing by default.  Only kCut is interesting,
  // because this usually means to remove members instead of adding them.

  virtual void OnNewPort(morkEnv* ev, const morkPlace& inPlace) = 0;
  virtual void OnPortGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;  
  virtual void OnPortEnd(morkEnv* ev, const morkSpan& inSpan) = 0;  

  virtual void OnNewGroup(morkEnv* ev, const morkPlace& inPlace, mork_gid inGid) = 0;
  virtual void OnGroupGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;  
  virtual void OnGroupCommitEnd(morkEnv* ev, const morkSpan& inSpan) = 0;  
  virtual void OnGroupAbortEnd(morkEnv* ev, const morkSpan& inSpan) = 0;  

  virtual void OnNewPortRow(morkEnv* ev, const morkPlace& inPlace, 
    const morkMid& inMid, mork_change inChange) = 0;
  virtual void OnPortRowGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;  
  virtual void OnPortRowEnd(morkEnv* ev, const morkSpan& inSpan) = 0;  

  virtual void OnNewTable(morkEnv* ev, const morkPlace& inPlace,
    const morkMid& inMid, mork_change inChange) = 0;
  virtual void OnTableGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;
  virtual void OnTableEnd(morkEnv* ev, const morkSpan& inSpan) = 0;
    
  virtual void OnNewMeta(morkEnv* ev, const morkPlace& inPlace) = 0;
  virtual void OnMetaGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;
  virtual void OnMetaEnd(morkEnv* ev, const morkSpan& inSpan) = 0;

  virtual void OnNewRow(morkEnv* ev, const morkPlace& inPlace, 
    const morkMid& inMid, mork_change inChange) = 0;
  virtual void OnRowGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;  
  virtual void OnRowEnd(morkEnv* ev, const morkSpan& inSpan) = 0;  

  virtual void OnNewDict(morkEnv* ev, const morkPlace& inPlace) = 0;
  virtual void OnDictGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;  
  virtual void OnDictEnd(morkEnv* ev, const morkSpan& inSpan) = 0;  

  virtual void OnAlias(morkEnv* ev, const morkSpan& inSpan,
    const morkMid& inMid) = 0;

  virtual void OnAliasGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;

  virtual void OnNewCell(morkEnv* ev, const morkPlace& inPlace,
    const morkMid* inMid, const morkBuf* inBuf, mork_change inChange) = 0;
  // Exactly one of inMid and inBuf is nil, and the other is non-nil.
  // When hex ID syntax is used for a column, then inMid is not nil, and
  // when a naked string names a column, then inBuf is not nil.
    
  virtual void OnCellGlitch(morkEnv* ev, const morkGlitch& inGlitch) = 0;
  virtual void OnCellForm(morkEnv* ev, mork_cscode inCharsetFormat) = 0;
  virtual void OnCellEnd(morkEnv* ev, const morkSpan& inSpan) = 0;
    
  virtual void OnValue(morkEnv* ev, const morkSpan& inSpan,
    const morkBuf& inBuf) = 0;

  virtual void OnValueMid(morkEnv* ev, const morkSpan& inSpan,
    const morkMid& inMid) = 0;

  virtual void OnRowMid(morkEnv* ev, const morkSpan& inSpan,
    const morkMid& inMid) = 0;

  virtual void OnTableMid(morkEnv* ev, const morkSpan& inSpan,
    const morkMid& inMid) = 0;
  
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected parser helper methods

  void ParseLoop(morkEnv* ev); // find parse continuation and resume

  void StartParse(morkEnv* ev); // prepare for parsing
  void StopParse(morkEnv* ev); // terminate parsing & call needed methods

  int NextChar(morkEnv* ev); // next non-white content

  void OnCellState(morkEnv* ev);
  void OnMetaState(morkEnv* ev);
  void OnRowState(morkEnv* ev);
  void OnTableState(morkEnv* ev);
  void OnDictState(morkEnv* ev);
  void OnPortState(morkEnv* ev);
  void OnStartState(morkEnv* ev);
  
  void ReadCell(morkEnv* ev);
  void ReadRow(morkEnv* ev, int c);
  void ReadTable(morkEnv* ev);
  void ReadTableMeta(morkEnv* ev);
  void ReadDict(morkEnv* ev);
  void ReadMeta(morkEnv* ev, int inEndMeta);
  void ReadAlias(morkEnv* ev);
  mork_id ReadHex(morkEnv* ev, int* outNextChar);
  morkBuf* ReadValue(morkEnv* ev);
  morkBuf* ReadName(morkEnv* ev, int c);
  mork_bool ReadMid(morkEnv* ev, morkMid* outMid);
  
  void EndSpanOnThisByte(morkEnv* ev, morkSpan* ioSpan);
  void StartSpanOnLastByte(morkEnv* ev, morkSpan* ioSpan);
  
  int eat_line_break(morkEnv* ev, int inLast);
  int eat_line_continue(morkEnv* ev); // last char was '\\'
  int eat_comment(morkEnv* ev); // last char was '/'
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly morkParser methods
    
  mdb_count ParseMore( // return count of bytes consumed now
    morkEnv* ev,          // context
    mork_pos* outPos,     // current byte pos in the stream afterwards
    mork_bool* outDone,   // is parsing finished?
    mork_bool* outBroken  // is parsing irreparably dead and broken?
  );
  
  
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakParser(morkParser* me,
    morkEnv* ev, morkParser** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongParser(morkParser* me,
    morkEnv* ev, morkParser** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKPARSER_ */
