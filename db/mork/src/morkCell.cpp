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

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

void
morkCell::SetYarn(morkEnv* ev, const mdbYarn* inYarn, morkStore* ioStore)
{
  morkAtom* atom = ioStore->YarnToAtom(ev, inYarn);
  if ( atom )
    this->SetAtom(ev, atom, ioStore->StorePool()); // refcounts atom
}

void
morkCell::GetYarn(morkEnv* ev, mdbYarn* outYarn) const
{
  mCell_Atom->GetYarn(outYarn);
}

void
morkCell::AliasYarn(morkEnv* ev, mdbYarn* outYarn) const
{
  mCell_Atom->AliasYarn(outYarn);
}
  
  
void
morkCell::SetCellDirty()
{
  mork_column col = this->GetColumn();
  this->SetColumnAndChange(col, morkChange_kAdd);
}

void
morkCell::SetAtom(morkEnv* ev, morkAtom* ioAtom, morkPool* ioPool)
  // SetAtom() "acquires" the new ioAtom if non-nil, by calling AddCellUse()
  // to increase the refcount, and puts ioAtom into mCell_Atom.  If the old
  // atom in mCell_Atom is non-nil, then it is "released" first by a call to
  // CutCellUse(), and if the use count then becomes zero, then the old atom
  // is deallocated by returning it to the pool ioPool.  (And this is
  // why ioPool is a parameter to this method.)  Note that ioAtom can be nil
  // to cause the cell to refer to nothing, and the old atom in mCell_Atom
  // can also be nil, and all the atom refcounting is handled correctly.
  //
  // Note that if ioAtom was just created, it typically has a zero use count
  // before calling SetAtom().  But use count is one higher after SetAtom().
{
  morkAtom* oldAtom = mCell_Atom;
  if ( oldAtom != ioAtom ) // ioAtom is not already installed in this cell?
  {
    if ( oldAtom )
    {
      mCell_Atom = 0;
      if ( oldAtom->CutCellUse(ev) == 0 )
      {
        if ( ioPool )
        {
          if ( oldAtom->IsBook() )
            ((morkBookAtom*) oldAtom)->CutBookAtomFromSpace(ev);
            
          ioPool->ZapAtom(ev, oldAtom);
        }
        else
          ev->NilPointerError();
      }
    }
    if ( ioAtom )
      ioAtom->AddCellUse(ev);
      
    mCell_Atom = ioAtom;
  }
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
