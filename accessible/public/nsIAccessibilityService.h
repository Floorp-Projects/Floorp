/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsIAccessibilityService_h_
#define _nsIAccessibilityService_h_

#include "nsIAccessibleRetrieval.h"
#include "nsIAccessibleEvent.h"

#include "nsAutoPtr.h"

class Accessible;
class nsINode;
class nsIContent;
class nsIDocument;
class nsIFrame;
class nsIPresShell;
class nsObjectFrame;

// 10ff6dca-b219-4b64-9a4c-67a62b86edce
#define NS_IACCESSIBILITYSERVICE_IID \
{ 0x10ff6dca, 0xb219, 0x4b64, \
 { 0x9a, 0x4c, 0x67, 0xa6, 0x2b, 0x86, 0xed, 0xce } }

class nsIAccessibilityService : public nsIAccessibleRetrieval
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IACCESSIBILITYSERVICE_IID)

  /**
   * Return an accessible object for a DOM node in the given pres shell.
   *
   * @param  aNode      [in] the DOM node to get an accessible for
   * @param  aPresShell [in] the presentation shell which contains layout info
   *                         for the DOM node
   */
  virtual Accessible* GetAccessible(nsINode* aNode,
                                    nsIPresShell* aPresShell) = 0;

  /**
   * Return root document accessible that is or contains a document accessible
   * for the given presshell.
   *
   * @param aPresShell  [in] the presshell
   * @param aCanCreate  [in] points whether the root document accessible
   *                        should be returned from the cache or can be created
   */
  virtual Accessible* GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                                bool aCanCreate) = 0;

   /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  virtual Accessible* AddNativeRootAccessible(void* aAtkAccessible) = 0;
  virtual void RemoveNativeRootAccessible(Accessible* aRootAccessible) = 0;

  /**
   * Notification used to update the accessible tree when new content is
   * inserted.
   */
  virtual void ContentRangeInserted(nsIPresShell* aPresShell,
                                    nsIContent* aContainer,
                                    nsIContent* aStartChild,
                                    nsIContent* aEndChild) = 0;

  /**
   * Notification used to update the accessible tree when content is removed.
   */
  virtual void ContentRemoved(nsIPresShell* aPresShell, nsIContent* aContainer,
                              nsIContent* aChild) = 0;

  /**
   * Notify accessibility that anchor jump has been accomplished to the given
   * target. Used by layout.
   */
  virtual void NotifyOfAnchorJumpTo(nsIContent *aTarget) = 0;

  /**
   * Notify the accessibility service that the given presshell is
   * being destroyed.
   */
  virtual void PresShellDestroyed(nsIPresShell *aPresShell) = 0;

  /**
   * Fire accessible event of the given type for the given target.
   *
   * @param aEvent   [in] accessible event type
   * @param aTarget  [in] target of accessible event
   */
  virtual void FireAccessibleEvent(PRUint32 aEvent, Accessible* aTarget) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIAccessibilityService,
                              NS_IACCESSIBILITYSERVICE_IID)

// for component registration
// {DE401C37-9A7F-4278-A6F8-3DE2833989EF}
#define NS_ACCESSIBILITY_SERVICE_CID \
{ 0xde401c37, 0x9a7f, 0x4278, { 0xa6, 0xf8, 0x3d, 0xe2, 0x83, 0x39, 0x89, 0xef } }

extern nsresult
NS_GetAccessibilityService(nsIAccessibilityService** aResult);

#endif
