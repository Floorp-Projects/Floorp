/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _HPPA_INSTRUCTION_H_
#define _HPPA_INSTRUCTION_H_

#include "Fundamentals.h"
#include "Instruction.h"

enum HPPAInstructionKind
{
  ADD,     ADDBF,   ADDBT,   ADDO,    ADDIBF,  ADDIBT,  ADDIL,   ADDL,    ADDI,    ADDIT, 
  ADDITO,  ADDC,    ADDCO,   AND,     ANDCM,   BL,      BLE,     BLR,     BE,      BB, 
  BVB,     BV,      BREAK,   COMBF,   COMBT,   COMCLR,  COMIBF,  COMIBT,  COMICLR, CLDDX, 
  CLDDS,   CLDWX,   CLDWS,   COPR,    CSTDX,   CSTDS,   CSTWX,   CSTWS,   COPY,    DCOR, 
  DEP,     DEPI,    DIAG,    DS,      XOR,     EXTRS,   EXTRU,   XMPYU,   FABS,    FCMP,
  FCNVXF,  FCNVFX,  FCNVFXT, FCNVFF,  FDIV,    FLDDX,   FLDDS,   FLDWX,   FLDWS,   FMPY, 
  FMPYADD, FMPYSUB, FRND,    FSQRT,   FSTDX,   FSTDS,   FSTWS,   FSUB,    FTEST,   FDC,
  FDCE,    FIC,     FICE,    GATE,    DEBUGID, OR,      IDTLBA,  IDTLBP,  IITLBA,  IITLBP,
  IDCOR,   LDCWX,   LDCWS,   LDB,     LDBX,    LDBS,    LCI,     LDH,     LDHX,    LDHS,
  LDI,     LDIL,    LDO,     LPA,     LDSID,   LDW,     LDWAX,   LDWAS,   LDWM,    LDWX,
  LDWS,    MOVB,    MFCTL,   MFDBAM,  MFDBAO,  MFIBAM,  MFIBAO,  MFSP,    MOVIB,   MCTL,
  MTBAM,   MTBAO,   MTIBAM,  MTIBAO,  MTSP,    MTSM,    PMDIS,   PMENB,   PROBER,  PROBERI,
  PROBEW,  PROBEWI, PDC,     PDTLB,   PDTLBE,  PITLB,   PITLBE,  RSM,     RFI,     RFIR,
  SSM,     SHD,     SH1ADD,  SH1ADDL, SH1ADDO, SH2ADD,  SH2ADDL, SH2ADDO, SH3ADD,  SH3ADDL,
  SH3ADDO, SPOP0,   SPOP1,   SPOP2,   SPOP3,   STB,     STBS,    STBYS,   STH,     STHS,
  STW,     STWAS,   STWM,    STWS,    SUB,     SUBT,    SUBTO,   SUBO,    SUBI,    SUBIO,
  SUBB,    SUBBO,   SYNC,    SYNCDMA, UADDCM,  UADDCMT, UXOR,    VDEP,    VDEPI,   VEXTRU,
  VSHD,    ZDEP,    ZDEPI,   ZVDEP,   ZVDEPI,
  
  nHPPAInstructionKind, INVALID
};

typedef PRUint32 HPPAInstructionFlags;

struct HPPAInstructionInfo
{
  HPPAInstructionKind  kind;
  PRUint8              opcode;
  PRUint8              ext;
  PRUint8              format;
  char*                string;
  HPPAInstructionFlags flags;
};

#define HPPA_isValidInsn(kind) !(hiInfo[kind].flags & HPPA_UNIMP)

enum HPPAConditionKind
{
  hcNever,  hcE,  hcL,  hcLe, hcLu,  hcLeu, hcOver,  hcOdd,
  hcAlways, hcNe, hcGe, hcG,  hcGeu, hcGu,  hcNOver, hcEven,

  nHPPAConditionKind
};

struct HPPAConditionInfo
{
  HPPAConditionKind kind;
  HPPAConditionKind swapped;
  PRUint8 c;
  bool f;
  char *string;
};

enum HPPARegisterNumber
{
  zero,  r1,    rp,    r3,    r4,    r5,    r6,    r7,
  r8,    r9,    r10,   r11,   r12,   r13,   r14,   r15, 
  r16,   r17,   r18,   r19,   r20,   r21,   r22,   arg3,
  arg2,  arg1,  arg0,  dp,    ret0,  ret1,  sp,    r31,
  fr0L,  fr0R,  fr1L,  fr1R,  fr2L,  fr2R,  fr3L,  fr3R,
  fr4L,  fr4R,  fr5L,  fr5R,  fr6L,  fr6R,  fr7L,  fr7R,
  fr8L,  fr8R,  fr9L,  fr9R,  fr10L, fr10R, fr11L, fr11R,
  fr12L, fr12R, fr13L, fr13R, fr14L, fr14R, fr15L, fr15R,
  fr16L, fr16R, fr17L, fr17R, fr18L, fr18R, fr19L, fr19R, 
  fr20L, fr20R, fr21L, fr21R, fr22L, fr22R, fr23L, fr23R,
  fr24L, fr24R, fr25L, fr25R, fr26L, fr26R, fr27L, fr27R, 
  fr28L, fr28R, fr29L, fr29R, fr30L, fr30R, fr31L, fr31R,
};

enum HPPASpecialCallKind
{
  RemI,
  RemU,
  DivI,
  DivU,
  nSpecialCalls
};

struct HPPASpecialCallInfo
{
  HPPASpecialCallKind kind;
  char*               string;
  PRUint8             nUses;
  PRUint8             nDefines;
  PRUint32            interferences1;
  PRUint32            interferences2;
  PRUint32            interferences3;
  void*               address;
};

extern char* registerNumberToString[];
extern PRUint8 registerNumberToColor[];
extern HPPARegisterNumber colorToRegisterNumber[];
extern HPPAInstructionInfo hiInfo[];
extern HPPAConditionInfo hcInfo[];
extern HPPASpecialCallInfo hscInfo[];
extern HPPAInstructionKind shiftAddl[];


inline HPPARegisterNumber
udToRegisterNumber(InstructionUseOrDefine& useDef)
{
  assert(useDef.isVirtualRegister());
  return (colorToRegisterNumber[useDef.getVirtualRegister().getColor()]);
}

class HPPAInstruction : public InsnUseXDefineYFromPool
{
protected:
  HPPAInstructionKind  kind;
  HPPAInstructionFlags flags;
  PRUint8                in;
  PRUint8                out;

  inline static HPPAInstructionFlags kindFlags(HPPAInstructionKind iKind, HPPAInstructionFlags others) {return hiInfo[iKind].flags | others;}

  virtual HPPARegisterNumber getR1();
  virtual HPPARegisterNumber getR2();
  virtual HPPARegisterNumber getT();

  inline char* getR1String() {return registerNumberToString[getR1()];}
  inline char* getR2String() {return registerNumberToString[getR2()];}
  inline char* getTString() {return registerNumberToString[getT()];}

public:
  HPPAInstruction(DataNode* inPrimitive, Pool& inPool, PRUint8 nConsumer, PRUint8 nProducer, HPPAInstructionKind iKind, HPPAInstructionFlags iFlags) :
	InsnUseXDefineYFromPool(inPrimitive, inPool, nConsumer, nProducer), kind(iKind), flags(kindFlags(iKind, iFlags)), in(nConsumer), out(nProducer) {}

  virtual size_t getFormattedSize() {return (4);}              
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif

};

#define HPPA_UNIMP             0x80000000
#define HPPA_NONE              0x00000000
#define HPPA_R1_IS_ZERO        0x40000000
#define HPPA_IS_COPY           0x20000000
#define HPPA_NULLIFICATION     0x10000000
#define HPPA_DATA_HAS_IM14     0x08000000
#define HPPA_DATA_HAS_IM5      0x04000000
#define HPPA_DATA_HAS_IM21     0x02000000
#define HPPA_LOAD_VOLATILE     0x01000000
#define HPPA_LOAD_CONSTANT     0x00800000
#define HPPA_STORE_CONSTANT    0x00800000
#define HPPA_REGISTER_INDIRECT 0x00400000
#define HPPA_LAST_FLAG         0x00200000

#define HPPA_IM14_MASK         0x00003fff
#define HPPA_IM14_SHIFT        0
#define HPPA_VALID_IM14_BIT    0x8000
#define validIM14(im14)        (im14 & HPPA_VALID_IM14_BIT)
#define hasIM14(data)          ((data) & HPPA_DATA_HAS_IM14)
#define buildIM14(constant)    ((((constant) & HPPA_IM14_MASK) << HPPA_IM14_SHIFT) | HPPA_DATA_HAS_IM14)
#define getIM14(data)          (hasIM14(data) ? ((((data) >> HPPA_IM14_SHIFT) & HPPA_IM14_MASK) | HPPA_VALID_IM14_BIT) : 0)
#define IM14(im14)             ((PRInt16)((im14 & 0x2000) ? (im14 | ~HPPA_IM14_MASK) : (im14 & HPPA_IM14_MASK)))

#define HPPA_IM5_MASK          0x0000001f
#define HPPA_IM5_SHIFT         0
#define HPPA_VALID_IM5_BIT     0x80
#define validIM5(im5)          (im5 & HPPA_VALID_IM5_BIT)
#define hasIM5(data)           ((data) & HPPA_DATA_HAS_IM5)
#define buildIM5(constant)     ((((constant) & HPPA_IM5_MASK) << HPPA_IM5_SHIFT) | HPPA_DATA_HAS_IM5)
#define getIM5(data)           (hasIM5(data) ? ((((data) >> HPPA_IM5_SHIFT) & HPPA_IM5_MASK) | HPPA_VALID_IM5_BIT) : 0)
#define IM5(im5)               ((PRInt8)((im5 & 0x10) ? (im5 | ~HPPA_IM5_MASK) : (im5 & HPPA_IM5_MASK)))

#define HPPA_IM21_MASK         0x001fffff
#define HPPA_IM21_SHIFT        0
#define HPPA_VALID_IM21_BIT    0x00200000
#define hasIM21(im21)          (im21 & HPPA_DATA_HAS_IM21)
#define buildIM21(constant)    ((((constant) & HPPA_IM21_MASK) << HPPA_IM21_SHIFT) | HPPA_DATA_HAS_IM21)
#define getIM21(data)          (hasIM21(data) ? ((((data) >> HPPA_IM21_SHIFT) & HPPA_IM21_MASK) | HPPA_VALID_IM21_BIT) : 0)
#define IM21(im21)             ((PRInt32)((im21 & HPPA_IM21_MASK) << 11))

#define FITS_IM5(v,c)						\
  PR_BEGIN_MACRO							\
  if ((unsigned)(v) + 0x10 < 0x20)			\
	{										\
	  c = v;								\
	  return true;							\
	}										\
  else										\
	return false;							\
  PR_END_MACRO

#define FITS_UIM5(v,c)						\
  PR_BEGIN_MACRO							\
  if ((unsigned)(v) < 0x20)	   				\
	{										\
	  c = v;								\
	  return true;							\
	}										\
  else										\
	return false;							\
  PR_END_MACRO

#define FITS_IM11(v,c)						\
  PR_BEGIN_MACRO							\
  if ((unsigned)(v) + 0x400 < 0x800)		\
	{										\
	  c = v;								\
	  return true;							\
	}										\
  else										\
	return false;							\
  PR_END_MACRO

#define FITS_IM14(v,c)						\
  PR_BEGIN_MACRO							\
  if ((unsigned)(v) + 0x2000 < 0x4000)		\
	{										\
	  c = v;								\
	  return true;							\
	}										\
  else										\
	return false;							\
  PR_END_MACRO

#define HPPA_ADDR_ROUND(addr) (((addr) + 0x1000) & ~0x1fff)
#define HPPA_LEFTR(addr)      ((HPPA_ADDR_ROUND(addr) >> 11) & 0x1fffff)
#define HPPA_RIGHTR(addr)     ((addr) - HPPA_ADDR_ROUND(addr))
#define HPPA_LEFT(value)      (((value) & 0xfffff800) >> 11)
#define HPPA_RIGHT(value)     ((value) & 0x7ff)

inline bool extractIM5(const Value& v, PRUint8& out)	{FITS_IM5(v.i,out);}
inline bool extractIM5(PRInt32 v, PRUint8& out)			{FITS_IM5(v,out);}
inline bool extractUIM5(const Value& v, PRUint8& out)	{FITS_UIM5(v.i,out);}
inline bool extractUIM5(PRInt32 v, PRUint8& out)		{FITS_UIM5(v,out);}
inline bool extractIM11(const Value& v, PRUint16& out)	{FITS_IM11(v.i,out);}
inline bool extractIM11(PRInt32 v, PRUint16& out)		{FITS_IM11(v,out);}
inline bool extractIM14(const Value& v, PRUint16& out)	{FITS_IM14(v.i,out);}
inline bool extractIM14(PRInt32 v, PRUint16& out)		{FITS_IM14(v,out);}

template<class HPPAFormatN, HPPAInstructionKind iKind>
class HPPAGeneric : public HPPAFormatN
{
public:
  HPPAGeneric(DataNode* inPrimitive, Pool& inPool, HPPAInstructionFlags flags = 0) :
	HPPAFormatN(inPrimitive, inPool, iKind, flags) {}
};

template<class HPPAFormatN, HPPAInstructionKind iKind>
class HPPAGenericU16 : public HPPAFormatN
{
public:
  HPPAGenericU16(DataNode* inPrimitive, Pool& inPool, PRUint16 data, HPPAInstructionFlags flags = 0) :
	HPPAFormatN(inPrimitive, inPool, iKind, data, flags) {}
};

template<class HPPAFormatN, HPPAInstructionKind iKind>
class HPPAGenericU32 : public HPPAFormatN
{
public:
  HPPAGenericU32(DataNode* inPrimitive, Pool& inPool, PRUint32 data, HPPAInstructionFlags flags = 0) :
	HPPAFormatN(inPrimitive, inPool, iKind, data, flags) {}
};

template<class HPPAFormatN, HPPAInstructionKind iKind>
class HPPAGenericU8U8 : public HPPAFormatN
{
public:
  HPPAGenericU8U8(DataNode* inPrimitive, Pool& inPool, PRUint8 data1, PRUint8 data2, HPPAInstructionFlags flags = 0) :
	HPPAFormatN(inPrimitive, inPool, iKind, data1, data2, flags) {}
};


// Format1: 
//
// Loads and Stores, Load and Store Word Modify, Load Offset
// |31               |25            |20            |15   |13                                      0|
// |-----------------|--------------|--------------|-----|-----------------------------------------|
// |       op        |       b      |      t/r     |  s  |                  im14                   |
// |-----------------|--------------|--------------|-----|-----------------------------------------|

class HPPAFormat1 : public HPPAInstruction
{
protected:
  PRUint16 im14;

  inline static PRUint8 getNConsumer(HPPAInstructionKind kind, HPPAInstructionFlags flags) {return (kindFlags(kind, flags) & HPPA_R1_IS_ZERO) ? 0 : 1;}
  virtual PRUint16 getIm14() {return IM14(im14);}

public:
  HPPAFormat1(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, getNConsumer(kind,flags), 1, kind, flags), im14(getIM14(flags)) {assert(hasIM14(flags));}
  HPPAFormat1(DataNode* inPrimitive, Pool& inPool, PRUint8 nConsumer, PRUint8 nProducer, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, nConsumer, nProducer, kind, flags), im14(getIM14(flags)) {assert(hasIM14(flags));}


  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

class HPPALoad : public HPPAFormat1
{
protected:
  inline static PRUint8 getNConsumer(HPPAInstructionKind kind, HPPAInstructionFlags flags) {return ((kindFlags(kind, flags) & HPPA_LOAD_CONSTANT) == 0) ? 2 : 1;}
  inline static PRUint8 getNProducer(HPPAInstructionKind kind, HPPAInstructionFlags flags) {return ((kindFlags(kind, flags) & HPPA_LOAD_VOLATILE) != 0) ? 2 : 1;}

public:
  HPPALoad(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAFormat1(inPrimitive, inPool, getNConsumer(kind,flags), getNProducer(kind,flags), kind, flags) {}
  HPPALoad(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, PRUint8 nConsumer, PRUint8 nProducer, HPPAInstructionFlags flags = 0) :
	HPPAFormat1(inPrimitive, inPool, nConsumer, nProducer, kind, flags) {}

#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

class HPPASpillLoad : public HPPALoad
{
protected:
  virtual PRUint16 getIm14() {return 0;}
  virtual HPPARegisterNumber getR1() {return sp;}

public:
  HPPASpillLoad(DataNode* inPrimitive, Pool& inPool, VirtualRegister& stackSlotRegister, VirtualRegister& destRegister) :
	HPPALoad(inPrimitive, inPool, LDW, HPPA_LOAD_CONSTANT | buildIM14(0)) 
	{addUse(0, stackSlotRegister, vrcStackSlot); addDefine(0, destRegister, vrcInteger);}
};

class HPPALoadIntegerFromConvertStackSlot : public HPPALoad
{
protected:
  virtual PRUint16 getIm14() {return -16;}
  virtual HPPARegisterNumber getR1() {return sp;}

public:
  HPPALoadIntegerFromConvertStackSlot(DataNode* inPrimitive, Pool& inPool, VirtualRegister& destRegister) :
	HPPALoad(inPrimitive, inPool, LDW, 0, 1, buildIM14(0)) {addDefine(0, destRegister, vrcInteger);}
};


class HPPAStore : public HPPAFormat1
{
protected:
  inline static PRUint8 getNConsumer(HPPAInstructionKind kind, HPPAInstructionFlags flags) {return ((kindFlags(kind, flags) & HPPA_STORE_CONSTANT) == 0) ? 3 : 1;}

public:
  HPPAStore(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAFormat1(inPrimitive, inPool, getNConsumer(kind, flags), 1, kind, flags) {}
  HPPAStore(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, PRUint8 nConsumer, PRUint8 nProducer, HPPAInstructionFlags flags = 0) :
	HPPAFormat1(inPrimitive, inPool, nConsumer, nProducer, kind, flags) {}

  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

class HPPASpillStore : public HPPAStore
{
protected:
  virtual PRUint16 getIm14() {return 0;}
  virtual HPPARegisterNumber getR1() {return sp;}

public:
  HPPASpillStore(DataNode* inPrimitive, Pool& inPool, VirtualRegister& stackSlotRegister, VirtualRegister& srcRegister) :
	HPPAStore(inPrimitive, inPool, STW, HPPA_STORE_CONSTANT | buildIM14(0)) 
	{addUse(0, srcRegister, vrcInteger); addDefine(0, stackSlotRegister, vrcStackSlot);}

};

class HPPAStoreIntegerToConvertStackSlot : public HPPAStore
{
protected:
  virtual PRUint16 getIm14() {return -16;}
  virtual HPPARegisterNumber getR1() {return sp;}

public:
  HPPAStoreIntegerToConvertStackSlot(DataNode* inPrimitive, Pool& inPool, VirtualRegister& srcRegister) :
	HPPAStore(inPrimitive, inPool, STW, 1, 0, buildIM14(0)) {addUse(0, srcRegister, vrcInteger);}
};

typedef HPPAGeneric<HPPAFormat1,LDI> HPPALdi;
typedef HPPAGeneric<HPPAFormat1,LDO> HPPALdo;

// Format2:
//
// Indexed Loads
// |31               |25            |20            |15   |13|12|11   |9          |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|-----------|--|--------------|
// |       op        |       b      |       x      |  s  | u| 0|  cc |   ext4    | m|      t       |
// |-----------------|--------------|--------------|-----|--|--|-----|-----------|--|--------------|

// Format3:
// 
// Short Displacement Loads
// |31               |25            |20            |15   |13|12|11   |9          |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|-----------|--|--------------|
// |       op        |       b      |      im5     |  s  | a| 1|  cc |   ext4    | m|      t       |
// |-----------------|--------------|--------------|-----|--|--|-----|-----------|--|--------------|

// Format4:
//
// Short Displacement Stores, Store Bytes Short
// |31               |25            |20            |15   |13|12|11   |9          |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|-----------|--|--------------|
// |       op        |       b      |      im5     |  s  | a| 1|  cc |   ext4    | m|     im5      |
// |-----------------|--------------|--------------|-----|--|--|-----|-----------|--|--------------|

// Format5:
//
// Long Immediates
// |31               |25            |20                                                           0|
// |-----------------|--------------|--------------------------------------------------------------|
// |       op        |     t/r      |                             im21                             |
// |-----------------|--------------|--------------------------------------------------------------|
class HPPAFormat5 : public HPPAInstruction
{
protected:
  PRUint32 im21;

public:
  HPPAFormat5(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, 0, 1, kind, flags), im21(getIM21(flags)) {assert(hasIM21(flags));}
  
  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

typedef HPPAGeneric<HPPAFormat5,LDIL> HPPALdil;

// Format6:
//
// Arithmetic/Logical
// |31               |25            |20            |15      |12|11               |5 |4            0|
// |-----------------|--------------|--------------|--------|--|-----------------|--|--------------|
// |       op        |      r2      |      r1      |    c   | f|      ext6       |0 |       t      |
// |-----------------|--------------|--------------|--------|--|-----------------|--|--------------|
//

class HPPAFormat6 : public HPPAInstruction
{
protected:
  inline static PRUint8 getNConsumer(HPPAInstructionKind kind, HPPAInstructionFlags flags) {return (kindFlags(kind, flags) & HPPA_R1_IS_ZERO) ? 1 : 2;}

public:
  HPPAFormat6(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, getNConsumer(kind,flags), 1, kind, flags) {}

#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
  virtual void formatToMemory(void* where, uint32 offset);
  virtual InstructionFlags getFlags() const {return (flags & HPPA_IS_COPY) ? ifCopy : ifNone;}
};

typedef HPPAGeneric<HPPAFormat6,OR> HPPAOr;
typedef HPPAGeneric<HPPAFormat6,AND> HPPAAnd;
typedef HPPAGeneric<HPPAFormat6,XOR> HPPAXor;
typedef HPPAGeneric<HPPAFormat6,ADD> HPPAAdd;
typedef HPPAGeneric<HPPAFormat6,SUB> HPPASub;
typedef HPPAGeneric<HPPAFormat6,ADDL> HPPAAddl;
typedef HPPAGeneric<HPPAFormat6,COPY> HPPACopy;
typedef HPPAGeneric<HPPAFormat6,SH1ADDL> HPPASh1Addl;
typedef HPPAGeneric<HPPAFormat6,SH2ADDL> HPPASh2Addl;
typedef HPPAGeneric<HPPAFormat6,SH3ADDL> HPPASh3Addl;

// Format7:
//
// Arithmetic Immediate
// |31               |25            |20            |15      |12|11|10                             0|
// |-----------------|--------------|--------------|--------|--|--|--------------------------------|
// |       op        |       r      |       t      |    c   | f| e|             im11               |
// |-----------------|--------------|--------------|--------|--|--|--------------------------------|

// Format8:
//
// Extract
// |31               |25            |20            |15      |12      |9             |4            0|
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|
// |       op        |       r      |       t      |    c   |  ext3  |       p      |     clen     |
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|
class HPPAFormat8 : public HPPAInstruction
{
protected:
  PRUint8 cp;
  PRUint8 clen;

public:
  HPPAFormat8(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, PRUint8 p, PRUint8 len, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, 1, 1, kind, flags), cp(p), clen(len) {}

  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

typedef HPPAGenericU8U8<HPPAFormat8,EXTRU> HPPAExtru;
typedef HPPAGenericU8U8<HPPAFormat8,EXTRS> HPPAExtrs;

// Format9:
//
// Deposit
// |31               |25            |20            |15      |12      |9             |4            0|
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|
// |       op        |       t      |     r/im5    |    c   |  ext3  |      cp      |     clen     |
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|

class HPPAFormat9 : public HPPAInstruction
{
protected:
  PRUint8 im5;
  PRUint8 cp;
  PRUint8 clen;

  inline static PRUint8 getNConsumer(HPPAInstructionKind kind, HPPAInstructionFlags flags) 
	{ 
	  PRUint8 nUse = hasIM5(kindFlags(kind,flags)) ? 1 : 2;
	  // for ZDEP, ZDEPI the target is erased. This register will die.
	  return (kindFlags(kind, flags) & HPPA_R1_IS_ZERO) ? nUse - 1 : nUse;
	}

public:
  HPPAFormat9(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, PRUint8 p, PRUint8 len, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, getNConsumer(kind, flags), 1, kind, flags), im5(hasIM5(flags) ? getIM5(flags) : 0),
	cp(p), clen(len) {}

  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

typedef HPPAGenericU8U8<HPPAFormat9,DEPI> HPPADepi;
typedef HPPAGenericU8U8<HPPAFormat9,ZDEP> HPPAZdep;

// Format10:
//
// Shift
// |31               |25            |20            |15      |12      |9             |4            0|
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|
// |       op        |      r2      |      r1      |    c   |  ext3  |      cp      |       t      |
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|

// Format11:
//
// Conditional Branch
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |     r2/p     |     r1/im5   |    c   |                w1              | n| w|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|

class HPPAFormat11 : public HPPAInstruction
{
protected:
  ControlNode& target;
  HPPAConditionKind cond;
  PRUint8 im5;
  bool nullification;

  inline static PRUint8 getNConsumer(HPPAInstructionKind kind, HPPAInstructionFlags flags) {return hasIM5(kindFlags(kind,flags)) ? 1 : 2;}

public:
  HPPAFormat11(DataNode* inPrimitive, Pool& inPool, ControlNode& t, HPPAInstructionKind kind, 
			   HPPAConditionKind cKind, HPPAInstructionFlags flags = 0) 
	: HPPAInstruction(inPrimitive, inPool, getNConsumer(kind, flags), 0, kind, flags), 
	  target(t), cond(cKind), im5(hasIM5(flags) ? getIM5(flags) : 0), nullification(flags & HPPA_NULLIFICATION)
	{}

  virtual size_t getFormattedSize() {return (nullification ? 8 : 4);}
  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

typedef HPPAFormat11 HPPACondBranch;

// Format12:
//
// Branch External, Branch and Link External
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |      b       |      w1      |    s   |                w2              | n| w|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|

class HPPAFormat12 : public HPPAInstruction
{
protected:
  ControlNode& target;
  bool nullification;

public:
  HPPAFormat12(DataNode* inPrimitive, Pool& inPool, ControlNode& t, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) 
	: HPPAInstruction(inPrimitive, inPool, 0, 0, kind, flags), 
	  target(t), nullification(flags & HPPA_NULLIFICATION)
	{}

  virtual HPPARegisterNumber getT() {return zero;}
  virtual size_t getFormattedSize() {return (nullification ? 8 : 4);}
  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

typedef HPPAFormat12 HPPAAbsoluteBranch;

// Format13:
//
// Branch and Link, Gateway
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |      t       |      w1      |  ext3  |                w2              | n| w|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|

class HPPAFormat13 : public HPPAInstruction
{
protected:
  void*              address;
  HPPARegisterNumber returnRegister;
  bool nullification;

public:
  HPPAFormat13(DataNode* inPrimitive, Pool& inPool, void *f, PRUint8 nUses, PRUint8 nDefines, HPPAInstructionKind kind, 
			   HPPARegisterNumber ret, HPPAInstructionFlags flags = 0)
	: HPPAInstruction(inPrimitive, inPool, nUses, nDefines, kind, flags), address(f), returnRegister(ret), nullification(flags & HPPA_NULLIFICATION) {}

  virtual char *getAddressString() {return "";}
  virtual size_t getFormattedSize() {return (nullification ? 8 : 4);}
  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

class HPPASpecialCall : public HPPAFormat13
{
protected:
  HPPASpecialCallKind callKind;

public:
  HPPASpecialCall(DataNode* inPrimitive, Pool& inPool, HPPASpecialCallKind kind, HPPAInstructionFlags flags = 0) :
	HPPAFormat13(inPrimitive, inPool, hscInfo[kind].address, hscInfo[kind].nUses, hscInfo[kind].nDefines, BL, r31, flags), callKind(kind) {}
  
  virtual char* getAddressString() {return hscInfo[callKind].string;}
  virtual HPPARegisterNumber getT() {return r31;}

  virtual PRUint32* getExtraRegisterInterferenceBegin() {return &hscInfo[callKind].interferences1;}
  virtual PRUint32* getExtraRegisterInterferenceEnd() {return &hscInfo[callKind].interferences3 + 1;}
  virtual InstructionFlags getFlags() const {return ifSpecialCall;}
};

// Format14:
//
// Branch and Link Register, Branch Vectored
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |     t/b      |      x       |  ext3  |                 0              | n| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|

// Format15:
//
// Data Memory Management, Probe
// |31               |25            |20            |15   |13                     |5 |4            0|
// |-----------------|--------------|--------------|-----|-----------------------|--|--------------|
// |       op        |       b      |    r/x/im5   |  s  |          ext8         |m |      t       |
// |-----------------|--------------|--------------|-----|-----------------------|--|--------------|

// Format16:
//
// Instruction Memory Management
// |31               |25            |20            |15      |12                  |5 |4            0|
// |-----------------|--------------|--------------|--------|--------------------|--|--------------|
// |       op        |       b      |    r/x/im5   |   s    |       ext7         |m |      t       |
// |-----------------|--------------|--------------|--------|--------------------|--|--------------|

// Format17:
//
// Break
// |31               |25                                    |12                     |4            0|
// |-----------------|--------------------------------------|-----------------------|--------------|
// |       op        |                im13                  |          ext8         |     im5      |
// |-----------------|--------------------------------------|-----------------------|--------------|

// Format18:
//
// Diagnose
// |31               |25                                                                          0|
// |-----------------|-----------------------------------------------------------------------------|
// |       op        |                                                                             |
// |-----------------|-----------------------------------------------------------------------------|

// Format19:
//
// Move to/from Space Register
// |31               |25            |20            |15      |12                     |4            0|
// |-----------------|--------------|--------------|--------|-----------------------|--------------|
// |       op        |      rv      |       r      |   s    |          ext8         |      t       |
// |-----------------|--------------|--------------|--------|-----------------------|--------------|

// Format20:
//
// Load Space ID
// |31               |25            |20            |15   |13|12                     |4            0|
// |-----------------|--------------|--------------|-----|--|-----------------------|--------------|
// |       op        |       b      |      rv      |  s  | 0|          ext8         |      t       |
// |-----------------|--------------|--------------|-----|--|-----------------------|--------------|


// Format21:
//
// Move to Control Register
// |31               |25            |20            |15      |12                     |4            0|
// |-----------------|--------------|--------------|--------|-----------------------|--------------|
// |       op        |      t       |       r      |   rv   |          ext8         |      0       |
// |-----------------|--------------|--------------|--------|-----------------------|--------------|

// Format22:
//
// Move from Control Register
// |31               |25            |20            |15      |12                     |4            0|
// |-----------------|--------------|--------------|--------|-----------------------|--------------|
// |       op        |      r       |       0      |   rv   |          ext8         |      t       |
// |-----------------|--------------|--------------|--------|-----------------------|--------------|

// Format23:
//
// System Control
// |31               |25            |20            |15      |12                     |4            0|
// |-----------------|--------------|--------------|--------|-----------------------|--------------|
// |       op        |      b       |     r/im5    |    0   |          ext8         |      t       |
// |-----------------|--------------|--------------|--------|-----------------------|--------------|

// Format24:
//
// Special Operation Zero
// |31               |25                                          |10   |8       |5 |4            0|
// |-----------------|--------------------------------------------|-----|--------|--|--------------|
// |       op        |                    sop1                    |  0  |  sfu   |n |     sop2     |
// |-----------------|--------------------------------------------|-----|--------|--|--------------|

// Format25:
//
// Special Operation One
// |31               |25                                          |10   |8       |5 |4            0|
// |-----------------|--------------------------------------------|-----|--------|--|--------------|
// |       op        |                    sop                     |  1  |  sfu   |n |      t       |
// |-----------------|--------------------------------------------|-----|--------|--|--------------|

// Format26:
//
// Special Operation Two
// |31               |25            |20                           |10   |8       |5 |4            0|
// |-----------------|--------------|-----------------------------|-----|--------|--|--------------|
// |       op        |       r      |            sop1             |  2  |  sfu   |n |     sop2     |
// |-----------------|--------------|-----------------------------|-----|--------|--|--------------|

// Format27:
//
// Special Operation Three
// |31               |25            |20            |15            |10   |8       |5 |4            0|
// |-----------------|--------------|--------------|--------------|-----|--------|--|--------------|
// |       op        |       r2     |       r1     |     sop1     |  3  |  sfu   |n |     sop2     |
// |-----------------|--------------|--------------|--------------|-----|--------|--|--------------|

// Format28:
//
// Coprocessor Operation
// |31               |25                                                |8       |5 |4            0|
// |-----------------|--------------------------------------------------|--------|--|--------------|
// |       op        |                      sop1                        |  uid   |n |     sop2     |
// |-----------------|--------------------------------------------------|--------|--|--------------|

// Format29:
// 
// Coprocessor Indexed Loads
// |31               |25            |20            |15   |13|12|11   |9 |8       |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
// |       op        |       b      |       x      |  s  |u |0 | cc  |0 |  uid   |m |      t       |
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|

// Format30:
//
// Coprocessor Indexed Stores
// |31               |25            |20            |15   |13|12|11   |9 |8       |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
// |       op        |       b      |       x      |  s  |u |0 | cc  |1 |  uid   |m |      r       |
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|

// Format31:
//
// Coprocessor Short Displacement Loads
// |31               |25            |20            |15   |13|12|11   |9 |8       |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
// |       op        |       b      |      im5     |  s  |a |1 | cc  |0 |  uid   |m |      t       |
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
class HPPAFormat31 : public HPPAInstruction
{
protected:
  PRUint8 im5;
  virtual PRUint8 getIm5() {return IM5(im5);}

public:
  HPPAFormat31(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, 0, 1, kind, flags), im5(getIM5(flags)) {assert(hasIM5(flags));}
  
  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

class HPPALoadFixedPointFromConvertStackSlot : public HPPAFormat31
{
protected:
  virtual PRUint8 getIm5() {return -16;}
  virtual HPPARegisterNumber getR1() {return sp;}

public:
  HPPALoadFixedPointFromConvertStackSlot(DataNode* inPrimitive, Pool& inPool, VirtualRegister& destRegister) :
	HPPAFormat31(inPrimitive, inPool, FLDWS, HPPA_LOAD_CONSTANT | buildIM5(0)) {addDefine(0, destRegister, vrcFixedPoint);}
};

// Format32:
//
// Coprocessor Short Displacement Stores
// |31               |25            |20            |15   |13|12|11   |9 |8       |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
// |       op        |       b      |      im5     |  s  |a |1 | cc  |1 |  uid   |m |      r       |
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
class HPPAFormat32 : public HPPAInstruction
{
protected:
  PRUint8 im5;
  virtual PRUint8 getIm5() {return IM5(im5);}

public:
  HPPAFormat32(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, 1, 0, kind, flags), im5(getIM5(flags)) {assert(hasIM5(flags));}
  
  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};

class HPPAStoreFixedPointToConvertStackSlot : public HPPAFormat32
{
protected:
  virtual PRUint8 getIm5() {return -16;}
  virtual HPPARegisterNumber getR1() {return sp;}

public:
  HPPAStoreFixedPointToConvertStackSlot(DataNode* inPrimitive, Pool& inPool, VirtualRegister& srcRegister) :
	HPPAFormat32(inPrimitive, inPool, FSTWS, HPPA_STORE_CONSTANT | buildIM5(0)) {addUse(0, srcRegister, vrcFixedPoint);}
};

// Format33:
//
// Floating-point Operation Zero, Major Opcode 0C
// |31               |25            |20            |15      |12   |10   |8       |5 |4            0|
// |-----------------|--------------|--------------|--------|-----|-----|--------|--|--------------|
// |       op        |       r      |      0       |  sop   | fmt |  0  |   0    |0 |      t       |
// |-----------------|--------------|--------------|--------|-----|-----|--------|--|--------------|

// Format34:
//
// Floating-point Operation One, Major Opcode 0C
// |31               |25            |20         |16   |14   |12   |10   |8       |5 |4            0|
// |-----------------|--------------|-----------|-----|-----|-----|-----|--------|--|--------------|
// |       op        |       r      |     0     | sop | df  | sf  |  1  |    0   |0 |      t       |
// |-----------------|--------------|-----------|-----|-----|-----|-----|--------|--|--------------|

// Format35:
//
// Floating-point Operation Two, Major Opcode 0C
// |31               |25            |20            |15      |12   |10   |8       |5 |4            0|
// |-----------------|--------------|--------------|--------|-----|-----|--------|--|--------------|
// |       op        |       r1     |      r2      |  sop   | fmt |  2  |   0    |n |      c       |
// |-----------------|--------------|--------------|--------|-----|-----|--------|--|--------------|

// Format36:
//
// Floating-point Operation three, Major Opcode 0C
// |31               |25            |20            |15      |12   |10   |8       |5 |4            0|
// |-----------------|--------------|--------------|--------|-----|-----|--------|--|--------------|
// |       op        |       r1     |      r2      |  sop   | fmt |  3  |   0    |0 |      t       |
// |-----------------|--------------|--------------|--------|-----|-----|--------|--|--------------|

// Format37:
//
// Floating-point Operation Zero, Major Opcode 0E
// |31               |25            |20            |15      |12   |10   |8 |7 |6 |5 |4            0|
// |-----------------|--------------|--------------|--------|-----|-----|--|--|--|--|--------------|
// |       op        |       r1     |      r2      |  sop   | fmt |  0  |0 |r |t |0 |      t       |
// |-----------------|--------------|--------------|--------|-----|-----|--|--|--|--|--------------|

// Format38:
//
// Floating-point Operation One, Major Opcode 0E
// |31               |25            |20         |16   |14   |12   |10   |8 |7 |6 |5 |4            0|
// |-----------------|--------------|-----------|-----|-----|-----|-----|--|--|--|--|--------------|
// |       op        |       r      |     0     | sop | df  | sf  |  1  |0 |r |t |0 |      t       |
// |-----------------|--------------|-----------|-----|-----|-----|-----|--|--|--|--|--------------|

// Format39:
//
// Floating-point Operation Two, Major Opcode 0E
// |31               |25            |20            |15      |12|11|10   |8 |7 |6 |5 |4            0|
// |-----------------|--------------|--------------|--------|--|--|-----|--|--|--|--|--------------|
// |       op        |       r1     |      r2      |   sop  |r2|f |  2  |0 |r1|0 |0 |      c       |
// |-----------------|--------------|--------------|--------|--|--|-----|--|--|--|--|--------------|

// Format40:
//
// Floating-point Operation Three, Major Opcode 0E
// |31               |25            |20            |15      |12|11|10   |8 |7 |6 |5 |4            0|
// |-----------------|--------------|--------------|--------|--|--|-----|--|--|--|--|--------------|
// |       op        |       r1     |      r2      |   sop  |r2|f |  3  |x |r1|t |0 |      t       |
// |-----------------|--------------|--------------|--------|--|--|-----|--|--|--|--|--------------|
class HPPAFormat40 : public HPPAInstruction
{
protected:

public:
  HPPAFormat40(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags = 0) :
	HPPAInstruction(inPrimitive, inPool, 2, 1, kind, flags) {}

  virtual void formatToMemory(void* where, uint32 offset);
#if defined(DEBUG)
  virtual void printPretty(FILE* f);
#endif
};
typedef HPPAGeneric<HPPAFormat40,XMPYU> HPPAXmpyu;

// Format41:
//
// Floating-point Multiple-operation
// |31               |25            |20            |15            |10            |5 |4            0|
// |-----------------|--------------|--------------|--------------|--------------|--|--------------|
// |       op        |      rm1     |     rm2      |      ta      |      ra      |f |      tm      |
// |-----------------|--------------|--------------|--------------|--------------|--|--------------|


//------------------------------ Field Names ----------------------------------//
//                                                                             //
// a                  - modify before/after bit		  						   //
// b                  - base register										   //
// c                  - condition specifier									   //
// cc                 - cache control hint									   //
// clen               - 31 - extract/deposit length							   //
// cp                 - 31 - bit position									   //
// df                 - floating-point destination format					   //
// e or ext           - operation code extension							   //
// f                  - condition negation bit								   //
// f or fmt           - floating-point data format							   //
// im                 - immediate value										   //
// m                  - modify bit											   //
// n                  - nullify bit											   //
// op                 - operation code										   //
// p                  - extract/deposit/shift bit position					   //
// r, r1, or r2       - source register										   //
// ra, rm1, or rm2    - floating-point multiple-operation source register	   //
// rv                 - reserved instruction field							   //
// s                  - 2 or 3 bit space register							   //
// sf                 - floating-point source format						   //
// sfu                - special function unit number						   //
// sop, sop1, or sop2 - special function unit or coprocessor operation		   //
// t, ta, or tm       - target register										   //
// u                  - shift index bit										   //
// uid                - coprocessor unit identifier							   //
// w, w1, or w2       - word offset/word offset part						   //
// x                  - index register										   //
//                                                                             //
//-----------------------------------------------------------------------------//


#endif /* _HPPA_INSTRUCTION_H_ */
