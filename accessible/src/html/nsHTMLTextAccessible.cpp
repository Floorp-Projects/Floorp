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
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "States.h"

#include "nsIAccessibleRelation.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsBlockFrame.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::a11y;

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
  aName = mText;
  return NS_OK;
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

PRUint64
nsHTMLTextAccessible::NativeState()
{
  PRUint64 state = nsTextAccessible::NativeState();

  nsDocAccessible *docAccessible = GetDocAccessible();
  if (docAccessible) {
     PRUint64 docState = docAccessible->State();
     if (0 == (docState & states::EDITABLE)) {
       state |= states::READONLY; // Links not focusable in editor
     }
  }

  return state;
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

PRUint64
nsHTMLBRAccessible::NativeState()
{
  return states::READONLY;
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

Relation
nsHTMLOutputAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsAccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_CONTROLLED_BY)
    rel.AppendIter(new IDRefsIterator(mContent, nsAccessibilityAtoms::_for));

  return rel;
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
  nsHTMLLIAccessible(nsIContent* aContent, nsIWeakReference* aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell), mBullet(nsnull)
{
  mFlags |= eHTMLListItemAccessible;

  nsBlockFrame* blockFrame = do_QueryFrame(GetFrame());
  if (blockFrame && blockFrame->HasBullet()) {
    mBullet = new nsHTMLListBulletAccessible(mContent, mWeakShell);
    if (!GetDocAccessible()->BindToDocument(mBullet, nsnull))
      mBullet = nsnull;
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLLIAccessible, nsHyperTextAccessible)

void
nsHTMLLIAccessible::Shutdown()
{
  mBullet = nsnull;

  nsHyperTextAccessibleWrap::Shutdown();
}

PRUint32
nsHTMLLIAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LISTITEM;
}

PRUint64
nsHTMLLIAccessible::NativeState()
{
  return nsHyperTextAccessibleWrap::NativeState() | states::READONLY;
}

NS_IMETHODIMP nsHTMLLIAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  nsresult rv = nsAccessibleWrap::GetBounds(x, y, width, height);
  if (NS_FAILED(rv) || !mBullet)
    return rv;

  PRInt32 bulletX, bulletY, bulletWidth, bulletHeight;
  rv = mBullet->GetBounds(&bulletX, &bulletY, &bulletWidth, &bulletHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  *x = bulletX; // Move x coordinate of list item over to cover bullet as well
  *width += bulletWidth;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLIAccessible: public

void
nsHTMLLIAccessible::UpdateBullet(bool aHasBullet)
{
  if (aHasBullet == !!mBullet) {
    NS_NOTREACHED("Bullet and accessible are in sync already!");
    return;
  }

  nsDocAccessible* document = GetDocAccessible();
  if (aHasBullet) {
    mBullet = new nsHTMLListBulletAccessible(mContent, mWeakShell);
    if (document->BindToDocument(mBullet, nsnull)) {
      InsertChildAt(0, mBullet);
    }
  } else {
    RemoveChild(mBullet);
    document->UnbindFromDocument(mBullet);
    mBullet = nsnull;
  }

  // XXXtodo: fire show/hide and reorder events. That's hard to make it
  // right now because coalescence happens by DOM node.
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLIAccessible: nsAccessible protected

void
nsHTMLLIAccessible::CacheChildren()
{
  if (mBullet)
    AppendChild(mBullet);

  // Cache children from subtree.
  nsAccessibleWrap::CacheChildren();
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLListBulletAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLListBulletAccessible::
  nsHTMLListBulletAccessible(nsIContent* aContent, nsIWeakReference* aShell) :
    nsLeafAccessible(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLListBulletAccessible: nsAccessNode

bool
nsHTMLListBulletAccessible::IsPrimaryForNode() const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLListBulletAccessible: nsAccessible

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetName(nsAString &aName)
{
  aName.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Native anonymous content, ARIA can't be used. Get list bullet text.
  nsBlockFrame* blockFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  NS_ASSERTION(blockFrame, "No frame for list item!");
  if (blockFrame) {
    blockFrame->GetBulletText(aName);

    // Append space otherwise bullets are jammed up against list text.
    aName.Append(' ');
  }

  return NS_OK;
}

PRUint32
nsHTMLListBulletAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_STATICTEXT;
}

PRUint64
nsHTMLListBulletAccessible::NativeState()
{
  PRUint64 state = nsLeafAccessible::NativeState();

  state &= ~states::FOCUSABLE;
  state |= states::READONLY;
  return state;
}

void
nsHTMLListBulletAccessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset,
                                         PRUint32 aLength)
{
  nsAutoString bulletText;
  nsBlockFrame* blockFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  NS_ASSERTION(blockFrame, "No frame for list item!");
  if (blockFrame)
    blockFrame->GetBulletText(bulletText);

  aText.Append(Substring(bulletText, aStartOffset, aLength));
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

PRUint64
nsHTMLListAccessible::NativeState()
{
  return nsHyperTextAccessibleWrap::NativeState() | states::READONLY;
}

