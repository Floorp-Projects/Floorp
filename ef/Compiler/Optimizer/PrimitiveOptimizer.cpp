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
#include "PrimitiveOptimizer.h"
#include "ControlGraph.h"

//
// Quickly trim primitives whose results are not used from the graph.
// This is a quick-and-dirty implementation that misses some opportunities;
// for example, cycles of dead primitives are not detected.
//
void simpleTrimDeadPrimitives(ControlGraph &cg)
{
	cg.dfsSearch();
	ControlNode **dfsList = cg.dfsList;
	ControlNode **pcn = dfsList + cg.nNodes;

	while (pcn != dfsList) {
		ControlNode &cn = **--pcn;

		// Search the primitives backwards; remove any whose outputs are not used
		// and which are not roots.
		DoublyLinkedList<Primitive> &primitives = cn.getPrimitives();
		DoublyLinkedList<Primitive>::iterator primIter = primitives.end();
		while (!primitives.done(primIter)) {
			Primitive &p = primitives.get(primIter);
			primIter = primitives.retreat(primIter);
			if (!p.isRoot() && !p.hasConsumers())
				p.destroy();
		}

		// Search the phi nodes backwards; remove any whose outputs are not used.
		DoublyLinkedList<PhiNode> &phiNodes = cn.getPhiNodes();
		DoublyLinkedList<PhiNode>::iterator phiIter = phiNodes.end();
		while (!phiNodes.done(phiIter)) {
			PhiNode &n = phiNodes.get(phiIter);
			phiIter = phiNodes.retreat(phiIter);
			if (!n.hasConsumers())
				n.remove();
		}
	}
}

