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
#ifndef InterfaceDispatchTable_H
#define InterfaceDispatchTable_H

/* Efficient mapping from {Class, Interface} to vtable offset */

#include "Vector.h"
#include "HashTable.h"

/* A type used to hold the offset, in bytes, into a vtable */
typedef Uint32 VTableOffset;

struct InterfaceVIndexPair {
  Uint32 interfaceNumber;      // Unique serial number of interface
  Uint32 baseVOffset;          // Offset, in bytes, to first entry in interface's vtable 
};
typedef Vector<InterfaceVIndexPair> InterfaceList;
typedef InterfaceList *InterfaceListPtr;

struct InterfaceListKeyOps {
  static bool equals(InterfaceList * userKey, InterfaceList * hashTableKey);

  static InterfaceList *copyKey(Pool &/*pool*/, InterfaceList* userKey) {
    return userKey;
  }

  static Uint32 hashCode(InterfaceList * userKey);
};

class InterfaceDispatchTable;
typedef InterfaceDispatchTable *InterfaceDispatchTablePtr;
typedef HashTable<InterfaceDispatchTablePtr, InterfaceListPtr, InterfaceListKeyOps> InterfaceSetHashTable;

class InterfaceDispatchTable {
public:
  /* Maximum number of simultaneously loaded interfaces in the entire VM.
     If we exceed this limit, we throw an error during compilation.
     This is only an enum because const class members suck in C++ */
  enum {maxInterfaces = 2048};
  
  /* Arbitrary upper bound for global lookup table sizes.
     If we blow out these limits, we throw an error during compilation.
     This is only an enum because const members suck in C++ */
  enum {interfaceDispatchTableSize      = 4096};
  enum {interfaceAssignabilityTableSize = 8192};

private:

  /* The system-wide table used for mapping {class,interface} to a sub-vtable */
  static VTableOffset VOffsetTable[interfaceDispatchTableSize];

  /* Class-specific offset to add to interfaceNumber when accessing VIndexTable */
  Uint16 vShift;
  
  /* The system-wide table used for performing instanceof and checkcast
     operations on interface types */
  static Uint16 assignabilityTable[interfaceAssignabilityTableSize];

  /* Class-specific offset to add to interfaceNumber when accessing assignabilityTable */
  Uint16 aShift;

  /* Hashtable keyed by the set of interfaces that are implemented by a class */
  static InterfaceSetHashTable *hashTable;
  
  /* List of interface/VIndex pairs */
  InterfaceList *interfaceList;

  /* Internal helper functions */
  bool tryFitVOffset(Uint32 shift);
  bool packIntoVOffsetTable();
  bool tryFitAssignability(Uint32 shift);
  bool packIntoAssignabilityTable();
  void mergeInterfaceLists();
  
  /* Unique number used for */
  Uint16 assignabilityColor;

  /* Counter used to assign unique values to assignability color, above */
  static Uint16 sColorCounter;

  InterfaceDispatchTable *inheritedInterfaceTable;

public:
  static void staticInit(Pool &pool);

  /* After building up the list of all implemented interfaces using
     interfaceDispatchTable::add(), the build method is used to integrate
     that list into the global interface/class lookup table in a 
     space-efficient manner. */
  InterfaceDispatchTable *build();

  InterfaceDispatchTable(): vShift(0), aShift(0), interfaceList(NULL),
      assignabilityColor(0), inheritedInterfaceTable(0) {}

  /* Add an interface to the set of interfaces that is implemented,
     with a sub-vtable that begins at a byte offset of baseVOffset in
     the enclosing vtable. */
  void add(Uint32 interfaceNumber, Uint32 baseVOffset);

  /* Same as above, but for an interface that has no methods. */
  void add(Uint32 interfaceNumber) {add(interfaceNumber, 0);}

  void inherit(InterfaceDispatchTable *inheritedTable);

  /* Return the table addresses to be stored in the Type object for a class. */
  VTableOffset *getVOffsetTableBaseAddress() {return &VOffsetTable[vShift];};
  Uint16 *getAssignableTableBaseAddress() {return &assignabilityTable[aShift];};

  /* Return the value which must be found in an interface's assignment
     table entry to indicate that an object is assignable
     to that interface type. */
  Uint16 getAssignableMatchValue() const {return assignabilityColor;}
};

bool build(InterfaceDispatchTable * &interfaceDispatchTable);


#endif  /* InterfaceDispatchTable_H */
