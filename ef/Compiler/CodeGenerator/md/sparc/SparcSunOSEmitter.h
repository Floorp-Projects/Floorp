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

#ifndef _SPARC_SUNOS_EMITTER_H_
#define _SPARC_SUNOS_EMITTER_H_

#include "InstructionEmitter.h"
#include "SparcInstruction.h"
#include "VirtualRegister.h"

class PrimArg;
class Pool;

class SparcSunOSEmitter : public InstructionEmitter
{
public:
  SparcSunOSEmitter(Pool& inPool, VirtualRegisterManager& vrMan) : InstructionEmitter(inPool, vrMan) {}
  
  virtual void emitPrimitive(Primitive& inPrimitive, NamedRule inRule);

  virtual uint32 getPrologSize() const;
  virtual void formatPrologToMemory(void* inWhere);
  virtual uint32 getEpilogSize() const;
  virtual void formatEpilogToMemory(void* inWhere);

  virtual void emitArguments(ControlBegin& inBeginNode);
  virtual void emitArgument(PrimArg& /*inArguments*/, const uint /*inArgumentIndex*/) {}
  virtual Instruction& emitCopy(DataNode& inDataNode, VirtualRegister& fromVr, VirtualRegister& toVr);
  virtual Instruction& emitLoad(DataNode& /*inDataNode*/, VirtualRegister& /*loadedRegister*/, VirtualRegister& /*stackRegister*/) {}
  virtual Instruction& emitStore(DataNode& /*inDataNode*/, VirtualRegister& /*storedRegister*/, VirtualRegister& /*stackRegister*/) {};
  virtual Instruction& emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget);

  void emit_Insn_I(Primitive& inPrimitive, SparcInstructionKind kind);
  void emit_InsnI_I(Primitive& inPrimitive, SparcInstructionKind kind);
  void emit_Const_I(Primitive& inPrimitive);
  void emit_Cmp_I(Primitive& inPrimitive);
  void emit_CmpI_I(Primitive& inPrimitive);
  void emit_B(Primitive& inPrimitive, SparcConditionKind kind);
  void emit_ModE_I(Primitive& inPrimitive);
  void emit_DivI_I(Primitive& inPrimitive);
  void emit_ChkNull(Primitive& inPrimitive);
  void emit_LimitR(Primitive& inPrimitive);
  void emit_Limit(Primitive& inPrimitive);
  void emit_LdC_IRegisterIndirect(Primitive& inPrimitive);
  void emit_Ld_IRegisterIndirect(Primitive& inPrimitive);
  void emit_StI_IRegisterIndirect(Primitive& inPrimitive);
  void emit_St_IRegisterIndirect(Primitive& inPrimitive);
  VirtualRegister& genLoadConstant(DataNode& inPrimitive, int32 inConstant);
};

#endif /* _SPARC_SUNOS_EMITTER_H_ */
