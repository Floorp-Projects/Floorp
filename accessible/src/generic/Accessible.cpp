/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"

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
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "States.h"
#include "StyleInfo.h"

#include "nsContentUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
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
#include "nsIStringBundle.h"
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

#ifdef DEBUG
#include "nsIDOMCharacterData.h"
#endif

#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;


////////////////////////////////////////////////////////////////////////////////
// Accessible. nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(Accessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Accessible, nsAccessNode)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mParent");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mParent.get()));

  PRUint32 i, length = tmp->mChildren.Length();
  for (i = 0; i < length; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildren[i]");
    cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mChildren[i].get()));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Accessible, nsAccessNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mChildren)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(Accessible, nsAccessNode)
NS_IMPL_RELEASE_INHERITED(Accessible, nsAccessNode)

nsresult
Accessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  // Custom-built QueryInterface() knows when we support nsIAccessibleSelectable
  // based on role attribute and aria-multiselectable
  *aInstancePtr = nsnull;

  if (aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(Accessible);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIAccessible))) {
    *aInstancePtr = static_cast<nsIAccessible*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(Accessible))) {
    *aInstancePtr = static_cast<Accessible*>(this);
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

Accessible::Accessible(nsIContent* aContent, DocAccessible* aDoc) :
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
      Name(buf);
      printf(" Name:[%s]", NS_ConvertUTF16toUTF8(buf).get());
     }
     printf("\n");
   }
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
Accessible::~Accessible()
{
}

void
Accessible::SetRoleMapEntry(nsRoleMapEntry* aRoleMapEntry)
{
  mRoleMapEntry = aRoleMapEntry;
}

NS_IMETHODIMP
Accessible::GetDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  NS_IF_ADDREF(*aDocument = Document());
  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetDOMNode(nsIDOMNode** aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nsnull;

  nsINode *node = GetNode();
  if (node)
    CallQueryInterface(node, aDOMNode);

  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetRootDocument(nsIAccessibleDocument** aRootDocument)
{
  NS_ENSURE_ARG_POINTER(aRootDocument);

  NS_IF_ADDREF(*aRootDocument = RootAccessible());
  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetLanguage(nsAString& aLanguage)
{
  Language(aLanguage);
  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetName(nsAString& aName)
{
  aName.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString name;
  Name(name);
  aName.Assign(name);

  return NS_OK;
}

ENameValueFlag
Accessible::Name(nsString& aName)
{
  aName.Truncate();

  GetARIAName(aName);
  if (!aName.IsEmpty())
    return eNameOK;

  nsCOMPtr<nsIXBLAccessible> xblAccessible(do_QueryInterface(mContent));
  if (xblAccessible) {
    xblAccessible->GetAccessibleName(aName);
    if (!aName.IsEmpty())
      return eNameOK;
  }

  nsresult rv = GetNameInternal(aName);
  if (!aName.IsEmpty())
    return eNameOK;

  // In the end get the name from tooltip.
  if (mContent->IsHTML()) {
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::title, aName)) {
      aName.CompressWhitespace();
      return eNameFromTooltip;
    }
  } else if (mContent->IsXUL()) {
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext, aName)) {
      aName.CompressWhitespace();
      return eNameFromTooltip;
    }
  } else {
    return eNameOK;
  }

  if (rv != NS_OK_EMPTY_NAME)
    aName.SetIsVoid(true);

  return eNameOK;
}

NS_IMETHODIMP
Accessible::GetDescription(nsAString& aDescription)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString desc;
  Description(desc);
  aDescription.Assign(desc);

  return NS_OK;
}

void
Accessible::Description(nsString& aDescription)
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
      Accessible* descr = nsnull;
      while ((descr = iter.Next()))
        nsTextEquivUtils::AppendTextEquivFromContent(this, descr->GetContent(),
                                                     &aDescription);
      }

      if (aDescription.IsEmpty()) {
        nsIAtom *descAtom = isXUL ? nsGkAtoms::tooltiptext :
                                    nsGkAtoms::title;
        if (mContent->GetAttr(kNameSpaceID_None, descAtom, aDescription)) {
          nsAutoString name;
          Name(name);
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
Accessible::GetAccessKey(nsAString& aAccessKey)
{
  aAccessKey.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  AccessKey().ToString(aAccessKey);
  return NS_OK;
}

KeyBinding
Accessible::AccessKey() const
{
  PRUint32 key = nsCoreUtils::GetAccessKeyFor(mContent);
  if (!key && mContent->IsElement()) {
    Accessible* label = nsnull;

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
Accessible::KeyboardShortcut() const
{
  return KeyBinding();
}

NS_IMETHODIMP
Accessible::GetParent(nsIAccessible** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aParent = Parent());
  return *aParent ? NS_OK : NS_ERROR_FAILURE;
}

  /* readonly attribute nsIAccessible nextSibling; */
NS_IMETHODIMP
Accessible::GetNextSibling(nsIAccessible** aNextSibling)
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
Accessible::GetPreviousSibling(nsIAccessible ** aPreviousSibling)
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
Accessible::GetFirstChild(nsIAccessible** aFirstChild)
{
  NS_ENSURE_ARG_POINTER(aFirstChild);
  *aFirstChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aFirstChild = FirstChild());
  return NS_OK;
}

  /* readonly attribute nsIAccessible lastChild; */
NS_IMETHODIMP
Accessible::GetLastChild(nsIAccessible** aLastChild)
{
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aLastChild = LastChild());
  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetChildAt(PRInt32 aChildIndex, nsIAccessible** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // If child index is negative, then return last child.
  // XXX: do we really need this?
  if (aChildIndex < 0)
    aChildIndex = ChildCount() - 1;

  Accessible* child = GetChildAt(aChildIndex);
  if (!child)
    return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*aChild = child);
  return NS_OK;
}

// readonly attribute nsIArray children;
NS_IMETHODIMP
Accessible::GetChildren(nsIArray** aOutChildren)
{
  NS_ENSURE_ARG_POINTER(aOutChildren);
  *aOutChildren = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> children =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsIAccessible* child = GetChildAt(childIdx);
    children->AppendElement(child, false);
  }

  NS_ADDREF(*aOutChildren = children);
  return NS_OK;
}

bool
Accessible::CanHaveAnonChildren()
{
  return true;
}

/* readonly attribute long childCount; */
NS_IMETHODIMP
Accessible::GetChildCount(PRInt32* aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aChildCount = ChildCount();
  return NS_OK;
}

/* readonly attribute long indexInParent; */
NS_IMETHODIMP
Accessible::GetIndexInParent(PRInt32* aIndexInParent)
{
  NS_ENSURE_ARG_POINTER(aIndexInParent);

  *aIndexInParent = IndexInParent();
  return *aIndexInParent != -1 ? NS_OK : NS_ERROR_FAILURE;
}

void 
Accessible::TranslateString(const nsString& aKey, nsAString& aStringOut)
{
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    services::GetStringBundleService();
  if (!stringBundleService)
    return;

  nsCOMPtr<nsIStringBundle> stringBundle;
  stringBundleService->CreateBundle(
    "chrome://global-platform/locale/accessible.properties", 
    getter_AddRefs(stringBundle));
  if (!stringBundle)
    return;

  nsXPIDLString xsValue;
  nsresult rv = stringBundle->GetStringFromName(aKey.get(), getter_Copies(xsValue));
  if (NS_SUCCEEDED(rv))
    aStringOut.Assign(xsValue);
}

PRUint64
Accessible::VisibilityState()
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
Accessible::NativeState()
{
  PRUint64 state = 0;

  if (!IsInDocument())
    state |= states::STALE;

  if (mContent->IsElement()) {
    nsEventStates elementState = mContent->AsElement()->State();

    if (elementState.HasState(NS_EVENT_STATE_INVALID))
      state |= states::INVALID;

    if (elementState.HasState(NS_EVENT_STATE_REQUIRED))
      state |= states::REQUIRED;

    state |= NativeInteractiveState();
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

  // Bypass the link states specialization for non links.
  if (!mRoleMapEntry || mRoleMapEntry->roleRule == kUseNativeRole ||
      mRoleMapEntry->role == roles::LINK)
    state |= NativeLinkState();

  return state;
}

PRUint64
Accessible::NativeInteractiveState() const
{
  if (!mContent->IsElement())
    return 0;

  if (NativelyUnavailable())
    return states::UNAVAILABLE;

  nsIFrame* frame = GetFrame();
  if (frame && frame->IsFocusable())
    return states::FOCUSABLE;

  return 0;
}

PRUint64
Accessible::NativeLinkState() const
{
  // Expose linked state for simple xlink.
  return nsCoreUtils::IsXLink(mContent) ? states::LINKED : 0;
}

bool
Accessible::NativelyUnavailable() const
{
  if (mContent->IsHTML())
    return mContent->AsElement()->State().HasState(NS_EVENT_STATE_DISABLED);

  return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

  /* readonly attribute boolean focusedChild; */
NS_IMETHODIMP
Accessible::GetFocusedChild(nsIAccessible** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aChild = FocusedChild());
  return NS_OK;
}

Accessible*
Accessible::FocusedChild()
{
  Accessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && (focus == this || focus->Parent() == this))
    return focus;

  return nsnull;
}

Accessible*
Accessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                         EWhichChildAtPoint aWhichChild)
{
  // If we can't find the point in a child, we will return the fallback answer:
  // we return |this| if the point is within it, otherwise nsnull.
  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = GetBounds(&x, &y, &width, &height);
  NS_ENSURE_SUCCESS(rv, nsnull);

  Accessible* fallbackAnswer = nsnull;
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
  DocAccessible* accDocument = Document();
  NS_ENSURE_TRUE(accDocument, nsnull);

  nsIFrame *frame = accDocument->GetFrame();
  NS_ENSURE_TRUE(frame, nsnull);

  nsPresContext *presContext = frame->PresContext();

  nsRect screenRect = frame->GetScreenRectInAppUnits();
  nsPoint offset(presContext->DevPixelsToAppUnits(aX) - screenRect.x,
                 presContext->DevPixelsToAppUnits(aY) - screenRect.y);

  nsIPresShell* presShell = presContext->PresShell();
  nsIFrame *foundFrame = presShell->GetFrameForPoint(frame, offset);

  nsIContent* content = nsnull;
  if (!foundFrame || !(content = foundFrame->GetContent()))
    return fallbackAnswer;

  // Get accessible for the node with the point or the first accessible in
  // the DOM parent chain.
  DocAccessible* contentDocAcc = GetAccService()->
    GetDocAccessible(content->OwnerDoc());

  // contentDocAcc in some circumstances can be NULL. See bug 729861
  NS_ASSERTION(contentDocAcc, "could not get the document accessible");
  if (!contentDocAcc)
    return fallbackAnswer;

  Accessible* accessible = contentDocAcc->GetAccessibleOrContainer(content);
  if (!accessible)
    return fallbackAnswer;

  // Hurray! We have an accessible for the frame that layout gave us.
  // Since DOM node of obtained accessible may be out of flow then we should
  // ensure obtained accessible is a child of this accessible.
  Accessible* child = accessible;
  while (child != this) {
    Accessible* parent = child->Parent();
    if (!parent) {
      // Reached the top of the hierarchy. These bounds were inside an
      // accessible that is not a descendant of this one.
      return fallbackAnswer;
    }

    // If we landed on a legitimate child of |this|, and we want the direct
    // child, return it here.
    if (parent == this && aWhichChild == eDirectChild)
        return child;

    child = parent;
  }

  // Manually walk through accessible children and see if the are within this
  // point. Skip offscreen or invisible accessibles. This takes care of cases
  // where layout won't walk into things for us, such as image map areas and
  // sub documents (XXX: subdocuments should be handled by methods of
  // OuterDocAccessibles).
  PRUint32 childCount = accessible->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = accessible->GetChildAt(childIdx);

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

  return accessible;
}

// nsIAccessible getChildAtPoint(in long x, in long y)
NS_IMETHODIMP
Accessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                            nsIAccessible** aAccessible)
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
Accessible::GetDeepestChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible = ChildAtPoint(aX, aY, eDeepestChild));
  return NS_OK;
}

void
Accessible::GetBoundsRect(nsRect& aTotalBounds, nsIFrame** aBoundingFrame)
{
  nsIFrame* frame = GetFrame();
  if (frame) {
    *aBoundingFrame = nsLayoutUtils::GetContainingBlockForClientRect(frame);
    aTotalBounds = nsLayoutUtils::
      GetAllInFlowRectsUnion(frame, *aBoundingFrame,
                             nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
  }
}

/* void getBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP
Accessible::GetBounds(PRInt32* aX, PRInt32* aY,
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

NS_IMETHODIMP
Accessible::SetSelected(bool aSelect)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* select = nsAccUtils::GetSelectableContainer(this, State());
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

NS_IMETHODIMP
Accessible::TakeSelection()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE)
      select->ClearSelection();
    return SetSelected(true);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Accessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsIFrame *frame = GetFrame();
  NS_ENSURE_STATE(frame);

  nsIContent* focusContent = mContent;

  // If the accessible focus is managed by container widget then focus the
  // widget and set the accessible as its current item.
  if (!frame->IsFocusable()) {
    Accessible* widget = ContainerWidget();
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
Accessible::GetHTMLName(nsAString& aLabel)
{
  nsAutoString label;

  Accessible* labelAcc = nsnull;
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
Accessible::GetXULName(nsAString& aLabel)
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

    Accessible* labelAcc = nsnull;
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
Accessible::HandleAccEvent(AccEvent* aEvent)
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
Accessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aRole = Role();
  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetAttributes(nsIPersistentProperties **aAttributes)
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
  GroupPos groupPos = GroupPosition();
  nsAccUtils::SetAccGroupAttrs(attributes, groupPos.level,
                               groupPos.setSize, groupPos.posInSet);

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
Accessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
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

GroupPos
Accessible::GroupPosition()
{
  GroupPos groupPos;

  // Get group position from ARIA attributes.
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_level, &groupPos.level);
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_setsize, &groupPos.setSize);
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_posinset, &groupPos.posInSet);

  // If ARIA is missed and the accessible is visible then calculate group
  // position from hierarchy.
  if (State() & states::INVISIBLE)
    return groupPos;

  // Calculate group level if ARIA is missed.
  if (groupPos.level == 0) {
    PRInt32 level = GetLevelInternal();
    if (level != 0)
      groupPos.level = level;
  }

  // Calculate position in group and group size if ARIA is missed.
  if (groupPos.posInSet == 0 || groupPos.setSize == 0) {
    PRInt32 posInSet = 0, setSize = 0;
    GetPositionAndSizeInternal(&posInSet, &setSize);
    if (posInSet != 0 && setSize != 0) {
      if (groupPos.posInSet == 0)
        groupPos.posInSet = posInSet;

      if (groupPos.setSize == 0)
        groupPos.setSize = setSize;
    }
  }

  return groupPos;
}

NS_IMETHODIMP
Accessible::GroupPosition(PRInt32* aGroupLevel,
                          PRInt32* aSimilarItemsInGroup,
                          PRInt32* aPositionInGroup)
{
  NS_ENSURE_ARG_POINTER(aGroupLevel);
  *aGroupLevel = 0;

  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  *aSimilarItemsInGroup = 0;

  NS_ENSURE_ARG_POINTER(aPositionInGroup);
  *aPositionInGroup = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  GroupPos groupPos = GroupPosition();

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetState(PRUint32* aState, PRUint32* aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsAccUtils::To32States(State(), aState, aExtraState);
  return NS_OK;
}

PRUint64
Accessible::State()
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
        Accessible* relTarget = nsnull;
        while ((relTarget = rel.Next())) {
          if (relTarget->Role() == roles::PROPERTYPAGE &&
              FocusMgr()->IsFocusWithin(relTarget))
            state |= states::SELECTED;
        }
      }
    } else if (state & states::FOCUSED) {
      Accessible* container = nsAccUtils::GetSelectableContainer(this, state);
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
    Accessible* widget = ContainerWidget();
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
Accessible::ApplyARIAState(PRUint64* aState) const
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
Accessible::GetValue(nsAString& aValue)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString value;
  Value(value);
  aValue.Assign(value);

  return NS_OK;
}

void
Accessible::Value(nsString& aValue)
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
  if (nsCoreUtils::IsXLink(mContent))
    nsContentUtils::GetLinkLocation(mContent->AsElement(), aValue);
}

// nsIAccessibleValue
NS_IMETHODIMP
Accessible::GetMaximumValue(double *aMaximumValue)
{
  return GetAttrValue(nsGkAtoms::aria_valuemax, aMaximumValue);
}

NS_IMETHODIMP
Accessible::GetMinimumValue(double *aMinimumValue)
{
  return GetAttrValue(nsGkAtoms::aria_valuemin, aMinimumValue);
}

NS_IMETHODIMP
Accessible::GetMinimumIncrement(double *aMinIncrement)
{
  NS_ENSURE_ARG_POINTER(aMinIncrement);
  *aMinIncrement = 0;

  // No mimimum increment in dynamic content spec right now
  return NS_OK_NO_ARIA_VALUE;
}

NS_IMETHODIMP
Accessible::GetCurrentValue(double *aValue)
{
  return GetAttrValue(nsGkAtoms::aria_valuenow, aValue);
}

NS_IMETHODIMP
Accessible::SetCurrentValue(double aValue)
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
NS_IMETHODIMP
Accessible::SetName(const nsAString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Accessible::GetKeyboardShortcut(nsAString& aKeyBinding)
{
  aKeyBinding.Truncate();
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  KeyboardShortcut().ToString(aKeyBinding);
  return NS_OK;
}

role
Accessible::ARIATransformRole(role aRole)
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
      Accessible* targetAcc = nsnull;
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
Accessible::NativeRole()
{
  return nsCoreUtils::IsXLink(mContent) ? roles::LINK : roles::NOTHING;
}

// readonly attribute PRUint8 actionCount
NS_IMETHODIMP
Accessible::GetActionCount(PRUint8* aActionCount)
{
  NS_ENSURE_ARG_POINTER(aActionCount);
  *aActionCount = 0;
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aActionCount = ActionCount();
  return NS_OK;
}

PRUint8
Accessible::ActionCount()
{
  return GetActionRule() == eNoAction ? 0 : 1;
}

/* DOMString getAccActionName (in PRUint8 index); */
NS_IMETHODIMP
Accessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 actionRule = GetActionRule();

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
   {
     PRUint64 state = State();
     if (state & states::CHECKED)
       aName.AssignLiteral("uncheck");
     else if (state & states::MIXED)
       aName.AssignLiteral("cycle");
     else
       aName.AssignLiteral("check");
     return NS_OK;
   }

   case eJumpAction:
     aName.AssignLiteral("jump");
     return NS_OK;

   case eOpenCloseAction:
     if (State() & states::COLLAPSED)
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
     if (State() & states::COLLAPSED)
       aName.AssignLiteral("expand");
     else
       aName.AssignLiteral("collapse");
     return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

// AString getActionDescription(in PRUint8 index)
NS_IMETHODIMP
Accessible::GetActionDescription(PRUint8 aIndex, nsAString& aDescription)
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
Accessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (GetActionRule() != eNoAction) {
    DoCommand();
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/* DOMString getHelp (); */
NS_IMETHODIMP Accessible::GetHelp(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIContent*
Accessible::GetAtomicRegion() const
{
  nsIContent *loopContent = mContent;
  nsAutoString atomic;
  while (loopContent && !loopContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_atomic, atomic))
    loopContent = loopContent->GetParent();

  return atomic.EqualsLiteral("true") ? loopContent : nsnull;
}

// nsIAccessible getRelationByType()
NS_IMETHODIMP
Accessible::GetRelationByType(PRUint32 aType,
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
Accessible::RelationByType(PRUint32 aType)
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
            return Relation(mDoc, formContent);
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
            dom::Element* rootElm = mContent->OwnerDoc()->GetRootElement();
            if (rootElm) {
              nsIContent* possibleButtonEl = rootElm->OwnerDoc()->
                GetAnonymousElementByAttribute(rootElm, nsGkAtoms::_default,
                                               NS_LITERAL_STRING("true"));
              buttonEl = do_QueryInterface(possibleButtonEl);
            }
          }
          nsCOMPtr<nsIContent> relatedContent(do_QueryInterface(buttonEl));
          return Relation(mDoc, relatedContent);
        }
      }
      return Relation();
    }
    case nsIAccessibleRelation::RELATION_MEMBER_OF:
      return Relation(mDoc, GetAtomicRegion());
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
Accessible::GetRelations(nsIArray **aRelations)
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
NS_IMETHODIMP Accessible::ExtendSelection()
{
  // XXX Should be implemented, but not high priority
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void getNativeInterface(out voidPtr aOutAccessible); */
NS_IMETHODIMP Accessible::GetNativeInterface(void **aOutAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
Accessible::DoCommand(nsIContent *aContent, PRUint32 aActionIndex)
{
  nsIContent* content = aContent ? aContent : mContent.get();
  NS_DISPATCH_RUNNABLEMETHOD_ARG2(DispatchClickEvent, this, content,
                                  aActionIndex);
}

void
Accessible::DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex)
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
Accessible::ScrollTo(PRUint32 aHow)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCoreUtils::ScrollTo(mDoc->PresShell(), mContent, aHow);
  return NS_OK;
}

NS_IMETHODIMP
Accessible::ScrollToPoint(PRUint32 aCoordinateType, PRInt32 aX, PRInt32 aY)
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
NS_IMETHODIMP
Accessible::GetSelectedChildren(nsIArray** aSelectedAccessibles)
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
NS_IMETHODIMP
Accessible::RefSelection(PRInt32 aIndex, nsIAccessible** aSelected)
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

NS_IMETHODIMP
Accessible::GetSelectionCount(PRInt32* aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  *aSelectionCount = SelectedItemCount();
  return NS_OK;
}

NS_IMETHODIMP Accessible::AddChildToSelection(PRInt32 aIndex)
{
  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  return aIndex >= 0 && AddItemToSelection(aIndex) ?
    NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP Accessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  return aIndex >=0 && RemoveItemFromSelection(aIndex) ?
    NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP Accessible::IsChildSelected(PRInt32 aIndex, bool *aIsSelected)
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
Accessible::ClearSelection()
{
  if (IsDefunct() || !IsSelect())
    return NS_ERROR_FAILURE;

  UnselectAll();
  return NS_OK;
}

NS_IMETHODIMP
Accessible::SelectAllSelection(bool* aIsMultiSelect)
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
Accessible::GetAnchorCount(PRInt32 *aAnchorCount)
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
Accessible::GetStartIndex(PRInt32 *aStartIndex)
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
Accessible::GetEndIndex(PRInt32 *aEndIndex)
{
  NS_ENSURE_ARG_POINTER(aEndIndex);
  *aEndIndex = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aEndIndex = EndOffset();
  return NS_OK;
}

NS_IMETHODIMP
Accessible::GetURI(PRInt32 aIndex, nsIURI **aURI)
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
Accessible::GetAnchor(PRInt32 aIndex, nsIAccessible** aAccessible)
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
Accessible::GetValid(bool *aValid)
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
Accessible::GetSelected(bool *aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = false;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aSelected = IsLinkSelected();
  return NS_OK;

}

void
Accessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset,
                         PRUint32 aLength)
{
  // Return text representation of non-text accessible within hypertext
  // accessible. Text accessible overrides this method to return enclosed text.
  if (aStartOffset != 0 || aLength == 0)
    return;

  nsIFrame *frame = GetFrame();
  if (!frame)
    return;

  NS_ASSERTION(mParent,
               "Called on accessible unbound from tree. Result can be wrong.");

  if (frame->GetType() == nsGkAtoms::brFrame) {
    aText += kForcedNewLineChar;
  } else if (mParent && nsAccUtils::MustPrune(mParent)) {
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
Accessible::Shutdown()
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
// Accessible public methods

nsresult
Accessible::GetARIAName(nsAString& aName)
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
Accessible::GetNameInternal(nsAString& aName)
{
  if (mContent->IsHTML())
    return GetHTMLName(aName);

  if (mContent->IsXUL())
    return GetXULName(aName);

  return NS_OK;
}

// Accessible protected
void
Accessible::BindToParent(Accessible* aParent, PRUint32 aIndexInParent)
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
Accessible::UnbindFromParent()
{
  mParent = nsnull;
  mIndexInParent = -1;
  mIndexOfEmbeddedChild = -1;
  mGroupInfo = nsnull;
}

void
Accessible::InvalidateChildren()
{
  PRInt32 childCount = mChildren.Length();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = mChildren.ElementAt(childIdx);
    child->UnbindFromParent();
  }

  mEmbeddedObjCollector = nsnull;
  mChildren.Clear();
  SetChildrenFlag(eChildrenUninitialized);
}

bool
Accessible::AppendChild(Accessible* aChild)
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
Accessible::InsertChildAt(PRUint32 aIndex, Accessible* aChild)
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
Accessible::RemoveChild(Accessible* aChild)
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

Accessible*
Accessible::GetChildAt(PRUint32 aIndex)
{
  Accessible* child = mChildren.SafeElementAt(aIndex, nsnull);
  if (!child)
    return nsnull;

#ifdef DEBUG
  Accessible* realParent = child->mParent;
  NS_ASSERTION(!realParent || realParent == this,
               "Two accessibles have the same first child accessible!");
#endif

  return child;
}

PRUint32
Accessible::ChildCount() const
{
  return mChildren.Length();
}

PRInt32
Accessible::GetIndexOf(Accessible* aChild)
{
  return (aChild->mParent != this) ? -1 : aChild->IndexInParent();
}

PRInt32
Accessible::IndexInParent() const
{
  return mIndexInParent;
}

PRUint32
Accessible::EmbeddedChildCount()
{
  if (IsChildrenFlag(eMixedChildren)) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector->Count();
  }

  return ChildCount();
}

Accessible*
Accessible::GetEmbeddedChildAt(PRUint32 aIndex)
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
Accessible::GetIndexOfEmbeddedChild(Accessible* aChild)
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
Accessible::IsLink()
{
  // Every embedded accessible within hypertext accessible implements
  // hyperlink interface.
  return mParent && mParent->IsHyperText() && nsAccUtils::IsEmbeddedObject(this);
}

PRUint32
Accessible::StartOffset()
{
  NS_PRECONDITION(IsLink(), "StartOffset is called not on hyper link!");

  HyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nsnull;
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

PRUint32
Accessible::EndOffset()
{
  NS_PRECONDITION(IsLink(), "EndOffset is called on not hyper link!");

  HyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nsnull;
  return hyperText ? (hyperText->GetChildOffset(this) + 1) : 0;
}

bool
Accessible::IsLinkSelected()
{
  NS_PRECONDITION(IsLink(),
                  "IsLinkSelected() called on something that is not a hyper link!");
  return FocusMgr()->IsFocused(this);
}

PRUint32
Accessible::AnchorCount()
{
  NS_PRECONDITION(IsLink(), "AnchorCount is called on not hyper link!");
  return 1;
}

Accessible*
Accessible::AnchorAt(PRUint32 aAnchorIndex)
{
  NS_PRECONDITION(IsLink(), "GetAnchor is called on not hyper link!");
  return aAnchorIndex == 0 ? this : nsnull;
}

already_AddRefed<nsIURI>
Accessible::AnchorURIAt(PRUint32 aAnchorIndex)
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
Accessible::IsSelect()
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
Accessible::SelectedItems()
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
Accessible::SelectedItemCount()
{
  PRUint32 count = 0;
  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  Accessible* selected = nsnull;
  while ((selected = iter.Next()))
    ++count;

  return count;
}

Accessible*
Accessible::GetSelectedItem(PRUint32 aIndex)
{
  AccIterator iter(this, filters::GetSelected, AccIterator::eTreeNav);
  Accessible* selected = nsnull;

  PRUint32 index = 0;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  return selected;
}

bool
Accessible::IsItemSelected(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  Accessible* selected = nsnull;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  return selected &&
    selected->State() & states::SELECTED;
}

bool
Accessible::AddItemToSelection(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  Accessible* selected = nsnull;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(true);

  return static_cast<bool>(selected);
}

bool
Accessible::RemoveItemFromSelection(PRUint32 aIndex)
{
  PRUint32 index = 0;
  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  Accessible* selected = nsnull;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(false);

  return static_cast<bool>(selected);
}

bool
Accessible::SelectAll()
{
  bool success = false;
  Accessible* selectable = nsnull;

  AccIterator iter(this, filters::GetSelectable, AccIterator::eTreeNav);
  while((selectable = iter.Next())) {
    success = true;
    selectable->SetSelected(true);
  }
  return success;
}

bool
Accessible::UnselectAll()
{
  bool success = false;
  Accessible* selected = nsnull;

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
Accessible::IsWidget() const
{
  return false;
}

bool
Accessible::IsActiveWidget() const
{
  return FocusMgr()->IsFocused(this);
}

bool
Accessible::AreItemsOperable() const
{
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant);
}

Accessible*
Accessible::CurrentItem()
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
      DocAccessible* document = Document();
      if (document)
        return document->GetAccessible(activeDescendantElm);
    }
  }
  return nsnull;
}

void
Accessible::SetCurrentItem(Accessible* aItem)
{
  nsIAtom* id = aItem->GetContent()->GetID();
  if (id) {
    nsAutoString idStr;
    id->ToString(idStr);
    mContent->SetAttr(kNameSpaceID_None,
                      nsGkAtoms::aria_activedescendant, idStr, true);
  }
}

Accessible*
Accessible::ContainerWidget() const
{
  if (HasARIARole() && mContent->HasID()) {
    for (Accessible* parent = Parent(); parent; parent = parent->Parent()) {
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
// Accessible protected methods

void
Accessible::CacheChildren()
{
  DocAccessible* doc = Document();
  NS_ENSURE_TRUE(doc,);

  nsAccTreeWalker walker(doc, mContent, CanHaveAnonChildren());

  Accessible* child = nsnull;
  while ((child = walker.NextChild()) && AppendChild(child));
}

void
Accessible::TestChildCache(Accessible* aCachedChild) const
{
#ifdef DEBUG
  PRInt32 childCount = mChildren.Length();
  if (childCount == 0) {
    NS_ASSERTION(IsChildrenFlag(eChildrenUninitialized),
                 "No children but initialized!");
    return;
  }

  Accessible* child = nsnull;
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    child = mChildren[childIdx];
    if (child == aCachedChild)
      break;
  }

  NS_ASSERTION(child == aCachedChild,
               "[TestChildCache] cached accessible wasn't found. Wrong accessible tree!");  
#endif
}

// Accessible public
bool
Accessible::EnsureChildren()
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

Accessible*
Accessible::GetSiblingAtOffset(PRInt32 aOffset, nsresult* aError) const
{
  if (!mParent || mIndexInParent == -1) {
    if (aError)
      *aError = NS_ERROR_UNEXPECTED;

    return nsnull;
  }

  if (aError &&
      mIndexInParent + aOffset >= static_cast<PRInt32>(mParent->ChildCount())) {
    *aError = NS_OK; // fail peacefully
    return nsnull;
  }

  Accessible* child = mParent->GetChildAt(mIndexInParent + aOffset);
  if (aError && !child)
    *aError = NS_ERROR_UNEXPECTED;

  return child;
}

Accessible* 
Accessible::GetFirstAvailableAccessible(nsINode *aStartNode) const
{
  Accessible* accessible = mDoc->GetAccessible(aStartNode);
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
    Accessible* accessible = mDoc->GetAccessible(node);
    if (accessible)
      return accessible;
  }

  return nsnull;
}

nsresult
Accessible::GetAttrValue(nsIAtom *aProperty, double *aValue)
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
Accessible::GetActionRule()
{
  if (InteractiveState() & states::UNAVAILABLE)
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
Accessible::GetGroupInfo()
{
  if (mGroupInfo)
    return mGroupInfo;

  mGroupInfo = AccGroupInfo::CreateGroupInfo(this);
  return mGroupInfo;
}

void
Accessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet, PRInt32 *aSetSize)
{
  AccGroupInfo* groupInfo = GetGroupInfo();
  if (groupInfo) {
    *aPosInSet = groupInfo->PosInSet();
    *aSetSize = groupInfo->SetSize();
  }
}

PRInt32
Accessible::GetLevelInternal()
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

    Accessible* parent = this;
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
    Accessible* parent = this;
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
      PRUint32 siblingCount = parent->ChildCount();
      for (PRUint32 siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        Accessible* sibling = parent->GetChildAt(siblingIdx);

        Accessible* siblingChild = sibling->LastChild();
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
    stringBundleService->CreateBundle(
      "chrome://global-platform/locale/platformKeys.properties",
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
