/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eric Vaughan <evaughan@netscape.com> (original author)
 *   Alexander Surkov <surkov.alexander@gmail.com>
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

#ifndef _nsIAccessibilityService_h_
#define _nsIAccessibilityService_h_

#include "nsIAccessibleRetrieval.h"
#include "nsIAccessibleEvent.h"

#include "nsAutoPtr.h"

class nsAccessible;
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
  virtual nsAccessible* GetAccessibleInShell(nsINode* aNode,
                                             nsIPresShell* aPresShell) = 0;

  /**
   * Return root document accessible that is or contains a document accessible
   * for the given presshell.
   *
   * @param aPresShell  [in] the presshell
   * @param aCanCreate  [in] points whether the root document accessible
   *                        should be returned from the cache or can be created
   */
  virtual nsAccessible* GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                                  bool aCanCreate) = 0;

  /**
   * Creates accessible for the given DOM node or frame.
   */
  virtual already_AddRefed<nsAccessible>
    CreateHTMLBRAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTML4ButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLCaptionAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLCheckboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLComboboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLGroupboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLHRAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLImageAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLLabelAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLLIAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLListboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLMediaAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLObjectFrameAccessible(nsObjectFrame* aFrame, nsIContent* aContent,
                                    nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLRadioButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTableAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTableCellAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTextAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTextFieldAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateHyperTextAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;
  virtual already_AddRefed<nsAccessible>
    CreateOuterDocAccessible(nsIContent* aContent, nsIPresShell* aPresShell) = 0;

  /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  virtual nsAccessible* AddNativeRootAccessible(void* aAtkAccessible) = 0;
  virtual void RemoveNativeRootAccessible(nsAccessible* aRootAccessible) = 0;

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
   * Recreate an accessible for the given content node in the presshell.
   */
  virtual void RecreateAccessible(nsIPresShell* aPresShell,
                                  nsIContent* aContent) = 0;

  /**
   * Fire accessible event of the given type for the given target.
   *
   * @param aEvent   [in] accessible event type
   * @param aTarget  [in] target of accessible event
   */
  virtual void FireAccessibleEvent(PRUint32 aEvent, nsAccessible* aTarget) = 0;
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
