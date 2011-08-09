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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsOuterDocAccessible.h"

#include "States.h"
#include "nsAccUtils.h"
#include "nsDocAccessible.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsOuterDocAccessible
////////////////////////////////////////////////////////////////////////////////

nsOuterDocAccessible::
  nsOuterDocAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(nsOuterDocAccessible,
                             nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public (DON'T add methods here)

PRUint32
nsOuterDocAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_INTERNAL_FRAME;
}

PRUint64
nsOuterDocAccessible::NativeState()
{
  return nsAccessible::NativeState() & ~states::FOCUSABLE;
}

nsAccessible*
nsOuterDocAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild)
{
  PRInt32 docX = 0, docY = 0, docWidth = 0, docHeight = 0;
  nsresult rv = GetBounds(&docX, &docY, &docWidth, &docHeight);
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (aX < docX || aX >= docX + docWidth || aY < docY || aY >= docY + docHeight)
    return nsnull;

  // Always return the inner doc as direct child accessible unless bounds
  // outside of it.
  nsAccessible* child = GetChildAt(0);
  NS_ENSURE_TRUE(child, nsnull);

  if (aWhichChild == eDeepestChild)
    return child->ChildAtPoint(aX, aY, eDeepestChild);
  return child;
}

nsresult
nsOuterDocAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  nsAutoString tag;
  aAttributes->GetStringProperty(NS_LITERAL_CSTRING("tag"), tag);
  if (!tag.IsEmpty()) {
    // We're overriding the ARIA attributes on an sub document, but we don't want to
    // override the other attributes
    return NS_OK;
  }
  return nsAccessible::GetAttributesInternal(aAttributes);
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

PRUint8
nsOuterDocAccessible::ActionCount()
{
  // Internal frame, which is the doc's parent, should not have a click action.
  return 0;
}

NS_IMETHODIMP
nsOuterDocAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsOuterDocAccessible::GetActionDescription(PRUint8 aIndex, nsAString& aDescription)
{
  aDescription.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsOuterDocAccessible::DoAction(PRUint8 aIndex)
{
  return NS_ERROR_INVALID_ARG;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public

void
nsOuterDocAccessible::Shutdown()
{
  // XXX: sometimes outerdoc accessible is shutdown because of layout style
  // change however the presshell of underlying document isn't destroyed and
  // the document doesn't get pagehide events. Shutdown underlying document if
  // any to avoid hanging document accessible.
  NS_LOG_ACCDOCDESTROY_MSG("A11y outerdoc shutdown")
  NS_LOG_ACCDOCDESTROY_ACCADDRESS("outerdoc", this)

  nsAccessible *childAcc = mChildren.SafeElementAt(0, nsnull);
  if (childAcc) {
    NS_LOG_ACCDOCDESTROY("outerdoc's child document shutdown",
                         childAcc->GetDocumentNode())
    childAcc->Shutdown();
  }

  nsAccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public

void
nsOuterDocAccessible::InvalidateChildren()
{
  // Do not invalidate children because nsAccDocManager is responsible for
  // document accessible lifetime when DOM document is created or destroyed. If
  // DOM document isn't destroyed but its presshell is destroyed (for example,
  // when DOM node of outerdoc accessible is hidden), then outerdoc accessible
  // notifies nsAccDocManager about this. If presshell is created for existing
  // DOM document (for example when DOM node of outerdoc accessible is shown)
  // then allow nsAccDocManager to handle this case since the document
  // accessible is created and appended as a child when it's requested.

  SetChildrenFlag(eChildrenUninitialized);
}

PRBool
nsOuterDocAccessible::AppendChild(nsAccessible *aAccessible)
{
  // We keep showing the old document for a bit after creating the new one,
  // and while building the new DOM and frame tree. That's done on purpose
  // to avoid weird flashes of default background color.
  // The old viewer will be destroyed after the new one is created.
  // For a11y, it should be safe to shut down the old document now.
  if (mChildren.Length())
    mChildren[0]->Shutdown();

  if (!nsAccessible::AppendChild(aAccessible))
    return PR_FALSE;

  NS_LOG_ACCDOCCREATE("append document to outerdoc",
                      aAccessible->GetDocumentNode())
  NS_LOG_ACCDOCCREATE_ACCADDRESS("outerdoc", this)

  return PR_TRUE;
}

PRBool
nsOuterDocAccessible::RemoveChild(nsAccessible *aAccessible)
{
  nsAccessible *child = mChildren.SafeElementAt(0, nsnull);
  if (child != aAccessible) {
    NS_ERROR("Wrong child to remove!");
    return PR_FALSE;
  }

  NS_LOG_ACCDOCDESTROY_FOR("remove document from outerdoc",
                           child->GetDocumentNode(), child)
  NS_LOG_ACCDOCDESTROY_ACCADDRESS("outerdoc", this)

  PRBool wasRemoved = nsAccessible::RemoveChild(child);

  NS_ASSERTION(!mChildren.Length(),
               "This child document of outerdoc accessible wasn't removed!");

  return wasRemoved;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected

void
nsOuterDocAccessible::CacheChildren()
{
  // Request document accessible for the content document to make sure it's
  // created. It will appended to outerdoc accessible children asynchronously.
  nsIDocument* outerDoc = mContent->GetCurrentDoc();
  if (outerDoc) {
    nsIDocument* innerDoc = outerDoc->GetSubDocumentFor(mContent);
    if (innerDoc)
      GetAccService()->GetDocAccessible(innerDoc);
  }
}
