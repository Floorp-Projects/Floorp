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

#ifndef _SPILLING_H_
#define _SPILLING_H_

#include "Fundamentals.h"
#include "VirtualRegister.h"
#include "Instruction.h"
#include "Fifo.h"
#include "CpuInfo.h"

class ControlNode;

class Spilling
{
protected:

  VirtualRegisterManager& vRegManager;
  MdEmitter&     emitter;

public:

  Spilling(VirtualRegisterManager& vrMan, MdEmitter& emit) : vRegManager(vrMan), emitter(emit) {}

  void insertSpillCode(ControlNode** dfsList, Uint32 nNodes);
};

class RegisterFifo : public Fifo<Uint32>
{
private:
  FastBitSet bitset;
public:
  RegisterFifo(Uint32 size) : Fifo<Uint32>(size), bitset(size) {}
  Uint32 get();
  void put(Uint32 r);
  bool test(Uint32 r) {return bitset.test(r);}
};

inline Uint32
RegisterFifo::get()
{
  Uint32 r = Fifo<Uint32>::get();
  bitset.clear(r);
  return r;
}

inline void
RegisterFifo::put(Uint32 r)
{
  Fifo<Uint32>::put(r);
  bitset.set(r);
}

#endif /* _SPILLING_H_ */
