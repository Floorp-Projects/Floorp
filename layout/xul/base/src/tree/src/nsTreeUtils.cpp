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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Jan Varga <varga@ku.sk>
 *   Nate Nielsen <nielsen@memberwebs.com>
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

#include "nsReadableUtils.h"
#include "nsTreeUtils.h"
#include "nsChildIterator.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsINodeInfo.h"

nsresult
nsTreeUtils::TokenizeProperties(const nsAString& aProperties, nsISupportsArray* aPropertiesArray)
{
  NS_PRECONDITION(aPropertiesArray != nsnull, "null ptr");
  if (! aPropertiesArray)
     return NS_ERROR_NULL_POINTER;

  nsAString::const_iterator end;
  aProperties.EndReading(end);

  nsAString::const_iterator iter;
  aProperties.BeginReading(iter);

  do {
    // Skip whitespace
    while (iter != end && nsCRT::IsAsciiSpace(*iter))
      ++iter;

    // If only whitespace, we're done
    if (iter == end)
      break;

    // Note the first non-whitespace character
    nsAString::const_iterator first = iter;

    // Advance to the next whitespace character
    while (iter != end && ! nsCRT::IsAsciiSpace(*iter))
      ++iter;

    // XXX this would be nonsensical
    NS_ASSERTION(iter != first, "eh? something's wrong here");
    if (iter == first)
      break;

    nsCOMPtr<nsIAtom> atom = do_GetAtom(Substring(first, iter));
    aPropertiesArray->AppendElement(atom);
  } while (iter != end);

  return NS_OK;
}

nsresult
nsTreeUtils::GetImmediateChild(nsIContent* aContainer, nsIAtom* aTag,
                               nsIContent** aResult)
{
  ChildIterator iter, last;
  for (ChildIterator::Init(aContainer, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> child = *iter;

    if (child->Tag() == aTag) {
      NS_ADDREF(*aResult = child);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

nsresult
nsTreeUtils::GetDescendantChild(nsIContent* aContainer, nsIAtom* aTag, nsIContent** aResult)
{
  ChildIterator iter, last;
  for (ChildIterator::Init(aContainer, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> child = *iter;
    if (child->Tag() == aTag) {
      NS_ADDREF(*aResult = child);
      return NS_OK;
    }
    else {
      nsresult rv = GetDescendantChild(child, aTag, aResult);
      if(NS_FAILED(rv))
        return rv;

      if(*aResult)
        return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

nsresult
nsTreeUtils::UpdateSortIndicators(nsIContent* aColumn, const nsAString& aDirection)
{
  aColumn->SetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, aDirection, PR_TRUE);
  aColumn->SetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, NS_LITERAL_STRING("true"), PR_TRUE);

  // Unset sort attribute(s) on the other columns
  nsIContent* parentContent = aColumn->GetParent();
  if (parentContent) {
    nsINodeInfo *ni = parentContent->GetNodeInfo();

    if (ni && ni->Equals(nsXULAtoms::treecols, kNameSpaceID_XUL)) {
      PRUint32 numChildren = parentContent->GetChildCount();
      for (PRUint32 i = 0; i < numChildren; ++i) {
        nsIContent *childContent = parentContent->GetChildAt(i);

        if (childContent) {
          ni = childContent->GetNodeInfo();

          if (ni && ni->Equals(nsXULAtoms::treecol, kNameSpaceID_XUL) &&
              childContent != aColumn) {
            childContent->UnsetAttr(kNameSpaceID_None,
                                    nsXULAtoms::sortDirection, PR_TRUE);
            childContent->UnsetAttr(kNameSpaceID_None,
                                    nsXULAtoms::sortActive, PR_TRUE);
          }
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsTreeUtils::GetColumnIndex(nsIContent* aColumn, PRInt32* aResult)
{
  nsIContent* parentContent = aColumn->GetParent();
  if (parentContent) {
    nsINodeInfo *ni = parentContent->GetNodeInfo();

    if (ni && ni->Equals(nsXULAtoms::treecols, kNameSpaceID_XUL)) {
      PRUint32 numChildren = parentContent->GetChildCount();
      PRInt32 colIndex = 0;
      for (PRUint32 i = 0; i < numChildren; ++i) {
        nsIContent *childContent = parentContent->GetChildAt(i);

        if (childContent) {
          ni = childContent->GetNodeInfo();

          if (ni && ni->Equals(nsXULAtoms::treecol, kNameSpaceID_XUL)) {

            if (childContent == aColumn) {
              *aResult = colIndex;
	      return NS_OK;
            }
            colIndex++;
          }
        }
      }
    }
  }

  *aResult = -1;
  return NS_OK;
}
