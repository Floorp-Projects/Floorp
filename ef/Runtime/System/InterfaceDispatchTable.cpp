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

#include "InterfaceDispatchTable.h"

/* This class is used to construct the data structures that enable fast,
   constant-time assignability checks, i.e. for implementing the instanceof
   and checkcast bytecodes.

   This class also builds the lookup tables used for constant-time dispatch
   of interface functions.
*/

/*
  Algorithm notes:

  Dispatch of interface methods is handled by an interface-specific "sub-vtable"
  that is embedded within the vtable of each class that implements that interface.
  The location of this sub-vtable within a vtable may be different for each class
  (because the classes share no inheritance relationship).  The mapping from a
  {Class, Interface} tuple to a sub-vtable offset can be performed in constant
  time by enumerating all classes and interfaces and performing a 2-D table
  lookup.  In the example table below, sub-vtable offsets are represented
  symbolically with capital letters:

 
                                Interface #
                    |  0     1     2     3     4     5  ....
                ---------------------------------------------
                0   |  X                       Y
                1   |  X           Z           Y
        Class   2   |  Y
          #     3   |  W           
                4   |              V           T
                    |
 

  For reasonably large programs, however, this lookup table can become quite
  large, so we borrow a technique for compressing sparse matrices.  Typically,
  many classes implement some or all of the same interfaces as other classes.
  The 2-D table is compressed into 1-D by shifting rows in the table until there
  are no conflicting entries in each column:


                                Interface #
 [SHIFT]            |  0     1     2     3     4     5  ....
                ---------------------------------------------
  0-->          0   |  X                       Y
  0-->          1   |  X           Z           Y
  4-->   Class  2   |                          Y
  1-->     #    3   |        W           
  1-->          4   |                    V           T
                    |

  Next, the 2-D array is compressed into a 1-D vector by collapsing all the rows:

                                     Index #
                       0     1     2     3     4     5  ....
                       -------------------------------------
                       X     W     Z     V     Y     T

  To lookup the sub-vtable location for a given class, a given class' SHIFT
  value is added to the desired interface's number to form the index into
  the compacted 1-D table.

  Note that this lookup table scheme assumes that, before an interface method
  is dispatched, the class is known to implement the interface.  However,
  this same table cannot be used to perform the assignability check which
  would determine if the class implemented the interface.  For example,
  in the sample table above class #0 does not implement interface #2,
  and yet there is an entry corresponding to this combination in the
  compressed 1-D lookup table.

*/

/*

  For performing assignability tests (in order to implement the instanceof and
  checkcast bytecodes) a separate lookup table is used, but that table employs a
  similar methodology as the vtable offset table used for interface dispatch.
  
  As with the vtable/dispatch lookup table, a 2D array is compressed into a
  1-D vector.  However, this table has stricter limitations on how this
  compression can take place.

  Consider the table below in which the presence of a letter in the table
  indicates that the corresponding class is implemented by the interface.
  Each entry letter represents a unique color that is applied to all the
  entries for a given class, i.e. all entries in the same row have the
  same color.  (However, if any two classes implement exactly the same
  set of interfaces, they can be colored the same, as with classes 2 and
  3 below:

                                  Interface #
                    |  0     1     2     3     4     5  ....
                ---------------------------------------------
                0   |  A                       A
                1   |  B           B           B
        Class   2   |  C
          #     3   |  C                 
                4   |                    E     E
                    |


  Applying shifts to each row:

                                Interface #
 [SHIFT]            |  0     1     2     3     4     5     6 ....
                -------------------------------------------------
  0-->          0   |  A                       A
  1-->          1   |        B           B           B
  2-->   Class  2   |              C
  2-->     #    3   |              C           
  3-->          4   |                                      E     E
                    | 


 Finally, the 2-D array is compressed into a 1-D vector by collapsing all the rows:

                                     Index #
                       0     1     2     3     4     5     6 ....
                       -------------------------------------------
                       A     B     C     B     A     B     E     E

  To determine if an instance of a particular class is assignable to
  a variable with a given interface type, we test to see if the following
  expression is true:

  table[class(instance).shift + interface.number] == color(class(instance))

*/

/* The system-wide table for handling vtable-based dispatch of interface
   methods for all classes/interfaces */
VTableOffset InterfaceDispatchTable::VOffsetTable[interfaceDispatchTableSize];

/* The system-wide table used for performing instanceof and checkcast
   operations on interface types and on arrays of interface types */
Uint16 InterfaceDispatchTable::assignabilityTable[interfaceAssignabilityTableSize];

/* Counter used to assign unique values to assignability color */
Uint16 InterfaceDispatchTable::sColorCounter;

/* Hashtable keyed by the set of interfaces that are implemented by a class.
   This is used to eliminate redundancy in the assignability table. */
InterfaceSetHashTable *InterfaceDispatchTable::hashTable;

/* Class initialization */
void InterfaceDispatchTable::staticInit(Pool &pool) {
    hashTable = new InterfaceSetHashTable(pool);
}

/* Return true if the list contains a given interface, false otherwise. */
static bool contains(InterfaceList *interfaceList, Uint32 interfaceNumber)
{
    int size = interfaceList->size();
    for (int i = 0; i < size; i++) {
        if ((*interfaceList)[i].interfaceNumber == interfaceNumber)
            return true;
    }
    return false;
}

/* Used to implement HashTable's keyed by InterfaceLists.
   Return true if two interfaceListKey1 and interfaceListKey2
   contain the identical set of interfaces.

   XXX - uses slow N^2 algorithm
 */
bool InterfaceListKeyOps::equals(InterfaceList * interfaceListKey1,
                                 InterfaceList * interfaceListKey2) {
    int size1 = interfaceListKey1->size();
    int size2 = interfaceListKey2->size();

    if (size1 != size2)
        return false;

    for (int i = 0; i < size1; i++) {
        InterfaceVIndexPair iPair = (*interfaceListKey1)[i];
        if (!contains(interfaceListKey2, iPair.interfaceNumber))
            return false;
    }
    return true;
}

/* Used to implement HashTable's keyed by InterfaceLists.
   Returns a hashcode that is the same among any two InterfaceLists
   that contain the same set of interfaces.  Since InterfaceLists
   are unsorted, the hashcode must not depend on the order 
   of elements in the list. */
Uint32 InterfaceListKeyOps::hashCode(InterfaceList * interfaceListKey) {
    Uint32 hashCode = 0;
    int size = interfaceListKey->size();
    for (int i = 0; i < size; i++) {
        InterfaceVIndexPair iPair = (*interfaceListKey)[i];
        Uint32 interfaceNumber = iPair.interfaceNumber;
        hashCode *= (interfaceNumber + 1);
    }
    return hashCode;
}
    
/* Check to see whether it is possible to pack a sparse vector of
   interface numbers into the global vtable interface lookup table in a
   conflict-free manner by shifting the vector entries uniformly
   using the provided value. */
bool InterfaceDispatchTable::tryFitVOffset(Uint32 shift)
{
    int size = interfaceList->size();
    for (int i = 0; i < size; i++) {
        InterfaceVIndexPair iPair = (*interfaceList)[i];
        Uint32 tableIndex = iPair.interfaceNumber + shift;
        Uint32 baseVOffset = VOffsetTable[tableIndex];
        if (baseVOffset && iPair.baseVOffset && (baseVOffset != iPair.baseVOffset))
            return false;
    }
    return true;
}

/* Discover a conflict-free packing in the global vtable interface lookup
   table of the given classes' vector of implemented interfaces.  The
   packing is made possible by shifting the vector left or right so that
   the non-empty entries fall in the unoccupied interstices in the global
   interface lookup table. */ 
bool InterfaceDispatchTable::packIntoVOffsetTable()
{
    bool success = false;
    
    /* A brute-force scan to locate a collision-free packing */
    Uint32 maxShift = interfaceDispatchTableSize - maxInterfaces;
    Uint32 shift = 0;
    if (inheritedInterfaceTable) {
        /* Since we have to accomodate all the interfaces in our parent 
           (as well as our own), we know that there will be no packing
           that uses a shift value less than our parent's, so start there. */
        Uint32 parentShift = inheritedInterfaceTable->vShift;
        shift = parentShift;
    }
    for (; shift <= maxShift; shift++) {
        success = tryFitVOffset(shift);
        if (success)
            break;
    }
    
    /* See if we overflowed the global interface lookup table */
    if (!success)
        return false;
    
    /* We found a way to pack this class' interfaces into the global
       interface dispatch table.  Copy the entries into the table. */
    Uint32 size = interfaceList->size();
    for (Uint32 i = 0; i < size; i++) {
        InterfaceVIndexPair ipair = (*interfaceList)[i];
        Uint32 tableIndex = ipair.interfaceNumber + shift;
        if (ipair.baseVOffset)
            VOffsetTable[tableIndex] = ipair.baseVOffset;
    }

    vShift = shift;

    return true;
}

/* Check to see whether it is possible to pack a sparse vector of
   interface numbers into the global interface assignability table in
   a conflict-free manner by shifting the vector entries uniformly
   using the provided value. */
bool InterfaceDispatchTable::tryFitAssignability(Uint32 shift)
{
    int size = interfaceList->size();
    for (int i = 0; i < size; i++) {
        InterfaceVIndexPair iPair = (*interfaceList)[i];
        Uint32 tableIndex = iPair.interfaceNumber + shift;
        if (assignabilityTable[tableIndex]) 
            return false;
    }
    return true;
}

/* Discover a conflict-free packing in the global interface lookup table
   of the given classes' vector of implemented interfaces.  The packing
   is made possible by shifting the vector left or right so that the
   non-empty entries fall in the unoccupied interstices in the global
   interface lookup table. */ 
bool InterfaceDispatchTable::packIntoAssignabilityTable()
{
    InterfaceDispatchTable *match;

    /* If another class implements exactly the same set of interfaces
       we can share our assignability testing table with that class. */
    if (hashTable->get(interfaceList, &match)) {
        aShift = match->aShift;
        assignabilityColor = match->assignabilityColor;
        return true;
    }
    hashTable->add(interfaceList, this);

    assignabilityColor = ++sColorCounter;

    bool success = false;
    
    /* A brute-force scan to locate a collision-free packing */
    Uint32 maxShift = interfaceAssignabilityTableSize - maxInterfaces;
    Uint32 shift = 0;
    if (inheritedInterfaceTable) {
        /* Since we have to accomodate all the interfaces in our parent 
           (as well as our own), we know that there will be no packing
           that uses a shift value less than or the same as our parent's. */
        Uint32 parentShift = inheritedInterfaceTable->aShift;
        shift = parentShift + 1;
    }
    for (; shift <= maxShift; shift++) {
        success = tryFitAssignability(shift);
        if (success)
            break;
    }
    
    /* See if we overflowed the global interface lookup table */
    if (!success)
        return false;
    
    /* We found a way to pack this class' interfaces into the global
       interface dispatch table.  Copy the entries into the table. */
    Uint32 size = interfaceList->size();
    for (Uint32 i = 0; i < size; i++) {
        InterfaceVIndexPair ipair = (*interfaceList)[i];
        Uint32 tableIndex = ipair.interfaceNumber + shift;
        assignabilityTable[tableIndex] = assignabilityColor;
    }

    aShift = shift;

    return true;
}

/* Add an interface to the set of interfaces that is implemented by 
   a single class.  This interface has a sub-vtable that begins at
   an offset of baseVIndex from the beginning of the class' vtable. */
void InterfaceDispatchTable::add(Uint32 interfaceNumber,
                                 Uint32 baseVOffset)
{
    if (interfaceList == NULL) {
        interfaceList = new InterfaceList(5);
    } else {     
       /* Don't add the same interface more than once.  (This can
          occur when a class implements an interface that is
          a sub-interface of more than one disjoint interface which
          is implemented by the class.) */
        int size = interfaceList->size();
        /* Linear search is probably OK given the small size of most 
           interface lists */
        for (int i = 0; i < size; i++) {
            if (interfaceNumber == (*interfaceList)[i].interfaceNumber)
                return;
        }
    }
    
    InterfaceVIndexPair iPair;
    iPair.baseVOffset = baseVOffset;
    iPair.interfaceNumber = interfaceNumber;
    interfaceList->append(iPair);
}

/* Merge the list of interfaces with the list of inherited interfaces. */
void InterfaceDispatchTable::mergeInterfaceLists()
{
    if (!inheritedInterfaceTable)
        return;
    InterfaceList *inheritedInterfaceList = inheritedInterfaceTable->interfaceList;
    int size = inheritedInterfaceList->size();
    InterfaceVIndexPair *iPair;
    for (int i = 0; i < size; i++) {
        iPair = &(*inheritedInterfaceList)[i];
        add(iPair->interfaceNumber, iPair->baseVOffset);
    }
}

/* After building up the list of all implemented interfaces using
   interfaceDispatchTable::add(), the build method is used to integrate
   that list into the global interface/class lookup table in a 
   space-efficient manner. */
InterfaceDispatchTable *InterfaceDispatchTable::build()
{
    /* If this class doesn't define any new interfaces, then we can share
       the interface table packing with its parent class */
    if (interfaceList == NULL) {
        if (inheritedInterfaceTable) {
            InterfaceDispatchTable *tmpTable = inheritedInterfaceTable;
            delete this;
            return tmpTable;
        } else {
            return this;
        }
    } 
    
    /* Merge the list of interfaces with the list of inherited interfaces. */
    mergeInterfaceLists();

    /* The given class implements some interfaces that its direct superclass does
       not.  Attempt to find a conflict-free packing in each of the
       global interface tables of the list of implemented interfaces for the
       given class. */
    if (!packIntoVOffsetTable())
        return NULL;

    /* Build table for instanceof and checkcast operations */
    if (!packIntoAssignabilityTable())
        return NULL;

#if 0
    /* We don't delete the interface list because it can be inherited by subclasses */
    delete interfaceList;
    interfaceList = NULL;
#endif

    return this;
}

/* Integrate the list into the global interface/class lookup table in a 
   space-efficient manner.  This may cause the original 
   interfaceDispatchTable to be destroyed and replaced with one that
   is shared with another class. */
bool build(InterfaceDispatchTable * &interfaceDispatchTable)
{
    InterfaceDispatchTable *newTable = interfaceDispatchTable->build();
    if (newTable) {
        interfaceDispatchTable = newTable;
        return true;
    }
    return false;
}

/* Use another interface lookup table as the basis for constructing this table.
   This is used when inheriting interfaces from superclasses. */
void InterfaceDispatchTable::inherit(InterfaceDispatchTable *table)
{
    if (!table)
        return;

    InterfaceList *inheritedInterfaceList = table->interfaceList;
    if (!inheritedInterfaceList)
        return;

    assert (!inheritedInterfaceTable);
    inheritedInterfaceTable = table;
}

     
