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

#ifndef _HPPA_EMITTER_H_
#define _HPPA_EMITTER_H_

#include "InstructionEmitter.h"
#include "HPPAInstruction.h"
#include "VirtualRegister.h"
#include "FastBitSet.h"

class PrimArg;
class Pool;

#define UNDEFINED_RULE(inRuleKind) \
	case inRuleKind: 	           \
      PR_ASSERT(false);	           \
    break;

#define UNIMP_CODE(a)

/*
 *-----------------------------------------------------------------------
 *
 * HPPA-1.1 Instruction Emitter.
 *
 *-----------------------------------------------------------------------
 */
class HPPAEmitter : public InstructionEmitter
{
protected:
  Instruction& genInstruction(DataNode* inPrimitive, Pool& inPool, HPPAInstructionKind kind, PRUint32 data = 0);

  void emitInstruction(Primitive& inPrimitive, HPPAInstructionKind kind);
  void emitCondBranch(Primitive& inPrimitive, HPPAConditionKind kind);
  void emitOrImm(Primitive& inPrimitive, const Value& value);
  void emitAndImm(Primitive& inPrimitive, const Value& value);
  void emitXorImm(Primitive& inPrimitive, const Value& value);
  void emitAddImm(DataNode& inPrimitive, const Value& value);
  void emitDivImm(Primitive& inPrimitive, const Value& value);
  void emitMulImm(Primitive& inPrimitive, const Value& value);
  void emitShlImm(Primitive& inPrimitive, const Value& value);
  void emitConst(Primitive& inPrimitive, const Value& value);
  void emitResult(Primitive& inPrimitive);
  void emitChkNull(Primitive& inPrimitive);
  void emitLimitR(Primitive& inPrimitive);
  void emitLimit(Primitive& inPrimitive);
  void emitLoad(Primitive& inPrimitive, HPPAInstructionFlags flags = HPPA_NONE);
  void emitLoadFromImm(Primitive& inPrimitive, HPPAInstructionFlags flags = HPPA_NONE);
  void emitStore(Primitive& inPrimitive, HPPAInstructionFlags flags = HPPA_NONE);
  void emitStoreImm(Primitive& inPrimitive, HPPAInstructionFlags flags = HPPA_NONE);
  void emitStoreToImm(Primitive& inPrimitive, HPPAInstructionFlags flags = HPPA_NONE);
  void emitStoreImmToImm(Primitive& inPrimitive, HPPAInstructionFlags flags = HPPA_NONE);
  void emitShAddIndirect(Primitive& inPrimitive);
  void emitSpecialCall(Primitive& inPrimitive, HPPASpecialCallKind kind);

  VirtualRegister& genLoadConstant(DataNode& inPrimitive, const Value& value, bool checkIM14 = true);
  VirtualRegister& genShiftAdd(Primitive& inPrimitive, const PRUint8 shiftBy, VirtualRegister& shiftedReg, VirtualRegister& addedReg);
  VirtualRegister& genShiftSub(Primitive& inPrimitive, const PRUint8 shiftBy, VirtualRegister& shiftedReg, VirtualRegister& subtractedReg, bool neg = false);

  static void initSpecialCall();
  static bool initSpecialCallDone;

public:
  HPPAEmitter(Pool& inPool, VirtualRegisterManager& vrMan) : InstructionEmitter(inPool, vrMan) 
	{
	  if (!initSpecialCallDone)
		{
		  initSpecialCallDone = true;
		  initSpecialCall();
		}
	}

  virtual void emitPrimitive(Primitive& inPrimitive, NamedRule inRule);

  void calculatePrologEpilog(Method& /*inMethod*/, uint32& outPrologSize, uint32& outEpilogSize) {outPrologSize = 32; outEpilogSize = 4;}
  virtual void formatPrologToMemory(void* inWhere);
  virtual void formatEpilogToMemory(void* inWhere) {*(PRUint32*)inWhere = 0xe840c002; /* bv,n 0(%rp) */}

  virtual void emitArgument(PrimArg& /*inArguments*/, const PRUint32 /*inArgumentIndex*/) {}
  virtual void emitArguments(ControlBegin& inBeginNode);
  virtual Instruction& emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget);
  virtual bool emitCopyAfter(DataNode& inDataNode, SortedDoublyLinkedList<Instruction>::iterator where, VirtualRegister& fromVr, VirtualRegister& toVr);
  virtual void emitLoadAfter(DataNode& /*inDataNode*/, SortedDoublyLinkedList<Instruction>::iterator /*where*/, VirtualRegister& /*loadedReg*/, VirtualRegister& /*stackReg*/) {}
  virtual void emitStoreAfter(DataNode& /*inDataNode*/, SortedDoublyLinkedList<Instruction>::iterator /*where*/, VirtualRegister& /*storedReg*/, VirtualRegister& /*stackReg*/) {}
};

#endif /* _HPPA_EMITTER_H_ */
