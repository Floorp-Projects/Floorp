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

#ifndef _CG_SCHEDULER_H_
#define _CG_SCHEDULER_H_

#include "Fundamentals.h"
#include "Pool.h"

class ControlGraph;
class ControlNode;
class InstructionEmitter;

class ControlGraphScheduler
{
protected:
  ControlGraph& cg;
  InstructionEmitter& emitter;

  ControlNode** orderNodes();
  void addAbsoluteBranches(ControlNode** orderedNodes);

public:
  ControlGraphScheduler(ControlGraph& mCg, InstructionEmitter& mEmitter) : cg(mCg), emitter(mEmitter) {}
  
  ControlNode** scheduleNodes();
};

#endif /* _CG_SCHEDULER_H_ */
