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
#ifndef TRANSLATIONENV_H
#define TRANSLATIONENV_H

#include "ControlNodes.h"
#include "LocalEnv.h"

struct BasicBlock;


struct TranslationCommonEnv: CommonEnv
{
	Pool &primitivePool;						// Pool for allocating phi and constant data flow nodes

	PhiNode *genEmptyPhiNode(const BasicBlock &block, ValueKind kind, Uint32 nExpectedInputs) const;
	void finishPhiNode(const BasicBlock &block) const;

	TranslationCommonEnv(Pool &envPool, Pool &primitivePool, Uint32 nLocals, Uint32 stackSize):
		CommonEnv(envPool, 1, nLocals, stackSize), primitivePool(primitivePool) {}
};


class TranslationBinding
{
  public:
	enum Category
	{
		tbNone,			// Unknown contents
		tbSecondWord,	// This variable is the second word of a long or double
		tbConstant,		// This variable is a constant constVal defined in basic block definer
		tbDataEdge,		// This variable was defined by the given DataNode
		tbPhantomPhi,	// This variable was defined by the given phantom phi node
		tbConstantPhi	// This variable was defined by the given phantom phi node that always evaluates to a constant
	};

	static bool isPhi(Category c) {return c == tbPhantomPhi || c == tbConstantPhi;}


  private:
	struct Constant
	{
		const ValueKind kind ENUM_8;			// Kind of constant
		bool producerExists BOOL_8;				// If true, a data flow node exists for this constant and can be found
		//short unused SHORT_16;				//   using dataNode below.  If false, the data flow node can be created
		union {									//   in the placeForDataNode control node if needed.
			ControlNode *placeForDataNode;		// Control node inside which the constant was defined
			DataNode *dataNode;					// Link to data flow node that defined this constant
		};
		Value value;							// Constant's value

		Constant(ValueKind kind, ControlNode *placeForDataNode);

		bool operator==(const Constant &c) const {ValueKind k = kind; return k == c.kind && value.eq(k, c.value);}
		bool operator!=(const Constant &c) const {ValueKind k = kind; return k != c.kind || !value.eq(k, c.value);}

	  private:
		void genRealNode(Pool &primitivePool, Uint32 bci);
	  public:
		bool isNonzero() const {return value.isNonzero(kind);}
		DataNode &realize(Pool &primitivePool, Uint32 bci);
	};


	class PhantomPhi
	{
		const BasicBlock &container;			// Block in which this phantom phi node is defined
		Uint32 nArgs;							// Number of occupied arguments in the args array below (valid only when realPhiExists is false)
		bool realPhiExists BOOL_8;				// If true, a real phi node exists and can be found using realPhi below.
	  public:
		const ValueKind kind ENUM_8;			// Kind of value stored in this phi node
		DEBUG_ONLY(bool duplicate BOOL_8;)		// Flag used for assertNoSharedPhantomPhis
	  private:
		bool alwaysNonzero BOOL_8;				// If true, the value of this PhantomPhi is known to always be nonzero
		union {									//   (valid only if realPhiExists is false)
			TranslationBinding *args;			// Array of phiSize TranslationBinding records for the arguments of a phantom phi node
			PhiNode *realPhi;					// Real phi node if one has been generated
		};

		PhantomPhi(const PhantomPhi &);			// Copying forbidden
		void operator=(const PhantomPhi &);		// Copying forbidden
	  public:
		PhantomPhi(ValueKind kind, Uint32 size, Uint32 nInitialArgs, const TranslationBinding &initialArgs, const TranslationBinding &arg,
				const BasicBlock &container, bool alwaysNonzero, Pool &pool);

		const BasicBlock &getContainer() const {return container;}
		Uint32 getNArgs() const {assert(!realPhiExists); return nArgs;}
		bool isAlwaysNonzero() const;
		void setAlwaysNonzero(bool nz);
		void append(bool expand, Uint32 newSize, 
					const TranslationBinding &arg, Uint32 bci);
	  private:
		void genRealPhi(Uint32 bci);
	  public:
		DataNode &realize(Uint32 bci);
	};


	// A ConstantPhi is a PhantomPhi with the added restriction that all of its arguments are
	// either the same constant or other ConstantPhis of the same constant.
	struct ConstantPhi: PhantomPhi
	{
		const Constant *constant;				// Constant to which all of the arguments of this phantom phi node evaluate

		ConstantPhi(ValueKind kind, Uint32 size, Uint32 nInitialArgs, const TranslationBinding &initialArgs, const TranslationBinding &arg,
				const BasicBlock &container, const Constant *constant, Pool &pool);
	};


	Category category ENUM_8;					// TranslationBinding category (determines interpretation of union below)
	//bool unused BOOL_8;
	//short unused SHORT_16;
	union {
		Constant *constant;						// Constant value (Constant may be shared among different bindings)
		DataNode *dataNode;						// Place where this variable is defined
		PhantomPhi *phantomPhi;					// Phantom phi node (PhantomPhi may be shared among different bindings)
		ConstantPhi *constantPhi;				// Phantom phi node all of whose arguments evaluate to the same constant (may be shared)
		void *data;								// Used for comparing constNum, constAddr, etc.
		// Note:  The phantomPhi and constantPhi pointers from several different bindings in an environment
		// may point to a common PhantomPhi or ConstantPhi record under the following circumstances:
		//  1.  phantomPhi->container (or constantPhi->container) is not the current environment's container
		//  2.  All calls to meet and anticipate have completed (at this point sharing can be
		//        introduced, for instance, by assigning one local to another).
		// Sharing of PhantomPhi or ConstantPhi records in the current environment while meet or
		// anticipate is running would cause trouble because these routines extend the phantom phi arrays.
	};

  public:
	bool operator==(const TranslationBinding &b) const
		{return category == b.category && data == b.data;}
	bool operator!=(const TranslationBinding &b) const
		{return category != b.category || data != b.data;}

	bool isDataEdge() const { return category == tbDataEdge; }
	bool isSecondWord() const {return category == tbSecondWord;}
	const Constant *constantValue() const;
	ValueKind getKind() const;
	bool isAlwaysNonzero() const;

	void extract(VariableOrConstant &result, Uint32 bci) const;
	DataNode &extract(Pool &primitivePool, Uint32 bci) const;
	DataNode &extract(Uint32 bci) const;

	void clear();
	inline void defineSecondWord();
	void defineInt(Int32 v, ControlNode *placeForDataNode, Pool &pool);
	void defineLong(Int64 v, ControlNode *placeForDataNode, Pool &pool);
	void defineFloat(Flt32 v, ControlNode *placeForDataNode, Pool &pool);
	void defineDouble(Flt64 v, ControlNode *placeForDataNode, Pool &pool);
	void definePtr(addr v, ControlNode *placeForDataNode, Pool &pool);
	void define(ValueKind kind, const Value &v, ControlNode *placeForDataNode, Pool &pool);
	void define(DataNode &dataNode);
	void define(const VariableOrConstant &poc, ControlNode *placeForDataNode, Pool &pool);
  private:
	void define(PhantomPhi *p) {category = tbPhantomPhi; phantomPhi = p;}
	void define(ConstantPhi *p) {category = tbConstantPhi; constantPhi = p;}

	friend class TranslationEnv;
};


class TranslationEnv: public LocalEnv<TranslationBinding, TranslationCommonEnv, 1>
{
	Uint32 phiSize;								// Number of LocalBindings allocated in this environment's phantom phi bindings
	Uint32 phiOffset;							// Next index to be allocated in this environment's phantom phi bindings
  #ifdef DEBUG
	bool anticipated;							// True if anticipate has been called on this environment
  #endif

  public:
	TranslationEnv(TranslationCommonEnv &commonEnv);
	TranslationEnv(const TranslationEnv &env);
	void operator=(const TranslationEnv &env);
	void move(TranslationEnv &env);

	Uint32 getPhiSize() const {return phiSize;}
	const TranslationCommonEnv &getCommonEnv() const {return commonEnv;}

  #ifdef DEBUG
	void assertNoSharedPhantomPhis(const BasicBlock &container);
  #endif
	void meet(const TranslationEnv &env, bool memoryOnly, const BasicBlock &container);
	void anticipate(const BasicBlock &container);
};


// --- INLINES ----------------------------------------------------------------


//
// Initialize a Constant of the given kind.  The constant starts out with no
// real data flow node, but if one is needed, it will be created in placeForDataNode.
//
inline TranslationBinding::Constant::Constant(ValueKind kind, ControlNode *placeForDataNode):
	kind(kind),
	producerExists(false),
	placeForDataNode(placeForDataNode)
{
	assert(placeForDataNode);
	// Prevent this ControlNode from being recycled because a real data flow node could be
	// created in it at any time.
	placeForDataNode->inhibitRecycling();
}


//
// Create if necessary and return a real data flow node for this constant.
// Allocate the primitive node out of the given pool.
//
inline DataNode &TranslationBinding::Constant::realize(Pool &primitivePool,
													   Uint32 bci)
{
	if (!producerExists)
		genRealNode(primitivePool, bci);
	return *dataNode;
}


//
// Create if necessary and return the real phi node for this PhantomPhi.
//
inline DataNode &TranslationBinding::PhantomPhi::realize(Uint32 bci)
{
	if (!realPhiExists)
		genRealPhi(bci);
	return *realPhi;
}


//
// Create a new ConstantPhi record with room for size arguments.  Ouf of these,
// set the first nInitialArgs (which may be zero) to initialArgs, and set the
// next argument to arg.  Clearly, nInitialArgs must be less than size.
// container is the BasicBlock in which this constant phantom phi node is defined.
// Every argument of this ConstantPhi must evaluate to constant.
// Allocate needed storage from pool.
//
inline TranslationBinding::ConstantPhi::ConstantPhi(ValueKind kind, Uint32 size, Uint32 nInitialArgs, const TranslationBinding &initialArgs,
		const TranslationBinding &arg, const BasicBlock &container, const Constant *constant, Pool &pool):
	PhantomPhi(kind, size, nInitialArgs, initialArgs, arg, container, constant->isNonzero(), pool),
	constant(constant)
{}


//
// If this TranslationBinding would always evaluate to the same constant, return that
// constant; otherwise, return nil.
//
inline const TranslationBinding::Constant *TranslationBinding::constantValue() const
{
	switch (category) {
		case tbConstant:
			return constant;
		case tbConstantPhi:
			return constantPhi->constant;
		default:
			return 0;
	}
}


//
// If this TranslationBinding would always evaluate to the same constant, return that
// constant in result.  Otherwise, return the producer in result, creating phi
// nodes for the producer if necessary.
//
inline void TranslationBinding::extract(VariableOrConstant &result, 
										Uint32 bci) const
{
	const Constant *c;

	switch (category) {
		case tbConstant:
			c = constant;
			result.setConstant(c->kind, c->value);
			break;
		case tbDataEdge:
			result.setVariable(*dataNode);
			break;
		case tbPhantomPhi:
			result.setVariable(phantomPhi->realize(bci));
			break;
		case tbConstantPhi:
			c = constantPhi->constant;
			result.setConstant(c->kind, c->value);
			break;
		default:
			trespass("Can't extract this binding");
	}
}


//
// Return the producer for this TranslationBinding, creating phi nodes or
// constant nodes if necessary.  Create a producer even if it's just a
// constant.
//
inline DataNode &
TranslationBinding::extract(Pool &primitivePool, Uint32 bci) const
{
	switch (category) {
		case tbConstant:
			return constant->realize(primitivePool, bci);
		case tbPhantomPhi:
		case tbConstantPhi:
			return phantomPhi->realize(bci);
		case tbDataEdge:
			return *dataNode;
		default:
			trespass("Can't extract this binding");
			return *(DataNode *)0;
	}
}


//
// Return the producer for this TranslationBinding, creating phi nodes if
// necessary.  It is an error if the producer is a constant.
//
inline DataNode &TranslationBinding::extract(Uint32 bci) const
{
	switch (category) {
		case tbPhantomPhi:
		case tbConstantPhi:
			return phantomPhi->realize(bci);
		case tbDataEdge:
			return *dataNode;
		default:
			trespass("Can't extract this binding");
			return *(DataNode *)0;
	}
}


//
// Clear any previous binding from this TranslationBinding.
//
inline void TranslationBinding::clear()
{
	category = tbNone;
	data = 0;
}


//
// Bind this TranslationBinding to represent the second word of a long or double.
// This TranslationBinding won't carry any more descriptive information about that
// long or double -- all such information is carried by the first word's binding.
//
inline void TranslationBinding::defineSecondWord()
{
	category = tbSecondWord;
	data = 0;
}


//
// Bind this TranslationBinding to be the result (first word if it's a long or double)
// of the given DataNode.
//
inline void TranslationBinding::define(DataNode &dataNode)
{
	category = tbDataEdge;
	TranslationBinding::dataNode = &dataNode;
}


//
// Bind this TranslationBinding to be the result (first word if it's a long or double)
// of the given DataNode or Constant.
// Allocate needed environment storage from pool.
//
inline void TranslationBinding::define(const VariableOrConstant &poc, ControlNode *placeForDataNode, Pool &pool)
{
	if (poc.isConstant())
		define(poc.getKind(), poc.getConstant(), placeForDataNode, pool);
	else
		define(poc.getVariable());
}


// ----------------------------------------------------------------------------


const Uint32 initialPhiSize = 3;

//
// Create a new translation binding environment.
// The environment starts with one predecessor for the purpose of tracking
// the origins of phi nodes' entries.
//
inline TranslationEnv::TranslationEnv(TranslationCommonEnv &commonEnv):
	LocalEnv<TranslationBinding, TranslationCommonEnv, 1>(commonEnv),
	phiSize(initialPhiSize),
	phiOffset(1)
  #ifdef DEBUG
	, anticipated(false)
  #endif
{}


//
// Copy a translation binding environment.
// The copy starts with one predecessor for the purpose of tracking
// the origins of phi nodes' entries.
//
inline TranslationEnv::TranslationEnv(const TranslationEnv &env):
	LocalEnv<TranslationBinding, TranslationCommonEnv, 1>(env),
	phiSize(initialPhiSize),
	phiOffset(1)
  #ifdef DEBUG
	, anticipated(false)
  #endif
{}


//
// Assign the given translation binding environment to this one.
// The copy starts with one predecessor for the purpose of tracking
// the origins of phi nodes' entries.
//
inline void TranslationEnv::operator=(const TranslationEnv &env)
{
	phiSize = initialPhiSize;
	phiOffset = 1;
  #ifdef DEBUG
	anticipated = false;
  #endif
	LocalEnv<TranslationBinding, TranslationCommonEnv, 1>::operator=(env);
}


//
// Destructively move the given LocalEnv (which must have been initialized)
// to this LocalEnv, which must be uninitialized.  The given LocalEnv is
// left uninitialized.
// The moved environment is reset to have only one predecessor for the purpose
// of tracking the origins of phi nodes' entries.
//
inline void TranslationEnv::move(TranslationEnv &env)
{
	phiSize = initialPhiSize;
	phiOffset = 1;
  #ifdef DEBUG
	anticipated = false;
  #endif
	LocalEnv<TranslationBinding, TranslationCommonEnv, 1>::move(env);
}

#endif
