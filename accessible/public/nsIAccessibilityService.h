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

class nsAccessible;
class nsIContent;
class nsIDocument;
class nsIFrame;
class nsIPresShell;
class nsObjectFrame;

// 9f43b315-53c6-4d46-9818-9c8593e91984
#define NS_IACCESSIBILITYSERVICE_IID \
{0x9f43b315, 0x53c6, 0x4d46,         \
  {0x98, 0x18, 0x9c, 0x85, 0x93, 0xe9, 0x19, 0x84} }

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
  virtual nsAccessible* GetAccessibleInShell(nsIDOMNode *aNode,
                                             nsIPresShell *aPresShell) = 0;

  /**
   * Creates accessible for the given DOM node or frame.
   */
  virtual nsresult CreateOuterDocAccessible(nsIDOMNode *aNode,
                                            nsIAccessible **aAccessible) = 0;

  virtual nsresult CreateHTML4ButtonAccessible(nsIFrame *aFrame,
                                               nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHyperTextAccessible(nsIFrame *aFrame,
                                             nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLBRAccessible(nsIFrame *aFrame,
                                          nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLButtonAccessible(nsIFrame *aFrame,
                                              nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLLIAccessible(nsIFrame *aFrame,
                                          nsIFrame *aBulletFrame,
                                          const nsAString& aBulletText,
                                          nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLCheckboxAccessible(nsIFrame *aFrame,
                                                nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLComboboxAccessible(nsIDOMNode *aNode,
                                                nsIWeakReference *aPresShell,
                                                nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLGenericAccessible(nsIFrame *aFrame,
                                               nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLGroupboxAccessible(nsIFrame *aFrame,
                                                nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLHRAccessible(nsIFrame *aFrame,
                                          nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLImageAccessible(nsIFrame *aFrame,
                                             nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLLabelAccessible(nsIFrame *aFrame,
                                             nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLListboxAccessible(nsIDOMNode *aNode,
                                               nsIWeakReference *aPresShell,
                                               nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLMediaAccessible(nsIFrame *aFrame,
                                             nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLObjectFrameAccessible(nsObjectFrame *aFrame,
                                                   nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLRadioButtonAccessible(nsIFrame *aFrame,
                                                   nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLSelectOptionAccessible(nsIDOMNode *aNode,
                                                    nsIAccessible *aAccParent,
                                                    nsIWeakReference *aPresShell,
                                                    nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLTableAccessible(nsIFrame *aFrame,
                                             nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLTableCellAccessible(nsIFrame *aFrame,
                                                 nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLTextAccessible(nsIFrame *aFrame,
                                            nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLTextFieldAccessible(nsIFrame *aFrame,
                                                 nsIAccessible **aAccessible) = 0;
  virtual nsresult CreateHTMLCaptionAccessible(nsIFrame *aFrame,
                                               nsIAccessible **aAccessible) = 0;

  /**
   * Adds/remove ATK root accessible for gtk+ native window to/from children
   * of the application accessible.
   */
  virtual nsresult AddNativeRootAccessible(void *aAtkAccessible,
                                           nsIAccessible **aAccessible) = 0;
  virtual nsresult
    RemoveNativeRootAccessible(nsIAccessible *aRootAccessible) = 0;

  /**
   * Used to describe sort of changes leading to accessible tree invalidation.
   */
  enum {
    NODE_APPEND = 0x01,
    NODE_REMOVE = 0x02,
    NODE_SIGNIFICANT_CHANGE = 0x03,
    FRAME_SHOW = 0x04,
    FRAME_HIDE = 0x05,
    FRAME_SIGNIFICANT_CHANGE = 0x06
  };

  /**
   * Invalidate the accessible tree when DOM tree or frame tree is changed.
   *
   * @param aPresShell   [in] the presShell where changes occurred
   * @param aContent     [in] the affected DOM content
   * @param aChangeType  [in] the change type (see constants declared above)
   */
  virtual nsresult InvalidateSubtreeFor(nsIPresShell *aPresShell,
                                        nsIContent *aContent,
                                        PRUint32 aChangeType) = 0;

  /**
   * Notify accessibility that anchor jump has been accomplished to the given
   * target. Used by layout.
   */
  virtual void NotifyOfAnchorJumpTo(nsIContent *aTarget) = 0;

  /**
   * Fire accessible event of the given type for the given target.
   *
   * @param aEvent   [in] accessible event type
   * @param aTarget  [in] target of accessible event
   */
  virtual nsresult FireAccessibleEvent(PRUint32 aEvent,
                                       nsIAccessible *aTarget) = 0;
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
