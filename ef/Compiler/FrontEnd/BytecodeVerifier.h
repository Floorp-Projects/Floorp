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
#ifndef BYTECODEVERIFIER_H
#define BYTECODEVERIFIER_H

#include "BytecodeGraph.h"
#include "Tree.h"


// A data structure for keeping track of all call sites for each subroutine.
// Only includes mappings for skJsr call sites; skJsrNoRet call sites are deleted
// from this association list as soon as they are discovered not to have a ret.
class SubroutineCallSiteList
{
	struct Association
	{
		BytecodeBlock &subroutine;				// A subroutine (never nil) ...
		BytecodeBlock *const callSite;			// ... and one of its call sites (may be nil)

		Association(BytecodeBlock &subroutine, BytecodeBlock &callSite): subroutine(subroutine), callSite(&callSite) {}
		explicit Association(BytecodeBlock &subroutine): subroutine(subroutine), callSite(0) {}

		bool operator<(const Association &a2) const;
	};

	class Node: public TreeNode<Node>
	{
		Association association;				// An association of a subroutine with one of its call sites

	  public:
		explicit Node(Association &association): association(association) {}
		const Association &getKey() const {return association;}
	};

  public:
	class Iterator
	{
		BytecodeBlock &subroutine;				// The subroutine whose call sites we're iterating
		Node *node;								// The node currently at the iterator's position or nil if none

	  public:
		Iterator(SubroutineCallSiteList &csList, BytecodeBlock &subroutine);
		bool more() const {return node != 0;}
		BytecodeBlock &operator*() const {assert(node); return *node->getKey().callSite;}
		void operator++();
	};

  private:
	SortedTree<Node, const Association &> tree;	// The actual container of all associations in this SubroutineCallSiteList
  public:

	void addAssociation(BytecodeBlock &subroutine, BytecodeBlock &callSite, Pool &pool);
	void removeAssociation(BytecodeBlock &subroutine, BytecodeBlock &callSite);
	BytecodeBlock *popAssociation(BytecodeBlock *&subroutine, bool &onlyOneCallSite);

	friend class Iterator;
};


class BytecodeVerifier: public VerificationEnv::Common
{
	BytecodeGraph &bytecodeGraph;				// The BytecodeGraph which we're examining
	ClassFileSummary &classFileSummary;			// The BytecodeGraph's ClassFileSummary
	SubroutineCallSiteList subroutineCallSiteList; // A mapping from subroutines to their call sites

	BytecodeVerifier(BytecodeGraph &bytecodeGraph, Pool &envPool);

	// Inlining subroutines
	void normalizeEnv(VerificationEnv &env, BasicBlock::StackNormalization stackNormalization);
	bool predecessorChanged(BasicBlock &block, VerificationEnv &predecessorEnv, Uint32 generation, bool canOwnEnv);
	bool predecessorChanged(BasicBlock **blocksBegin, BasicBlock **blocksEnd, VerificationEnv &predecessorEnv,
							Uint32 generation, bool canOwnEnv);
	bool propagateDataflow(CatchBlock &block, Uint32 generation);
	bool propagateDataflow(BytecodeBlock &block, Uint32 generation);
	void computeDataflow();
	void computeRetReachables();
	void duplicateSubroutines();

  public:
	static void inlineSubroutines(BytecodeGraph &bytecodeGraph, Pool &tempPool);
};

#endif
