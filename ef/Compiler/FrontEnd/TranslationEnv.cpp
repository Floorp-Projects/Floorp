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
#include "BytecodeGraph.h"


// ----------------------------------------------------------------------------
// TranslationCommonEnv


//
// Create a new, empty phi node in the block's first control node.
// nExpectedInputs is the estimated number of inputs that the phi node will
// eventually have; however, there is no guarantee that the actual number of
// inputs won't be higher or lower.
// This method disengages the phi nodes in the block's first control node
// and must be balanced by a call to finishPhiNode.
//
PhiNode *TranslationCommonEnv::genEmptyPhiNode(const BasicBlock &block, ValueKind kind, Uint32 nExpectedInputs) const
{
	PhiNode *phiNode = new(primitivePool) PhiNode(nExpectedInputs, kind, primitivePool);
	ControlNode *cn = block.getFirstControlNode();
	assert(cn);
	cn->disengagePhis();
	cn->addPhiNode(*phiNode);
	return phiNode;
}


//
// Balance a call to genEmptyPhiNode.  The caller should have set up the
// (formerly) empty phi node's inputs by now.
//
inline void TranslationCommonEnv::finishPhiNode(const BasicBlock &block) const
{
	ControlNode *cn = block.getFirstControlNode();
	assert(cn);
	cn->reengagePhis();
}


// ----------------------------------------------------------------------------
// TranslationBinding


//
// Create the real constant primitive node for this Constant.  Allocate the
// primitive node out of the given pool.
//
void TranslationBinding::Constant::genRealNode(Pool &primitivePool, Uint32 bci)
{
	assert(!producerExists);

	PrimConst *primConst = new(primitivePool) PrimConst(kind, value, bci);
	placeForDataNode->appendPrimitive(*primConst);
	producerExists = true;
	dataNode = primConst;
}


//
// Create a new PhantomPhi record with room for size arguments.  Ouf of these,
// set the first nInitialArgs (which may be zero) to initialArgs, and set the
// next argument to arg.  Clearly, nInitialArgs must be less than size.
// container is the BasicBlock in which this phantom phi node is defined.
// If alwaysNonzero is true, the value of the PhantomPhi is known to never be
// zero.
// Allocate needed storage from pool.
//
TranslationBinding::PhantomPhi::PhantomPhi(ValueKind kind, Uint32 size, Uint32 nInitialArgs, const TranslationBinding &initialArgs,
		const TranslationBinding &arg, const BasicBlock &container, bool alwaysNonzero, Pool &pool):
	container(container),
	nArgs(nInitialArgs + 1),
	realPhiExists(false),
	kind(kind),
	alwaysNonzero(alwaysNonzero)
{
	TranslationBinding *a = new(pool) TranslationBinding[size];
	args = a;
	while (nInitialArgs--)
		*a++ = initialArgs;
	*a = arg;
}


//
// Return true if the value of this PhantomPhi is known to never be zero.
//
bool TranslationBinding::PhantomPhi::isAlwaysNonzero() const
{
	return realPhiExists ? realPhi->isAlwaysNonzero() : alwaysNonzero;
}


//
// Inform this PhantomPhi about whether its value is never zero.
//
void TranslationBinding::PhantomPhi::setAlwaysNonzero(bool nz)
{
	if (realPhiExists)
		realPhi->setAlwaysNonzero(nz);
	else
		alwaysNonzero = nz;
}


//
// Append a new argument arg to this PhantomPhi's array.  If expand is true,
// the array first needs to be physically expanded to size newSize (which is
// guaranteed to hold all of its elements).
//
void TranslationBinding::PhantomPhi::append(bool expand, Uint32 newSize,
											const TranslationBinding &arg,
											Uint32 bci) 
{
	if (realPhiExists) {
		Pool &primitivePool = container.getTranslationEnvIn().getCommonEnv().primitivePool;
		realPhi->addInput(arg.extract(primitivePool, bci), primitivePool);
	} else {
		Uint32 n = nArgs;
		if (expand) {
			TranslationBinding *newArgs = new(container.getTranslationEnvIn().envPool()) TranslationBinding[newSize];
			copy(args, args + n, newArgs);
			args = newArgs;
			assert(newSize > n);
		}
		args[n] = arg;
		nArgs = n + 1;
		if (alwaysNonzero)
			alwaysNonzero = arg.isAlwaysNonzero();
	}
}


//
// Create the real phi node for this PhantomPhi.
//
void TranslationBinding::PhantomPhi::genRealPhi(Uint32 bci)
{
	assert(!realPhiExists);
	TranslationBinding *a = args;
	const TranslationEnv &env = container.getTranslationEnvIn();
	const TranslationCommonEnv &commonEnv = env.getCommonEnv();
	Pool &primitivePool = commonEnv.primitivePool;
	Uint32 n = getNArgs();

	// Carefully create a new phi node before calling extract on the arguments
	// because one of the arguments could refer back to this phi node.
	realPhi = commonEnv.genEmptyPhiNode(container, kind, env.getPhiSize());
	realPhiExists = true;
	while (n--)
		realPhi->addInput(a++->extract(primitivePool, bci), primitivePool);
	commonEnv.finishPhiNode(container);
}


//
// Return the kind of value represented by this binding.
//
ValueKind TranslationBinding::getKind() const
{
	switch (category) {
		case tbConstant:
			return constant->kind;
		case tbDataEdge:
			return dataNode->getKind();
		case tbPhantomPhi:
		case tbConstantPhi:
			return phantomPhi->kind;
		default:
			return vkVoid;
	}
}


//
// Return true if the value of this binding is known to never be zero.
//
bool TranslationBinding::isAlwaysNonzero() const
{
	switch (category) {
		case tbConstant:
			return constant->isNonzero();
		case tbDataEdge:
			return dataNode->isAlwaysNonzero();
		case tbPhantomPhi:
		case tbConstantPhi:
			return phantomPhi->isAlwaysNonzero();
		default:
			return true;
	}
}


//
// Bind this TranslationBinding to represent the given int constant.
// If a primitive to explicitly generate the constant is needed, it will
// be placed in the given ControlNode.
// Allocate needed storage from pool.
//
void TranslationBinding::defineInt(Int32 v, ControlNode *placeForDataNode, Pool &pool)
{
	category = tbConstant;
	Constant *c = new(pool) Constant(vkInt, placeForDataNode);
	c->value.i = v;
	constant = c;
}


//
// Bind this TranslationBinding to represent the first word of the given long constant.
// If a primitive to explicitly generate the constant is needed, it will
// be placed in the given ControlNode.
// Allocate needed storage from pool.
//
void TranslationBinding::defineLong(Int64 v, ControlNode *placeForDataNode, Pool &pool)
{
	category = tbConstant;
	Constant *c = new(pool) Constant(vkLong, placeForDataNode);
	c->value.l = v;
	constant = c;
}


//
// Bind this TranslationBinding to represent the given float constant.
// If a primitive to explicitly generate the constant is needed, it will
// be placed in the given ControlNode.
// Allocate needed storage from pool.
//
void TranslationBinding::defineFloat(Flt32 v, ControlNode *placeForDataNode, Pool &pool)
{
	category = tbConstant;
	Constant *c = new(pool) Constant(vkFloat, placeForDataNode);
	c->value.f = v;
	constant = c;
}


//
// Bind this TranslationBinding to represent the first word of the given double constant.
// If a primitive to explicitly generate the constant is needed, it will
// be placed in the given ControlNode.
// Allocate needed storage from pool.
//
void TranslationBinding::defineDouble(Flt64 v, ControlNode *placeForDataNode, Pool &pool)
{
	category = tbConstant;
	Constant *c = new(pool) Constant(vkDouble, placeForDataNode);
	c->value.d = v;
	constant = c;
}


//
// Bind this TranslationBinding to represent the given pointer constant.
// If a primitive to explicitly generate the constant is needed, it will
// be placed in the given ControlNode.
// Allocate needed storage from pool.
//
void TranslationBinding::definePtr(addr v, ControlNode *placeForDataNode, Pool &pool)
{
	category = tbConstant;
	Constant *c = new(pool) Constant(vkAddr, placeForDataNode);
	c->value.a = v;
	constant = c;
}


//
// Bind this TranslationBinding to represent the constant (or first word of the constant
// if it takes two words).
// If a primitive to explicitly generate the constant is needed, it will
// be placed in the given ControlNode.
// Allocate needed environment storage from pool.
//
void TranslationBinding::define(ValueKind kind, const Value &v, ControlNode *placeForDataNode, Pool &pool)
{
	category = tbConstant;
	Constant *c = new(pool) Constant(kind, placeForDataNode);
	c->value = v;
	constant = c;
}


// ----------------------------------------------------------------------------
// TranslationEnv


#ifdef DEBUG
//
// Assert that none of the PhantomPhi or ConstantPhi nodes referring to the current container
// are shared.
//
void TranslationEnv::assertNoSharedPhantomPhis(const BasicBlock &container)
{
	TranslationBinding *dst = bindingsBegin();
	TranslationBinding *dstEnd = bindingsEnd();
	for (; dst != dstEnd; dst++)
		if (dst->category == TranslationBinding::tbPhantomPhi || dst->category == TranslationBinding::tbConstantPhi)
			dst->phantomPhi->duplicate = false;
	for (dst = bindingsBegin(); dst != dstEnd; dst++)
		if (dst->category == TranslationBinding::tbPhantomPhi || dst->category == TranslationBinding::tbConstantPhi) {
			TranslationBinding::PhantomPhi *phi = dst->phantomPhi;
			assert(&phi->getContainer() != &container || !phi->duplicate);
			phi->duplicate = true;
		}
}
#endif


//
// Intersect the given TranslationEnv (which must have been initialized)
// into this TranslationEnv, which must also be initialized.
// This environment must be part of the given container.
// If memoryOnly is true, only intersect the memory binding.
//
void TranslationEnv::meet(const TranslationEnv &env, bool memoryOnly, const BasicBlock &container)
{
  #ifdef DEBUG
	assert(this == &container.getTranslationEnvIn());
	assert(compatible(env));
	assertNoSharedPhantomPhis(container);
  #endif
	TranslationBinding *dst = bindingsBegin();
	TranslationBinding *dstEnd = memoryOnly ? bindingsMemoryEnd() : bindingsEnd();
	const TranslationBinding *src = env.bindingsBegin();
	Pool &pool = commonEnv.envPool;
	Uint32 oldPhiOffset = phiOffset;
	Uint32 newPhiSize = phiSize;
	bool expandThis = false;
	if (oldPhiOffset >= newPhiSize) {
		newPhiSize = oldPhiOffset*2 + 1;
		expandThis = true;
	}

	while (dst != dstEnd) {
		TranslationBinding::Category cDst = dst->category;
		TranslationBinding::Category cSrc = src->category;
		ValueKind kind;

		if (cSrc == TranslationBinding::tbNone) {
			assert(!anticipated || cDst == TranslationBinding::tbNone);
			dst->clear();
		} else
			switch (cDst) {

				case TranslationBinding::tbNone:
					break;

				case TranslationBinding::tbSecondWord:
					if (cSrc != TranslationBinding::tbSecondWord) {
						assert(!anticipated);
						dst->clear();
					}
					break;

				case TranslationBinding::tbConstantPhi:
					{
						TranslationBinding::ConstantPhi *phi = dst->constantPhi;
						kind = phi->kind;
						if (src->getKind() != kind) {
							assert(!anticipated);
							dst->clear();
							break;
						}
						const TranslationBinding::Constant *srcConstant = src->constantValue();
						bool constantMatches = srcConstant && *phi->constant == *srcConstant;

						if (&phi->getContainer() == &container)
							if (constantMatches)
								// asharma - fix this!
								phi->append(expandThis, newPhiSize, *src, 0);
							else {
								// The constant doesn't match, so turn this into a regular phantom phi node.
								assert(!anticipated);
								dst->category = TranslationBinding::tbPhantomPhi;
								goto phantomPhi;
							}
						else
							// Don't merge if both source and destination are the same and they don't refer to
							// this container.
							if (cSrc != TranslationBinding::tbConstantPhi || src->constantPhi != phi)
								if (constantMatches) {
									assert(!anticipated);
									dst->define(new(pool) TranslationBinding::ConstantPhi(kind, newPhiSize, oldPhiOffset,
																						  *dst, *src, container, srcConstant, pool));
								} else
									goto createPhi;
					}
					break;

				case TranslationBinding::tbPhantomPhi:
					if (src->getKind() != dst->phantomPhi->kind) {
						assert(!anticipated);
						dst->clear();
						break;
					}
				phantomPhi:
					{
						TranslationBinding::PhantomPhi *phi = dst->phantomPhi;

						if (&phi->getContainer() == &container) {
							// Use existing phantom phi node in this node
							assert(!phi->isAlwaysNonzero() || src->isAlwaysNonzero() || !anticipated);
							// asharma - fix this!
							phi->append(expandThis, newPhiSize, *src, 0);
							break;
						}

						// Don't merge if both source and destination are the same and they don't refer to
						// this container.
						if (cSrc != TranslationBinding::tbPhantomPhi || src->phantomPhi != phi) {
							kind = phi->kind;
							goto createPhi;
						}
					}
					break;

				case TranslationBinding::tbDataEdge:
					if (cSrc != TranslationBinding::tbDataEdge || src->dataNode != dst->dataNode) {
						kind = dst->dataNode->getKind();
						if (src->getKind() != kind) {
							assert(!anticipated);
							dst->clear();
							break;
						}
					createPhi:
						assert(!anticipated);
						dst->define(new(pool) TranslationBinding::PhantomPhi(kind, newPhiSize, oldPhiOffset, *dst, *src, container,
																			 dst->isAlwaysNonzero() && src->isAlwaysNonzero(), pool));
					}
					break;

				case TranslationBinding::tbConstant:
					if (cSrc != TranslationBinding::tbConstant || src->constant != dst->constant) {
						kind = dst->constant->kind;
						if (src->getKind() != kind) {
							assert(!anticipated);
							dst->clear();
							break;
						}
						const TranslationBinding::Constant *srcConstant = src->constantValue();
						if (srcConstant && *dst->constant == *srcConstant) {
							dst->define(new(pool) TranslationBinding::ConstantPhi(kind, newPhiSize, oldPhiOffset,
																				  *dst, *src, container, srcConstant, pool));
							break;
						}
						goto createPhi;
					}
			}
		src++;
		dst++;
	}
	phiOffset = oldPhiOffset + 1;
	phiSize = newPhiSize;
}


//
// Add phantom phi nodes to all variables in this local environment because we'd like
// to use this environment without knowing all of its predecessors yet.
// This environment must be part of the given container.
//
void TranslationEnv::anticipate(const BasicBlock &container)
{
  #ifdef DEBUG
	assert(this == &container.getTranslationEnvIn());
	assertNoSharedPhantomPhis(container);
	assert(!anticipated);
	anticipated = true;
  #endif
	TranslationBinding *dst = bindingsBegin();
	TranslationBinding *dstEnd = bindingsEnd();
	Pool &pool = commonEnv.envPool;

	while (dst != dstEnd) {
		switch (dst->category) {
			case TranslationBinding::tbNone:
			case TranslationBinding::tbSecondWord:
				break;
			case TranslationBinding::tbConstantPhi:
				if (&dst->constantPhi->getContainer() != &container)
					goto createPhi;
				dst->constantPhi->setAlwaysNonzero(false);
				dst->category = TranslationBinding::tbPhantomPhi;
				break;
			case TranslationBinding::tbPhantomPhi:
				if (&dst->phantomPhi->getContainer() == &container)
					break; // We already have an appropriate phi node.
				// Fall into default case because we need to create a new phi node; the current
				// value was a phi node from another control node.
			default:
			createPhi:
				assert(phiOffset > 0 && phiSize >= phiOffset);
				dst->define(new(pool) TranslationBinding::PhantomPhi(dst->getKind(), phiSize, phiOffset-1,
																	 *dst, *dst, container, false, pool));
				break;
		}
		dst++;
	}
}
