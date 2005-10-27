/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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

#include "nscore.h"

class nsIAtom;
typedef unsigned long PtrBits;

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

// Categories of properties
// 0x00000000U is global.
// 0x00000001U is reserved.
#define DOM_USER_DATA         0x00000002U
#define DOM_USER_DATA_HANDLER 0x00000004U

class nsPropertyTable
{
 public:
  /**
   * Get the value of the property |aPropertyName| for node |aObject|.
   * |aResult|, if supplied, is filled in with a return status code.
   **/
  void* GetProperty(const void *aObject,
                    nsIAtom    *aPropertyName,
                    nsresult   *aResult = nsnull)
  {
    return GetPropertyInternal(aObject, 0, aPropertyName, PR_FALSE, aResult);
  }

  void* GetProperty(const void *aObject,
                    PRUint32    aCategory,
                    nsIAtom    *aPropertyName,
                    nsresult   *aResult = nsnull)
  {
    return GetPropertyInternal(aObject, aCategory, aPropertyName, PR_FALSE,
                               aResult);
  }

  /**
   * Set the value of the property |aPropertyName| in the global category to
   * |aPropertyValue| for node |aObject|.  |aDtor| is a destructor for the
   * property value to be called if the property is removed.  It can be null
   * if no destructor is required.  |aDtorData| is an optional pointer to an
   * opaque context to be passed to the property destructor.  Note that the
   * destructor is global for each property name regardless of node; it is an
   * error to set a given property with a different destructor than was used
   * before (this will return NS_ERROR_INVALID_ARG). If aOldValue is non-null
   * it will contain the old value after the function returns (the destructor
   * for the old value will not be run in that case).
   */
  NS_HIDDEN_(nsresult) SetProperty(const void         *aObject,
                                   nsIAtom            *aPropertyName,
                                   void               *aPropertyValue,
                                   NSPropertyDtorFunc  aDtor,
                                   void               *aDtorData,
                                   void              **aOldValue = nsnull)
  {
    return SetPropertyInternal(aObject, 0, aPropertyName, aPropertyValue,
                               aDtor, aDtorData, aOldValue);
  }

  /**
   * Set the value of the property |aPropertyName| in the category |aCategory|
   * to |aPropertyValue| for node |aObject|.  |aDtor| is a destructor for the
   * property value to be called if the property is removed.  It can be null
   * if no destructor is required.  Note that the destructor is global for
   * each property name regardless of node; it is an error to set a given
   * property with a different destructor than was used before (this will
   * return NS_ERROR_INVALID_ARG). If aOldValue is non-null it will contain the
   * old value after the function returns (the destructor for the old value
   * will not be run in that case).
   */
  NS_HIDDEN_(nsresult) SetProperty(const void         *aObject,
                                   PRUint32            aCategory,
                                   nsIAtom            *aPropertyName,
                                   void               *aPropertyValue,
                                   NSPropertyDtorFunc  aDtor,
                                   void              **aOldValue = nsnull)
  {
    return SetPropertyInternal(aObject, aCategory, aPropertyName,
                               aPropertyValue, aDtor, nsnull, aOldValue);
  }

  /**
   * Delete the property |aPropertyName| in the global category for object
   * |aObject|. The property's destructor function will be called.
   */
  NS_HIDDEN_(nsresult) DeleteProperty(const void *aObject,
                                      nsIAtom    *aPropertyName)
  {
    return DeleteProperty(aObject, 0, aPropertyName);
  }

  /**
   * Delete the property |aPropertyName| in category |aCategory| for object
   * |aObject|. The property's destructor function will be called.
   */
  NS_HIDDEN_(nsresult) DeleteProperty(const void *aObject,
                                      PRUint32    aCategory,
                                      nsIAtom    *aPropertyName);

  /**
   * Unset the property |aPropertyName| in the global category for object
   * |aObject|, but do not call the property's destructor function.  The
   * property value is returned.
   */
  void* UnsetProperty(const void *aObject,
                      nsIAtom    *aPropertyName,
                      nsresult   *aStatus = nsnull)
  {
    return GetPropertyInternal(aObject, 0, aPropertyName, PR_TRUE, aStatus);
  }

  /**
   * Unset the property |aPropertyName| in category |aCategory| for object
   * |aObject|, but do not call the property's destructor function.  The
   * property value is returned.
   */
  void* UnsetProperty(const void *aObject,
                      PRUint32    aCategory,
                      nsIAtom    *aPropertyName,
                      nsresult   *aStatus = nsnull)
  {
    return GetPropertyInternal(aObject, aCategory, aPropertyName, PR_TRUE,
                               aStatus);
  }

  /**
   * Deletes all of the properties for object |aObject|, calling the
   * destructor function for each property.
   */
  NS_HIDDEN_(void) DeleteAllPropertiesFor(const void *aObject);

  /**
   * Enumerate the properties in category |aCategory| for object |aObject|.
   * For every property |aCallback| will be called with as arguments |aObject|,
   * the property name, the property value and |aData|.
   */
  NS_HIDDEN_(void) Enumerate(const void *aObject, PRUint32 aCategory,
                             NSPropertyFunc aCallback, void *aData);

  ~nsPropertyTable() NS_HIDDEN;

  struct PropertyList;

 private:
  NS_HIDDEN_(void) DestroyPropertyList();
  NS_HIDDEN_(PropertyList*) GetPropertyListFor(PRUint32 aCategory,
                                               nsIAtom *aPropertyName) const;
  NS_HIDDEN_(void*) GetPropertyInternal(const void *aObject,
                                        PRUint32    aCategory,
                                        nsIAtom    *aPropertyName,
                                        PRBool      aRemove,
                                        nsresult   *aStatus);
  NS_HIDDEN_(nsresult) SetPropertyInternal(const void         *aObject,
                                           PRUint32            aCategory,
                                           nsIAtom            *aPropertyName,
                                           void               *aPropertyValue,
                                           NSPropertyDtorFunc  aDtor,
                                           void               *aDtorData,
                                           void              **aOldValue);

  PropertyList *mPropertyList;
};
#endif
