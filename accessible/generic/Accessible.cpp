/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"

#include "nsIXBLAccessible.h"

#include "AccCollector.h"
#include "AccGroupInfo.h"
#include "AccIterator.h"
#include "nsAccUtils.h"
#include "nsAccessibilityService.h"
#include "ApplicationAccessible.h"
#include "nsEventShell.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "States.h"
#include "StyleInfo.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "TreeWalker.h"

#include "nsIDOMElement.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMKeyEvent.h"
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

#include "nsDeckFrame.h"
#include "nsLayoutUtils.h"
#include "nsIPresShell.h"
#include "nsIStringBundle.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsView.h"
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

#ifdef DEBUG
#include "nsIDOMCharacterData.h"
#endif

#include "mozilla/Assertions.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventStates.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/TreeWalker.h"

using namespace mozilla;
using namespace mozilla::a11y;


////////////////////////////////////////////////////////////////////////////////
// Accessible: nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION(Accessible,
                         mContent, mParent, mChildren)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Accessible)
  if (aIID.Equals(NS_GET_IID(Accessible)))
    foundInterface = this;
  else
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, Accessible)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Accessible)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(Accessible, LastRelease())

Accessible::Accessible(nsIContent* aContent, DocAccessible* aDoc) :
  mContent(aContent), mDoc(aDoc),
  mParent(nullptr), mIndexInParent(-1), mChildrenFlags(eChildrenUninitialized),
  mStateFlags(0), mContextFlags(0), mType(0), mGenericTypes(0),
  mIndexOfEmbeddedChild(-1), mRoleMapEntry(nullptr)
{
  mBits.groupInfo = nullptr;
}

Accessible::~Accessible()
{
  NS_ASSERTION(!mDoc, "LastRelease was never called!?!");
}

ENameValueFlag
Accessible::Name(nsString& aName)
{
  aName.Truncate();

  if (!HasOwnContent())
    return eNameOK;

  ARIAName(aName);
  if (!aName.IsEmpty())
    return eNameOK;

  nsCOMPtr<nsIXBLAccessible> xblAccessible(do_QueryInterface(mContent));
  if (xblAccessible) {
    xblAccessible->GetAccessibleName(aName);
    if (!aName.IsEmpty())
      return eNameOK;
  }

  ENameValueFlag nameFlag = NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

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
  } else if (mContent->IsSVG()) {
    // If user agents need to choose among multiple ‘desc’ or ‘title’ elements
    // for processing, the user agent shall choose the first one.
    for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
         childElm = childElm->GetNextSibling()) {
      if (childElm->IsSVG(nsGkAtoms::desc)) {
        nsTextEquivUtils::AppendTextEquivFromContent(this, childElm, &aName);
        return eNameFromTooltip;
      }
    }
  }

  if (nameFlag != eNoNameOnPurpose)
    aName.SetIsVoid(true);

  return nameFlag;
}

void
Accessible::Description(nsString& aDescription)
{
  // There are 4 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. It has no DHTML describedby property
  // 3. it doesn't have an accName; or
  // 4. its title attribute already equals to its accName nsAutoString name;

  if (!HasOwnContent() || mContent->IsNodeOfType(nsINode::eTEXT))
    return;

  nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsGkAtoms::aria_describedby,
                           aDescription);

  if (aDescription.IsEmpty()) {
    bool isXUL = mContent->IsXUL();
    if (isXUL) {
      // Try XUL <description control="[id]">description text</description>
      XULDescriptionIterator iter(Document(), mContent);
      Accessible* descr = nullptr;
      while ((descr = iter.Next())) {
        nsTextEquivUtils::AppendTextEquivFromContent(this, descr->GetContent(),
                                                     &aDescription);
      }
    }

    if (aDescription.IsEmpty()) {
      // Keep the Name() method logic.
      if (mContent->IsHTML()) {
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::title, aDescription);
      } else if (mContent->IsXUL()) {
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext, aDescription);
      } else if (mContent->IsSVG()) {
        for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
             childElm = childElm->GetNextSibling()) {
          if (childElm->IsSVG(nsGkAtoms::desc)) {
            nsTextEquivUtils::AppendTextEquivFromContent(this, childElm,
                                                         &aDescription);
            break;
          }
        }
      }
    }
  }

  if (!aDescription.IsEmpty()) {
    aDescription.CompressWhitespace();
    nsAutoString name;
    Name(name);
    // Don't expose a description if it is the same as the name.
    if (aDescription.Equals(name))
      aDescription.Truncate();
  }
}

KeyBinding
Accessible::AccessKey() const
{
  if (!HasOwnContent())
    return KeyBinding();

  uint32_t key = nsCoreUtils::GetAccessKeyFor(mContent);
  if (!key && mContent->IsElement()) {
    Accessible* label = nullptr;

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

  nsCOMPtr<nsIDocShellTreeItem> treeItem(document->GetDocShell());
  if (!treeItem)
    return KeyBinding();

  nsresult rv = NS_ERROR_FAILURE;
  int32_t modifierMask = 0;
  switch (treeItem->ItemType()) {
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

bool
Accessible::CanHaveAnonChildren()
{
  return true;
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

uint64_t
Accessible::VisibilityState()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return states::INVISIBLE;

  // Walk the parent frame chain to see if there's invisible parent or the frame
  // is in background tab.
  if (!frame->StyleVisibility()->IsVisible())
    return states::INVISIBLE;

  nsIFrame* curFrame = frame;
  do {
    nsView* view = curFrame->GetView();
    if (view && view->GetVisibility() == nsViewVisibility_kHide)
      return states::INVISIBLE;

    if (nsLayoutUtils::IsPopup(curFrame))
      return 0;

    // Offscreen state for background tab content and invisible for not selected
    // deck panel.
    nsIFrame* parentFrame = curFrame->GetParent();
    nsDeckFrame* deckFrame = do_QueryFrame(parentFrame);
    if (deckFrame && deckFrame->GetSelectedBox() != curFrame) {
      if (deckFrame->GetContent()->IsXUL() &&
          deckFrame->GetContent()->Tag() == nsGkAtoms::tabpanels)
        return states::OFFSCREEN;

      NS_NOTREACHED("Children of not selected deck panel are not accessible.");
      return states::INVISIBLE;
    }

    // If contained by scrollable frame then check that at least 12 pixels
    // around the object is visible, otherwise the object is offscreen.
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(parentFrame);
    if (scrollableFrame) {
      nsRect scrollPortRect = scrollableFrame->GetScrollPortRect();
      nsRect frameRect = nsLayoutUtils::TransformFrameRectToAncestor(
        frame, frame->GetRectRelativeToSelf(), parentFrame);
      if (!scrollPortRect.Contains(frameRect)) {
        const nscoord kMinPixels = nsPresContext::CSSPixelsToAppUnits(12);
        scrollPortRect.Deflate(kMinPixels, kMinPixels);
        if (!scrollPortRect.Intersects(frameRect))
          return states::OFFSCREEN;
      }
    }

    if (!parentFrame) {
      parentFrame = nsLayoutUtils::GetCrossDocParentFrame(curFrame);
      if (parentFrame && !parentFrame->StyleVisibility()->IsVisible())
        return states::INVISIBLE;
    }

    curFrame = parentFrame;
  } while (curFrame);

  // Zero area rects can occur in the first frame of a multi-frame text flow,
  // in which case the rendered text is not empty and the frame should not be
  // marked invisible.
  // XXX Can we just remove this check? Why do we need to mark empty
  // text invisible?
  if (frame->GetType() == nsGkAtoms::textFrame &&
      !(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
      frame->GetRect().IsEmpty()) {
    nsAutoString renderedText;
    frame->GetRenderedText(&renderedText, nullptr, nullptr, 0, 1);
    if (renderedText.IsEmpty())
      return states::INVISIBLE;
  }

  return 0;
}

uint64_t
Accessible::NativeState()
{
  uint64_t state = 0;

  if (!IsInDocument())
    state |= states::STALE;

  if (HasOwnContent() && mContent->IsElement()) {
    EventStates elementState = mContent->AsElement()->State();

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
  if (frame) {
    if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
      state |= states::FLOATING;

    // XXX we should look at layout for non XUL box frames, but need to decide
    // how that interacts with ARIA.
    if (HasOwnContent() && mContent->IsXUL() && frame->IsBoxFrame()) {
      const nsStyleXUL* xulStyle = frame->StyleXUL();
      if (xulStyle && frame->IsBoxFrame()) {
        // In XUL all boxes are either vertical or horizontal
        if (xulStyle->mBoxOrient == NS_STYLE_BOX_ORIENT_VERTICAL)
          state |= states::VERTICAL;
        else
          state |= states::HORIZONTAL;
      }
    }
  }

  // Check if a XUL element has the popup attribute (an attached popup menu).
  if (HasOwnContent() && mContent->IsXUL() &&
      mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::popup))
    state |= states::HASPOPUP;

  // Bypass the link states specialization for non links.
  if (!mRoleMapEntry || mRoleMapEntry->roleRule == kUseNativeRole ||
      mRoleMapEntry->role == roles::LINK)
    state |= NativeLinkState();

  return state;
}

uint64_t
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

uint64_t
Accessible::NativeLinkState() const
{
  return 0;
}

bool
Accessible::NativelyUnavailable() const
{
  if (mContent->IsHTML())
    return mContent->AsElement()->State().HasState(NS_EVENT_STATE_DISABLED);

  return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

Accessible*
Accessible::FocusedChild()
{
  Accessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && (focus == this || focus->Parent() == this))
    return focus;

  return nullptr;
}

Accessible*
Accessible::ChildAtPoint(int32_t aX, int32_t aY,
                         EWhichChildAtPoint aWhichChild)
{
  // If we can't find the point in a child, we will return the fallback answer:
  // we return |this| if the point is within it, otherwise nullptr.
  Accessible* fallbackAnswer = nullptr;
  nsIntRect rect = Bounds();
  if (aX >= rect.x && aX < rect.x + rect.width &&
      aY >= rect.y && aY < rect.y + rect.height)
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
  NS_ENSURE_TRUE(accDocument, nullptr);

  nsIFrame* rootFrame = accDocument->GetFrame();
  NS_ENSURE_TRUE(rootFrame, nullptr);

  nsIFrame* startFrame = rootFrame;

  // Check whether the point is at popup content.
  nsIWidget* rootWidget = rootFrame->GetView()->GetNearestWidget(nullptr);
  NS_ENSURE_TRUE(rootWidget, nullptr);

  nsIntRect rootRect;
  rootWidget->GetScreenBounds(rootRect);

  WidgetMouseEvent dummyEvent(true, NS_MOUSE_MOVE, rootWidget,
                              WidgetMouseEvent::eSynthesized);
  dummyEvent.refPoint = LayoutDeviceIntPoint(aX - rootRect.x, aY - rootRect.y);

  nsIFrame* popupFrame = nsLayoutUtils::
    GetPopupFrameForEventCoordinates(accDocument->PresContext()->GetRootPresContext(),
                                     &dummyEvent);
  if (popupFrame) {
    // If 'this' accessible is not inside the popup then ignore the popup when
    // searching an accessible at point.
    DocAccessible* popupDoc =
      GetAccService()->GetDocAccessible(popupFrame->GetContent()->OwnerDoc());
    Accessible* popupAcc =
      popupDoc->GetAccessibleOrContainer(popupFrame->GetContent());
    Accessible* popupChild = this;
    while (popupChild && !popupChild->IsDoc() && popupChild != popupAcc)
      popupChild = popupChild->Parent();

    if (popupChild == popupAcc)
      startFrame = popupFrame;
  }

  nsPresContext* presContext = startFrame->PresContext();
  nsRect screenRect = startFrame->GetScreenRectInAppUnits();
    nsPoint offset(presContext->DevPixelsToAppUnits(aX) - screenRect.x,
                   presContext->DevPixelsToAppUnits(aY) - screenRect.y);
  nsIFrame* foundFrame = nsLayoutUtils::GetFrameForPoint(startFrame, offset);

  nsIContent* content = nullptr;
  if (!foundFrame || !(content = foundFrame->GetContent()))
    return fallbackAnswer;

  // Get accessible for the node with the point or the first accessible in
  // the DOM parent chain.
  DocAccessible* contentDocAcc = GetAccService()->
    GetDocAccessible(content->OwnerDoc());

  // contentDocAcc in some circumstances can be nullptr. See bug 729861
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
  uint32_t childCount = accessible->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = accessible->GetChildAt(childIdx);

    nsIntRect childRect = child->Bounds();
    if (aX >= childRect.x && aX < childRect.x + childRect.width &&
        aY >= childRect.y && aY < childRect.y + childRect.height &&
        (child->State() & states::INVISIBLE) == 0) {

      if (aWhichChild == eDeepestChild)
        return child->ChildAtPoint(aX, aY, eDeepestChild);

      return child;
    }
  }

  return accessible;
}

nsRect
Accessible::RelativeBounds(nsIFrame** aBoundingFrame) const
{
  nsIFrame* frame = GetFrame();
  if (frame && mContent) {
    bool* hasHitRegionRect = static_cast<bool*>(mContent->GetProperty(nsGkAtoms::hitregion));

    if (hasHitRegionRect && mContent->IsElement()) {
      // This is for canvas fallback content
      // Find a canvas frame the found hit region is relative to.
      nsIFrame* canvasFrame = frame->GetParent();
      if (canvasFrame) {
        canvasFrame = nsLayoutUtils::GetClosestFrameOfType(canvasFrame, nsGkAtoms::HTMLCanvasFrame);
      }

      // make the canvas the bounding frame
      if (canvasFrame) {
        *aBoundingFrame = canvasFrame;
        dom::HTMLCanvasElement *canvas =
          dom::HTMLCanvasElement::FromContent(canvasFrame->GetContent());

        // get the bounding rect of the hit region
        nsRect bounds;
        if (canvas && canvas->CountContexts() &&
          canvas->GetContextAtIndex(0)->GetHitRegionRect(mContent->AsElement(), bounds)) {
          return bounds;
        }
      }
    }

    *aBoundingFrame = nsLayoutUtils::GetContainingBlockForClientRect(frame);
    return nsLayoutUtils::
      GetAllInFlowRectsUnion(frame, *aBoundingFrame,
                             nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
  }

  return nsRect();
}

nsIntRect
Accessible::Bounds() const
{
  nsIFrame* boundingFrame = nullptr;
  nsRect unionRectTwips = RelativeBounds(&boundingFrame);
  if (!boundingFrame)
    return nsIntRect();

  nsIntRect screenRect;
  nsPresContext* presContext = mDoc->PresContext();
  screenRect.x = presContext->AppUnitsToDevPixels(unionRectTwips.x);
  screenRect.y = presContext->AppUnitsToDevPixels(unionRectTwips.y);
  screenRect.width = presContext->AppUnitsToDevPixels(unionRectTwips.width);
  screenRect.height = presContext->AppUnitsToDevPixels(unionRectTwips.height);

  // We have the union of the rectangle, now we need to put it in absolute
  // screen coords.
  nsIntRect orgRectPixels = boundingFrame->GetScreenRectInAppUnits().
    ToNearestPixels(presContext->AppUnitsPerDevPixel());
  screenRect.x += orgRectPixels.x;
  screenRect.y += orgRectPixels.y;

  return screenRect;
}

void
Accessible::SetSelected(bool aSelect)
{
  if (!HasOwnContent())
    return;

  Accessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE) {
      if (mRoleMapEntry) {
        if (aSelect) {
          mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::aria_selected,
                            NS_LITERAL_STRING("true"), true);
        } else {
          mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::aria_selected, true);
        }
      }
      return;
    }

    if (aSelect)
      TakeFocus();
  }
}

void
Accessible::TakeSelection()
{
  Accessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE)
      select->UnselectAll();
    SetSelected(true);
  }
}

void
Accessible::TakeFocus()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return;

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
}

void
Accessible::XULElmName(DocAccessible* aDocument,
                       nsIContent* aElm, nsString& aName)
{
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

  // CASE #1 (via label attribute) -- great majority of the cases
  nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl = do_QueryInterface(aElm);
  if (labeledEl) {
    labeledEl->GetLabel(aName);
  } else {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> itemEl = do_QueryInterface(aElm);
    if (itemEl) {
      itemEl->GetLabel(aName);
    } else {
      nsCOMPtr<nsIDOMXULSelectControlElement> select = do_QueryInterface(aElm);
      // Use label if this is not a select control element which
      // uses label attribute to indicate which option is selected
      if (!select) {
        nsCOMPtr<nsIDOMXULElement> xulEl(do_QueryInterface(aElm));
        if (xulEl)
          xulEl->GetAttribute(NS_LITERAL_STRING("label"), aName);
      }
    }
  }

  // CASES #2 and #3 ------ label as a child or <label control="id" ... > </label>
  if (aName.IsEmpty()) {
    Accessible* labelAcc = nullptr;
    XULLabelIterator iter(aDocument, aElm);
    while ((labelAcc = iter.Next())) {
      nsCOMPtr<nsIDOMXULLabelElement> xulLabel =
        do_QueryInterface(labelAcc->GetContent());
      // Check if label's value attribute is used
      if (xulLabel && NS_SUCCEEDED(xulLabel->GetValue(aName)) && aName.IsEmpty()) {
        // If no value attribute, a non-empty label must contain
        // children that define its text -- possibly using HTML
        nsTextEquivUtils::
          AppendTextEquivFromContent(labelAcc, labelAcc->GetContent(), &aName);
      }
    }
  }

  aName.CompressWhitespace();
  if (!aName.IsEmpty())
    return;

  // Can get text from title of <toolbaritem> if we're a child of a <toolbaritem>
  nsIContent *bindingParent = aElm->GetBindingParent();
  nsIContent* parent =
    bindingParent? bindingParent->GetParent() : aElm->GetParent();
  while (parent) {
    if (parent->Tag() == nsGkAtoms::toolbaritem &&
        parent->GetAttr(kNameSpaceID_None, nsGkAtoms::title, aName)) {
      aName.CompressWhitespace();
      return;
    }
    parent = parent->GetParent();
  }
}

nsresult
Accessible::HandleAccEvent(AccEvent* aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  NS_ENSURE_TRUE(obsService, NS_ERROR_FAILURE);

  nsCOMPtr<nsISimpleEnumerator> observers;
  obsService->EnumerateObservers(NS_ACCESSIBLE_EVENT_TOPIC,
                                 getter_AddRefs(observers));

  NS_ENSURE_STATE(observers);

  bool hasObservers = false;
  observers->HasMoreElements(&hasObservers);
  if (hasObservers) {
    nsCOMPtr<nsIAccessibleEvent> event = MakeXPCEvent(aEvent);
    return obsService->NotifyObservers(event, NS_ACCESSIBLE_EVENT_TOPIC, nullptr);
  }

  return NS_OK;
}

already_AddRefed<nsIPersistentProperties>
Accessible::Attributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes = NativeAttributes();
  if (!HasOwnContent() || !mContent->IsElement())
    return attributes.forget();

  // 'xml-roles' attribute coming from ARIA.
  nsAutoString xmlRoles, unused;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::role, xmlRoles)) {
    attributes->SetStringProperty(NS_LITERAL_CSTRING("xml-roles"),
                                  xmlRoles, unused);
  }

  // Expose object attributes from ARIA attributes.
  aria::AttrIterator attribIter(mContent);
  nsAutoString name, value;
  while(attribIter.Next(name, value))
    attributes->SetStringProperty(NS_ConvertUTF16toUTF8(name), value, unused);

  if (IsARIAHidden()) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::hidden,
                           NS_LITERAL_STRING("true"));
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

  return attributes.forget();
}

already_AddRefed<nsIPersistentProperties>
Accessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);

  nsAutoString unused;

  // We support values, so expose the string value as well, via the valuetext
  // object attribute. We test for the value interface because we don't want
  // to expose traditional Value() information such as URL's on links and
  // documents, or text in an input.
  if (HasNumericValue()) {
    nsAutoString valuetext;
    Value(valuetext);
    attributes->SetStringProperty(NS_LITERAL_CSTRING("valuetext"), valuetext,
                                  unused);
  }

  // Expose checkable object attribute if the accessible has checkable state
  if (State() & states::CHECKABLE) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::checkable,
                           NS_LITERAL_STRING("true"));
  }

  // Expose 'explicit-name' attribute.
  nsAutoString name;
  if (Name(name) != eNameFromSubtree && !name.IsVoid()) {
    attributes->SetStringProperty(NS_LITERAL_CSTRING("explicit-name"),
                                  NS_LITERAL_STRING("true"), unused);
  }

  // Group attributes (level/setsize/posinset)
  GroupPos groupPos = GroupPosition();
  nsAccUtils::SetAccGroupAttrs(attributes, groupPos.level,
                               groupPos.setSize, groupPos.posInSet);

  // If the accessible doesn't have own content (such as list item bullet or
  // xul tree item) then don't calculate content based attributes.
  if (!HasOwnContent())
    return attributes.forget();

  nsEventShell::GetEventAttributes(GetNode(), attributes);

  // Get container-foo computed live region properties based on the closest
  // container with the live region attribute. Inner nodes override outer nodes
  // within the same document. The inner nodes can be used to override live
  // region behavior on more general outer nodes. However, nodes in outer
  // documents override nodes in inner documents: outer doc author may want to
  // override properties on a widget they used in an iframe.
  nsIContent* startContent = mContent;
  while (startContent) {
    nsIDocument* doc = startContent->GetComposedDoc();
    if (!doc)
      break;

    nsAccUtils::SetLiveContainerAttributes(attributes, startContent,
                                           nsCoreUtils::GetRoleContent(doc));

    // Allow ARIA live region markup from outer documents to override
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = doc->GetDocShell();
    if (!docShellTreeItem)
      break;

    nsCOMPtr<nsIDocShellTreeItem> sameTypeParent;
    docShellTreeItem->GetSameTypeParent(getter_AddRefs(sameTypeParent));
    if (!sameTypeParent || sameTypeParent == docShellTreeItem)
      break;

    nsIDocument* parentDoc = doc->GetParentDocument();
    if (!parentDoc)
      break;

    startContent = parentDoc->FindContentForSubDocument(doc);
  }

  if (!mContent->IsElement())
    return attributes.forget();

  nsAutoString id;
  if (nsCoreUtils::GetID(mContent, id))
    attributes->SetStringProperty(NS_LITERAL_CSTRING("id"), id, unused);

  // Expose class because it may have useful microformat information.
  nsAutoString _class;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, _class))
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::_class, _class);

  // Expose tag.
  nsAutoString tagName;
  mContent->NodeInfo()->GetName(tagName);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::tag, tagName);

  // Expose draggable object attribute.
  nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(mContent);
  if (htmlElement) {
    bool draggable = false;
    htmlElement->GetDraggable(&draggable);
    if (draggable) {
      nsAccUtils::SetAccAttr(attributes, nsGkAtoms::draggable,
                             NS_LITERAL_STRING("true"));
    }
  }

  // Don't calculate CSS-based object attributes when no frame (i.e.
  // the accessible is unattached from the tree).
  if (!mContent->GetPrimaryFrame())
    return attributes.forget();

  // CSS style based object attributes.
  nsAutoString value;
  StyleInfo styleInfo(mContent->AsElement(), mDoc->PresShell());

  // Expose 'display' attribute.
  styleInfo.Display(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::display, value);

  // Expose 'text-align' attribute.
  styleInfo.TextAlign(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::textAlign, value);

  // Expose 'text-indent' attribute.
  styleInfo.TextIndent(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::textIndent, value);

  // Expose 'margin-left' attribute.
  styleInfo.MarginLeft(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::marginLeft, value);

  // Expose 'margin-right' attribute.
  styleInfo.MarginRight(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::marginRight, value);

  // Expose 'margin-top' attribute.
  styleInfo.MarginTop(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::marginTop, value);

  // Expose 'margin-bottom' attribute.
  styleInfo.MarginBottom(value);
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::marginBottom, value);

  return attributes.forget();
}

GroupPos
Accessible::GroupPosition()
{
  GroupPos groupPos;
  if (!HasOwnContent())
    return groupPos;

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
    int32_t level = GetLevelInternal();
    if (level != 0)
      groupPos.level = level;
  }

  // Calculate position in group and group size if ARIA is missed.
  if (groupPos.posInSet == 0 || groupPos.setSize == 0) {
    int32_t posInSet = 0, setSize = 0;
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

uint64_t
Accessible::State()
{
  if (IsDefunct())
    return states::DEFUNCT;

  uint64_t state = NativeState();
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
        Relation rel = RelationByType(RelationType::LABEL_FOR);
        Accessible* relTarget = nullptr;
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

  const uint32_t kExpandCollapseStates = states::COLLAPSED | states::EXPANDED;
  if ((state & kExpandCollapseStates) == kExpandCollapseStates) {
    // Cannot be both expanded and collapsed -- this happens in ARIA expanded
    // combobox because of limitation of ARIAMap.
    // XXX: Perhaps we will be able to make this less hacky if we support
    // extended states in ARIAMap, e.g. derive COLLAPSED from
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

  const nsStyleDisplay* display = frame->StyleDisplay();
  if (display && display->mOpacity == 1.0f &&
      !(state & states::INVISIBLE)) {
    state |= states::OPAQUE1;
  }

  return state;
}

void
Accessible::ApplyARIAState(uint64_t* aState) const
{
  if (!mContent->IsElement())
    return;

  dom::Element* element = mContent->AsElement();

  // Test for universal states first
  *aState |= aria::UniversalStatesFor(element);

  if (mRoleMapEntry) {

    // We only force the readonly bit off if we have a real mapping for the aria
    // role. This preserves the ability for screen readers to use readonly
    // (primarily on the document) as the hint for creating a virtual buffer.
    if (mRoleMapEntry->role != roles::NOTHING)
      *aState &= ~states::READONLY;

    if (mContent->HasID()) {
      // If has a role & ID and aria-activedescendant on the container, assume focusable
      nsIContent *ancestorContent = mContent;
      while ((ancestorContent = ancestorContent->GetParent()) != nullptr) {
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
    while ((ancestorContent = ancestorContent->GetParent()) != nullptr) {
      if (ancestorContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_disabled,
                                       nsGkAtoms::_true, eCaseMatters)) {
          // ancestor has aria-disabled property, this is disabled
        *aState |= states::UNAVAILABLE;
        break;
      }
    }
  }

  // special case: A native button element whose role got transformed by ARIA to a toggle button
  // Also applies to togglable button menus, like in the Dev Tools Web Console.
  if (IsButton() || IsMenuButton())
    aria::MapToState(aria::eARIAPressed, element, aState);

  if (!mRoleMapEntry)
    return;

  *aState |= mRoleMapEntry->state;

  if (aria::MapToState(mRoleMapEntry->attributeMap1, element, aState) &&
      aria::MapToState(mRoleMapEntry->attributeMap2, element, aState) &&
      aria::MapToState(mRoleMapEntry->attributeMap3, element, aState))
    aria::MapToState(mRoleMapEntry->attributeMap4, element, aState);

  // ARIA gridcell inherits editable/readonly states from the grid until it's
  // overridden.
  if ((mRoleMapEntry->Is(nsGkAtoms::gridcell) ||
       mRoleMapEntry->Is(nsGkAtoms::columnheader) ||
       mRoleMapEntry->Is(nsGkAtoms::rowheader)) &&
      !(*aState & (states::READONLY | states::EDITABLE))) {
    const TableCellAccessible* cell = AsTableCell();
    if (cell) {
      TableAccessible* table = cell->Table();
      if (table) {
        Accessible* grid = table->AsAccessible();
        uint64_t gridState = 0;
        grid->ApplyARIAState(&gridState);
        *aState |= (gridState & (states::READONLY | states::EDITABLE));
      }
    }
  }
}

void
Accessible::Value(nsString& aValue)
{
  if (!mRoleMapEntry)
    return;

  if (mRoleMapEntry->valueRule != eNoValue) {
    // aria-valuenow is a number, and aria-valuetext is the optional text
    // equivalent. For the string value, we will try the optional text
    // equivalent first.
    if (!mContent->GetAttr(kNameSpaceID_None,
                           nsGkAtoms::aria_valuetext, aValue)) {
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_valuenow,
                        aValue);
    }
    return;
  }

  // Value of textbox is a textified subtree.
  if (mRoleMapEntry->Is(nsGkAtoms::textbox)) {
    nsTextEquivUtils::GetTextEquivFromSubtree(this, aValue);
    return;
  }

  // Value of combobox is a text of current or selected item.
  if (mRoleMapEntry->Is(nsGkAtoms::combobox)) {
    Accessible* option = CurrentItem();
    if (!option) {
      Accessible* listbox = nullptr;
      ARIAOwnsIterator iter(this);
      while ((listbox = iter.Next()) && !listbox->IsListControl());

      if (!listbox) {
        uint32_t childCount = ChildCount();
        for (uint32_t idx = 0; idx < childCount; idx++) {
          Accessible* child = mChildren.ElementAt(idx);
          if (child->IsListControl())
            listbox = child;
        }
      }

      if (listbox)
        option = listbox->GetSelectedItem(0);
    }

    if (option)
      nsTextEquivUtils::GetTextEquivFromSubtree(option, aValue);
  }
}

double
Accessible::MaxValue() const
{
  return AttrNumericValue(nsGkAtoms::aria_valuemax);
}

double
Accessible::MinValue() const
{
  return AttrNumericValue(nsGkAtoms::aria_valuemin);
}

double
Accessible::Step() const
{
  return UnspecifiedNaN<double>(); // no mimimum increment (step) in ARIA.
}

double
Accessible::CurValue() const
{
  return AttrNumericValue(nsGkAtoms::aria_valuenow);
}

bool
Accessible::SetCurValue(double aValue)
{
  if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return false;

  const uint32_t kValueCannotChange = states::READONLY | states::UNAVAILABLE;
  if (State() & kValueCannotChange)
    return false;

  double checkValue = MinValue();
  if (!IsNaN(checkValue) && aValue < checkValue)
    return false;

  checkValue = MaxValue();
  if (!IsNaN(checkValue) && aValue > checkValue)
    return false;

  nsAutoString strValue;
  strValue.AppendFloat(aValue);

  return NS_SUCCEEDED(
    mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::aria_valuenow, strValue, true));
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
    } else {
      // Listbox is owned by a combobox
      Relation rel = RelationByType(RelationType::NODE_CHILD_OF);
      Accessible* targetAcc = nullptr;
      while ((targetAcc = rel.Next()))
        if (targetAcc->Role() == roles::COMBOBOX)
          return roles::COMBOBOX_LIST;
    }

  } else if (aRole == roles::OPTION) {
    if (mParent && mParent->Role() == roles::COMBOBOX_LIST)
      return roles::COMBOBOX_OPTION;

  } else if (aRole == roles::MENUITEM) {
    // Menuitem has a submenu.
    if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_haspopup,
                              nsGkAtoms::_true, eCaseMatters)) {
      return roles::PARENT_MENUITEM;
    }
  }

  return aRole;
}

role
Accessible::NativeRole()
{
  return roles::NOTHING;
}

uint8_t
Accessible::ActionCount()
{
  return GetActionRule() == eNoAction ? 0 : 1;
}

void
Accessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  if (aIndex != 0)
    return;

  uint32_t actionRule = GetActionRule();

 switch (actionRule) {
   case eActivateAction:
     aName.AssignLiteral("activate");
     return;

   case eClickAction:
     aName.AssignLiteral("click");
     return;

   case ePressAction:
     aName.AssignLiteral("press");
     return;

   case eCheckUncheckAction:
   {
     uint64_t state = State();
     if (state & states::CHECKED)
       aName.AssignLiteral("uncheck");
     else if (state & states::MIXED)
       aName.AssignLiteral("cycle");
     else
       aName.AssignLiteral("check");
     return;
   }

   case eJumpAction:
     aName.AssignLiteral("jump");
     return;

   case eOpenCloseAction:
     if (State() & states::COLLAPSED)
       aName.AssignLiteral("open");
     else
       aName.AssignLiteral("close");
     return;

   case eSelectAction:
     aName.AssignLiteral("select");
     return;

   case eSwitchAction:
     aName.AssignLiteral("switch");
     return;

   case eSortAction:
     aName.AssignLiteral("sort");
     return;

   case eExpandAction:
     if (State() & states::COLLAPSED)
       aName.AssignLiteral("expand");
     else
       aName.AssignLiteral("collapse");
     return;
  }
}

bool
Accessible::DoAction(uint8_t aIndex)
{
  if (aIndex != 0)
    return false;

  if (GetActionRule() != eNoAction) {
    DoCommand();
    return true;
  }

  return false;
}

nsIContent*
Accessible::GetAtomicRegion() const
{
  nsIContent *loopContent = mContent;
  nsAutoString atomic;
  while (loopContent && !loopContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_atomic, atomic))
    loopContent = loopContent->GetParent();

  return atomic.EqualsLiteral("true") ? loopContent : nullptr;
}

Relation
Accessible::RelationByType(RelationType aType)
{
  if (!HasOwnContent())
    return Relation();

  // Relationships are defined on the same content node that the role would be
  // defined on.
  switch (aType) {
    case RelationType::LABELLED_BY: {
      Relation rel(new IDRefsIterator(mDoc, mContent,
                                      nsGkAtoms::aria_labelledby));
      if (mContent->IsHTML()) {
        rel.AppendIter(new HTMLLabelIterator(Document(), this));
      } else if (mContent->IsXUL()) {
        rel.AppendIter(new XULLabelIterator(Document(), mContent));
      }

      return rel;
    }

    case RelationType::LABEL_FOR: {
      Relation rel(new RelatedAccIterator(Document(), mContent,
                                          nsGkAtoms::aria_labelledby));
      if (mContent->Tag() == nsGkAtoms::label && mContent->IsXUL())
        rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::control));

      return rel;
    }

    case RelationType::DESCRIBED_BY: {
      Relation rel(new IDRefsIterator(mDoc, mContent,
                                      nsGkAtoms::aria_describedby));
      if (mContent->IsXUL())
        rel.AppendIter(new XULDescriptionIterator(Document(), mContent));

      return rel;
    }

    case RelationType::DESCRIPTION_FOR: {
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

    case RelationType::NODE_CHILD_OF: {
      Relation rel(new ARIAOwnedByIterator(this));

      // This is an ARIA tree or treegrid that doesn't use owns, so we need to
      // get the parent the hard way.
      if (mRoleMapEntry && (mRoleMapEntry->role == roles::OUTLINEITEM ||
                            mRoleMapEntry->role == roles::LISTITEM ||
                            mRoleMapEntry->role == roles::ROW)) {
        rel.AppendTarget(GetGroupInfo()->ConceptualParent());
      }

      // If accessible is in its own Window, or is the root of a document,
      // then we should provide NODE_CHILD_OF relation so that MSAA clients
      // can easily get to true parent instead of getting to oleacc's
      // ROLE_WINDOW accessible which will prevent us from going up further
      // (because it is system generated and has no idea about the hierarchy
      // above it).
      nsIFrame *frame = GetFrame();
      if (frame) {
        nsView *view = frame->GetViewExternal();
        if (view) {
          nsIScrollableFrame *scrollFrame = do_QueryFrame(frame);
          if (scrollFrame || view->GetWidget() || !frame->GetParent())
            rel.AppendTarget(Parent());
        }
      }

      return rel;
    }

    case RelationType::NODE_PARENT_OF: {
      Relation rel(new ARIAOwnsIterator(this));

      // ARIA tree or treegrid can do the hierarchy by @aria-level, ARIA trees
      // also can be organized by groups.
      if (mRoleMapEntry &&
          (mRoleMapEntry->role == roles::OUTLINEITEM ||
           mRoleMapEntry->role == roles::LISTITEM ||
           mRoleMapEntry->role == roles::ROW ||
           mRoleMapEntry->role == roles::OUTLINE ||
           mRoleMapEntry->role == roles::LIST ||
           mRoleMapEntry->role == roles::TREE_TABLE)) {
        rel.AppendIter(new ItemIterator(this));
      }

      return rel;
    }

    case RelationType::CONTROLLED_BY:
      return Relation(new RelatedAccIterator(Document(), mContent,
                                             nsGkAtoms::aria_controls));

    case RelationType::CONTROLLER_FOR: {
      Relation rel(new IDRefsIterator(mDoc, mContent,
                                      nsGkAtoms::aria_controls));
      rel.AppendIter(new HTMLOutputIterator(Document(), mContent));
      return rel;
    }

    case RelationType::FLOWS_TO:
      return Relation(new IDRefsIterator(mDoc, mContent,
                                         nsGkAtoms::aria_flowto));

    case RelationType::FLOWS_FROM:
      return Relation(new RelatedAccIterator(Document(), mContent,
                                             nsGkAtoms::aria_flowto));

    case RelationType::MEMBER_OF:
          return Relation(mDoc, GetAtomicRegion());

    case RelationType::SUBWINDOW_OF:
    case RelationType::EMBEDS:
    case RelationType::EMBEDDED_BY:
    case RelationType::POPUP_FOR:
    case RelationType::PARENT_WINDOW_OF:
      return Relation();

    case RelationType::DEFAULT_BUTTON: {
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
            uint32_t length;
            possibleDefaultButtons->GetLength(&length);
            nsCOMPtr<nsIDOMNode> possibleButton;
            // Check for button in list of default="true" elements
            for (uint32_t count = 0; count < length && !buttonEl; count ++) {
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

    case RelationType::CONTAINING_DOCUMENT:
      return Relation(mDoc);

    case RelationType::CONTAINING_TAB_PANE: {
      nsCOMPtr<nsIDocShell> docShell =
        nsCoreUtils::GetDocShellFor(GetNode());
      if (docShell) {
        // Walk up the parent chain without crossing the boundary at which item
        // types change, preventing us from walking up out of tab content.
        nsCOMPtr<nsIDocShellTreeItem> root;
        docShell->GetSameTypeRootTreeItem(getter_AddRefs(root));
        if (root) {
          // If the item type is typeContent, we assume we are in browser tab
          // content. Note, this includes content such as about:addons,
          // for consistency.
          if (root->ItemType() == nsIDocShellTreeItem::typeContent) {
            return Relation(nsAccUtils::GetDocAccessibleFor(root));
          }
        }
      }
      return  Relation();
    }

    case RelationType::CONTAINING_APPLICATION:
      return Relation(ApplicationAcc());

    default:
      return Relation();
  }
}

void
Accessible::GetNativeInterface(void** aNativeAccessible)
{
}

void
Accessible::DoCommand(nsIContent *aContent, uint32_t aActionIndex)
{
  class Runnable MOZ_FINAL : public nsRunnable
  {
  public:
    Runnable(Accessible* aAcc, nsIContent* aContent, uint32_t aIdx) :
      mAcc(aAcc), mContent(aContent), mIdx(aIdx) { }

    NS_IMETHOD Run() MOZ_OVERRIDE
    {
      if (mAcc)
        mAcc->DispatchClickEvent(mContent, mIdx);

      return NS_OK;
    }

    void Revoke()
    {
      mAcc = nullptr;
      mContent = nullptr;
    }

  private:
    nsRefPtr<Accessible> mAcc;
    nsCOMPtr<nsIContent> mContent;
    uint32_t mIdx;
  };

  nsIContent* content = aContent ? aContent : mContent.get();
  nsCOMPtr<nsIRunnable> runnable = new Runnable(this, content, aActionIndex);
  NS_DispatchToMainThread(runnable);
}

void
Accessible::DispatchClickEvent(nsIContent *aContent, uint32_t aActionIndex)
{
  if (IsDefunct())
    return;

  nsCOMPtr<nsIPresShell> presShell = mDoc->PresShell();

  // Scroll into view.
  presShell->ScrollContentIntoView(aContent,
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::SCROLL_OVERFLOW_HIDDEN);

  nsWeakFrame frame = aContent->GetPrimaryFrame();
  if (!frame)
    return;

  // Compute x and y coordinates.
  nsPoint point;
  nsCOMPtr<nsIWidget> widget = frame->GetNearestWidget(point);
  if (!widget)
    return;

  nsSize size = frame->GetSize();

  nsRefPtr<nsPresContext> presContext = presShell->GetPresContext();
  int32_t x = presContext->AppUnitsToDevPixels(point.x + size.width / 2);
  int32_t y = presContext->AppUnitsToDevPixels(point.y + size.height / 2);

  // Simulate a touch interaction by dispatching touch events with mouse events.
  nsCoreUtils::DispatchTouchEvent(NS_TOUCH_START, x, y, aContent, frame, presShell, widget);
  nsCoreUtils::DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, x, y, aContent, frame, presShell, widget);
  nsCoreUtils::DispatchTouchEvent(NS_TOUCH_END, x, y, aContent, frame, presShell, widget);
  nsCoreUtils::DispatchMouseEvent(NS_MOUSE_BUTTON_UP, x, y, aContent, frame, presShell, widget);
}

void
Accessible::ScrollToPoint(uint32_t aCoordinateType, int32_t aX, int32_t aY)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return;

  nsIntPoint coords =
    nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType, this);

  nsIFrame* parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent()))
    nsCoreUtils::ScrollFrameToPoint(parentFrame, frame, coords);
}

void
Accessible::AppendTextTo(nsAString& aText, uint32_t aStartOffset,
                         uint32_t aLength)
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

void
Accessible::Shutdown()
{
  // Mark the accessible as defunct, invalidate the child count and pointers to
  // other accessibles, also make sure none of its children point to this parent
  mStateFlags |= eIsDefunct;

  InvalidateChildren();
  if (mParent)
    mParent->RemoveChild(this);

  mContent = nullptr;
  mDoc = nullptr;
  if (SelectionMgr() && SelectionMgr()->AccessibleWithCaret(nullptr) == this)
    SelectionMgr()->ResetCaretOffset();
}

// Accessible protected
void
Accessible::ARIAName(nsString& aName)
{
  // aria-labelledby now takes precedence over aria-label
  nsresult rv = nsTextEquivUtils::
    GetTextEquivFromIDRefs(this, nsGkAtoms::aria_labelledby, aName);
  if (NS_SUCCEEDED(rv)) {
    aName.CompressWhitespace();
  }

  if (aName.IsEmpty() &&
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_label, aName)) {
    aName.CompressWhitespace();
  }
}

// Accessible protected
ENameValueFlag
Accessible::NativeName(nsString& aName)
{
  if (mContent->IsHTML()) {
    Accessible* label = nullptr;
    HTMLLabelIterator iter(Document(), this);
    while ((label = iter.Next())) {
      nsTextEquivUtils::AppendTextEquivFromContent(this, label->GetContent(),
                                                   &aName);
      aName.CompressWhitespace();
    }

    if (!aName.IsEmpty())
      return eNameOK;

    nsTextEquivUtils::GetNameFromSubtree(this, aName);
    return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
  }

  if (mContent->IsXUL()) {
    XULElmName(mDoc, mContent, aName);
    if (!aName.IsEmpty())
      return eNameOK;

    nsTextEquivUtils::GetNameFromSubtree(this, aName);
    return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
  }

  if (mContent->IsSVG()) {
    // If user agents need to choose among multiple ‘desc’ or ‘title’ elements
    // for processing, the user agent shall choose the first one.
    for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
         childElm = childElm->GetNextSibling()) {
      if (childElm->IsSVG(nsGkAtoms::title)) {
        nsTextEquivUtils::AppendTextEquivFromContent(this, childElm, &aName);
        return eNameOK;
      }
    }
  }

  return eNameOK;
}

// Accessible protected
void
Accessible::BindToParent(Accessible* aParent, uint32_t aIndexInParent)
{
  NS_PRECONDITION(aParent, "This method isn't used to set null parent!");

  if (mParent) {
    if (mParent != aParent) {
      NS_ERROR("Adopting child!");
      mParent->RemoveChild(this);
      mParent->InvalidateChildrenGroupInfo();
    } else {
      NS_ERROR("Binding to the same parent!");
      return;
    }
  }

  mParent = aParent;
  mIndexInParent = aIndexInParent;

#ifdef DEBUG
  AssertInMutatingSubtree();
#endif

  // Note: this is currently only used for richlistitems and their children.
  if (mParent->HasNameDependentParent() || mParent->IsXULListItem())
    mContextFlags |= eHasNameDependentParent;
  else
    mContextFlags &= ~eHasNameDependentParent;

  if (mParent->IsARIAHidden() || aria::HasDefinedARIAHidden(mContent))
    SetARIAHidden(true);
}

// Accessible protected
void
Accessible::UnbindFromParent()
{
#ifdef DEBUG
  AssertInMutatingSubtree();
#endif
  mParent = nullptr;
  mIndexInParent = -1;
  mIndexOfEmbeddedChild = -1;
  if (IsProxy())
    MOZ_CRASH("this should never be called on proxy wrappers");

  delete mBits.groupInfo;
  mBits.groupInfo = nullptr;
  mContextFlags &= ~eHasNameDependentParent;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible public methods

RootAccessible*
Accessible::RootAccessible() const
{
  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(GetNode());
  NS_ASSERTION(docShell, "No docshell for mContent");
  if (!docShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  docShell->GetRootTreeItem(getter_AddRefs(root));
  NS_ASSERTION(root, "No root content tree item");
  if (!root) {
    return nullptr;
  }

  DocAccessible* docAcc = nsAccUtils::GetDocAccessibleFor(root);
  return docAcc ? docAcc->AsRoot() : nullptr;
}

nsIFrame*
Accessible::GetFrame() const
{
  return mContent ? mContent->GetPrimaryFrame() : nullptr;
}

nsINode*
Accessible::GetNode() const
{
  return mContent;
}

void
Accessible::Language(nsAString& aLanguage)
{
  aLanguage.Truncate();

  if (!mDoc)
    return;

  nsCoreUtils::GetLanguageFor(mContent, nullptr, aLanguage);
  if (aLanguage.IsEmpty()) { // Nothing found, so use document's language
    mDoc->DocumentNode()->GetHeaderData(nsGkAtoms::headerContentLanguage,
                                        aLanguage);
  }
}

void
Accessible::InvalidateChildren()
{
  int32_t childCount = mChildren.Length();
  for (int32_t childIdx = 0; childIdx < childCount; childIdx++) {
    mChildren.ElementAt(childIdx)->UnbindFromParent();
  }

  mEmbeddedObjCollector = nullptr;
  mChildren.Clear();
  SetChildrenFlag(eChildrenUninitialized);
}

bool
Accessible::InsertChildAt(uint32_t aIndex, Accessible* aChild)
{
  if (!aChild)
    return false;

  if (aIndex == mChildren.Length()) {
    if (!mChildren.AppendElement(aChild))
      return false;

  } else {
    if (!mChildren.InsertElementAt(aIndex, aChild))
      return false;

    for (uint32_t idx = aIndex + 1; idx < mChildren.Length(); idx++) {
      NS_ASSERTION(static_cast<uint32_t>(mChildren[idx]->mIndexInParent) == idx - 1,
                   "Accessible child index doesn't match");
      mChildren[idx]->mIndexInParent = idx;
    }

    mEmbeddedObjCollector = nullptr;
  }

  if (!nsAccUtils::IsEmbeddedObject(aChild))
    SetChildrenFlag(eMixedChildren);

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

  uint32_t index = static_cast<uint32_t>(aChild->mIndexInParent);
  if (index >= mChildren.Length() || mChildren[index] != aChild) {
    NS_ERROR("Child is bound to parent but parent hasn't this child at its index!");
    aChild->UnbindFromParent();
    return false;
  }

  for (uint32_t idx = index + 1; idx < mChildren.Length(); idx++) {
    NS_ASSERTION(static_cast<uint32_t>(mChildren[idx]->mIndexInParent) == idx,
                 "Accessible child index doesn't match");
    mChildren[idx]->mIndexInParent = idx - 1;
  }

  aChild->UnbindFromParent();
  mChildren.RemoveElementAt(index);
  mEmbeddedObjCollector = nullptr;

  return true;
}

Accessible*
Accessible::GetChildAt(uint32_t aIndex) const
{
  Accessible* child = mChildren.SafeElementAt(aIndex, nullptr);
  if (!child)
    return nullptr;

#ifdef DEBUG
  Accessible* realParent = child->mParent;
  NS_ASSERTION(!realParent || realParent == this,
               "Two accessibles have the same first child accessible!");
#endif

  return child;
}

uint32_t
Accessible::ChildCount() const
{
  return mChildren.Length();
}

int32_t
Accessible::IndexInParent() const
{
  return mIndexInParent;
}

uint32_t
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
Accessible::GetEmbeddedChildAt(uint32_t aIndex)
{
  if (IsChildrenFlag(eMixedChildren)) {
    if (!mEmbeddedObjCollector)
      mEmbeddedObjCollector = new EmbeddedObjCollector(this);
    return mEmbeddedObjCollector ?
      mEmbeddedObjCollector->GetAccessibleAt(aIndex) : nullptr;
  }

  return GetChildAt(aIndex);
}

int32_t
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

uint32_t
Accessible::StartOffset()
{
  NS_PRECONDITION(IsLink(), "StartOffset is called not on hyper link!");

  HyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nullptr;
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

uint32_t
Accessible::EndOffset()
{
  NS_PRECONDITION(IsLink(), "EndOffset is called on not hyper link!");

  HyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nullptr;
  return hyperText ? (hyperText->GetChildOffset(this) + 1) : 0;
}

uint32_t
Accessible::AnchorCount()
{
  NS_PRECONDITION(IsLink(), "AnchorCount is called on not hyper link!");
  return 1;
}

Accessible*
Accessible::AnchorAt(uint32_t aAnchorIndex)
{
  NS_PRECONDITION(IsLink(), "GetAnchor is called on not hyper link!");
  return aAnchorIndex == 0 ? this : nullptr;
}

already_AddRefed<nsIURI>
Accessible::AnchorURIAt(uint32_t aAnchorIndex)
{
  NS_PRECONDITION(IsLink(), "AnchorURIAt is called on not hyper link!");
  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// SelectAccessible

void
Accessible::SelectedItems(nsTArray<Accessible*>* aItems)
{
  AccIterator iter(this, filters::GetSelected);
  Accessible* selected = nullptr;
  while ((selected = iter.Next()))
    aItems->AppendElement(selected);
}

uint32_t
Accessible::SelectedItemCount()
{
  uint32_t count = 0;
  AccIterator iter(this, filters::GetSelected);
  Accessible* selected = nullptr;
  while ((selected = iter.Next()))
    ++count;

  return count;
}

Accessible*
Accessible::GetSelectedItem(uint32_t aIndex)
{
  AccIterator iter(this, filters::GetSelected);
  Accessible* selected = nullptr;

  uint32_t index = 0;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  return selected;
}

bool
Accessible::IsItemSelected(uint32_t aIndex)
{
  uint32_t index = 0;
  AccIterator iter(this, filters::GetSelectable);
  Accessible* selected = nullptr;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  return selected &&
    selected->State() & states::SELECTED;
}

bool
Accessible::AddItemToSelection(uint32_t aIndex)
{
  uint32_t index = 0;
  AccIterator iter(this, filters::GetSelectable);
  Accessible* selected = nullptr;
  while ((selected = iter.Next()) && index < aIndex)
    index++;

  if (selected)
    selected->SetSelected(true);

  return static_cast<bool>(selected);
}

bool
Accessible::RemoveItemFromSelection(uint32_t aIndex)
{
  uint32_t index = 0;
  AccIterator iter(this, filters::GetSelectable);
  Accessible* selected = nullptr;
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
  Accessible* selectable = nullptr;

  AccIterator iter(this, filters::GetSelectable);
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
  Accessible* selected = nullptr;

  AccIterator iter(this, filters::GetSelected);
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
  if (FocusMgr()->HasDOMFocus(mContent))
    return true;

  // If text entry of combobox widget has a focus then the combobox widget is
  // active.
  if (mRoleMapEntry && mRoleMapEntry->Is(nsGkAtoms::combobox)) {
    uint32_t childCount = ChildCount();
    for (uint32_t idx = 0; idx < childCount; idx++) {
      Accessible* child = mChildren.ElementAt(idx);
      if (child->Role() == roles::ENTRY)
        return FocusMgr()->HasDOMFocus(child->GetContent());
    }
  }

  return false;
}

bool
Accessible::AreItemsOperable() const
{
  return HasOwnContent() &&
    mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant);
}

Accessible*
Accessible::CurrentItem()
{
  // Check for aria-activedescendant, which changes which element has focus.
  // For activedescendant, the ARIA spec does not require that the user agent
  // checks whether pointed node is actually a DOM descendant of the element
  // with the aria-activedescendant attribute.
  nsAutoString id;
  if (HasOwnContent() &&
      mContent->GetAttr(kNameSpaceID_None,
                        nsGkAtoms::aria_activedescendant, id)) {
    nsIDocument* DOMDoc = mContent->OwnerDoc();
    dom::Element* activeDescendantElm = DOMDoc->GetElementById(id);
    if (activeDescendantElm) {
      DocAccessible* document = Document();
      if (document)
        return document->GetAccessible(activeDescendantElm);
    }
  }
  return nullptr;
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
      if (parent->IsDoc())
        break;
    }
  }
  return nullptr;
}

void
Accessible::SetARIAHidden(bool aIsDefined)
{
  if (aIsDefined)
    mContextFlags |= eARIAHidden;
  else
    mContextFlags &= ~eARIAHidden;

  uint32_t length = mChildren.Length();
  for (uint32_t i = 0; i < length; i++) {
    mChildren[i]->SetARIAHidden(aIsDefined);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Accessible protected methods

void
Accessible::LastRelease()
{
  // First cleanup if needed...
  if (mDoc) {
    Shutdown();
    NS_ASSERTION(!mDoc,
                 "A Shutdown() impl forgot to call its parent's Shutdown?");
  }
  // ... then die.
  delete this;
}

void
Accessible::CacheChildren()
{
  DocAccessible* doc = Document();
  NS_ENSURE_TRUE_VOID(doc);

  TreeWalker walker(this, mContent);

  Accessible* child = nullptr;
  while ((child = walker.NextChild()) && AppendChild(child));
}

void
Accessible::TestChildCache(Accessible* aCachedChild) const
{
#ifdef DEBUG
  int32_t childCount = mChildren.Length();
  if (childCount == 0) {
    NS_ASSERTION(IsChildrenFlag(eChildrenUninitialized),
                 "No children but initialized!");
    return;
  }

  Accessible* child = nullptr;
  for (int32_t childIdx = 0; childIdx < childCount; childIdx++) {
    child = mChildren[childIdx];
    if (child == aCachedChild)
      break;
  }

  NS_ASSERTION(child == aCachedChild,
               "[TestChildCache] cached accessible wasn't found. Wrong accessible tree!");
#endif
}

void
Accessible::EnsureChildren()
{
  NS_ASSERTION(!IsDefunct(), "Caching children for defunct accessible!");

  if (!IsChildrenFlag(eChildrenUninitialized))
    return;

  // State is embedded children until text leaf accessible is appended.
  SetChildrenFlag(eEmbeddedChildren); // Prevent reentry
  CacheChildren();
}

Accessible*
Accessible::GetSiblingAtOffset(int32_t aOffset, nsresult* aError) const
{
  if (!mParent || mIndexInParent == -1) {
    if (aError)
      *aError = NS_ERROR_UNEXPECTED;

    return nullptr;
  }

  if (aError &&
      mIndexInParent + aOffset >= static_cast<int32_t>(mParent->ChildCount())) {
    *aError = NS_OK; // fail peacefully
    return nullptr;
  }

  Accessible* child = mParent->GetChildAt(mIndexInParent + aOffset);
  if (aError && !child)
    *aError = NS_ERROR_UNEXPECTED;

  return child;
}

double
Accessible::AttrNumericValue(nsIAtom* aAttr) const
{
  if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return UnspecifiedNaN<double>();

  nsAutoString attrValue;
  if (!mContent->GetAttr(kNameSpaceID_None, aAttr, attrValue))
    return UnspecifiedNaN<double>();

  nsresult error = NS_OK;
  double value = attrValue.ToDouble(&error);
  return NS_FAILED(error) ? UnspecifiedNaN<double>() : value;
}

uint32_t
Accessible::GetActionRule() const
{
  if (!HasOwnContent() || (InteractiveState() & states::UNAVAILABLE))
    return eNoAction;

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
  if (IsProxy())
    MOZ_CRASH("This should never be called on proxy wrappers");

  if (mBits.groupInfo){
    if (HasDirtyGroupInfo()) {
      mBits.groupInfo->Update();
      SetDirtyGroupInfo(false);
    }

    return mBits.groupInfo;
  }

  mBits.groupInfo = AccGroupInfo::CreateGroupInfo(this);
  return mBits.groupInfo;
}

void
Accessible::InvalidateChildrenGroupInfo()
{
  uint32_t length = mChildren.Length();
  for (uint32_t i = 0; i < length; i++) {
    Accessible* child = mChildren[i];
    child->SetDirtyGroupInfo(true);
  }
}

void
Accessible::GetPositionAndSizeInternal(int32_t *aPosInSet, int32_t *aSetSize)
{
  AccGroupInfo* groupInfo = GetGroupInfo();
  if (groupInfo) {
    *aPosInSet = groupInfo->PosInSet();
    *aSetSize = groupInfo->SetSize();
  }
}

int32_t
Accessible::GetLevelInternal()
{
  int32_t level = nsAccUtils::GetDefaultLevel(this);

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
    // Expose 'level' attribute on nested lists. We support two hierarchies:
    // a) list -> listitem -> list -> listitem (nested list is a last child
    //   of listitem of the parent list);
    // b) list -> listitem -> group -> listitem (nested listitems are contained
    //   by group that is a last child of the parent listitem).

    // Calculate 'level' attribute based on number of parent listitems.
    level = 0;
    Accessible* parent = this;
    while ((parent = parent->Parent())) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::LISTITEM)
        ++ level;
      else if (parentRole != roles::LIST && parentRole != roles::GROUPING)
        break;
    }

    if (level == 0) {
      // If this listitem is on top of nested lists then expose 'level'
      // attribute.
      parent = Parent();
      uint32_t siblingCount = parent->ChildCount();
      for (uint32_t siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        Accessible* sibling = parent->GetChildAt(siblingIdx);

        Accessible* siblingChild = sibling->LastChild();
        if (siblingChild) {
          roles::Role lastChildRole = siblingChild->Role();
          if (lastChildRole == roles::LIST || lastChildRole == roles::GROUPING)
            return 1;
        }
      }
    } else {
      ++ level; // level is 1-index based
    }
  }

  return level;
}

void
Accessible::StaticAsserts() const
{
  static_assert(eLastChildrenFlag <= (1 << kChildrenFlagsBits) - 1,
                "Accessible::mChildrenFlags was oversized by eLastChildrenFlag!");
  static_assert(eLastStateFlag <= (1 << kStateFlagsBits) - 1,
                "Accessible::mStateFlags was oversized by eLastStateFlag!");
  static_assert(eLastAccType <= (1 << kTypeBits) - 1,
                "Accessible::mType was oversized by eLastAccType!");
  static_assert(eLastContextFlag <= (1 << kContextFlagsBits) - 1,
                "Accessible::mContextFlags was oversized by eLastContextFlag!");
  static_assert(eLastAccGenericType <= (1 << kGenericTypesBits) - 1,
                "Accessible::mGenericType was oversized by eLastAccGenericType!");
}

void
Accessible::AssertInMutatingSubtree() const
{
  if (IsDoc() || IsApplication())
    return;

  const Accessible *acc = this;
  while (!acc->IsDoc() && !(acc->mStateFlags & eSubtreeMutating)) {
    acc = acc->Parent();
    if (!acc)
      return;
  }

  MOZ_ASSERT(acc->mStateFlags & eSubtreeMutating);
}

////////////////////////////////////////////////////////////////////////////////
// KeyBinding class

// static
uint32_t
KeyBinding::AccelModifier()
{
  switch (WidgetInputEvent::AccelModifier()) {
    case MODIFIER_ALT:
      return kAlt;
    case MODIFIER_CONTROL:
      return kControl;
    case MODIFIER_META:
      return kMeta;
    case MODIFIER_OS:
      return kOS;
    default:
      MOZ_CRASH("Handle the new result of WidgetInputEvent::AccelModifier()");
      return 0;
  }
}

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
  keyStringBundle->GetStringFromName(MOZ_UTF16("MODIFIER_SEPARATOR"),
                                     getter_Copies(separator));

  nsAutoString modifierName;
  if (mModifierMask & kControl) {
    keyStringBundle->GetStringFromName(MOZ_UTF16("VK_CONTROL"),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kAlt) {
    keyStringBundle->GetStringFromName(MOZ_UTF16("VK_ALT"),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kShift) {
    keyStringBundle->GetStringFromName(MOZ_UTF16("VK_SHIFT"),
                                       getter_Copies(modifierName));

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kMeta) {
    keyStringBundle->GetStringFromName(MOZ_UTF16("VK_META"),
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
    aValue.AppendLiteral("<Control>");

  if (mModifierMask & kAlt)
    aValue.AppendLiteral("<Alt>");

  if (mModifierMask & kShift)
    aValue.AppendLiteral("<Shift>");

  if (mModifierMask & kMeta)
      aValue.AppendLiteral("<Meta>");

  aValue.Append(mKey);
}

