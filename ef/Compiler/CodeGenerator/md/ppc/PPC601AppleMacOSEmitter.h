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
//	PPC601AllMacOSEmitter.h
//
//	Scott M. Silver
//

#ifndef _H_PPC601AllMacOSEmitter
#define _H_PPC601AllMacOSEmitter

#include "InstructionEmitter.h"
#include "VirtualRegister.h"
#include "Value.h"
#include "PPCInstructions.h"
#include "PPC601AppleMacOS_Support.h"

#include "FormatStructures.h"	// for FCI, StackFrameInfo

/* FIX these  have been moved to FormatStructures.h

struct StackFrameInfo
{
	Uint8	GPR_words;
	Uint8	FPR_words;
	Uint32	localStore_bytes;

	// returns the offset into the stack frame of the beginning of the restore area
	Uint32	getRestoreOffset() { return localStore_bytes; }
};

// struct FormattedCodeInfo;
*/

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> class Call;

class PPCEmitter :
	public InstructionEmitter
{
	// these Call Instructions set "themselves" up, ie they call "useProducer" 
	friend class Call<true, true, false, false>;		
	friend class Call<true, false, false, false>;
	friend class Call<false, false, false, false>;
	friend class Call<true, true, true, false>;
	friend class Call<true, true, true, true>;	
	friend class MacDebugger;

	friend class PPCFormatter;		// uses protected member variables for linkage, etc
	
protected:
	uint32			mMaxArgWords;			// maximum words needed in call build area (accumulated)
	uint32			mCrossTOCPtrGlCount;	// number of ptr glue's needed (all are cross-toc now, even if not needed)
	JitGlobals		mAccumulatorTOC;		// TOC which contains all the data extracted at emit time, transferred to actual TOC
											// when formatting (we need to make sure we have a TOC which can contain all of the
											// necessary data)
	
public:
					PPCEmitter(Pool& inPool, VirtualRegisterManager& inVrManager) :
						InstructionEmitter(inPool, inVrManager),
						mCrossTOCPtrGlCount(0),
						mMaxArgWords(0) { }
						
	virtual void	emitPrimitive(Primitive& inPrimitive, NamedRule inRule);
	bool		 	emitCopyAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& fromVr, VirtualRegister& toVr);
	void			emitLoadAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& loadedReg, VirtualRegister& stackReg);
	void			emitStoreAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& storedReg, VirtualRegister& stackReg);
	Instruction& 	emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget);
	void			emitArguments(ControlNode::BeginExtra& inBeginNode);

private:
	void			emit_LimitR(Primitive& inPrimitive);
	void			emit_Limit(Primitive& inPrimitive, PPCInsnFlags inFlags);
	void			emit_Const_I(Primitive& inPrimitive);
	void			emit_Const_F(Primitive& inPrimitive);
	void			emit_Ld_IRegisterIndirect(Primitive& inPrimitive);
	void			emit_Add_I(Primitive& inPrimitive);
	void			emit_AddI_I(Primitive& inPrimitive);
	
	void			emit_Div_I(Primitive& inPrimitive);
	void			emit_DivE_I(Primitive& inPrimitive);
	void			emit_DivI_I(Primitive& inPrimitive);
	void			emit_DivR_I(Primitive& inPrimitive);
	void			emit_DivRE_I(Primitive& inPrimitive);
	void			emit_DivU_I(Primitive& inPrimitive);
	void			emit_DivUE_I(Primitive& inPrimitive);
	void			emit_DivUI_I(Primitive& inPrimitive);
	void			emit_DivUR_I(Primitive& inPrimitive);
	void			emit_DivURE_I(Primitive& inPrimitive);
		
	void			emit_Mod_I(Primitive& inPrimitive);
	void			emit_ModE_I(Primitive& inPrimitive);
			
	void			emit_CmpI_I(Primitive& inPrimitive);
	void			emit_Cmp_I(Primitive& inPrimitive);
	void			emit_StI_I(Primitive& inPrimitive);
	void 			emit_StI_IRegisterIndirect(Primitive& inPrimitive);
	void			emit_St_IRegisterIndirect(Primitive& inPrimitive);
	void			emit_ShlI_I(Primitive& inPrimitive);
	void			emit_Call(Primitive& inPrimitive);
	void			emit_Result_I(Primitive& inPrimitive);
	void			emit_Result_F(Primitive& inPrimitive);

	void			genBranch(Primitive& inPrimitive, Condition2 cond);
	void 			genLoadIZero(Primitive& inPrimitive, PPCInsnFlags inFlags);
	void			genArith_ID(Primitive& inPrimitive, DFormKind inKind, XFormKind inXKind);
	void			genArith_IX(Primitive& inPrimitive, XFormKind inKind);
	void 			genArith_RD(Primitive& inPrimitive, DFormKind inKind, XFormKind inXKind, bool inIsUnsigned);
	void			genArithReversed_IX(Primitive& inPrimitive, XFormKind inKind);
	void			emit_SubR(Primitive& inPrimitive, bool inUnsigned);
	void			emit_SubI(Primitive& inPrimitive);
	
	void			genThrowIfDivisorZero(Primitive& inPrimitive);
	
protected:
	VirtualRegister&	genLoadConstant_I(DataNode& inPrimitive, Int32 inConstant);
	VirtualRegister&	genLoadConstant_F(DataNode& inPrimitive, Flt32 inConstant);
};

class PPCFormatter
{
private:
	StackFrameInfo mStackPolicy;

protected:
	void*		mNextFreePostMethod;		// next free block of memory following the method's epilog (valid after beginFormatting)
	JitGlobals*	mRealTOC;					// contains this methods actual TOC (valid after beginFormatting)
	Int16		mRealTOCOffset;				// this is the "fix-up" added to an offset obtained from the accumulator TOC (valid after beginFormatting)
	
	uint8		mSaveGPRwords;				// number of non-volatile GPR's used in the function (valid after calculatePrologEpilog)
	uint8		mSaveFPRwords;				// number of non-volatile FPR's used in the function (valid after calculatePrologEpilog)
	uint32		mFrameSizeWords;			// whole size of frame (valid after calculatePrologEpilog)
	
	// these Instructions need access to mNextFreePostMethod and the real TOC
	friend class Call<true, true, false, false>;		
	friend class Call<true, false, false, false>;
	friend class Call<false, false, false, false>;
	friend class Call<true, true, true, false>;
	friend class Call<true, true, true, true>;	
	friend class MacDebugger;
	
	// Fix-up for real TOC
	friend class LdD_RTOC;
	
public:	
	const PPCEmitter&			mEmitter;
	
			PPCFormatter(const MdEmitter& inEmitter) :
				mEmitter(inEmitter) { }

	void	calculatePrologEpilog(Method& inMethod, Uint32& outPrologSize, Uint32& outEpilogSize);
	void	formatEpilogToMemory(void* inWhere);
	void	formatPrologToMemory(void* inWhere);

	void	beginFormatting(const FormattedCodeInfo& inInfo);
	void	endFormatting(const FormattedCodeInfo& /*inInfo*/) { }
	void	calculatePrePostMethod(Method& inMethod, Uint32& outPreMethodSize, Uint32& outPostMethodSize);
	void	formatPostMethodToMemory(void* inWhere, const FormattedCodeInfo& inInfo);
	void	formatPreMethodToMemory(void* /*inWhere*/, const FormattedCodeInfo& /*inInfo*/) { }

	void	initStackFrameInfo() {}

	Uint8*	createTransitionVector(const FormattedCodeInfo& /*inInfo*/);
	
	StackFrameInfo&	getStackFrameInfo() {return mStackPolicy; }

};

#ifdef DEBUG
void *disassemble1(LogModuleObject &f, void* inFrom);
#endif

#endif // _H_PPC601AllMacOSEmitter




