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

#ifndef _COLORING_H_
#define _COLORING_H_

#include "Fundamentals.h"
#include "VirtualRegister.h"
#include "RegisterAssigner.h"
#include "Pool.h"

class FastBitSet;
class FastBitMatrix;

class Coloring : public RegisterAssigner
{
private:
  Pool& pool;

protected:
  PRInt32 getLowestSpillCostRegister(FastBitSet& bitset);
  PRUint32* simplify(FastBitMatrix interferenceMatrix, PRUint32* stackPtr);
  bool select(FastBitMatrix& interferenceMatrix, PRUint32* stackBase, PRUint32* stackPtr);

public:
  Coloring(Pool& p, VirtualRegisterManager& vrMan) : RegisterAssigner(vrMan), pool(p) {}

  virtual bool assignRegisters(FastBitMatrix& interferenceMatrix);
};

#endif /* _COLORING_H_ */
