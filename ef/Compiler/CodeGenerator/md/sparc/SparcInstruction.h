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

#ifndef _SPARC_INSTRUCTION_H_
#define _SPARC_INSTRUCTION_H_

#include "Fundamentals.h"
#include "Instruction.h"

//
// Instruction format A (op=1): CALL
//
// |31   |29                                                                                      0|
// |-----------------------------------------------------------------------------------------------|
// | op  |                                       disp30                                            |
// |-----------------------------------------------------------------------------------------------|
//
//
// Instruction format B (op=0): SETHI & Branches
//
// |31   |29|28         |24      |21                                                              0|
// |-----------------------------------------------------------------------------------------------|
// | op  |       rd     |   op2  |                             imm22                               |
// |-----------------------------------------------------------------------------------------------|
// | op  |a |   cond    |   op2  |                             disp22                              |
// |-----------------------------------------------------------------------------------------------|
//
//
// Instruction format C (op=2|op=3): arithmetic, logical, shift, memory instructions & remaining.
//
// |31   |29            |24               |18            |13|12                     |4            0|
// |-----------------------------------------------------------------------------------------------|
// | op  |       rd     |        op3      |      rs1     |0 |           asi         |      rs2     |
// |-----------------------------------------------------------------------------------------------|
// | op  |       rd     |        op3      |      rs1     |1 |               simm13                 |
// |-----------------------------------------------------------------------------------------------|
// | op  |       rd     |        op3      |      rs1     |            opf           |      rs2     |
// |-----------------------------------------------------------------------------------------------|
//

enum SparcInstructionKind
{
  siAnd,
  siAndCC,
  siAndN,
  siAndNCC,
  siOr,
  siOrCC,
  siOrN,
  siOrNCC,
  siXor,
  siXorCC,
  siXNor,
  siXNorCC,
  siSll,
  siSrl,
  siSra,
  siAdd,
  siAddCC,
  siAddX,
  siAddXCC,
  siSub,
  siSubCC,
  siSubX,
  siSubXCC,
  siUMul,
  siUMulCC,
  siSMul,
  siSMulCC,
  siUDiv,
  siUDivCC,
  siSDiv,
  siSDivCC,
  siTrap,
  siLdSb,
  siLdSh,
  siLdUb,
  siLdUh,
  siLd,
  siLdd,
  siStSb,
  siStSh,
  siSt,
  siStd,
  siBranch,
  nSparcInstructionKind
};

enum SparcAccessKind
{
  saRRR, // insn r[s1],r[s2],r[d]
  saRIR, // insn r[s1],imm13,r[d]
  saRZR, // insn r[s1],r[g0],r[d]
  saZRR, // insn r[g0],r[s2],r[d]
  saZIR, // insn r[g0],imm13,r[d]
  saZZR, // insn r[g0],r[g0],r[d]
  saRRZ, // insn r[s1],r[s2],r[g0]
  saRIZ, // insn r[s1],imm13,r[g0]
  saRZZ, // insn r[s1],r[g0],r[g0]
  nSparcAccessKind
};

#define saRR saRRZ
#define saRI saRIZ
#define saRZ saRZZ

enum SparcConditionKind
{
  scA,   // always
  scN,   // never
  scNe,  // on Not Equal
  scE,   // on Equal
  scG,   // on Greater
  scLe,  // on Less or Equal
  scGe,  // on Greater or Equal
  scL,   // on Less
  scGu,  // on Greater Unsigned
  scLeu, // on Less or Equal Unsigned
  scCc,  // on Carry Clear (Greater than or Equal, Unsigned)
  scGeu,
  scCs,  // on Carry Set (Less than, Unsigned)
  scLu,
  scPos, // on Positive
  scNeg, // on Negative
  scVc,  // on Overflow Clear
  scVs,  // on Overflow Set
  nSparcConditionKind
};

enum SparcRegisterNumber
{
  g0, g1, g2, g3, g4, g5, g6, g7,
  o0, o1, o2, o3, o4, o5, sp, o7,
  l0, l1, l2, l3, l4, l5, l6, l7,
  i0, i1, i2, i3, i4, i5, fp, i7
};

struct SparcConditionInfo
{
  SparcConditionKind kind;
  uint8 cond;
  char *string;
};

struct SparcInstructionInfo
{
  SparcInstructionKind kind;
  uint8 op;
  uint8 op3;
  char* string;
};

struct SparcAccessInfo
{
  SparcAccessKind kind;
  uint8 nConsumer;
  uint8 nProducer;
};

extern SparcInstructionInfo siInfo[];
extern SparcAccessInfo saInfo[];
extern SparcConditionInfo scInfo[];
extern char* registerNumberToString[];
extern SparcRegisterNumber colorToRegisterNumber[];
extern uint8 registerNumberToColor[];

class SparcFormatBCond : public InsnUseXDefineYFromPool
{
protected:
  SparcInstructionKind kind;
  SparcConditionKind cond;
  bool a;
  ControlNode& target;

public:
  SparcFormatBCond(DataNode* inPrimitive, Pool& inPool, ControlNode& t, SparcInstructionKind iKind, SparcConditionKind cKind, bool annul) :
	InsnUseXDefineYFromPool(inPrimitive, inPool, 1, 0),
	kind(iKind), cond(cKind), a(annul), target(t) {}

  virtual void printPretty(FILE* f);
  virtual size_t getFormattedSize() {return (a ? 8 : 4);}
  virtual void formatToMemory(void* inStart, uint32 inOffset);
};


class SparcBranch : public SparcFormatBCond
{
public:
  SparcBranch(DataNode* inPrimitive, Pool& inPool, ControlNode& t, SparcConditionKind cKind, bool annul) :
	SparcFormatBCond(inPrimitive, inPool, t, siBranch, cKind, annul) {}
};

class SparcFormatC : public InsnUseXDefineYFromPool
{
protected:
  SparcInstructionKind kind;
  SparcAccessKind access;
  int16 imm13;

public:
  SparcFormatC(DataNode* inPrimitive, Pool& inPool, uint8 nConsumer = 0, uint8 nProducer = 0) : 
	InsnUseXDefineYFromPool(inPrimitive, inPool, nConsumer, nProducer) {}

  virtual void printPretty(FILE* f);
  virtual size_t getFormattedSize() {return 4;}		
  virtual void formatToMemory(void* inStart, uint32 /*inOffset*/);


  inline SparcRegisterNumber useToRegisterNumber(InstructionUse& use);
  inline SparcRegisterNumber defineToRegisterNumber(InstructionDefine& use);

  virtual uint8 getRS1();
  virtual uint8 getRS2();
  virtual uint8 getRD();
};

class SparcInsn : public SparcFormatC
{
public:
  SparcInsn(DataNode* inPrimitive, Pool& inPool, SparcInstructionKind iKind, SparcAccessKind aKind, int16 constant = 0) : 
	SparcFormatC(inPrimitive, inPool, saInfo[aKind].nConsumer, saInfo[aKind].nProducer)
	{kind = iKind; access = aKind; imm13 = constant;}

  virtual InstructionFlags getFlags() const {if (kind == siOr && access == saRZR) return (ifCopy); else return(ifNone);}
};

class SparcTrap : public SparcFormatC
{
protected:
  SparcConditionKind cond;
public:
  SparcTrap(DataNode* inPrimitive, Pool& inPool, SparcConditionKind cKind, SparcAccessKind aKind, int16 constant = 0) :
	SparcFormatC(inPrimitive, inPool, 1, 0), cond(cKind)
	{kind = siTrap; access = aKind; imm13 = constant;}
  virtual uint8 getRS1() {return g0;}
  virtual uint8 getRS2() {return g0;}
  virtual uint8 getRD() {return scInfo[cond].cond;}
  virtual void printPretty(FILE* f);
};

class SparcCmp : public SparcFormatC
{
public:
  SparcCmp(DataNode* inPrimitive, Pool& inPool, SparcAccessKind aKind, int16 constant = 0) : 
	SparcFormatC(inPrimitive, inPool, saInfo[aKind].nConsumer, 1)
	{kind = siSubCC; access = aKind; imm13 = constant;}
};

class SparcLoadStore : public SparcFormatC
{
protected:
  bool hasMemoryEdge;
  bool isLoad;
  bool isLoadC;

public:
  SparcLoadStore(DataNode* inPrimitive, Pool& inPool, SparcInstructionKind iKind, SparcAccessKind aKind, bool has_mem, bool is_load, bool is_loadC, int16 constant = 0) : 
	SparcFormatC(inPrimitive, inPool, is_load ? (is_loadC ? 1 : 2) : 3, (is_load && has_mem) ? 2 : 1), hasMemoryEdge(has_mem), isLoad(is_load), isLoadC(is_loadC)
	{
	  kind = iKind; access = aKind; imm13 = constant;
	}
  virtual void printPretty(FILE* f);
  virtual uint8 getRS1();
  virtual uint8 getRS2();
  virtual uint8 getRD();
};

class SparcLoadC : public SparcLoadStore
{
public:
  SparcLoadC(DataNode* inPrimitive, Pool& inPool, SparcInstructionKind iKind, SparcAccessKind aKind, int16 constant = 0) : 
	SparcLoadStore(inPrimitive, inPool, iKind, aKind, false, true, true, constant) {}
};

class SparcLoad : public SparcLoadStore
{
public:
  SparcLoad(DataNode* inPrimitive, Pool& inPool, SparcInstructionKind iKind, SparcAccessKind aKind, bool memEdge, int16 constant = 0) : 
	SparcLoadStore(inPrimitive, inPool, iKind, aKind, memEdge, true, false, constant) {}
};

class SparcStore : public SparcLoadStore
{
public:
  SparcStore(DataNode* inPrimitive, Pool& inPool, SparcInstructionKind iKind, SparcAccessKind aKind, int16 constant = 0) : 
	SparcLoadStore(inPrimitive, inPool, iKind, aKind, true, false, false, constant) {}
};

inline SparcRegisterNumber SparcFormatC::
useToRegisterNumber(InstructionUse& use)
{
  assert(use.isVirtualRegister());
  return (colorToRegisterNumber[use.getVirtualRegister().getColor()]);
}

inline SparcRegisterNumber SparcFormatC::
defineToRegisterNumber(InstructionDefine& def)
{
  assert(def.isVirtualRegister());
  return (colorToRegisterNumber[def.getVirtualRegister().getColor()]);
}

#endif /* _SPARC_INSTRUCTION_H_ */
