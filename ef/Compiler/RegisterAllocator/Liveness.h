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

#ifndef _LIVENESS_H_
#define _LIVENESS_H_

#include "Fundamentals.h"
#include "ControlGraph.h"

class Pool;
class FastBitSet;
class VirtualRegisterManager;

class Liveness
{
private:
	Pool&					pool;					// Pool for local memory allocation.
	FastBitSet*				liveIn;					// Array of LiveIn bitsets
	FastBitSet*				liveOut;				// Array of LiveOut bitsets
	Uint32					size;					// Size of the arrays.

private:

	void buildAcyclicLivenessAnalysis(const ControlGraph& controlGraph, const VirtualRegisterManager& vrManager);
	void buildCyclicLivenessAnalysis(const ControlGraph& controlGraph, const VirtualRegisterManager& vrManager);

	void operator = (const Liveness&);				// No copy operator.
	Liveness(const Liveness&); 						// No copy constructor.

public:

	Liveness(Pool& pool) : pool(pool) {}

	inline void buildLivenessAnalysis(const ControlGraph& controlGraph, const VirtualRegisterManager& vrManager);
	FastBitSet& getLiveIn(Uint32 blockNumber) {PR_ASSERT(blockNumber < size); return liveIn[blockNumber];}
	FastBitSet& getLiveOut(Uint32 blockNumber) {PR_ASSERT(blockNumber < size); return liveOut[blockNumber];}

	DEBUG_LOG_ONLY(void printPretty(FILE* f));
};

inline void Liveness::buildLivenessAnalysis(const ControlGraph& controlGraph, const VirtualRegisterManager& vrManager)
{
	if (controlGraph.hasBackEdges)
		buildCyclicLivenessAnalysis(controlGraph, vrManager);
	else
		buildAcyclicLivenessAnalysis(controlGraph, vrManager);
}

#endif // _LIVENESS_H_
