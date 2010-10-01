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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsHTMLTextAccessible.h"

#include "nsDocAccessible.h"
#include "nsAccUtils.h"
#include "nsRelUtils.h"
#include "nsTextEquivUtils.h"

#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsComponentManagerUtils.h"

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTextAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLTextAccessible::
  nsHTMLTextAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsTextAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTextAccessible, nsTextAccessible)

NS_IMETHODIMP
nsHTMLTextAccessible::GetName(nsAString& aName)
{
  // Text node, ARIA can't be used.
  aName.Truncate();
  return AppendTextTo(aName, 0, PR_UINT32_MAX);
}

PRUint32
nsHTMLTextAccessible::NativeRole()
{
  nsIFrame *frame = GetFrame();
  // Don't return on null frame -- we still return a role
  // after accessible is shutdown/DEFUNCT
  if (frame && frame->IsGeneratedContentFrame()) {
    return nsIAccessibleRole::ROLE_STATICTEXT;
  }

  return nsTextAccessible::NativeRole();
}

nsresult
nsHTMLTextAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsTextAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsDocAccessible *docAccessible = GetDocAccessible();
  if (docAccessible) {
     PRUint32 state, extState;
     docAccessible->GetState(&state, &extState);
     if (0 == (extState & nsIAccessibleStates::EXT_STATE_EDITABLE)) {
       *aState |= nsIAccessibleStates::STATE_READONLY; // Links not focusable in editor
     }
  }

  return NS_OK;
}

nsresult
nsHTMLTextAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (NativeRole() == nsIAccessibleRole::ROLE_STATICTEXT) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("auto-generated"),
                                  NS_LITERAL_STRING("true"), oldValueUnused);
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLHRAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLHRAccessible::
  nsHTMLHRAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsLeafAccessible(aContent, aShell)
{
}

PRUint32
nsHTMLHRAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_SEPARATOR;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLBRAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLBRAccessible::
  nsHTMLBRAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsLeafAccessible(aContent, aShell)
{
}

PRUint32
nsHTMLBRAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_WHITESPACE;
}

nsresult
nsHTMLBRAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  *aState = nsIAccessibleStates::STATE_READONLY;
  if (aExtraState)
    *aExtraState = 0;

  return NS_OK;
}

nsresult
nsHTMLBRAccessible::GetNameInternal(nsAString& aName)
{
  aName = static_cast<PRUnichar>('\n');    // Newline char
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLabelAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLabelAccessible::
  nsHTMLLabelAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLLabelAccessible, nsHyperTextAccessible)

nsresult
nsHTMLLabelAccessible::GetNameInternal(nsAString& aName)
{
  return nsTextEquivUtils::GetNameFromSubtree(this, aName);
}

PRUint32
nsHTMLLabelAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LABEL;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLOuputAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLOutputAccessible::
  nsHTMLOutputAccessible(nsIContent* aContent, nsIWeakReference* aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLOutputAccessible, nsHyperTextAccessible)

NS_IMETHODIMP
nsHTMLOutputAccessible::GetRelationByType(PRUint32 aRelationType,
                                          nsIAccessibleRelation** aRelation)
{
  nsresult rv = nsAccessibleWrap::GetRelationByType(aRelationType, aRelation);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_RELATION_TARGET)
    return NS_OK; // XXX bug 381599, avoid performance problems

  if (aRelationType == nsIAccessibleRelation::RELATION_CONTROLLED_BY) {
    return nsRelUtils::
      AddTargetFromIDRefsAttr(aRelationType, aRelation, mContent,
                              nsAccessibilityAtoms::_for);
  }

  return NS_OK;
}

PRUint32
nsHTMLOutputAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_SECTION;
}

nsresult
nsHTMLOutputAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
{
  nsresult rv = nsAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::live,
                         NS_LITERAL_STRING("polite"));
  
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLLIAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLIAccessible::
  nsHTMLLIAccessible(nsIContent *aContent, nsIWeakReference *aShell,
                     const nsAString& aBulletText) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
  if (!aBulletText.IsEmpty()) {
    mBulletAccessible = new nsHTMLListBulletAccessible(mContent, mWeakShell, 
                                                       aBulletText);
    if (mBulletAccessible)
      mBulletAccessible->Init();
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLLIAccessible, nsHyperTextAccessible)

void
nsHTMLLIAccessible::Shutdown()
{
  if (mBulletAccessible) {
    // Ensure that pointer to this is nulled out.
    mBulletAccessible->Shutdown();
  }

  nsHyperTextAccessibleWrap::Shutdown();
  mBulletAccessible = nsnull;
}

PRUint32
nsHTMLLIAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LISTITEM;
}

nsresult
nsHTMLLIAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLLIAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  nsresult rv = nsAccessibleWrap::GetBounds(x, y, width, height);
  if (NS_FAILED(rv) || !mBulletAccessible) {
    return rv;
  }

  PRInt32 bulletX, bulletY, bulletWidth, bulletHeight;
  rv = mBulletAccessible->GetBounds(&bulletX, &bulletY, &bulletWidth, &bulletHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  *x = bulletX; // Move x coordinate of list item over to cover bullet as well
  *width += bulletWidth;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLIAccessible: nsAccessible protected

void
nsHTMLLIAccessible::CacheChildren()
{
  if (mBulletAccessible)
    AppendChild(mBulletAccessible);

  // Cache children from subtree.
  nsAccessibleWrap::CacheChildren();
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLListBulletAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLListBulletAccessible::
  nsHTMLListBulletAccessible(nsIContent *aContent, nsIWeakReference *aShell,
                             const nsAString& aBulletText) :
    nsLeafAccessible(aContent, aShell), mBulletText(aBulletText)
{
  mBulletText += ' '; // Otherwise bullets are jammed up against list text
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mContent is same as for list item, use |this| pointer as the unique
  // id.
  *aUniqueID = static_cast<void*>(this);
  return NS_OK;
}

void
nsHTMLListBulletAccessible::Shutdown()
{
  mBulletText.Truncate();
  nsLeafAccessible::Shutdown();
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetName(nsAString &aName)
{
  // Native anonymous content, ARIA can't be used.
  aName = mBulletText;
  return NS_OK;
}

PRUint32
nsHTMLListBulletAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_STATICTEXT;
}

nsresult
nsHTMLListBulletAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsLeafAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

nsresult
nsHTMLListBulletAccessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset,
                                         PRUint32 aLength)
{
  PRUint32 maxLength = mBulletText.Length() - aStartOffset;
  if (aLength > maxLength) {
    aLength = maxLength;
  }
  aText += Substring(mBulletText, aStartOffset, aLength);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLListAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLListAccessible::
  nsHTMLListAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLListAccessible, nsHyperTextAccessible)

PRUint32
nsHTMLListAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LIST;
}

nsresult
nsHTMLListAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

