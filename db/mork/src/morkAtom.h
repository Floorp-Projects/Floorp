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

#ifndef _MORKATOM_
#define _MORKATOM_ 1

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


#define morkAtom_kMaxByteSize 255 /* max for 8-bit integer */
#define morkAtom_kForeverCellUses 0x0FF /* max for 8-bit integer */
#define morkAtom_kMaxCellUses 0x07F /* max for 7-bit integer */

#define morkAtom_kKindWeeAnon  'a'  /* means morkWeeAnonAtom subclass */
#define morkAtom_kKindBigAnon  'A'  /* means morkBigAnonAtom subclass */
#define morkAtom_kKindWeeBook  'b'  /* means morkWeeBookAtom subclass */
#define morkAtom_kKindBigBook  'B'  /* means morkBigBookAtom subclass */
#define morkAtom_kKindRowOid   'r'  /* means morkOidAtom subclass */
#define morkAtom_kKindTableOid 't'  /* means morkOidAtom subclass */

/*| Atom: .
|*/
class morkAtom { //
 
public: 

  mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  mork_change   mAtom_Change;    // how has this atom been changed?
  mork_u1       mAtom_Size;      // only for atoms smaller than 256 bytes

public: 
  morkAtom(mork_aid inAid, mork_u1 inKind);
  
  mork_bool IsWeeAnon() const { return mAtom_Kind == morkAtom_kKindWeeAnon; }
  mork_bool IsBigAnon() const { return mAtom_Kind == morkAtom_kKindBigAnon; }
  mork_bool IsWeeBook() const { return mAtom_Kind == morkAtom_kKindWeeBook; }
  mork_bool IsBigBook() const { return mAtom_Kind == morkAtom_kKindBigBook; }
  mork_bool IsRowOid() const { return mAtom_Kind == morkAtom_kKindRowOid; }
  mork_bool IsTableOid() const { return mAtom_Kind == morkAtom_kKindTableOid; }

  mork_bool IsBook() const { return this->IsWeeBook() || this->IsBigBook(); }

public: // empty construction does nothing
  morkAtom() { }

public: // one-byte refcounting, freezing at maximum
  void       MakeCellUseForever(morkEnv* ev);
  mork_u1    AddCellUse(morkEnv* ev);
  mork_u1    CutCellUse(morkEnv* ev);
  
  mork_bool  IsCellUseForever() const 
  { return mAtom_CellUses == morkAtom_kForeverCellUses; }
  
  void       SetAtomDirty() { mAtom_Change = morkChange_kAdd; }

private: // warnings

  static void CellUsesUnderflowWarning(morkEnv* ev);

public: // errors

  static void BadAtomKindError(morkEnv* ev);
  static void ZeroAidError(morkEnv* ev);
  static void AtomSizeOverflowError(morkEnv* ev);

public: // yarns
  
  mork_bool   AsBuf(morkBuf& outBuf) const;
  mork_bool   AliasYarn(mdbYarn* outYarn) const;
  mork_bool   GetYarn(mdbYarn* outYarn) const;

private: // copying is not allowed
  morkAtom(const morkAtom& other);
  morkAtom& operator=(const morkAtom& other);
};

/*| OidAtom: an atom that references a row or table by identity.
|*/
class morkOidAtom : public morkAtom { //

  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // NOT USED IN "BIG" format atoms
 
public:
  mdbOid           mOidAtom_Oid;       // identity of referenced object

public: // empty construction does nothing
  morkOidAtom() { }
  void InitRowOidAtom(morkEnv* ev, const mdbOid& inOid);
  void InitTableOidAtom(morkEnv* ev, const mdbOid& inOid);

private: // copying is not allowed
  morkOidAtom(const morkOidAtom& other);
  morkOidAtom& operator=(const morkOidAtom& other);
};

/*| WeeAnonAtom: an atom whose content immediately follows morkAtom slots
**| in an inline fashion, so that morkWeeAnonAtom contains both leading
**| atom slots and then the content bytes without further overhead.  Note
**| that charset encoding is not indicated, so zero is implied for Latin1.
**| (Non-Latin1 content must be stored in a morkBigAnonAtom with a charset.)
**|
**|| An anon (anonymous) atom has no identity, with no associated bookkeeping
**| for lookup needed for sharing like a book atom.
**|
**|| A wee anon atom is immediate but not shared with any other users of this
**| atom, so no bookkeeping for sharing is needed.  This means the atom has
**| no ID, because the atom has no identity other than this immediate content,
**| and no hash table is needed to look up this particular atom.  This also
**| applies to the larger format morkBigAnonAtom, which has more slots.
|*/
class morkWeeAnonAtom : public morkAtom { //

  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // only for atoms smaller than 256 bytes
  
public:
  mork_u1 mWeeAnonAtom_Body[ 1 ]; // 1st byte of immediate content vector

public: // empty construction does nothing
  morkWeeAnonAtom() { }
  void InitWeeAnonAtom(morkEnv* ev, const morkBuf& inBuf);
  
  // allow extra trailing byte for a null byte:
  static mork_size SizeForFill(mork_fill inFill)
  { return sizeof(morkWeeAnonAtom) + inFill; }

private: // copying is not allowed
  morkWeeAnonAtom(const morkWeeAnonAtom& other);
  morkWeeAnonAtom& operator=(const morkWeeAnonAtom& other);
};

/*| BigAnonAtom: another immediate atom that cannot be encoded as the smaller
**| morkWeeAnonAtom format because either the size is too great, and/or the
**| charset is not the default zero for Latin1 and must be explicitly noted.
**|
**|| An anon (anonymous) atom has no identity, with no associated bookkeeping
**| for lookup needed for sharing like a book atom.
|*/
class morkBigAnonAtom : public morkAtom { //

  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // NOT USED IN "BIG" format atoms
 
public:
  mork_cscode   mBigAnonAtom_Form;      // charset format encoding
  mork_size     mBigAnonAtom_Size;      // size of content vector
  mork_u1       mBigAnonAtom_Body[ 1 ]; // 1st byte of immed content vector

public: // empty construction does nothing
  morkBigAnonAtom() { }
  void InitBigAnonAtom(morkEnv* ev, const morkBuf& inBuf, mork_cscode inForm);
  
  // allow extra trailing byte for a null byte:
  static mork_size SizeForFill(mork_fill inFill)
  { return sizeof(morkBigAnonAtom) + inFill; }

private: // copying is not allowed
  morkBigAnonAtom(const morkBigAnonAtom& other);
  morkBigAnonAtom& operator=(const morkBigAnonAtom& other);
};

#define morkBookAtom_kMaxBodySize 1024 /* if larger, cannot be shared */

/*| BookAtom: the common subportion of wee book atoms and big book atoms that
**| includes the atom ID and the pointer to the space referencing this atom
**| through a hash table.
|*/
class morkBookAtom : public morkAtom { //
  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // only for atoms smaller than 256 bytes
  
public:
  morkAtomSpace* mBookAtom_Space; // mBookAtom_Space->mSpace_Scope is atom scope 
  mork_aid       mBookAtom_Id;    // identity token for this shared atom

public: // empty construction does nothing
  morkBookAtom() { }

  static void NonBookAtomTypeError(morkEnv* ev);

public: // Hash() and Equal() for atom ID maps are same for all subclasses:

  mork_u4 HashAid() const { return mBookAtom_Id; }
  mork_bool EqualAid(const morkBookAtom* inAtom) const
  { return ( mBookAtom_Id == inAtom->mBookAtom_Id); }

public: // Hash() and Equal() for atom body maps know about subclasses:
  
  mork_u4 HashFormAndBody(morkEnv* ev) const;
  mork_bool EqualFormAndBody(morkEnv* ev, const morkBookAtom* inAtom) const;

public: // separation from containing space

  void CutBookAtomFromSpace(morkEnv* ev);

private: // copying is not allowed
  morkBookAtom(const morkBookAtom& other);
  morkBookAtom& operator=(const morkBookAtom& other);
};

/*| WeeBookAtom: .
|*/
class morkWeeBookAtom : public morkBookAtom { //
  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // only for atoms smaller than 256 bytes

  // morkAtomSpace* mBookAtom_Space; // mBookAtom_Space->mSpace_Scope is scope 
  // mork_aid       mBookAtom_Id;    // identity token for this shared atom
  
public:
  mork_u1     mWeeBookAtom_Body[ 1 ]; // 1st byte of immed content vector

public: // empty construction does nothing
  morkWeeBookAtom() { }
  morkWeeBookAtom(mork_aid inAid);
  
  void InitWeeBookAtom(morkEnv* ev, const morkBuf& inBuf,
    morkAtomSpace* ioSpace, mork_aid inAid);
  
  // allow extra trailing byte for a null byte:
  static mork_size SizeForFill(mork_fill inFill)
  { return sizeof(morkWeeBookAtom) + inFill; }

private: // copying is not allowed
  morkWeeBookAtom(const morkWeeBookAtom& other);
  morkWeeBookAtom& operator=(const morkWeeBookAtom& other);
};

/*| BigBookAtom: .
|*/
class morkBigBookAtom : public morkBookAtom { //
  
  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // NOT USED IN "BIG" format atoms

  // morkAtomSpace* mBookAtom_Space; // mBookAtom_Space->mSpace_Scope is scope 
  // mork_aid       mBookAtom_Id;    // identity token for this shared atom
  
public:
  mork_cscode   mBigBookAtom_Form;      // charset format encoding
  mork_size     mBigBookAtom_Size;      // size of content vector
  mork_u1       mBigBookAtom_Body[ 1 ]; // 1st byte of immed content vector

public: // empty construction does nothing
  morkBigBookAtom() { }
  void InitBigBookAtom(morkEnv* ev, const morkBuf& inBuf,
    mork_cscode inForm, morkAtomSpace* ioSpace, mork_aid inAid);
  
  // allow extra trailing byte for a null byte:
  static mork_size SizeForFill(mork_fill inFill)
  { return sizeof(morkBigBookAtom) + inFill; }

private: // copying is not allowed
  morkBigBookAtom(const morkBigBookAtom& other);
  morkBigBookAtom& operator=(const morkBigBookAtom& other);
};

/*| MaxBookAtom: .
|*/
class morkMaxBookAtom : public morkBigBookAtom { //
  
  // mork_u1       mAtom_Kind;      // identifies a specific atom subclass
  // mork_u1       mAtom_CellUses;  // number of persistent uses in a cell
  // mork_change   mAtom_Change;    // how has this atom been changed?
  // mork_u1       mAtom_Size;      // NOT USED IN "BIG" format atoms

  // morkAtomSpace* mBookAtom_Space; // mBookAtom_Space->mSpace_Scope is scope 
  // mork_aid       mBookAtom_Id;    // identity token for this shared atom

  // mork_cscode   mBigBookAtom_Form;      // charset format encoding
  // mork_size     mBigBookAtom_Size;      // size of content vector
  // mork_u1       mBigBookAtom_Body[ 1 ]; // 1st byte of immed content vector
  
public:
  mork_u1 mMaxBookAtom_Body[ morkBookAtom_kMaxBodySize + 3 ]; // max bytes

public: // empty construction does nothing
  morkMaxBookAtom() { }
  void InitMaxBookAtom(morkEnv* ev, const morkBuf& inBuf,
    mork_cscode inForm, morkAtomSpace* ioSpace, mork_aid inAid)
  { this->InitBigBookAtom(ev, inBuf, inForm, ioSpace, inAid); }

private: // copying is not allowed
  morkMaxBookAtom(const morkMaxBookAtom& other);
  morkMaxBookAtom& operator=(const morkMaxBookAtom& other);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKATOM_ */
