/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
