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

#ifndef _VIRTUAL_REGISTER_H_
#define _VIRTUAL_REGISTER_H_

#include "Fundamentals.h"
#include "DoublyLinkedList.h"
#include "FastBitSet.h"
#include "FastBitMatrix.h"

class VirtualRegisterManager;
class VirtualRegister;
class Instruction;


/*
 *--------------------------VirtualRegister.h----------------------------
 *
 * enum VRClass --
 *
 *-----------------------------------------------------------------------
 */
enum VRClass
{
  InvalidRegisterKind,
  IntegerRegister,
  FloatingPointRegister,
  FixedPointRegister,
  MemoryRegister,
  StackSlotRegister,
  MemoryArgumentRegister,
  
  nVRClass
};

const VRClass vrcUnspecified = InvalidRegisterKind;
const VRClass vrcInteger = IntegerRegister;
const VRClass vrcFloatingPoint = FloatingPointRegister;
const VRClass vrcFixedPoint = FixedPointRegister;
const VRClass vrcStackSlot = StackSlotRegister;

typedef VRClass VirtualRegisterKind;


/*
 *--------------------------VirtualRegister.h----------------------------
 *
 * class VirtualRegisterPtr --
 *
 *-----------------------------------------------------------------------
 */
class VirtualRegisterPtr : public DoublyLinkedEntry<VirtualRegisterPtr>
{
  friend class VirtualRegisterManager;
  friend class VirtualRegister;

private:

  VirtualRegister* virtualRegisterAddress;
  VRClass          registerClassConstraint;

  inline VirtualRegister* link(VirtualRegister* vReg);
  inline VirtualRegister* unlink();

public:

#if DEBUG
  /*
   * Copying a VirtualRegisterPtr is forbidden.
   */
  VirtualRegisterPtr(const VirtualRegisterPtr& vRegPtr) {PR_ASSERT(0);}
  void operator = (const VirtualRegisterPtr& vRegPtr) {PR_ASSERT(0);}
#endif

  /*
   * Constructors & destructors.
   */
  inline VirtualRegisterPtr() : virtualRegisterAddress(NULL), registerClassConstraint(vrcInteger) {}
  ~VirtualRegisterPtr() {unlink();}
  
  /*
   * Initialization.
   */
  inline bool isInitialized() {return (virtualRegisterAddress != NULL) ? true : false;}
  inline void initialize(VirtualRegister& vReg, VRClass constraint = vrcInteger);
  inline void uninitialize() {unlink(); DEBUG_ONLY(registerClassConstraint = vrcUnspecified;)}

  /*
   * Getting the virtual registers or the register class constraint.
   */
  inline VirtualRegister& getVirtualRegister() {return *virtualRegisterAddress;}
  inline VRClass getClassConstraint() {return registerClassConstraint;}
  inline void setClassConstraint(VRClass constraint) {registerClassConstraint = constraint;}
};


/*
 *--------------------------VirtualRegister.h----------------------------
 *
 * class VirtualRegister --
 *
 *-----------------------------------------------------------------------
 */
class VirtualRegister
{
  friend class VirtualRegisterManager;

private:

  DoublyLinkedList<VirtualRegisterPtr> virtualRegisterPtrs;
  Instruction*                         definingInstruction;

  VirtualRegisterManager&              registerManager;
  PRUint32                             registerIndex;
  VRClass                              registerClass;

  bool                                 isMachineRegister;

  /*
   * Copying a VirtualRegister is forbidden.
   */
  VirtualRegister(const VirtualRegister& vReg);
  void operator = (const VirtualRegister& vReg);

  VirtualRegisterPtr lock;
#if DEBUG
  bool isLocked;
#endif

  /*
   * Constructors.
   */
  VirtualRegister(VirtualRegisterManager& myManager, PRUint32 myIndex, VRClass myClass = vrcInteger);

public:

  VirtualRegister* equivalentRegister[nVRClass];
  FastBitSet       specialInterference;
  FastBitSet       liveness;
  bool             hasSpecialInterference;
#ifdef DEBUG
  bool             isAnnotated;
#endif

  /*
   * Spilling & Coloring informations.
   */
  struct
  {
	Flt32        nLoads;
	Flt32        nStores;
	Flt32        nCopies;
	Uint32       liveLength;

	Flt32        spillCost;
	bool         infiniteSpillCost;
	bool         willSpill;

	Instruction* lastUsingInstruction;	
  } spillInfo;

  struct 
  {
	PRUint32   interferenceDegree;
	PRUint8    color;
	PRUint8    preColor;
	PRUint32   preColoredRegisterIndex;
	bool       isPreColored;
  } colorInfo;

  /*
   * For Code Generation
   */
  inline void setDefiningInstruction(Instruction& insn) {definingInstruction = &insn;}
  inline Instruction* getDefiningInstruction() {return definingInstruction;}

  /*
   * Setting the color.
   */
  inline void preColorRegister(PRUint8 preColor);
  inline bool isPreColored() {return colorInfo.isPreColored;}
  inline PRUint8 getPreColor() {PR_ASSERT(isPreColored()); return colorInfo.preColor;}

  inline void colorRegister(PRUint8 color) {colorInfo.color = color;}
  inline PRUint8 getColor() {return colorInfo.color;}

  void resetColoringInfo();

  inline void setClass(VRClass myClass) {registerClass = myClass;}
  inline VRClass getClass() {return registerClass;}

  /*
   * Getting the register's variables.
   */
  inline bool isReferenced() {return !virtualRegisterPtrs.empty();}
  inline DoublyLinkedList<VirtualRegisterPtr>& getVirtualRegisterPtrs() {return virtualRegisterPtrs;}
  inline PRUint32 getRegisterIndex() {return registerIndex;}
  inline VirtualRegisterManager& getManager() {return registerManager;}

  // check for real use of these methods !.
  inline VirtualRegister& getAlias() {return *equivalentRegister[registerClass];}
  inline void setAlias(VirtualRegister& alias) {equivalentRegister[registerClass] = &alias;}

  // lock
  inline void retainSelf() {PR_ASSERT(!isLocked); lock.initialize(*this); DEBUG_ONLY(isLocked = true);}
  inline void releaseSelf() {PR_ASSERT(isLocked); lock.uninitialize(); DEBUG_ONLY(isLocked = false);}
};


/*
 *--------------------------VirtualRegister.h----------------------------
 *
 * class VirtualRegisterManager --
 *
 *-----------------------------------------------------------------------
 */
class VirtualRegisterManager
{
private:

  VirtualRegister** virtualRegisters;
  VirtualRegister** machineRegisters;

  PRUint32 nAllocatedRegisters;
  PRUint32 firstFreeRegisterIndex;
  PRUint32 size;

  void resize(PRUint32 amount);
  void freeVirtualRegister(PRUint32 index);

public:

  Pool&             pool;

  typedef PRUint32 iterator;

#if 0
  struct 
  {
#endif
	PRUint8 nUsedStackSlots;
	PRUint8 nUsedCalleeSavedRegisters;
#if 0
  } RegisterUsageInfo;
#endif

  VirtualRegisterManager(Pool& p);

  VirtualRegister& newVirtualRegister(VRClass cl = vrcInteger);
  inline VirtualRegister& getVirtualRegister(PRUint32 index);
  VirtualRegister& getMachineRegister(PRUint8 color);

  void checkForVirtualRegisterDeath(PRUint32 index);

  void compactMemory();
  void moveVirtualRegister(PRUint32 source, PRUint32 dest);

  inline iterator begin() {return advance(~0);}
  inline bool done(iterator i) {return (i >= size) ? true : false;}
  inline iterator advance(iterator i);

  inline PRUint32 count() {return size;}
};


/*
 *--------------------------VirtualRegister.h----------------------------
 *
 * Inlines
 *
 *-----------------------------------------------------------------------
 */

inline void VirtualRegister::
preColorRegister(PRUint8 preColor)
{
#ifdef DEBUG
	if (isAnnotated)
		trespass("This is not a temporary register. It cannot be preColored. !!!");
#endif

  colorInfo.isPreColored = true; 
  colorInfo.preColor = preColor; 
  colorInfo.preColoredRegisterIndex = registerManager.getMachineRegister(preColor).getRegisterIndex();
}

inline VirtualRegister& VirtualRegisterManager::
getVirtualRegister(PRUint32 index)
{
  PR_ASSERT((index < size) && virtualRegisters[index]); 
  return *virtualRegisters[index];
}

/*
 * Return the next valid VirtualRegister index.
 */
inline VirtualRegisterManager::iterator VirtualRegisterManager::
advance(VirtualRegisterManager::iterator i)
{
  i++;
  while (i < size)
	{
	  if ((virtualRegisters[i] != NULL) && !virtualRegisters[i]->isMachineRegister)
		break;
	  i++;
	}
  return i;
}

/*
 * VirtualRegister constructor
 */
inline VirtualRegister::
VirtualRegister(VirtualRegisterManager& myManager, PRUint32 myIndex, VRClass myClass) :
  registerManager(myManager), registerIndex(myIndex), registerClass(myClass), isMachineRegister(false), 
  specialInterference(myManager.pool), liveness(myManager.pool)
{
  colorInfo.isPreColored = false;
  colorInfo.color = 0;
  spillInfo.willSpill = false;
  spillInfo.spillCost = 0;
  hasSpecialInterference = false;
#if DEBUG
  colorInfo.color = 0;
  definingInstruction = NULL;
  isLocked = false;
  isAnnotated = false;
#endif
}

/*
 * Add this VirtualRegisterPtr to the VirtualRegister's owners list.
 */
inline VirtualRegister* VirtualRegisterPtr::
link(VirtualRegister* vReg)
{
  PR_ASSERT(vReg != NULL);
  vReg->getVirtualRegisterPtrs().addLast(*this);
  return vReg;
}

/*
 * Remove this VirtualRegisterPtr from the VirtualRegister's owners list
 */
inline VirtualRegister* VirtualRegisterPtr::
unlink()
{
  if (virtualRegisterAddress != NULL) 
	{
	  remove(); 
	  virtualRegisterAddress->getManager().checkForVirtualRegisterDeath(virtualRegisterAddress->getRegisterIndex());
	  virtualRegisterAddress = NULL;
	}
  return virtualRegisterAddress;
}


inline void VirtualRegisterPtr::
initialize(VirtualRegister& vReg, VRClass constraint)
{
  unlink();
  virtualRegisterAddress = link(&vReg); 
  registerClassConstraint = constraint;
}

#endif /* _VIRTUAL_REGISTER_H_ */
