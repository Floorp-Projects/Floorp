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
#ifndef PRIMITIVEBUILDERS_H
#define PRIMITIVEBUILDERS_H

#include "ControlGraph.h"
#include "SysCalls.h"

struct Signature;


struct UnaryBuilder
{
	virtual Primitive *makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const = 0;
};


class UnaryGenericBuilder: public UnaryBuilder
{
  protected:
	const PrimitiveOperation op;				// Kind of primitive operation

	Primitive *makeOneProducerGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const;

  public:
	UnaryGenericBuilder(PrimitiveOperation op): op(op) {}
};


struct BinaryBuilder
{
	virtual Primitive *makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const = 0;
};


class BinaryGenericBuilder: public BinaryBuilder
{
  protected:
	const PrimitiveOperation op;				// Kind of primitive operation

	virtual Primitive *makeTwoProducerGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const;

  public:
	BinaryGenericBuilder(PrimitiveOperation op): op(op) {}
};


// ----------------------------------------------------------------------------
// NegBuilder
// Builder of negations

template<class T>
struct NegBuilder: UnaryBuilder
{
	Primitive *makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const;
};

extern const NegBuilder<Int32> builder_Neg_II;
extern const NegBuilder<Int64> builder_Neg_LL;
extern const NegBuilder<Flt32> builder_Neg_FF;
extern const NegBuilder<Flt64> builder_Neg_DD;


// ----------------------------------------------------------------------------
// ConvBuilder
// Builder of numeric conversions

template<class TA, class TR>
struct ConvBuilder: UnaryGenericBuilder
{
	Primitive *makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const;

	ConvBuilder(PrimitiveOperation op): UnaryGenericBuilder(op) {}
};

extern const ConvBuilder<Int32, Int64> builder_Conv_IL;
extern const ConvBuilder<Int32, Flt32> builder_Conv_IF;
extern const ConvBuilder<Int32, Flt64> builder_Conv_ID;
extern const ConvBuilder<Int64, Int32> builder_Conv_LI;
extern const ConvBuilder<Int64, Flt32> builder_Conv_LF;
extern const ConvBuilder<Int64, Flt64> builder_Conv_LD;
extern const ConvBuilder<Flt32, Int32> builder_Conv_FI;
extern const ConvBuilder<Flt32, Int64> builder_Conv_FL;
extern const ConvBuilder<Flt32, Flt64> builder_Conv_FD;
extern const ConvBuilder<Flt64, Int32> builder_Conv_DI;
extern const ConvBuilder<Flt64, Int64> builder_Conv_DL;
extern const ConvBuilder<Flt64, Flt32> builder_Conv_DF;


// ----------------------------------------------------------------------------
// ExtBuilder
// Builder of integer bit field extractions

struct ExtBuilder: UnaryBuilder
{
	const bool isSigned;						// True indicates the bit field is signed
	const int width;							// Width of the right-justified bit field (between 1 and 31)

	Primitive *makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const;

	ExtBuilder(bool isSigned, int width): isSigned(isSigned), width(width) {}
};

extern const ExtBuilder builder_ExtB_II;
extern const ExtBuilder builder_ExtC_II;
extern const ExtBuilder builder_ExtS_II;


// ----------------------------------------------------------------------------
// BinarySpecializedBuilder

template<class T1, class T2, class TR>
struct BinarySpecializedBuilder: BinaryGenericBuilder
{
	virtual void evaluate(T1 arg1, T2 arg2, TR &result) const = 0;
	Primitive *specializeConstConst(T1 arg1, T2 arg2, VariableOrConstant &result) const;
	virtual Primitive *specializeConstVar(T1 arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	virtual Primitive *specializeVarConst(DataNode &arg1, T2 arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeConstArg(T1 arg1, const VariableOrConstant &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeArgConst(const VariableOrConstant &arg1, T2 arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const;

	BinarySpecializedBuilder(PrimitiveOperation op): BinaryGenericBuilder(op) {}
};


//
// Allocate or evaluate the primitive with the two given arguments, both of which
// are constants.  The result will be a constant as well.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
//
template<class T1, class T2, class TR>
inline Primitive *BinarySpecializedBuilder<T1, T2, TR>::specializeConstConst(T1 arg1, T2 arg2, VariableOrConstant &result) const
{
	TR res;
	evaluate(arg1, arg2, res);
	result.setConstant(res);
	return 0;
}


//
// Allocate or evaluate the primitive with the two given arguments, the first of which
// is a constant and the second either a constant or a variable.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
//
template<class T1, class T2, class TR>
inline Primitive *BinarySpecializedBuilder<T1, T2, TR>::specializeConstArg(T1 arg1, const VariableOrConstant &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	assert(arg2.hasKind(TypeValueKind(T2)));
	Primitive *prim;

	if (arg2.isConstant())
		prim = specializeConstConst(arg1, arg2.getConstant().TypeGetValueContents(T2), result);
	else
		prim = specializeConstVar(arg1, arg2.getVariable(), result, container);
	assert(result.hasKind(TypeValueKind(TR)));
	return prim;
}


//
// Allocate or evaluate the primitive with the two given arguments, the second of which
// is a constant and the first either a constant or a variable.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
//
template<class T1, class T2, class TR>
inline Primitive *BinarySpecializedBuilder<T1, T2, TR>::specializeArgConst(const VariableOrConstant &arg1, T2 arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	assert(arg1.hasKind(TypeValueKind(T1)));
	Primitive *prim;

	if (arg1.isConstant())
		prim = specializeConstConst(arg1.getConstant().TypeGetValueContents(T1), arg2, result);
	else
		prim = specializeVarConst(arg1.getVariable(), arg2, result, container);
	assert(result.hasKind(TypeValueKind(TR)));
	return prim;
}


// ----------------------------------------------------------------------------
// CommutativeBuilder
// Builder of commutative operations

template<class TA, class TR>
struct CommutativeBuilder: BinarySpecializedBuilder<TA, TA, TR>
{
	Primitive *specializeConstVar(TA arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;

	CommutativeBuilder(PrimitiveOperation op): BinarySpecializedBuilder<TA, TA, TR>(op) {}
};


// ----------------------------------------------------------------------------
// AddBuilder
// Builder of homogeneous additions

template<class T>
struct AddBuilder: CommutativeBuilder<T, T>
{
	const bool coalesce;						// If true, this add may be coalesced with an add in one of the arguments

	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	AddBuilder(PrimitiveOperation op, bool coalesce): CommutativeBuilder<T, T>(op), coalesce(coalesce) {}
};

extern const AddBuilder<Int32> builder_Add_III;
extern const AddBuilder<Int64> builder_Add_LLL;
extern const AddBuilder<Flt32> builder_Add_FFF;
extern const AddBuilder<Flt64> builder_Add_DDD;

// Coalescing versions of the above; these automatically convert expressions
// like (x+c)+d where c and d are constants into x+(c+d).
extern const AddBuilder<Int32> builder_AddCoal_III;
extern const AddBuilder<Int64> builder_AddCoal_LLL;
// Coalescing is not available for floating point because floating point addition
// is not associative.

inline const AddBuilder<Int32> &getAddBuilder(Int32, bool coalesce) {return coalesce ? builder_AddCoal_III : builder_Add_III;}
inline const AddBuilder<Int64> &getAddBuilder(Int64, bool coalesce) {return coalesce ? builder_AddCoal_LLL : builder_Add_LLL;}
inline const AddBuilder<Flt32> &getAddBuilder(Flt32, bool) {return builder_Add_FFF;}
inline const AddBuilder<Flt64> &getAddBuilder(Flt64, bool) {return builder_Add_DDD;}
#define TypeGetAddBuilder(T, coalesce) getAddBuilder((T)0, coalesce)


// ----------------------------------------------------------------------------
// AddABuilder
// Builder of pointer additions

struct AddABuilder: BinarySpecializedBuilder<addr, Int32, addr>
{
	const bool coalesce;						// If true, this add may be coalesced with an add in one of the arguments

	void evaluate(addr arg1, Int32 arg2, addr &result) const;
	Primitive *specializeConstVar(addr arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeVarConst(DataNode &arg1, Int32 arg2, VariableOrConstant &result, ControlNode *container) const;

	explicit AddABuilder(bool coalesce): BinarySpecializedBuilder<addr, Int32, addr>(poAdd_A), coalesce(coalesce) {}
};

extern const AddABuilder builder_Add_AIA;
// Coalescing version of the above; this automatically converts expressions
// like (x+c)+d where c and d are constants into x+(c+d).
extern const AddABuilder builder_AddCoal_AIA;


// ----------------------------------------------------------------------------
// SubBuilder
// Builder of homogeneous subtractions

template<class T>
struct SubBuilder: BinarySpecializedBuilder<T, T, T>
{
	const bool coalesce;						// If true, this add may be coalesced with an add in one of the arguments

	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeConstVar(T arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	SubBuilder(PrimitiveOperation op, bool coalesce): BinarySpecializedBuilder<T, T, T>(op), coalesce(coalesce) {}
};

extern const SubBuilder<Int32> builder_Sub_III;
extern const SubBuilder<Int64> builder_Sub_LLL;
extern const SubBuilder<Flt32> builder_Sub_FFF;
extern const SubBuilder<Flt64> builder_Sub_DDD;

// Coalescing versions of the above; these automatically convert expressions
// like (x-c)-d where c and d are constants into x-(c+d).
extern const SubBuilder<Int32> builder_SubCoal_III;
extern const SubBuilder<Int64> builder_SubCoal_LLL;
// Coalescing is not available for floating point because floating point addition
// is not associative.

inline const SubBuilder<Int32> &getSubBuilder(Int32) {return builder_Sub_III;}
inline const SubBuilder<Int64> &getSubBuilder(Int64) {return builder_Sub_LLL;}
inline const SubBuilder<Flt32> &getSubBuilder(Flt32) {return builder_Sub_FFF;}
inline const SubBuilder<Flt64> &getSubBuilder(Flt64) {return builder_Sub_DDD;}
#define TypeGetSubBuilder(T) getSubBuilder((T)0)


// ----------------------------------------------------------------------------
// MulBuilder
// Builder of homogeneous multiplications

template<class T>
struct MulBuilder: CommutativeBuilder<T, T>
{
	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	MulBuilder(PrimitiveOperation op): CommutativeBuilder<T, T>(op) {}
};

extern const MulBuilder<Int32> builder_Mul_III;
extern const MulBuilder<Int64> builder_Mul_LLL;
extern const MulBuilder<Flt32> builder_Mul_FFF;
extern const MulBuilder<Flt64> builder_Mul_DDD;


// ----------------------------------------------------------------------------
// DivModBuilder
// Builder of integer divisions and modulos

class DivModBuilder: public BinaryBuilder
{
	const BinaryBuilder &generalBuilder;		// Builder to use when we don't know whether the divisor is zero
	const BinaryBuilder &nonZeroBuilder;		// Builder to use when we know that the divisor is nonzero

  public:
	Primitive *makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const;

	DivModBuilder(const BinaryBuilder &generalBuilder, const BinaryBuilder &nonZeroBuilder):
		generalBuilder(generalBuilder), nonZeroBuilder(nonZeroBuilder) {}
};

extern const DivModBuilder builder_Div_III;
extern const DivModBuilder builder_Div_LLL;
extern const DivModBuilder builder_Mod_III;
extern const DivModBuilder builder_Mod_LLL;


// ----------------------------------------------------------------------------
// FDivBuilder
// Builder of floating point divisions

template<class T>
struct FDivBuilder: BinarySpecializedBuilder<T, T, T>
{
	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeConstVar(T arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	FDivBuilder(PrimitiveOperation op): BinarySpecializedBuilder<T, T, T>(op) {}
};

extern const FDivBuilder<Flt32> builder_Div_FFF;
extern const FDivBuilder<Flt64> builder_Div_DDD;


// ----------------------------------------------------------------------------
// FRemBuilder
// Builder of floating point remainders

template<class T>
struct FRemBuilder: BinarySpecializedBuilder<T, T, T>
{
	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeConstVar(T arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	FRemBuilder(PrimitiveOperation op): BinarySpecializedBuilder<T, T, T>(op) {}
};

extern const FRemBuilder<Flt32> builder_Rem_FFF;
extern const FRemBuilder<Flt64> builder_Rem_DDD;


// ----------------------------------------------------------------------------
// ShiftBuilder
// Builder of arithmetic and logical shifts

enum ShiftDir {sdLeft, sdRightArithmetic, sdRightLogical, sdSignedExtract};

template<class T>
struct ShiftBuilder: BinarySpecializedBuilder<T, Int32, T>
{
	const ShiftDir shiftDir;

	void evaluate(T arg1, Int32 arg2, T &result) const;
	Primitive *specializeConstVar(T arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeVarConst(DataNode &arg1, Int32 arg2, VariableOrConstant &result, ControlNode *container) const;

	ShiftBuilder(PrimitiveOperation op, ShiftDir shiftDir);
};

extern const ShiftBuilder<Int32> builder_Shl_III;
extern const ShiftBuilder<Int64> builder_Shl_LIL;
extern const ShiftBuilder<Int32> builder_Shr_III;
extern const ShiftBuilder<Int64> builder_Shr_LIL;
extern const ShiftBuilder<Int32> builder_UShr_III;
extern const ShiftBuilder<Int64> builder_UShr_LIL;
extern const ShiftBuilder<Int32> builder_Ext_III;
extern const ShiftBuilder<Int64> builder_Ext_LIL;


// ----------------------------------------------------------------------------
// LogicalBuilder
// Builder of homogeneous logical operations

enum LogicalOp {loAnd, loOr, loXor};

template<class T>
struct LogicalBuilder: CommutativeBuilder<T, T>
{
	const LogicalOp logicalOp;					// The logical operation op that this builder builds
	const T identity;							// A value i such that for all x:  x op i, i op x, and x are all equal.
	const T annihilator;						// A value a such that for all x:  x op a, a op x, and a are all equal.
												// If there is no such a, set annihilator to the same value as identity.

	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	LogicalBuilder(PrimitiveOperation op, LogicalOp logicalOp, T identity, T annihilator);
};

extern const LogicalBuilder<Int32> builder_And_III;
extern const LogicalBuilder<Int64> builder_And_LLL;
extern const LogicalBuilder<Int32> builder_Or_III;
extern const LogicalBuilder<Int64> builder_Or_LLL;
extern const LogicalBuilder<Int32> builder_Xor_III;
extern const LogicalBuilder<Int64> builder_Xor_LLL;


// ----------------------------------------------------------------------------
// ComparisonBuilder
// Builder of two-argument comparisons

template<class T>
struct ComparisonBuilder: BinarySpecializedBuilder<T, T, Condition>
{
	const bool isUnsigned;						// Is the comparison unsigned?
	const bool equalityOnly;					// Do we only care about equality/inequality?

	void evaluate(T arg1, T arg2, Condition &result) const;
	Primitive *specializeConstVar(T arg1, DataNode &arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;
	Primitive *makeTwoProducerGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const;

	ComparisonBuilder(PrimitiveOperation op, bool isUnsigned, bool equalityOnly);
};

extern const ComparisonBuilder<Int32> builder_Cmp_IIC;
extern const ComparisonBuilder<Int64> builder_Cmp_LLC;
extern const ComparisonBuilder<Flt32> builder_Cmp_FFC;
extern const ComparisonBuilder<Flt64> builder_Cmp_DDC;

extern const ComparisonBuilder<Int32> builder_CmpU_IIC;
extern const ComparisonBuilder<addr> builder_CmpU_AAC;

// Same as Cmp or CmpU except that the comparison only compares for equality
// so the resulting condition is always either cEq or cUn; these may be slightly
// faster than Cmp or CmpU when they do apply.
extern const ComparisonBuilder<Int32> builder_CmpEq_IIC;
extern const ComparisonBuilder<addr> builder_CmpUEq_AAC;


// ----------------------------------------------------------------------------
// Cond2Builder
// Builder of two-argument two-way conditionals that yield an integer

class Cond2Builder: public BinaryBuilder
{
	const PrimitiveOperation opNormal ENUM_16;	// Kind of condition primitive when the comparison is done normally
	const PrimitiveOperation opReverse ENUM_16;	// Kind of condition primitive when the comparison's arguments are reversed
	const BinaryBuilder &comparer;				// Builder of the comparison

  public:
	Primitive *makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const;

	Cond2Builder(PrimitiveOperation opNormal, PrimitiveOperation opReverse, const BinaryBuilder &comparer);
};

extern const Cond2Builder builder_Eq_III;
extern const Cond2Builder builder_Ne_III;
extern const Cond2Builder builder_Lt_III;
extern const Cond2Builder builder_Ge_III;
extern const Cond2Builder builder_Gt_III;
extern const Cond2Builder builder_Le_III;
extern const Cond2Builder builder_LtU_III;
extern const Cond2Builder builder_GeU_III;
extern const Cond2Builder builder_GtU_III;
extern const Cond2Builder builder_LeU_III;
extern const Cond2Builder builder_Eq_AAI;
extern const Cond2Builder builder_Ne_AAI;


// ----------------------------------------------------------------------------
// Cond3Builder
// Builder of two-argument three-way conditionals that yield an integer

class Cond3Builder: public BinaryBuilder
{
	const PrimitiveOperation opNormal ENUM_16;	// Kind of condition primitive when the comparison is done normally
	const PrimitiveOperation opReverse ENUM_16;	// Kind of condition primitive when the comparison's arguments are reversed
	const BinaryBuilder &comparer;				// Builder of the comparison

  public:
	Primitive *makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result, ControlNode *container) const;

	Cond3Builder(PrimitiveOperation opNormal, PrimitiveOperation opReverse, const BinaryBuilder &comparer);
};

extern const Cond3Builder builder_Cmp3_LLI;
extern const Cond3Builder builder_Cmp3L_FFI;
extern const Cond3Builder builder_Cmp3G_FFI;
extern const Cond3Builder builder_Cmp3L_DDI;
extern const Cond3Builder builder_Cmp3G_DDI;


// ----------------------------------------------------------------------------
// TestBuilder
// Builder of one-argument three-way and two-way comparisons against zero

template<class T>
class TestBuilder: public UnaryBuilder
{
	const BinaryBuilder &condBuilder; 			// Two-argument version of the same comparison

  public:
	Primitive *makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const;

	explicit TestBuilder(const BinaryBuilder &condBuilder);
};

extern const TestBuilder<Int32> builder_Eq0_II;
extern const TestBuilder<Int32> builder_Ne0_II;
extern const TestBuilder<Int32> builder_Lt0_II;
extern const TestBuilder<Int32> builder_Ge0_II;
extern const TestBuilder<Int32> builder_Gt0_II;
extern const TestBuilder<Int32> builder_Le0_II;
extern const TestBuilder<addr> builder_Eq0_AI;
extern const TestBuilder<addr> builder_Ne0_AI;


// ----------------------------------------------------------------------------
// Check primitive builders

enum CheckResult {chkAlwaysOK, chkNeverOK, chkMaybeOK};

CheckResult makeChkNull(const VariableOrConstant &arg, Primitive *&prim,
						ControlNode *container, Uint32 bci);
CheckResult makeLimit(const VariableOrConstant &arg1, const VariableOrConstant &arg2, Primitive *&prim, ControlNode *container);
Primitive *makeChkCastI(DataNode &arg1, DataNode &arg2, ControlNode *container);
Primitive *makeChkCastA(DataNode &dp, const Type &type, ControlNode *container);
CheckResult makeLimCast(const VariableOrConstant &arg1, Int32 limit, Primitive *&prim, ControlNode *container);


// ----------------------------------------------------------------------------
// LoadBuilder
// Builder of memory loads

class LoadBuilder
{
	const PrimitiveOperation op ENUM_16;		// Kind of load primitive
	const TypeKind outputType ENUM_8;			// The type of the value that we're loading

	PrimLoad *createLoad(PrimitiveOperation op, DataNode **memory,
						 VariableOrConstant &result, bool isVolatile, bool
						 isConstant, ControlNode *container, Uint32 bci)
		const; 
  public:
	void makeLoad(DataNode **memory, addr address, 
				  VariableOrConstant &result, bool isVolatile, 
				  bool isConstant, ControlNode *container, Uint32 bci) const;
	void makeLoad(DataNode **memory, const VariableOrConstant &address, 
				  VariableOrConstant &result, bool isVolatile, 
				  bool isConstant, ControlNode *container, Uint32 bci) const; 

	LoadBuilder(PrimitiveOperation op, TypeKind outputType);
};

extern const LoadBuilder builder_Ld_AI;
extern const LoadBuilder builder_Ld_AL;
extern const LoadBuilder builder_Ld_AF;
extern const LoadBuilder builder_Ld_AD;
extern const LoadBuilder builder_Ld_AA;
extern const LoadBuilder builder_LdS_AB;
extern const LoadBuilder builder_LdS_AH;
extern const LoadBuilder builder_LdU_AB;
extern const LoadBuilder builder_LdU_AH;


// ----------------------------------------------------------------------------
// StoreBuilder
// Builder of memory stores

class StoreBuilder
{
	const PrimitiveOperation opPure ENUM_16;	// Kind of store primitive when the arguments are variables
	const ValueKind inputKind ENUM_8;			// The kind of the value that we're storing

  public:
	Primitive &makeStore(DataNode &memoryIn, const VariableOrConstant
						 &address, const VariableOrConstant &value, 
						 bool isVolatile, ControlNode *container, Uint32 bci)
		const;

	StoreBuilder(PrimitiveOperation opPure, ValueKind inputKind);
};

extern const StoreBuilder builder_St_AB;
extern const StoreBuilder builder_St_AH;
extern const StoreBuilder builder_St_AI;
extern const StoreBuilder builder_St_AL;
extern const StoreBuilder builder_St_AF;
extern const StoreBuilder builder_St_AD;
extern const StoreBuilder builder_St_AA;

// ----------------------------------------------------------------------------
// Monitor primitive builders

Primitive &makeMonitorPrimitive(PrimitiveOperation op, DataNode &memoryIn, const VariableOrConstant &arg, Uint32 slot,
								ControlNode *container, Uint32 bci);


// ----------------------------------------------------------------------------
// Call builders

// Use the checked inline version below instead of this makeSysCall.
Primitive &makeSysCall(const SysCall &sysCall, DataNode **memory, const VariableOrConstant *args,
					   VariableOrConstant *results, ControlNode &container, Uint32 bci);

inline Primitive &makeSysCall(const SysCall &sysCall, DataNode **memory, uint DEBUG_ONLY(nArgs), const VariableOrConstant *args,
							  uint DEBUG_ONLY(nResults), VariableOrConstant *results, ControlNode &container, Uint32 bci)
{
	assert(nArgs == sysCall.nInputs - sysCall.hasMemoryInput() &&
		   nResults == sysCall.nOutputs - sysCall.hasMemoryOutput() &&
		   sysCall.hasMemoryInput() == (memory != 0) &&
		   sysCall.hasMemoryOutput() == (memory != 0));
	return makeSysCall(sysCall, memory, args, results, container, bci);
}


Primitive &makeCall(const Signature &sig, DataNode *&memory, const VariableOrConstant &function, const Method *method,
					const VariableOrConstant *args, VariableOrConstant *results, ControlNode &container, Uint32 bci);

#endif
