/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/**
 * nsPropertyTable allows a set of arbitrary key/value pairs to be stored
 * for any number of nodes, in a global hashtable rather than on the nodes
 * themselves.  Nodes can be any type of object; the hashtable keys are
 * nsIAtom pointers, and the values are void pointers.
 */

#ifndef nsPropertyTable_h_
#define nsPropertyTable_h_

#include "mozilla/MemoryReporting.h"
#include "nscore.h"

class nsIAtom;

typedef void
(*NSPropertyFunc)(void           *aObject,
                  nsIAtom        *aPropertyName,
                  void           *aPropertyValue,
                  void           *aData);

/**
 * Callback type for property destructors.  |aObject| is the object
 * the property is being removed for, |aPropertyName| is the property
 * being removed, |aPropertyValue| is the value of the property, and |aData|
 * is the opaque destructor data that was passed to SetProperty().
 **/
typedef NSPropertyFunc NSPropertyDtorFunc;
class nsINode;
class nsIFrame;

class nsPropertyOwner
{
public:
  nsPropertyOwner(const nsPropertyOwner& aOther) : mObject(aOther.mObject) {}

  // These are the types of objects that can own properties. No object should
  // inherit more then one of these classes.
  // To add support for more types just add to this list.
  MOZ_IMPLICIT nsPropertyOwner(const nsINode* aObject) : mObject(aObject) {}
  MOZ_IMPLICIT nsPropertyOwner(const nsIFrame* aObject) : mObject(aObject) {}

  operator const void*() { return mObject; }
  const void* get() { return mObject; }

private:
  const void* mObject;
};

class nsPropertyTable
{
 public:
  /**
   * Get the value of the property |aPropertyName| for node |aObject|.
   * |aResult|, if supplied, is filled in with a return status code.
   **/
  void* GetProperty(const nsPropertyOwner& aObject,
                    nsIAtom    *aPropertyName,
                    nsresult   *aResult = nullptr)
  {
    return GetPropertyInternal(aObject, aPropertyName, false, aResult);
  }

  /**
   * Set the value of the property |aPropertyName| to
   * |aPropertyValue| for node |aObject|.  |aDtor| is a destructor for the
   * property value to be called if the property is removed.  It can be null
   * if no destructor is required.  |aDtorData| is an optional pointer to an
   * opaque context to be passed to the property destructor.  Note that the
   * destructor is global for each property name regardless of node; it is an
   * error to set a given property with a different destructor than was used
   * before (this will return NS_ERROR_INVALID_ARG). If aOldValue is non-null
   * it will contain the old value after the function returns (the destructor
   * for the old value will not be run in that case). If |aTransfer| is true
   * the property will be transfered to the new table when the property table
   * for |aObject| changes (currently the tables for nodes are owned by their
   * ownerDocument, so if the ownerDocument for a node changes, its property
   * table changes too). If |aTransfer| is false the property will just be
   * deleted instead.
   */
  nsresult SetProperty(const nsPropertyOwner&     aObject,
                                   nsIAtom            *aPropertyName,
                                   void               *aPropertyValue,
                                   NSPropertyDtorFunc  aDtor,
                                   void               *aDtorData,
                                   bool                aTransfer = false,
                                   void              **aOldValue = nullptr)
  {
    return SetPropertyInternal(aObject, aPropertyName, aPropertyValue,
                               aDtor, aDtorData, aTransfer, aOldValue);
  }

  /**
   * Delete the property |aPropertyName| in the global category for object
   * |aObject|. The property's destructor function will be called.
   */
  nsresult DeleteProperty(nsPropertyOwner aObject,
                                      nsIAtom    *aPropertyName);

  /**
   * Unset the property |aPropertyName| in the global category for object
   * |aObject|, but do not call the property's destructor function.  The
   * property value is returned.
   */
  void* UnsetProperty(const nsPropertyOwner& aObject,
                      nsIAtom    *aPropertyName,
                      nsresult   *aStatus = nullptr)
  {
    return GetPropertyInternal(aObject, aPropertyName, true, aStatus);
  }

  /**
   * Deletes all of the properties for object |aObject|, calling the
   * destructor function for each property.
   */
  void DeleteAllPropertiesFor(nsPropertyOwner aObject);

  /**
   * Transfers all properties for object |aObject| that were set with the
   * |aTransfer| argument as true to |aTable|. Deletes the other properties
   * for object |aObject|, calling the destructor function for each property.
   * If transfering a property fails, this deletes all the properties for
   * object |aObject|.
   */
  nsresult
    TransferOrDeleteAllPropertiesFor(nsPropertyOwner aObject,
                                     nsPropertyTable *aOtherTable);

  /**
   * Enumerate the properties for object |aObject|.
   * For every property |aCallback| will be called with as arguments |aObject|,
   * the property name, the property value and |aData|.
   */
  void Enumerate(nsPropertyOwner aObject,
                             NSPropertyFunc aCallback, void *aData);

  /**
   * Enumerate all the properties.
   * For every property |aCallback| will be called with arguments the owner,
   * the property name, the property value and |aData|.
   */
  void EnumerateAll(NSPropertyFunc aCallback, void *aData);

  /**
   * Deletes all of the properties for all objects in the property
   * table, calling the destructor function for each property.
   */
  void DeleteAllProperties();

  nsPropertyTable() : mPropertyList(nullptr) {}  
  ~nsPropertyTable() {
    DeleteAllProperties();
  }

  /**
   * Function useable as destructor function for property data that is
   * XPCOM objects. The function will call NS_IF_RELASE on the value
   * to destroy it.
   */
  static void SupportsDtorFunc(void *aObject, nsIAtom *aPropertyName,
                               void *aPropertyValue, void *aData);

  class PropertyList;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  void DestroyPropertyList();
  PropertyList* GetPropertyListFor(nsIAtom *aPropertyName) const;
  void* GetPropertyInternal(nsPropertyOwner aObject,
                                        nsIAtom    *aPropertyName,
                                        bool        aRemove,
                                        nsresult   *aStatus);
  nsresult SetPropertyInternal(nsPropertyOwner     aObject,
                                           nsIAtom            *aPropertyName,
                                           void               *aPropertyValue,
                                           NSPropertyDtorFunc  aDtor,
                                           void               *aDtorData,
                                           bool                aTransfer,
                                           void              **aOldValue);

  PropertyList *mPropertyList;
};
#endif
