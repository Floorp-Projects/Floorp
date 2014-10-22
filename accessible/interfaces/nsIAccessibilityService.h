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

namespace mozilla {
namespace a11y {

class Accessible;

} // namespace a11y
} // namespace mozilla

class nsINode;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsPluginFrame;

// 0e7e6879-854b-4260-bc6e-525b5fb5cf34
#define NS_IACCESSIBILITYSERVICE_IID \
{ 0x0e7e6879, 0x854b, 0x4260, \
 { 0xbc, 0x6e, 0x52, 0x5b, 0x5f, 0xb5, 0xcf, 0x34 } }

class nsIAccessibilityService : public nsIAccessibleRetrieval
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IACCESSIBILITYSERVICE_IID)

  /**
   * Return root document accessible that is or contains a document accessible
   * for the given presshell.
   *
   * @param aPresShell  [in] the presshell
   * @param aCanCreate  [in] points whether the root document accessible
   *                        should be returned from the cache or can be created
   */
  virtual mozilla::a11y::Accessible*
    GetRootDocumentAccessible(nsIPresShell* aPresShell, bool aCanCreate) = 0;

   /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  virtual mozilla::a11y::Accessible*
    AddNativeRootAccessible(void* aAtkAccessible) = 0;
  virtual void
    RemoveNativeRootAccessible(mozilla::a11y::Accessible* aRootAccessible) = 0;

  /**
   * Fire accessible event of the given type for the given target.
   *
   * @param aEvent   [in] accessible event type
   * @param aTarget  [in] target of accessible event
   */
  virtual void FireAccessibleEvent(uint32_t aEvent,
                                   mozilla::a11y::Accessible* aTarget) = 0;

  /**
   * Return true if the given DOM node has accessible object.
   */
  virtual bool HasAccessible(nsIDOMNode* aDOMNode) = 0;
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
