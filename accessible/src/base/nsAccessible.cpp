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
 *   John Gaunt (jgaunt@netscape.com)
 *   Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessible.h"

#include "nsIXBLAccessible.h"

#include "AccGroupInfo.h"
#include "AccIterator.h"
#include "nsAccUtils.h"
#include "nsARIAMap.h"
#include "nsDocAccessible.h"
#include "nsEventShell.h"

#include "nsAccEvent.h"
#include "nsAccessibilityService.h"
#include "nsAccTreeWalker.h"
#include "nsRelUtils.h"
#include "nsTextEquivUtils.h"

#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULLabelElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsPIDOMWindow.h"

#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIForm.h"
#include "nsIFormControl.h"

#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScrollableFrame.h"
#include "nsFocusManager.h"

#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "prdtoa.h"
#include "nsIAtom.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIURI.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsWhitespaceTokenizer.h"
#include "nsAttrName.h"
#include "nsNetUtil.h"
#include "nsIEventStateManager.h"

#ifdef NS_DEBUG
#include "nsIDOMCharacterData.h"
#endif

#include "mozilla/unused.h"


////////////////////////////////////////////////////////////////////////////////
// nsAccessible. nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsAccessible, nsAccessNode)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mParent");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mParent.get()));

  PRUint32 i, length = tmp->mChildren.Length();
  for (i = 0; i < length; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildren[i]");
    cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mChildren[i].get()));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsAccessible, nsAccessNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mChildren)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsAccessible, nsAccessNode)
NS_IMPL_RELEASE_INHERITED(nsAccessible, nsAccessNode)

nsresult nsAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  // Custom-built QueryInterface() knows when we support nsIAccessibleSelectable
  // based on role attribute and aria-multiselectable
  *aInstancePtr = nsnull;

  if (aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    *aInstancePtr = &NS_CYCLE_COLLECTION_NAME(nsAccessible);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIAccessible))) {
    *aInstancePtr = static_cast<nsIAccessible*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsAccessible))) {
    *aInstancePtr = static_cast<nsAccessible*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIAccessibleSelectable))) {
    if (IsSelect()) {
      *aInstancePtr = static_cast<nsIAccessibleSelectable*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_ERROR_NO_INTERFACE;
  }

  if (aIID.Equals(NS_GET_IID(nsIAccessibleValue))) {
    if (mRoleMapEntry && mRoleMapEntry->valueRule != eNoValue) {
      *aInstancePtr = static_cast<nsIAccessibleValue*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
  }                       

  if (aIID.Equals(NS_GET_IID(nsIAccessibleHyperLink))) {
    if (IsHyperLink()) {
      *aInstancePtr = static_cast<nsIAccessibleHyperLink*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_ERROR_NO_INTERFACE;
  }

  return nsAccessNodeWrap::QueryInterface(aIID, aInstancePtr);
}

nsAccessible::nsAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessNodeWrap(aContent, aShell),
  mParent(nsnull), mIndexInParent(-1), mChildrenFlags(eChildrenUninitialized),
  mIndexOfEmbeddedChild(-1), mRoleMapEntry(nsnull)
{
#ifdef NS_DEBUG_X
   {
     nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aShell));
     printf(">>> %p Created Acc - DOM: %p  PS: %p", 
            (void*)static_cast<nsIAccessible*>(this), (void*)aNode,
            (void*)shell.get());
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content) {
      nsAutoString buf;
      if (content->NodeInfo())
        content->NodeInfo()->GetQualifiedName(buf);
      printf(" Con: %s@%p", NS_ConvertUTF16toUTF8(buf).get(), (void *)content.get());
      if (NS_SUCCEEDED(GetName(buf))) {
        printf(" Name:[%s]", NS_ConvertUTF16toUTF8(buf).get());
       }
     }
     printf("\n");
   }
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessible::~nsAccessible()
{
}

void
nsAccessible::SetRoleMapEntry(nsRoleMapEntry* aRoleMapEntry)
{
  mRoleMapEntry = aRoleMapEntry;
}

NS_IMETHODIMP
nsAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  GetARIAName(aName);
  if (!aName.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIXBLAccessible> xblAccessible(do_QueryInterface(mContent));
  if (xblAccessible) {
    xblAccessible->GetAccessibleName(aName);
    if (!aName.IsEmpty())
      return NS_OK;
  }

  nsresult rv = GetNameInternal(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aName.IsEmpty())
    return NS_OK;

  // In the end get the name from tooltip.
  nsIAtom *tooltipAttr = nsnull;

  if (mContent->IsHTML())
    tooltipAttr = nsAccessibilityAtoms::title;
  else if (mContent->IsXUL())
    tooltipAttr = nsAccessibilityAtoms::tooltiptext;
  else
    return NS_OK;

  // XXX: if CompressWhiteSpace worked on nsAString we could avoid a copy.
  nsAutoString name;
  if (mContent->GetAttr(kNameSpaceID_None, tooltipAttr, name)) {
    name.CompressWhitespace();
    aName = name;
    return NS_OK_NAME_FROM_TOOLTIP;
  }

  if (rv != NS_OK_EMPTY_NAME)
    aName.SetIsVoid(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetDescription(nsAString& aDescription)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // There are 4 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. It has no DHTML describedby property
  // 3. it doesn't have an accName; or
  // 4. its title attribute already equals to its accName nsAutoString name; 

  if (!mContent->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString description;
    nsresult rv = nsTextEquivUtils::
      GetTextEquivFromIDRefs(this, nsAccessibilityAtoms::aria_describedby,
                             description);
    NS_ENSURE_SUCCESS(rv, rv);

    if (description.IsEmpty()) {
      PRBool isXUL = mContent->IsXUL();
      if (isXUL) {
        // Try XUL <description control="[id]">description text</description>
        nsIContent *descriptionContent =
          nsCoreUtils::FindNeighbourPointingToNode(mContent,
                                                   nsAccessibilityAtoms::control,
                                                   nsAccessibilityAtoms::description);

        if (descriptionContent) {
          // We have a description content node
          nsTextEquivUtils::
            AppendTextEquivFromContent(this, descriptionContent, &description);
        }
      }
      if (description.IsEmpty()) {
        nsIAtom *descAtom = isXUL ? nsAccessibilityAtoms::tooltiptext :
                                    nsAccessibilityAtoms::title;
        if (mContent->GetAttr(kNameSpaceID_None, descAtom, description)) {
          nsAutoString name;
          GetName(name);
          if (name.IsEmpty() || description == name) {
            // Don't use tooltip for a description if this object
            // has no name or the tooltip is the same as the name
            description.Truncate();
          }
        }
      }
    }
    description.CompressWhitespace();
    aDescription = description;
  }

  return NS_OK;
}

// mask values for ui.key.chromeAccess and ui.key.contentAccess
#define NS_MODIFIER_SHIFT    1
#define NS_MODIFIER_CONTROL  2
#define NS_MODIFIER_ALT      4
#define NS_MODIFIER_META     8

// returns the accesskey modifier mask used in the given node's context
// (i.e. chrome or content), or 0 if an error occurs
static PRInt32
GetAccessModifierMask(nsIContent* aContent)
{
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefBranch)
    return 0;

  // use ui.key.generalAccessKey (unless it is -1)
  PRInt32 accessKey;
  nsresult rv = prefBranch->GetIntPref("ui.key.generalAccessKey", &accessKey);
  if (NS_SUCCEEDED(rv) && accessKey != -1) {
    switch (accessKey) {
      case nsIDOMKeyEvent::DOM_VK_SHIFT:   return NS_MODIFIER_SHIFT;
      case nsIDOMKeyEvent::DOM_VK_CONTROL: return NS_MODIFIER_CONTROL;
      case nsIDOMKeyEvent::DOM_VK_ALT:     return NS_MODIFIER_ALT;
      case nsIDOMKeyEvent::DOM_VK_META:    return NS_MODIFIER_META;
      default:                             return 0;
    }
  }

  // get the docShell to this DOMNode, return 0 on failure
  nsCOMPtr<nsIDocument> document = aContent->GetCurrentDoc();
  if (!document)
    return 0;
  nsCOMPtr<nsISupports> container = document->GetContainer();
  if (!container)
    return 0;
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));
  if (!treeItem)
    return 0;

  // determine the access modifier used in this context
  PRInt32 itemType, accessModifierMask = 0;
  treeItem->GetItemType(&itemType);
  switch (itemType) {

  case nsIDocShellTreeItem::typeChrome:
    rv = prefBranch->GetIntPref("ui.key.chromeAccess", &accessModifierMask);
    break;

  case nsIDocShellTreeItem::typeContent:
    rv = prefBranch->GetIntPref("ui.key.contentAccess", &accessModifierMask);
    break;
  }

  return NS_SUCCEEDED(rv) ? accessModifierMask : 0;
}

NS_IMETHODIMP
nsAccessible::GetKeyboardShortcut(nsAString& aAccessKey)
{
  aAccessKey.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 key = nsCoreUtils::GetAccessKeyFor(mContent);
  if (!key && mContent->IsElement()) {
    // Copy access key from label node unless it is labeled
    // via an ancestor <label>, in which case that would be redundant
    nsCOMPtr<nsIContent> labelContent(nsCoreUtils::GetLabelContent(mContent));
    if (labelContent && !nsCoreUtils::IsAncestorOf(labelContent, mContent))
      key = nsCoreUtils::GetAccessKeyFor(labelContent);
  }

  if (!key)
    return NS_OK;

  nsAutoString accesskey(key);

  // Append the modifiers in reverse order, result: Control+Alt+Shift+Meta+<key>
  nsAutoString propertyKey;
  PRInt32 modifierMask = GetAccessModifierMask(mContent);
  if (modifierMask & NS_MODIFIER_META) {
    propertyKey.AssignLiteral("VK_META");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }
  if (modifierMask & NS_MODIFIER_SHIFT) {
    propertyKey.AssignLiteral("VK_SHIFT");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }
  if (modifierMask & NS_MODIFIER_ALT) {
    propertyKey.AssignLiteral("VK_ALT");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }
  if (modifierMask & NS_MODIFIER_CONTROL) {
    propertyKey.AssignLiteral("VK_CONTROL");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }

  aAccessKey = accesskey;
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetParent(nsIAccessible **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  NS_IF_ADDREF(*aParent = GetParent());
  return *aParent ? NS_OK : NS_ERROR_FAILURE;
}

  /* readonly attribute nsIAccessible nextSibling; */
NS_IMETHODIMP
nsAccessible::GetNextSibling(nsIAccessible **aNextSibling) 
{
  NS_ENSURE_ARG_POINTER(aNextSibling);

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aNextSibling = GetSiblingAtOffset(1, &rv));
  return rv;
}

  /* readonly attribute nsIAccessible previousSibling; */
NS_IMETHODIMP
nsAccessible::GetPreviousSibling(nsIAccessible * *aPreviousSibling) 
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aPreviousSibling = GetSiblingAtOffset(-1, &rv));
  return rv;
}

  /* readonly attribute nsIAccessible firstChild; */
NS_IMETHODIMP
nsAccessible::GetFirstChild(nsIAccessible **aFirstChild) 
{
  NS_ENSURE_ARG_POINTER(aFirstChild);
  *aFirstChild = nsnull;

  if (gIsCacheDisabled)
    InvalidateChildren();

  PRInt32 childCount = GetChildCount();
  NS_ENSURE_TRUE(childCount != -1, NS_ERROR_FAILURE);

  if (childCount > 0)
    NS_ADDREF(*aFirstChild = GetChildAt(0));

  return NS_OK;
}

  /* readonly attribute nsIAccessible lastChild; */
NS_IMETHODIMP
nsAccessible::GetLastChild(nsIAccessible **aLastChild)
{
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nsnull;

  PRInt32 childCount = GetChildCount();
  NS_ENSURE_TRUE(childCount != -1, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aLastChild = GetChildAt(childCount - 1));
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetChildAt(PRInt32 aChildIndex, nsIAccessible **aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nsnull;

  PRInt32 childCount = GetChildCount();
  NS_ENSURE_TRUE(childCount != -1, NS_ERROR_FAILURE);

  // If child index is negative, then return last child.
  // XXX: do we really need this?
  if (aChildIndex < 0)
    aChildIndex = childCount - 1;

  nsAccessible* child = GetChildAt(aChildIndex);
  if (!child)
    return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*aChild = child);
  return NS_OK;
}

// readonly attribute nsIArray children;
NS_IMETHODIMP
nsAccessible::GetChildren(nsIArray **aOutChildren)
{
  NS_ENSURE_ARG_POINTER(aOutChildren);
  *aOutChildren = nsnull;

  PRInt32 childCount = GetChildCount();
  NS_ENSURE_TRUE(childCount != -1, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> children =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsIAccessible* child = GetChildAt(childIdx);
    children->AppendElement(child, PR_FALSE);
  }

  NS_ADDREF(*aOutChildren = children);
  return NS_OK;
}

PRBool
nsAccessible::GetAllowsAnonChildAccessibles()
{
  return PR_TRUE;
}

/* readonly attribute long childCount; */
NS_IMETHODIMP
nsAccessible::GetChildCount(PRInt32 *aChildCount) 
{
  NS_ENSURE_ARG_POINTER(aChildCount);

  *aChildCount = GetChildCount();
  return *aChildCount != -1 ? NS_OK : NS_ERROR_FAILURE;  
}

/* readonly attribute long indexInParent; */
NS_IMETHODIMP
nsAccessible::GetIndexInParent(PRInt32 *aIndexInParent)
{
  NS_ENSURE_ARG_POINTER(aIndexInParent);

  *aIndexInParent = GetIndexInParent();
  return *aIndexInParent != -1 ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAccessible::GetTranslatedString(const nsAString& aKey, nsAString& aStringOut)
{
  nsXPIDLString xsValue;

  if (!gStringBundle || 
    NS_FAILED(gStringBundle->GetStringFromName(PromiseFlatString(aKey).get(), getter_Copies(xsValue)))) 
    return NS_ERROR_FAILURE;

  aStringOut.Assign(xsValue);
  return NS_OK;
}

nsresult nsAccessible::GetFullKeyName(const nsAString& aModifierName, const nsAString& aKeyName, nsAString& aStringOut)
{
  nsXPIDLString modifierName, separator;

  if (!gKeyStringBundle ||
      NS_FAILED(gKeyStringBundle->GetStringFromName(PromiseFlatString(aModifierName).get(), 
                                                    getter_Copies(modifierName))) ||
      NS_FAILED(gKeyStringBundle->GetStringFromName(PromiseFlatString(NS_LITERAL_STRING("MODIFIER_SEPARATOR")).get(), 
                                                    getter_Copies(separator)))) {
    return NS_ERROR_FAILURE;
  }

  aStringOut = modifierName + separator + aKeyName; 
  return NS_OK;
}

PRBool nsAccessible::IsVisible(PRBool *aIsOffscreen) 
{
  // We need to know if at least a kMinPixels around the object is visible
  // Otherwise it will be marked nsIAccessibleStates::STATE_OFFSCREEN
  // The STATE_INVISIBLE flag is for elements which are programmatically hidden
  
  *aIsOffscreen = PR_TRUE;
  if (IsDefunct())
    return PR_FALSE;

  const PRUint16 kMinPixels  = 12;
   // Set up the variables we need, return false if we can't get at them all
  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  if (!shell) 
    return PR_FALSE;

  nsIFrame *frame = GetFrame();
  if (!frame) {
    return PR_FALSE;
  }

  // If visibility:hidden or visibility:collapsed then mark with STATE_INVISIBLE
  if (!frame->GetStyleVisibility()->IsVisible())
  {
      return PR_FALSE;
  }

  // We don't use the more accurate GetBoundsRect, because that is more expensive
  // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough
  // indicator
  nsSize frameSize = frame->GetSize();
  nsRectVisibility rectVisibility =
    shell->GetRectVisibility(frame, nsRect(nsPoint(0,0), frameSize),
                             nsPresContext::CSSPixelsToAppUnits(kMinPixels));

  if (frame->GetRect().IsEmpty()) {
    PRBool isEmpty = PR_TRUE;

    nsIAtom *frameType = frame->GetType();
    if (frameType == nsAccessibilityAtoms::textFrame) {
      // Zero area rects can occur in the first frame of a multi-frame text flow,
      // in which case the rendered text is not empty and the frame should not be marked invisible
      nsAutoString renderedText;
      frame->GetRenderedText (&renderedText, nsnull, nsnull, 0, 1);
      isEmpty = renderedText.IsEmpty();
    }
    else if (frameType == nsAccessibilityAtoms::inlineFrame) {
      // Yuck. Unfortunately inline frames can contain larger frames inside of them,
      // so we can't really believe this is a zero area rect without checking more deeply.
      // GetBounds() will do that for us.
      PRInt32 x, y, width, height;
      GetBounds(&x, &y, &width, &height);
      isEmpty = width == 0 || height == 0;
    }

    if (isEmpty && !(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
      // Consider zero area objects hidden unless they are absolutely positioned
      // or floating and may have descendants that have a non-zero size
      return PR_FALSE;
    }
  }

  // The frame intersects the viewport, but we need to check the parent view chain :(
  nsIDocument* doc = mContent->GetOwnerDoc();
  if (!doc)  {
    return PR_FALSE;
  }

  nsIFrame* frameWithView =
    frame->HasView() ? frame : frame->GetAncestorWithViewExternal();
  nsIView* view = frameWithView->GetViewExternal();
  PRBool isVisible = CheckVisibilityInParentChain(doc, view);
  if (isVisible && rectVisibility == nsRectVisibility_kVisible) {
    *aIsOffscreen = PR_FALSE;
  }
  return isVisible;
}

nsresult
nsAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  if (aExtraState)
    *aExtraState = 0;

  nsEventStates intrinsicState = mContent->IntrinsicState();

  if (intrinsicState.HasState(NS_EVENT_STATE_INVALID))
    *aState |= nsIAccessibleStates::STATE_INVALID;

  if (intrinsicState.HasState(NS_EVENT_STATE_REQUIRED))
    *aState |= nsIAccessibleStates::STATE_REQUIRED;

  PRBool disabled = mContent->IsHTML() ? 
    (intrinsicState.HasState(NS_EVENT_STATE_DISABLED)) :
    (mContent->AttrValueIs(kNameSpaceID_None,
                           nsAccessibilityAtoms::disabled,
                           nsAccessibilityAtoms::_true,
                           eCaseMatters));

  // Set unavailable state based on disabled state, otherwise set focus states
  if (disabled) {
    *aState |= nsIAccessibleStates::STATE_UNAVAILABLE;
  }
  else if (mContent->IsElement()) {
    nsIFrame *frame = GetFrame();
    if (frame && frame->IsFocusable()) {
      *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
    }

    if (gLastFocusedNode == mContent) {
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
    }
  }

  // Check if nsIAccessibleStates::STATE_INVISIBLE and
  // STATE_OFFSCREEN flags should be turned on for this object.
  PRBool isOffscreen;
  if (!IsVisible(&isOffscreen)) {
    *aState |= nsIAccessibleStates::STATE_INVISIBLE;
  }
  if (isOffscreen) {
    *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
  }

  nsIFrame *frame = GetFrame();
  if (frame && (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW))
    *aState |= nsIAccessibleStates::STATE_FLOATING;

  // Check if a XUL element has the popup attribute (an attached popup menu).
  if (mContent->IsXUL())
    if (mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::popup))
      *aState |= nsIAccessibleStates::STATE_HASPOPUP;

  // Add 'linked' state for simple xlink.
  if (nsCoreUtils::IsXLink(mContent))
    *aState |= nsIAccessibleStates::STATE_LINKED;

  return NS_OK;
}

  /* readonly attribute boolean focusedChild; */
NS_IMETHODIMP
nsAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{ 
  nsAccessible *focusedChild = nsnull;
  if (gLastFocusedNode == mContent) {
    focusedChild = this;
  }
  else if (gLastFocusedNode) {
    focusedChild = GetAccService()->GetAccessible(gLastFocusedNode);
    if (focusedChild && focusedChild->GetParent() != this)
      focusedChild = nsnull;
  }

  NS_IF_ADDREF(*aFocusedChild = focusedChild);
  return NS_OK;
}

// nsAccessible::GetChildAtPoint()
nsresult
nsAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY, PRBool aDeepestChild,
                              nsIAccessible **aChild)
{
  // If we can't find the point in a child, we will return the fallback answer:
  // we return |this| if the point is within it, otherwise nsnull.
  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = GetBounds(&x, &y, &width, &height);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessible> fallbackAnswer;
  if (aX >= x && aX < x + width && aY >= y && aY < y + height)
    fallbackAnswer = this;

  if (nsAccUtils::MustPrune(this)) {  // Do not dig any further
    NS_IF_ADDREF(*aChild = fallbackAnswer);
    return NS_OK;
  }

  // Search an accessible at the given point starting from accessible document
  // because containing block (see CSS2) for out of flow element (for example,
  // absolutely positioned element) may be different from its DOM parent and
  // therefore accessible for containing block may be different from accessible
  // for DOM parent but GetFrameForPoint() should be called for containing block
  // to get an out of flow element.
  nsDocAccessible *accDocument = GetDocAccessible();
  NS_ENSURE_TRUE(accDocument, NS_ERROR_FAILURE);

  nsIFrame *frame = accDocument->GetFrame();
  NS_ENSURE_STATE(frame);

  nsPresContext *presContext = frame->PresContext();

  nsIntRect screenRect = frame->GetScreenRectExternal();
  nsPoint offset(presContext->DevPixelsToAppUnits(aX - screenRect.x),
                 presContext->DevPixelsToAppUnits(aY - screenRect.y));

  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
  nsIFrame *foundFrame = presShell->GetFrameForPoint(frame, offset);

  nsIContent* content = nsnull;
  if (!foundFrame || !(content = foundFrame->GetContent())) {
    NS_IF_ADDREF(*aChild = fallbackAnswer);
    return NS_OK;
  }

  // Get accessible for the node with the point or the first accessible in
  // the DOM parent chain.
  nsAccessible* accessible =
   GetAccService()->GetAccessibleOrContainer(content, mWeakShell);
  if (!accessible) {
    NS_IF_ADDREF(*aChild = fallbackAnswer);
    return NS_OK;
  }

  if (accessible == this) {
    // Manually walk through accessible children and see if the are within this
    // point. Skip offscreen or invisible accessibles. This takes care of cases
    // where layout won't walk into things for us, such as image map areas and
    // sub documents (XXX: subdocuments should be handled by methods of
    // nsOuterDocAccessibles).
    PRInt32 childCount = GetChildCount();
    for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
      nsAccessible *child = GetChildAt(childIdx);

      PRInt32 childX, childY, childWidth, childHeight;
      child->GetBounds(&childX, &childY, &childWidth, &childHeight);
      if (aX >= childX && aX < childX + childWidth &&
          aY >= childY && aY < childY + childHeight &&
          (nsAccUtils::State(child) & nsIAccessibleStates::STATE_INVISIBLE) == 0) {

        if (aDeepestChild)
          return child->GetDeepestChildAtPoint(aX, aY, aChild);

        NS_IF_ADDREF(*aChild = child);
        return NS_OK;
      }
    }

    // The point is in this accessible but not in a child. We are allowed to
    // return |this| as the answer.
    NS_IF_ADDREF(*aChild = accessible);
    return NS_OK;
  }

  // Since DOM node of obtained accessible may be out of flow then we should
  // ensure obtained accessible is a child of this accessible.
  nsCOMPtr<nsIAccessible> parent, child(accessible);
  while (PR_TRUE) {
    child->GetParent(getter_AddRefs(parent));
    if (!parent) {
      // Reached the top of the hierarchy. These bounds were inside an
      // accessible that is not a descendant of this one.
      NS_IF_ADDREF(*aChild = fallbackAnswer);      
      return NS_OK;
    }

    if (parent == this) {
      NS_ADDREF(*aChild = (aDeepestChild ? accessible : child));
      return NS_OK;
    }
    child.swap(parent);
  }

  return NS_OK;
}

// nsIAccessible getChildAtPoint(in long x, in long y)
NS_IMETHODIMP
nsAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                              nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return GetChildAtPoint(aX, aY, PR_FALSE, aAccessible);
}

// nsIAccessible getDeepestChildAtPoint(in long x, in long y)
NS_IMETHODIMP
nsAccessible::GetDeepestChildAtPoint(PRInt32 aX, PRInt32 aY,
                                     nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return GetChildAtPoint(aX, aY, PR_TRUE, aAccessible);
}

void nsAccessible::GetBoundsRect(nsRect& aTotalBounds, nsIFrame** aBoundingFrame)
{
/*
 * This method is used to determine the bounds of a content node.
 * Because HTML wraps and links are not always rectangular, this
 * method uses the following algorithm:
 *
 * 1) Start with an empty rectangle
 * 2) Add the rect for the primary frame from for the DOM node.
 * 3) For each next frame at the same depth with the same DOM node, add that rect to total
 * 4) If that frame is an inline frame, search deeper at that point in the tree, adding all rects
 */

  // Initialization area
  *aBoundingFrame = nsnull;
  nsIFrame *firstFrame = GetBoundsFrame();
  if (!firstFrame)
    return;

  // Find common relative parent
  // This is an ancestor frame that will incompass all frames for this content node.
  // We need the relative parent so we can get absolute screen coordinates
  nsIFrame *ancestorFrame = firstFrame;

  while (ancestorFrame) {  
    *aBoundingFrame = ancestorFrame;
    // If any other frame type, we only need to deal with the primary frame
    // Otherwise, there may be more frames attached to the same content node
    if (!nsCoreUtils::IsCorrectFrameType(ancestorFrame,
                                         nsAccessibilityAtoms::inlineFrame) &&
        !nsCoreUtils::IsCorrectFrameType(ancestorFrame,
                                         nsAccessibilityAtoms::textFrame))
      break;
    ancestorFrame = ancestorFrame->GetParent();
  }

  nsIFrame *iterFrame = firstFrame;
  nsCOMPtr<nsIContent> firstContent(mContent);
  nsIContent* iterContent = firstContent;
  PRInt32 depth = 0;

  // Look only at frames below this depth, or at this depth (if we're still on the content node we started with)
  while (iterContent == firstContent || depth > 0) {
    // Coordinates will come back relative to parent frame
    nsRect currFrameBounds = iterFrame->GetRect();
    
    // Make this frame's bounds relative to common parent frame
    currFrameBounds +=
      iterFrame->GetParent()->GetOffsetToExternal(*aBoundingFrame);

    // Add this frame's bounds to total
    aTotalBounds.UnionRect(aTotalBounds, currFrameBounds);

    nsIFrame *iterNextFrame = nsnull;

    if (nsCoreUtils::IsCorrectFrameType(iterFrame,
                                        nsAccessibilityAtoms::inlineFrame)) {
      // Only do deeper bounds search if we're on an inline frame
      // Inline frames can contain larger frames inside of them
      iterNextFrame = iterFrame->GetFirstChild(nsnull);
    }

    if (iterNextFrame) 
      ++depth;  // Child was found in code above this: We are going deeper in this iteration of the loop
    else {  
      // Use next sibling if it exists, or go back up the tree to get the first next-in-flow or next-sibling 
      // within our search
      while (iterFrame) {
        iterNextFrame = iterFrame->GetNextContinuation();
        if (!iterNextFrame)
          iterNextFrame = iterFrame->GetNextSibling();
        if (iterNextFrame || --depth < 0) 
          break;
        iterFrame = iterFrame->GetParent();
      }
    }

    // Get ready for the next round of our loop
    iterFrame = iterNextFrame;
    if (iterFrame == nsnull)
      break;
    iterContent = nsnull;
    if (depth == 0)
      iterContent = iterFrame->GetContent();
  }
}


/* void getBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP
nsAccessible::GetBounds(PRInt32* aX, PRInt32* aY,
                        PRInt32* aWidth, PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Flush layout so that all the frame construction, reflow, and styles are
  // up-to-date since we rely on frames, and styles when calculating state.
  // We don't flush the display because we don't care about painting.
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  presShell->FlushPendingNotifications(Flush_Layout);

  // This routine will get the entire rectangle for all the frames in this node.
  // -------------------------------------------------------------------------
  //      Primary Frame for node
  //  Another frame, same node                <- Example
  //  Another frame, same node

  nsRect unionRectTwips;
  nsIFrame* boundingFrame = nsnull;
  GetBoundsRect(unionRectTwips, &boundingFrame);   // Unions up all primary frames for this node and all siblings after it
  NS_ENSURE_STATE(boundingFrame);

  nsPresContext* presContext = presShell->GetPresContext();
  *aX = presContext->AppUnitsToDevPixels(unionRectTwips.x);
  *aY = presContext->AppUnitsToDevPixels(unionRectTwips.y);
  *aWidth = presContext->AppUnitsToDevPixels(unionRectTwips.width);
  *aHeight = presContext->AppUnitsToDevPixels(unionRectTwips.height);

  // We have the union of the rectangle, now we need to put it in absolute screen coords
  nsIntRect orgRectPixels = boundingFrame->GetScreenRectExternal();
  *aX += orgRectPixels.x;
  *aY += orgRectPixels.y;

  return NS_OK;
}

// helpers

nsIFrame* nsAccessible::GetBoundsFrame()
{
  return GetFrame();
}

/* void removeSelection (); */
NS_IMETHODIMP nsAccessible::SetSelected(PRBool aSelect)
{
  // Add or remove selection
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 state = nsAccUtils::State(this);
  if (state & nsIAccessibleStates::STATE_SELECTABLE) {
    nsCOMPtr<nsIAccessible> multiSelect =
      nsAccUtils::GetMultiSelectableContainer(mContent);
    if (!multiSelect) {
      return aSelect ? TakeFocus() : NS_ERROR_FAILURE;
    }

    if (mRoleMapEntry) {
      if (aSelect) {
        return mContent->SetAttr(kNameSpaceID_None,
                                 nsAccessibilityAtoms::aria_selected,
                                 NS_LITERAL_STRING("true"), PR_TRUE);
      }
      return mContent->UnsetAttr(kNameSpaceID_None,
                                 nsAccessibilityAtoms::aria_selected, PR_TRUE);
    }
  }

  return NS_OK;
}

/* void takeSelection (); */
NS_IMETHODIMP nsAccessible::TakeSelection()
{
  // Select only this item
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 state = nsAccUtils::State(this);
  if (state & nsIAccessibleStates::STATE_SELECTABLE) {
    nsCOMPtr<nsIAccessible> multiSelect =
      nsAccUtils::GetMultiSelectableContainer(mContent);
    if (multiSelect) {
      nsCOMPtr<nsIAccessibleSelectable> selectable = do_QueryInterface(multiSelect);
      selectable->ClearSelection();
    }
    return SetSelected(PR_TRUE);
  }

  return NS_ERROR_FAILURE;
}

/* void takeFocus (); */
NS_IMETHODIMP
nsAccessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsIFrame *frame = GetFrame();
  NS_ENSURE_STATE(frame);

  nsIContent* focusContent = mContent;

  // If the current element can't take real DOM focus and if it has an ID and
  // an ancestor with an aria-activedescendant attribute present, then set DOM
  // focus to that ancestor and set aria-activedescendant on the ancestor to
  // the ID of the desired element.
  if (!frame->IsFocusable()) {
    nsAutoString id;
    if (nsCoreUtils::GetID(mContent, id)) {

      nsIContent* ancestorContent = mContent;
      while ((ancestorContent = ancestorContent->GetParent()) &&
             !ancestorContent->HasAttr(kNameSpaceID_None,
                                       nsAccessibilityAtoms::aria_activedescendant));

      if (ancestorContent) {
        nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
        if (presShell) {
          nsIFrame *frame = ancestorContent->GetPrimaryFrame();
          if (frame && frame->IsFocusable()) {
            focusContent = ancestorContent;
            focusContent->SetAttr(kNameSpaceID_None,
                                  nsAccessibilityAtoms::aria_activedescendant,
                                  id, PR_TRUE);
          }
        }
      }
    }
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(focusContent));
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm)
    fm->SetFocus(element, 0);

  return NS_OK;
}

nsresult
nsAccessible::GetHTMLName(nsAString& aLabel)
{
  nsIContent *labelContent = nsCoreUtils::GetHTMLLabelContent(mContent);
  if (labelContent) {
    nsAutoString label;
    nsresult rv =
      nsTextEquivUtils::AppendTextEquivFromContent(this, labelContent, &label);
    NS_ENSURE_SUCCESS(rv, rv);

    label.CompressWhitespace();
    if (!label.IsEmpty()) {
      aLabel = label;
      return NS_OK;
    }
  }

  return nsTextEquivUtils::GetNameFromSubtree(this, aLabel);
}

/**
  * 3 main cases for XUL Controls to be labeled
  *   1 - control contains label="foo"
  *   2 - control has, as a child, a label element
  *        - label has either value="foo" or children
  *   3 - non-child label contains control="controlID"
  *        - label has either value="foo" or children
  * Once a label is found, the search is discontinued, so a control
  *  that has a label child as well as having a label external to
  *  the control that uses the control="controlID" syntax will use
  *  the child label for its Name.
  */
nsresult
nsAccessible::GetXULName(nsAString& aLabel)
{
  // CASE #1 (via label attribute) -- great majority of the cases
  nsresult rv = NS_OK;

  nsAutoString label;
  nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl(do_QueryInterface(mContent));
  if (labeledEl) {
    rv = labeledEl->GetLabel(label);
  }
  else {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> itemEl(do_QueryInterface(mContent));
    if (itemEl) {
      rv = itemEl->GetLabel(label);
    }
    else {
      nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mContent));
      // Use label if this is not a select control element which 
      // uses label attribute to indicate which option is selected
      if (!select) {
        nsCOMPtr<nsIDOMXULElement> xulEl(do_QueryInterface(mContent));
        if (xulEl) {
          rv = xulEl->GetAttribute(NS_LITERAL_STRING("label"), label);
        }
      }
    }
  }

  // CASES #2 and #3 ------ label as a child or <label control="id" ... > </label>
  if (NS_FAILED(rv) || label.IsEmpty()) {
    label.Truncate();
    nsIContent *labelContent =
      nsCoreUtils::FindNeighbourPointingToNode(mContent,
                                               nsAccessibilityAtoms::control,
                                               nsAccessibilityAtoms::label);

    nsCOMPtr<nsIDOMXULLabelElement> xulLabel(do_QueryInterface(labelContent));
    // Check if label's value attribute is used
    if (xulLabel && NS_SUCCEEDED(xulLabel->GetValue(label)) && label.IsEmpty()) {
      // If no value attribute, a non-empty label must contain
      // children that define its text -- possibly using HTML
      nsTextEquivUtils::AppendTextEquivFromContent(this, labelContent, &label);
    }
  }

  // XXX If CompressWhiteSpace worked on nsAString we could avoid a copy
  label.CompressWhitespace();
  if (!label.IsEmpty()) {
    aLabel = label;
    return NS_OK;
  }

  // Can get text from title of <toolbaritem> if we're a child of a <toolbaritem>
  nsIContent *bindingParent = mContent->GetBindingParent();
  nsIContent *parent = bindingParent? bindingParent->GetParent() :
                                      mContent->GetParent();
  while (parent) {
    if (parent->Tag() == nsAccessibilityAtoms::toolbaritem &&
        parent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title, label)) {
      label.CompressWhitespace();
      aLabel = label;
      return NS_OK;
    }
    parent = parent->GetParent();
  }

  return nsTextEquivUtils::GetNameFromSubtree(this, aLabel);
}

nsresult
nsAccessible::HandleAccEvent(AccEvent* aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obsService, NS_ERROR_FAILURE);

  nsCOMPtr<nsISimpleEnumerator> observers;
  obsService->EnumerateObservers(NS_ACCESSIBLE_EVENT_TOPIC,
                                 getter_AddRefs(observers));

  NS_ENSURE_STATE(observers);

  PRBool hasObservers = PR_FALSE;
  observers->HasMoreElements(&hasObservers);
  if (hasObservers) {
    nsRefPtr<nsAccEvent> evnt(aEvent->CreateXPCOMObject());
    return obsService->NotifyObservers(evnt, NS_ACCESSIBLE_EVENT_TOPIC, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aRole = Role();
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);  // In/out param. Created if necessary.
  
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attributes = *aAttributes;
  if (!attributes) {
    // Create only if an array wasn't already passed in
    attributes = do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
    NS_ENSURE_TRUE(attributes, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aAttributes = attributes);
  }
 
  nsresult rv = GetAttributesInternal(attributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString id;
  nsAutoString oldValueUnused;
  if (nsCoreUtils::GetID(mContent, id)) {
    // Expose ID. If an <iframe id> exists override the one on the <body> of the source doc,
    // because the specific instance is what makes the ID useful for scripts
    attributes->SetStringProperty(NS_LITERAL_CSTRING("id"), id, oldValueUnused);
  }
  
  nsAutoString xmlRoles;
  if (mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::role, xmlRoles)) {
    attributes->SetStringProperty(NS_LITERAL_CSTRING("xml-roles"),  xmlRoles, oldValueUnused);          
  }

  nsCOMPtr<nsIAccessibleValue> supportsValue = do_QueryInterface(static_cast<nsIAccessible*>(this));
  if (supportsValue) {
    // We support values, so expose the string value as well, via the valuetext object attribute
    // We test for the value interface because we don't want to expose traditional get_accValue()
    // information such as URL's on links and documents, or text in an input
    nsAutoString valuetext;
    GetValue(valuetext);
    attributes->SetStringProperty(NS_LITERAL_CSTRING("valuetext"), valuetext, oldValueUnused);
  }

  // Expose checkable object attribute if the accessible has checkable state
  if (nsAccUtils::State(this) & nsIAccessibleStates::STATE_CHECKABLE)
    nsAccUtils::SetAccAttr(attributes, nsAccessibilityAtoms::checkable, NS_LITERAL_STRING("true"));

  // Group attributes (level/setsize/posinset)
  PRInt32 level = 0, posInSet = 0, setSize = 0;
  rv = GroupPosition(&level, &setSize, &posInSet);
  if (NS_SUCCEEDED(rv))
    nsAccUtils::SetAccGroupAttrs(attributes, level, setSize, posInSet);

  // Expose object attributes from ARIA attributes.
  PRUint32 numAttrs = mContent->GetAttrCount();
  for (PRUint32 count = 0; count < numAttrs; count ++) {
    const nsAttrName *attr = mContent->GetAttrNameAt(count);
    if (attr && attr->NamespaceEquals(kNameSpaceID_None)) {
      nsIAtom *attrAtom = attr->Atom();
      nsDependentAtomString attrStr(attrAtom);
      if (!StringBeginsWith(attrStr, NS_LITERAL_STRING("aria-"))) 
        continue; // Not ARIA
      PRUint8 attrFlags = nsAccUtils::GetAttributeCharacteristics(attrAtom);
      if (attrFlags & ATTR_BYPASSOBJ)
        continue; // No need to handle exposing as obj attribute here
      if ((attrFlags & ATTR_VALTOKEN) &&
          !nsAccUtils::HasDefinedARIAToken(mContent, attrAtom))
        continue; // only expose token based attributes if they are defined
      nsAutoString value;
      if (mContent->GetAttr(kNameSpaceID_None, attrAtom, value)) {
        attributes->SetStringProperty(NS_ConvertUTF16toUTF8(Substring(attrStr, 5)), value, oldValueUnused);
      }
    }
  }

  // If there is no aria-live attribute then expose default value of 'live'
  // object attribute used for ARIA role of this accessible.
  if (mRoleMapEntry) {
    nsAutoString live;
    nsAccUtils::GetAccAttr(attributes, nsAccessibilityAtoms::live, live);
    if (live.IsEmpty()) {
      if (nsAccUtils::GetLiveAttrValue(mRoleMapEntry->liveAttRule, live))
        nsAccUtils::SetAccAttr(attributes, nsAccessibilityAtoms::live, live);
    }
  }

  return NS_OK;
}

nsresult
nsAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  // Attributes set by this method will not be used to override attributes on a sub-document accessible
  // when there is a <frame>/<iframe> element that spawned the sub-document
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mContent));

  nsAutoString tagName;
  element->GetTagName(tagName);
  if (!tagName.IsEmpty()) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("tag"), tagName,
                                   oldValueUnused);
  }

  nsEventShell::GetEventAttributes(GetNode(), aAttributes);
 
  // Expose class because it may have useful microformat information
  // Let the class from an iframe's document be exposed, don't override from <iframe class>
  nsAutoString _class;
  if (mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::_class, _class))
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::_class, _class);

  // Get container-foo computed live region properties based on the closest container with
  // the live region attribute. 
  // Inner nodes override outer nodes within the same document --
  //   The inner nodes can be used to override live region behavior on more general outer nodes
  // However, nodes in outer documents override nodes in inner documents:
  //   Outer doc author may want to override properties on a widget they used in an iframe
  nsIContent *startContent = mContent;
  while (PR_TRUE) {
    NS_ENSURE_STATE(startContent);
    nsIDocument *doc = startContent->GetDocument();
    nsIContent* rootContent = nsCoreUtils::GetRoleContent(doc);
    NS_ENSURE_STATE(rootContent);
    nsAccUtils::SetLiveContainerAttributes(aAttributes, startContent,
                                           rootContent);

    // Allow ARIA live region markup from outer documents to override
    nsCOMPtr<nsISupports> container = doc->GetContainer(); 
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
      do_QueryInterface(container);
    if (!docShellTreeItem)
      break;

    nsCOMPtr<nsIDocShellTreeItem> sameTypeParent;
    docShellTreeItem->GetSameTypeParent(getter_AddRefs(sameTypeParent));
    if (!sameTypeParent || sameTypeParent == docShellTreeItem)
      break;

    nsIDocument *parentDoc = doc->GetParentDocument();
    if (!parentDoc)
      break;

    startContent = parentDoc->FindContentForSubDocument(doc);      
  }

  // Expose 'display' attribute.
  nsAutoString value;
  nsresult rv = GetComputedStyleValue(EmptyString(),
                                      NS_LITERAL_STRING("display"),
                                      value);
  if (NS_SUCCEEDED(rv))
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::display,
                           value);

  // Expose 'text-align' attribute.
  rv = GetComputedStyleValue(EmptyString(), NS_LITERAL_STRING("text-align"),
                             value);
  if (NS_SUCCEEDED(rv))
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::textAlign,
                           value);

  // Expose 'text-indent' attribute.
  rv = GetComputedStyleValue(EmptyString(), NS_LITERAL_STRING("text-indent"),
                             value);
  if (NS_SUCCEEDED(rv))
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::textIndent,
                           value);

  // Expose draggable object attribute?
  nsCOMPtr<nsIDOMNSHTMLElement> htmlElement = do_QueryInterface(mContent);
  if (htmlElement) {
    PRBool draggable = PR_FALSE;
    htmlElement->GetDraggable(&draggable);
    if (draggable) {
      nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::draggable,
                             NS_LITERAL_STRING("true"));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GroupPosition(PRInt32 *aGroupLevel,
                            PRInt32 *aSimilarItemsInGroup,
                            PRInt32 *aPositionInGroup)
{
  NS_ENSURE_ARG_POINTER(aGroupLevel);
  *aGroupLevel = 0;

  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  *aSimilarItemsInGroup = 0;

  NS_ENSURE_ARG_POINTER(aPositionInGroup);
  *aPositionInGroup = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Get group position from ARIA attributes.
  nsCoreUtils::GetUIntAttr(mContent, nsAccessibilityAtoms::aria_level,
                           aGroupLevel);
  nsCoreUtils::GetUIntAttr(mContent, nsAccessibilityAtoms::aria_posinset,
                           aPositionInGroup);
  nsCoreUtils::GetUIntAttr(mContent, nsAccessibilityAtoms::aria_setsize,
                           aSimilarItemsInGroup);

  // If ARIA is missed and the accessible is visible then calculate group
  // position from hierarchy.
  if (nsAccUtils::State(this) & nsIAccessibleStates::STATE_INVISIBLE)
    return NS_OK;

  // Calculate group level if ARIA is missed.
  if (*aGroupLevel == 0) {
    PRInt32 level = GetLevelInternal();
    if (level != 0)
      *aGroupLevel = level;
  }

  // Calculate position in group and group size if ARIA is missed.
  if (*aSimilarItemsInGroup == 0 || *aPositionInGroup == 0) {
    PRInt32 posInSet = 0, setSize = 0;
    GetPositionAndSizeInternal(&posInSet, &setSize);
    if (posInSet != 0 && setSize != 0) {
      if (*aPositionInGroup == 0)
        *aPositionInGroup = posInSet;

      if (*aSimilarItemsInGroup == 0)
        *aSimilarItemsInGroup = setSize;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  if (!IsDefunct()) {
    // Flush layout so that all the frame construction, reflow, and styles are
    // up-to-date since we rely on frames, and styles when calculating state.
    // We don't flush the display because we don't care about painting.
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    presShell->FlushPendingNotifications(Flush_Layout);
  }

  nsresult rv = GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // Apply ARIA states to be sure accessible states will be overriden.
  GetARIAState(aState, aExtraState);

  if (mRoleMapEntry && mRoleMapEntry->role == nsIAccessibleRole::ROLE_PAGETAB) {
    if (*aState & nsIAccessibleStates::STATE_FOCUSED) {
      *aState |= nsIAccessibleStates::STATE_SELECTED;
    } else {
      // Expose 'selected' state on ARIA tab if the focus is on internal element
      // of related tabpanel.
      nsCOMPtr<nsIAccessible> tabPanel = nsRelUtils::
        GetRelatedAccessible(this, nsIAccessibleRelation::RELATION_LABEL_FOR);

      if (nsAccUtils::Role(tabPanel) == nsIAccessibleRole::ROLE_PROPERTYPAGE) {
        nsRefPtr<nsAccessible> tabPanelAcc(do_QueryObject(tabPanel));
        nsINode *tabPanelNode = tabPanelAcc->GetNode();
        if (nsCoreUtils::IsAncestorOf(tabPanelNode, gLastFocusedNode))
          *aState |= nsIAccessibleStates::STATE_SELECTED;
      }
    }
  }

  const PRUint32 kExpandCollapseStates =
    nsIAccessibleStates::STATE_COLLAPSED | nsIAccessibleStates::STATE_EXPANDED;
  if ((*aState & kExpandCollapseStates) == kExpandCollapseStates) {
    // Cannot be both expanded and collapsed -- this happens in ARIA expanded
    // combobox because of limitation of nsARIAMap.
    // XXX: Perhaps we will be able to make this less hacky if we support
    // extended states in nsARIAMap, e.g. derive COLLAPSED from
    // EXPANDABLE && !EXPANDED.
    *aState &= ~nsIAccessibleStates::STATE_COLLAPSED;
  }

  // Set additional states which presence depends on another states.
  if (!aExtraState)
    return NS_OK;

  if (!(*aState & nsIAccessibleStates::STATE_UNAVAILABLE)) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_ENABLED |
                    nsIAccessibleStates::EXT_STATE_SENSITIVE;
  }

  if ((*aState & nsIAccessibleStates::STATE_COLLAPSED) ||
      (*aState & nsIAccessibleStates::STATE_EXPANDED))
    *aExtraState |= nsIAccessibleStates::EXT_STATE_EXPANDABLE;

  if (mRoleMapEntry) {
    // If an object has an ancestor with the activedescendant property
    // pointing at it, we mark it as ACTIVE even if it's not currently focused.
    // This allows screen reader virtual buffer modes to know which descendant
    // is the current one that would get focus if the user navigates to the container widget.
    nsAutoString id;
    if (nsCoreUtils::GetID(mContent, id)) {
      nsIContent *ancestorContent = mContent;
      nsAutoString activeID;
      while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
        if (ancestorContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_activedescendant, activeID)) {
          if (id == activeID) {
            *aExtraState |= nsIAccessibleStates::EXT_STATE_ACTIVE;
          }
          break;
        }
      }
    }
  }

  // For some reasons DOM node may have not a frame. We tract such accessibles
  // as invisible.
  nsIFrame *frame = GetFrame();
  if (!frame)
    return NS_OK;

  const nsStyleDisplay* display = frame->GetStyleDisplay();
  if (display && display->mOpacity == 1.0f &&
      !(*aState & nsIAccessibleStates::STATE_INVISIBLE)) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_OPAQUE;
  }

  const nsStyleXUL *xulStyle = frame->GetStyleXUL();
  if (xulStyle) {
    // In XUL all boxes are either vertical or horizontal
    if (xulStyle->mBoxOrient == NS_STYLE_BOX_ORIENT_VERTICAL) {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_VERTICAL;
    }
    else {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_HORIZONTAL;
    }
  }
  
  // If we are editable, force readonly bit off
  if (*aExtraState & nsIAccessibleStates::EXT_STATE_EDITABLE)
    *aState &= ~nsIAccessibleStates::STATE_READONLY;
 
  return NS_OK;
}

nsresult
nsAccessible::GetARIAState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Test for universal states first
  PRUint32 index = 0;
  while (nsStateMapEntry::MapToStates(mContent, aState, aExtraState,
                                      nsARIAMap::gWAIUnivStateMap[index])) {
    ++ index;
  }

  if (mRoleMapEntry) {

    // We only force the readonly bit off if we have a real mapping for the aria
    // role. This preserves the ability for screen readers to use readonly
    // (primarily on the document) as the hint for creating a virtual buffer.
    if (mRoleMapEntry->role != nsIAccessibleRole::ROLE_NOTHING)
      *aState &= ~nsIAccessibleStates::STATE_READONLY;

    if (mContent->HasAttr(kNameSpaceID_None, mContent->GetIDAttributeName())) {
      // If has a role & ID and aria-activedescendant on the container, assume focusable
      nsIContent *ancestorContent = mContent;
      while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
        if (ancestorContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_activedescendant)) {
            // ancestor has activedescendant property, this content could be active
          *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
          break;
        }
      }
    }
  }

  if (*aState & nsIAccessibleStates::STATE_FOCUSABLE) {
    // Special case: aria-disabled propagates from ancestors down to any focusable descendant
    nsIContent *ancestorContent = mContent;
    while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
      if (ancestorContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::aria_disabled,
                                       nsAccessibilityAtoms::_true, eCaseMatters)) {
          // ancestor has aria-disabled property, this is disabled
        *aState |= nsIAccessibleStates::STATE_UNAVAILABLE;
        break;
      }
    }    
  }

  if (!mRoleMapEntry)
    return NS_OK;

  // Note: the readonly bitflag will be overridden later if content is editable
  *aState |= mRoleMapEntry->state;
  if (nsStateMapEntry::MapToStates(mContent, aState, aExtraState,
                                   mRoleMapEntry->attributeMap1) &&
      nsStateMapEntry::MapToStates(mContent, aState, aExtraState,
                                   mRoleMapEntry->attributeMap2)) {
    nsStateMapEntry::MapToStates(mContent, aState, aExtraState,
                                 mRoleMapEntry->attributeMap3);
  }

  return NS_OK;
}

// Not implemented by this class

/* DOMString getValue (); */
NS_IMETHODIMP
nsAccessible::GetValue(nsAString& aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (mRoleMapEntry) {
    if (mRoleMapEntry->valueRule == eNoValue) {
      return NS_OK;
    }

    // aria-valuenow is a number, and aria-valuetext is the optional text equivalent
    // For the string value, we will try the optional text equivalent first
    if (!mContent->GetAttr(kNameSpaceID_None,
                           nsAccessibilityAtoms::aria_valuetext, aValue)) {
      mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_valuenow,
                        aValue);
    }
  }

  if (!aValue.IsEmpty())
    return NS_OK;

  // Check if it's a simple xlink.
  if (nsCoreUtils::IsXLink(mContent)) {
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    if (presShell) {
      nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
      return presShell->GetLinkLocation(DOMNode, aValue);
    }
  }

  return NS_OK;
}

// nsIAccessibleValue
NS_IMETHODIMP
nsAccessible::GetMaximumValue(double *aMaximumValue)
{
  return GetAttrValue(nsAccessibilityAtoms::aria_valuemax, aMaximumValue);
}

NS_IMETHODIMP
nsAccessible::GetMinimumValue(double *aMinimumValue)
{
  return GetAttrValue(nsAccessibilityAtoms::aria_valuemin, aMinimumValue);
}

NS_IMETHODIMP
nsAccessible::GetMinimumIncrement(double *aMinIncrement)
{
  NS_ENSURE_ARG_POINTER(aMinIncrement);
  *aMinIncrement = 0;

  // No mimimum increment in dynamic content spec right now
  return NS_OK_NO_ARIA_VALUE;
}

NS_IMETHODIMP
nsAccessible::GetCurrentValue(double *aValue)
{
  return GetAttrValue(nsAccessibilityAtoms::aria_valuenow, aValue);
}

NS_IMETHODIMP
nsAccessible::SetCurrentValue(double aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return NS_OK_NO_ARIA_VALUE;

  const PRUint32 kValueCannotChange = nsIAccessibleStates::STATE_READONLY |
                                      nsIAccessibleStates::STATE_UNAVAILABLE;

  if (nsAccUtils::State(this) & kValueCannotChange)
    return NS_ERROR_FAILURE;

  double minValue = 0;
  if (NS_SUCCEEDED(GetMinimumValue(&minValue)) && aValue < minValue)
    return NS_ERROR_INVALID_ARG;

  double maxValue = 0;
  if (NS_SUCCEEDED(GetMaximumValue(&maxValue)) && aValue > maxValue)
    return NS_ERROR_INVALID_ARG;

  nsAutoString newValue;
  newValue.AppendFloat(aValue);
  return mContent->SetAttr(kNameSpaceID_None,
                           nsAccessibilityAtoms::aria_valuenow, newValue,
                           PR_TRUE);
}

/* void setName (in DOMString name); */
NS_IMETHODIMP nsAccessible::SetName(const nsAString& name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAccessible::GetDefaultKeyBinding(nsAString& aKeyBinding)
{
  aKeyBinding.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetKeyBindings(PRUint8 aActionIndex,
                             nsIDOMDOMStringList **aKeyBindings)
{
  // Currently we support only unique key binding on element for default action.
  NS_ENSURE_TRUE(aActionIndex == 0, NS_ERROR_INVALID_ARG);

  nsAccessibleDOMStringList *keyBindings = new nsAccessibleDOMStringList();
  NS_ENSURE_TRUE(keyBindings, NS_ERROR_OUT_OF_MEMORY);

  nsAutoString defaultKey;
  nsresult rv = GetDefaultKeyBinding(defaultKey);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!defaultKey.IsEmpty())
    keyBindings->Add(defaultKey);

  NS_ADDREF(*aKeyBindings = keyBindings);
  return NS_OK;
}

PRUint32
nsAccessible::Role()
{
  // No ARIA role or it doesn't suppress role from native markup.
  if (!mRoleMapEntry || mRoleMapEntry->roleRule != kUseMapRole)
    return NativeRole();

  // XXX: these unfortunate exceptions don't fit into the ARIA table. This is
  // where the accessible role depends on both the role and ARIA state.
  if (mRoleMapEntry->role == nsIAccessibleRole::ROLE_PUSHBUTTON) {
    if (nsAccUtils::HasDefinedARIAToken(mContent,
                                        nsAccessibilityAtoms::aria_pressed)) {
      // For simplicity, any existing pressed attribute except "" or "undefined"
      // indicates a toggle.
      return nsIAccessibleRole::ROLE_TOGGLE_BUTTON;
    }

    if (mContent->AttrValueIs(kNameSpaceID_None,
                              nsAccessibilityAtoms::aria_haspopup,
                              nsAccessibilityAtoms::_true,
                              eCaseMatters)) {
      // For button with aria-haspopup="true".
      return nsIAccessibleRole::ROLE_BUTTONMENU;
    }

  } else if (mRoleMapEntry->role == nsIAccessibleRole::ROLE_LISTBOX) {
    // A listbox inside of a combobox needs a special role because of ATK
    // mapping to menu.
    if (mParent && mParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX) {
      return nsIAccessibleRole::ROLE_COMBOBOX_LIST;

      nsCOMPtr<nsIAccessible> possibleCombo =
        nsRelUtils::GetRelatedAccessible(this,
                                         nsIAccessibleRelation::RELATION_NODE_CHILD_OF);
      if (nsAccUtils::Role(possibleCombo) == nsIAccessibleRole::ROLE_COMBOBOX)
        return nsIAccessibleRole::ROLE_COMBOBOX_LIST;
    }

  } else if (mRoleMapEntry->role == nsIAccessibleRole::ROLE_OPTION) {
    if (mParent && mParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX_LIST)
      return nsIAccessibleRole::ROLE_COMBOBOX_OPTION;
  }

  return mRoleMapEntry->role;
}

PRUint32
nsAccessible::NativeRole()
{
  return nsCoreUtils::IsXLink(mContent) ?
    nsIAccessibleRole::ROLE_LINK : nsIAccessibleRole::ROLE_NOTHING;
}

// readonly attribute PRUint8 numActions
NS_IMETHODIMP
nsAccessible::GetNumActions(PRUint8 *aNumActions)
{
  NS_ENSURE_ARG_POINTER(aNumActions);
  *aNumActions = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 actionRule = GetActionRule(nsAccUtils::State(this));
  if (actionRule == eNoAction)
    return NS_OK;

  *aNumActions = 1;
  return NS_OK;
}

/* DOMString getAccActionName (in PRUint8 index); */
NS_IMETHODIMP
nsAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 states = nsAccUtils::State(this);
  PRUint32 actionRule = GetActionRule(states);

 switch (actionRule) {
   case eActivateAction:
     aName.AssignLiteral("activate");
     return NS_OK;

   case eClickAction:
     aName.AssignLiteral("click");
     return NS_OK;

   case eCheckUncheckAction:
     if (states & nsIAccessibleStates::STATE_CHECKED)
       aName.AssignLiteral("uncheck");
     else if (states & nsIAccessibleStates::STATE_MIXED)
       aName.AssignLiteral("cycle");
     else
       aName.AssignLiteral("check");
     return NS_OK;

   case eJumpAction:
     aName.AssignLiteral("jump");
     return NS_OK;

   case eOpenCloseAction:
     if (states & nsIAccessibleStates::STATE_COLLAPSED)
       aName.AssignLiteral("open");
     else
       aName.AssignLiteral("close");
     return NS_OK;

   case eSelectAction:
     aName.AssignLiteral("select");
     return NS_OK;

   case eSwitchAction:
     aName.AssignLiteral("switch");
     return NS_OK;
     
   case eSortAction:
     aName.AssignLiteral("sort");
     return NS_OK;
   
   case eExpandAction:
     if (states & nsIAccessibleStates::STATE_COLLAPSED)
       aName.AssignLiteral("expand");
     else
       aName.AssignLiteral("collapse");
     return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

// AString getActionDescription(in PRUint8 index)
NS_IMETHODIMP
nsAccessible::GetActionDescription(PRUint8 aIndex, nsAString& aDescription)
{
  // default to localized action name.
  nsAutoString name;
  nsresult rv = GetActionName(aIndex, name);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetTranslatedString(name, aDescription);
}

// void doAction(in PRUint8 index)
NS_IMETHODIMP
nsAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (GetActionRule(nsAccUtils::State(this)) != eNoAction) {
    DoCommand();
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/* DOMString getHelp (); */
NS_IMETHODIMP nsAccessible::GetHelp(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleToRight(); */
NS_IMETHODIMP nsAccessible::GetAccessibleToRight(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleToLeft(); */
NS_IMETHODIMP nsAccessible::GetAccessibleToLeft(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleAbove(); */
NS_IMETHODIMP nsAccessible::GetAccessibleAbove(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleBelow(); */
NS_IMETHODIMP nsAccessible::GetAccessibleBelow(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIDOMNode* nsAccessible::GetAtomicRegion()
{
  nsIContent *loopContent = mContent;
  nsAutoString atomic;
  while (loopContent && !loopContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_atomic, atomic)) {
    loopContent = loopContent->GetParent();
  }

  nsCOMPtr<nsIDOMNode> atomicRegion;
  if (atomic.EqualsLiteral("true")) {
    atomicRegion = do_QueryInterface(loopContent);
  }
  return atomicRegion;
}

// nsIAccessible getRelationByType()
NS_IMETHODIMP
nsAccessible::GetRelationByType(PRUint32 aRelationType,
                                nsIAccessibleRelation **aRelation)
{
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Relationships are defined on the same content node that the role would be
  // defined on.
  nsresult rv;
  switch (aRelationType)
  {
  case nsIAccessibleRelation::RELATION_LABEL_FOR:
    {
      if (mContent->Tag() == nsAccessibilityAtoms::label) {
        nsIAtom *IDAttr = mContent->IsHTML() ?
          nsAccessibilityAtoms::_for : nsAccessibilityAtoms::control;
        rv = nsRelUtils::
          AddTargetFromIDRefAttr(aRelationType, aRelation, mContent, IDAttr);
        NS_ENSURE_SUCCESS(rv, rv);

        if (rv != NS_OK_NO_RELATION_TARGET)
          return NS_OK; // XXX bug 381599, avoid performance problems
      }

      return nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::aria_labelledby);
    }

  case nsIAccessibleRelation::RELATION_LABELLED_BY:
    {
      rv = nsRelUtils::
        AddTargetFromIDRefsAttr(aRelationType, aRelation, mContent,
                                nsAccessibilityAtoms::aria_labelledby);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_RELATION_TARGET)
        return NS_OK; // XXX bug 381599, avoid performance problems

      return nsRelUtils::
        AddTargetFromContent(aRelationType, aRelation,
                             nsCoreUtils::GetLabelContent(mContent));
    }

  case nsIAccessibleRelation::RELATION_DESCRIBED_BY:
    {
      rv = nsRelUtils::
        AddTargetFromIDRefsAttr(aRelationType, aRelation, mContent,
                                nsAccessibilityAtoms::aria_describedby);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_RELATION_TARGET)
        return NS_OK; // XXX bug 381599, avoid performance problems

      return nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::control,
                               nsAccessibilityAtoms::description);
    }

  case nsIAccessibleRelation::RELATION_DESCRIPTION_FOR:
    {
      rv = nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::aria_describedby);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_RELATION_TARGET)
        return NS_OK; // XXX bug 381599, avoid performance problems

      if (mContent->Tag() == nsAccessibilityAtoms::description &&
          mContent->IsXUL()) {
        // This affectively adds an optional control attribute to xul:description,
        // which only affects accessibility, by allowing the description to be
        // tied to a control.
        return nsRelUtils::
          AddTargetFromIDRefAttr(aRelationType, aRelation, mContent,
                                 nsAccessibilityAtoms::control);
      }

      return NS_OK;
    }

  case nsIAccessibleRelation::RELATION_NODE_CHILD_OF:
    {
      rv = nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::aria_owns);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_RELATION_TARGET)
        return NS_OK; // XXX bug 381599, avoid performance problems

      // This is an ARIA tree or treegrid that doesn't use owns, so we need to
      // get the parent the hard way.
      if (mRoleMapEntry &&
          (mRoleMapEntry->role == nsIAccessibleRole::ROLE_OUTLINEITEM ||
           mRoleMapEntry->role == nsIAccessibleRole::ROLE_ROW)) {

        AccGroupInfo* groupInfo = GetGroupInfo();
        if (!groupInfo)
          return NS_OK_NO_RELATION_TARGET;

        return nsRelUtils::AddTarget(aRelationType, aRelation,
                                     groupInfo->GetConceptualParent());
      }

      // If accessible is in its own Window, or is the root of a document,
      // then we should provide NODE_CHILD_OF relation so that MSAA clients
      // can easily get to true parent instead of getting to oleacc's
      // ROLE_WINDOW accessible which will prevent us from going up further
      // (because it is system generated and has no idea about the hierarchy
      // above it).
      nsIFrame *frame = GetFrame();
      if (frame) {
        nsIView *view = frame->GetViewExternal();
        if (view) {
          nsIScrollableFrame *scrollFrame = do_QueryFrame(frame);
          if (scrollFrame || view->GetWidget() || !frame->GetParent()) {
            return nsRelUtils::AddTarget(aRelationType, aRelation, GetParent());
          }
        }
      }

      return NS_OK;
    }

  case nsIAccessibleRelation::RELATION_CONTROLLED_BY:
    {
      return nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::aria_controls);
    }

  case nsIAccessibleRelation::RELATION_CONTROLLER_FOR:
    {
      nsresult rv = nsRelUtils::
        AddTargetFromIDRefsAttr(aRelationType, aRelation, mContent,
                                nsAccessibilityAtoms::aria_controls);
      NS_ENSURE_SUCCESS(rv,rv);

      if (rv != NS_OK_NO_RELATION_TARGET)
        return NS_OK; // XXX bug 381599, avoid performance problems      

      return nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::_for,
                               nsAccessibilityAtoms::output);
    }

  case nsIAccessibleRelation::RELATION_FLOWS_TO:
    {
      return nsRelUtils::
        AddTargetFromIDRefsAttr(aRelationType, aRelation, mContent,
                                nsAccessibilityAtoms::aria_flowto);
    }

  case nsIAccessibleRelation::RELATION_FLOWS_FROM:
    {
      return nsRelUtils::
        AddTargetFromNeighbour(aRelationType, aRelation, mContent,
                               nsAccessibilityAtoms::aria_flowto);
    }

  case nsIAccessibleRelation::RELATION_DEFAULT_BUTTON:
    {
      if (mContent->IsHTML()) {
        // HTML form controls implements nsIFormControl interface.
        nsCOMPtr<nsIFormControl> control(do_QueryInterface(mContent));
        if (control) {
          nsCOMPtr<nsIForm> form(do_QueryInterface(control->GetFormElement()));
          if (form) {
            nsCOMPtr<nsIContent> formContent =
              do_QueryInterface(form->GetDefaultSubmitElement());
            return nsRelUtils::AddTargetFromContent(aRelationType, aRelation,
                                                    formContent);
          }
        }
      }
      else {
        // In XUL, use first <button default="true" .../> in the document
        nsCOMPtr<nsIDOMXULDocument> xulDoc =
          do_QueryInterface(mContent->GetOwnerDoc());
        nsCOMPtr<nsIDOMXULButtonElement> buttonEl;
        if (xulDoc) {
          nsCOMPtr<nsIDOMNodeList> possibleDefaultButtons;
          xulDoc->GetElementsByAttribute(NS_LITERAL_STRING("default"),
                                         NS_LITERAL_STRING("true"),
                                         getter_AddRefs(possibleDefaultButtons));
          if (possibleDefaultButtons) {
            PRUint32 length;
            possibleDefaultButtons->GetLength(&length);
            nsCOMPtr<nsIDOMNode> possibleButton;
            // Check for button in list of default="true" elements
            for (PRUint32 count = 0; count < length && !buttonEl; count ++) {
              possibleDefaultButtons->Item(count, getter_AddRefs(possibleButton));
              buttonEl = do_QueryInterface(possibleButton);
            }
          }
          if (!buttonEl) { // Check for anonymous accept button in <dialog>
            nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(xulDoc));
            if (xblDoc) {
              nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(xulDoc);
              NS_ASSERTION(domDoc, "No DOM document");
              nsCOMPtr<nsIDOMElement> rootEl;
              domDoc->GetDocumentElement(getter_AddRefs(rootEl));
              if (rootEl) {
                nsCOMPtr<nsIDOMElement> possibleButtonEl;
                xblDoc->GetAnonymousElementByAttribute(rootEl,
                                                      NS_LITERAL_STRING("default"),
                                                      NS_LITERAL_STRING("true"),
                                                      getter_AddRefs(possibleButtonEl));
                buttonEl = do_QueryInterface(possibleButtonEl);
              }
            }
          }
          nsCOMPtr<nsIContent> relatedContent(do_QueryInterface(buttonEl));
          return nsRelUtils::AddTargetFromContent(aRelationType, aRelation,
                                                  relatedContent);
        }
      }
      return NS_OK;
    }

  case nsIAccessibleRelation::RELATION_MEMBER_OF:
    {
      nsCOMPtr<nsIContent> regionContent = do_QueryInterface(GetAtomicRegion());
      return nsRelUtils::
        AddTargetFromContent(aRelationType, aRelation, regionContent);
    }

  case nsIAccessibleRelation::RELATION_SUBWINDOW_OF:
  case nsIAccessibleRelation::RELATION_EMBEDS:
  case nsIAccessibleRelation::RELATION_EMBEDDED_BY:
  case nsIAccessibleRelation::RELATION_POPUP_FOR:
  case nsIAccessibleRelation::RELATION_PARENT_WINDOW_OF:
    {
      return NS_OK_NO_RELATION_TARGET;
    }

  default:
    return NS_ERROR_INVALID_ARG;
  }
}

NS_IMETHODIMP
nsAccessible::GetRelationsCount(PRUint32 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  nsCOMPtr<nsIArray> relations;
  nsresult rv = GetRelations(getter_AddRefs(relations));
  NS_ENSURE_SUCCESS(rv, rv);

  return relations->GetLength(aCount);
}

NS_IMETHODIMP
nsAccessible::GetRelation(PRUint32 aIndex, nsIAccessibleRelation **aRelation)
{
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nsnull;

  nsCOMPtr<nsIArray> relations;
  nsresult rv = GetRelations(getter_AddRefs(relations));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibleRelation> relation;
  rv = relations->QueryElementAt(aIndex, NS_GET_IID(nsIAccessibleRelation),
                                 getter_AddRefs(relation));

  // nsIArray::QueryElementAt() returns NS_ERROR_ILLEGAL_VALUE on invalid index.
  if (rv == NS_ERROR_ILLEGAL_VALUE)
    return NS_ERROR_INVALID_ARG;

  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aRelation = relation);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetRelations(nsIArray **aRelations)
{
  NS_ENSURE_ARG_POINTER(aRelations);

  nsCOMPtr<nsIMutableArray> relations = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(relations, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 relType = nsIAccessibleRelation::RELATION_FIRST;
       relType < nsIAccessibleRelation::RELATION_LAST;
       ++relType) {

    nsCOMPtr<nsIAccessibleRelation> relation;
    nsresult rv = GetRelationByType(relType, getter_AddRefs(relation));

    if (NS_SUCCEEDED(rv) && relation)
      relations->AppendElement(relation, PR_FALSE);
  }

  NS_ADDREF(*aRelations = relations);
  return NS_OK;
}

/* void extendSelection (); */
NS_IMETHODIMP nsAccessible::ExtendSelection()
{
  // XXX Should be implemented, but not high priority
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void getNativeInterface(out voidPtr aOutAccessible); */
NS_IMETHODIMP nsAccessible::GetNativeInterface(void **aOutAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsAccessible::DoCommand(nsIContent *aContent, PRUint32 aActionIndex)
{
  nsIContent* content = aContent ? aContent : mContent.get();
  NS_DISPATCH_RUNNABLEMETHOD_ARG2(DispatchClickEvent, this, content,
                                  aActionIndex);
}

void
nsAccessible::DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex)
{
  if (IsDefunct())
    return;

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();

  // Scroll into view.
  presShell->ScrollContentIntoView(aContent, NS_PRESSHELL_SCROLL_ANYWHERE,
                                   NS_PRESSHELL_SCROLL_ANYWHERE);

  // Fire mouse down and mouse up events.
  PRBool res = nsCoreUtils::DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, presShell,
                                               aContent);
  if (!res)
    return;

  nsCoreUtils::DispatchMouseEvent(NS_MOUSE_BUTTON_UP, presShell, aContent);
}

// nsIAccessibleSelectable
NS_IMETHODIMP nsAccessible::GetSelectedChildren(nsIArray **aSelectedAccessibles)
{
  NS_ENSURE_ARG_POINTER(aSelectedAccessibles);
  *aSelectedAccessibles = nsnull;

  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIArray> items = SelectedItems();
  if (items) {
    PRUint32 length = 0;
    items->GetLength(&length);
    if (length)
      items.swap(*aSelectedAccessibles);
  }

  return NS_OK;
}

// return the nth selected descendant nsIAccessible object
NS_IMETHODIMP nsAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = nsnull;

  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }

  *aSelected = GetSelectedItem(aIndex);
  if (*aSelected) {
    NS_ADDREF(*aSelected);
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  *aSelectionCount = SelectedItemCount();
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::AddChildToSelection(PRInt32 aIndex)
{
  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  return aIndex >= 0 && AddItemToSelection(aIndex) ?
    NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsAccessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  return aIndex >=0 && RemoveItemFromSelection(aIndex) ?
    NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsAccessible::IsChildSelected(PRInt32 aIndex, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  NS_ENSURE_TRUE(aIndex >= 0, NS_ERROR_FAILURE);

  *aIsSelected = IsItemSelected(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::ClearSelection()
{
  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  UnselectAll();
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::SelectAllSelection(PRBool* aIsMultiSelect)
{
  NS_ENSURE_ARG_POINTER(aIsMultiSelect);
  *aIsMultiSelect = PR_FALSE;

  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  *aIsMultiSelect = SelectAll();
  return NS_OK;
}

// nsIAccessibleHyperLink
// Because of new-atk design, any embedded object in text can implement
// nsIAccessibleHyperLink, which helps determine where it is located
// within containing text

// readonly attribute long nsIAccessibleHyperLink::anchorCount
NS_IMETHODIMP
nsAccessible::GetAnchorCount(PRInt32 *aAnchorCount)
{
  NS_ENSURE_ARG_POINTER(aAnchorCount);
  *aAnchorCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aAnchorCount = AnchorCount();
  return NS_OK;
}

// readonly attribute long nsIAccessibleHyperLink::startIndex
NS_IMETHODIMP
nsAccessible::GetStartIndex(PRInt32 *aStartIndex)
{
  NS_ENSURE_ARG_POINTER(aStartIndex);
  *aStartIndex = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aStartIndex = StartOffset();
  return NS_OK;
}

// readonly attribute long nsIAccessibleHyperLink::endIndex
NS_IMETHODIMP
nsAccessible::GetEndIndex(PRInt32 *aEndIndex)
{
  NS_ENSURE_ARG_POINTER(aEndIndex);
  *aEndIndex = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aEndIndex = EndOffset();
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetURI(PRInt32 aIndex, nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex < 0 || aIndex >= static_cast<PRInt32>(AnchorCount()))
    return NS_ERROR_INVALID_ARG;

  *aURI = GetAnchorURI(aIndex).get();
  return NS_OK;
}


NS_IMETHODIMP
nsAccessible::GetAnchor(PRInt32 aIndex, nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex < 0 || aIndex >= static_cast<PRInt32>(AnchorCount()))
    return NS_ERROR_INVALID_ARG;

  NS_IF_ADDREF(*aAccessible = GetAnchor(aIndex));
  return NS_OK;
}

// readonly attribute boolean nsIAccessibleHyperLink::valid
NS_IMETHODIMP
nsAccessible::GetValid(PRBool *aValid)
{
  NS_ENSURE_ARG_POINTER(aValid);
  *aValid = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aValid = IsValid();
  return NS_OK;
}

// readonly attribute boolean nsIAccessibleHyperLink::selected
NS_IMETHODIMP
nsAccessible::GetSelected(PRBool *aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aSelected = IsSelected();
  return NS_OK;

}

nsresult
nsAccessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset, PRUint32 aLength)
{
  // Return text representation of non-text accessible within hypertext
  // accessible. Text accessible overrides this method to return enclosed text.
  if (aStartOffset != 0)
    return NS_OK;

  nsIFrame *frame = GetFrame();
  NS_ENSURE_STATE(frame);

  if (frame->GetType() == nsAccessibilityAtoms::brFrame) {
    aText += kForcedNewLineChar;
  } else if (nsAccUtils::MustPrune(this)) {
    // Expose imaginary embedded object character if the accessible hans't
    // children.
    aText += kImaginaryEmbeddedObjectChar;
  } else {
    aText += kEmbeddedObjectChar;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public methods

PRBool
nsAccessible::Init()
{
  if (!nsAccessNodeWrap::Init())
    return PR_FALSE;

  nsDocAccessible* document =
    GetAccService()->GetDocAccessible(mContent->GetOwnerDoc());
  NS_ASSERTION(document, "Cannot cache new nsAccessible!");

  return document ? document->CacheAccessible(this) : PR_FALSE;
}

void
nsAccessible::Shutdown()
{
  // Invalidate the child count and pointers to other accessibles, also make
  // sure none of its children point to this parent
  InvalidateChildren();
  if (mParent)
    mParent->RemoveChild(this);

  nsAccessNodeWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public methods

nsresult
nsAccessible::GetARIAName(nsAString& aName)
{
  nsAutoString label;

  // aria-labelledby now takes precedence over aria-label
  nsresult rv = nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsAccessibilityAtoms::aria_labelledby, label);
  if (NS_SUCCEEDED(rv)) {
    label.CompressWhitespace();
    aName = label;
  }

  if (label.IsEmpty() &&
      mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_label,
                        label)) {
    label.CompressWhitespace();
    aName = label;
  }
  
  return NS_OK;
}

nsresult
nsAccessible::GetNameInternal(nsAString& aName)
{
  if (mContent->IsHTML())
    return GetHTMLName(aName);

  if (mContent->IsXUL())
    return GetXULName(aName);

  return NS_OK;
}

// nsAccessible protected
void
nsAccessible::BindToParent(nsAccessible* aParent, PRUint32 aIndexInParent)
{
  NS_PRECONDITION(aParent, "This method isn't used to set null parent!");

  if (mParent) {
    if (mParent != aParent) {
      NS_ERROR("Adopting child!");
      mParent->InvalidateChildren();
    } else {
      NS_ERROR("Binding to the same parent!");
      return;
    }
  }

  mParent = aParent;
  mIndexInParent = aIndexInParent;
}

void
nsAccessible::UnbindFromParent()
{
  mParent = nsnull;
  mIndexInParent = -1;
  mIndexOfEmbeddedChild = -1;
  mGroupInfo = nsnull;
}

void
nsAccessible::InvalidateChildren()
{
  PRInt32 childCount = mChildren.Length();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible* child = mChildren.ElementAt(childIdx);
    child->UnbindFromParent();
  }

  mEmbeddedObjCollector = nsnull;
  mChildren.Clear();
  mChildrenFlags = eChildrenUninitialized;
}

PRBool
nsAccessible::AppendChild(nsAccessible* aChild)
{
  if (!mChildren.AppendElement(aChild))
    return PR_FALSE;

  if (!nsAccUtils::IsEmbeddedObject(aChild))
    mChildrenFlags = eMixedChildren;

  aChild->BindToParent(this, mChildren.Length() - 1);
  return PR_TRUE;
}

PRBool
nsAccessible::InsertChildAt(PRUint32 aIndex, nsAccessible* aChild)
{
  if (!mChildren.InsertElementAt(aIndex, aChild))
    return PR_FALSE;

  for (PRUint32 idx = aIndex + 1; idx < mChildren.Length(); idx++)
    mChildren[idx]->mIndexInParent++;

  if (nsAccUtils::IsText(aChild))
    mChildrenFlags = eMixedChildren;

  mEmbeddedObjCollector = nsnull;

  aChild->BindToParent(this, aIndex);
  return PR_TRUE;
}

PRBool
nsAccessible::RemoveChild(nsAccessible* aChild)
{
  if (aChild->mParent != this || aChild->mIndexInParent == -1)
    return PR_FALSE;

  for (PRUint32 idx = aChild->mIndexInParent + 1; idx < mChildren.Length(); idx++)
    mChildren[idx]->mIndexInParent--;

  mChildren.RemoveElementAt(aChild->mIndexInParent);
  mEmbeddedObjCollector = nsnull;

  aChild->UnbindFromParent();
  return PR_TRUE;
}

nsAccessible*
nsAccessible::GetParent()
{
  if (mParent)
    return mParent;

  if (IsDefunct())
    return nsnull;

  // XXX: mParent can be null randomly because supposedly we get layout
  // notification and invalidate parent-child relations, this accessible stays
  // unattached. This should gone after bug 572951.
  NS_WARNING("Bad accessible tree!");

#ifdef DEBUG
  nsDocAccessible *docAccessible = GetDocAccessible();
  NS_ASSERTION(docAccessible, "No document accessible for valid accessible!");
#endif

  nsAccessible* parent = GetAccService()->GetContainerAccessible(mContent,
                                                                 mWeakShell);
  NS_ASSERTION(parent, "No accessible parent for valid accessible!");
  if (!parent)
    return nsnull;

  // Repair parent-child relations.
  parent->EnsureChildren();
  NS_ASSERTION(parent == mParent, "Wrong children repair!");
  return parent;
}

nsAccessible*
nsAccessible::GetChildAt(PRUint32 aIndex)
{
  if (EnsureChildren())
    return nsnull;

  nsAccessible *child = mChildren.SafeElementAt(aIndex, nsnull);
  if (!child)
    return nsnull;

#ifdef DEBUG
  nsAccessible* realParent = child->mParent;
  NS_ASSERTION(!realParent || realParent == this,
               "Two accessibles have the same first child accessible!");
#endif

  return child;
}

PRInt32
nsAccessible::GetChildCount()
{
  return EnsureChildren() ? -1 : mChildren.Length();
}

PRInt32
nsAccessible::GetIndexOf(nsAccessible* aChild)
{
  return EnsureChildren() || (aChild->mParent != this) ?
    -1 : aChild->GetIndexInParent();
}

PRInt32
nsAccessible::GetIndexInParent()
{
  // XXX: call GetParent() to repair the tree if it's broken.
  GetParent();
  return mIndexInParent;
}

PRInt32
nsAccessible::GetEmbeddedChildCount()
{
  if (EnsureChildren())
    return -1;

  if (mChildrenFlags == eMixedChildren) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector ? mEmbeddedObjCollector->Count() : -1;
  }

  return GetChildCount();
}

nsAccessible*
nsAccessible::GetEmbeddedChildAt(PRUint32 aIndex)
{
  if (EnsureChildren())
    return nsnull;

  if (mChildrenFlags == eMixedChildren) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector ?
      mEmbeddedObjCollector->GetAccessibleAt(aIndex) : nsnull;
  }

  return GetChildAt(aIndex);
}

PRInt32
nsAccessible::GetIndexOfEmbeddedChild(nsAccessible* aChild)
{
  if (EnsureChildren())
    return -1;

  if (mChildrenFlags == eMixedChildren) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector ?
      mEmbeddedObjCollector->GetIndexAt(aChild) : -1;
  }

  return GetIndexOf(aChild);
}

#ifdef DEBUG
PRBool
nsAccessible::IsInCache()
{
  nsDocAccessible *docAccessible =
    GetAccService()->GetDocAccessible(mContent->GetOwnerDoc());
  if (docAccessible)
    return docAccessible->GetCachedAccessibleByUniqueID(UniqueID()) ? PR_TRUE : PR_FALSE;

  return PR_FALSE;
}
#endif


////////////////////////////////////////////////////////////////////////////////
// HyperLinkAccessible methods

bool
nsAccessible::IsHyperLink()
{
  // Every embedded accessible within hypertext accessible implements
  // hyperlink interface.
  nsRefPtr<nsHyperTextAccessible> hyperText = do_QueryObject(GetParent());
  return hyperText && nsAccUtils::IsEmbeddedObject(this);
}

PRUint32
nsAccessible::StartOffset()
{
  NS_PRECONDITION(IsHyperLink(), "StartOffset is called not on hyper link!");

  nsRefPtr<nsHyperTextAccessible> hyperText(do_QueryObject(GetParent()));
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

PRUint32
nsAccessible::EndOffset()
{
  NS_PRECONDITION(IsHyperLink(), "EndOffset is called on not hyper link!");

  nsRefPtr<nsHyperTextAccessible> hyperText(do_QueryObject(GetParent()));
  return hyperText ? (hyperText->GetChildOffset(this) + 1) : 0;
}

bool
nsAccessible::IsValid()
{
  NS_PRECONDITION(IsHyperLink(), "IsValid is called on not hyper link!");

  PRUint32 state = nsAccUtils::State(this);
  return (0 == (state & nsIAccessibleStates::STATE_INVALID));
  // XXX In order to implement this we would need to follow every link
  // Perhaps we can get information about invalid links from the cache
  // In the mean time authors can use role="link" aria-invalid="true"
  // to force it for links they internally know to be invalid
}

bool
nsAccessible::IsSelected()
{
  NS_PRECONDITION(IsHyperLink(), "IsSelected is called on not hyper link!");
  return (gLastFocusedNode == GetNode());
}

PRUint32
nsAccessible::AnchorCount()
{
  NS_PRECONDITION(IsHyperLink(), "AnchorCount is called on not hyper link!");
  return 1;
}

nsAccessible*
nsAccessible::GetAnchor(PRUint32 aAnchorIndex)
{
  NS_PRECONDITION(IsHyperLink(), "GetAnchor is called on not hyper link!");
  return aAnchorIndex == 0 ? this : nsnull;
}

already_AddRefed<nsIURI>
nsAccessible::GetAnchorURI(PRUint32 aAnchorIndex)
{
  NS_PRECONDITION(IsHyperLink(), "GetAnchorURI is called on not hyper link!");

  if (aAnchorIndex != 0)
    return nsnull;

  // Check if it's a simple xlink.
  if (nsCoreUtils::IsXLink(mContent)) {
    nsAutoString href;
    mContent->GetAttr(kNameSpaceID_XLink, nsAccessibilityAtoms::href, href);

    nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
    nsCOMPtr<nsIDocument> document = mContent->GetOwnerDoc();
    nsIURI* anchorURI = nsnull;
    NS_NewURI(&anchorURI, href,
              document ? document->GetDocumentCharacterSet().get() : nsnull,
              baseURI);
    return anchorURI;
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// SelectAccessible

bool
nsAccessible::IsSelect()
{
  // If we have an ARIA role attribute present and the role allows multi
  // selectable state, then we need to support SelectAccessible interface. If
  // either attribute (role or multiselectable) change, then we'll destroy this
  // accessible so that we can follow COM identity rules.

  return mRoleMapEntry &&
    (mRoleMapEntry->attributeMap1 == eARIAMultiSelectable ||
     mRoleMapEntry->attributeMap2 == eARIAMultiSelectable ||
     mRoleMapEntry->attributeMap3 == eARIAMultiSelectable);
}

already_AddRefed<nsIArray>
nsAccessible::SelectedItems()
{
  nsCOMPtr<nsIMutableArray> selectedItems = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!selectedItems)
    return nsnull;

  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  nsIAccessible* selected = nsnull;
  while ((selected = iter.GetNext()))
    selectedItems->AppendElement(selected, PR_FALSE);

  nsIMutableArray* items = nsnull;
  selectedItems.forget(&items);
  return items;
}

PRUint32
nsAccessible::SelectedItemCount()
{
  PRUint32 count = 0;
  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.GetNext()))
    ++count;

  return count;
}

nsAccessible*
nsAccessible::GetSelectedItem(PRUint32 aIndex)
{
  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;

  PRUint32 index = 0;
  while ((selected = iter.GetNext()) && index < aIndex)
    index++;

  return selected;
}

bool
nsAccessible::IsItemSelected(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.GetNext()) && index < aIndex)
    index++;

  return selected &&
    nsAccUtils::State(selected) & nsIAccessibleStates::STATE_SELECTED;
}

bool
nsAccessible::AddItemToSelection(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.GetNext()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(PR_TRUE);

  return static_cast<bool>(selected);
}

bool
nsAccessible::RemoveItemFromSelection(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.GetNext()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(PR_FALSE);

  return static_cast<bool>(selected);
}

bool
nsAccessible::SelectAll()
{
  bool success = false;
  nsAccessible* selectable = nsnull;

  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  while((selectable = iter.GetNext())) {
    success = true;
    selectable->SetSelected(PR_TRUE);
  }
  return success;
}

bool
nsAccessible::UnselectAll()
{
  bool success = false;
  nsAccessible* selected = nsnull;

  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  while ((selected = iter.GetNext())) {
    success = true;
    selected->SetSelected(PR_FALSE);
  }
  return success;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected methods

void
nsAccessible::CacheChildren()
{
  nsAccTreeWalker walker(mWeakShell, mContent, GetAllowsAnonChildAccessibles());

  nsRefPtr<nsAccessible> child;
  while ((child = walker.GetNextChild()) && AppendChild(child));
}

void
nsAccessible::TestChildCache(nsAccessible *aCachedChild)
{
#ifdef DEBUG
  PRInt32 childCount = mChildren.Length();
  if (childCount == 0) {
    NS_ASSERTION(mChildrenFlags == eChildrenUninitialized,
                 "No children but initialized!");
    return;
  }

  nsAccessible *child = nsnull;
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    child = mChildren[childIdx];
    if (child == aCachedChild)
      break;
  }

  NS_ASSERTION(child == aCachedChild,
               "[TestChildCache] cached accessible wasn't found. Wrong accessible tree!");  
#endif
}

// nsAccessible public
PRBool
nsAccessible::EnsureChildren()
{
  if (IsDefunct()) {
    mChildrenFlags = eChildrenUninitialized;
    return PR_TRUE;
  }

  if (mChildrenFlags != eChildrenUninitialized)
    return PR_FALSE;

  // State is embedded children until text leaf accessible is appended.
  mChildrenFlags = eEmbeddedChildren; // Prevent reentry
  CacheChildren();

  return PR_FALSE;
}

nsAccessible*
nsAccessible::GetSiblingAtOffset(PRInt32 aOffset, nsresult* aError)
{
  if (IsDefunct()) {
    if (aError)
      *aError = NS_ERROR_FAILURE;

    return nsnull;
  }

  nsAccessible *parent = GetParent();
  if (!parent) {
    if (aError)
      *aError = NS_ERROR_UNEXPECTED;

    return nsnull;
  }

  if (mIndexInParent == -1) {
    if (aError)
      *aError = NS_ERROR_UNEXPECTED;

    return nsnull;
  }

  if (aError) {
    PRInt32 childCount = parent->GetChildCount();
    if (mIndexInParent + aOffset >= childCount) {
      *aError = NS_OK; // fail peacefully
      return nsnull;
    }
  }

  nsAccessible* child = parent->GetChildAt(mIndexInParent + aOffset);
  if (aError && !child)
    *aError = NS_ERROR_UNEXPECTED;

  return child;
}

nsAccessible *
nsAccessible::GetFirstAvailableAccessible(nsINode *aStartNode) const
{
  nsAccessible *accessible =
    GetAccService()->GetAccessibleInWeakShell(aStartNode, mWeakShell);
  if (accessible)
    return accessible;

  nsIContent *content = nsCoreUtils::GetRoleContent(aStartNode);
  nsAccTreeWalker walker(mWeakShell, content, PR_FALSE);
  nsRefPtr<nsAccessible> childAccessible = walker.GetNextChild();
  return childAccessible;
}

PRBool nsAccessible::CheckVisibilityInParentChain(nsIDocument* aDocument, nsIView* aView)
{
  nsIDocument* document = aDocument;
  nsIView* view = aView;
  // both view chain and widget chain are broken between chrome and content
  while (document != nsnull) {
    while (view != nsnull) {
      if (view->GetVisibility() == nsViewVisibility_kHide) {
        return PR_FALSE;
      }
      view = view->GetParent();
    }

    nsIDocument* parentDoc = document->GetParentDocument();
    if (parentDoc != nsnull) {
      nsIContent* content = parentDoc->FindContentForSubDocument(document);
      if (content != nsnull) {
        nsIPresShell* shell = parentDoc->GetShell();
        if (!shell) {
          return PR_FALSE;
        }
        nsIFrame* frame = content->GetPrimaryFrame();
        while (frame != nsnull && !frame->HasView()) {
          frame = frame->GetParent();
        }

        if (frame != nsnull) {
          view = frame->GetViewExternal();
        }
      }
    }

    document = parentDoc;
  }

  return PR_TRUE;
}

nsresult
nsAccessible::GetAttrValue(nsIAtom *aProperty, double *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;  // Node already shut down

 if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return NS_OK_NO_ARIA_VALUE;

  nsAutoString attrValue;
  mContent->GetAttr(kNameSpaceID_None, aProperty, attrValue);

  // Return zero value if there is no attribute or its value is empty.
  if (attrValue.IsEmpty())
    return NS_OK;

  PRInt32 error = NS_OK;
  double value = attrValue.ToFloat(&error);
  if (NS_SUCCEEDED(error))
    *aValue = value;

  return NS_OK;
}

PRUint32
nsAccessible::GetActionRule(PRUint32 aStates)
{
  if (aStates & nsIAccessibleStates::STATE_UNAVAILABLE)
    return eNoAction;
  
  // Check if it's simple xlink.
  if (nsCoreUtils::IsXLink(mContent))
    return eJumpAction;

  // Return "click" action on elements that have an attached popup menu.
  if (mContent->IsXUL())
    if (mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::popup))
      return eClickAction;

  // Has registered 'click' event handler.
  PRBool isOnclick = nsCoreUtils::HasClickListener(mContent);

  if (isOnclick)
    return eClickAction;
  
  // Get an action based on ARIA role.
  if (mRoleMapEntry &&
      mRoleMapEntry->actionRule != eNoAction)
    return mRoleMapEntry->actionRule;

  // Get an action based on ARIA attribute.
  if (nsAccUtils::HasDefinedARIAToken(mContent,
                                      nsAccessibilityAtoms::aria_expanded))
    return eExpandAction;

  return eNoAction;
}

AccGroupInfo*
nsAccessible::GetGroupInfo()
{
  if (mGroupInfo)
    return mGroupInfo;

  mGroupInfo = AccGroupInfo::CreateGroupInfo(this);
  return mGroupInfo;
}

void
nsAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet, PRInt32 *aSetSize)
{
  AccGroupInfo* groupInfo = GetGroupInfo();
  if (groupInfo) {
    *aPosInSet = groupInfo->PosInSet();
    *aSetSize = groupInfo->SetSize();
  }
}

PRInt32
nsAccessible::GetLevelInternal()
{
  PRInt32 level = nsAccUtils::GetDefaultLevel(this);

  PRUint32 role = Role();
  nsAccessible* parent = GetParent();

  if (role == nsIAccessibleRole::ROLE_OUTLINEITEM) {
    // Always expose 'level' attribute for 'outlineitem' accessible. The number
    // of nested 'grouping' accessibles containing 'outlineitem' accessible is
    // its level.
    level = 1;

    while (parent) {
      PRUint32 parentRole = parent->Role();

      if (parentRole == nsIAccessibleRole::ROLE_OUTLINE)
        break;
      if (parentRole == nsIAccessibleRole::ROLE_GROUPING)
        ++ level;

      parent = parent->GetParent();
    }

  } else if (role == nsIAccessibleRole::ROLE_LISTITEM) {
    // Expose 'level' attribute on nested lists. We assume nested list is a last
    // child of listitem of parent list. We don't handle the case when nested
    // lists have more complex structure, for example when there are accessibles
    // between parent listitem and nested list.

    // Calculate 'level' attribute based on number of parent listitems.
    level = 0;

    while (parent) {
      PRUint32 parentRole = parent->Role();

      if (parentRole == nsIAccessibleRole::ROLE_LISTITEM)
        ++ level;
      else if (parentRole != nsIAccessibleRole::ROLE_LIST)
        break;

      parent = parent->GetParent();
    }

    if (level == 0) {
      // If this listitem is on top of nested lists then expose 'level'
      // attribute.
      nsAccessible* parent(GetParent());
      PRInt32 siblingCount = parent->GetChildCount();
      for (PRInt32 siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        nsAccessible* sibling = parent->GetChildAt(siblingIdx);

        nsCOMPtr<nsIAccessible> siblingChild;
        sibling->GetLastChild(getter_AddRefs(siblingChild));
        if (nsAccUtils::Role(siblingChild) == nsIAccessibleRole::ROLE_LIST) {
          level = 1;
          break;
        }
      }
    } else {
      ++ level; // level is 1-index based
    }
  }

  return level;
}
