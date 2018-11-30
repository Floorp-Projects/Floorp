/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This sort service is used to sort content by attribute.
 */

#ifndef nsXULSortService_h
#define nsXULSortService_h

#include "nsAString.h"
#include "nsError.h"

namespace mozilla {

namespace dom {
class Element;
}  // namespace dom

/**
 * Sort the contents of the widget containing <code>aNode</code>
 * using <code>aSortKey</code> as the comparison key, and
 * <code>aSortDirection</code> as the direction.
 *
 * @param aNode A node in the XUL widget whose children are to be sorted.
 * @param aSortKey The value to be used as the comparison key.
 * @param aSortHints One or more hints as to how to sort:
 *
 *   ascending: to sort the contents in ascending order
 *   descending: to sort the contents in descending order
 *   comparecase: perform case sensitive comparisons
 *   integer: treat values as integers, non-integers are compared as strings
 *   twostate: don't allow the natural (unordered state)
 */
nsresult XULWidgetSort(dom::Element* aNode, const nsAString& aSortKey,
                       const nsAString& aSortHints);

}  // namespace mozilla

#endif  // nsXULSortService_h
