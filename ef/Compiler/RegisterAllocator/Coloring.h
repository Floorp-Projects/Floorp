/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
