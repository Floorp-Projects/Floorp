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

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKPARSER_
#include "morkParser.h"
#endif

#ifndef _MORKSTREAM_
#include "morkStream.h"
#endif

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

#ifndef _MORKSINK_
#include "morkSink.h"
#endif

#ifndef _MORKCH_
#include "morkCh.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkParser::CloseMorkNode(morkEnv* ev) // CloseParser() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseParser(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkParser::~morkParser() // assert CloseParser() executed earlier
{
  MORK_ASSERT(mParser_Heap==0);
  MORK_ASSERT(mParser_Stream==0);
}

/*public non-poly*/
morkParser::morkParser(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  morkStream* ioStream, mdb_count inBytesPerParseSegment,
  nsIMdbHeap* ioSlotHeap)
: morkNode(ev, inUsage, ioHeap)
, mParser_Heap( 0 )
, mParser_Stream( 0 )
, mParser_MoreGranularity( inBytesPerParseSegment )
, mParser_State( morkParser_kStartState )

, mParser_GroupContentStartPos( 0 )

, mParser_TableMid(  )
, mParser_RowMid(  )
, mParser_CellMid(  )
    
, mParser_InPort( morkBool_kFalse )
, mParser_InDict( morkBool_kFalse )
, mParser_InCell( morkBool_kFalse )
, mParser_InMeta( morkBool_kFalse )
    
, mParser_InPortRow( morkBool_kFalse )
, mParser_InRow( morkBool_kFalse )
, mParser_InTable( morkBool_kFalse )
, mParser_InGroup( morkBool_kFalse )

, mParser_AtomChange( morkChange_kNil )
, mParser_CellChange( morkChange_kNil )
, mParser_RowChange( morkChange_kNil )
, mParser_TableChange( morkChange_kNil )

, mParser_Change( morkChange_kNil )
, mParser_IsBroken( morkBool_kFalse )
, mParser_IsDone( morkBool_kFalse )
, mParser_DoMore( morkBool_kTrue )
    
, mParser_Mid()

, mParser_ScopeCoil(ev, ioSlotHeap)
, mParser_ValueCoil(ev, ioSlotHeap)
, mParser_ColumnCoil(ev, ioSlotHeap)
, mParser_StringCoil(ev, ioSlotHeap)

, mParser_ScopeSpool(ev, &mParser_ScopeCoil)
, mParser_ValueSpool(ev, &mParser_ValueCoil)
, mParser_ColumnSpool(ev, &mParser_ColumnCoil)
, mParser_StringSpool(ev, &mParser_StringCoil)

, mParser_MidYarn(ev, morkUsage_kMember, ioSlotHeap)
{
  if ( inBytesPerParseSegment < morkParser_kMinGranularity )
    inBytesPerParseSegment = morkParser_kMinGranularity;
  else if ( inBytesPerParseSegment > morkParser_kMaxGranularity )
    inBytesPerParseSegment = morkParser_kMaxGranularity;
    
  mParser_MoreGranularity = inBytesPerParseSegment;

  if ( ioSlotHeap && ioStream )
  {
    nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mParser_Heap);
    morkStream::SlotStrongStream(ioStream, ev, &mParser_Stream);
    
    if ( ev->Good() )
    {
      mParser_Tag = morkParser_kTag;
      mNode_Derived = morkDerived_kParser;
    }
  }
  else
    ev->NilPointerError();
}

/*public non-poly*/ void
morkParser::CloseParser(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      if ( !this->IsShutNode() )
      {
        mParser_ScopeCoil.CloseCoil(ev);
        mParser_ValueCoil.CloseCoil(ev);
        mParser_ColumnCoil.CloseCoil(ev);
        mParser_StringCoil.CloseCoil(ev);
        nsIMdbHeap_SlotStrongHeap((nsIMdbHeap*) 0, ev, &mParser_Heap);
        morkStream::SlotStrongStream((morkStream*) 0, ev, &mParser_Stream);
        this->MarkShut();
      }
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

/*protected non-poly*/ void
morkParser::NonGoodParserError(morkEnv* ev) // when GoodParserTag() is false
{
  ev->NewError("non-morkNode");
}

/*protected non-poly*/ void
morkParser::NonUsableParserError(morkEnv* ev) //
{
  if ( this->IsNode() )
  {
    if ( this->IsOpenNode() )
    {
      if (  this->GoodParserTag() )
      {
         // okay
      }
      else
        this->NonGoodParserError(ev);
    }
    else
      this->NonOpenNodeError(ev);
  }
  else
    this->NonNodeError(ev);
}


/*protected non-poly*/ void
morkParser::StartParse(morkEnv* ev)
{
  mParser_InCell = morkBool_kFalse;
  mParser_InMeta = morkBool_kFalse;
  mParser_InDict = morkBool_kFalse;
  mParser_InPortRow = morkBool_kFalse;
  
  mParser_RowMid.ClearMid();
  mParser_TableMid.ClearMid();
  mParser_CellMid.ClearMid();
  
  mParser_GroupId = 0;
  mParser_InPort = morkBool_kTrue;

  mParser_GroupSpan.ClearSpan();
  mParser_DictSpan.ClearSpan();
  mParser_AliasSpan.ClearSpan();
  mParser_MetaSpan.ClearSpan();
  mParser_TableSpan.ClearSpan();
  mParser_RowSpan.ClearSpan();
  mParser_CellSpan.ClearSpan();
  mParser_ColumnSpan.ClearSpan();
  mParser_SlotSpan.ClearSpan();

   mParser_PortSpan.ClearSpan();
}

/*protected non-poly*/ void
morkParser::StopParse(morkEnv* ev)
{
  if ( mParser_InCell )
  {
    mParser_InCell = morkBool_kFalse;
    mParser_CellSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnCellEnd(ev, mParser_CellSpan);
  }
  if ( mParser_InMeta )
  {
    mParser_InMeta = morkBool_kFalse;
    mParser_MetaSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnMetaEnd(ev, mParser_MetaSpan);
  }
  if ( mParser_InDict )
  {
    mParser_InDict = morkBool_kFalse;
    mParser_DictSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnDictEnd(ev, mParser_DictSpan);
  }
  if ( mParser_InPortRow )
  {
    mParser_InPortRow = morkBool_kFalse;
    mParser_RowSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnPortRowEnd(ev, mParser_RowSpan);
  }
  if ( mParser_InRow )
  {
    mParser_InRow = morkBool_kFalse;
    mParser_RowMid.ClearMid();
    mParser_RowSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnRowEnd(ev, mParser_RowSpan);
  }
  if ( mParser_InTable )
  {
    mParser_InTable = morkBool_kFalse;
    mParser_TableMid.ClearMid();
    mParser_TableSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnTableEnd(ev, mParser_TableSpan);
  }
  if ( mParser_GroupId )
  {
    mParser_GroupId = 0;
    mParser_GroupSpan.SetEndWithEnd(mParser_PortSpan);
    this->OnGroupAbortEnd(ev, mParser_GroupSpan);
  }
  if ( mParser_InPort )
  {
    mParser_InPort = morkBool_kFalse;
    this->OnPortEnd(ev, mParser_PortSpan);
  }
}

int morkParser::eat_comment(morkEnv* ev) // last char was '/'
{
  morkStream* s = mParser_Stream;
  // Note morkStream::Getc() returns EOF when an error occurs, so
  // we don't need to check for both c != EOF and ev->Good() below.
  
  register int c = s->Getc(ev);
  if ( c == '/' ) // C++ style comment?
  {
    while ( (c = s->Getc(ev)) != EOF && c != 0xA && c != 0xD )
      /* empty */;
      
    if ( c == 0xA || c == 0xD )
      c = this->eat_line_break(ev, c);
  }
  else if ( c == '*' ) /* C style comment? */
  {
    int depth = 1; // count depth of comments until depth reaches zero
    
    while ( depth > 0 && c != EOF ) // still looking for comment end(s)?
    {
      while ( (c = s->Getc(ev)) != EOF && c != '/' && c != '*' )
      {
        if ( c == 0xA || c == 0xD ) // need to count a line break?
        {
          c = this->eat_line_break(ev, c);
          if ( c == '/' || c == '*' )
            break; // end while loop
        }
      }
        
      if ( c == '*' ) // maybe end of a comment, if next char is '/'?
      {
        if ( (c = s->Getc(ev)) == '/' ) // end of comment?
          --depth; // depth of comments has decreased by one
        else if ( c != EOF ) // need to put the char back?
          s->Ungetc(c); // especially need to put back '*', 0xA, or 0xD
      }
      else if ( c == '/' ) // maybe nested comemnt, if next char is '*'?
      {
        if ( (c = s->Getc(ev)) == '*' ) // nested comment?
          ++depth; // depth of comments has increased by one
        else if ( c != EOF ) // need to put the char back?
          s->Ungetc(c); // especially need to put back '/', 0xA, or 0xD
      }
        
      if ( ev->Bad() )
        c = EOF;
    }
    if ( c == EOF && depth > 0 )
      ev->NewWarning("EOF before end of comment");
  }
  else
    ev->NewWarning("expected / or *");
  
  return c;
}

int morkParser::eat_line_break(morkEnv* ev, int inLast)
{
  morkStream* s = mParser_Stream;
  register int c = s->Getc(ev); // get next char after 0xA or 0xD
  this->CountLineBreak();
  if ( c == 0xA || c == 0xD ) // another line break character?
  {
    if ( c != inLast ) // not the same as the last one?
      c = s->Getc(ev); // get next char after two-byte linebreak
  }
  return c;
}

int morkParser::eat_line_continue(morkEnv* ev) // last char was '\'
{
  morkStream* s = mParser_Stream;
  register int c = s->Getc(ev);
  if ( c == 0xA || c == 0xD ) // linebreak follows \ as expected?
  {
    c = this->eat_line_break(ev, c);
  }
  else
    ev->NewWarning("expected linebreak");
  
  return c;
}

int morkParser::NextChar(morkEnv* ev) // next non-white content
{
  morkStream* s = mParser_Stream;
  register int c = s->Getc(ev);
  while ( c > 0 && ev->Good() )
  {
    if ( c == '/' )
      c = this->eat_comment(ev);
    else if ( c == 0xA || c == 0xD )
      c = this->eat_line_break(ev, c);
    else if ( c == '\\' )
      c = this->eat_line_continue(ev);
    else if ( morkCh_IsWhite(c) )
      c = s->Getc(ev);
    else  
      break; // end while loop when return c is acceptable
  }
  if ( ev->Bad() )
  {
    mParser_State = morkParser_kBrokenState;
    mParser_DoMore = morkBool_kFalse;
    mParser_IsDone = morkBool_kTrue;
    mParser_IsBroken = morkBool_kTrue;
    c = EOF;
  }
  else if ( c == EOF )
  {
    mParser_DoMore = morkBool_kFalse;
    mParser_IsDone = morkBool_kTrue;
  }
  return c;
}

void
morkParser::OnCellState(morkEnv* ev)
{
  ev->StubMethodOnlyError();
}

void
morkParser::OnMetaState(morkEnv* ev)
{
  ev->StubMethodOnlyError();
}

void
morkParser::OnRowState(morkEnv* ev)
{
  ev->StubMethodOnlyError();
}

void
morkParser::OnTableState(morkEnv* ev)
{
  ev->StubMethodOnlyError();
}

void
morkParser::OnDictState(morkEnv* ev)
{
  ev->StubMethodOnlyError();
}

morkBuf* morkParser::ReadName(morkEnv* ev, register int c)
{
  morkBuf* outBuf = 0;
  
  if ( !morkCh_IsName(c) )
    ev->NewError("not a name char");

  morkCoil* coil = &mParser_ColumnCoil;
  coil->ClearBufFill();

  morkSpool* spool = &mParser_ColumnSpool;
  spool->Seek(ev, /*pos*/ 0);
  
  if ( ev->Good() )
  {
    spool->Putc(ev, c);
    
    morkStream* s = mParser_Stream;
    while ( (c = s->Getc(ev)) != EOF && morkCh_IsMore(c) && ev->Good() )
      spool->Putc(ev, c);
      
    if ( ev->Good() )
    {
      if ( c != EOF )
      {
        s->Ungetc(c);
        spool->FlushSink(ev); // update coil->mBuf_Fill
      }
      else
        this->UnexpectedEofError(ev);
        
      if ( ev->Good() )
        outBuf = coil;
    }
  }  
  return outBuf;
}

mork_bool
morkParser::ReadMid(morkEnv* ev, morkMid* outMid)
{
  outMid->ClearMid();
  
  morkStream* s = mParser_Stream;
  int next;
  outMid->mMid_Oid.mOid_Id = this->ReadHex(ev, &next);
  register int c = next;
  if ( c == ':' )
  {
    if ( (c = s->Getc(ev)) != EOF && ev->Good() )
    {
      if ( c == '^' )
      {
        outMid->mMid_Oid.mOid_Scope = this->ReadHex(ev, &next);
        if ( ev->Good() )
          s->Ungetc(next);
      }
      else if ( morkCh_IsName(c) )
      {
        outMid->mMid_Buf = this->ReadName(ev, c); 
      }
      else
        ev->NewError("expected name or hex after ':' following ID");
    }
    
    if ( c == EOF && ev->Good() )
      this->UnexpectedEofError(ev);
  }
  else
    s->Ungetc(c);
  
  return ev->Good();
}

void
morkParser::ReadCell(morkEnv* ev)
{
  mParser_CellMid.ClearMid();
  this->StartSpanOnLastByte(ev, &mParser_CellSpan);
  morkMid* cellMid = 0; // if mid syntax is used for column
  morkBuf* cellBuf = 0; // if naked string is used for column

  morkStream* s = mParser_Stream;
  register int c;
  if ( (c = s->Getc(ev)) != EOF && ev->Good() )
  {
    this->StartSpanOnLastByte(ev, &mParser_ColumnSpan);
    if ( c == '^' )
    {
      cellMid = &mParser_CellMid;
      this->ReadMid(ev, cellMid);
      // if ( !mParser_CellMid.mMid_Oid.mOid_Scope )
      //  mParser_CellMid.mMid_Oid.mOid_Scope = (mork_scope) 'c';
    }
    else
    {
      cellBuf = this->ReadName(ev, c); 
    }
    if ( ev->Good() )
    {
      this->EndSpanOnThisByte(ev, &mParser_ColumnSpan);

      mParser_InCell = morkBool_kTrue;
      this->OnNewCell(ev, *mParser_CellSpan.AsPlace(),
        cellMid, cellBuf, mParser_CellChange);

      if ( (c = this->NextChar(ev)) != EOF && ev->Good() )
      {
        this->StartSpanOnLastByte(ev, &mParser_SlotSpan);
        if ( c == '=' )
        {
          morkBuf* buf = this->ReadValue(ev);
          if ( buf )
          {
            this->EndSpanOnThisByte(ev, &mParser_SlotSpan);
            this->OnValue(ev, mParser_SlotSpan, *buf);
          }
        }
        else if ( c == '^' )
        {
          if ( this->ReadMid(ev, &mParser_Mid) )
          {
            this->EndSpanOnThisByte(ev, &mParser_SlotSpan);
            if ( (c = this->NextChar(ev)) != EOF && ev->Good() )
            {
              if ( c != ')' )
                ev->NewError("expected ')' after cell ^ID value");
            }
            else if ( c == EOF )
              this->UnexpectedEofError(ev);
            
            if ( ev->Good() )
              this->OnValueMid(ev, mParser_SlotSpan, mParser_Mid);
          }
        }
        else if ( c == 'r' || c == 't' || c == '"' || c == '\'' )
        {
          ev->NewError("cell syntax not yet supported");
        }
        else
        {
          ev->NewError("unknown cell syntax");
        }
      }
      
      this->EndSpanOnThisByte(ev, &mParser_CellSpan);
      mParser_InCell = morkBool_kFalse;
      this->OnCellEnd(ev, mParser_CellSpan);
    }
  }
  
  if ( c == EOF && ev->Good() )
    this->UnexpectedEofError(ev);
}

void morkParser::ReadRow(morkEnv* ev, int c)
// zm:Row       ::= zm:S? '[' zm:S? zm:Id zm:RowItem* zm:S? ']'
// zm:RowItem   ::= zm:MetaRow | zm:Cell
// zm:MetaRow   ::= zm:S? '[' zm:S? zm:Cell* zm:S? ']' /* meta attributes */
// zm:Cell      ::= zm:S? '(' zm:Column zm:S? zm:Slot? ')'
{
  if ( ev->Good() )
  {
    this->StartSpanOnLastByte(ev, &mParser_RowSpan);
    if ( mParser_Change )
      mParser_RowChange = mParser_Change;

    if ( c == '[' )
    {
      if ( this->ReadMid(ev, &mParser_RowMid) )
      {
        mParser_InRow = morkBool_kTrue;
        this->OnNewRow(ev, *mParser_RowSpan.AsPlace(),
          mParser_RowMid, mParser_RowChange);

        mParser_Change = mParser_RowChange = morkChange_kNil;

        while ( (c = this->NextChar(ev)) != EOF && ev->Good() && c != ']' )
        {
          switch ( c )
          {
            case '(': // cell
              this->ReadCell(ev);
              break;
              
            case '[': // meta
              this->ReadMeta(ev, ']');
              break;
            
            case '+': // plus
              mParser_CellChange = morkChange_kAdd;
              break;
              
            case '-': // minus
              mParser_CellChange = morkChange_kCut;
              break;
              
            case '!': // bang
              mParser_CellChange = morkChange_kSet;
              break;
              
            default:
              ev->NewWarning("unexpected byte in row");
              break;
          } // switch
        } // while
        
        this->EndSpanOnThisByte(ev, &mParser_RowSpan);
        mParser_InRow = morkBool_kFalse;
        this->OnRowEnd(ev, mParser_RowSpan);

      } // if ReadMid
    } // if '['
    
    else // c != '['
    {
      morkStream* s = mParser_Stream;
      s->Ungetc(c);
      if ( this->ReadMid(ev, &mParser_RowMid) )
      {
        mParser_InRow = morkBool_kTrue;
        this->OnNewRow(ev, *mParser_RowSpan.AsPlace(),
          mParser_RowMid, mParser_RowChange);

        mParser_Change = mParser_RowChange = morkChange_kNil;

        this->EndSpanOnThisByte(ev, &mParser_RowSpan);
        mParser_InRow = morkBool_kFalse;
        this->OnRowEnd(ev, mParser_RowSpan);
      }
    }
  }
  
  if ( ev->Bad() )
    mParser_State = morkParser_kBrokenState;
  else if ( c == EOF )
    mParser_State = morkParser_kDoneState;
}

void morkParser::ReadTable(morkEnv* ev)
// zm:Table     ::= zm:S? '{' zm:S? zm:Id zm:TableItem* zm:S? '}'
// zm:TableItem ::= zm:MetaTable | zm:RowRef | zm:Row
// zm:MetaTable ::= zm:S? '{' zm:S? zm:Cell* zm:S? '}' /* meta attributes */
{
  this->StartSpanOnLastByte(ev, &mParser_TableSpan);

  if ( mParser_Change )
    mParser_TableChange = mParser_Change;
  
  if ( ev->Good() && this->ReadMid(ev, &mParser_TableMid) )
  {
    mParser_InTable = morkBool_kTrue;
    this->OnNewTable(ev, *mParser_TableSpan.AsPlace(),  
      mParser_TableMid, mParser_TableChange);

    mParser_Change = mParser_TableChange = morkChange_kNil;

    int c;
    while ( (c = this->NextChar(ev)) != EOF && ev->Good() && c != '}' )
    {
      if ( morkCh_IsHex(c) )
      {
        this->ReadRow(ev, c);
      }
      else
      {
        switch ( c )
        {
          case '[': // row
            this->ReadRow(ev, '[');
            break;
            
          case '{': // meta
            this->ReadMeta(ev, '}');
            break;
          
          case '+': // plus
            mParser_RowChange = morkChange_kAdd;
            break;
            
          case '-': // minus
            mParser_RowChange = morkChange_kCut;
            break;
            
          case '!': // bang
            mParser_RowChange = morkChange_kSet;
            break;
            
          default:
            ev->NewWarning("unexpected byte in table");
            break;
        }
      }
    }

    this->EndSpanOnThisByte(ev, &mParser_TableSpan);
    mParser_InTable = morkBool_kFalse;
    this->OnTableEnd(ev, mParser_TableSpan);

    if ( ev->Bad() )
      mParser_State = morkParser_kBrokenState;
    else if ( c == EOF )
      mParser_State = morkParser_kDoneState;
  }
}

mork_id morkParser::ReadHex(morkEnv* ev, int* outNextChar)
// zm:Hex   ::= [0-9a-fA-F] /* a single hex digit */
// zm:Hex+  ::= zm:Hex | zm:Hex zm:Hex+
{
  mork_id hex = 0;

  morkStream* s = mParser_Stream;
  register int c = this->NextChar(ev);
    
  if ( ev->Good() )
  {
    if ( c != EOF )
    {
      if ( morkCh_IsHex(c) )
      {
        do
        {
          if ( morkCh_IsDigit(c) ) // '0' through '9'?
            c -= '0';
          else if ( morkCh_IsUpper(c) ) // 'A' through 'F'?
            c -= ('A' - 10) ; // c = (c - 'A') + 10;
          else // 'a' through 'f'?
            c -= ('a' - 10) ; // c = (c - 'a') + 10;

          hex = (hex << 4) + c;
        }
        while ( (c = s->Getc(ev)) != EOF && ev->Good() && morkCh_IsHex(c) );
      }
      else
        this->ExpectedHexDigitError(ev, c);
    }
  }
  if ( c == EOF )
    this->EofInsteadOfHexError(ev);
    
  *outNextChar = c;
  return hex;
}

/*static*/ void
morkParser::EofInsteadOfHexError(morkEnv* ev)
{
  ev->NewWarning("eof instead of hex");
}

/*static*/ void
morkParser::ExpectedHexDigitError(morkEnv* ev, int c)
{
  ev->NewWarning("expected hex digit");
}

/*static*/ void
morkParser::ExpectedEqualError(morkEnv* ev)
{
  ev->NewWarning("expected '='");
}

/*static*/ void
morkParser::UnexpectedEofError(morkEnv* ev)
{
  ev->NewWarning("unexpected eof");
}

morkBuf* morkParser::ReadValue(morkEnv* ev)
{
  morkBuf* outBuf = 0;

  morkCoil* coil = &mParser_ValueCoil;
  coil->ClearBufFill();

  morkSpool* spool = &mParser_ValueSpool;
  spool->Seek(ev, /*pos*/ 0);
  
  if ( ev->Good() )
  {
    morkStream* s = mParser_Stream;
    register int c;
    while ( (c = s->Getc(ev)) != EOF && c != ')' && ev->Good() )
    {
      if ( c == '\\' ) // "\" escapes the next char? 
      {
        if ( (c = s->Getc(ev)) == 0xA || c == 0xD ) // linebreak after \?
          c = this->eat_line_break(ev, c);

        if ( c == EOF || ev->Bad() )
          break; // end while loop
      }
      else if ( c == '$' ) // "$" escapes next two hex digits?
      {
        if ( (c = s->Getc(ev)) != EOF && ev->Good() )
        {
          mork_ch first = (mork_ch) c; // first hex digit
          if ( (c = s->Getc(ev)) != EOF && ev->Good() )
          {
            mork_ch second = (mork_ch) c; // second hex digit
            c = ev->HexToByte(first, second);
          }
          else
            break;
        }
        else
          break;
      }
      spool->Putc(ev, c);
    }
      
    if ( ev->Good() )
    {
      if ( c != EOF )
        spool->FlushSink(ev); // update coil->mBuf_Fill
      else
        this->UnexpectedEofError(ev);
        
      if ( ev->Good() )
        outBuf = coil;
    }
  }
  return outBuf; 
}

void morkParser::ReadAlias(morkEnv* ev)
// zm:Alias     ::= zm:S? '(' ('#')? zm:Hex+ zm:S? zm:Value ')'
// zm:Value   ::= '=' ([^)$\] | '\' zm:NonCRLF | zm:Continue | zm:Dollar)*
{
  this->StartSpanOnLastByte(ev, &mParser_AliasSpan);

  int nextChar;
  mork_id hex = this->ReadHex(ev, &nextChar);
  register int c = nextChar;

  mParser_Mid.ClearMid();
  mParser_Mid.mMid_Oid.mOid_Id = hex;

  if ( morkCh_IsWhite(c) && ev->Good() )
    c = this->NextChar(ev);

  if ( ev->Good() )
  {
    if ( c == '=' )
    {
      mParser_Mid.mMid_Buf = this->ReadValue(ev);
      if ( mParser_Mid.mMid_Buf )
      {
        this->EndSpanOnThisByte(ev, &mParser_AliasSpan);
        this->OnAlias(ev, mParser_AliasSpan, mParser_Mid);
      }
    }
    else
      this->ExpectedEqualError(ev);
  }
}

void morkParser::ReadMeta(morkEnv* ev, int inEndMeta)
// zm:MetaDict  ::= zm:S? '<' zm:S? zm:Cell* zm:S? '>' /* meta attributes */
// zm:MetaTable ::= zm:S? '{' zm:S? zm:Cell* zm:S? '}' /* meta attributes */
// zm:MetaRow   ::= zm:S? '[' zm:S? zm:Cell* zm:S? ']' /* meta attributes */
{
  this->StartSpanOnLastByte(ev, &mParser_MetaSpan);
  mParser_InMeta = morkBool_kTrue;
  this->OnNewMeta(ev, *mParser_MetaSpan.AsPlace());

  mork_bool more = morkBool_kTrue; // until end meta
  int c;
  while ( more && (c = this->NextChar(ev)) != EOF && ev->Good() )
  {
    switch ( c )
    {
      case '(': // cell
        this->ReadCell(ev);
        break;
        
      case '>': // maybe end meta?
        if ( inEndMeta == '>' )
          more = morkBool_kFalse; // stop reading meta
        else
          this->UnexpectedByteInMetaWarning(ev);
        break;
        
      case '}': // maybe end meta?
        if ( inEndMeta == '}' )
          more = morkBool_kFalse; // stop reading meta
        else
          this->UnexpectedByteInMetaWarning(ev);
        break;
        
      case ']': // maybe end meta?
        if ( inEndMeta == ']' )
          more = morkBool_kFalse; // stop reading meta
        else
          this->UnexpectedByteInMetaWarning(ev);
        break;
        
      case '[': // maybe table meta row?
        if ( mParser_InTable )
          this->ReadRow(ev, '['); 
        else
          this->UnexpectedByteInMetaWarning(ev);
        break;
        
      default:
        if ( mParser_InTable && morkCh_IsHex(c) )
          this->ReadRow(ev, c);
        else
          this->UnexpectedByteInMetaWarning(ev);
        break;
    }
  }

  this->EndSpanOnThisByte(ev, &mParser_MetaSpan);
  mParser_InMeta = morkBool_kFalse;
  this->OnMetaEnd(ev, mParser_MetaSpan);
}

/*static*/ void
morkParser::UnexpectedByteInMetaWarning(morkEnv* ev)
{
  ev->NewWarning("unexpected byte in meta");
}

/*static*/ void
morkParser::NonParserTypeError(morkEnv* ev)
{
  ev->NewError("non morkParser");
}

void morkParser::ReadDict(morkEnv* ev)
// zm:Dict      ::= zm:S? '<' zm:DictItem* zm:S? '>'
// zm:DictItem  ::= zm:MetaDict | zm:Alias
// zm:MetaDict  ::= zm:S? '<' zm:S? zm:Cell* zm:S? '>' /* meta attributes */
// zm:Alias     ::= zm:S? '(' ('#')? zm:Hex+ zm:S? zm:Value ')'
{
  mParser_Change = morkChange_kNil;
  mParser_AtomChange = morkChange_kNil;
  
  this->StartSpanOnLastByte(ev, &mParser_DictSpan);
  mParser_InDict = morkBool_kTrue;
  this->OnNewDict(ev, *mParser_DictSpan.AsPlace());
  
  int c;
  while ( (c = this->NextChar(ev)) != EOF && ev->Good() && c != '>' )
  {
    switch ( c )
    {
      case '(': // alias
        this->ReadAlias(ev);
        break;
        
      case '<': // meta
        this->ReadMeta(ev, '>');
        break;
        
      default:
        ev->NewWarning("unexpected byte in dict");
        break;
    }
  }

  this->EndSpanOnThisByte(ev, &mParser_DictSpan);
  mParser_InDict = morkBool_kFalse;
  this->OnDictEnd(ev, mParser_DictSpan);
  
  if ( ev->Bad() )
    mParser_State = morkParser_kBrokenState;
  else if ( c == EOF )
    mParser_State = morkParser_kDoneState;
}

void morkParser::EndSpanOnThisByte(morkEnv* ev, morkSpan* ioSpan)
{
  mork_pos here = mParser_Stream->Tell(ev);
  if ( ev->Good() )
  {
    this->SetHerePos(here);
    ioSpan->SetEndWithEnd(mParser_PortSpan);
  }
}

void morkParser::StartSpanOnLastByte(morkEnv* ev, morkSpan* ioSpan)
{
  mork_pos here = mParser_Stream->Tell(ev);
  if ( here > 0 ) // positive?
    --here;
  else
    here = 0;
    
  if ( ev->Good() )
  {
    this->SetHerePos(here);
    ioSpan->SetStartWithEnd(mParser_PortSpan);
    ioSpan->SetEndWithEnd(mParser_PortSpan);
  }
}

void
morkParser::OnPortState(morkEnv* ev)
{
  mParser_InPort = morkBool_kTrue;
  this->OnNewPort(ev, *mParser_PortSpan.AsPlace());

  int c;
  while ( (c = this->NextChar(ev)) != EOF && ev->Good() )
  {
    switch ( c )
    {
      case '[': // row
        this->ReadRow(ev, '[');
        break;
        
      case '{': // table
        this->ReadTable(ev);
        break;
        
      case '<': // dict
        this->ReadDict(ev);
        break;
        
      case '+': // plus
        mParser_Change = morkChange_kAdd;
        break;
        
      case '-': // minus
        mParser_Change = morkChange_kCut;
        break;
        
      case '!': // bang
        mParser_Change = morkChange_kSet;
        break;
        
      default:
        ev->NewWarning("unexpected byte in OnPortState()");
        break;
    }
  }

  mParser_InPort = morkBool_kFalse;
  this->OnPortEnd(ev, mParser_PortSpan);
  
  if ( ev->Bad() )
    mParser_State = morkParser_kBrokenState;
  else if ( c == EOF )
    mParser_State = morkParser_kDoneState;
}

void
morkParser::OnStartState(morkEnv* ev)
{
  morkStream* s = mParser_Stream;
  if ( s && s->IsNode() && s->IsOpenNode() )
  {
    s->Seek(ev, 0);
    if ( ev->Good() )
    {
      this->StartParse(ev);
      mParser_State = morkParser_kPortState;
    }
  }
  else
    ev->NilPointerError();

  if ( ev->Bad() )
    mParser_State = morkParser_kBrokenState;
}

/*protected non-poly*/ void
morkParser::ParseLoop(morkEnv* ev)
{
  mParser_Change = morkChange_kNil;
  mParser_DoMore = morkBool_kTrue;
            
  while ( mParser_DoMore )
  {
    switch ( mParser_State )
    {
      case morkParser_kCellState: // 0
        this->OnCellState(ev); break;
        
      case morkParser_kMetaState: // 1
        this->OnMetaState(ev); break;
        
      case morkParser_kRowState: // 2
        this->OnRowState(ev); break;
        
      case morkParser_kTableState: // 3
        this->OnTableState(ev); break;
        
      case morkParser_kDictState: // 4
        this->OnDictState(ev); break;
        
      case morkParser_kPortState: // 5
        this->OnPortState(ev); break;
        
      case morkParser_kStartState: // 6
        this->OnStartState(ev); break;
       
      case morkParser_kDoneState: // 7
        mParser_DoMore = morkBool_kFalse;
        mParser_IsDone = morkBool_kTrue;
        this->StopParse(ev);
        break;
      case morkParser_kBrokenState: // 8
        mParser_DoMore = morkBool_kFalse;
        mParser_IsBroken = morkBool_kTrue;
        this->StopParse(ev);
        break;
      default: // ?
        MORK_ASSERT(morkBool_kFalse);
        mParser_State = morkParser_kBrokenState;
        break;
    }
  }
}
    
/*public non-poly*/ mdb_count
morkParser::ParseMore( // return count of bytes consumed now
    morkEnv* ev,          // context
    mork_pos* outPos,     // current byte pos in the stream afterwards
    mork_bool* outDone,   // is parsing finished?
    mork_bool* outBroken  // is parsing irreparably dead and broken?
  )
{
  mdb_count outCount = 0;
  if ( this->IsNode() && this->GoodParserTag() && this->IsOpenNode() )
  {
    mork_pos startPos = this->HerePos();

    if ( !mParser_IsDone && !mParser_IsBroken )
      this->ParseLoop(ev);
  
    mork_pos endPos = this->HerePos();
    if ( outDone )
      *outDone = mParser_IsDone;
    if ( outBroken )
      *outBroken = mParser_IsBroken;
    if ( outPos )
      *outPos = endPos;
      
    if ( endPos > startPos )
      outCount = endPos - startPos;
  }
  else
  {
    this->NonUsableParserError(ev);
    if ( outDone )
      *outDone = morkBool_kTrue;
    if ( outBroken )
      *outBroken = morkBool_kTrue;
    if ( outPos )
      *outPos = 0;
  }
  return outCount;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
