/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *   Ginn Chen (ginn.chen@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsMai.h"
#include "nsRootAccessibleWrap.h"
#include "nsAppRootAccessible.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIFocusController.h"

#ifdef MOZ_XUL
#include "nsIAccessibleTreeCache.h"
#endif

nsRootAccessibleWrap::nsRootAccessibleWrap(nsIDOMNode *aDOMNode,
                                           nsIWeakReference* aShell):
    nsRootAccessible(aDOMNode, aShell)
{
    MAI_LOG_DEBUG(("New Root Acc=%p\n", (void*)this));
}

NS_IMETHODIMP nsRootAccessibleWrap::Init()
{
    nsresult rv = nsRootAccessible::Init();
    nsAppRootAccessible *root = nsAppRootAccessible::Create();
    if (root) {
        root->AddRootAccessible(this);
    }
    return rv;
}

nsRootAccessibleWrap::~nsRootAccessibleWrap()
{
    MAI_LOG_DEBUG(("Delete Root Acc=%p\n", (void*)this));
}

NS_IMETHODIMP nsRootAccessibleWrap::Shutdown()
{
    nsAppRootAccessible *root = nsAppRootAccessible::Create();
    if (root) {
        root->RemoveRootAccessible(this);
    }
    return nsRootAccessible::Shutdown();
}

NS_IMETHODIMP nsRootAccessibleWrap::GetParent(nsIAccessible **  aParent)
{
    nsAppRootAccessible *root = nsAppRootAccessible::Create();
    nsresult rv = NS_OK;
    if (root) {
        NS_IF_ADDREF(*aParent = root);
    }
    else {
        *aParent = nsnull;
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

/* virtual */
nsresult nsRootAccessibleWrap::HandleEventWithTarget(nsIDOMEvent *aEvent,
                                                     nsIDOMNode  *aTargetNode)
{
    // first let the cross-platform code dispatch any events, before
    // we start doing platform-specific things
    nsRootAccessible::HandleEventWithTarget(aEvent, aTargetNode);
    
    nsAutoString eventType;
    aEvent->GetType(eventType);
    nsAutoString localName;
    aTargetNode->GetLocalName(localName);

    if (eventType.LowerCaseEqualsLiteral("pagehide")) {
      // nsRootAccessible::HandleEventWithTarget() has destoryed the accessible object
      // we don't want to create it again
      return NS_OK;
    }
    
    nsCOMPtr<nsIAccessible> accessible;
    nsCOMPtr<nsIAccessibilityService> accService = GetAccService();
    accService->GetAccessibleFor(aTargetNode, getter_AddRefs(accessible));
    if (!accessible)
      return NS_OK;

    nsCOMPtr<nsPIAccessible> privAcc(do_QueryInterface(accessible));
    
#ifdef MOZ_XUL
  // If it's a tree element, need the currently selected item
    nsCOMPtr<nsIAccessible> treeItemAccessible;
    if (localName.EqualsLiteral("tree")) {
        nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
            do_QueryInterface(aTargetNode);
        if (multiSelect) {
            PRInt32 treeIndex = -1;
            multiSelect->GetCurrentIndex(&treeIndex);
            if (treeIndex >= 0) {
                nsCOMPtr<nsIAccessibleTreeCache> treeCache(do_QueryInterface(accessible));
                if (!treeCache || 
                    NS_FAILED(treeCache->GetCachedTreeitemAccessible(
                    treeIndex,
                    nsnull,
                    getter_AddRefs(treeItemAccessible))) ||
                    !treeItemAccessible) {
                        return NS_ERROR_OUT_OF_MEMORY;
                }
                accessible = treeItemAccessible;
            }
        }
    }
#endif
  
    StateChange stateData;
    if (eventType.LowerCaseEqualsLiteral("focus")) {
#ifdef MOZ_XUL
        if (treeItemAccessible) { // use focused treeitem
            privAcc = do_QueryInterface(treeItemAccessible);
            privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                                      treeItemAccessible, nsnull);
            accessible = treeItemAccessible;
        }
        else
#endif 
        if (localName.EqualsIgnoreCase("radiogroup")) {
            // fire focus event for checked radio instead of radiogroup
            PRInt32 childCount = 0;
            accessible->GetChildCount(&childCount);
            nsCOMPtr<nsIAccessible> radioAcc;
            for (PRInt32 index = 0; index < childCount; index++) {
                accessible->GetChildAt(index, getter_AddRefs(radioAcc));
                if (radioAcc) {
                    radioAcc->GetFinalState(&stateData.state);
                    if (stateData.state & (nsIAccessibleStates::STATE_CHECKED |
                        nsIAccessibleStates::STATE_SELECTED)) {
                        break;
                    }
                }
            }
            accessible = radioAcc;
            if (radioAcc) {
                privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, radioAcc, nsnull);
            }
        }
        else {
            FireAccessibleFocusEvent(accessible, aTargetNode, aEvent);
        }
        if (accessible) {
            // Fire state change event for focus
            stateData.enable = PR_TRUE;
            stateData.state = nsIAccessibleStates::STATE_FOCUSED;
            privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, accessible,
                                      &stateData);
        }
    }
    else if (eventType.LowerCaseEqualsLiteral("select")) {
#ifdef MOZ_XUL
        if (treeItemAccessible) { // it's a XUL <tree>
            // use EVENT_FOCUS instead of EVENT_ATK_SELECTION_CHANGE
            privAcc = do_QueryInterface(treeItemAccessible);
            privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_FOCUS, 
                                    treeItemAccessible, nsnull);
        }
        else 
#endif
        if (localName.LowerCaseEqualsLiteral("tabpanels")) {
            // make GOK refresh "UI-Grab" window
            privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_REORDER, accessible, nsnull);
        }
    }
    else if (eventType.LowerCaseEqualsLiteral("checkboxstatechange") || // it's a XUL <checkbox>
             eventType.LowerCaseEqualsLiteral("radiostatechange")) { // it's a XUL <radio>
        accessible->GetFinalState(&stateData.state);
        // prefPane tab is implemented as list items in A11y, so we need to
        // check nsIAccessibleStates::STATE_SELECTED also
        stateData.enable = (stateData.state &
          (nsIAccessibleStates::STATE_CHECKED |
           nsIAccessibleStates::STATE_SELECTED)) != 0;
        stateData.state = nsIAccessibleStates::STATE_CHECKED;
        privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, accessible, &stateData);
        // only fire focus event for checked radio
        if (eventType.LowerCaseEqualsLiteral("radiostatechange") &&
            stateData.enable) {
            FireAccessibleFocusEvent(accessible, aTargetNode, aEvent);
        }
    }
    else if (eventType.LowerCaseEqualsLiteral("openstatechange")) { // collapsed/expanded changed
        accessible->GetFinalState(&stateData.state);
        stateData.enable = (stateData.state & nsIAccessibleStates::STATE_EXPANDED) != 0;
        stateData.state = nsIAccessibleStates::STATE_EXPANDED;
        privAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, accessible, &stateData);
    }
    else if (eventType.LowerCaseEqualsLiteral("popuphiding")) {
        // If accessible focus was inside popup that closes,
        // then restore it to true current focus.
        // This is the case when we've been getting DOMMenuItemActive events
        // inside of a combo box that closes. The real focus is on the combo box.
        if (!gLastFocusedNode) {
            return NS_OK;
        }
        nsCOMPtr<nsIDOMNode> parentOfFocus;
        gLastFocusedNode->GetParentNode(getter_AddRefs(parentOfFocus));
        if (parentOfFocus != aTargetNode) {
            return NS_OK;
        }
        // Focus was inside of popup that's being hidden
        FireCurrentFocusEvent();
    }
    else if (eventType.LowerCaseEqualsLiteral("popupshown")) {
#ifdef MOZ_XUL
      nsCOMPtr<nsIContent> content(do_QueryInterface(aTargetNode));
      if (content->NodeInfo()->Equals(nsAccessibilityAtoms::tooltip, kNameSpaceID_XUL) ||
          content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                               NS_LITERAL_STRING("autocomplete"), eIgnoreCase)) {
        // 1) Don't fire focus events for tooltips, that wouldn't make any sense.
        // 2) Don't fire focus events for autocomplete popups, because they come up
        // automatically while the user is typing, and setting focus there would
        // interrupt the user.
        // ------------------------------------------------------------------------
        // If the AT wants to know about these popups it can track the ATK state change
        // event we fire for ATK_STATE_INVISIBLE on the popup.
        // This is fired as a result of the nsIAccessibleEvent::EVENT_MENUPOPUPSTART 
        // we fire in the nsRootAccessible event handling for all popups.
        return NS_OK;
      }
#endif
      FireAccessibleFocusEvent(accessible, aTargetNode, aEvent);      
    }
    return NS_OK;
}

nsNativeRootAccessibleWrap::nsNativeRootAccessibleWrap(AtkObject *aAccessible):
    nsRootAccessible(nsnull, nsnull)
{
    g_object_ref(aAccessible);
    nsAccessibleWrap::mAtkObject = aAccessible;
}
