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

#include "SparcSunOSEmitter.h"
#include "SparcInstruction.h"
#include "ControlNodes.h"
#include "sparc-sunos.nad.burg.h"

#define BINARY_PRIMITIVE_I(inRuleKind, inOp) \
	case inRuleKind: 	                     \
	  emit_Insn_I(inPrimitive, inOp);	     \
    break;
		
#define BINARY_PRIMITIVEI_I(inRuleKind, inOp) \
	case inRuleKind: 	                      \
	  emit_InsnI_I(inPrimitive, inOp);	      \
    break;
		
#define UNDEFINED_RULE(inRuleKind) \
	case inRuleKind: 	           \
      assert(false);	           \
    break;

#define BINARY_PRIMITIVE_SET(inRuleRoot, inOp)  \
    BINARY_PRIMITIVE_I(inRuleRoot##_I, inOp);   \
	UNDEFINED_RULE(inRuleRoot##_L);	            \
	BINARY_PRIMITIVEI_I(inRuleRoot##I_I, inOp); \
	UNDEFINED_RULE(inRuleRoot##I_L); 

void SparcSunOSEmitter::
emitPrimitive(Primitive& inPrimitive, NamedRule inRule) 
{
  switch (inRule)
	{
	case emConst_I:
	  emit_Const_I(inPrimitive);
	  break;
	case emArg_I:
	case emArg_L:
	case emArg_F:
	case emArg_D:
	case emArg_A:
	  assert(false);
	  break;
	case emResult_M:
	case emArg_M:
	  break;
	case emResult_I:
	  {
		VirtualRegister*	returnVr;
		if (inPrimitive.nthInputIsConstant(0))
		  {
			returnVr = &genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(0).i);
		  }
		else
		  {
			SparcInsn& copyInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siOr, saRZR);
			
			returnVr = &defineTemporary(copyInsn, 0);
			useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0);
		  }
		
		returnVr->precolorRegister(registerNumberToColor[o0]);
				
		InsnExternalUse& externalUse = *new(mPool) InsnExternalUse(&inPrimitive);
		useTemporaryVR(externalUse, *returnVr, 0);
		inPrimitive.setInstructionRoot(&externalUse);
	  }
	  break;
	case emIfLt:
	  emit_B(inPrimitive, scL);
	  break;
	case emIfULt:
	  emit_B(inPrimitive, scLu);
	  break;
	case emIfEq:
	case emIfUEq:
	  emit_B(inPrimitive, scE);
	  break;
	case emIfLe:
	  emit_B(inPrimitive, scLe);
	  break;
	case emIfULe:
	  emit_B(inPrimitive, scLeu);
	  break;
	case emIfGt:
	  emit_B(inPrimitive, scG);
	  break;
	case emIfUGt:
	  emit_B(inPrimitive, scGu);
	  break;
	case emIfLgt:
	case emIfNe:
	  emit_B(inPrimitive, scNe);
	  break;
	case emIfGe:
	  emit_B(inPrimitive, scGe);
	  break;
	case emIfUGe:
	  emit_B(inPrimitive, scGeu);
	  break;
	case emIfOrd:
	case emIfUnord:    
	  assert(false);  // Shouldn't happen with int edges...
	  break;
	BINARY_PRIMITIVE_SET(emAnd, siAnd);
	BINARY_PRIMITIVE_SET(emOr,  siOr );
	BINARY_PRIMITIVE_SET(emXor, siXor);
	BINARY_PRIMITIVE_SET(emAdd, siAdd);
	case emAdd_A:
	  emit_Insn_I(inPrimitive, siAdd);
	  break;
	case emAddI_A:
	  emit_InsnI_I(inPrimitive, siAdd);
	  break;
	BINARY_PRIMITIVE_SET(emMul, siSMul);
	case emDivI_I:
	  emit_DivI_I(inPrimitive);
	  break;
	case emModE_I:
	  emit_ModE_I(inPrimitive);
	  break;
	case emShlI_I:
	  emit_InsnI_I(inPrimitive, siSll);
	  break;
	case emChkNull:
	  emit_ChkNull(inPrimitive);
	  break;
	case emLimitR:
	  emit_LimitR(inPrimitive);
	  break;
	case emLimit:
	  emit_Limit(inPrimitive);
	  break;
	case emLdC_IRegisterIndirect:
	  emit_LdC_IRegisterIndirect(inPrimitive);
	  break;
	case emLd_IRegisterIndirect:
	  emit_Ld_IRegisterIndirect(inPrimitive);
	  break;
	case emLdC_I:
	  fprintf(stdout, "*** should emit emLdC_I\n");
	  //genLoadIZero(inPrimitive, pfConstant);
	  break;
	case emLd_I:
	  fprintf(stdout, "*** should emit emLd_I\n");
	  //genLoadIZero(inPrimitive, 0);
	  break;
	case emSt_I:
	  fprintf(stdout, "*** should emit emSt_I\n");
	  //{
	  //StoreNoUpdateD& newInsn = *new(mPool) StoreNoUpdateD(&inPrimitive, mPool, dfStw, 0, 0); 
	  //newInsn.standardUseDefine(*this);
	  //}	
	  break;
	case emStI_I:
	  fprintf(stdout, "*** should emit emStI_I\n");
	  //emit_StI_I(inPrimitive);
	  break;
	case emStI_IRegisterIndirect:
	  emit_StI_IRegisterIndirect(inPrimitive);
	  break;
	case emSt_IRegisterIndirect:
	  emit_St_IRegisterIndirect(inPrimitive);
	  break;
	case emCallI_I:
	case emCallI_L:
	case emCallI_F:
	case emCallI_D:
	case emCallI_P:
	  fprintf(stdout, "*** should emit emCallI_P\n");
	  //emit_Call(inPrimitive);
	  break;
	case emCmp_I:
	  emit_Cmp_I(inPrimitive);
	  break;
	case emCmpI_I:
	  emit_CmpI_I(inPrimitive);
	  break;
	default:
	  //assert(false)
	  break;
	}
}

void SparcSunOSEmitter::
emit_Const_I(Primitive& inPrimitive)
{
  int32 constI = (*static_cast<const PrimConst *>(&inPrimitive)).value.i;
  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcInsn& newInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siOr, constI ? saZIR : saZZR, constI & 0x1fff);
	  defineProducer(inPrimitive, newInsn, 0);
	}
  else if ((constI & 0x1fff) == 0)
	{
	  // FIXME
	  assert(0);
	}
  else
	{
	  // FIXME
	  assert(0);
	}
}

void SparcSunOSEmitter::
emit_InsnI_I(Primitive& inPrimitive, SparcInstructionKind kind)
{
  int32 constI = inPrimitive.nthInputConstant(1).i;
  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcInsn& newInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, kind, constI ? saRIR : saRZR, constI & 0x1fff);
	  newInsn.standardUseDefine(*this);
	}
  else
	{
	  assert(0);
	}
}

void SparcSunOSEmitter::
emit_Insn_I(Primitive& inPrimitive, SparcInstructionKind kind)
{
  SparcInsn& newInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, kind, saRRR);
  newInsn.standardUseDefine(*this);
}

void SparcSunOSEmitter::
emit_Cmp_I(Primitive& inPrimitive)
{
  SparcCmp& newInsn = *new(mPool) SparcCmp(&inPrimitive, mPool, saRR);
  newInsn.standardUseDefine(*this);
}

void SparcSunOSEmitter::
emit_CmpI_I(Primitive& inPrimitive)
{
  int32 constI = inPrimitive.nthInputConstant(1).i;
  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcCmp& newInsn = *new(mPool) SparcCmp(&inPrimitive, mPool, constI ? saRI : saRZ, constI & 0x1fff);
	  newInsn.standardUseDefine(*this);
	}
  else
	{
	  assert(0);
	}
}

void SparcSunOSEmitter::
emit_B(Primitive& inPrimitive, SparcConditionKind cond)
{
  ControlIf& controlIf = ControlIf::cast(*inPrimitive.getContainer());
  SparcBranch& newInsn = *new(mPool) SparcBranch(&inPrimitive, mPool, *(controlIf.getTrueIfEdge().target), cond, true);
  newInsn.standardUseDefine(*this);
}

Instruction& SparcSunOSEmitter::
emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget)
{
  SparcBranch& newInsn = *new(mPool) SparcBranch(&inDataNode, mPool, inTarget, scA, true);
  return (newInsn);
}

void SparcSunOSEmitter::
emitArguments(ControlBegin& inBeginNode)
{
  uint32 curParamWords;			// number of words of argument space used
  uint32 curFloatingPointArg;	// number of floating point arguments

  InsnExternalDefine& defineInsn = *new(mPool) InsnExternalDefine(&inBeginNode.arguments[0], mPool, inBeginNode.nArguments * 2);
	
  uint32 curArgIdx;	// current index into the arguments in the ControlBegin
	
  curParamWords = 0;
  curFloatingPointArg = 0;
  for (curArgIdx = 0; curArgIdx < inBeginNode.nArguments && curParamWords < 6; curArgIdx++)
	{
	  PrimArg&	curArg = inBeginNode.arguments[curArgIdx];

	  switch (curArg.getOperation())
		{
		case poArg_I:
		case poArg_A:
		  {
			SparcInsn& copyInsn = *new(mPool) SparcInsn(&curArg, mPool, siOr, saRZR);

			// hook up this vr to the external define FIX-ME
			VirtualRegister& inputVr = defineTemporary(defineInsn, curArgIdx);
			inputVr.precolorRegister(registerNumberToColor[i0+curParamWords]);

			// emit the copy
			useTemporaryVR(copyInsn, inputVr, 0);
			defineProducer(curArg, copyInsn, 0);
	
			curParamWords++;
			break;
		  }
		case poArg_L:
		  assert(false);

		case poArg_F:
		  assert(false);

		case poArg_D:
		  assert(false);
		  
		  break;
		  
		case poArg_M:
		  break;

		default:
		  assert(false);
		  break;
		}
	}	

  // FIXME: emit the load for the arguments on the stack

  // continue counting from before
  // make all the rest of the defines as type none
  for (;curArgIdx < inBeginNode.nArguments * 2; curArgIdx++)
	{
	  defineInsn.getInstructionDefineBegin()[curArgIdx].kind = udNone;
	}
  // check for half stack half reg case
}

Instruction& SparcSunOSEmitter::
emitCopy(DataNode& inDataNode, VirtualRegister& fromVr, VirtualRegister& toVr)
{
  SparcInsn& newInsn = *new(mPool) SparcInsn(&inDataNode, mPool, siOr, saRZR);
  newInsn.addUse(0, fromVr);
  newInsn.addDefine(0, toVr);
#if DEBUG
  toVr.setDefineInstruction(newInsn);
#endif
  return (newInsn);
}

void SparcSunOSEmitter::
emit_DivI_I(Primitive& inPrimitive)
{
  Value& immediate = inPrimitive.nthInputConstant(1);
  if(isPowerOfTwo(immediate.i))
	{
	  uint8 shiftBy = 31 - leadingZeros(immediate.i);

	  SparcInsn& shiftRightL = *new(mPool) SparcInsn(&inPrimitive, mPool, siSrl, saRIR, 31);
	  useProducer(inPrimitive.nthInputVariable(0), shiftRightL, 0);
	  VirtualRegister& tempVr = defineTemporary(shiftRightL, 0);
	  inPrimitive.setInstructionRoot(&shiftRightL);

	  SparcInsn& addInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siAdd, saRRR);
	  useProducer(inPrimitive.nthInputVariable(0), addInsn, 0);
	  useTemporaryVR(addInsn, tempVr, 1);
	  redefineTemporary(addInsn, tempVr, 0);

	  SparcInsn& shiftRight = *new(mPool) SparcInsn(&inPrimitive, mPool, siSra, saRIR, shiftBy);
	  useTemporaryVR(shiftRight, tempVr, 0);
	  defineProducer(inPrimitive, shiftRight, 0);	
	} 
  else 
	// FIXME: check 0
	emit_InsnI_I(inPrimitive, siSDiv);
}

void SparcSunOSEmitter::
emit_ModE_I(Primitive& inPrimitive)
{
  // Check the divisor != 0
  SparcCmp& cmpInsn = *new(mPool) SparcCmp(&inPrimitive, mPool, saRZ);
  useProducer(inPrimitive.nthInputVariable(1), cmpInsn, 0);
  cmpInsn.addDefine(0, udCond);
  SparcTrap& trapInsn = *new(mPool) SparcTrap(&inPrimitive, mPool, scE, saRRR /* FIXME */);
  trapInsn.addUse(0, udCond);
  trapInsn.getInstructionUseBegin()->setDefiningInstruction(cmpInsn);
  inPrimitive.setInstructionRoot(&trapInsn);

  SparcInsn& divInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siSDiv, saRRR);
  useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
  useProducer(inPrimitive.nthInputVariable(1), divInsn, 1);
  VirtualRegister& tempVr = defineTemporary(divInsn, 0);

  SparcInsn& mullInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siSMul, saRRR);
  useProducer(inPrimitive.nthInputVariable(1), mullInsn, 1);
  useTemporaryVR(mullInsn, tempVr, 0);
  redefineTemporary(mullInsn, tempVr, 0);
	
  SparcInsn& subInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siSub, saRRR);
  useTemporaryVR(subInsn, tempVr, 0);
  useProducer(inPrimitive.nthInputVariable(0), subInsn, 1);
  defineProducer(inPrimitive, subInsn, 0);	
}

void SparcSunOSEmitter::
emit_ChkNull(Primitive& inPrimitive)
{
  SparcCmp& cmpInsn = *new(mPool) SparcCmp(&inPrimitive, mPool, saRZ);
  useProducer(inPrimitive.nthInputVariable(0), cmpInsn, 0);
  cmpInsn.addDefine(0, udCond);

  SparcTrap& trapInsn = *new(mPool) SparcTrap(&inPrimitive, mPool, scE, saRRR /* FIXME */);
  trapInsn.addUse(0, udCond);
  trapInsn.getInstructionUseBegin()->setDefiningInstruction(cmpInsn);
  inPrimitive.setInstructionRoot(&trapInsn);
}

void SparcSunOSEmitter::
emit_LimitR(Primitive& inPrimitive)
{
  int32 constI = inPrimitive.nthInputConstant(0).i;
  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcCmp& cmpInsn = *new(mPool) SparcCmp(&inPrimitive, mPool, constI ? saRI : saRZ, constI & 0x1fff);
	  useProducer(inPrimitive.nthInputVariable(1), cmpInsn, 0);
	  cmpInsn.addDefine(0, udCond);
	  
	  SparcTrap& trapInsn = *new(mPool) SparcTrap(&inPrimitive, mPool, scLe, saRRR /* FIXME */);
	  trapInsn.addUse(0, udCond);
	  trapInsn.getInstructionUseBegin()->setDefiningInstruction(cmpInsn);
	  inPrimitive.setInstructionRoot(&trapInsn);
	}
  else
	{
	  assert(0);
	}
}

void SparcSunOSEmitter::
emit_Limit(Primitive& inPrimitive)
{
  SparcCmp& cmpInsn = *new(mPool) SparcCmp(&inPrimitive, mPool, saRR);
  useProducer(inPrimitive.nthInputVariable(0), cmpInsn, 0);
  useProducer(inPrimitive.nthInputVariable(1), cmpInsn, 1);
  cmpInsn.addDefine(0, udCond);

  SparcTrap& trapInsn = *new(mPool) SparcTrap(&inPrimitive, mPool, scGe, saRRR /* FIXME */);
  trapInsn.addUse(0, udCond);
  trapInsn.getInstructionUseBegin()->setDefiningInstruction(cmpInsn);
  inPrimitive.setInstructionRoot(&trapInsn);
}


void SparcSunOSEmitter::
emit_LdC_IRegisterIndirect(Primitive& inPrimitive)
{		
  DataNode& immSource = inPrimitive.nthInput(0).getProducer().getNode();

  int32 constI = immSource.nthInputConstant(1).i;
  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcLoadC& newInsn = *new(mPool) SparcLoadC(&inPrimitive, mPool, siLd, saRIR, constI);
	  useProducer(immSource.nthInputVariable(0), newInsn, 0);
	  defineProducer(inPrimitive, newInsn, 0);	
	}
  else
	{
	  assert(false);
	}
}

void SparcSunOSEmitter::
emit_Ld_IRegisterIndirect(Primitive& inPrimitive)
{		
  DataNode& immSource = inPrimitive.nthInput(1).getProducer().getNode();

  int32 constI = immSource.nthInputConstant(1).i;

  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcLoad& newInsn = *new(mPool) SparcLoad(&inPrimitive, mPool, siLd, saRIR, false, constI);
	  useProducer(inPrimitive.nthInputVariable(0), newInsn, 0);		// M to poLd_I
	  useProducer(immSource.nthInputVariable(0), newInsn, 1);			// Vint to poAdd_A
	  defineProducer(inPrimitive, newInsn, 0);	// Vint from poLd_I
	}
  else
	{
	  assert(false);
	}
}

void SparcSunOSEmitter::
emit_StI_IRegisterIndirect(Primitive& inPrimitive)
{
  SparcStore* storeInsn;

  // load the constant to be stored into constVr
  VirtualRegister& constVr = genLoadConstant(inPrimitive, inPrimitive.nthInputConstant(2).i);
		
  // grab the immediate value from the incoming poAddA_I
  DataNode& addrSource = inPrimitive.nthInput(1).getProducer().getNode();

  int32 offset = addrSource.nthInputConstant(1).i;
  if ((offset >= -4096) && (offset <= 4095))
	{
	  storeInsn = new(mPool) SparcStore(&inPrimitive, mPool, siSt, saRIR, offset);
	  useProducer(addrSource.nthInputVariable(0), *storeInsn, 1);
	}
  else
	{
	  assert(false);
	}
  useProducer(inPrimitive.nthInputVariable(0), *storeInsn, 0);		// <- store
  useTemporaryVR(*storeInsn, constVr, 2);							// <- value
  defineProducer(inPrimitive, *storeInsn, 0); // -> store
}

void SparcSunOSEmitter::
emit_St_IRegisterIndirect(Primitive& inPrimitive)
{
  DataNode& addrSource = inPrimitive.nthInput(1).getProducer().getNode();

  int32 offset = addrSource.nthInputConstant(1).i;
  if ((offset >= -4096) && (offset <= 4095))
	{
	  SparcStore& storeInsn = *new(mPool) SparcStore(&inPrimitive, mPool, siSt, saRIR, offset); 
	  useProducer(inPrimitive.nthInputVariable(0), storeInsn, 0);		// <- store
	  useProducer(addrSource.nthInputVariable(0), storeInsn, 1);		// <- address base
	  useProducer(inPrimitive.nthInputVariable(2), storeInsn, 2);		// <- value 
	  defineProducer(inPrimitive, storeInsn, 0);  // -> store
	}
  else
	{
	  assert(false);
	}
}


VirtualRegister& SparcSunOSEmitter::
genLoadConstant(DataNode& inPrimitive, int32 constI)
{
  if ((constI >= -4096) && (constI <= 4095))
	{
	  SparcInsn& newInsn = *new(mPool) SparcInsn(&inPrimitive, mPool, siOr, constI ? saZIR : saZZR, constI & 0x1fff);
	  VirtualRegister& constantVr = defineTemporary(newInsn, 0);
	  return constantVr;
	}
  else if ((constI & 0x1fff) == 0)
	{
	  // FIXME
	  assert(0);
	}
  else
	{
	  // FIXME
	  assert(0);
	}
}

uint32 SparcSunOSEmitter::
getPrologSize() const 
{
  return 4;
}

void SparcSunOSEmitter::
formatPrologToMemory(void* inWhere) 
{
  *(uint32*) inWhere = 0x9de3bf90; // save %sp,-112,%sp
}

uint32 SparcSunOSEmitter::
getEpilogSize() const 
{
  return 8;
}

void SparcSunOSEmitter::
formatEpilogToMemory(void* inWhere) 
{
  ((uint32*) inWhere)[0] = 0x81c7e008; // ret
  ((uint32*) inWhere)[1] = 0x81e80000; // restore %g0,%g0,%g0
}
