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

#include "Fundamentals.h"
#include "prlog.h"
#include "prbit.h"

#include "HPPAEmitter.h"
#include "HPPAInstruction.h"
#include "HPPAMul.h"
#include "HPPAFuncs.h"
#include "ControlGraph.h"
#include "hppa.nad.burg.h"
#include "NativeCodeCache.h"

bool HPPAEmitter::initSpecialCallDone = false;

/*
 *-----------------------------------------------------------------------
 *
 * canUseDepi1 --
 *
 *  Return true if a DEPI can be used instead of an OR. If mask contains
 *  only one sequence of consecutives ONES then a depi can be emitted.
 *  e.g.: 0...01...10...0 | 0...01...1 | 1...10...0
 *
 *-----------------------------------------------------------------------
 */
inline static bool
canUseDepi1(PRUint32 mask)
{
  // If bits are consecutives mask & -mask will generate one bit at the first bit found.
  // By adding it to the consecutive bits it will generate one bit after the last found.
  mask += mask & -mask; 
  // Test that only one bit is set.
  return (mask & (mask - 1)) == 0;
}

/*
 *-----------------------------------------------------------------------
 *
 * canUseExtruOrDepi01 --
 *
 *  Return true if a DEPI or an EXTRU can be used instead of an AND. 
 *  If mask contains only one sequence of consecutives ZEROS then a depi 
 *  or an extru can be emitted.
 *  e.g.: 1...10...01...1 | 0...01...1 | 1...10...0
 *
 *-----------------------------------------------------------------------
 */
inline static bool
canUseExtruOrDepi0(PRUint32 mask)
{
  // Same as canUseDepi1, with ~mask.
  return canUseDepi1(~mask);
}

void HPPAEmitter::
emitPrimitive(Primitive& inPrimitive, NamedRule inRule) 
{
  switch (inRule)
	{
	case emConst_I:
	  emitConst(inPrimitive, (*static_cast<const PrimConst *>(&inPrimitive)).value);
	  break;
	case emArg_I:
	case emArg_L:
	case emArg_F:
	case emArg_D:
	case emArg_A:
	  PR_ASSERT(false);
	  break;
	case emResult_M:
	case emArg_M:
	  break;
	case emResult_I:
	  emitResult(inPrimitive);
	  break;
	case emIfLt:
	  emitCondBranch(inPrimitive, hcL);
	  break;
	case emIfULt:
	  emitCondBranch(inPrimitive, hcLu);
	  break;
	case emIfEq:
	case emIfUEq:
	  emitCondBranch(inPrimitive, hcE);
	  break;
	case emIfLe:
	  emitCondBranch(inPrimitive, hcLe);
	  break;
	case emIfULe:
	  emitCondBranch(inPrimitive, hcLeu);
	  break;
	case emIfGt:
	  emitCondBranch(inPrimitive, hcG);
	  break;
	case emIfUGt:
	  emitCondBranch(inPrimitive, hcGu);
	  break;
	case emIfLgt:
	case emIfNe:
	  emitCondBranch(inPrimitive, hcNe);
	  break;
	case emIfGe:
	  emitCondBranch(inPrimitive, hcGe);
	  break;
	case emIfUGe:
	  emitCondBranch(inPrimitive, hcGeu);
	  break;
	case emIfOrd:
	case emIfUnord:    
	  PR_ASSERT(false);  // Shouldn't happen with int edges...
	  break;
	case emOr_I:
	  emitInstruction(inPrimitive, OR);
	  break;
	case emOrI_I:
	  emitOrImm (inPrimitive, inPrimitive.nthInputConstant(1));
	  break;
	case emAnd_I:
	  emitInstruction(inPrimitive, AND);
	  break;
	case emAndI_I:
	  emitAndImm (inPrimitive, inPrimitive.nthInputConstant(1));
	  break;
	case emXor_I:
	  emitInstruction(inPrimitive, XOR);
	  break;
	case emXorI_I:
	  emitXorImm (inPrimitive, inPrimitive.nthInputConstant(1));
	  break;
	case emShAdd_IIndirect:
	  emitShAddIndirect(inPrimitive);
	  break;
	case emAdd_I:
	case emAdd_A:
	  emitInstruction(inPrimitive, ADDL);
	  break;
	case emAddI_I:
	case emAddI_A:
	  emitAddImm(inPrimitive, inPrimitive.nthInputConstant(1));
	  break;

	  UNIMP_CODE(BINARY_PRIMITIVE_SET(emMul, siSMul));
	case emDivI_I:
	  emitDivImm(inPrimitive, inPrimitive.nthInputConstant(1));
	  break;
	case emMulI_I:
	  emitMulImm(inPrimitive, inPrimitive.nthInputConstant(1));
	  break;
	case emModE_I:
	  emitSpecialCall(inPrimitive, RemI);
	  break;
	case emShlI_I:
	  emitShlImm(inPrimitive, inPrimitive.nthInputConstant(1));
	  break;
	case emChkNull:
	  emitChkNull(inPrimitive);
	  break;
	case emLimitR:
	  emitLimitR(inPrimitive);
	  break;
	case emLimit:
	  emitLimit(inPrimitive);
	  break;

	case emLdC_IRegisterIndirect:
	  emitLoad(inPrimitive, HPPA_REGISTER_INDIRECT | HPPA_LOAD_CONSTANT);
	  break;
	case emLd_IRegisterIndirect:
	  emitLoad(inPrimitive, HPPA_REGISTER_INDIRECT);
	  break;
	case emLdC_I:
	  emitLoad(inPrimitive, HPPA_LOAD_CONSTANT);
	  break;
	case emLd_I:
	  emitLoad(inPrimitive);
	  break;
	case emSt_I:
	  emitStore(inPrimitive);
	  break;
	case emStI_I:
	  emitStoreImm(inPrimitive);
	  break;
	case emStI_IRegisterIndirect:
	  emitStoreImm(inPrimitive, HPPA_REGISTER_INDIRECT);
	  break;
	case emSt_IRegisterIndirect:
	  emitStore(inPrimitive, HPPA_REGISTER_INDIRECT);
	  break;

	case emCallI_I:
	case emCallI_L:
	case emCallI_F:
	case emCallI_D:
	case emCallI_P:
	  UNIMP_CODE(emit_Call(inPrimitive));
	  break;

	default:
	  //PR_ASSERT(false)
	  break;
	}
}

Instruction& HPPAEmitter::
genInstruction(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, HPPAInstructionFlags flags)
{
  PR_ASSERT(HPPA_isValidInsn(kind));

  switch(hiInfo[kind].format)
	{
	case 1:
	  PR_ASSERT(hasIM14(flags));
	  return *new(inPool) HPPAFormat1(inPrimitive, inPool, kind, flags);

	case 6:
	  return *new(inPool) HPPAFormat6(inPrimitive, inPool, kind);

	default:
	  PR_ASSERT(false);
	}
}

void HPPAEmitter::
emitSpecialCall(Primitive& inPrimitive, HPPASpecialCallKind kind)
{
  Instruction& call = *new(mPool) HPPASpecialCall(&inPrimitive, mPool, kind, HPPA_NULLIFICATION);
  HPPASpecialCallInfo& callInfo = hscInfo[kind];

  for (PRUint8 i = 0; i < callInfo.nUses; i++)
	{
	  Instruction& copy = *new(mPool) HPPACopy(&inPrimitive, mPool);
	  useProducer(inPrimitive.nthInputVariable(i), copy, 0);
	  VirtualRegister& vReg = defineTemporary(copy, 0);
	  vReg.preColorRegister(registerNumberToColor[arg0 - i]);
	  useTemporaryVR(call, vReg, i);
	}
  for (PRUint8 j = 0; j < callInfo.nDefines; j++)
	{
	  Instruction& copy = *new(mPool) HPPACopy(&inPrimitive, mPool);
	  VirtualRegister& vReg = defineTemporary(call, j);
	  vReg.preColorRegister(registerNumberToColor[ret1 - j]);
	  useTemporaryVR(copy, vReg, 0);
	  defineProducer(nthOutputProducer(inPrimitive, j), copy, 0);
	}
}

void HPPAEmitter::
emitShAddIndirect(Primitive& inPrimitive)
{
  // Need to get immediate argument from poShl_I producer's primitive
  DataNode& immSource = inPrimitive.nthInput(1).getProducer().getNode();
  DataNode& shiftedProducer = immSource.nthInputVariable(0);
  DataNode& addedProducer = inPrimitive.nthInputVariable(0);
  DataNode& resultProducer = inPrimitive;
  PRUint8 shiftBy = immSource.nthInputConstant(1).i;

  if (shiftBy == 0)
	{
	  Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
	  useProducer(addedProducer, addl, 0);
	  useProducer(shiftedProducer, addl, 1);
	  defineProducer(resultProducer, addl, 0);
	}
  else if (shiftBy <= 3)
	{
	  Instruction& shiftAdd = genInstruction(&inPrimitive, mPool, shiftAddl[shiftBy]);
	  useProducer(shiftedProducer, shiftAdd, 0);
	  useProducer(addedProducer, shiftAdd, 1);
	  defineProducer(resultProducer, shiftAdd, 0);
	}
  else
	{
	  Instruction& shift = *new(mPool) HPPAZdep(&inPrimitive, mPool, 31 - shiftBy, 32 - shiftBy);
	  useProducer(shiftedProducer, shift, 0);
	  defineProducer(immSource, shift, 0);

	  Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
	  useProducer(addedProducer, addl, 0);
	  useProducer(immSource, addl, 1);
	  defineProducer(resultProducer, addl, 0);
	}
}

void HPPAEmitter::
emitStore(Primitive& inPrimitive, HPPAInstructionFlags flags)
{
  if (flags & HPPA_REGISTER_INDIRECT)
	{
	  // Need to get immediate argument from poAdd_A producer's primitive
	  DataNode& immSource = inPrimitive.nthInput(1).getProducer().getNode();
	  PRUint16  im14;
	  
	  if (extractIM14(immSource.nthInputConstant(1), im14))
		{
		  Instruction& store = *new(mPool) HPPAStore(&inPrimitive, mPool, STW, buildIM14(im14) | flags);
		  
		  useProducer(inPrimitive.nthInputVariable(0), store, 0);
		  useProducer(immSource.nthInputVariable(0), store, 1);
		  useProducer(inPrimitive.nthInputVariable(2), store, 2);
		  defineProducer(inPrimitive, store, 0);
		}
	  else
		{
		  emitAddImm(immSource, immSource.nthInputConstant(1));
		  emitStore(inPrimitive, flags & ~HPPA_REGISTER_INDIRECT);
		}
	}
  else
	{
	  Instruction& store = *new(mPool) HPPAStore(&inPrimitive, mPool, STW, buildIM14(0) | flags);
	  store.standardUseDefine(*this);
	}
}

void HPPAEmitter::
emitStoreToImm(Primitive& inPrimitive, HPPAInstructionFlags flags)
{
  PR_ASSERT((flags & HPPA_REGISTER_INDIRECT) == 0);
  Instruction& store = *new(mPool) HPPAStore(&inPrimitive, mPool, STW, buildIM14(0) | flags);
  VirtualRegister& constantVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(1));
  useProducer(inPrimitive.nthInputVariable(0), store, 0);
  useTemporaryVR(store, constantVR, 1);
  useProducer(inPrimitive.nthInputVariable(2), store, 2);
  defineProducer(inPrimitive, store, 0);
}

void HPPAEmitter::
emitStoreImm(Primitive& inPrimitive, HPPAInstructionFlags flags)
{
  if (flags & HPPA_REGISTER_INDIRECT)
	{
	  // Need to get immediate argument from poAdd_A producer's primitive
	  DataNode& immSource = inPrimitive.nthInput(1).getProducer().getNode();
	  PRUint16  im14;
	  
	  if (extractIM14(immSource.nthInputConstant(1), im14))
		{
		  Instruction& store = *new(mPool) HPPAStore(&inPrimitive, mPool, STW, buildIM14(im14) | flags);
		  VirtualRegister& constantVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(2));

		  useProducer(inPrimitive.nthInputVariable(0), store, 0);
		  useProducer(immSource.nthInputVariable(0), store, 1);
		  useTemporaryVR(store, constantVR, 2);
		  defineProducer(inPrimitive, store, 0);
		}
	  else
		{
		  emitAddImm(immSource, immSource.nthInputConstant(1));
		  emitStoreImm(inPrimitive, flags & ~HPPA_REGISTER_INDIRECT);
		}
	}
  else
	{
	  Instruction& store = *new(mPool) HPPAStore(&inPrimitive, mPool, STW, buildIM14(0) | flags);
	  VirtualRegister& constantVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(2));

	  useProducer(inPrimitive.nthInputVariable(0), store, 0);
	  useProducer(inPrimitive.nthInputVariable(1), store, 1);
	  useTemporaryVR(store, constantVR, 2);
	  defineProducer(inPrimitive, store, 0);
	}
}

void HPPAEmitter::
emitStoreImmToImm(Primitive& inPrimitive, HPPAInstructionFlags flags)
{
  PR_ASSERT((flags & HPPA_REGISTER_INDIRECT) == 0);
  Instruction& store = *new(mPool) HPPAStore(&inPrimitive, mPool, STW, buildIM14(0) | flags);
  VirtualRegister& addressVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(1));
  VirtualRegister& constantVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(2));
  useProducer(inPrimitive.nthInputVariable(0), store, 0);
  useTemporaryVR(store, addressVR, 1);
  useTemporaryVR(store, constantVR, 2);
  defineProducer(inPrimitive, store, 0);
}

void HPPAEmitter::
emitLoad(Primitive& inPrimitive, HPPAInstructionFlags flags)
{
  if (flags & HPPA_REGISTER_INDIRECT)
	{
	  // Need to get immediate argument from poAdd_A producer's primitive
	  DataNode& immSource = inPrimitive.nthInput((flags & HPPA_LOAD_CONSTANT) ? 0 : 1).getProducer().getNode();
	  PRUint16  im14;
	  
	  if (extractIM14(immSource.nthInputConstant(1), im14))
		{
		  Instruction& load = *new(mPool) HPPALoad(&inPrimitive, mPool, LDW, buildIM14(im14) | flags);
		  
		  useProducer(immSource.nthInputVariable(0), load, (flags & HPPA_LOAD_CONSTANT) ? 0 : 1);
		  if (!(flags & HPPA_LOAD_CONSTANT))
			useProducer(inPrimitive.nthInputVariable(0), load, 0);


		  defineProducer(inPrimitive, load, 0);
		  if (flags & HPPA_LOAD_VOLATILE)
			defineProducer(nthOutputProducer(inPrimitive, 1), load, 1);
		}
	  else
		{
		  emitAddImm(immSource, immSource.nthInputConstant(1));
		  emitLoad(inPrimitive, flags & ~HPPA_REGISTER_INDIRECT);
		}
	}
  else
	{
	  Instruction& load = *new(mPool) HPPALoad(&inPrimitive, mPool, LDW, buildIM14(0) | flags);
	  load.standardUseDefine(*this);
	}
}

void HPPAEmitter::
emitLoadFromImm(Primitive& inPrimitive, HPPAInstructionFlags flags)
{
  PR_ASSERT((flags & HPPA_REGISTER_INDIRECT) == 0);
  Instruction& load = *new(mPool) HPPALoad(&inPrimitive, mPool, LDW, buildIM14(0) | flags);
  VirtualRegister& constantVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant((flags & HPPA_LOAD_CONSTANT) ? 0 : 1));

  if (!(flags & HPPA_LOAD_CONSTANT))
	useProducer(inPrimitive.nthInputVariable(0), load, 0);
  useTemporaryVR(load, constantVR,(flags & HPPA_LOAD_CONSTANT) ? 0 : 1);

  defineProducer(inPrimitive, load, 0);
  if (flags & HPPA_LOAD_VOLATILE)
	defineProducer(nthOutputProducer(inPrimitive, 1), load, 1);
}

VirtualRegister& HPPAEmitter::
genLoadConstant(DataNode& inPrimitive, const Value& value, bool checkIM14)
{
  // It is possible to use the instruction zdepi to generate a constant.
  // also zvdepi might be used if the constant is shifted after.

  PRUint16 im14;
  if (checkIM14 && extractIM14(value, im14))
	{
	  Instruction& ldi = *new(mPool) HPPALdi(&inPrimitive, mPool, buildIM14(im14));
	  return defineTemporary(ldi, 0);
	}
  else
	{
	  Instruction& ldil = *new(mPool) HPPALdil(&inPrimitive, mPool, buildIM21(HPPA_LEFT(value.i)));
	  VirtualRegister& constantVR = defineTemporary(ldil, 0);
	  
	  if (HPPA_RIGHT(value.i) != 0)
		{
		  Instruction& ldo = *new(mPool) HPPALdo(&inPrimitive, mPool, buildIM14(HPPA_RIGHT(value.i)));
		  useTemporaryVR(ldo, constantVR, 0);
		  redefineTemporary(ldo, constantVR, 0);
		}
	  return constantVR;
	}
}

void HPPAEmitter::
emitLimitR(Primitive& inPrimitive)
{
  ControlNode& endNode = inPrimitive.getContainer()->controlGraph.getEndNode();
  PRUint8 constant;

  if (extractIM5(inPrimitive.nthInputConstant(0), constant))
	{
	  Instruction& compare = *new(mPool) HPPACondBranch(&inPrimitive, mPool, endNode, COMIBF, hcLe, buildIM5(constant) | HPPA_NULLIFICATION);
	  useProducer(inPrimitive.nthInputVariable(1), compare, 0);
	  inPrimitive.setInstructionRoot(&compare);
	}
  else
	{
	  Instruction& compare = *new(mPool) HPPACondBranch(&inPrimitive, mPool, endNode, COMBF, hcLe, HPPA_NULLIFICATION);
	  VirtualRegister& constantVR = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(0));
	  
	  useProducer(inPrimitive.nthInputVariable(1), compare, 0);
	  useTemporaryVR(compare, constantVR, 1);
	  inPrimitive.setInstructionRoot(&compare);
	}
}

void HPPAEmitter::
emitLimit(Primitive& inPrimitive)
{
  ControlNode& endNode = inPrimitive.getContainer()->controlGraph.getEndNode();
  Instruction& compare = *new(mPool) HPPACondBranch(&inPrimitive, mPool, endNode, COMBF, hcL, HPPA_NULLIFICATION);
  compare.standardUseDefine(*this);
}

void HPPAEmitter::
emitChkNull(Primitive& inPrimitive)
{
  ControlNode& endNode = inPrimitive.getContainer()->controlGraph.getEndNode();
  Instruction& compare = *new(mPool) HPPACondBranch(&inPrimitive, mPool, endNode, COMIBT, hcE, buildIM5(0) | HPPA_NULLIFICATION);
  compare.standardUseDefine(*this);
}

void HPPAEmitter::
emitResult(Primitive& inPrimitive)
{
  VirtualRegister*	returnVr;
  if (inPrimitive.nthInputIsConstant(0))
	{
	  returnVr = &genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(0));
	}
  else
	{
	  Instruction& copy = *new(mPool) HPPACopy(&inPrimitive, mPool);
	  returnVr = &defineTemporary(copy, 0);
	  useProducer(inPrimitive.nthInputVariable(0), copy, 0);
	}
		
  returnVr->preColorRegister(registerNumberToColor[ret0]);
		
  InsnExternalUse& externalUse = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);
  useTemporaryVR(externalUse, *returnVr, 0);
  inPrimitive.setInstructionRoot(&externalUse);
}

VirtualRegister& HPPAEmitter::
genShiftAdd(Primitive& inPrimitive, const PRUint8 shiftBy, VirtualRegister& shiftedReg, VirtualRegister& addedReg)
{
  // will emit (shiftedReg << shiftBy) + addedReg
  if (shiftBy == 0)
	{
	  Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
	  useTemporaryVR(addl, shiftedReg, 0);
	  useTemporaryVR(addl, addedReg, 1);
	  return defineTemporary(addl, 0);
	}
  else if (shiftBy <= 3)
	{
	  Instruction& shiftAdd = genInstruction(&inPrimitive, mPool, shiftAddl[shiftBy]);
	  useTemporaryVR(shiftAdd, shiftedReg, 0);
	  useTemporaryVR(shiftAdd, addedReg, 1);
	  return defineTemporary(shiftAdd, 0);
	}
  else
	{
	  Instruction& shift = *new(mPool) HPPAZdep(&inPrimitive, mPool, 31 - shiftBy, 32 - shiftBy);
	  useTemporaryVR(shift, shiftedReg, 0);
	  VirtualRegister& vReg = defineTemporary(shift, 0);
	  Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
	  useTemporaryVR(addl, vReg, 0);
	  useTemporaryVR(addl, addedReg, 1);
	  return defineTemporary(addl, 0);
	}
}

VirtualRegister& HPPAEmitter::
genShiftSub(Primitive& inPrimitive, const PRUint8 shiftBy, VirtualRegister& shiftedReg, VirtualRegister& subtractedReg, bool neg)
{
  // will emit (shiftedReg << shiftBy) - subtractedReg OR  subtractedReg - (shiftedReg << shiftBy) if neg is true.
  PRUint8 shiftedPos = neg ? 1 : 0;
  PRUint8 subPos =     neg ? 0 : 1;

  if (shiftBy == 0)
	{
	  Instruction& sub = *new(mPool) HPPASub(&inPrimitive, mPool);
	  useTemporaryVR(sub, shiftedReg, shiftedPos);
	  useTemporaryVR(sub, subtractedReg, subPos);
	  return defineTemporary(sub, 0);
	}
  else
	{
	  Instruction& shift = *new(mPool) HPPAZdep(&inPrimitive, mPool, 31 - shiftBy, 32 - shiftBy);
	  useTemporaryVR(shift, shiftedReg, 0);
	  VirtualRegister& vReg = defineTemporary(shift, 0);
	  Instruction& sub = *new(mPool) HPPASub(&inPrimitive, mPool);
	  useTemporaryVR(sub, vReg, shiftedPos);
	  useTemporaryVR(sub, subtractedReg, subPos);
	  return defineTemporary(sub, 0);
	}
}

void HPPAEmitter::
emitMulImm(Primitive& inPrimitive, const Value& value)
{
  enum MethodType
  {
	mRegular,
	mOpposite,
	mAddMultiplicand
  };

  MulAlgorithm     algorithm, tempAlgorithm;
  VirtualRegister* accum;
  VirtualRegister* multiplicand;
  MethodType       methodType = mRegular;
  PRUint8            mulCost = 30; // is it true for all the pa proc ?
  DEBUG_ONLY(PRInt32 val;)
  
  // Try the multiplication by value.i
  getMulAlgorithm(&algorithm, value.i, mulCost);

  // Try the multiplication by -value.i
  getMulAlgorithm(&tempAlgorithm, -value.i, mulCost - 4);
  if ((tempAlgorithm.cost + 4) < algorithm.cost)
	{
	  algorithm = tempAlgorithm;
	  algorithm.cost += 4; // cost for neg.
	  methodType = mOpposite; 
	}

  // Try the multiplication by value.i-1
  getMulAlgorithm(&tempAlgorithm, value.i - 1, mulCost);
  if ((tempAlgorithm.cost + 4) < algorithm.cost)
	{
	  algorithm = tempAlgorithm;
	  algorithm.cost += 4; // cost for ldo.
	  methodType = mAddMultiplicand; 
	}

  if (algorithm.cost < mulCost)
	{
	  // found a cheaper way to do the multiply.
	  if (algorithm.operations[0] == maZero)
		{
		  Instruction& ldi = *new(mPool) HPPALdi(&inPrimitive, mPool, buildIM14(0));
		  defineProducer(inPrimitive, ldi, 0);
		  return;
		}
	  else if (algorithm.operations[0] == maMultiplicand)
		{
		  Instruction& copy = *new(mPool) HPPACopy(&inPrimitive, mPool);
		  useProducer(inPrimitive.nthInputVariable(0), copy, 0);
		  accum = &defineTemporary(copy, 0);
		  DEBUG_ONLY(val = 1;)
		}
	  else
		{
		  PR_ASSERT(false); // there is an error in getMulAlgorithm.
		}

	  multiplicand = inPrimitive.nthInputVariable(0).getVirtualRegisterAnnotation();
	  for (PRUint8 n = 1; n < algorithm.nOperations; n++)
		{
		  PRUint8 shiftBy = algorithm.shiftAmount[n];

		  switch (algorithm.operations[n])
			{
			case maShiftValue:                    // accum = accum << shiftBy
			  {
				Instruction& zdep = *new(mPool) HPPAZdep(&inPrimitive, mPool, 31 - shiftBy, 32 - shiftBy);
				useTemporaryVR(zdep, *accum, 0);
				accum = &defineTemporary(zdep, 0);
				DEBUG_ONLY(val <<= shiftBy);
			  }
			  break;
			  
			case maAddValueToShiftMultiplicand:   // accum = accum + (multiplicand << shiftBy)
			  accum = &genShiftAdd(inPrimitive, shiftBy, *multiplicand, *accum);
			  DEBUG_ONLY(val += (1 << shiftBy);)
			  break;
			  
			case maSubShiftMultiplicandFromValue: // accum = accum - (multiplicand << shiftBy)
			  accum = &genShiftSub(inPrimitive, shiftBy, *multiplicand, *accum, true);
			  DEBUG_ONLY(val -= (1 << shiftBy);)
			  break;
			  
			case maAddValueToShiftValue:          // accum = (accum << shiftBy) + accum
			  accum = &genShiftAdd(inPrimitive, shiftBy, *accum, *accum);
			  DEBUG_ONLY(val += (val << shiftBy);)
			  break;
			  
			case maSubValueFromShiftValue:        // accum = (accum << shiftBy) - accum
			  accum = &genShiftSub(inPrimitive, shiftBy, *accum, *accum, false);
			  DEBUG_ONLY(val = (val << shiftBy) - val;)
			  break;
			  
			case maAddMultiplicandToShiftValue:   // accum = (accum << shiftBy) + multiplicand
			  accum = &genShiftAdd(inPrimitive, shiftBy, *accum, *multiplicand);
			  DEBUG_ONLY(val = (val << shiftBy) + 1;)
			  break;
			  
			case maSubMultiplicandFromShiftValue: // accum = (accum << shiftBy) - multiplicand
			  accum = &genShiftSub(inPrimitive, shiftBy, *accum, *multiplicand, false);
			  DEBUG_ONLY(val = (val << shiftBy) - 1;)
			  break;
			  
			default:
			  PR_ASSERT(false); // there is an error in getMulAlgorithm
			}
		}

	  switch (methodType)
		{
		case mRegular:
		  defineProducerWithExistingVirtualRegister(*accum, inPrimitive, 
													*accum->getDefiningInstruction(), 0);
		  break;
		case mOpposite:
		  {
			Instruction& neg = *new(mPool) HPPASub(&inPrimitive, mPool, HPPA_R1_IS_ZERO);
			useTemporaryVR(neg, *accum, 0);
			defineProducer(inPrimitive, neg, 0);
			DEBUG_ONLY(val = -val;)
		  }
		  break;
		case mAddMultiplicand:
		  {
			Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
			useTemporaryVR(addl, *accum, 0);
			useTemporaryVR(addl, *multiplicand, 1);
			defineProducer(inPrimitive, addl, 0);	
			DEBUG_ONLY(val = val + 1;)
		  }
		  break;
		}
	  DEBUG_ONLY(PR_ASSERT(val = value.i);)
	}
  else
	{
	  Instruction& xmpyu = *new(mPool) HPPAXmpyu(&inPrimitive, mPool);
	  VirtualRegister& constantVR = genLoadConstant(inPrimitive, value);

	  useProducer(inPrimitive.nthInputVariable(0), xmpyu, 0, vrcFixedPoint);
	  useTemporaryVR(xmpyu, constantVR, 1, vrcFixedPoint);
	  defineProducer(inPrimitive, xmpyu, 0, vrcFixedPoint);
	}
}

void HPPAEmitter::
emitDivImm(Primitive& inPrimitive, const Value& value)
{
  PRUint32 shift = value.i;
  if(shift != 0 && (shift & (shift - 1)) == 0)
	{
	  PRUint8 shiftBy;
	  PR_FLOOR_LOG2(shiftBy, shift);

	  Instruction& extru = *new(mPool) HPPAExtru(&inPrimitive, mPool, 0, 1);
	  useProducer(inPrimitive.nthInputVariable(0), extru, 0);
	  VirtualRegister& tempVR = defineTemporary(extru, 0);

	  Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
	  useProducer(inPrimitive.nthInputVariable(0), addl, 0);
	  useTemporaryVR(addl, tempVR, 1);
	  redefineTemporary(addl, tempVR, 0);

	  Instruction& extrs = *new(mPool) HPPAExtrs(&inPrimitive, mPool, 31 - shiftBy, 32 - shiftBy);
	  useTemporaryVR(extrs, tempVR, 0);
	  defineProducer(inPrimitive, extrs, 0);
	} 
  else
	{
	  PR_ASSERT(false); // FIXME
	}
}

void HPPAEmitter::
emitAddImm(DataNode& inPrimitive, const Value& value)
{
  PRUint16 im14;
  if(extractIM14(value, im14))
	{
	  Instruction& add = *new(mPool) HPPALdo(&inPrimitive, mPool, buildIM14(im14));
	  add.standardUseDefine(*this);
	}
  else
	{
	  Instruction& addl = *new(mPool) HPPAAddl(&inPrimitive, mPool);
	  VirtualRegister& constantVR = genLoadConstant(inPrimitive, value);

	  useProducer(inPrimitive.nthInputVariable(0), addl, 0);
	  useTemporaryVR(addl, constantVR, 1);
	  defineProducer(inPrimitive, addl, 0);	
	}
}
 
void HPPAEmitter::
emitConst(Primitive& inPrimitive, const Value& value)
{
  PRUint16 im14;
  if (extractIM14(value, im14))
	{
	  Instruction& ldi = *new(mPool) HPPALdi(&inPrimitive, mPool, buildIM14(im14));
	  ldi.standardUseDefine(*this);
	}
  else
	{
	  Instruction& ldil = *new(mPool) HPPALdil(&inPrimitive, mPool, buildIM21(HPPA_LEFT(value.i)));
	  VirtualRegister& vReg = *defineProducer(inPrimitive, ldil, 0);
	  
	  if (HPPA_RIGHT(value.i) != 0)
		{
		  Instruction& ldo = *new(mPool) HPPALdo(&inPrimitive, mPool, buildIM14(HPPA_RIGHT(value.i)));
		  useTemporaryVR(ldo, vReg, 0);
		  redefineTemporary(ldo, vReg, 0);
		}
	}
}

void HPPAEmitter::
emitShlImm(Primitive& inPrimitive, const Value& value)
{
  Instruction& zdep = *new(mPool) HPPAZdep(&inPrimitive, mPool, 31 - value.i, 32 - value.i);
  zdep.standardUseDefine(*this);
}

void HPPAEmitter::
emitAndImm(Primitive& inPrimitive, const Value& value)
{
  if (canUseExtruOrDepi0(value.i))
	{
	  PRUint32 mask = value.i;
	  PRUint8 ls0, ls1, ms0, p, len;
	  
	  // count the number of low ONES
	  for (ls0 = 0; ls0 < 32; ls0++)
		if ((mask & (1 << ls0)) == 0)
		  break;
	  
	  // count the number of middle ZEROS
	  for (ls1 = ls0; ls1 < 32; ls1++)
		if ((mask & (1 << ls1)) != 0)
		  break;
	  
	  // count then number of high ONES
	  for (ms0 = ls1; ms0 < 32; ms0++)
		if ((mask & (1 << ms0)) == 0)
		  break;
	  
	  // Be sure that we counted all the bits
	  PR_ASSERT(ms0 == 32);
	  
	  if (ls1 == 32)
		{
		  // The sequence is 0...01...1
		  len = ls0;
		  PR_ASSERT(len);
		  
		  Instruction& extru = *new(mPool) HPPAExtru(&inPrimitive, mPool, 31, len);
		  extru.standardUseDefine(*this);
		}
	  else
		{
		  // The sequence is 1...10...01...1 | 1...10...0
		  p = 31 - ls0;
		  len = ls1 - ls0;
		  
		  Instruction& copy = *new(mPool) HPPACopy(&inPrimitive, mPool);
		  useProducer(inPrimitive.nthInputVariable(0), copy, 0);
		  VirtualRegister& vReg = *defineProducer(inPrimitive, copy, 0);
		  
		  Instruction& depi = *new(mPool) HPPADepi(&inPrimitive, mPool, 31 - p, 32 - len, buildIM5(0));
		  useTemporaryVR(depi, vReg, 0);
		  redefineTemporary(depi, vReg, 0);
		}
	}
  else
	{
	  // No optimization is possible. Emit the regular AND with a load constant before.
	  Instruction& andInsn = *new(mPool) HPPAAnd(&inPrimitive, mPool);
	  VirtualRegister& constantVR = genLoadConstant(inPrimitive, value);

	  useProducer(inPrimitive.nthInputVariable(0), andInsn, 0);
	  useTemporaryVR(andInsn, constantVR, 1);
	  defineProducer(inPrimitive, andInsn, 0);	
	}
}

void HPPAEmitter::
emitXorImm(Primitive& inPrimitive, const Value& value)
{
  Instruction& xorInsn = *new(mPool) HPPAXor(&inPrimitive, mPool);
  VirtualRegister& constantVR = genLoadConstant(inPrimitive, value);
  
  useProducer(inPrimitive.nthInputVariable(0), xorInsn, 0);
  useTemporaryVR(xorInsn, constantVR, 1);
  defineProducer(inPrimitive, xorInsn, 0);	
}

void HPPAEmitter::
emitOrImm(Primitive& inPrimitive, const Value& value)
{
  if (canUseDepi1(value.i))
	{
	  // We can emit a depi instruction.
	  PRUint32 mask = value.i;
	  PRUint8 bs0, bs1, p, len;
	  
	  // Count the number of low ZEROS.
	  for (bs0 = 0; bs0 < 32; bs0++)
		if ((mask & (1 << bs0)) != 0)
		  break;

	  // Count the number of middle ONES
	  for (bs1 = bs0; bs1 < 32; bs1++)
		if ((mask & (1 << bs1)) == 0)
		  break;

	  // Be sure that we counted all bits in the sequence. x...x1...10...0 | x...x1...1
	  // where x...x must be 0...0
	  PR_ASSERT(((bs1 == 32) || (((PRUint32) 1 << bs1) > mask)));

	  p = 31 - bs0;
	  len = bs1 - bs0;	  

	  Instruction& copy = *new(mPool) HPPACopy(&inPrimitive, mPool);
	  useProducer(inPrimitive.nthInputVariable(0), copy, 0);
	  VirtualRegister& vReg = *defineProducer(inPrimitive, copy, 0);

	  Instruction& depi = *new(mPool) HPPADepi(&inPrimitive, mPool, 31 - p, 32 - len, buildIM5(-1));
	  useTemporaryVR(depi, vReg, 0);
	  redefineTemporary(depi, vReg, 0);
	}
  else
	{
	  // No optimization is possible. Emit the regular OR with a load constant before.
	  Instruction& orInsn = *new(mPool) HPPAOr(&inPrimitive, mPool);
	  VirtualRegister& constantVR = genLoadConstant(inPrimitive, value);

	  useProducer(inPrimitive.nthInputVariable(0), orInsn, 0);
	  useTemporaryVR(orInsn, constantVR, 1);
	  defineProducer(inPrimitive, orInsn, 0);	
	}
}

void HPPAEmitter::
emitInstruction(Primitive& inPrimitive, HPPAInstructionKind kind)
{
  Instruction& newInsn = genInstruction(&inPrimitive, mPool, kind);
  newInsn.standardUseDefine(*this);
}

void HPPAEmitter::
emitArguments(ControlBegin& inBeginNode)
{
  PRUint32 curParamWords;		// number of words of argument space used
  PRUint32 curFloatingPointArg;	// number of floating point arguments

  InsnExternalDefine& defineInsn = *new(mPool) InsnExternalDefine(&inBeginNode.arguments[0], mPool, inBeginNode.nArguments * 2);
	
  PRUint32 curArgIdx;	// current index into the arguments in the ControlBegin
	
  curParamWords = 0;
  curFloatingPointArg = 0;
  for (curArgIdx = 0; curArgIdx < inBeginNode.nArguments && curParamWords < 4; curArgIdx++)
	{
	  PrimArg&	curArg = inBeginNode.arguments[curArgIdx];

	  switch (curArg.getOperation())
		{
		case poArg_I:
		case poArg_A:
		  {
			Instruction& copyInsn = *new(mPool) HPPACopy(&curArg, mPool);

			// hook up this vr to the external define FIX-ME
			VirtualRegister& inputVr = defineTemporary(defineInsn, curArgIdx);
			inputVr.preColorRegister(registerNumberToColor[arg0-curParamWords]);

			// emit the copy
			useTemporaryVR(copyInsn, inputVr, 0);
			defineProducer(curArg, copyInsn, 0);
	
			curParamWords++;
			break;
		  }
		case poArg_L:
		  PR_ASSERT(false); // FIXME

		case poArg_F:
		  PR_ASSERT(false); // FIXME

		case poArg_D:
		  PR_ASSERT(false); // FIXME
		  
		  break;
		  
		case poArg_M:
		  break;

		default:
		  PR_ASSERT(false); // Should not happen
		  break;
		}
	}	

  // continue counting from before
  // make all the rest of the defines as type none
  for (;curArgIdx < inBeginNode.nArguments * 2; curArgIdx++)
	{
	  defineInsn.getInstructionDefineBegin()[curArgIdx].kind = udNone;
	}
  // check for half stack half reg case
}

void HPPAEmitter::
emitCondBranch(Primitive& inPrimitive, HPPAConditionKind kind)
{
  ControlNode& target = ControlIf::cast(*inPrimitive.getContainer())->getTrueIfEdge().target;
  DataNode& compare = inPrimitive.nthInput(0).getProducer().getNode(); // get the compare.
  PRUint8 constant;
  bool negate = hcInfo[kind].f;

  switch (compare.getOperation())
	{
	case poCmpU_I:
	case poCmpU_A:
	case poCmp_I:
	  {
		Instruction& cb = *new(mPool) HPPACondBranch(&inPrimitive, mPool, target, negate ? COMBF : COMBT, 
													 negate ? HPPAConditionKind(kind & 7) : kind, HPPA_NULLIFICATION);
		useProducer(compare.nthInputVariable(0), cb, 0);
		useProducer(compare.nthInputVariable(1), cb, 1);
		inPrimitive.setInstructionRoot(&cb);
	  }
	  break;

	case poCmpUI_I:
	case poCmpUI_A:
	case poCmpI_I:
	  if (extractIM5(compare.nthInputConstant(1), constant))
		{
		  kind = hcInfo[kind].swapped;
		  negate = hcInfo[kind].f;
		  Instruction& cb = *new(mPool) HPPACondBranch(&inPrimitive, mPool, target, negate ? COMIBF : COMIBT, 
													   negate ? HPPAConditionKind(kind & 7) : kind, buildIM5(constant) 
													   | HPPA_NULLIFICATION);
		  useProducer(compare.nthInputVariable(0), cb, 0);
		  inPrimitive.setInstructionRoot(&cb);
		}
	  else
		{
		  Instruction& cb = *new(mPool) HPPACondBranch(&inPrimitive, mPool, target, negate ? COMBF : COMBT, 
													   negate ? HPPAConditionKind(kind & 7) : kind, HPPA_NULLIFICATION);
		  VirtualRegister& constantVR = genLoadConstant(compare, compare.nthInputConstant(1));
		  useProducer(compare.nthInputVariable(0), cb, 0);
		  useTemporaryVR(cb, constantVR, 1);
		  inPrimitive.setInstructionRoot(&cb);
		}
	  break;

	case poCmp_L:
	case poCmpU_L:
	case poFCmp_F:
	case poFCmp_D:
	case poConst_C:
	default:
	  PR_ASSERT(false); // FIXME
	  break;
	}
}

bool HPPAEmitter::
emitCopyAfter(DataNode& inDataNode, SortedDoublyLinkedList<Instruction>::iterator where, VirtualRegister& fromVr, VirtualRegister& toVr)
{
#define COPY_KIND(fc, tc) ((((fc) & 0xff) << 8) | ((tc) & 0xff))

  PRUint32 copyKind = COPY_KIND(fromVr.getClass(),toVr.getClass());

  switch(copyKind)
	{
	case COPY_KIND(vrcInteger, vrcInteger):
	  {
		Instruction& copy = *new(mPool) HPPACopy(&inDataNode, mPool);
		copy.addUse(0, fromVr);
		copy.addDefine(0, toVr);
		toVr.setDefiningInstruction(copy);
		
		where->append(copy);
	  }
	  return false;

	case COPY_KIND(vrcInteger, vrcFixedPoint):
	  {
		Instruction& intStore = *new(mPool) HPPAStoreIntegerToConvertStackSlot(&inDataNode, mPool, fromVr);
		Instruction& fixedLoad = *new(mPool) HPPALoadFixedPointFromConvertStackSlot(&inDataNode, mPool, toVr);
		toVr.setDefiningInstruction(fixedLoad);
		// FIXME use & def

		where->append(intStore);
		intStore.append(fixedLoad);
	  }
	  return true;

	case COPY_KIND(vrcFixedPoint, vrcInteger):
	  {
		Instruction& fixedStore = *new(mPool) HPPAStoreFixedPointToConvertStackSlot(&inDataNode, mPool, fromVr);
		Instruction& intLoad = *new(mPool) HPPALoadIntegerFromConvertStackSlot(&inDataNode, mPool, toVr);
		toVr.setDefiningInstruction(intLoad);
		// FIXME use & def

		where->append(fixedStore);
		fixedStore.append(intLoad);
	  }
	  return true;

	default:
	  fprintf(stdout, "unhandled copy %d->%d\n", fromVr.getClass(), toVr.getClass());
	  return false;
	}
#undef COPY_KIND
}

Instruction& HPPAEmitter::
emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget)
{
  Instruction& branch = *new(mPool) HPPAAbsoluteBranch(&inDataNode, mPool, inTarget, BL, HPPA_NULLIFICATION);
  return branch;
}

void HPPAEmitter::
initSpecialCall()
{
#if defined(hppa)
  PRUint32* codeSpaceBase = HPPASpecialCodeBegin;
  PRUint32* codeSpaceLimit = HPPASpecialCodeEnd;
  PRUint32 specialCodeSize = (codeSpaceLimit - codeSpaceBase) * sizeof(PRUint32);

  PRUint32* specialCode = (PRUint32 *) NativeCodeCache::getCache().acquireMemory(specialCodeSize + 4);
  specialCode = (PRUint32 *) (((PRUint32) specialCode) + 3 & -4); // round up

  copy(codeSpaceBase, codeSpaceLimit, specialCode);
  
  for (PRUint8 i = 0; i < nSpecialCalls; i++)
	{
	  PRUint32 offset = ((PRUint32 *) hscInfo[i].address) - codeSpaceBase;
	  MethodDescriptor md(hscInfo[i].string);

	  NativeCodeCache::getCache().mapMemoryToMethod(md, (void *) &specialCode[offset]);
#ifdef DEBUG
	  hscInfo[i].address = (void *) &specialCode[offset];
#endif /* DEBUG */

	}

#endif /* defined(hppa) */
}

void HPPAEmitter::
formatPrologToMemory(void* inWhere)
{
  PRUint32* prolog = (PRUint32 *) inWhere;

  // 00: where + 00000008
  // 04: 00000000
  // 08: bl <+0x10>, %rp        0xe8400020
  // 0c: nop                    0x08000240
  // 10: ldw -18(%sr0,%sp),%rp  0x4bc23fd1
  // 14: ldsid (%sr0,%rp),%r1   0x004010a1
  // 18: mtsp %r1,%sr0          0x00011820
  // 1c: be,n 0(%sr0,%rp)       0xe0400002
  // 20: ..... func ....

  prolog[0] = ((PRUint32) prolog) + 8;
  prolog[1] = 0x00000000;
  prolog[2] = 0xe8400020;
  prolog[3] = 0x08000240;
  prolog[4] = 0x4bc23fd1;
  prolog[5] = 0x004010a1;
  prolog[6] = 0x00011820;
  prolog[7] = 0xe0400002;
}
