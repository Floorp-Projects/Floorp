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

#ifndef _MORKCELL_
#define _MORKCELL_ 1

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDelta_kShift 8 /* 8 bit shift */
#define morkDelta_kChangeMask 0x0FF /* low 8 bit mask */
#define morkDelta_kColumnMask (~ (mork_column) morkDelta_kChangeMask)
#define morkDelta_Init(self,cl,ch) ((self) = ((cl)<<morkDelta_kShift) | (ch))
#define morkDelta_Change(self) ((mork_change) ((self) & morkDelta_kChangeMask))
#define morkDelta_Column(self) ((self) >> morkDelta_kShift)

class morkCell { // minimal cell format

public:
  mork_delta   mCell_Delta;   // encoding of both column and change
  morkAtom*    mCell_Atom;    // content in this cell
  
public:
  morkCell() : mCell_Delta( 0 ), mCell_Atom( 0 ) { }

  morkCell(const morkCell& c)
  : mCell_Delta( c.mCell_Delta ), mCell_Atom( c.mCell_Atom ) { }
  
  // note if ioAtom is non-nil, caller needs to call ioAtom->AddCellUse():
  morkCell(mork_column inCol, mork_change inChange, morkAtom* ioAtom)
  {
    morkDelta_Init(mCell_Delta, inCol,inChange);
    mCell_Atom = ioAtom;
  }

  // note if ioAtom is non-nil, caller needs to call ioAtom->AddCellUse():
  void Init(mork_column inCol, mork_change inChange, morkAtom* ioAtom)
  {
    morkDelta_Init(mCell_Delta,inCol,inChange);
    mCell_Atom = ioAtom;
  }
  
  mork_column  GetColumn() const { return morkDelta_Column(mCell_Delta); }
  mork_change  GetChange() const { return morkDelta_Change(mCell_Delta); }
  
  mork_bool IsCellClean() const { return GetChange() == morkChange_kNil; }
  mork_bool IsCellDirty() const { return GetChange() != morkChange_kNil; }

  void SetCellClean(); // set change to kNil
  void SetCellDirty(); // set change to kAdd
  
  void SetCellColumnDirty(mork_column inCol)
  { this->SetColumnAndChange(inCol, morkChange_kAdd); }
  
  void SetCellColumnClean(mork_column inCol)
  { this->SetColumnAndChange(inCol, morkChange_kNil); }
  
  void         SetColumnAndChange(mork_column inCol, mork_change inChange)
  { morkDelta_Init(mCell_Delta, inCol, inChange); }
    
  morkAtom*  GetAtom() { return mCell_Atom; }
  
  void       SetAtom(morkEnv* ev, morkAtom* ioAtom, morkPool* ioPool);
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
  
  void       SetYarn(morkEnv* ev, const mdbYarn* inYarn, morkStore* ioStore);
  
  void       AliasYarn(morkEnv* ev, mdbYarn* outYarn) const;
  void       GetYarn(morkEnv* ev, mdbYarn* outYarn) const;
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKCELL_ */
