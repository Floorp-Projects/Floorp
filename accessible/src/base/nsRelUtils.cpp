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

#include "nsRelUtils.h"

#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsCoreUtils.h"

#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentXBL.h"

#include "nsAutoPtr.h"
#include "nsArrayUtils.h"

already_AddRefed<nsIAccessible>
nsRelUtils::GetRelatedAccessible(nsIAccessible *aAccessible,
                                 PRUint32 aRelationType)
{
  nsCOMPtr<nsIAccessibleRelation> relation;
  nsresult rv = aAccessible->GetRelationByType(aRelationType,
                                               getter_AddRefs(relation));
  if (NS_FAILED(rv) || !relation)
    return nsnull;

  nsIAccessible *targetAccessible = nsnull;
  rv = relation->GetTarget(0, &targetAccessible);
  return targetAccessible;
}

nsresult
nsRelUtils::AddTarget(PRUint32 aRelationType, nsIAccessibleRelation **aRelation,
                      nsIAccessible *aTarget)
{
  if (!aTarget)
    return NS_OK_NO_RELATION_TARGET;

  if (*aRelation) {
    nsRefPtr<nsAccessibleRelation> relation = QueryAccRelation(*aRelation);
    return relation->AddTarget(aTarget);
  }

  *aRelation = new nsAccessibleRelationWrap(aRelationType, aTarget);
  NS_ENSURE_TRUE(*aRelation, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aRelation);
  return NS_OK;
}

nsresult
nsRelUtils::AddTargetFromContent(PRUint32 aRelationType,
                                 nsIAccessibleRelation **aRelation,
                                 nsIContent *aContent)
{
  if (!aContent)
    return NS_OK_NO_RELATION_TARGET;

  nsAccessible* accessible = GetAccService()->GetAccessible(aContent);
  return AddTarget(aRelationType, aRelation, accessible);
}

nsresult
nsRelUtils::AddTargetFromIDRefAttr(PRUint32 aRelationType,
                                   nsIAccessibleRelation **aRelation,
                                   nsIContent *aContent, nsIAtom *aAttr,
                                   PRBool aMayBeAnon)
{
  nsAutoString id;
  if (!aContent->GetAttr(kNameSpaceID_None, aAttr, id))
    return NS_OK_NO_RELATION_TARGET;

  nsCOMPtr<nsIDOMDocument> document =
    do_QueryInterface(aContent->GetOwnerDoc());
  NS_ASSERTION(document, "The given node is not in document!");
  if (!document)
    return NS_OK_NO_RELATION_TARGET;

  nsCOMPtr<nsIDOMElement> refElm;
  if (aMayBeAnon && aContent->GetBindingParent()) {
    nsCOMPtr<nsIDOMDocumentXBL> documentXBL(do_QueryInterface(document));
    nsCOMPtr<nsIDOMElement> bindingParent =
      do_QueryInterface(aContent->GetBindingParent());
    documentXBL->GetAnonymousElementByAttribute(bindingParent,
                                                NS_LITERAL_STRING("id"),
                                                id,
                                                getter_AddRefs(refElm));
  }
  else {
    document->GetElementById(id, getter_AddRefs(refElm));
  }

  nsCOMPtr<nsIContent> refContent(do_QueryInterface(refElm));
  return AddTargetFromContent(aRelationType, aRelation, refContent);
}

nsresult
nsRelUtils::AddTargetFromIDRefsAttr(PRUint32 aRelationType,
                                    nsIAccessibleRelation **aRelation,
                                    nsIContent *aContent, nsIAtom *aAttr)
{
  nsresult rv = NS_OK_NO_RELATION_TARGET;

  nsIContent* refElm = nsnull;
  IDRefsIterator iter(aContent, aAttr);
  while ((refElm = iter.NextElem())) {
    rv = AddTargetFromContent(aRelationType, aRelation, refElm);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}
