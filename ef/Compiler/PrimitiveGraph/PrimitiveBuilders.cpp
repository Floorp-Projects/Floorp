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
#include "PrimitiveBuilders.h"
#include "Method.h"
#include <math.h>       // include'ed for fmod()

// ----------------------------------------------------------------------------
// UnaryGenericBuilder

//
// Allocate a primitive with the given argument, which is known to be a variable.
// The kind of primitive operation comes from this UnaryGenericBuilder record.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from the argument).
//

Primitive *UnaryGenericBuilder::makeOneProducerGeneric(const VariableOrConstant &arg, VariableOrConstant &result,
			ControlNode *container) const
{
	PrimUnaryGeneric *prim = new(container->getPrimitivePool()) PrimUnaryGeneric(op, false);
	prim->getInput().setVariable(arg.getVariable());
	result.setVariable(*prim);
	container->appendPrimitive(*prim);
	return prim;
}


// ----------------------------------------------------------------------------
// BinaryGenericBuilder

//
// Allocate a primitive with the two given arguments, both of which are known to
// be variables.
// The kind of primitive operation comes from this BinaryGenericBuilder record.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
//
Primitive *BinaryGenericBuilder::makeTwoProducerGeneric(const VariableOrConstant *args, VariableOrConstant &result,
		ControlNode *container) const
{
	PrimBinaryGeneric *prim = new(container->getPrimitivePool()) PrimBinaryGeneric(op, false);
	prim->getInputs()[0].setVariable(args[0].getVariable());
	prim->getInputs()[1].setVariable(args[1].getVariable());
	result.setVariable(*prim);
	container->appendPrimitive(*prim);
	return prim;
}


// ----------------------------------------------------------------------------
// NegBuilder

template<class T>
Primitive *NegBuilder<T>::makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result,
			ControlNode *container) const
{
	return TypeGetSubBuilder(T).specializeConstArg(TypeGetAdditiveIdentity(T), arg, result, container);
}

#ifdef MANUAL_TEMPLATES
 template class NegBuilder<Int32>;
 template class NegBuilder<Int64>;
 template class NegBuilder<Flt32>;
 template class NegBuilder<Flt64>;
#endif

const NegBuilder<Int32> builder_Neg_II;
const NegBuilder<Int64> builder_Neg_LL;
const NegBuilder<Flt32> builder_Neg_FF;
const NegBuilder<Flt64> builder_Neg_DD;


// ----------------------------------------------------------------------------
// ConvBuilder

template<class TA, class TR>
Primitive *ConvBuilder<TA, TR>::makeUnaryGeneric(const VariableOrConstant &arg,
			VariableOrConstant &result, ControlNode *container) const
{
	assert(arg.hasKind(TypeValueKind(TA)));

	Primitive *prim;

	if (arg.isConstant()) {
		TR res;
		convertNumber(arg.getConstant().TypeGetValueContents(TA), res);
		result.setConstant(res);
		prim = 0;
	} else
		prim = makeOneProducerGeneric(arg, result, container);
	assert(result.hasKind(TypeValueKind(TR)));
	return prim;
}

#ifdef MANUAL_TEMPLATES
 template class ConvBuilder<Int32, Int64>;
 template class ConvBuilder<Int32, Flt32>;
 template class ConvBuilder<Int32, Flt64>;
 template class ConvBuilder<Int64, Int32>;
 template class ConvBuilder<Int64, Flt32>;
 template class ConvBuilder<Int64, Flt64>;
 template class ConvBuilder<Flt32, Int32>;
 template class ConvBuilder<Flt32, Int64>;
 template class ConvBuilder<Flt32, Flt64>;
 template class ConvBuilder<Flt64, Int32>;
 template class ConvBuilder<Flt64, Int64>;
 template class ConvBuilder<Flt64, Flt32>;
#endif

const ConvBuilder<Int32, Int64> builder_Conv_IL(poConvL_I);
const ConvBuilder<Int32, Flt32> builder_Conv_IF(poFConvF_I);
const ConvBuilder<Int32, Flt64> builder_Conv_ID(poFConvD_I);
const ConvBuilder<Int64, Int32> builder_Conv_LI(poConvI_L);
const ConvBuilder<Int64, Flt32> builder_Conv_LF(poFConvF_L);
const ConvBuilder<Int64, Flt64> builder_Conv_LD(poFConvD_L);
const ConvBuilder<Flt32, Int32> builder_Conv_FI(poFConvI_F);
const ConvBuilder<Flt32, Int64> builder_Conv_FL(poFConvL_F);
const ConvBuilder<Flt32, Flt64> builder_Conv_FD(poFConvD_F);
const ConvBuilder<Flt64, Int32> builder_Conv_DI(poFConvI_D);
const ConvBuilder<Flt64, Int64> builder_Conv_DL(poFConvL_D);
const ConvBuilder<Flt64, Flt32> builder_Conv_DF(poFConvF_D);


// ----------------------------------------------------------------------------
// ExtBuilder

Primitive *ExtBuilder::makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *container) const
{
	if (isSigned)
		return builder_Ext_III.specializeArgConst(arg, width, result, container);
	else
		return builder_And_III.specializeArgConst(arg, ((Int32)1<<width)-1, result, container);
}

const ExtBuilder builder_ExtB_II(true, 8);
const ExtBuilder builder_ExtC_II(false, 16);
const ExtBuilder builder_ExtS_II(true, 16);


// ----------------------------------------------------------------------------
// BinarySpecializedBuilder

//
// Allocate or evaluate a primitive with the two given arguments.  If the result is
// always a constant, just return the constant in result.  If not, create a new
// PrimBinaryGeneric in the ControlNode container, link it to its arguments, and return a
// reference to it in result.  It is also possible that the primitive just
// returns one of its arguments (such as (x + 0), which simplifies to x), in which
// case result will be set to one of the args.
// The kind of primitive operation comes from this BinarySpecializedBuilder record.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
//
template<class T1, class T2, class TR>
Primitive *BinarySpecializedBuilder<T1, T2, TR>::makeBinaryGeneric(const VariableOrConstant *args,
		VariableOrConstant &result, ControlNode *container) const
{
	assert(args[0].hasKind(TypeValueKind(T1)) && args[1].hasKind(TypeValueKind(T2)));

	bool constArg2 = args[1].isConstant();
	Primitive *prim;

	if (args[0].isConstant())
		if (constArg2)
			prim = specializeConstConst(args[0].getConstant().TypeGetValueContents(T1),
						args[1].getConstant().TypeGetValueContents(T2), result);
		else
			prim = specializeConstVar(args[0].getConstant().TypeGetValueContents(T1), args[1].getVariable(), result, container);
	else
		if (constArg2)
			prim = specializeVarConst(args[0].getVariable(), args[1].getConstant().TypeGetValueContents(T2), result, container);
		else
			prim = makeTwoProducerGeneric(args, result, container);
    assert(result.hasKind(TypeValueKind(TR)));
	return prim;
}


//
// Allocate or evaluate a primitive with the two given arguments, the first of which
// is a constant and the second a variable.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
// This is a virtual method.  The default simply creates the appropriate PrimBinaryGeneric node.
//
template<class T1, class T2, class TR>
Primitive *BinarySpecializedBuilder<T1, T2, TR>::specializeConstVar(T1 arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	PrimBinaryGeneric *prim = new(container->getPrimitivePool()) PrimBinaryGeneric(op, false);
	prim->getInputs()[0].setConstant(arg1);
	prim->getInputs()[1].setVariable(arg2);
	result.setVariable(*prim);
	container->appendPrimitive(*prim);
	return prim;
}


//
// Allocate or evaluate a primitive with the two given arguments, the first of which
// is a variable and the second a constant.
// Return the last new primitive that generates the result or 0 if none (when the
// result is a constant or copied directly from one of the arguments).
// This is a virtual method.  The default simply creates the appropriate PrimBinaryGeneric node.
//
template<class T1, class T2, class TR>
Primitive *BinarySpecializedBuilder<T1, T2, TR>::specializeVarConst(DataNode &arg1, T2 arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	PrimBinaryGeneric *prim = new(container->getPrimitivePool()) PrimBinaryGeneric(op, false);
	prim->getInputs()[0].setVariable(arg1);
	prim->getInputs()[1].setConstant(arg2);
	result.setVariable(*prim);
	container->appendPrimitive(*prim);
	return prim;
}


#ifdef MANUAL_TEMPLATES
 template class BinarySpecializedBuilder<Int32, Int32, Int32>;
 template class BinarySpecializedBuilder<Int64, Int32, Int64>;
 template class BinarySpecializedBuilder<Int64, Int64, Int64>;
 template class BinarySpecializedBuilder<Flt32, Flt32, Flt32>;
 template class BinarySpecializedBuilder<Flt64, Flt64, Flt64>;

 template class BinarySpecializedBuilder<Int32, Int32, Condition>;
 template class BinarySpecializedBuilder<Int64, Int64, Condition>;
 template class BinarySpecializedBuilder<Flt32, Flt32, Condition>;
 template class BinarySpecializedBuilder<Flt64, Flt64, Condition>;
 template class BinarySpecializedBuilder<Address, Int32, Address>;
 template class BinarySpecializedBuilder<Address, Address, Condition>;
#endif


// ----------------------------------------------------------------------------
// CommutativeBuilder

//
// In a commutative operation, always make the constant into the second argument.
//
template<class TA, class TR>
Primitive *CommutativeBuilder<TA, TR>::specializeConstVar(TA arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	return specializeVarConst(arg2, arg1, result, container);
}

#ifdef MANUAL_TEMPLATES
 template class CommutativeBuilder<Int32, Int32>;
 template class CommutativeBuilder<Int64, Int64>;
 template class CommutativeBuilder<Flt32, Flt32>;
 template class CommutativeBuilder<Flt64, Flt64>;
#endif


// ----------------------------------------------------------------------------
// AddBuilder

template<class T>
void AddBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	result = arg1 + arg2;
}

template<class T>
Primitive *AddBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
    DataNode *a1 = &arg1;
    while (true) {
        if (isAdditiveAnnihilator(arg2)) {
            result.setConstant(arg2);
            return 0;
        } else if (isAdditiveIdentity(arg2)) {
            result.setVariable(*a1);
            return 0;
        } else if (coalesce && a1->hasOperation(op)) {
            PrimBinaryGeneric &node = PrimBinaryGeneric::cast(*a1);
            DataConsumer *inputs = node.getInputs();
            if (inputs[1].isConstant()) {
                a1 = &inputs[0].getVariable();
                assert(inputs[1].hasKind(TypeValueKind(T)));
                arg2 += inputs[1].getConstant().TypeGetValueContents(T);
                continue;
            }
        }
        return CommutativeBuilder<T, T>::specializeVarConst(*a1, arg2, result, container);
    }
}

#ifdef MANUAL_TEMPLATES
 template class AddBuilder<Int32>;
 template class AddBuilder<Int64>;
 template class AddBuilder<Flt32>;
 template class AddBuilder<Flt64>;
#endif

const AddBuilder<Int32> builder_Add_III(poAdd_I, false);
const AddBuilder<Int64> builder_Add_LLL(poAdd_L, false);
const AddBuilder<Flt32> builder_Add_FFF(poFAdd_F, false);
const AddBuilder<Flt64> builder_Add_DDD(poFAdd_D, false);

const AddBuilder<Int32> builder_AddCoal_III(poAdd_I, true);
const AddBuilder<Int64> builder_AddCoal_LLL(poAdd_L, true);


// ----------------------------------------------------------------------------
// AddABuilder

void AddABuilder::evaluate(addr arg1, Int32 arg2, addr &result) const
{
	result = arg1 + arg2;
}

Primitive *AddABuilder::specializeVarConst(DataNode &arg1, Int32 arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	DataNode *a1 = &arg1;
	while (true) {
		if (arg2 == 0) {
			result.setVariable(*a1);
			return 0;
		} else if (coalesce && a1->hasOperation(poAdd_A)) {
			PrimBinaryGeneric &node = PrimBinaryGeneric::cast(*a1);
			DataConsumer *inputs = node.getInputs();
			if (inputs[1].isConstant()) {
				a1 = &inputs[0].getVariable();
				assert(inputs[1].hasKind(vkInt));
				arg2 += inputs[1].getConstant().i;
				continue;
			}
		}
        return BinarySpecializedBuilder<addr, Int32, addr>::specializeVarConst(*a1, arg2, result, container);
    }
}

Primitive *AddABuilder::specializeConstVar(addr arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (!arg1)
		result.setVariable(arg2);
	else
		return BinarySpecializedBuilder<addr, Int32, addr>::specializeConstVar(arg1, arg2, result, container);
	return 0;
}

const AddABuilder builder_Add_AIA(false);
const AddABuilder builder_AddCoal_AIA(true);


// ----------------------------------------------------------------------------
// SubBuilder

template<class T>
void SubBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	result = arg1 - arg2;
}

template<class T>
Primitive *SubBuilder<T>::specializeConstVar(T arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isAdditiveAnnihilator(arg1))
		result.setConstant(arg1);
	else
		return BinarySpecializedBuilder<T, T, T>::specializeConstVar(arg1, arg2, result, container);
	return 0;
}

template<class T>
Primitive *SubBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	return TypeGetAddBuilder(T, coalesce).specializeVarConst(arg1, -arg2, result, container);
}

#ifdef MANUAL_TEMPLATES
 template class SubBuilder<Int32>;
 template class SubBuilder<Int64>;
 template class SubBuilder<Flt32>;
 template class SubBuilder<Flt64>;
#endif

const SubBuilder<Int32> builder_Sub_III(poSub_I, false);
const SubBuilder<Int64> builder_Sub_LLL(poSub_L, false);
const SubBuilder<Flt32> builder_Sub_FFF(poFSub_F, false);
const SubBuilder<Flt64> builder_Sub_DDD(poFSub_D, false);

const SubBuilder<Int32> builder_SubCoal_III(poSub_I, true);
const SubBuilder<Int64> builder_SubCoal_LLL(poSub_L, true);


// ----------------------------------------------------------------------------
// MulBuilder

template<class T>
void MulBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	result = arg1 * arg2;
}

template<class T>
Primitive *MulBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isMultiplicativeAnnihilator(arg2))
		result.setConstant(arg2);
	else if (isMultiplicativeIdentity(arg2))
		result.setVariable(arg1);
	else if (isMultiplicativeNegator(arg2))
		return TypeGetSubBuilder(T).specializeConstVar(TypeGetAdditiveIdentity(T), arg1, result, container);
	else
		return CommutativeBuilder<T, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

#ifdef MANUAL_TEMPLATES
 template class MulBuilder<Int32>;
 template class MulBuilder<Int64>;
 template class MulBuilder<Flt32>;
 template class MulBuilder<Flt64>;
#endif

const MulBuilder<Int32> builder_Mul_III(poMul_I);
const MulBuilder<Int64> builder_Mul_LLL(poMul_L);
const MulBuilder<Flt32> builder_Mul_FFF(poFMul_F);
const MulBuilder<Flt64> builder_Mul_DDD(poFMul_D);


// ----------------------------------------------------------------------------
// InternalDivBuilder

template<class T>
struct InternalDivBuilder: BinarySpecializedBuilder<T, T, T>
{
	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	InternalDivBuilder(PrimitiveOperation op):
		BinarySpecializedBuilder<T, T, T>(op) {}
};


template<class T>
void InternalDivBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	assert(!isZero(arg2));

	// the test JCK-114a|vm|instr|irem|irem001|irem00101|irem00101|Test1 tests boundary cases
	// for division. Eventually we may have a more elegant way of dealing with these kinds off errors,
	// but for now we'll explicitly check
	if(arg1 == 0x80000000 && arg2 == 0xffffffff) {
		result = arg1;	// see pg 243 of JVM Spec
		return;
	}

	result = arg1 / arg2;
}

template<class T>
Primitive *InternalDivBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	assert(!isZero(arg2));
	if (isMultiplicativeIdentity(arg2))
		result.setVariable(arg1);
	else if (isMultiplicativeNegator(arg2))
		return TypeGetSubBuilder(T).specializeConstVar(TypeGetAdditiveIdentity(T), arg1, result, container);
	else
		return BinarySpecializedBuilder<T, T, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

#ifdef MANUAL_TEMPLATES
 template class InternalDivBuilder<Int32>;
 template class InternalDivBuilder<Int64>;
#endif

static const InternalDivBuilder<Int32> internalBuilder_Div_III(poDivE_I);
static const InternalDivBuilder<Int64> internalBuilder_Div_LLL(poDivE_L);
static const InternalDivBuilder<Int32> internalBuilder_DivNZ_III(poDiv_I);
static const InternalDivBuilder<Int64> internalBuilder_DivNZ_LLL(poDiv_L);



// ----------------------------------------------------------------------------
// InternalModBuilder

template<class T>
struct InternalModBuilder: BinarySpecializedBuilder<T, T, T>
{
	void evaluate(T arg1, T arg2, T &result) const;
	Primitive *specializeVarConst(DataNode &arg1, T arg2, VariableOrConstant &result, ControlNode *container) const;

	InternalModBuilder(PrimitiveOperation op):
		BinarySpecializedBuilder<T, T, T>(op) {}
};


template<class T>
void InternalModBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	assert(!isZero(arg2));

	// see comment in InternalDivBuilder
	if(arg1 == 0x80000000 && arg2 == 0xffffffff) {
		result = 0;	// see pg 271 of JVM Spec
		return;
	}

	result = arg1 % arg2;
}

template<class T>
Primitive *InternalModBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	assert(!isZero(arg2));
	if (isMultiplicativeIdentity(arg2) || isMultiplicativeNegator(arg2))
		// Dividing by 1 or -1 always yields a remainder of 0.
		result.setConstant(TypeGetZero(T));
	else
		return BinarySpecializedBuilder<T, T, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

#ifdef MANUAL_TEMPLATES
 template class InternalModBuilder<Int32>;
 template class InternalModBuilder<Int64>;
#endif

static const InternalModBuilder<Int32> internalBuilder_Mod_III(poModE_I);
static const InternalModBuilder<Int64> internalBuilder_Mod_LLL(poModE_L);
static const InternalModBuilder<Int32> internalBuilder_ModNZ_III(poMod_I);
static const InternalModBuilder<Int64> internalBuilder_ModNZ_LLL(poMod_L);

// ----------------------------------------------------------------------------
// DivModBuilder

Primitive *DivModBuilder::makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result,
		ControlNode *container) const
{
	// Select a builder depending on whether the divisor can be zero.
	return (args[1].isAlwaysNonzero() ? nonZeroBuilder : generalBuilder).makeBinaryGeneric(args, result, container);
}

const DivModBuilder builder_Div_III(internalBuilder_Div_III, internalBuilder_DivNZ_III);
const DivModBuilder builder_Div_LLL(internalBuilder_Div_LLL, internalBuilder_DivNZ_LLL);
const DivModBuilder builder_Mod_III(internalBuilder_Mod_III, internalBuilder_ModNZ_III);
const DivModBuilder builder_Mod_LLL(internalBuilder_Mod_LLL, internalBuilder_ModNZ_LLL);


// ----------------------------------------------------------------------------
// FDivBuilder

template<class T>
void FDivBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	result = arg1 / arg2;
}

template<class T>
Primitive *FDivBuilder<T>::specializeConstVar(T arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isNaN(arg1))
		result.setConstant(arg1);
	else
		return BinarySpecializedBuilder<T, T, T>::specializeConstVar(arg1, arg2, result, container);
	return 0;
}

template<class T>
Primitive *FDivBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isNaN(arg2))
		result.setConstant(arg2);
	else if (isMultiplicativeIdentity(arg2))
		result.setVariable(arg1);
	else if (isMultiplicativeNegator(arg2))
		return TypeGetSubBuilder(T).specializeConstVar(TypeGetAdditiveIdentity(T), arg1, result, container);
	else
		return BinarySpecializedBuilder<T, T, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

#ifdef MANUAL_TEMPLATES
 template class FDivBuilder<Flt32>;
 template class FDivBuilder<Flt64>;
#endif

const FDivBuilder<Flt32> builder_Div_FFF(poFDiv_F);
const FDivBuilder<Flt64> builder_Div_DDD(poFDiv_D);


// ----------------------------------------------------------------------------
// FRemBuilder

template<class T>
void FRemBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
    result = (T)javaFMod((double)arg1, (double)arg2);
}

template<class T>
Primitive *FRemBuilder<T>::specializeConstVar(T arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isNaN(arg1) || isInfinite(arg1))
		result.setConstant(TypeGetNaN(T));
	else
		return BinarySpecializedBuilder<T, T, T>::specializeConstVar(arg1, arg2, result, container);
	return 0;
}

template<class T>
Primitive *FRemBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isNaN(arg2) || isZero(arg2))
		result.setConstant(TypeGetNaN(T));
	else
		return BinarySpecializedBuilder<T, T, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

#ifdef MANUAL_TEMPLATES
 template class FRemBuilder<Flt32>;
 template class FRemBuilder<Flt64>;
#endif

const FRemBuilder<Flt32> builder_Rem_FFF(poFRem_F);
const FRemBuilder<Flt64> builder_Rem_DDD(poFRem_D);


// ----------------------------------------------------------------------------
// ShiftBuilder

inline Int32 shiftMask(Int32) {return 31;}
inline Int32 shiftMask(Int64) {return 63;}

template<class T>
void ShiftBuilder<T>::evaluate(T arg1, Int32 arg2, T &result) const
{
	arg2 &= shiftMask(arg1);
	switch (shiftDir) {
		case sdLeft:
			result = arg1 << arg2;
			break;
		case sdRightArithmetic:
			result = arg1 >> arg2;
			break;
		case sdRightLogical:
			result = toSigned(toUnsigned(arg1) >> arg2);
			break;
		case sdSignedExtract:
			arg2 = -arg2 & shiftMask(arg1);
			result = arg1 << arg2 >> arg2;
			break;
	}
}

template<class T>
Primitive *ShiftBuilder<T>::specializeConstVar(T arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (isZero(arg1) || shiftDir == sdRightArithmetic && isMultiplicativeNegator(arg1))
		result.setConstant(arg1);
	else
		return BinarySpecializedBuilder<T, Int32, T>::specializeConstVar(arg1, arg2, result, container);
	return 0;
}

template<class T>
Primitive *ShiftBuilder<T>::specializeVarConst(DataNode &arg1, Int32 arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	arg2 &= shiftMask((T)0);
	if (arg2 == 0)
		result.setVariable(arg1);
	else
		return BinarySpecializedBuilder<T, Int32, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

template<class T>
ShiftBuilder<T>::ShiftBuilder(PrimitiveOperation op, ShiftDir shiftDir):
	BinarySpecializedBuilder<T, Int32, T>(op),
	shiftDir(shiftDir)
{}


#ifdef MANUAL_TEMPLATES
 template class ShiftBuilder<Int32>;
 template class ShiftBuilder<Int64>;
#endif

const ShiftBuilder<Int32> builder_Shl_III(poShl_I, sdLeft);
const ShiftBuilder<Int64> builder_Shl_LIL(poShl_L, sdLeft);
const ShiftBuilder<Int32> builder_Shr_III(poShr_I, sdRightArithmetic);
const ShiftBuilder<Int64> builder_Shr_LIL(poShr_L, sdRightArithmetic);
const ShiftBuilder<Int32> builder_UShr_III(poShrU_I, sdRightLogical);
const ShiftBuilder<Int64> builder_UShr_LIL(poShrU_L, sdRightLogical);
const ShiftBuilder<Int32> builder_Ext_III(poExt_I, sdSignedExtract);
const ShiftBuilder<Int64> builder_Ext_LIL(poExt_L, sdSignedExtract);


// ----------------------------------------------------------------------------
// LogicalBuilder

template<class T>
void LogicalBuilder<T>::evaluate(T arg1, T arg2, T &result) const
{
	switch(logicalOp) {
		case loAnd:
			result = arg1 & arg2;
			break;
		case loOr:
			result = arg1 | arg2;
			break;
		case loXor:
			result = arg1 ^ arg2;
			break;
	}
}

template<class T>
Primitive *LogicalBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	if (arg2 == identity)
		result.setVariable(arg1);
	else if (arg2 == annihilator)
		result.setConstant(arg2);
	else
		return CommutativeBuilder<T, T>::specializeVarConst(arg1, arg2, result, container);
	return 0;
}

template<class T>
LogicalBuilder<T>::LogicalBuilder(PrimitiveOperation op, LogicalOp logicalOp,	T identity, T annihilator):
	CommutativeBuilder<T, T>(op),
	logicalOp(logicalOp),
	identity(identity),
	annihilator(annihilator)
{}


#ifdef MANUAL_TEMPLATES
 template class LogicalBuilder<Int32>;
 template class LogicalBuilder<Int64>;
#endif

const LogicalBuilder<Int32> builder_And_III(poAnd_I, loAnd, -1, 0);
const LogicalBuilder<Int64> builder_And_LLL(poAnd_L, loAnd, (Int64)-1, (Int64)0);
const LogicalBuilder<Int32> builder_Or_III(poOr_I, loOr, 0, -1);
const LogicalBuilder<Int64> builder_Or_LLL(poOr_L, loOr, (Int64)0, (Int64)-1);
const LogicalBuilder<Int32> builder_Xor_III(poXor_I, loXor, 0, 0);
const LogicalBuilder<Int64> builder_Xor_LLL(poXor_L, loXor, (Int64)0, (Int64)0);


// ----------------------------------------------------------------------------
// ComparisonBuilder

template<class T>
void ComparisonBuilder<T>::evaluate(T arg1, T arg2, Condition &result) const
{
	Condition res = compare(arg1, arg2, isUnsigned);
	if (equalityOnly && res != cEq)
		res = cUn;
	result = res;
}

// Return nil if the comparison was generated normally, non-nil if its arguments were reversed.
template<class T>
Primitive *ComparisonBuilder<T>::specializeConstVar(T arg1, DataNode &arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	Primitive *p = specializeVarConst(arg2, arg1, result, container);
	assert(p == 0);
	return static_cast<Primitive *>(&result.getVariable());
}

// Return nil if the comparison was generated normally, non-nil if its arguments were reversed.
template<class T>
Primitive *ComparisonBuilder<T>::specializeVarConst(DataNode &arg1, T arg2,
		VariableOrConstant &result, ControlNode *container) const
{
	bool isNeverEqual = arg1.isAlwaysNonzero() && isZero(arg2);
	if (equalityOnly && isNeverEqual)
		result.setConstant(cUn);
	else {
		PrimBinaryGeneric *prim = new(container->getPrimitivePool()) PrimBinaryGeneric(op, isNeverEqual);
		prim->getInputs()[0].setVariable(arg1);
		prim->getInputs()[1].setConstant(arg2);
		result.setVariable(*prim);
		container->appendPrimitive(*prim);
	}
	return 0;
}

// Return nil if the comparison was generated normally, non-nil if its arguments were reversed.
template<class T>
Primitive *ComparisonBuilder<T>::makeTwoProducerGeneric(const VariableOrConstant *args, VariableOrConstant &result,
		ControlNode *container) const
{
	BinaryGenericBuilder::makeTwoProducerGeneric(args, result, container);
	return 0;
}

template<class T>
ComparisonBuilder<T>::ComparisonBuilder(PrimitiveOperation op, bool isUnsigned, bool equalityOnly):
	BinarySpecializedBuilder<T, T, Condition>(op),
	isUnsigned(isUnsigned),
	equalityOnly(equalityOnly)
{}


#ifdef MANUAL_TEMPLATES
 template class ComparisonBuilder<Int32>;
 template class ComparisonBuilder<Int64>;
 template class ComparisonBuilder<Flt32>;
 template class ComparisonBuilder<Flt64>;
 template class ComparisonBuilder<addr>;
#endif

const ComparisonBuilder<Int32> builder_Cmp_IIC(poCmp_I, false, false);
const ComparisonBuilder<Int64> builder_Cmp_LLC(poCmp_L, false, false);
const ComparisonBuilder<Flt32> builder_Cmp_FFC(poFCmp_F, false, false);
const ComparisonBuilder<Flt64> builder_Cmp_DDC(poFCmp_D, false, false);

const ComparisonBuilder<Int32> builder_CmpU_IIC(poCmpU_I, true, false);
const ComparisonBuilder<addr> builder_CmpU_AAC(poCmpU_A, true, false);

const ComparisonBuilder<Int32> builder_CmpEq_IIC(poCmp_I, false, true);
const ComparisonBuilder<addr> builder_CmpUEq_AAC(poCmpU_A, true, true);


// ----------------------------------------------------------------------------
// Cond2Builder

Primitive *Cond2Builder::makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result,
		ControlNode *container) const
{
	VariableOrConstant c;

	// If non-nil, the comparer reversed its arguments so we must use the reverse condition.
	PrimitiveOperation condKind = comparer.makeBinaryGeneric(args, c, container) ? opReverse : opNormal;

	bool constResult;
	DataNode *cp = partialComputeComparison(cond2ToCondition2(condKind), c, constResult);
	if (cp) {
		PrimUnaryGeneric *prim = new(container->getPrimitivePool()) PrimUnaryGeneric(condKind, false);
		prim->getInput().setVariable(*cp);
		result.setVariable(*prim);
		container->appendPrimitive(*prim);
		return prim;
	} else {
		result.setConstant((Int32)constResult);
		return 0;
	}
}

Cond2Builder::Cond2Builder(PrimitiveOperation opNormal, PrimitiveOperation opReverse, const BinaryBuilder &comparer):
	opNormal(opNormal),
	opReverse(opReverse),
	comparer(comparer)
{}


const Cond2Builder builder_Eq_III(poEq_I, poEq_I, builder_CmpEq_IIC);
const Cond2Builder builder_Ne_III(poNe_I, poNe_I, builder_CmpEq_IIC);
const Cond2Builder builder_Lt_III(poLt_I, poGt_I, builder_Cmp_IIC);
const Cond2Builder builder_Ge_III(poGe_I, poLe_I, builder_Cmp_IIC);
const Cond2Builder builder_Gt_III(poGt_I, poLt_I, builder_Cmp_IIC);
const Cond2Builder builder_Le_III(poLe_I, poGe_I, builder_Cmp_IIC);
const Cond2Builder builder_LtU_III(poLt_I, poGt_I, builder_CmpU_IIC);
const Cond2Builder builder_GeU_III(poGe_I, poLe_I, builder_CmpU_IIC);
const Cond2Builder builder_GtU_III(poGt_I, poLt_I, builder_CmpU_IIC);
const Cond2Builder builder_LeU_III(poLe_I, poGe_I, builder_CmpU_IIC);
const Cond2Builder builder_Eq_AAI(poEq_I, poEq_I, builder_CmpUEq_AAC);
const Cond2Builder builder_Ne_AAI(poNe_I, poNe_I, builder_CmpUEq_AAC);


// ----------------------------------------------------------------------------
// Cond3Builder

Primitive *Cond3Builder::makeBinaryGeneric(const VariableOrConstant *args, VariableOrConstant &result,
		ControlNode *container) const
{
	VariableOrConstant c;

	// If non-nil, the comparer reversed its arguments so we must use the reverse condition.
	PrimitiveOperation condKind = comparer.makeBinaryGeneric(args, c, container) ? opReverse : opNormal;
	assert(c.hasKind(vkCond));

	if (c.isConstant()) {
		result.setConstant((Int32)applyCondition(cond3ToCondition3(condKind), c.getConstant().c));
		return 0;
	} else {
		DataNode &cp = c.getVariable();
		PrimUnaryGeneric *prim = new(container->getPrimitivePool()) PrimUnaryGeneric(condKind, cp.isAlwaysNonzero());
		prim->getInput().setVariable(cp);
		result.setVariable(*prim);
		container->appendPrimitive(*prim);
		return prim;
	}
}

Cond3Builder::Cond3Builder(PrimitiveOperation opNormal, PrimitiveOperation opReverse, const BinaryBuilder &comparer):
	opNormal(opNormal),
	opReverse(opReverse),
	comparer(comparer)
{}


const Cond3Builder builder_Cmp3_LLI(poCatL_I, poCatCL_I, builder_Cmp_LLC);
const Cond3Builder builder_Cmp3L_FFI(poCatL_I, poCatCL_I, builder_Cmp_FFC);
const Cond3Builder builder_Cmp3G_FFI(poCatG_I, poCatCG_I, builder_Cmp_FFC);
const Cond3Builder builder_Cmp3L_DDI(poCatL_I, poCatCL_I, builder_Cmp_DDC);
const Cond3Builder builder_Cmp3G_DDI(poCatG_I, poCatCG_I, builder_Cmp_DDC);


// ----------------------------------------------------------------------------
// TestBuilder

template<class T>
Primitive *TestBuilder<T>::makeUnaryGeneric(const VariableOrConstant &arg, VariableOrConstant &result,
			ControlNode *container) const
{
	VariableOrConstant args[2];
	args[0] = arg;
	args[1].setConstant(TypeGetZero(T));
	return condBuilder.makeBinaryGeneric(args, result, container);
}

template<class T>
TestBuilder<T>::TestBuilder(const BinaryBuilder &condBuilder):
	condBuilder(condBuilder)
{}


#ifdef MANUAL_TEMPLATES
 template class TestBuilder<Int32>;
 template class TestBuilder<addr>;
#endif

const TestBuilder<Int32> builder_Eq0_II(builder_Eq_III);
const TestBuilder<Int32> builder_Ne0_II(builder_Ne_III);
const TestBuilder<Int32> builder_Lt0_II(builder_Lt_III);
const TestBuilder<Int32> builder_Ge0_II(builder_Ge_III);
const TestBuilder<Int32> builder_Gt0_II(builder_Gt_III);
const TestBuilder<Int32> builder_Le0_II(builder_Le_III);
const TestBuilder<addr> builder_Eq0_AI(builder_Eq_AAI);
const TestBuilder<addr> builder_Ne0_AI(builder_Ne_AAI);


// ----------------------------------------------------------------------------
// ChkNull and Limit primitive builders

//
// Construct a primitive to throw NullPointerException if arg is null.
// arg must have kind vkAddr.  If the test of arg against null can be done statically,
// return either chkAlwaysOK (if arg is never null) or chkNeverOK (if arg is always
// null), don't emit any primitives, and set prim to nil; otherwise, return
// chkMaybeOK, emit the check primitive, and set prim to point to the emitted
// primitive.
//
CheckResult 
makeChkNull(const VariableOrConstant &arg, Primitive *&prim, 
			ControlNode *container, Uint32 bci) 
{
	assert(arg.hasKind(vkAddr));

	prim = 0;
	if (arg.isConstant())
		return !arg.getConstant().a ? chkNeverOK : chkAlwaysOK;
	DataNode &dp = arg.getVariable();
	if (dp.isAlwaysNonzero())
		return chkAlwaysOK;
	PrimUnaryGeneric *p = new(container->getPrimitivePool()) PrimUnaryGeneric(poChkNull, bci);
	p->getInput().setVariable(dp);
	container->appendPrimitive(*p);
	prim = p;
	return chkMaybeOK;
}


//
// Construct a primitive to throw ArrayIndexOutOfBounds if arg1 >= arg2, treating
// both arg1 and arg2 as unsigned integers.  Both arguments must have kind vkInt.
// If the test can be done statically, return either chkAlwaysOK (if the test is
// always false) or chkNeverOK (if the test is always true), don't emit any primitives,
// and set prim to nil; otherwise, return chkMaybeOK, emit the limit primitive,
// and set prim to point to the emitted primitive.
//
CheckResult makeLimit(const VariableOrConstant &arg1, const VariableOrConstant &arg2, Primitive *&prim, ControlNode *container)
{
	assert(arg1.hasKind(vkInt) && arg2.hasKind(vkInt));

	prim = 0;
	if (arg2.isConstant()) {
		if (arg2.getConstant().i == 0)
			return chkNeverOK;
		if (arg1.isConstant())
			return (Uint32)arg1.getConstant().i < (Uint32)arg2.getConstant().i ? chkAlwaysOK : chkNeverOK;
	}
	PrimBinaryGeneric *p = new(container->getPrimitivePool()) PrimBinaryGeneric(poLimit, false);
	p->getInputs()[0] = arg1;
	p->getInputs()[1] = arg2;
	container->appendPrimitive(*p);
	prim = p;
	return chkMaybeOK;
}


//
// Construct a primitive to throw ClassCastException if the value of arg1 is not equal
// to the value of arg2.  Both arg1 and arg2 must have kind vkInt.
// Return the emitted primitive.
//
Primitive *makeChkCastI(DataNode &arg1, DataNode &arg2, ControlNode *container)
{
	assert(arg1.hasKind(vkInt) && arg2.hasKind(vkInt));

	PrimBinaryGeneric *p = new(container->getPrimitivePool()) PrimBinaryGeneric(poChkCast_I, false);
	p->getInputs()[0].setVariable(arg1);
	p->getInputs()[1].setVariable(arg2);
	container->appendPrimitive(*p);
	return p;
}


//
// Construct a primitive to throw ClassCastException if the value of dp is not equal
// to the given type object.  dp must have kind vkAddr.
// Return the emitted primitive.
//
Primitive *makeChkCastA(DataNode &dp, const Type &type, ControlNode *container)
{
	assert(dp.hasKind(vkAddr));

	PrimBinaryGeneric *p = new(container->getPrimitivePool()) PrimBinaryGeneric(poChkCast_A, false);
	p->getInputs()[0].setVariable(dp);
	p->getInputs()[1].setConstant(objectAddress(&type));
	container->appendPrimitive(*p);
	return p;
}


//
// Construct a primitive to throw ClassCastException if arg1 < limit, treating
// both arg1 and limit as either signed or unsigned integers.  arg1 must have kind vkInt.
// If the test can be done statically, return either chkAlwaysOK (if the test is
// always false) or chkNeverOK (if the test is always true), don't emit any primitives,
// and set prim to nil; otherwise, return chkMaybeOK, emit the limit primitive,
// and set prim to point to the emitted primitive.
//
CheckResult makeLimCast(const VariableOrConstant &arg1, Int32 limit, Primitive *&prim, ControlNode *container)
{
	assert(arg1.hasKind(vkInt));

	prim = 0;
	if (arg1.isConstant())
		return arg1.getConstant().i >= limit ? chkAlwaysOK : chkNeverOK;
	PrimBinaryGeneric *p = new(container->getPrimitivePool()) PrimBinaryGeneric(poLimCast, false);
	p->getInputs()[0] = arg1;
	p->getInputs()[1].setConstant(limit);
	container->appendPrimitive(*p);
	prim = p;
	return chkMaybeOK;
}


// ----------------------------------------------------------------------------
// LoadBuilder


//
// This subroutine is used by both variants of makeLoad below to actually
// create and return the PrimLoad primitive.
//
PrimLoad *
LoadBuilder::createLoad(PrimitiveOperation op, DataNode **memory,
						VariableOrConstant &result, bool isVolatile, 
						bool isConstant, ControlNode *container, Uint32 bci) 
	const
{
	Pool &pool = container->getPrimitivePool();
	if (isVolatile)
		op = (PrimitiveOperation)(op + poLdV_I - poLd_I);
	PrimLoad *prim = new(pool) PrimLoad(op, isVolatile, bci);
	if (isConstant)
		prim->getIncomingMemory().setConstant(mConstant);
	else
		prim->getIncomingMemory().setVariable(**memory);
	container->appendPrimitive(*prim);

	// If this is a volatile load, the result is a tuple, so create the
	// projection nodes to extract the memory and value results.
	DataNode *res = prim;
	if (isVolatile) {
		PrimProj *proj = new(pool) PrimProj(poProj_M, tcMemory, true, bci);
		container->appendPrimitive(*proj);
		proj->getInput().setVariable(*prim);
		*memory = proj;
		proj = new(pool) PrimProj(typeKindToValueKind(outputType), tcValue,
								  false, bci); 
		container->appendPrimitive(*proj);
		proj->getInput().setVariable(*prim);
		res = proj;
	}

	result.setVariable(*res);
	return prim;
}


//
// Make a Load primitive that reads the given constant address and store the
// value in result.  If isVolatile is true, obtain an incoming memory edge
// from memory, and store the outgoing memory edge there.  If isConstant is
// true, the memory argument is ignored.  If both isVolatile and isConstant
// are false, obtain incoming memory edge from memory but don't update it.
// It's not legal for both isVolatile and isConstant to be true.
//
void 
LoadBuilder::makeLoad(DataNode **memory, addr address, 
					  VariableOrConstant &result, bool isVolatile, 
					  bool isConstant, ControlNode *container, Uint32 bci)
	const 
{
	assert(!(isVolatile && isConstant));
	assert(!memory || (*memory)->hasKind(vkMemory));

	Value v;
	if (isConstant && read(outputType, address, v))
		result.setConstant(typeKindToValueKind(outputType), v);
	else {
		PrimLoad *prim = createLoad(op, memory, result, isVolatile,
									isConstant, container, bci);
		prim->getAddress().setConstant(address);
	}
}


//
// Make a Load primitive that reads the given address and store the value
// in result.  If isVolatile is true, obtain an incoming memory edge from
// memory, and store the outgoing memory edge there.  If isConstant is true,
// the memory argument is ignored.  If both isVolatile and isConstant are false,
// obtain incoming memory edge from memory but don't update it.  It's not
// legal for both isVolatile and isConstant to be true.
//
void 
LoadBuilder::makeLoad(DataNode **memory, const VariableOrConstant &address,
					  VariableOrConstant &result, bool isVolatile, 
					  bool isConstant, ControlNode *container, Uint32 bci) const 
{
	assert(!(isVolatile && isConstant));
	assert((!memory || (*memory)->hasKind(vkMemory)) && address.hasKind(vkAddr));
	if (address.isConstant())
		makeLoad(memory, address.getConstant().a, result, isVolatile,
				 isConstant, container, bci);
	else {
		PrimLoad *prim = createLoad(op, memory, result, isVolatile,
									isConstant, container, bci);
		prim->getAddress().setVariable(address.getVariable());
	}
}


LoadBuilder::LoadBuilder(PrimitiveOperation op, TypeKind outputType):
	op(op),
	outputType(outputType)
{}


const LoadBuilder builder_Ld_AI(poLd_I, tkInt);
const LoadBuilder builder_Ld_AL(poLd_L, tkLong);
const LoadBuilder builder_Ld_AF(poLd_F, tkFloat);
const LoadBuilder builder_Ld_AD(poLd_D, tkDouble);
const LoadBuilder builder_Ld_AA(poLd_A, tkObject);
const LoadBuilder builder_LdS_AB(poLdS_B, tkByte);
const LoadBuilder builder_LdS_AH(poLdS_H, tkShort);
const LoadBuilder builder_LdU_AB(poLdU_B, tkUByte);
const LoadBuilder builder_LdU_AH(poLdU_H, tkChar);


// ----------------------------------------------------------------------------
// StoreBuilder


//
// Make a Store primitive that writes the given value into the given address.
// Obtain an incoming memory edge from memoryIn, and return the outgoing
// memory edge.  If isVolatile is true, the store is volatile
// (which currently doesn't make any difference).
//
Primitive &StoreBuilder::makeStore(DataNode &memoryIn, 
								   const VariableOrConstant &address, 
								   const VariableOrConstant &value, 
								   bool isVolatile, ControlNode *container,
								   Uint32 bci) const  
{
	assert(memoryIn.hasKind(vkMemory) && address.hasKind(vkAddr) && value.hasKind(inputKind));

	PrimitiveOperation op = opPure;
	if (isVolatile)
		op = (PrimitiveOperation)(op + poStV_B - poSt_B);
	PrimStore &prim = *new(container->getPrimitivePool()) PrimStore(op, bci);
	prim.getIncomingMemory().setVariable(memoryIn);
	prim.getAddress() = address;
	prim.getData() = value;
	container->appendPrimitive(prim);
	return prim;
}


StoreBuilder::StoreBuilder(PrimitiveOperation opPure, ValueKind inputKind):
	opPure(opPure),
	inputKind(inputKind)
{}


const StoreBuilder builder_St_AB(poSt_B, vkInt);
const StoreBuilder builder_St_AH(poSt_H, vkInt);
const StoreBuilder builder_St_AI(poSt_I, vkInt);
const StoreBuilder builder_St_AL(poSt_L, vkLong);
const StoreBuilder builder_St_AF(poSt_F, vkFloat);
const StoreBuilder builder_St_AD(poSt_D, vkDouble);
const StoreBuilder builder_St_AA(poSt_A, vkAddr);


// ----------------------------------------------------------------------------
// Monitor primitive builders


//
// Construct an MEnter or MExit primitive using the given arg and saves subheader
// slot index.  Return the emitted primitive, which is also the outgoing memory edge.
//
Primitive &
makeMonitorPrimitive(PrimitiveOperation op, DataNode &memoryIn, 
					 const VariableOrConstant &arg, Uint32 slot, 
					 ControlNode *container, Uint32 bci) 
{
	assert(memoryIn.hasKind(vkMemory) && arg.hasKind(vkAddr));

	PrimMonitor &prim = *new(container->getPrimitivePool()) 
		PrimMonitor(op, slot, bci);
	prim.getIncomingMemory().setVariable(memoryIn);
	prim.getMonitor() = arg;
	container->appendPrimitive(prim);
	return prim;
}


// ----------------------------------------------------------------------------
// Call builders


//
// Make a system call primitive that takes zero or more arguments (given in
// the args array) and produces zero or more results (returned in the results
// array).  If memory is nonnil, obtain an incoming memory edge from memory,
// and store the outgoing memory edge there; if memory is nil, then the
// primitive does not read or write memory.  Put the system call primitive
// into the given container and return that system call primitive.  This
// function does not simplify primitives all of whose arguments are
// constants; the caller should do so if desired.  The returned primitive is
// guaranteed to be the only one generated by this function that uses the
// incoming memory edge, if any.
//
Primitive &
makeSysCall(const SysCall &sysCall, DataNode **memory, 
			const VariableOrConstant *args,
			VariableOrConstant *results, ControlNode &container, Uint32 bci)
{
	Pool &pool = container.getPrimitivePool();
	PrimSysCall &prim = *new(pool) PrimSysCall(sysCall, pool, bci);
	container.appendPrimitive(prim);

	uint i = sysCall.nInputs;
	DataConsumer *inputs = prim.getInputsBegin();
  #ifdef DEBUG
	const ValueKind *inputKinds = sysCall.inputKinds;
  #endif
	if (memory) {
	  #ifdef DEBUG
		assert(i && isMemoryKind(*inputKinds));
		inputKinds++;
	  #endif
		inputs++->setVariable(**memory);
		i--;
	}
	while (i--) {
	  #ifdef DEBUG
		assert(!args->hasKind(vkMemory) && args->hasKind(*inputKinds));
		inputKinds++;
	  #endif
		*inputs++ = *args++;
	}
	assert(inputs == prim.getInputsEnd());

	i = sysCall.nOutputs;
	const ValueKind *outputKind = sysCall.outputKinds;
	const bool *outputNonzero = sysCall.outputsNonzero;
	if (memory) {
		assert(i && isMemoryKind(sysCall.outputKinds[0]));
		PrimProj *proj = new(pool) PrimProj(poProj_M, tcMemory, true, bci);
		container.appendPrimitive(*proj);
		proj->getInput().setVariable(prim);
		*memory = proj;
		i--;
		outputKind++;
		outputNonzero++;
	}
	TupleComponent tc = tcFirstResult;
	while (i--) {
		PrimProj *proj = new(pool) PrimProj(*outputKind++, tc,
											*outputNonzero++, bci); 
		container.appendPrimitive(*proj);
		proj->getInput().setVariable(prim);
		results++->setVariable(*proj);
		tc = (TupleComponent)(tc + 1);
	}

	return prim;
}


//
// Make a call primitive that calls the given function.  The call takes zero
// or more arguments (given in the args array, with the count stored in the
// signature) and produces zero or one result (returned in the results
// array).  Obtain an incoming memory edge from memory, and store the
// outgoing memory edge there.  Put the call primitive into the given
// container and return that primitive.
//
// If the Method object is known for this function, the method argument
// should point to it; if not, the method argument must be nil.
//
Primitive &makeCall(const Signature &sig, DataNode *&memory, 
					const VariableOrConstant &function, const Method *method, 
					const VariableOrConstant *args, VariableOrConstant *results,
					ControlNode &container, Uint32 bci) 
{
	Pool &pool = container.getPrimitivePool();
	uint nArguments = sig.nArguments;
	uint nResults = sig.getNResults();
	
	// We can't know the Method for dynamic function calls!
	assert(!method || function.isConstant()); 

	PrimCall &prim = *new(pool) PrimCall(poCall, method, nArguments,
										 nResults, pool, bci); 
	container.appendPrimitive(prim);

	prim.getIncomingMemory().setVariable(*memory);
	prim.getFunction() = function;
	DataConsumer *inputs = prim.getArguments();
  #ifdef DEBUG
	const Type **argumentTypes = sig.argumentTypes;
  #endif
	while (nArguments--) {
	  #ifdef DEBUG
		assert(args->hasKind(typeKindToValueKind((*argumentTypes)->typeKind)));
		argumentTypes++;
	  #endif
		*inputs++ = *args++;
	}

	PrimProj *proj = new(pool) PrimProj(poProj_M, tcMemory, true, bci);
	container.appendPrimitive(*proj);
	proj->getInput().setVariable(prim);
	memory = proj;
	if (nResults) {
		proj = new(pool)
			PrimProj(typeKindToValueKind(sig.resultType->typeKind),
					 tcFirstResult, false, bci); 
		container.appendPrimitive(*proj);
		proj->getInput().setVariable(prim);
		results->setVariable(*proj);
	}

	return prim;
}

