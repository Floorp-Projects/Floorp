/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkCursor::CloseMorkNode(morkEnv* ev) // CloseCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkCursor::~morkCursor() // assert CloseCursor() executed earlier
{
}

/*public non-poly*/
morkCursor::morkCursor(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap)
: morkObject(ev, inUsage, ioHeap, morkColor_kNone, (morkHandle*) 0)
, mCursor_Seed( 0 )
, mCursor_Pos( -1 )
, mCursor_DoFailOnSeedOutOfSync( morkBool_kFalse )
{
  if ( ev->Good() )
    mNode_Derived = morkDerived_kCursor;
}

NS_IMPL_ISUPPORTS_INHERITED1(morkCursor, morkObject, nsIMdbCursor)

/*public non-poly*/ void
morkCursor::CloseCursor(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mCursor_Seed = 0;
      mCursor_Pos = -1;
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// { ----- begin ref counting for well-behaved cyclic graphs -----
NS_IMETHODIMP
morkCursor::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  *outCount = WeakRefsOnly();
  return NS_OK;
}  
NS_IMETHODIMP
morkCursor::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  *outCount = StrongRefsOnly();
  return NS_OK;
}
// ### TODO - clean up this cast, if required
NS_IMETHODIMP
morkCursor::AddWeakRef(nsIMdbEnv* mev)
{
  return morkNode::AddWeakRef((morkEnv *) mev);
}
NS_IMETHODIMP
morkCursor::AddStrongRef(nsIMdbEnv* mev)
{
  return morkNode::AddStrongRef((morkEnv *) mev);
}

NS_IMETHODIMP
morkCursor::CutWeakRef(nsIMdbEnv* mev)
{
  return morkNode::CutWeakRef((morkEnv *) mev);
}
NS_IMETHODIMP
morkCursor::CutStrongRef(nsIMdbEnv* mev)
{
  return morkNode::CutStrongRef((morkEnv *) mev);
}

  
NS_IMETHODIMP
morkCursor::CloseMdbObject(nsIMdbEnv* mev)
{
  return morkNode::CloseMdbObject((morkEnv *) mev);
}

NS_IMETHODIMP
morkCursor::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  *outOpen = IsOpenNode();
  return NS_OK;
}
NS_IMETHODIMP
morkCursor::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  *outIsReadonly = IsFrozen();
  return NS_OK;
}
// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

NS_IMETHODIMP
morkCursor::GetCount(nsIMdbEnv* mev, mdb_count* outCount)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkCursor::GetSeed(nsIMdbEnv* mev, mdb_seed* outSeed)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkCursor::SetPos(nsIMdbEnv* mev, mdb_pos inPos)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkCursor::GetPos(nsIMdbEnv* mev, mdb_pos* outPos)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkCursor::SetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool inFail)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkCursor::GetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool* outFail)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
