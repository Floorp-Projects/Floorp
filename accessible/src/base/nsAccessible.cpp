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
#include "nsAccEvent.h"
#include "nsAccessibleRelation.h"
#include "nsAccessibilityService.h"
#include "nsAccTreeWalker.h"
#include "nsIAccessibleRelation.h"
#include "nsEventShell.h"
#include "nsRootAccessible.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "StyleInfo.h"

#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMHTMLElement.h"
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

#include "nsLayoutUtils.h"
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
#include "nsIURI.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsWhitespaceTokenizer.h"
#include "nsAttrName.h"
#include "nsNetUtil.h"
#include "nsEventStates.h"

#ifdef NS_DEBUG
#include "nsIDOMCharacterData.h"
#endif

#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;


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
    if (IsLink()) {
      *aInstancePtr = static_cast<nsIAccessibleHyperLink*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_ERROR_NO_INTERFACE;
  }

  return nsAccessNodeWrap::QueryInterface(aIID, aInstancePtr);
}

nsAccessible::nsAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsAccessNodeWrap(aContent, aDoc),
  mParent(nsnull), mIndexInParent(-1), mFlags(eChildrenUninitialized),
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
      printf(" Con: %s@%p",
             NS_ConvertUTF16toUTF8(content->NodeInfo()->QualifiedName()).get(),
             (void *)content.get());
      nsAutoString buf;
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
nsAccessible::GetDocument(nsIAccessibleDocument **aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  NS_IF_ADDREF(*aDocument = Document());
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetDOMNode(nsIDOMNode **aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nsnull;

  nsINode *node = GetNode();
  if (node)
    CallQueryInterface(node, aDOMNode);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetRootDocument(nsIAccessibleDocument **aRootDocument)
{
  NS_ENSURE_ARG_POINTER(aRootDocument);

  nsRootAccessible* rootDocument = RootAccessible();
  NS_IF_ADDREF(*aRootDocument = rootDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetLanguage(nsAString& aLanguage)
{
  Language(aLanguage);
  return NS_OK;
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
    tooltipAttr = nsGkAtoms::title;
  else if (mContent->IsXUL())
    tooltipAttr = nsGkAtoms::tooltiptext;
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
    aName.SetIsVoid(true);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetDescription(nsAString& aDescription)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString desc;
  Description(desc);
  aDescription.Assign(desc);

  return NS_OK;
}

void
nsAccessible::Description(nsString& aDescription)
{
  // There are 4 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. It has no DHTML describedby property
  // 3. it doesn't have an accName; or
  // 4. its title attribute already equals to its accName nsAutoString name; 

  if (mContent->IsNodeOfType(nsINode::eTEXT))
    return;

  nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsGkAtoms::aria_describedby,
                           aDescription);

  if (aDescription.IsEmpty()) {
    bool isXUL = mContent->IsXUL();
    if (isXUL) {
      // Try XUL <description control="[id]">description text</description>
      XULDescriptionIterator iter(Document(), mContent);
      nsAccessible* descr = nsnull;
      while ((descr = iter.Next()))
        nsTextEquivUtils::AppendTextEquivFromContent(this, descr->GetContent(),
                                                     &aDescription);
      }

      if (aDescription.IsEmpty()) {
        nsIAtom *descAtom = isXUL ? nsGkAtoms::tooltiptext :
                                    nsGkAtoms::title;
        if (mContent->GetAttr(kNameSpaceID_None, descAtom, aDescription)) {
          nsAutoString name;
          GetName(name);
          if (name.IsEmpty() || aDescription == name)
            // Don't use tooltip for a description if this object
            // has no name or the tooltip is the same as the name
            aDescription.Truncate();
        }
      }
    }
    aDescription.CompressWhitespace();
}

NS_IMETHODIMP
nsAccessible::GetKeyboardShortcut(nsAString& aAccessKey)
{
  aAccessKey.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccessKey().ToString(aAccessKey);
  return NS_OK;
}

KeyBinding
nsAccessible::AccessKey() const
{
  PRUint32 key = nsCoreUtils::GetAccessKeyFor(mContent);
  if (!key && mContent->IsElement()) {
    nsAccessible* label = nsnull;

    // Copy access key from label node.
    if (mContent->IsHTML()) {
      // Unless it is labeled via an ancestor <label>, in which case that would
      // be redundant.
      HTMLLabelIterator iter(Document(), this,
                             HTMLLabelIterator::eSkipAncestorLabel);
      label = iter.Next();

    } else if (mContent->IsXUL()) {
      XULLabelIterator iter(Document(), mContent);
      label = iter.Next();
    }

    if (label)
      key = nsCoreUtils::GetAccessKeyFor(label->GetContent());
  }

  if (!key)
    return KeyBinding();

  // Get modifier mask. Use ui.key.generalAccessKey (unless it is -1).
  switch (Preferences::GetInt("ui.key.generalAccessKey", -1)) {
  case -1:
    break;
  case nsIDOMKeyEvent::DOM_VK_SHIFT:
    return KeyBinding(key, KeyBinding::kShift);
  case nsIDOMKeyEvent::DOM_VK_CONTROL:
    return KeyBinding(key, KeyBinding::kControl);
  case nsIDOMKeyEvent::DOM_VK_ALT:
    return KeyBinding(key, KeyBinding::kAlt);
  case nsIDOMKeyEvent::DOM_VK_META:
    return KeyBinding(key, KeyBinding::kMeta);
  default:
    return KeyBinding();
  }

  // Determine the access modifier used in this context.
  nsIDocument* document = mContent->GetCurrentDoc();
  if (!document)
    return KeyBinding();
  nsCOMPtr<nsISupports> container = document->GetContainer();
  if (!container)
    return KeyBinding();
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));
  if (!treeItem)
    return KeyBinding();

  nsresult rv = NS_ERROR_FAILURE;
  PRInt32 itemType = 0, modifierMask = 0;
  treeItem->GetItemType(&itemType);
  switch (itemType) {
    case nsIDocShellTreeItem::typeChrome:
      rv = Preferences::GetInt("ui.key.chromeAccess", &modifierMask);
      break;
    case nsIDocShellTreeItem::typeContent:
      rv = Preferences::GetInt("ui.key.contentAccess", &modifierMask);
      break;
  }

  return NS_SUCCEEDED(rv) ? KeyBinding(key, modifierMask) : KeyBinding();
}

KeyBinding
nsAccessible::KeyboardShortcut() const
{
  return KeyBinding();
}

NS_IMETHODIMP
nsAccessible::GetParent(nsIAccessible **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aParent = Parent());
  return *aParent ? NS_OK : NS_ERROR_FAILURE;
}

  /* readonly attribute nsIAccessible nextSibling; */
NS_IMETHODIMP
nsAccessible::GetNextSibling(nsIAccessible **aNextSibling) 
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  NS_IF_ADDREF(*aNextSibling = GetSiblingAtOffset(1, &rv));
  return rv;
}

  /* readonly attribute nsIAccessible previousSibling; */
NS_IMETHODIMP
nsAccessible::GetPreviousSibling(nsIAccessible * *aPreviousSibling) 
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

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

  if (IsDefunct())
    return NS_ERROR_FAILURE;

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

  if (IsDefunct())
    return NS_ERROR_FAILURE;

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

  if (IsDefunct())
    return NS_ERROR_FAILURE;

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

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 childCount = GetChildCount();
  NS_ENSURE_TRUE(childCount != -1, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> children =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsIAccessible* child = GetChildAt(childIdx);
    children->AppendElement(child, false);
  }

  NS_ADDREF(*aOutChildren = children);
  return NS_OK;
}

bool
nsAccessible::CanHaveAnonChildren()
{
  return true;
}

/* readonly attribute long childCount; */
NS_IMETHODIMP
nsAccessible::GetChildCount(PRInt32 *aChildCount) 
{
  NS_ENSURE_ARG_POINTER(aChildCount);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aChildCount = GetChildCount();
  return *aChildCount != -1 ? NS_OK : NS_ERROR_FAILURE;  
}

/* readonly attribute long indexInParent; */
NS_IMETHODIMP
nsAccessible::GetIndexInParent(PRInt32 *aIndexInParent)
{
  NS_ENSURE_ARG_POINTER(aIndexInParent);

  *aIndexInParent = IndexInParent();
  return *aIndexInParent != -1 ? NS_OK : NS_ERROR_FAILURE;
}

void 
nsAccessible::TranslateString(const nsAString& aKey, nsAString& aStringOut)
{
  nsXPIDLString xsValue;

  gStringBundle->GetStringFromName(PromiseFlatString(aKey).get(), getter_Copies(xsValue));
  aStringOut.Assign(xsValue);
}

PRUint64
nsAccessible::VisibilityState()
{
  PRUint64 vstates = states::INVISIBLE | states::OFFSCREEN;

  nsIFrame* frame = GetFrame();
  if (!frame)
    return vstates;

  nsIPresShell* shell(mDoc->PresShell());
  if (!shell)
    return vstates;

  // We need to know if at least a kMinPixels around the object is visible,
  // otherwise it will be marked states::OFFSCREEN.
  const PRUint16 kMinPixels  = 12;
  const nsSize frameSize = frame->GetSize();
  const nsRectVisibility rectVisibility =
    shell->GetRectVisibility(frame, nsRect(nsPoint(0,0), frameSize),
                             nsPresContext::CSSPixelsToAppUnits(kMinPixels));

  if (rectVisibility == nsRectVisibility_kVisible)
    vstates &= ~states::OFFSCREEN;

  // Zero area rects can occur in the first frame of a multi-frame text flow,
  // in which case the rendered text is not empty and the frame should not be
  // marked invisible.
  // XXX Can we just remove this check? Why do we need to mark empty
  // text invisible?
  if (frame->GetType() == nsGkAtoms::textFrame &&
      !(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
      frame->GetRect().IsEmpty()) {
    nsAutoString renderedText;
    frame->GetRenderedText(&renderedText, nsnull, nsnull, 0, 1);
    if (renderedText.IsEmpty())
      return vstates;

  }

  // XXX Do we really need to cross from content to chrome ancestor?
  if (!frame->IsVisibleConsideringAncestors(nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY))
    return vstates;

  // Assume we are visible enough.
  return vstates &= ~states::INVISIBLE;
}

PRUint64
nsAccessible::NativeState()
{
  PRUint64 state = 0;

  nsDocAccessible* document = Document();
  if (!document || !document->IsInDocument(this))
    state |= states::STALE;

  bool disabled = false;
  if (mContent->IsElement()) {
    nsEventStates elementState = mContent->AsElement()->State();

    if (elementState.HasState(NS_EVENT_STATE_INVALID))
      state |= states::INVALID;

    if (elementState.HasState(NS_EVENT_STATE_REQUIRED))
      state |= states::REQUIRED;

    disabled = mContent->IsHTML() ? 
      (elementState.HasState(NS_EVENT_STATE_DISABLED)) :
      (mContent->AttrValueIs(kNameSpaceID_None,
                             nsGkAtoms::disabled,
                             nsGkAtoms::_true,
                             eCaseMatters));
  }

  // Set unavailable state based on disabled state, otherwise set focus states
  if (disabled) {
    state |= states::UNAVAILABLE;
  }
  else if (mContent->IsElement()) {
    nsIFrame* frame = GetFrame();
    if (frame && frame->IsFocusable())
      state |= states::FOCUSABLE;

    if (FocusMgr()->IsFocused(this))
      state |= states::FOCUSED;
  }

  // Gather states::INVISIBLE and states::OFFSCREEN flags for this object.
  state |= VisibilityState();

  nsIFrame *frame = GetFrame();
  if (frame && (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW))
    state |= states::FLOATING;

  // Check if a XUL element has the popup attribute (an attached popup menu).
  if (mContent->IsXUL())
    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::popup))
      state |= states::HASPOPUP;

  // Add 'linked' state for simple xlink.
  if (nsCoreUtils::IsXLink(mContent))
    state |= states::LINKED;

  return state;
}

  /* readonly attribute boolean focusedChild; */
NS_IMETHODIMP
nsAccessible::GetFocusedChild(nsIAccessible** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aChild = FocusedChild());
  return NS_OK;
}

nsAccessible*
nsAccessible::FocusedChild()
{
  nsAccessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && (focus == this || focus->Parent() == this))
    return focus;

  return nsnull;
}

// nsAccessible::ChildAtPoint()
nsAccessible*
nsAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                           EWhichChildAtPoint aWhichChild)
{
  // If we can't find the point in a child, we will return the fallback answer:
  // we return |this| if the point is within it, otherwise nsnull.
  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = GetBounds(&x, &y, &width, &height);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsAccessible* fallbackAnswer = nsnull;
  if (aX >= x && aX < x + width && aY >= y && aY < y + height)
    fallbackAnswer = this;

  if (nsAccUtils::MustPrune(this))  // Do not dig any further
    return fallbackAnswer;

  // Search an accessible at the given point starting from accessible document
  // because containing block (see CSS2) for out of flow element (for example,
  // absolutely positioned element) may be different from its DOM parent and
  // therefore accessible for containing block may be different from accessible
  // for DOM parent but GetFrameForPoint() should be called for containing block
  // to get an out of flow element.
  nsDocAccessible* accDocument = Document();
  NS_ENSURE_TRUE(accDocument, nsnull);

  nsIFrame *frame = accDocument->GetFrame();
  NS_ENSURE_TRUE(frame, nsnull);

  nsPresContext *presContext = frame->PresContext();

  nsRect screenRect = frame->GetScreenRectInAppUnits();
  nsPoint offset(presContext->DevPixelsToAppUnits(aX) - screenRect.x,
                 presContext->DevPixelsToAppUnits(aY) - screenRect.y);

  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
  nsIFrame *foundFrame = presShell->GetFrameForPoint(frame, offset);

  nsIContent* content = nsnull;
  if (!foundFrame || !(content = foundFrame->GetContent()))
    return fallbackAnswer;

  // Get accessible for the node with the point or the first accessible in
  // the DOM parent chain.
  nsDocAccessible* contentDocAcc = GetAccService()->
    GetDocAccessible(content->OwnerDoc());

  // contentDocAcc in some circumstances can be NULL. See bug 729861
  NS_ASSERTION(contentDocAcc, "could not get the document accessible");
  if (!contentDocAcc)
    return fallbackAnswer;

  nsAccessible* accessible = contentDocAcc->GetAccessibleOrContainer(content);
  if (!accessible)
    return fallbackAnswer;

  if (accessible == this) {
    // Manually walk through accessible children and see if the are within this
    // point. Skip offscreen or invisible accessibles. This takes care of cases
    // where layout won't walk into things for us, such as image map areas and
    // sub documents (XXX: subdocuments should be handled by methods of
    // OuterDocAccessibles).
    PRInt32 childCount = GetChildCount();
    for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
      nsAccessible *child = GetChildAt(childIdx);

      PRInt32 childX, childY, childWidth, childHeight;
      child->GetBounds(&childX, &childY, &childWidth, &childHeight);
      if (aX >= childX && aX < childX + childWidth &&
          aY >= childY && aY < childY + childHeight &&
          (child->State() & states::INVISIBLE) == 0) {

        if (aWhichChild == eDeepestChild)
          return child->ChildAtPoint(aX, aY, eDeepestChild);

        return child;
      }
    }

    // The point is in this accessible but not in a child. We are allowed to
    // return |this| as the answer.
    return accessible;
  }

  // Since DOM node of obtained accessible may be out of flow then we should
  // ensure obtained accessible is a child of this accessible.
  nsAccessible* child = accessible;
  while (true) {
    nsAccessible* parent = child->Parent();
    if (!parent) {
      // Reached the top of the hierarchy. These bounds were inside an
      // accessible that is not a descendant of this one.
      return fallbackAnswer;
    }

    if (parent == this)
      return aWhichChild == eDeepestChild ? accessible : child;

    child = parent;
  }

  return nsnull;
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

  NS_IF_ADDREF(*aAccessible = ChildAtPoint(aX, aY, eDirectChild));
  return NS_OK;
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

  NS_IF_ADDREF(*aAccessible = ChildAtPoint(aX, aY, eDeepestChild));
  return NS_OK;
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
    if (ancestorFrame->GetType() != nsGkAtoms::inlineFrame &&
        ancestorFrame->GetType() != nsGkAtoms::textFrame)
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

    if (iterFrame->GetType() == nsGkAtoms::inlineFrame) {
      // Only do deeper bounds search if we're on an inline frame
      // Inline frames can contain larger frames inside of them
      iterNextFrame = iterFrame->GetFirstPrincipalChild();
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

  nsIPresShell* presShell = mDoc->PresShell();

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
  nsIntRect orgRectPixels = boundingFrame->GetScreenRectInAppUnits().
    ToNearestPixels(presContext->AppUnitsPerDevPixel());
  *aX += orgRectPixels.x;
  *aY += orgRectPixels.y;

  return NS_OK;
}

// helpers

nsIFrame* nsAccessible::GetBoundsFrame()
{
  return GetFrame();
}

NS_IMETHODIMP nsAccessible::SetSelected(bool aSelect)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE) {
      if (mRoleMapEntry) {
        if (aSelect) {
          return mContent->SetAttr(kNameSpaceID_None,
                                   nsGkAtoms::aria_selected,
                                   NS_LITERAL_STRING("true"), true);
        }
        return mContent->UnsetAttr(kNameSpaceID_None,
                                   nsGkAtoms::aria_selected, true);
      }

      return NS_OK;
    }

    return aSelect ? TakeFocus() : NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::TakeSelection()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAccessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE)
      select->ClearSelection();
    return SetSelected(true);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAccessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsIFrame *frame = GetFrame();
  NS_ENSURE_STATE(frame);

  nsIContent* focusContent = mContent;

  // If the accessible focus is managed by container widget then focus the
  // widget and set the accessible as its current item.
  if (!frame->IsFocusable()) {
    nsAccessible* widget = ContainerWidget();
    if (widget && widget->AreItemsOperable()) {
      nsIContent* widgetElm = widget->GetContent();
      nsIFrame* widgetFrame = widgetElm->GetPrimaryFrame();
      if (widgetFrame && widgetFrame->IsFocusable()) {
        focusContent = widgetElm;
        widget->SetCurrentItem(this);
      }
    }
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(focusContent));
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->SetFocus(element, 0);

  return NS_OK;
}

nsresult
nsAccessible::GetHTMLName(nsAString& aLabel)
{
  nsAutoString label;

  nsAccessible* labelAcc = nsnull;
  HTMLLabelIterator iter(Document(), this);
  while ((labelAcc = iter.Next())) {
    nsresult rv = nsTextEquivUtils::
      AppendTextEquivFromContent(this, labelAcc->GetContent(), &label);
    NS_ENSURE_SUCCESS(rv, rv);

    label.CompressWhitespace();
  }

  if (label.IsEmpty())
    return nsTextEquivUtils::GetNameFromSubtree(this, aLabel);

  aLabel = label;
  return NS_OK;
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

    nsAccessible* labelAcc = nsnull;
    XULLabelIterator iter(Document(), mContent);
    while ((labelAcc = iter.Next())) {
      nsCOMPtr<nsIDOMXULLabelElement> xulLabel =
        do_QueryInterface(labelAcc->GetContent());
      // Check if label's value attribute is used
      if (xulLabel && NS_SUCCEEDED(xulLabel->GetValue(label)) && label.IsEmpty()) {
        // If no value attribute, a non-empty label must contain
        // children that define its text -- possibly using HTML
        nsTextEquivUtils::
          AppendTextEquivFromContent(this, labelAcc->GetContent(), &label);
      }
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
    if (parent->Tag() == nsGkAtoms::toolbaritem &&
        parent->GetAttr(kNameSpaceID_None, nsGkAtoms::title, label)) {
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

  bool hasObservers = false;
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
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::role, xmlRoles)) {
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
  if (State() & states::CHECKABLE)
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::checkable, NS_LITERAL_STRING("true"));

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
    nsAccUtils::GetAccAttr(attributes, nsGkAtoms::live, live);
    if (live.IsEmpty()) {
      if (nsAccUtils::GetLiveAttrValue(mRoleMapEntry->liveAttRule, live))
        nsAccUtils::SetAccAttr(attributes, nsGkAtoms::live, live);
    }
  }

  return NS_OK;
}

nsresult
nsAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  // If the accessible isn't primary for its node (such as list item bullet or
  // xul tree item then don't calculate content based attributes.
  if (!IsPrimaryForNode())
    return NS_OK;

  // Attributes set by this method will not be used to override attributes on a sub-document accessible
  // when there is a <frame>/<iframe> element that spawned the sub-document

  nsEventShell::GetEventAttributes(GetNode(), aAttributes);
 
  // Expose class because it may have useful microformat information
  // Let the class from an iframe's document be exposed, don't override from <iframe class>
  nsAutoString _class;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, _class))
    nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::_class, _class);

  // Get container-foo computed live region properties based on the closest container with
  // the live region attribute. 
  // Inner nodes override outer nodes within the same document --
  //   The inner nodes can be used to override live region behavior on more general outer nodes
  // However, nodes in outer documents override nodes in inner documents:
  //   Outer doc author may want to override properties on a widget they used in an iframe
  nsIContent* startContent = mContent;
  while (startContent) {
    nsIDocument* doc = startContent->GetDocument();
    nsIContent* rootContent = nsCoreUtils::GetRoleContent(doc);
    if (!rootContent)
      return NS_OK;

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

  if (!mContent->IsElement())
    return NS_OK;

  // Expose tag.
  nsAutoString tagName;
  mContent->NodeInfo()->GetName(tagName);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::tag, tagName);

  // Expose draggable object attribute?
  nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(mContent);
  if (htmlElement) {
    bool draggable = false;
    htmlElement->GetDraggable(&draggable);
    if (draggable) {
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::draggable,
                             NS_LITERAL_STRING("true"));
    }
  }

  // Don't calculate CSS-based object attributes when no frame (i.e.
  // the accessible is unattached from the tree).
  if (!mContent->GetPrimaryFrame())
    return NS_OK;

  // CSS style based object attributes.
  nsAutoString value;
  StyleInfo styleInfo(mContent->AsElement(), mDoc->PresShell());

  // Expose 'display' attribute.
  styleInfo.Display(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::display, value);

  // Expose 'text-align' attribute.
  styleInfo.TextAlign(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textAlign, value);

  // Expose 'text-indent' attribute.
  styleInfo.TextIndent(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textIndent, value);

  // Expose 'margin-left' attribute.
  styleInfo.MarginLeft(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::marginLeft, value);

  // Expose 'margin-right' attribute.
  styleInfo.MarginRight(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::marginRight, value);

  // Expose 'margin-top' attribute.
  styleInfo.MarginTop(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::marginTop, value);

  // Expose 'margin-bottom' attribute.
  styleInfo.MarginBottom(value);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::marginBottom, value);

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
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_level,
                           aGroupLevel);
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_posinset,
                           aPositionInGroup);
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_setsize,
                           aSimilarItemsInGroup);

  // If ARIA is missed and the accessible is visible then calculate group
  // position from hierarchy.
  if (State() & states::INVISIBLE)
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
nsAccessible::GetState(PRUint32* aState, PRUint32* aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsAccUtils::To32States(State(), aState, aExtraState);
  return NS_OK;
}

PRUint64
nsAccessible::State()
{
  if (IsDefunct())
    return states::DEFUNCT;

  PRUint64 state = NativeState();
  // Apply ARIA states to be sure accessible states will be overridden.
  ApplyARIAState(&state);

  // If this is an ARIA item of the selectable widget and if it's focused and
  // not marked unselected explicitly (i.e. aria-selected="false") then expose
  // it as selected to make ARIA widget authors life easier.
  if (mRoleMapEntry && !(state & states::SELECTED) &&
      !mContent->AttrValueIs(kNameSpaceID_None,
                             nsGkAtoms::aria_selected,
                             nsGkAtoms::_false, eCaseMatters)) {
    // Special case for tabs: focused tab or focus inside related tab panel
    // implies selected state.
    if (mRoleMapEntry->role == roles::PAGETAB) {
      if (state & states::FOCUSED) {
        state |= states::SELECTED;
      } else {
        // If focus is in a child of the tab panel surely the tab is selected!
        Relation rel = RelationByType(nsIAccessibleRelation::RELATION_LABEL_FOR);
        nsAccessible* relTarget = nsnull;
        while ((relTarget = rel.Next())) {
          if (relTarget->Role() == roles::PROPERTYPAGE &&
              FocusMgr()->IsFocusWithin(relTarget))
            state |= states::SELECTED;
        }
      }
    } else if (state & states::FOCUSED) {
      nsAccessible* container = nsAccUtils::GetSelectableContainer(this, state);
      if (container &&
          !nsAccUtils::HasDefinedARIAToken(container->GetContent(),
                                           nsGkAtoms::aria_multiselectable)) {
        state |= states::SELECTED;
      }
    }
  }

  const PRUint32 kExpandCollapseStates = states::COLLAPSED | states::EXPANDED;
  if ((state & kExpandCollapseStates) == kExpandCollapseStates) {
    // Cannot be both expanded and collapsed -- this happens in ARIA expanded
    // combobox because of limitation of nsARIAMap.
    // XXX: Perhaps we will be able to make this less hacky if we support
    // extended states in nsARIAMap, e.g. derive COLLAPSED from
    // EXPANDABLE && !EXPANDED.
    state &= ~states::COLLAPSED;
  }

  if (!(state & states::UNAVAILABLE)) {
    state |= states::ENABLED | states::SENSITIVE;

    // If the object is a current item of container widget then mark it as
    // ACTIVE. This allows screen reader virtual buffer modes to know which
    // descendant is the current one that would get focus if the user navigates
    // to the container widget.
    nsAccessible* widget = ContainerWidget();
    if (widget && widget->CurrentItem() == this)
      state |= states::ACTIVE;
  }

  if ((state & states::COLLAPSED) || (state & states::EXPANDED))
    state |= states::EXPANDABLE;

  // For some reasons DOM node may have not a frame. We tract such accessibles
  // as invisible.
  nsIFrame *frame = GetFrame();
  if (!frame)
    return state;

  const nsStyleDisplay* display = frame->GetStyleDisplay();
  if (display && display->mOpacity == 1.0f &&
      !(state & states::INVISIBLE)) {
    state |= states::OPAQUE1;
  }

  const nsStyleXUL *xulStyle = frame->GetStyleXUL();
  if (xulStyle) {
    // In XUL all boxes are either vertical or horizontal
    if (xulStyle->mBoxOrient == NS_STYLE_BOX_ORIENT_VERTICAL) {
      state |= states::VERTICAL;
    }
    else {
      state |= states::HORIZONTAL;
    }
  }

  return state;
}

void
nsAccessible::ApplyARIAState(PRUint64* aState)
{
  if (!mContent->IsElement())
    return;

  dom::Element* element = mContent->AsElement();

  // Test for universal states first
  *aState |= nsARIAMap::UniversalStatesFor(element);

  if (mRoleMapEntry) {

    // We only force the readonly bit off if we have a real mapping for the aria
    // role. This preserves the ability for screen readers to use readonly
    // (primarily on the document) as the hint for creating a virtual buffer.
    if (mRoleMapEntry->role != roles::NOTHING)
      *aState &= ~states::READONLY;

    if (mContent->HasAttr(kNameSpaceID_None, mContent->GetIDAttributeName())) {
      // If has a role & ID and aria-activedescendant on the container, assume focusable
      nsIContent *ancestorContent = mContent;
      while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
        if (ancestorContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant)) {
            // ancestor has activedescendant property, this content could be active
          *aState |= states::FOCUSABLE;
          break;
        }
      }
    }
  }

  if (*aState & states::FOCUSABLE) {
    // Special case: aria-disabled propagates from ancestors down to any focusable descendant
    nsIContent *ancestorContent = mContent;
    while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
      if (ancestorContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_disabled,
                                       nsGkAtoms::_true, eCaseMatters)) {
          // ancestor has aria-disabled property, this is disabled
        *aState |= states::UNAVAILABLE;
        break;
      }
    }    
  }

  if (!mRoleMapEntry)
    return;

  *aState |= mRoleMapEntry->state;

  if (aria::MapToState(mRoleMapEntry->attributeMap1, element, aState) &&
      aria::MapToState(mRoleMapEntry->attributeMap2, element, aState))
    aria::MapToState(mRoleMapEntry->attributeMap3, element, aState);
}

NS_IMETHODIMP
nsAccessible::GetValue(nsAString& aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString value;
  Value(value);
  aValue.Assign(value);

  return NS_OK;
}

void
nsAccessible::Value(nsString& aValue)
{
  if (mRoleMapEntry) {
    if (mRoleMapEntry->valueRule == eNoValue)
      return;

    // aria-valuenow is a number, and aria-valuetext is the optional text equivalent
    // For the string value, we will try the optional text equivalent first
    if (!mContent->GetAttr(kNameSpaceID_None,
                           nsGkAtoms::aria_valuetext, aValue)) {
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_valuenow,
                        aValue);
    }
  }

  if (!aValue.IsEmpty())
    return;

  // Check if it's a simple xlink.
  if (nsCoreUtils::IsXLink(mContent)) {
    nsIPresShell* presShell = mDoc->PresShell();
    if (presShell) {
      nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
      presShell->GetLinkLocation(DOMNode, aValue);
    }
  }
}

// nsIAccessibleValue
NS_IMETHODIMP
nsAccessible::GetMaximumValue(double *aMaximumValue)
{
  return GetAttrValue(nsGkAtoms::aria_valuemax, aMaximumValue);
}

NS_IMETHODIMP
nsAccessible::GetMinimumValue(double *aMinimumValue)
{
  return GetAttrValue(nsGkAtoms::aria_valuemin, aMinimumValue);
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
  return GetAttrValue(nsGkAtoms::aria_valuenow, aValue);
}

NS_IMETHODIMP
nsAccessible::SetCurrentValue(double aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return NS_OK_NO_ARIA_VALUE;

  const PRUint32 kValueCannotChange = states::READONLY | states::UNAVAILABLE;

  if (State() & kValueCannotChange)
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
                           nsGkAtoms::aria_valuenow, newValue, true);
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
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  KeyboardShortcut().ToString(aKeyBinding);
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

role
nsAccessible::ARIATransformRole(role aRole)
{
  // XXX: these unfortunate exceptions don't fit into the ARIA table. This is
  // where the accessible role depends on both the role and ARIA state.
  if (aRole == roles::PUSHBUTTON) {
    if (nsAccUtils::HasDefinedARIAToken(mContent, nsGkAtoms::aria_pressed)) {
      // For simplicity, any existing pressed attribute except "" or "undefined"
      // indicates a toggle.
      return roles::TOGGLE_BUTTON;
    }

    if (mContent->AttrValueIs(kNameSpaceID_None,
                              nsGkAtoms::aria_haspopup,
                              nsGkAtoms::_true,
                              eCaseMatters)) {
      // For button with aria-haspopup="true".
      return roles::BUTTONMENU;
    }

  } else if (aRole == roles::LISTBOX) {
    // A listbox inside of a combobox needs a special role because of ATK
    // mapping to menu.
    if (mParent && mParent->Role() == roles::COMBOBOX) {
      return roles::COMBOBOX_LIST;

      Relation rel = RelationByType(nsIAccessibleRelation::RELATION_NODE_CHILD_OF);
      nsAccessible* targetAcc = nsnull;
      while ((targetAcc = rel.Next()))
        if (targetAcc->Role() == roles::COMBOBOX)
          return roles::COMBOBOX_LIST;
    }

  } else if (aRole == roles::OPTION) {
    if (mParent && mParent->Role() == roles::COMBOBOX_LIST)
      return roles::COMBOBOX_OPTION;
  }

  return aRole;
}

role
nsAccessible::NativeRole()
{
  return nsCoreUtils::IsXLink(mContent) ? roles::LINK : roles::NOTHING;
}

// readonly attribute PRUint8 numActions
NS_IMETHODIMP
nsAccessible::GetNumActions(PRUint8* aActionCount)
{
  NS_ENSURE_ARG_POINTER(aActionCount);
  *aActionCount = 0;
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aActionCount = ActionCount();
  return NS_OK;
}

PRUint8
nsAccessible::ActionCount()
{
  return GetActionRule(State()) == eNoAction ? 0 : 1;
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

  PRUint64 states = State();
  PRUint32 actionRule = GetActionRule(states);

 switch (actionRule) {
   case eActivateAction:
     aName.AssignLiteral("activate");
     return NS_OK;

   case eClickAction:
     aName.AssignLiteral("click");
     return NS_OK;

   case ePressAction:
     aName.AssignLiteral("press");
     return NS_OK;

   case eCheckUncheckAction:
     if (states & states::CHECKED)
       aName.AssignLiteral("uncheck");
     else if (states & states::MIXED)
       aName.AssignLiteral("cycle");
     else
       aName.AssignLiteral("check");
     return NS_OK;

   case eJumpAction:
     aName.AssignLiteral("jump");
     return NS_OK;

   case eOpenCloseAction:
     if (states & states::COLLAPSED)
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
     if (states & states::COLLAPSED)
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

  TranslateString(name, aDescription);
  return NS_OK;
}

// void doAction(in PRUint8 index)
NS_IMETHODIMP
nsAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (GetActionRule(State()) != eNoAction) {
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

nsIContent*
nsAccessible::GetAtomicRegion() const
{
  nsIContent *loopContent = mContent;
  nsAutoString atomic;
  while (loopContent && !loopContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_atomic, atomic))
    loopContent = loopContent->GetParent();

  return atomic.EqualsLiteral("true") ? loopContent : nsnull;
}

// nsIAccessible getRelationByType()
NS_IMETHODIMP
nsAccessible::GetRelationByType(PRUint32 aType,
                                nsIAccessibleRelation** aRelation)
{
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nsnull;
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Relation rel = RelationByType(aType);
  NS_ADDREF(*aRelation = new nsAccessibleRelation(aType, &rel));
  return *aRelation ? NS_OK : NS_ERROR_FAILURE;
}

Relation
nsAccessible::RelationByType(PRUint32 aType)
{
  // Relationships are defined on the same content node that the role would be
  // defined on.
  switch (aType) {
    case nsIAccessibleRelation::RELATION_LABEL_FOR: {
      Relation rel(new RelatedAccIterator(Document(), mContent,
                                          nsGkAtoms::aria_labelledby));
      if (mContent->Tag() == nsGkAtoms::label)
        rel.AppendIter(new IDRefsIterator(mDoc, mContent, mContent->IsHTML() ?
                                          nsGkAtoms::_for :
                                          nsGkAtoms::control));

      return rel;
    }
    case nsIAccessibleRelation::RELATION_LABELLED_BY: {
      Relation rel(new IDRefsIterator(mDoc, mContent,
                                      nsGkAtoms::aria_labelledby));
      if (mContent->IsHTML()) {
        rel.AppendIter(new HTMLLabelIterator(Document(), this));
      } else if (mContent->IsXUL()) {
        rel.AppendIter(new XULLabelIterator(Document(), mContent));
      }

      return rel;
    }
    case nsIAccessibleRelation::RELATION_DESCRIBED_BY: {
      Relation rel(new IDRefsIterator(mDoc, mContent,
                                      nsGkAtoms::aria_describedby));
      if (mContent->IsXUL())
        rel.AppendIter(new XULDescriptionIterator(Document(), mContent));

      return rel;
    }
    case nsIAccessibleRelation::RELATION_DESCRIPTION_FOR: {
      Relation rel(new RelatedAccIterator(Document(), mContent,
                                          nsGkAtoms::aria_describedby));

      // This affectively adds an optional control attribute to xul:description,
      // which only affects accessibility, by allowing the description to be
      // tied to a control.
      if (mContent->Tag() == nsGkAtoms::description &&
          mContent->IsXUL())
        rel.AppendIter(new IDRefsIterator(mDoc, mContent,
                                          nsGkAtoms::control));

      return rel;
    }
    case nsIAccessibleRelation::RELATION_NODE_CHILD_OF: {
      Relation rel(new RelatedAccIterator(Document(), mContent,
                                          nsGkAtoms::aria_owns));
      
      // This is an ARIA tree or treegrid that doesn't use owns, so we need to
      // get the parent the hard way.
      if (mRoleMapEntry && (mRoleMapEntry->role == roles::OUTLINEITEM || 
                            mRoleMapEntry->role == roles::ROW)) {
        AccGroupInfo* groupInfo = GetGroupInfo();
        if (!groupInfo)
          return rel;

        rel.AppendTarget(groupInfo->ConceptualParent());
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
          if (scrollFrame || view->GetWidget() || !frame->GetParent())
            rel.AppendTarget(Parent());
        }
      }

      return rel;
    }
    case nsIAccessibleRelation::RELATION_CONTROLLED_BY:
      return Relation(new RelatedAccIterator(Document(), mContent,
                                             nsGkAtoms::aria_controls));
    case nsIAccessibleRelation::RELATION_CONTROLLER_FOR: {
      Relation rel(new IDRefsIterator(mDoc, mContent,
                                      nsGkAtoms::aria_controls));
      rel.AppendIter(new HTMLOutputIterator(Document(), mContent));
      return rel;
    }
    case nsIAccessibleRelation::RELATION_FLOWS_TO:
      return Relation(new IDRefsIterator(mDoc, mContent,
                                         nsGkAtoms::aria_flowto));
    case nsIAccessibleRelation::RELATION_FLOWS_FROM:
      return Relation(new RelatedAccIterator(Document(), mContent,
                                             nsGkAtoms::aria_flowto));
    case nsIAccessibleRelation::RELATION_DEFAULT_BUTTON: {
      if (mContent->IsHTML()) {
        // HTML form controls implements nsIFormControl interface.
        nsCOMPtr<nsIFormControl> control(do_QueryInterface(mContent));
        if (control) {
          nsCOMPtr<nsIForm> form(do_QueryInterface(control->GetFormElement()));
          if (form) {
            nsCOMPtr<nsIContent> formContent =
              do_QueryInterface(form->GetDefaultSubmitElement());
            return Relation(formContent);
          }
        }
      } else {
        // In XUL, use first <button default="true" .../> in the document
        nsCOMPtr<nsIDOMXULDocument> xulDoc =
          do_QueryInterface(mContent->OwnerDoc());
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
          return Relation(relatedContent);
        }
      }
      return Relation();
    }
    case nsIAccessibleRelation::RELATION_MEMBER_OF:
      return Relation(GetAtomicRegion());
    case nsIAccessibleRelation::RELATION_SUBWINDOW_OF:
    case nsIAccessibleRelation::RELATION_EMBEDS:
    case nsIAccessibleRelation::RELATION_EMBEDDED_BY:
    case nsIAccessibleRelation::RELATION_POPUP_FOR:
    case nsIAccessibleRelation::RELATION_PARENT_WINDOW_OF:
    default:
    return Relation();
  }
}

NS_IMETHODIMP
nsAccessible::GetRelations(nsIArray **aRelations)
{
  NS_ENSURE_ARG_POINTER(aRelations);
  *aRelations = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMutableArray> relations = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(relations, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 relType = nsIAccessibleRelation::RELATION_FIRST;
       relType < nsIAccessibleRelation::RELATION_LAST;
       ++relType) {

    nsCOMPtr<nsIAccessibleRelation> relation;
    nsresult rv = GetRelationByType(relType, getter_AddRefs(relation));

    if (NS_SUCCEEDED(rv) && relation) {
      PRUint32 targets = 0;
      relation->GetTargetsCount(&targets);
      if (targets)
        relations->AppendElement(relation, false);
    }
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

  nsIPresShell* presShell = mDoc->PresShell();

  // Scroll into view.
  presShell->ScrollContentIntoView(aContent,
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::SCROLL_OVERFLOW_HIDDEN);

  // Fire mouse down and mouse up events.
  bool res = nsCoreUtils::DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, presShell,
                                               aContent);
  if (!res)
    return;

  nsCoreUtils::DispatchMouseEvent(NS_MOUSE_BUTTON_UP, presShell, aContent);
}

NS_IMETHODIMP
nsAccessible::ScrollTo(PRUint32 aHow)
{
  nsCoreUtils::ScrollTo(mDoc->PresShell(), mContent, aHow);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::ScrollToPoint(PRUint32 aCoordinateType, PRInt32 aX, PRInt32 aY)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  nsIntPoint coords;
  nsresult rv = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType,
                                                  this, &coords);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame *parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent()))
    nsCoreUtils::ScrollFrameToPoint(parentFrame, frame, coords);

  return NS_OK;
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

NS_IMETHODIMP nsAccessible::IsChildSelected(PRInt32 aIndex, bool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

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
nsAccessible::SelectAllSelection(bool* aIsMultiSelect)
{
  NS_ENSURE_ARG_POINTER(aIsMultiSelect);
  *aIsMultiSelect = false;

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

  nsRefPtr<nsIURI>(AnchorURIAt(aIndex)).forget(aURI);
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

  NS_IF_ADDREF(*aAccessible = AnchorAt(aIndex));
  return NS_OK;
}

// readonly attribute boolean nsIAccessibleHyperLink::valid
NS_IMETHODIMP
nsAccessible::GetValid(bool *aValid)
{
  NS_ENSURE_ARG_POINTER(aValid);
  *aValid = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aValid = IsLinkValid();
  return NS_OK;
}

// readonly attribute boolean nsIAccessibleHyperLink::selected
NS_IMETHODIMP
nsAccessible::GetSelected(bool *aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aSelected = IsLinkSelected();
  return NS_OK;

}

void
nsAccessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset,
                           PRUint32 aLength)
{
  // Return text representation of non-text accessible within hypertext
  // accessible. Text accessible overrides this method to return enclosed text.
  if (aStartOffset != 0 || aLength == 0)
    return;

  nsIFrame *frame = GetFrame();
  if (!frame)
    return;

  if (frame->GetType() == nsGkAtoms::brFrame) {
    aText += kForcedNewLineChar;
  } else if (nsAccUtils::MustPrune(Parent())) {
    // Expose the embedded object accessible as imaginary embedded object
    // character if its parent hypertext accessible doesn't expose children to
    // AT.
    aText += kImaginaryEmbeddedObjectChar;
  } else {
    aText += kEmbeddedObjectChar;
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public methods

void
nsAccessible::Shutdown()
{
  // Mark the accessible as defunct, invalidate the child count and pointers to 
  // other accessibles, also make sure none of its children point to this parent
  mFlags |= eIsDefunct;

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
    GetTextEquivFromIDRefs(this, nsGkAtoms::aria_labelledby, label);
  if (NS_SUCCEEDED(rv)) {
    label.CompressWhitespace();
    aName = label;
  }

  if (label.IsEmpty() &&
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_label,
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
      mParent->RemoveChild(this);
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
  SetChildrenFlag(eChildrenUninitialized);
}

bool
nsAccessible::AppendChild(nsAccessible* aChild)
{
  if (!aChild)
    return false;

  if (!mChildren.AppendElement(aChild))
    return false;

  if (!nsAccUtils::IsEmbeddedObject(aChild))
    SetChildrenFlag(eMixedChildren);

  aChild->BindToParent(this, mChildren.Length() - 1);
  return true;
}

bool
nsAccessible::InsertChildAt(PRUint32 aIndex, nsAccessible* aChild)
{
  if (!aChild)
    return false;

  if (!mChildren.InsertElementAt(aIndex, aChild))
    return false;

  for (PRUint32 idx = aIndex + 1; idx < mChildren.Length(); idx++) {
    NS_ASSERTION(mChildren[idx]->mIndexInParent == idx - 1, "Accessible child index doesn't match");
    mChildren[idx]->mIndexInParent = idx;
  }

  if (nsAccUtils::IsText(aChild))
    SetChildrenFlag(eMixedChildren);

  mEmbeddedObjCollector = nsnull;

  aChild->BindToParent(this, aIndex);
  return true;
}

bool
nsAccessible::RemoveChild(nsAccessible* aChild)
{
  if (!aChild)
    return false;

  if (aChild->mParent != this || aChild->mIndexInParent == -1)
    return false;

  PRUint32 index = static_cast<PRUint32>(aChild->mIndexInParent);
  if (index >= mChildren.Length() || mChildren[index] != aChild) {
    NS_ERROR("Child is bound to parent but parent hasn't this child at its index!");
    aChild->UnbindFromParent();
    return false;
  }

  for (PRUint32 idx = index + 1; idx < mChildren.Length(); idx++) {
    NS_ASSERTION(mChildren[idx]->mIndexInParent == idx, "Accessible child index doesn't match");
    mChildren[idx]->mIndexInParent = idx - 1;
  }

  aChild->UnbindFromParent();
  mChildren.RemoveElementAt(index);
  mEmbeddedObjCollector = nsnull;

  return true;
}

nsAccessible*
nsAccessible::GetChildAt(PRUint32 aIndex)
{
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
  return mChildren.Length();
}

PRInt32
nsAccessible::GetIndexOf(nsAccessible* aChild)
{
  return (aChild->mParent != this) ? -1 : aChild->IndexInParent();
}

PRInt32
nsAccessible::IndexInParent() const
{
  return mIndexInParent;
}

PRInt32
nsAccessible::GetEmbeddedChildCount()
{
  if (IsChildrenFlag(eMixedChildren)) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector ? mEmbeddedObjCollector->Count() : -1;
  }

  return GetChildCount();
}

nsAccessible*
nsAccessible::GetEmbeddedChildAt(PRUint32 aIndex)
{
  if (IsChildrenFlag(eMixedChildren)) {
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
  if (IsChildrenFlag(eMixedChildren)) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector ?
      mEmbeddedObjCollector->GetIndexAt(aChild) : -1;
  }

  return GetIndexOf(aChild);
}

////////////////////////////////////////////////////////////////////////////////
// HyperLinkAccessible methods

bool
nsAccessible::IsLink()
{
  // Every embedded accessible within hypertext accessible implements
  // hyperlink interface.
  return mParent && mParent->IsHyperText() && nsAccUtils::IsEmbeddedObject(this);
}

PRUint32
nsAccessible::StartOffset()
{
  NS_PRECONDITION(IsLink(), "StartOffset is called not on hyper link!");

  nsHyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nsnull;
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

PRUint32
nsAccessible::EndOffset()
{
  NS_PRECONDITION(IsLink(), "EndOffset is called on not hyper link!");

  nsHyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nsnull;
  return hyperText ? (hyperText->GetChildOffset(this) + 1) : 0;
}

bool
nsAccessible::IsLinkSelected()
{
  NS_PRECONDITION(IsLink(),
                  "IsLinkSelected() called on something that is not a hyper link!");
  return FocusMgr()->IsFocused(this);
}

PRUint32
nsAccessible::AnchorCount()
{
  NS_PRECONDITION(IsLink(), "AnchorCount is called on not hyper link!");
  return 1;
}

nsAccessible*
nsAccessible::AnchorAt(PRUint32 aAnchorIndex)
{
  NS_PRECONDITION(IsLink(), "GetAnchor is called on not hyper link!");
  return aAnchorIndex == 0 ? this : nsnull;
}

already_AddRefed<nsIURI>
nsAccessible::AnchorURIAt(PRUint32 aAnchorIndex)
{
  NS_PRECONDITION(IsLink(), "AnchorURIAt is called on not hyper link!");

  if (aAnchorIndex != 0)
    return nsnull;

  // Check if it's a simple xlink.
  if (nsCoreUtils::IsXLink(mContent)) {
    nsAutoString href;
    mContent->GetAttr(kNameSpaceID_XLink, nsGkAtoms::href, href);

    nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
    nsCOMPtr<nsIDocument> document = mContent->OwnerDoc();
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
    (mRoleMapEntry->attributeMap1 == aria::eARIAMultiSelectable ||
     mRoleMapEntry->attributeMap2 == aria::eARIAMultiSelectable ||
     mRoleMapEntry->attributeMap3 == aria::eARIAMultiSelectable);
}

already_AddRefed<nsIArray>
nsAccessible::SelectedItems()
{
  nsCOMPtr<nsIMutableArray> selectedItems = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!selectedItems)
    return nsnull;

  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  nsIAccessible* selected = nsnull;
  while ((selected = iter.Next()))
    selectedItems->AppendElement(selected, false);

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
  while ((selected = iter.Next()))
    ++count;

  return count;
}

nsAccessible*
nsAccessible::GetSelectedItem(PRUint32 aIndex)
{
  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;

  PRUint32 index = 0;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  return selected;
}

bool
nsAccessible::IsItemSelected(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  return selected &&
    selected->State() & states::SELECTED;
}

bool
nsAccessible::AddItemToSelection(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(true);

  return static_cast<bool>(selected);
}

bool
nsAccessible::RemoveItemFromSelection(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  nsAccessible* selected = nsnull;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(false);

  return static_cast<bool>(selected);
}

bool
nsAccessible::SelectAll()
{
  bool success = false;
  nsAccessible* selectable = nsnull;

  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  while((selectable = iter.Next())) {
    success = true;
    selectable->SetSelected(true);
  }
  return success;
}

bool
nsAccessible::UnselectAll()
{
  bool success = false;
  nsAccessible* selected = nsnull;

  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  while ((selected = iter.Next())) {
    success = true;
    selected->SetSelected(false);
  }
  return success;
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool
nsAccessible::IsWidget() const
{
  return false;
}

bool
nsAccessible::IsActiveWidget() const
{
  return FocusMgr()->IsFocused(this);
}

bool
nsAccessible::AreItemsOperable() const
{
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant);
}

nsAccessible*
nsAccessible::CurrentItem()
{
  // Check for aria-activedescendant, which changes which element has focus.
  // For activedescendant, the ARIA spec does not require that the user agent
  // checks whether pointed node is actually a DOM descendant of the element
  // with the aria-activedescendant attribute.
  nsAutoString id;
  if (mContent->GetAttr(kNameSpaceID_None,
                        nsGkAtoms::aria_activedescendant, id)) {
    nsIDocument* DOMDoc = mContent->OwnerDoc();
    dom::Element* activeDescendantElm = DOMDoc->GetElementById(id);
    if (activeDescendantElm) {
      nsDocAccessible* document = Document();
      if (document)
        return document->GetAccessible(activeDescendantElm);
    }
  }
  return nsnull;
}

void
nsAccessible::SetCurrentItem(nsAccessible* aItem)
{
  nsIAtom* id = aItem->GetContent()->GetID();
  if (id) {
    nsAutoString idStr;
    id->ToString(idStr);
    mContent->SetAttr(kNameSpaceID_None,
                      nsGkAtoms::aria_activedescendant, idStr, true);
  }
}

nsAccessible*
nsAccessible::ContainerWidget() const
{
  if (HasARIARole() && mContent->HasID()) {
    for (nsAccessible* parent = Parent(); parent; parent = parent->Parent()) {
      nsIContent* parentContent = parent->GetContent();
      if (parentContent &&
        parentContent->HasAttr(kNameSpaceID_None,
                               nsGkAtoms::aria_activedescendant)) {
        return parent;
      }

      // Don't cross DOM document boundaries.
      if (parent->IsDocumentNode())
        break;
    }
  }
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected methods

void
nsAccessible::CacheChildren()
{
  nsDocAccessible* doc = Document();
  NS_ENSURE_TRUE(doc,);

  nsAccTreeWalker walker(doc, mContent, CanHaveAnonChildren());

  nsAccessible* child = nsnull;
  while ((child = walker.NextChild()) && AppendChild(child));
}

void
nsAccessible::TestChildCache(nsAccessible* aCachedChild) const
{
#ifdef DEBUG
  PRInt32 childCount = mChildren.Length();
  if (childCount == 0) {
    NS_ASSERTION(IsChildrenFlag(eChildrenUninitialized),
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
bool
nsAccessible::EnsureChildren()
{
  if (IsDefunct()) {
    SetChildrenFlag(eChildrenUninitialized);
    return true;
  }

  if (!IsChildrenFlag(eChildrenUninitialized))
    return false;

  // State is embedded children until text leaf accessible is appended.
  SetChildrenFlag(eEmbeddedChildren); // Prevent reentry

  CacheChildren();
  return false;
}

nsAccessible*
nsAccessible::GetSiblingAtOffset(PRInt32 aOffset, nsresult* aError) const
{
  if (!mParent || mIndexInParent == -1) {
    if (aError)
      *aError = NS_ERROR_UNEXPECTED;

    return nsnull;
  }

  if (aError && mIndexInParent + aOffset >= mParent->GetChildCount()) {
    *aError = NS_OK; // fail peacefully
    return nsnull;
  }

  nsAccessible* child = mParent->GetChildAt(mIndexInParent + aOffset);
  if (aError && !child)
    *aError = NS_ERROR_UNEXPECTED;

  return child;
}

nsAccessible *
nsAccessible::GetFirstAvailableAccessible(nsINode *aStartNode) const
{
  nsAccessible* accessible = mDoc->GetAccessible(aStartNode);
  if (accessible)
    return accessible;

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aStartNode->OwnerDoc());
  NS_ENSURE_TRUE(domDoc, nsnull);

  nsCOMPtr<nsIDOMNode> currentNode = do_QueryInterface(aStartNode);
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetNode());
  nsCOMPtr<nsIDOMTreeWalker> walker;
  domDoc->CreateTreeWalker(rootNode,
                           nsIDOMNodeFilter::SHOW_ELEMENT | nsIDOMNodeFilter::SHOW_TEXT,
                           nsnull, false, getter_AddRefs(walker));
  NS_ENSURE_TRUE(walker, nsnull);

  walker->SetCurrentNode(currentNode);
  while (true) {
    walker->NextNode(getter_AddRefs(currentNode));
    if (!currentNode)
      return nsnull;

    nsCOMPtr<nsINode> node(do_QueryInterface(currentNode));
    nsAccessible* accessible = mDoc->GetAccessible(node);
    if (accessible)
      return accessible;
  }

  return nsnull;
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
  double value = attrValue.ToDouble(&error);
  if (NS_SUCCEEDED(error))
    *aValue = value;

  return NS_OK;
}

PRUint32
nsAccessible::GetActionRule(PRUint64 aStates)
{
  if (aStates & states::UNAVAILABLE)
    return eNoAction;
  
  // Check if it's simple xlink.
  if (nsCoreUtils::IsXLink(mContent))
    return eJumpAction;

  // Return "click" action on elements that have an attached popup menu.
  if (mContent->IsXUL())
    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::popup))
      return eClickAction;

  // Has registered 'click' event handler.
  bool isOnclick = nsCoreUtils::HasClickListener(mContent);

  if (isOnclick)
    return eClickAction;
  
  // Get an action based on ARIA role.
  if (mRoleMapEntry &&
      mRoleMapEntry->actionRule != eNoAction)
    return mRoleMapEntry->actionRule;

  // Get an action based on ARIA attribute.
  if (nsAccUtils::HasDefinedARIAToken(mContent,
                                      nsGkAtoms::aria_expanded))
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

  if (!IsBoundToParent())
    return level;

  roles::Role role = Role();
  if (role == roles::OUTLINEITEM) {
    // Always expose 'level' attribute for 'outlineitem' accessible. The number
    // of nested 'grouping' accessibles containing 'outlineitem' accessible is
    // its level.
    level = 1;

    nsAccessible* parent = this;
    while ((parent = parent->Parent())) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::OUTLINE)
        break;
      if (parentRole == roles::GROUPING)
        ++ level;

    }

  } else if (role == roles::LISTITEM) {
    // Expose 'level' attribute on nested lists. We assume nested list is a last
    // child of listitem of parent list. We don't handle the case when nested
    // lists have more complex structure, for example when there are accessibles
    // between parent listitem and nested list.

    // Calculate 'level' attribute based on number of parent listitems.
    level = 0;
    nsAccessible* parent = this;
    while ((parent = parent->Parent())) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::LISTITEM)
        ++ level;
      else if (parentRole != roles::LIST)
        break;

    }

    if (level == 0) {
      // If this listitem is on top of nested lists then expose 'level'
      // attribute.
      parent = Parent();
      PRInt32 siblingCount = parent->GetChildCount();
      for (PRInt32 siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        nsAccessible* sibling = parent->GetChildAt(siblingIdx);

        nsAccessible* siblingChild = sibling->LastChild();
        if (siblingChild && siblingChild->Role() == roles::LIST)
          return 1;
      }
    } else {
      ++ level; // level is 1-index based
    }
  }

  return level;
}


////////////////////////////////////////////////////////////////////////////////
// KeyBinding class

void
KeyBinding::ToPlatformFormat(nsAString& aValue) const
{
  nsCOMPtr<nsIStringBundle> keyStringBundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      mozilla::services::GetStringBundleService();
  if (stringBundleService)
    stringBundleService->CreateBundle(PLATFORM_KEYS_BUNDLE_URL,
                                      getter_AddRefs(keyStringBundle));

  if (!keyStringBundle)
    return;

  nsAutoString separator;
  keyStringBundle->GetStringFromName(NS_LITERAL_STRING("MODIFIER_SEPARATOR").get(),
                                     getter_Copies(separator));

  nsAutoString modifierName;
  if (mModifierMask & kControl) {
    keyStringBundle->GetStringFromName(NS_LITERAL_STRING("VK_CONTROL").get(),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kAlt) {
    keyStringBundle->GetStringFromName(NS_LITERAL_STRING("VK_ALT").get(),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kShift) {
    keyStringBundle->GetStringFromName(NS_LITERAL_STRING("VK_SHIFT").get(),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kMeta) {
    keyStringBundle->GetStringFromName(NS_LITERAL_STRING("VK_META").get(),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  aValue.Append(mKey);
}

void
KeyBinding::ToAtkFormat(nsAString& aValue) const
{
  nsAutoString modifierName;
  if (mModifierMask & kControl)
    aValue.Append(NS_LITERAL_STRING("<Control>"));

  if (mModifierMask & kAlt)
    aValue.Append(NS_LITERAL_STRING("<Alt>"));

  if (mModifierMask & kShift)
    aValue.Append(NS_LITERAL_STRING("<Shift>"));

  if (mModifierMask & kMeta)
      aValue.Append(NS_LITERAL_STRING("<Meta>"));

  aValue.Append(mKey);
}
