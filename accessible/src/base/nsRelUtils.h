/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsRelUtils_H_
#define _nsRelUtils_H_

#include "nsAccessibleRelationWrap.h"

#include "nsIAtom.h"
#include "nsIContent.h"

// Used by AddTarget...() methods. Returned when can't get target accessible.
#define NS_OK_NO_RELATION_TARGET \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x24)

/**
 * Utils class designed to work with accessible relations.
 */
class nsRelUtils
{
public:
  /**
   * Return first target of the relation of the given relation type for
   * the given accessible.
   *
   * @param aAccessible   [in] the accessible to get an relation
   * @param aRelationType [in] relation type
   * @return              an accessible
   */
  static already_AddRefed<nsIAccessible>
    GetRelatedAccessible(nsIAccessible *aAccessible, PRUint32 aRelationType);

  /**
   * Create the relation if the given relation is null. Add target to it
   * which is the given accessible.
   *
   * @param  aRelationType  [in] relation type
   * @param  aRelation      [in, out] relation object
   * @param  aTarget        [in] accessible object
   */
  static nsresult AddTarget(PRUint32 aRelationType,
                            nsIAccessibleRelation **aRelation,
                            nsIAccessible *aTarget);

  /**
   * Create the relation if the given relation is null and add the target to it
   * which is the accessible for the given node.
   *
   * @param  aRelationType  [in] relation type
   * @param  aRelation      [in, out] relation object
   * @param  aContent       [in] accessible node
   */
  static nsresult AddTargetFromContent(PRUint32 aRelationType,
                                       nsIAccessibleRelation **aRelation,
                                       nsIContent *aContent);

  /**
   * Create the relation if the given relation is null and add the target to it
   * pointed by IDRef attribute on the given node.
   *
   * @param  aRelationType  [in] relation type
   * @param  aRelation      [in, out] relation object
   * @param  aContent       [in] node having the given IDRef attribute
   * @param  aAttr          [in] IDRef attribute
   * @param  aMayBeAnon     [in] true if the target may be anonymous; if so,
   *                             we need to look for it under the binding
   *                             parent of aContent.
   */
  static nsresult AddTargetFromIDRefAttr(PRUint32 aRelationType,
                                         nsIAccessibleRelation **aRelation,
                                         nsIContent *aContent, nsIAtom *aAttr,
                                         PRBool aMayBeAnon = PR_FALSE);

  /**
   * Create the relation if the given relation is null and add the targets to it
   * that are pointed by IDRefs attribute on the given node.
   *
   * @param  aRelationType  [in] relation type
   * @param  aRelation      [in, out] relation object
   * @param  aContent       [in] node having the given IDRefs attribute
   * @param  aAttr          [in] IDRefs attribute
   */
  static nsresult AddTargetFromIDRefsAttr(PRUint32 aRelationType,
                                          nsIAccessibleRelation **aRelation,
                                          nsIContent *aContent, nsIAtom *aAttr);

  /**
   * Query nsAccessibleRelation from the given nsIAccessibleRelation.
   */
  static already_AddRefed<nsAccessibleRelation>
  QueryAccRelation(nsIAccessibleRelation *aRelation)
  {
    nsAccessibleRelation* relation = nsnull;
    if (aRelation)
      CallQueryInterface(aRelation, &relation);

    return relation;
  }
};

#endif
