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

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkParser::CloseMorkNode(morkEnv* ev) /*i*/ // CloseParser() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseParser(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkParser::~morkParser() /*i*/ // assert CloseParser() executed earlier
{
  MORK_ASSERT(mParser_Heap==0);
  MORK_ASSERT(mParser_Stream==0);
}

/*public non-poly*/
morkParser::morkParser(morkEnv* ev, /*i*/
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  morkStream* ioStream, mdb_count inBytesPerParseSegment,
  nsIMdbHeap* ioSlotHeap)
: morkNode(ev, inUsage, ioHeap)
, mParser_Heap( 0 )
, mParser_Stream( 0 )
, mParser_MoreGranularity( inBytesPerParseSegment )
, mParser_State( morkParser_kStartState )

, mParser_GroupContentStartPos( 0 )

, mParser_TableId( 0 )
, mParser_RowId( 0 )
    
, mParser_InPort( morkBool_kFalse )
, mParser_InDict( morkBool_kFalse )
, mParser_InCell( morkBool_kFalse )
, mParser_InMeta( morkBool_kFalse )
    
, mParser_InPortRow( morkBool_kFalse )
, mParser_IsBroken( morkBool_kFalse )
, mParser_IsDone( morkBool_kFalse )
    
, mParser_Alias()

, mParser_ScopeSpool(ev, ioHeap)
, mParser_ValueSpool(ev, ioHeap)
, mParser_ColumnSpool(ev, ioHeap)
, mParser_StringSpool(ev, ioHeap)

, mParser_ScopeSink(ev, &mParser_ScopeSpool)
, mParser_ValueSink(ev, &mParser_ValueSpool)
, mParser_ColumnSink(ev, &mParser_ColumnSpool)
, mParser_StringSink(ev, &mParser_StringSpool)

, mParser_AliasYarn(ev, morkUsage_kMember, ioHeap)
{
  ev->StubMethodOnlyError();
  
  if ( inBytesPerParseSegment < morkParser_kMinGranularity )
    inBytesPerParseSegment = morkParser_kMinGranularity;
  else if ( inBytesPerParseSegment > morkParser_kMaxGranularity )
    inBytesPerParseSegment = morkParser_kMaxGranularity;
    
  mParser_MoreGranularity = inBytesPerParseSegment;
  
  if ( ev->Good() )
  {
    mParser_Tag = morkParser_kTag;
    mNode_Derived = morkDerived_kParser;
  }
}

/*public non-poly*/ void
morkParser::CloseParser(morkEnv* ev) /*i*/ // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      if ( !this->IsShutNode() )
      {
        mParser_ScopeSpool.CloseSpool(ev);
        mParser_ValueSpool.CloseSpool(ev);
        mParser_ColumnSpool.CloseSpool(ev);
        mParser_StringSpool.CloseSpool(ev);
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

/*public non-poly*/ void
morkParser::SetParserStream(morkEnv* ev, morkStream* ioStream)
{
  morkStream* stream = mParser_Stream;
  if ( stream )
  {
    mParser_Stream = 0;
    stream->CutStrongRef(ev);
  }
  if ( ioStream && ioStream->AddStrongRef(ev) )
    mParser_Stream = ioStream;
}

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
   if ( mParser_InPortRow )
   {
      mParser_InPortRow = morkBool_kFalse;
      mParser_RowSpan.SetEndWithEnd(mParser_PortSpan);
      this->OnPortRowEnd(ev, mParser_RowSpan);
   }
   if ( mParser_RowId )
   {
      mParser_RowId = 0;
      mParser_RowSpan.SetEndWithEnd(mParser_PortSpan);
      this->OnRowEnd(ev, mParser_RowSpan);
   }
   if ( mParser_TableId )
   {
      mParser_TableId = 0;
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

///*protected non-poly*/ int
//morkParser::NextChar(morkEnv* ev)
//{
//}

/*protected non-poly*/ int
morkParser::NextChar(morkEnv* ev) // next non-white content
{
  register int c; // the most heavily used character variable
  int outChar = -1; // the byte to return from this method
  int d; // the byte after c on some occasions
  morkStream* s = mParser_Stream;
  if ( s )
  {
    while ( outChar == -1 && ev->Good() )
    {
      while ( (c = s->Getc(ev)) != EOF && ev->Good() )
      {
        if ( c == 0xA || c == 0xD ) // end of line?
        {
          this->AddLine();
          d = s->Getc(ev); // look for another byte in #xA #xD, or  #xD #xA
          if (( d == 0xA || d == 0xD ) && c != d ) // eat this one too?
          {
          }
          else if ( d != EOF ) // not trying to push back end of file
            s->Ungetc(d);
        }
      }

      if ( c == '/' ) // maybe start of comment?
      {
        int depth = 0;
      }
      else if ( c == '\\' ) // maybe start of line continuation?
      {
        if ( (c = s->Getc(ev)) == 0xA || c == 0xD )
         ;
      }
      
      if ( c == EOF ) // reached end of file?
      {
        if ( outChar == -1 )
          outChar = 1; // end while loop
        mParser_DoMore = morkBool_kFalse;
        mParser_IsDone = morkBool_kTrue;
      }
    }

    this->SetHerePos(s->Tell(ev));
  }
  else // nil stream pointer
  {
    ev->NilPointerError();
    mParser_State = morkParser_kBrokenState;
    mParser_DoMore = morkBool_kFalse;
    mParser_IsDone = morkBool_kTrue;
    mParser_IsBroken = morkBool_kTrue;
  }
  return outChar;
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
        break;
      case morkParser_kMetaState: // 1
        break;
      case morkParser_kRowState: // 2
        break;
      case morkParser_kTableState: // 3
        break;
      case morkParser_kDictState: // 4
        break;
      case morkParser_kPortState: // 5
        break;
        
      case morkParser_kStartState: // 6
        {
          morkStream* s = mParser_Stream;
          if ( s && s->IsNode() && s->IsOpenNode() )
          {
            this->StartParse(ev);
            mParser_State = morkParser_kPortState;
          }
          else
          {
            mParser_State = morkParser_kBrokenState;
            ev->NilPointerError();
          }
        }
        break;
       
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
