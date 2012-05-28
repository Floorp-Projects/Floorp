/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OuterDocAccessible.h"

#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#ifdef DEBUG
#include "Logging.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// OuterDocAccessible
////////////////////////////////////////////////////////////////////////////////

OuterDocAccessible::
  OuterDocAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
{
}

OuterDocAccessible::~OuterDocAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(OuterDocAccessible,
                             nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public (DON'T add methods here)

role
OuterDocAccessible::NativeRole()
{
  return roles::INTERNAL_FRAME;
}

nsAccessible*
OuterDocAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
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
OuterDocAccessible::GetAttributesInternal(nsIPersistentProperties* aAttributes)
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
OuterDocAccessible::ActionCount()
{
  // Internal frame, which is the doc's parent, should not have a click action.
  return 0;
}

NS_IMETHODIMP
OuterDocAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
OuterDocAccessible::GetActionDescription(PRUint8 aIndex,
                                         nsAString& aDescription)
{
  aDescription.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
OuterDocAccessible::DoAction(PRUint8 aIndex)
{
  return NS_ERROR_INVALID_ARG;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public

void
OuterDocAccessible::Shutdown()
{
  // XXX: sometimes outerdoc accessible is shutdown because of layout style
  // change however the presshell of underlying document isn't destroyed and
  // the document doesn't get pagehide events. Shutdown underlying document if
  // any to avoid hanging document accessible.
#ifdef DEBUG
  if (logging::IsEnabled(logging::eDocDestroy)) {
    logging::Msg("A11y outerdoc shutdown");
    logging::Address("outerdoc", this);
  }
#endif

  nsAccessible* childAcc = mChildren.SafeElementAt(0, nsnull);
  if (childAcc) {
#ifdef DEBUG
    if (logging::IsEnabled(logging::eDocDestroy)) {
      logging::DocDestroy("outerdoc's child document shutdown",
                      childAcc->GetDocumentNode());
    }
#endif
    childAcc->Shutdown();
  }

  nsAccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public

void
OuterDocAccessible::InvalidateChildren()
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

bool
OuterDocAccessible::AppendChild(nsAccessible* aAccessible)
{
  // We keep showing the old document for a bit after creating the new one,
  // and while building the new DOM and frame tree. That's done on purpose
  // to avoid weird flashes of default background color.
  // The old viewer will be destroyed after the new one is created.
  // For a11y, it should be safe to shut down the old document now.
  if (mChildren.Length())
    mChildren[0]->Shutdown();

  if (!nsAccessibleWrap::AppendChild(aAccessible))
    return false;

#ifdef DEBUG
  if (logging::IsEnabled(logging::eDocCreate)) {
    logging::DocCreate("append document to outerdoc",
                   aAccessible->GetDocumentNode());
    logging::Address("outerdoc", this);
  }
#endif

  return true;
}

bool
OuterDocAccessible::RemoveChild(nsAccessible* aAccessible)
{
  nsAccessible* child = mChildren.SafeElementAt(0, nsnull);
  if (child != aAccessible) {
    NS_ERROR("Wrong child to remove!");
    return false;
  }

#ifdef DEBUG
  if (logging::IsEnabled(logging::eDocDestroy)) {
    logging::DocDestroy("remove document from outerdoc", child->GetDocumentNode(),
                    child->AsDoc());
    logging::Address("outerdoc", this);
  }
#endif

  bool wasRemoved = nsAccessibleWrap::RemoveChild(child);

  NS_ASSERTION(!mChildren.Length(),
               "This child document of outerdoc accessible wasn't removed!");

  return wasRemoved;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected

void
OuterDocAccessible::CacheChildren()
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
