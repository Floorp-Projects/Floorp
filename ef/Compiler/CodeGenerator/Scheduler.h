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
// LinearInstructionScheduler.h
//
// Peter DeSantis 
// Scott M. Silver


// General Idea:
// preserves the src code ordering
// instructions.  It is the result of a topological sort of the control node from each primary
// root.  A primary root is one which is not reachable by any other root in the control node.
// When there is more than outgoing edge from a node, they are searched in increasing order of 
// their srcLine fields.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "CodeGenerator.h"
#include "Vector.h"

class LinearInstructionScheduler
{
public:  
  void			schedule(Vector<RootPair>& roots, ControlNode& controlNode);
  
protected:
  void			priorityTopoEmit(Instruction* inInstruction, ControlNode& controlNode);
  void          renumberCN(Vector<RootPair>& roots, ControlNode& controlNode);
  void          renumberPrimitive(Vector<RootPair>& roots, Primitive& p);
  void			linearSchedule(Vector<RootPair>& roots, ControlNode& controlNode);
};

#endif //SCHEDULER_H


