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

#ifndef _MORKBUILDER_
#include "morkBuilder.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkBuilder::CloseMorkNode(morkEnv* ev) // CloseBuilder() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseBuilder(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkBuilder::~morkBuilder() // assert CloseBuilder() executed earlier
{
  MORK_ASSERT(mBuilder_Table==0);
}

/*public non-poly*/
morkBuilder::morkBuilder(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  morkStream* ioStream, mdb_count inBytesPerParseSegment,
  nsIMdbHeap* ioSlotHeap, morkStore* ioStore)
: morkParser(ev, inUsage, ioHeap, ioStream,
  inBytesPerParseSegment, ioSlotHeap)
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kBuilder;
}

/*public non-poly*/ void
morkBuilder::CloseBuilder(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      this->CloseParser(ev);
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

/*static*/ void
morkBuilder::NonBuilderTypeError(morkEnv* ev)
{
  ev->NewError("non morkBuilder");
}

/*virtual*/ void
morkBuilder::AliasToYarn(morkEnv* ev,
  const morkAlias& inAlias,  // typically an alias to concat with strings
  mdbYarn* outYarn)
// The parser might ask that some aliases be turned into yarns, so they
// can be concatenated into longer blobs under some circumstances.  This
// is an alternative to using a long and complex callback for many parts
// for a single cell value.
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewPort(morkEnv* ev, const morkPlace& inPlace)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnPortGlitch(morkEnv* ev, const morkGlitch& inGlitch)  
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnPortEnd(morkEnv* ev, const morkSpan& inSpan)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewGroup(morkEnv* ev, const morkPlace& inPlace, mork_gid inGid)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnGroupGlitch(morkEnv* ev, const morkGlitch& inGlitch) 
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnGroupCommitEnd(morkEnv* ev, const morkSpan& inSpan)  
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnGroupAbortEnd(morkEnv* ev, const morkSpan& inSpan) 
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewPortRow(morkEnv* ev, const morkPlace& inPlace, 
  const morkAlias& inAlias, mork_change inChange)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnPortRowGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnPortRowEnd(morkEnv* ev, const morkSpan& inSpan)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewTable(morkEnv* ev, const morkPlace& inPlace,
  const morkAlias& inAlias, mork_change inChange)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnTableGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnTableEnd(morkEnv* ev, const morkSpan& inSpan)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewMeta(morkEnv* ev, const morkPlace& inPlace)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnMetaGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnMetaEnd(morkEnv* ev, const morkSpan& inSpan)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewRow(morkEnv* ev, const morkPlace& inPlace, 
  const morkAlias& inAlias, mork_change inChange)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnRowGlitch(morkEnv* ev, const morkGlitch& inGlitch) 
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnRowEnd(morkEnv* ev, const morkSpan& inSpan) 
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewDict(morkEnv* ev, const morkPlace& inPlace)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnDictGlitch(morkEnv* ev, const morkGlitch& inGlitch) 
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnDictEnd(morkEnv* ev, const morkSpan& inSpan)  
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnAlias(morkEnv* ev, const morkSpan& inSpan,
  const morkAlias& inAlias)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnAliasGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewCell(morkEnv* ev, const morkPlace& inPlace,
  const morkAlias& inAlias, mork_change inChange)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnCellGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnCellForm(morkEnv* ev, mork_cscode inCharsetFormat)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnCellEnd(morkEnv* ev, const morkSpan& inSpan)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnValue(morkEnv* ev, const morkSpan& inSpan,
  const morkBuf& inBuf)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnValueAlias(morkEnv* ev, const morkSpan& inSpan,
  const morkAlias& inAlias)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnRowAlias(morkEnv* ev, const morkSpan& inSpan,
  const morkAlias& inAlias)
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnTableAlias(morkEnv* ev, const morkSpan& inSpan,
  const morkAlias& inAlias)
{
  ev->StubMethodOnlyError();
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
