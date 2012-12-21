/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OuterDocAccessible.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#ifdef A11Y_LOG
#include "Logging.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// OuterDocAccessible
////////////////////////////////////////////////////////////////////////////////

OuterDocAccessible::
  OuterDocAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

OuterDocAccessible::~OuterDocAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(OuterDocAccessible,
                             Accessible)

////////////////////////////////////////////////////////////////////////////////
// Accessible public (DON'T add methods here)

role
OuterDocAccessible::NativeRole()
{
  return roles::INTERNAL_FRAME;
}

Accessible*
OuterDocAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                                 EWhichChildAtPoint aWhichChild)
{
  int32_t docX = 0, docY = 0, docWidth = 0, docHeight = 0;
  nsresult rv = GetBounds(&docX, &docY, &docWidth, &docHeight);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (aX < docX || aX >= docX + docWidth || aY < docY || aY >= docY + docHeight)
    return nullptr;

  // Always return the inner doc as direct child accessible unless bounds
  // outside of it.
  Accessible* child = GetChildAt(0);
  NS_ENSURE_TRUE(child, nullptr);

  if (aWhichChild == eDeepestChild)
    return child->ChildAtPoint(aX, aY, eDeepestChild);
  return child;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

uint8_t
OuterDocAccessible::ActionCount()
{
  // Internal frame, which is the doc's parent, should not have a click action.
  return 0;
}

NS_IMETHODIMP
OuterDocAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
OuterDocAccessible::GetActionDescription(uint8_t aIndex,
                                         nsAString& aDescription)
{
  aDescription.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
OuterDocAccessible::DoAction(uint8_t aIndex)
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
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocDestroy))
    logging::OuterDocDestroy(this);
#endif

  Accessible* childAcc = mChildren.SafeElementAt(0, nullptr);
  if (childAcc) {
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocDestroy)) {
      logging::DocDestroy("outerdoc's child document shutdown",
                          childAcc->AsDoc()->DocumentNode());
    }
#endif
    childAcc->Shutdown();
  }

  AccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// Accessible public

void
OuterDocAccessible::InvalidateChildren()
{
  // Do not invalidate children because DocManager is responsible for
  // document accessible lifetime when DOM document is created or destroyed. If
  // DOM document isn't destroyed but its presshell is destroyed (for example,
  // when DOM node of outerdoc accessible is hidden), then outerdoc accessible
  // notifies DocManager about this. If presshell is created for existing
  // DOM document (for example when DOM node of outerdoc accessible is shown)
  // then allow DocManager to handle this case since the document
  // accessible is created and appended as a child when it's requested.

  SetChildrenFlag(eChildrenUninitialized);
}

bool
OuterDocAccessible::AppendChild(Accessible* aAccessible)
{
  // We keep showing the old document for a bit after creating the new one,
  // and while building the new DOM and frame tree. That's done on purpose
  // to avoid weird flashes of default background color.
  // The old viewer will be destroyed after the new one is created.
  // For a11y, it should be safe to shut down the old document now.
  if (mChildren.Length())
    mChildren[0]->Shutdown();

  if (!AccessibleWrap::AppendChild(aAccessible))
    return false;

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocCreate)) {
    logging::DocCreate("append document to outerdoc",
                       aAccessible->AsDoc()->DocumentNode());
    logging::Address("outerdoc", this);
  }
#endif

  return true;
}

bool
OuterDocAccessible::RemoveChild(Accessible* aAccessible)
{
  Accessible* child = mChildren.SafeElementAt(0, nullptr);
  if (child != aAccessible) {
    NS_ERROR("Wrong child to remove!");
    return false;
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocDestroy)) {
    logging::DocDestroy("remove document from outerdoc",
                        child->AsDoc()->DocumentNode(), child->AsDoc());
    logging::Address("outerdoc", this);
  }
#endif

  bool wasRemoved = AccessibleWrap::RemoveChild(child);

  NS_ASSERTION(!mChildren.Length(),
               "This child document of outerdoc accessible wasn't removed!");

  return wasRemoved;
}


////////////////////////////////////////////////////////////////////////////////
// Accessible protected

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
