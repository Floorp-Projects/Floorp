/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalAccessible-inl.h"

#include "EmbeddedObjCollector.h"
#include "AccAttributes.h"
#include "AccGroupInfo.h"
#include "AccIterator.h"
#include "nsAccUtils.h"
#include "nsAccessibilityService.h"
#include "ApplicationAccessible.h"
#include "nsAccessiblePivot.h"
#include "nsGenericHTMLElement.h"
#include "NotificationController.h"
#include "nsEventShell.h"
#include "nsTextEquivUtils.h"
#include "DocAccessibleChild.h"
#include "EventTree.h"
#include "Pivot.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "States.h"
#include "StyleInfo.h"
#include "TextRange.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "TreeWalker.h"
#include "HTMLElementAccessibles.h"

#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsINodeList.h"
#include "nsPIDOMWindow.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLAnchorElement.h"
#include "nsIContent.h"
#include "nsIForm.h"
#include "nsIFormControl.h"

#include "nsDeckFrame.h"
#include "nsLayoutUtils.h"
#include "nsIStringBundle.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsView.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScrollableFrame.h"
#include "nsFocusManager.h"

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "prdtoa.h"
#include "nsAtom.h"
#include "nsIURI.h"
#include "nsArrayUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsAttrName.h"

#include "mozilla/Assertions.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Components.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventStates.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/Unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/TreeWalker.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/MutationEventBinding.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible: nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION_CLASS(LocalAccessible)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(LocalAccessible)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(LocalAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContent, mDoc)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LocalAccessible)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(LocalAccessible)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, LocalAccessible)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(LocalAccessible)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(LocalAccessible, LastRelease())

LocalAccessible::LocalAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : Accessible(),
      mContent(aContent),
      mDoc(aDoc),
      mParent(nullptr),
      mIndexInParent(-1),
      mStateFlags(0),
      mContextFlags(0),
      mReorderEventTarget(false),
      mShowEventTarget(false),
      mHideEventTarget(false) {
  mBits.groupInfo = nullptr;
  mIndexOfEmbeddedChild = -1;
}

LocalAccessible::~LocalAccessible() {
  NS_ASSERTION(!mDoc, "LastRelease was never called!?!");
}

ENameValueFlag LocalAccessible::Name(nsString& aName) const {
  aName.Truncate();

  if (!HasOwnContent()) return eNameOK;

  ARIAName(aName);
  if (!aName.IsEmpty()) return eNameOK;

  ENameValueFlag nameFlag = NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  // In the end get the name from tooltip.
  if (mContent->IsHTMLElement()) {
    if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::title,
                                       aName)) {
      aName.CompressWhitespace();
      return eNameFromTooltip;
    }
  } else if (mContent->IsXULElement()) {
    if (mContent->AsElement()->GetAttr(kNameSpaceID_None,
                                       nsGkAtoms::tooltiptext, aName)) {
      aName.CompressWhitespace();
      return eNameFromTooltip;
    }
  } else if (mContent->IsSVGElement()) {
    // If user agents need to choose among multiple 'desc' or 'title'
    // elements for processing, the user agent shall choose the first one.
    for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
         childElm = childElm->GetNextSibling()) {
      if (childElm->IsSVGElement(nsGkAtoms::desc)) {
        nsTextEquivUtils::AppendTextEquivFromContent(this, childElm, &aName);
        return eNameFromTooltip;
      }
    }
  }

  if (nameFlag != eNoNameOnPurpose) aName.SetIsVoid(true);

  return nameFlag;
}

void LocalAccessible::Description(nsString& aDescription) {
  // There are 4 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. It has no ARIA describedby or description property
  // 3. it doesn't have an accName; or
  // 4. its title attribute already equals to its accName nsAutoString name;

  if (!HasOwnContent() || mContent->IsText()) return;

  ARIADescription(aDescription);

  if (aDescription.IsEmpty()) {
    NativeDescription(aDescription);

    if (aDescription.IsEmpty()) {
      // Keep the Name() method logic.
      if (mContent->IsHTMLElement()) {
        mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::title,
                                       aDescription);
      } else if (mContent->IsXULElement()) {
        mContent->AsElement()->GetAttr(kNameSpaceID_None,
                                       nsGkAtoms::tooltiptext, aDescription);
      } else if (mContent->IsSVGElement()) {
        for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
             childElm = childElm->GetNextSibling()) {
          if (childElm->IsSVGElement(nsGkAtoms::desc)) {
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
    if (aDescription.Equals(name)) aDescription.Truncate();
  }
}

KeyBinding LocalAccessible::AccessKey() const {
  if (!HasOwnContent()) return KeyBinding();

  uint32_t key = nsCoreUtils::GetAccessKeyFor(mContent);
  if (!key && mContent->IsElement()) {
    LocalAccessible* label = nullptr;

    // Copy access key from label node.
    if (mContent->IsHTMLElement()) {
      // Unless it is labeled via an ancestor <label>, in which case that would
      // be redundant.
      HTMLLabelIterator iter(Document(), this,
                             HTMLLabelIterator::eSkipAncestorLabel);
      label = iter.Next();
    }
    if (!label) {
      XULLabelIterator iter(Document(), mContent);
      label = iter.Next();
    }

    if (label) key = nsCoreUtils::GetAccessKeyFor(label->GetContent());
  }

  if (!key) return KeyBinding();

  // Get modifier mask. Use ui.key.generalAccessKey (unless it is -1).
  switch (StaticPrefs::ui_key_generalAccessKey()) {
    case -1:
      break;
    case dom::KeyboardEvent_Binding::DOM_VK_SHIFT:
      return KeyBinding(key, KeyBinding::kShift);
    case dom::KeyboardEvent_Binding::DOM_VK_CONTROL:
      return KeyBinding(key, KeyBinding::kControl);
    case dom::KeyboardEvent_Binding::DOM_VK_ALT:
      return KeyBinding(key, KeyBinding::kAlt);
    case dom::KeyboardEvent_Binding::DOM_VK_META:
      return KeyBinding(key, KeyBinding::kMeta);
    default:
      return KeyBinding();
  }

  // Determine the access modifier used in this context.
  dom::Document* document = mContent->GetComposedDoc();
  if (!document) return KeyBinding();

  nsCOMPtr<nsIDocShellTreeItem> treeItem(document->GetDocShell());
  if (!treeItem) return KeyBinding();

  nsresult rv = NS_ERROR_FAILURE;
  int32_t modifierMask = 0;
  switch (treeItem->ItemType()) {
    case nsIDocShellTreeItem::typeChrome:
      modifierMask = StaticPrefs::ui_key_chromeAccess();
      rv = NS_OK;
      break;
    case nsIDocShellTreeItem::typeContent:
      modifierMask = StaticPrefs::ui_key_contentAccess();
      rv = NS_OK;
      break;
  }

  return NS_SUCCEEDED(rv) ? KeyBinding(key, modifierMask) : KeyBinding();
}

KeyBinding LocalAccessible::KeyboardShortcut() const { return KeyBinding(); }

void LocalAccessible::TranslateString(const nsString& aKey,
                                      nsAString& aStringOut) {
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      components::StringBundle::Service();
  if (!stringBundleService) return;

  nsCOMPtr<nsIStringBundle> stringBundle;
  stringBundleService->CreateBundle(
      "chrome://global-platform/locale/accessible.properties",
      getter_AddRefs(stringBundle));
  if (!stringBundle) return;

  nsAutoString xsValue;
  nsresult rv = stringBundle->GetStringFromName(
      NS_ConvertUTF16toUTF8(aKey).get(), xsValue);
  if (NS_SUCCEEDED(rv)) aStringOut.Assign(xsValue);
}

uint64_t LocalAccessible::VisibilityState() const {
  nsIFrame* frame = GetFrame();
  if (!frame) {
    // Element having display:contents is considered visible semantically,
    // despite it doesn't have a visually visible box.
    if (nsCoreUtils::IsDisplayContents(mContent)) {
      return states::OFFSCREEN;
    }
    return states::INVISIBLE;
  }

  if (!frame->StyleVisibility()->IsVisible()) return states::INVISIBLE;

  // It's invisible if the presshell is hidden by a visibility:hidden element in
  // an ancestor document.
  if (frame->PresShell()->IsUnderHiddenEmbedderElement()) {
    return states::INVISIBLE;
  }

  // Offscreen state if the document's visibility state is not visible.
  if (Document()->IsHidden()) return states::OFFSCREEN;

  // Walk the parent frame chain to see if the frame is in background tab or
  // scrolled out.
  nsIFrame* curFrame = frame;
  do {
    nsView* view = curFrame->GetView();
    if (view && view->GetVisibility() == nsViewVisibility_kHide) {
      return states::INVISIBLE;
    }

    if (nsLayoutUtils::IsPopup(curFrame)) return 0;

    // Offscreen state for background tab content and invisible for not selected
    // deck panel.
    nsIFrame* parentFrame = curFrame->GetParent();
    nsDeckFrame* deckFrame = do_QueryFrame(parentFrame);
    if (deckFrame && deckFrame->GetSelectedBox() != curFrame) {
      if (deckFrame->GetContent()->IsXULElement(nsGkAtoms::tabpanels)) {
        return states::OFFSCREEN;
      }

      MOZ_ASSERT_UNREACHABLE(
          "Children of not selected deck panel are not accessible.");
      return states::INVISIBLE;
    }

    // If contained by scrollable frame then check that at least 12 pixels
    // around the object is visible, otherwise the object is offscreen.
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(parentFrame);
    const nscoord kMinPixels = nsPresContext::CSSPixelsToAppUnits(12);
    if (scrollableFrame) {
      nsRect scrollPortRect = scrollableFrame->GetScrollPortRect();
      nsRect frameRect = nsLayoutUtils::TransformFrameRectToAncestor(
          frame, frame->GetRectRelativeToSelf(), parentFrame);
      if (!scrollPortRect.Contains(frameRect)) {
        scrollPortRect.Deflate(kMinPixels, kMinPixels);
        if (!scrollPortRect.Intersects(frameRect)) return states::OFFSCREEN;
      }
    }

    if (!parentFrame) {
      parentFrame = nsLayoutUtils::GetCrossDocParentFrameInProcess(curFrame);
      // Even if we couldn't find the parent frame, it might mean we are in an
      // out-of-process iframe, try to see if |frame| is scrolled out in an
      // scrollable frame in a cross-process ancestor document.
      if (!parentFrame &&
          nsLayoutUtils::FrameIsMostlyScrolledOutOfViewInCrossProcess(
              frame, kMinPixels)) {
        return states::OFFSCREEN;
      }
    }

    curFrame = parentFrame;
  } while (curFrame);

  // Zero area rects can occur in the first frame of a multi-frame text flow,
  // in which case the rendered text is not empty and the frame should not be
  // marked invisible.
  // XXX Can we just remove this check? Why do we need to mark empty
  // text invisible?
  if (frame->IsTextFrame() && !(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
      frame->GetRect().IsEmpty()) {
    nsIFrame::RenderedText text = frame->GetRenderedText(
        0, UINT32_MAX, nsIFrame::TextOffsetType::OffsetsInContentText,
        nsIFrame::TrailingWhitespace::DontTrim);
    if (text.mString.IsEmpty()) {
      return states::INVISIBLE;
    }
  }

  return 0;
}

uint64_t LocalAccessible::NativeState() const {
  uint64_t state = 0;

  if (!IsInDocument()) state |= states::STALE;

  if (HasOwnContent() && mContent->IsElement()) {
    EventStates elementState = mContent->AsElement()->State();

    if (elementState.HasState(NS_EVENT_STATE_INVALID)) state |= states::INVALID;

    if (elementState.HasState(NS_EVENT_STATE_REQUIRED)) {
      state |= states::REQUIRED;
    }

    state |= NativeInteractiveState();
    if (FocusMgr()->IsFocused(this)) state |= states::FOCUSED;
  }

  // Gather states::INVISIBLE and states::OFFSCREEN flags for this object.
  state |= VisibilityState();

  nsIFrame* frame = GetFrame();
  if (frame) {
    if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) state |= states::FLOATING;

    // XXX we should look at layout for non XUL box frames, but need to decide
    // how that interacts with ARIA.
    if (HasOwnContent() && mContent->IsXULElement() && frame->IsXULBoxFrame()) {
      const nsStyleXUL* xulStyle = frame->StyleXUL();
      if (xulStyle && frame->IsXULBoxFrame()) {
        // In XUL all boxes are either vertical or horizontal
        if (xulStyle->mBoxOrient == StyleBoxOrient::Vertical) {
          state |= states::VERTICAL;
        } else {
          state |= states::HORIZONTAL;
        }
      }
    }
  }

  // Check if a XUL element has the popup attribute (an attached popup menu).
  if (HasOwnContent() && mContent->IsXULElement() &&
      mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::popup)) {
    state |= states::HASPOPUP;
  }

  // Bypass the link states specialization for non links.
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->roleRule == kUseNativeRole ||
      roleMapEntry->role == roles::LINK) {
    state |= NativeLinkState();
  }

  return state;
}

uint64_t LocalAccessible::NativeInteractiveState() const {
  if (!mContent->IsElement()) return 0;

  if (NativelyUnavailable()) return states::UNAVAILABLE;

  nsIFrame* frame = GetFrame();
  if (frame && frame->IsFocusable()) return states::FOCUSABLE;

  return 0;
}

uint64_t LocalAccessible::NativeLinkState() const { return 0; }

bool LocalAccessible::NativelyUnavailable() const {
  if (mContent->IsHTMLElement()) return mContent->AsElement()->IsDisabled();

  return mContent->IsElement() && mContent->AsElement()->AttrValueIs(
                                      kNameSpaceID_None, nsGkAtoms::disabled,
                                      nsGkAtoms::_true, eCaseMatters);
}

LocalAccessible* LocalAccessible::FocusedChild() {
  LocalAccessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && (focus == this || focus->LocalParent() == this)) {
    return focus;
  }

  return nullptr;
}

Accessible* LocalAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                                          EWhichChildAtPoint aWhichChild) {
  Accessible* child = LocalChildAtPoint(aX, aY, aWhichChild);
  if (aWhichChild != EWhichChildAtPoint::DirectChild && child &&
      child->IsOuterDoc()) {
    child = child->ChildAtPoint(aX, aY, aWhichChild);
  }

  return child;
}

LocalAccessible* LocalAccessible::LocalChildAtPoint(
    int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) {
  // If we can't find the point in a child, we will return the fallback answer:
  // we return |this| if the point is within it, otherwise nullptr.
  LocalAccessible* fallbackAnswer = nullptr;
  nsIntRect rect = Bounds();
  if (rect.Contains(aX, aY)) fallbackAnswer = this;

  if (nsAccUtils::MustPrune(this)) {  // Do not dig any further
    return fallbackAnswer;
  }

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

  LayoutDeviceIntRect rootRect = rootWidget->GetScreenBounds();

  WidgetMouseEvent dummyEvent(true, eMouseMove, rootWidget,
                              WidgetMouseEvent::eSynthesized);
  dummyEvent.mRefPoint =
      LayoutDeviceIntPoint(aX - rootRect.X(), aY - rootRect.Y());

  nsIFrame* popupFrame = nsLayoutUtils::GetPopupFrameForEventCoordinates(
      accDocument->PresContext()->GetRootPresContext(), &dummyEvent);
  if (popupFrame) {
    // If 'this' accessible is not inside the popup then ignore the popup when
    // searching an accessible at point.
    DocAccessible* popupDoc =
        GetAccService()->GetDocAccessible(popupFrame->GetContent()->OwnerDoc());
    LocalAccessible* popupAcc =
        popupDoc->GetAccessibleOrContainer(popupFrame->GetContent());
    LocalAccessible* popupChild = this;
    while (popupChild && !popupChild->IsDoc() && popupChild != popupAcc) {
      popupChild = popupChild->LocalParent();
    }

    if (popupChild == popupAcc) startFrame = popupFrame;
  }

  nsPresContext* presContext = startFrame->PresContext();
  nsRect screenRect = startFrame->GetScreenRectInAppUnits();
  nsPoint offset(presContext->DevPixelsToAppUnits(aX) - screenRect.X(),
                 presContext->DevPixelsToAppUnits(aY) - screenRect.Y());

  nsIFrame* foundFrame = nsLayoutUtils::GetFrameForPoint(
      RelativeTo{startFrame, ViewportType::Visual}, offset);

  nsIContent* content = nullptr;
  if (!foundFrame || !(content = foundFrame->GetContent())) {
    return fallbackAnswer;
  }

  // Get accessible for the node with the point or the first accessible in
  // the DOM parent chain.
  DocAccessible* contentDocAcc =
      GetAccService()->GetDocAccessible(content->OwnerDoc());

  // contentDocAcc in some circumstances can be nullptr. See bug 729861
  NS_ASSERTION(contentDocAcc, "could not get the document accessible");
  if (!contentDocAcc) return fallbackAnswer;

  LocalAccessible* accessible =
      contentDocAcc->GetAccessibleOrContainer(content);
  if (!accessible) return fallbackAnswer;

  // Hurray! We have an accessible for the frame that layout gave us.
  // Since DOM node of obtained accessible may be out of flow then we should
  // ensure obtained accessible is a child of this accessible.
  LocalAccessible* child = accessible;
  while (child != this) {
    LocalAccessible* parent = child->LocalParent();
    if (!parent) {
      // Reached the top of the hierarchy. These bounds were inside an
      // accessible that is not a descendant of this one.
      return fallbackAnswer;
    }

    // If we landed on a legitimate child of |this|, and we want the direct
    // child, return it here.
    if (parent == this && aWhichChild == EWhichChildAtPoint::DirectChild) {
      return child;
    }

    child = parent;
  }

  // Manually walk through accessible children and see if the are within this
  // point. Skip offscreen or invisible accessibles. This takes care of cases
  // where layout won't walk into things for us, such as image map areas and
  // sub documents (XXX: subdocuments should be handled by methods of
  // OuterDocAccessibles).
  uint32_t childCount = accessible->ChildCount();
  if (childCount == 1 && accessible->IsOuterDoc() &&
      accessible->FirstChild()->IsRemote()) {
    // No local children.
    return accessible;
  }
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    LocalAccessible* child = accessible->LocalChildAt(childIdx);

    nsIntRect childRect = child->Bounds();
    if (childRect.Contains(aX, aY) &&
        (child->State() & states::INVISIBLE) == 0) {
      if (aWhichChild == EWhichChildAtPoint::DeepestChild) {
        return child->LocalChildAtPoint(aX, aY,
                                        EWhichChildAtPoint::DeepestChild);
      }

      return child;
    }
  }

  return accessible;
}

nsRect LocalAccessible::RelativeBounds(nsIFrame** aBoundingFrame) const {
  nsIFrame* frame = GetFrame();
  if (frame && mContent) {
    if (mContent->GetProperty(nsGkAtoms::hitregion) && mContent->IsElement()) {
      // This is for canvas fallback content
      // Find a canvas frame the found hit region is relative to.
      nsIFrame* canvasFrame = frame->GetParent();
      if (canvasFrame) {
        canvasFrame = nsLayoutUtils::GetClosestFrameOfType(
            canvasFrame, LayoutFrameType::HTMLCanvas);
      }

      // make the canvas the bounding frame
      if (canvasFrame) {
        *aBoundingFrame = canvasFrame;
        if (auto* canvas =
                dom::HTMLCanvasElement::FromNode(canvasFrame->GetContent())) {
          if (auto* context = canvas->GetCurrentContext()) {
            nsRect bounds;
            if (context->GetHitRegionRect(mContent->AsElement(), bounds)) {
              return bounds;
            }
          }
        }
      }
    }

    *aBoundingFrame = nsLayoutUtils::GetContainingBlockForClientRect(frame);
    nsRect unionRect = nsLayoutUtils::GetAllInFlowRectsUnion(
        frame, *aBoundingFrame, nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);

    if (unionRect.IsEmpty()) {
      // If we end up with a 0x0 rect from above (or one with negative
      // height/width) we should try using the ink overflow rect instead. If we
      // use this rect, our relative bounds will match the bounds of what
      // appears visually. We do this because some web authors (icloud.com for
      // example) employ things like 0x0 buttons with visual overflow. Without
      // this, such frames aren't navigable by screen readers.
      nsRect overflow = frame->InkOverflowRectRelativeToSelf();
      nsLayoutUtils::TransformRect(frame, *aBoundingFrame, overflow);
      return overflow;
    }

    return unionRect;
  }

  return nsRect();
}

nsRect LocalAccessible::BoundsInAppUnits() const {
  nsIFrame* boundingFrame = nullptr;
  nsRect unionRectTwips = RelativeBounds(&boundingFrame);
  if (!boundingFrame) {
    return nsRect();
  }

  PresShell* presShell = mDoc->PresContext()->PresShell();

  // We need to inverse translate with the offset of the edge of the visual
  // viewport from top edge of the layout viewport.
  nsPoint viewportOffset = presShell->GetVisualViewportOffset() -
                           presShell->GetLayoutViewportOffset();
  unionRectTwips.MoveBy(-viewportOffset);

  // We need to take into account a non-1 resolution set on the presshell.
  // This happens with async pinch zooming. Here we scale the bounds before
  // adding the screen-relative offset.
  unionRectTwips.ScaleRoundOut(presShell->GetResolution());
  // We have the union of the rectangle, now we need to put it in absolute
  // screen coords.
  nsRect orgRectPixels = boundingFrame->GetScreenRectInAppUnits();
  unionRectTwips.MoveBy(orgRectPixels.X(), orgRectPixels.Y());

  return unionRectTwips;
}

nsIntRect LocalAccessible::Bounds() const {
  return BoundsInAppUnits().ToNearestPixels(
      mDoc->PresContext()->AppUnitsPerDevPixel());
}

nsIntRect LocalAccessible::BoundsInCSSPixels() const {
  return BoundsInAppUnits().ToNearestPixels(AppUnitsPerCSSPixel());
}

void LocalAccessible::SetSelected(bool aSelect) {
  if (!HasOwnContent()) return;

  LocalAccessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE) {
      if (mContent->IsElement() && ARIARoleMap()) {
        if (aSelect) {
          mContent->AsElement()->SetAttr(
              kNameSpaceID_None, nsGkAtoms::aria_selected, u"true"_ns, true);
        } else {
          mContent->AsElement()->UnsetAttr(kNameSpaceID_None,
                                           nsGkAtoms::aria_selected, true);
        }
      }
      return;
    }

    if (aSelect) TakeFocus();
  }
}

void LocalAccessible::TakeSelection() {
  LocalAccessible* select = nsAccUtils::GetSelectableContainer(this, State());
  if (select) {
    if (select->State() & states::MULTISELECTABLE) select->UnselectAll();
    SetSelected(true);
  }
}

void LocalAccessible::TakeFocus() const {
  nsIFrame* frame = GetFrame();
  if (!frame) return;

  nsIContent* focusContent = mContent;

  // If the accessible focus is managed by container widget then focus the
  // widget and set the accessible as its current item.
  if (!frame->IsFocusable()) {
    LocalAccessible* widget = ContainerWidget();
    if (widget && widget->AreItemsOperable()) {
      nsIContent* widgetElm = widget->GetContent();
      nsIFrame* widgetFrame = widgetElm->GetPrimaryFrame();
      if (widgetFrame && widgetFrame->IsFocusable()) {
        focusContent = widgetElm;
        widget->SetCurrentItem(this);
      }
    }
  }

  if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
    dom::AutoHandlingUserInputStatePusher inputStatePusher(true);
    // XXXbz: Can we actually have a non-element content here?
    RefPtr<dom::Element> element = dom::Element::FromNodeOrNull(focusContent);
    fm->SetFocus(element, 0);
  }
}

void LocalAccessible::NameFromAssociatedXULLabel(DocAccessible* aDocument,
                                                 nsIContent* aElm,
                                                 nsString& aName) {
  LocalAccessible* label = nullptr;
  XULLabelIterator iter(aDocument, aElm);
  while ((label = iter.Next())) {
    // Check if label's value attribute is used
    label->Elm()->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aName);
    if (aName.IsEmpty()) {
      // If no value attribute, a non-empty label must contain
      // children that define its text -- possibly using HTML
      nsTextEquivUtils::AppendTextEquivFromContent(label, label->Elm(), &aName);
    }
  }
  aName.CompressWhitespace();
}

void LocalAccessible::XULElmName(DocAccessible* aDocument, nsIContent* aElm,
                                 nsString& aName) {
  /**
   * 3 main cases for XUL Controls to be labeled
   *   1 - control contains label="foo"
   *   2 - non-child label contains control="controlID"
   *        - label has either value="foo" or children
   *   3 - name from subtree; e.g. a child label element
   * Cases 1 and 2 are handled here.
   * Case 3 is handled by GetNameFromSubtree called in NativeName.
   * Once a label is found, the search is discontinued, so a control
   *  that has a label attribute as well as having a label external to
   *  the control that uses the control="controlID" syntax will use
   *  the label attribute for its Name.
   */

  // CASE #1 (via label attribute) -- great majority of the cases
  // Only do this if this is not a select control element, which uses label
  // attribute to indicate, which option is selected.
  nsCOMPtr<nsIDOMXULSelectControlElement> select =
      aElm->AsElement()->AsXULSelectControl();
  if (!select) {
    aElm->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
  }

  // CASE #2 -- label as <label control="id" ... ></label>
  if (aName.IsEmpty()) {
    NameFromAssociatedXULLabel(aDocument, aElm, aName);
  }

  aName.CompressWhitespace();
}

nsresult LocalAccessible::HandleAccEvent(AccEvent* aEvent) {
  NS_ENSURE_ARG_POINTER(aEvent);

  if (profiler_thread_is_being_profiled()) {
    nsAutoCString strEventType;
    GetAccService()->GetStringEventType(aEvent->GetEventType(), strEventType);
    nsAutoCString strMarker;
    strMarker.AppendLiteral("A11y Event - ");
    strMarker.Append(strEventType);
    PROFILER_MARKER_UNTYPED(strMarker, OTHER);
  }

  if (IPCAccessibilityActive() && Document()) {
    DocAccessibleChild* ipcDoc = mDoc->IPCDoc();
    // If ipcDoc is null, we can't fire the event to the client. We shouldn't
    // have fired the event in the first place, since this makes events
    // inconsistent for local and remote documents. To avoid this, don't call
    // nsEventShell::FireEvent on a DocAccessible for which
    // HasLoadState(eTreeConstructed) is false.
    MOZ_ASSERT(ipcDoc);
    if (ipcDoc) {
      uint64_t id = aEvent->GetAccessible()->IsDoc()
                        ? 0
                        : reinterpret_cast<uintptr_t>(
                              aEvent->GetAccessible()->UniqueID());

      switch (aEvent->GetEventType()) {
        case nsIAccessibleEvent::EVENT_SHOW:
          ipcDoc->ShowEvent(downcast_accEvent(aEvent));
          break;

        case nsIAccessibleEvent::EVENT_HIDE:
          ipcDoc->SendHideEvent(id, aEvent->IsFromUserInput());
          break;

        case nsIAccessibleEvent::EVENT_REORDER:
          // reorder events on the application acc aren't necessary to tell the
          // parent about new top level documents.
          if (!aEvent->GetAccessible()->IsApplication()) {
            ipcDoc->SendEvent(id, aEvent->GetEventType());
          }
          break;
        case nsIAccessibleEvent::EVENT_STATE_CHANGE: {
          AccStateChangeEvent* event = downcast_accEvent(aEvent);
          ipcDoc->SendStateChangeEvent(id, event->GetState(),
                                       event->IsStateEnabled());
          break;
        }
        case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED: {
          AccCaretMoveEvent* event = downcast_accEvent(aEvent);
          ipcDoc->SendCaretMoveEvent(id, event->GetCaretOffset(),
                                     event->IsSelectionCollapsed());
          break;
        }
        case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
        case nsIAccessibleEvent::EVENT_TEXT_REMOVED: {
          AccTextChangeEvent* event = downcast_accEvent(aEvent);
          const nsString& text = event->ModifiedText();
#if defined(XP_WIN)
          // On Windows, events for live region updates containing embedded
          // objects require us to dispatch synchronous events.
          bool sync = text.Contains(L'\xfffc') &&
                      nsAccUtils::IsARIALive(aEvent->GetAccessible());
#endif
          ipcDoc->SendTextChangeEvent(id, text, event->GetStartOffset(),
                                      event->GetLength(),
                                      event->IsTextInserted(),
                                      event->IsFromUserInput()
#if defined(XP_WIN)
                                      // This parameter only exists on Windows.
                                      ,
                                      sync
#endif
          );
          break;
        }
        case nsIAccessibleEvent::EVENT_SELECTION:
        case nsIAccessibleEvent::EVENT_SELECTION_ADD:
        case nsIAccessibleEvent::EVENT_SELECTION_REMOVE: {
          AccSelChangeEvent* selEvent = downcast_accEvent(aEvent);
          uint64_t widgetID =
              selEvent->Widget()->IsDoc()
                  ? 0
                  : reinterpret_cast<uintptr_t>(selEvent->Widget()->UniqueID());
          ipcDoc->SendSelectionEvent(id, widgetID, aEvent->GetEventType());
          break;
        }
        case nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED: {
          AccVCChangeEvent* vcEvent = downcast_accEvent(aEvent);
          LocalAccessible* position = vcEvent->NewAccessible();
          LocalAccessible* oldPosition = vcEvent->OldAccessible();
          ipcDoc->SendVirtualCursorChangeEvent(
              id,
              oldPosition ? reinterpret_cast<uintptr_t>(oldPosition->UniqueID())
                          : 0,
              vcEvent->OldStartOffset(), vcEvent->OldEndOffset(),
              position ? reinterpret_cast<uintptr_t>(position->UniqueID()) : 0,
              vcEvent->NewStartOffset(), vcEvent->NewEndOffset(),
              vcEvent->Reason(), vcEvent->BoundaryType(),
              vcEvent->IsFromUserInput());
          break;
        }
#if defined(XP_WIN)
        case nsIAccessibleEvent::EVENT_FOCUS: {
          ipcDoc->SendFocusEvent(id);
          break;
        }
#endif
        case nsIAccessibleEvent::EVENT_SCROLLING_END:
        case nsIAccessibleEvent::EVENT_SCROLLING: {
          AccScrollingEvent* scrollingEvent = downcast_accEvent(aEvent);
          ipcDoc->SendScrollingEvent(
              id, aEvent->GetEventType(), scrollingEvent->ScrollX(),
              scrollingEvent->ScrollY(), scrollingEvent->MaxScrollX(),
              scrollingEvent->MaxScrollY());
          break;
        }
#if !defined(XP_WIN)
        case nsIAccessibleEvent::EVENT_ANNOUNCEMENT: {
          AccAnnouncementEvent* announcementEvent = downcast_accEvent(aEvent);
          ipcDoc->SendAnnouncementEvent(id, announcementEvent->Announcement(),
                                        announcementEvent->Priority());
          break;
        }
        case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED: {
          AccTextSelChangeEvent* textSelChangeEvent = downcast_accEvent(aEvent);
          AutoTArray<TextRange, 1> ranges;
          textSelChangeEvent->SelectionRanges(&ranges);
          nsTArray<TextRangeData> textRangeData(ranges.Length());
          for (size_t i = 0; i < ranges.Length(); i++) {
            const TextRange& range = ranges.ElementAt(i);
            LocalAccessible* start = range.StartContainer();
            LocalAccessible* end = range.EndContainer();
            textRangeData.AppendElement(TextRangeData(
                start->IsDoc() && start->AsDoc()->IPCDoc()
                    ? 0
                    : reinterpret_cast<uint64_t>(start->UniqueID()),
                end->IsDoc() && end->AsDoc()->IPCDoc()
                    ? 0
                    : reinterpret_cast<uint64_t>(end->UniqueID()),
                range.StartOffset(), range.EndOffset()));
          }
          ipcDoc->SendTextSelectionChangeEvent(id, textRangeData);
          break;
        }
        case nsIAccessibleEvent::EVENT_NAME_CHANGE: {
          if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
            nsAutoString name;
            int32_t nameFlag = Name(name);
            RefPtr<AccAttributes> fields = new AccAttributes();
            fields->SetAttribute(nsGkAtoms::explicit_name, nameFlag);
            fields->SetAttribute(nsGkAtoms::name, name);
            nsTArray<CacheData> data;
            data.AppendElement(CacheData(
                IsDoc() ? 0 : reinterpret_cast<uint64_t>(UniqueID()), fields));
            ipcDoc->SendCache(1, data, true);
          }
          ipcDoc->SendEvent(id, aEvent->GetEventType());
          break;
        }
#endif
        default:
          ipcDoc->SendEvent(id, aEvent->GetEventType());
      }
    }
  }

  if (nsCoreUtils::AccEventObserversExist()) {
    nsCoreUtils::DispatchAccEvent(MakeXPCEvent(aEvent));
  }

  return NS_OK;
}

already_AddRefed<AccAttributes> LocalAccessible::Attributes() {
  RefPtr<AccAttributes> attributes = NativeAttributes();
  if (!HasOwnContent() || !mContent->IsElement()) return attributes.forget();

  // 'xml-roles' attribute coming from ARIA.
  nsAutoString xmlRoles;
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::role,
                                     xmlRoles)) {
    attributes->SetAttribute(nsGkAtoms::xmlroles, xmlRoles);
  } else if (nsAtom* landmark = LandmarkRole()) {
    // 'xml-roles' attribute for landmark.
    attributes->SetAttribute(nsGkAtoms::xmlroles, landmark);
  }

  // Expose object attributes from ARIA attributes.
  aria::AttrIterator attribIter(mContent);
  while (attribIter.Next()) {
    nsAutoString value;
    attribIter.AttrValue(value);
    attributes->SetAttribute(attribIter.AttrName(), value);
  }

  // If there is no aria-live attribute then expose default value of 'live'
  // object attribute used for ARIA role of this accessible.
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry) {
    if (roleMapEntry->Is(nsGkAtoms::searchbox)) {
      attributes->SetAttribute(nsGkAtoms::textInputType, u"search"_ns);
    }

    if (!attributes->HasAttribute(nsGkAtoms::aria_live)) {
      nsAutoString live;
      if (nsAccUtils::GetLiveAttrValue(roleMapEntry->liveAttRule, live)) {
        attributes->SetAttribute(nsGkAtoms::aria_live, live);
      }
    }
  }

  return attributes.forget();
}

already_AddRefed<AccAttributes> LocalAccessible::NativeAttributes() {
  RefPtr<AccAttributes> attributes = new AccAttributes();

  // We support values, so expose the string value as well, via the valuetext
  // object attribute. We test for the value interface because we don't want
  // to expose traditional Value() information such as URL's on links and
  // documents, or text in an input.
  if (HasNumericValue()) {
    nsAutoString valuetext;
    Value(valuetext);
    attributes->SetAttribute(nsGkAtoms::aria_valuetext, valuetext);
  }

  // Expose checkable object attribute if the accessible has checkable state
  if (State() & states::CHECKABLE) {
    attributes->SetAttribute(nsGkAtoms::checkable, true);
  }

  // Expose 'explicit-name' attribute.
  nsAutoString name;
  if (Name(name) != eNameFromSubtree && !name.IsVoid()) {
    attributes->SetAttribute(nsGkAtoms::explicit_name, true);
  }

  // Group attributes (level/setsize/posinset)
  GroupPos groupPos = GroupPosition();
  nsAccUtils::SetAccGroupAttrs(attributes, groupPos.level, groupPos.setSize,
                               groupPos.posInSet);

  bool hierarchical = false;
  uint32_t itemCount = AccGroupInfo::TotalItemCount(this, &hierarchical);
  if (itemCount) {
    attributes->SetAttribute(nsGkAtoms::child_item_count,
                             static_cast<int32_t>(itemCount));
  }

  if (hierarchical) {
    attributes->SetAttribute(nsGkAtoms::tree, true);
  }

  // If the accessible doesn't have own content (such as list item bullet or
  // xul tree item) then don't calculate content based attributes.
  if (!HasOwnContent()) return attributes.forget();

  nsEventShell::GetEventAttributes(GetNode(), attributes);

  // Get container-foo computed live region properties based on the closest
  // container with the live region attribute. Inner nodes override outer nodes
  // within the same document. The inner nodes can be used to override live
  // region behavior on more general outer nodes.
  nsAccUtils::SetLiveContainerAttributes(attributes, mContent);

  if (!mContent->IsElement()) return attributes.forget();

  nsAutoString id;
  if (nsCoreUtils::GetID(mContent, id)) {
    attributes->SetAttribute(nsGkAtoms::id, id);
  }

  // Expose class because it may have useful microformat information.
  nsAutoString _class;
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                     _class)) {
    attributes->SetAttribute(nsGkAtoms::_class, _class);
  }

  // Expose tag.
  attributes->SetAttribute(nsGkAtoms::tag, mContent->NodeInfo()->NameAtom());

  // Expose draggable object attribute.
  if (auto htmlElement = nsGenericHTMLElement::FromNode(mContent)) {
    if (htmlElement->Draggable()) {
      attributes->SetAttribute(nsGkAtoms::draggable, true);
    }
  }

  // Don't calculate CSS-based object attributes when no frame (i.e.
  // the accessible is unattached from the tree).
  if (!mContent->GetPrimaryFrame()) return attributes.forget();

  // CSS style based object attributes.
  nsAutoString value;
  StyleInfo styleInfo(mContent->AsElement());

  // Expose 'display' attribute.
  RefPtr<nsAtom> displayValue = styleInfo.Display();
  attributes->SetAttribute(nsGkAtoms::display, displayValue);

  // Expose 'text-align' attribute.
  RefPtr<nsAtom> textAlignValue = styleInfo.TextAlign();
  attributes->SetAttribute(nsGkAtoms::textAlign, textAlignValue);

  // Expose 'text-indent' attribute.
  mozilla::LengthPercentage textIndent = styleInfo.TextIndent();
  if (textIndent.ConvertsToLength()) {
    attributes->SetAttribute(nsGkAtoms::textIndent,
                             textIndent.ToLengthInCSSPixels());
  } else if (textIndent.ConvertsToPercentage()) {
    attributes->SetAttribute(nsGkAtoms::textIndent, textIndent.ToPercentage());
  }

  // Expose 'margin-left' attribute.
  attributes->SetAttribute(nsGkAtoms::marginLeft, styleInfo.MarginLeft());

  // Expose 'margin-right' attribute.
  attributes->SetAttribute(nsGkAtoms::marginRight, styleInfo.MarginRight());

  // Expose 'margin-top' attribute.
  attributes->SetAttribute(nsGkAtoms::marginTop, styleInfo.MarginTop());

  // Expose 'margin-bottom' attribute.
  attributes->SetAttribute(nsGkAtoms::marginBottom, styleInfo.MarginBottom());

  // Expose data-at-shortcutkeys attribute for web applications and virtual
  // cursors. Currently mostly used by JAWS.
  nsAutoString atShortcutKeys;
  if (mContent->AsElement()->GetAttr(
          kNameSpaceID_None, nsGkAtoms::dataAtShortcutkeys, atShortcutKeys)) {
    attributes->SetAttribute(nsGkAtoms::dataAtShortcutkeys, atShortcutKeys);
  }

  return attributes.forget();
}

bool LocalAccessible::AttributeChangesState(nsAtom* aAttribute) {
  return aAttribute == nsGkAtoms::aria_disabled ||
         aAttribute == nsGkAtoms::disabled ||
         aAttribute == nsGkAtoms::tabindex ||
         aAttribute == nsGkAtoms::aria_required ||
         aAttribute == nsGkAtoms::aria_invalid ||
         aAttribute == nsGkAtoms::aria_expanded ||
         aAttribute == nsGkAtoms::aria_checked ||
         (aAttribute == nsGkAtoms::aria_pressed && IsButton()) ||
         aAttribute == nsGkAtoms::aria_readonly ||
         aAttribute == nsGkAtoms::aria_current ||
         aAttribute == nsGkAtoms::aria_haspopup ||
         aAttribute == nsGkAtoms::aria_busy ||
         aAttribute == nsGkAtoms::aria_multiline ||
         aAttribute == nsGkAtoms::contenteditable ||
         (aAttribute == nsGkAtoms::href && IsHTMLLink());
}

void LocalAccessible::DOMAttributeChanged(int32_t aNameSpaceID,
                                          nsAtom* aAttribute, int32_t aModType,
                                          const nsAttrValue* aOldValue,
                                          uint64_t aOldState) {
  // Fire accessible event after short timer, because we need to wait for
  // DOM attribute & resulting layout to actually change. Otherwise,
  // assistive technology will retrieve the wrong state/value/selection info.

  // XXX todo
  // We still need to handle special HTML cases here
  // For example, if an <img>'s usemap attribute is modified
  // Otherwise it may just be a state change, for example an object changing
  // its visibility
  //
  // XXX todo: report aria state changes for "undefined" literal value changes
  // filed as bug 472142
  //
  // XXX todo:  invalidate accessible when aria state changes affect exposed
  // role filed as bug 472143

  if (AttributeChangesState(aAttribute)) {
    uint64_t currState = State();
    uint64_t diffState = currState ^ aOldState;
    if (diffState) {
      for (uint64_t state = 1; state <= states::LAST_ENTRY; state <<= 1) {
        if (diffState & state) {
          RefPtr<AccEvent> stateChangeEvent =
              new AccStateChangeEvent(this, state, (currState & state));
          mDoc->FireDelayedEvent(stateChangeEvent);
        }
      }
    }
  }

  // When a details object has its open attribute changed
  // we should fire a state-change event on the accessible of
  // its main summary
  if (aAttribute == nsGkAtoms::open) {
    // FromDetails checks if the given accessible belongs to
    // a details frame and also locates the accessible of its
    // main summary.
    if (HTMLSummaryAccessible* summaryAccessible =
            HTMLSummaryAccessible::FromDetails(this)) {
      RefPtr<AccEvent> expandedChangeEvent =
          new AccStateChangeEvent(summaryAccessible, states::EXPANDED);
      mDoc->FireDelayedEvent(expandedChangeEvent);
      return;
    }
  }

  // Check for namespaced ARIA attribute
  if (aNameSpaceID == kNameSpaceID_None) {
    // Check for hyphenated aria-foo property?
    if (StringBeginsWith(nsDependentAtomString(aAttribute), u"aria-"_ns)) {
      uint8_t attrFlags = aria::AttrCharacteristicsFor(aAttribute);
      if (!(attrFlags & ATTR_BYPASSOBJ)) {
        // For aria attributes like drag and drop changes we fire a generic
        // attribute change event; at least until native API comes up with a
        // more meaningful event.
        RefPtr<AccEvent> event =
            new AccObjectAttrChangedEvent(this, aAttribute);
        mDoc->FireDelayedEvent(event);
      }
    }
  }

  dom::Element* elm = Elm();

  // Fire text value change event whenever aria-valuetext is changed.
  if (aAttribute == nsGkAtoms::aria_valuetext) {
    mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE, this);
    return;
  }

  // Fire numeric value change event when aria-valuenow is changed and
  // aria-valuetext is empty
  if (aAttribute == nsGkAtoms::aria_valuenow &&
      (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_valuetext) ||
       elm->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_valuetext,
                        nsGkAtoms::_empty, eCaseMatters))) {
    mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, this);
    return;
  }

  if (aAttribute == nsGkAtoms::aria_owns) {
    mDoc->Controller()->ScheduleRelocation(this);
  }

  // Fire name change and description change events.
  if (aAttribute == nsGkAtoms::aria_label) {
    // A valid aria-labelledby would take precedence so an aria-label change
    // won't change the name.
    IDRefsIterator iter(mDoc, elm, nsGkAtoms::aria_labelledby);
    if (!iter.NextElem()) {
      mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    }
    return;
  }

  if (aAttribute == nsGkAtoms::aria_describedby) {
    mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE, this);
    if (aModType == dom::MutationEvent_Binding::MODIFICATION ||
        aModType == dom::MutationEvent_Binding::ADDITION) {
      // The subtrees of the new aria-describedby targets might be used to
      // compute the description for this. Therefore, we need to set
      // the eHasDescriptionDependent flag on all Accessibles in these subtrees.
      IDRefsIterator iter(mDoc, elm, nsGkAtoms::aria_describedby);
      while (LocalAccessible* target = iter.Next()) {
        target->ModifySubtreeContextFlags(eHasDescriptionDependent, true);
      }
    }
    return;
  }

  if (aAttribute == nsGkAtoms::aria_labelledby) {
    mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    if (aModType == dom::MutationEvent_Binding::MODIFICATION ||
        aModType == dom::MutationEvent_Binding::ADDITION) {
      // The subtrees of the new aria-labelledby targets might be used to
      // compute the name for this. Therefore, we need to set
      // the eHasNameDependent flag on all Accessibles in these subtrees.
      IDRefsIterator iter(mDoc, elm, nsGkAtoms::aria_labelledby);
      while (LocalAccessible* target = iter.Next()) {
        target->ModifySubtreeContextFlags(eHasNameDependent, true);
      }
    }
    return;
  }

  if (aAttribute == nsGkAtoms::alt &&
      !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_label) &&
      !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_labelledby)) {
    mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    return;
  }

  if (aAttribute == nsGkAtoms::title) {
    nsAutoString name;
    ARIAName(name);
    if (name.IsEmpty()) {
      NativeName(name);
      if (name.IsEmpty()) {
        mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
        return;
      }
    }

    if (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_describedby)) {
      mDoc->FireDelayedEvent(nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE,
                             this);
    }

    return;
  }

  // ARIA or XUL selection
  if ((mContent->IsXULElement() && aAttribute == nsGkAtoms::selected) ||
      aAttribute == nsGkAtoms::aria_selected) {
    LocalAccessible* widget = nsAccUtils::GetSelectableContainer(this, State());
    if (widget) {
      AccSelChangeEvent::SelChangeType selChangeType =
          elm->AttrValueIs(aNameSpaceID, aAttribute, nsGkAtoms::_true,
                           eCaseMatters)
              ? AccSelChangeEvent::eSelectionAdd
              : AccSelChangeEvent::eSelectionRemove;

      RefPtr<AccEvent> event =
          new AccSelChangeEvent(widget, this, selChangeType);
      mDoc->FireDelayedEvent(event);
    }

    return;
  }

}

GroupPos LocalAccessible::GroupPosition() {
  GroupPos groupPos;
  if (!HasOwnContent()) return groupPos;

  // Get group position from ARIA attributes.
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_level, &groupPos.level);
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_setsize,
                           &groupPos.setSize);
  nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_posinset,
                           &groupPos.posInSet);

  // If ARIA is missed and the accessible is visible then calculate group
  // position from hierarchy.
  if (State() & states::INVISIBLE) return groupPos;

  // Calculate group level if ARIA is missed.
  if (groupPos.level == 0) {
    int32_t level = GetLevelInternal();
    if (level != 0) {
      groupPos.level = level;
    } else {
      const nsRoleMapEntry* role = this->ARIARoleMap();
      if (role && role->Is(nsGkAtoms::heading)) {
        groupPos.level = 2;
      }
    }
  }

  // Calculate position in group and group size if ARIA is missed.
  if (groupPos.posInSet == 0 || groupPos.setSize == 0) {
    int32_t posInSet = 0, setSize = 0;
    GetPositionAndSizeInternal(&posInSet, &setSize);
    if (posInSet != 0 && setSize != 0) {
      if (groupPos.posInSet == 0) groupPos.posInSet = posInSet;

      if (groupPos.setSize == 0) groupPos.setSize = setSize;
    }
  }

  return groupPos;
}

uint64_t LocalAccessible::State() {
  if (IsDefunct()) return states::DEFUNCT;

  uint64_t state = NativeState();
  // Apply ARIA states to be sure accessible states will be overridden.
  ApplyARIAState(&state);

  // If this is an ARIA item of the selectable widget and if it's focused and
  // not marked unselected explicitly (i.e. aria-selected="false") then expose
  // it as selected to make ARIA widget authors life easier.
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && !(state & states::SELECTED) &&
      (!mContent->IsElement() ||
       !mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::aria_selected,
                                           nsGkAtoms::_false, eCaseMatters))) {
    // Special case for tabs: focused tab or focus inside related tab panel
    // implies selected state.
    if (roleMapEntry->role == roles::PAGETAB) {
      if (state & states::FOCUSED) {
        state |= states::SELECTED;
      } else {
        // If focus is in a child of the tab panel surely the tab is selected!
        Relation rel = RelationByType(RelationType::LABEL_FOR);
        LocalAccessible* relTarget = nullptr;
        while ((relTarget = rel.Next())) {
          if (relTarget->Role() == roles::PROPERTYPAGE &&
              FocusMgr()->IsFocusWithin(relTarget)) {
            state |= states::SELECTED;
          }
        }
      }
    } else if (state & states::FOCUSED) {
      LocalAccessible* container =
          nsAccUtils::GetSelectableContainer(this, state);
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
    LocalAccessible* widget = ContainerWidget();
    if (widget && widget->CurrentItem() == this) state |= states::ACTIVE;
  }

  if ((state & states::COLLAPSED) || (state & states::EXPANDED)) {
    state |= states::EXPANDABLE;
  }

  // For some reasons DOM node may have not a frame. We tract such accessibles
  // as invisible.
  nsIFrame* frame = GetFrame();
  if (!frame) return state;

  if (frame->StyleEffects()->mOpacity == 1.0f && !(state & states::INVISIBLE)) {
    state |= states::OPAQUE1;
  }

  return state;
}

void LocalAccessible::ApplyARIAState(uint64_t* aState) const {
  if (!mContent->IsElement()) return;

  dom::Element* element = mContent->AsElement();

  // Test for universal states first
  *aState |= aria::UniversalStatesFor(element);

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry) {
    // We only force the readonly bit off if we have a real mapping for the aria
    // role. This preserves the ability for screen readers to use readonly
    // (primarily on the document) as the hint for creating a virtual buffer.
    if (roleMapEntry->role != roles::NOTHING) *aState &= ~states::READONLY;

    if (mContent->HasID()) {
      // If has a role & ID and aria-activedescendant on the container, assume
      // focusable.
      const LocalAccessible* ancestor = this;
      while ((ancestor = ancestor->LocalParent()) && !ancestor->IsDoc()) {
        dom::Element* el = ancestor->Elm();
        if (el &&
            el->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant)) {
          *aState |= states::FOCUSABLE;
          break;
        }
      }
    }
  }

  if (*aState & states::FOCUSABLE) {
    // Propogate aria-disabled from ancestors down to any focusable descendant.
    const LocalAccessible* ancestor = this;
    while ((ancestor = ancestor->LocalParent()) && !ancestor->IsDoc()) {
      dom::Element* el = ancestor->Elm();
      if (el && el->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_disabled,
                                nsGkAtoms::_true, eCaseMatters)) {
        *aState |= states::UNAVAILABLE;
        break;
      }
    }
  } else {
    // Sometimes, we use aria-activedescendant targeting something which isn't
    // actually a descendant. This is technically a spec violation, but it's a
    // useful hack which makes certain things much easier. For example, we use
    // this for "fake focus" for multi select browser tabs and Quantumbar
    // autocomplete suggestions.
    // In these cases, the aria-activedescendant code above won't make the
    // active item focusable. It doesn't make sense for something to have
    // focus when it isn't focusable, so fix that here.
    if (FocusMgr()->IsActiveItem(this)) {
      *aState |= states::FOCUSABLE;
    }
  }

  // special case: A native button element whose role got transformed by ARIA to
  // a toggle button Also applies to togglable button menus, like in the Dev
  // Tools Web Console.
  if (IsButton() || IsMenuButton()) {
    aria::MapToState(aria::eARIAPressed, element, aState);
  }

  if (!roleMapEntry) return;

  *aState |= roleMapEntry->state;

  if (aria::MapToState(roleMapEntry->attributeMap1, element, aState) &&
      aria::MapToState(roleMapEntry->attributeMap2, element, aState) &&
      aria::MapToState(roleMapEntry->attributeMap3, element, aState)) {
    aria::MapToState(roleMapEntry->attributeMap4, element, aState);
  }

  // ARIA gridcell inherits readonly state from the grid until it's overridden.
  if ((roleMapEntry->Is(nsGkAtoms::gridcell) ||
       roleMapEntry->Is(nsGkAtoms::columnheader) ||
       roleMapEntry->Is(nsGkAtoms::rowheader)) &&
      !nsAccUtils::HasDefinedARIAToken(mContent, nsGkAtoms::aria_readonly)) {
    const TableCellAccessible* cell = AsTableCell();
    if (cell) {
      TableAccessible* table = cell->Table();
      if (table) {
        LocalAccessible* grid = table->AsAccessible();
        uint64_t gridState = 0;
        grid->ApplyARIAState(&gridState);
        *aState |= gridState & states::READONLY;
      }
    }
  }
}

void LocalAccessible::Value(nsString& aValue) const {
  if (HasNumericValue()) {
    // aria-valuenow is a number, and aria-valuetext is the optional text
    // equivalent. For the string value, we will try the optional text
    // equivalent first.
    if (!mContent->IsElement()) {
      return;
    }

    if (!mContent->AsElement()->GetAttr(kNameSpaceID_None,
                                        nsGkAtoms::aria_valuetext, aValue)) {
      if (!NativeHasNumericValue()) {
        double checkValue = CurValue();
        if (!IsNaN(checkValue)) {
          aValue.AppendFloat(checkValue);
        }
      }
    }
    return;
  }

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry) {
    return;
  }

  // Value of textbox is a textified subtree.
  if (roleMapEntry->Is(nsGkAtoms::textbox)) {
    nsTextEquivUtils::GetTextEquivFromSubtree(this, aValue);
    return;
  }

  // Value of combobox is a text of current or selected item.
  if (roleMapEntry->Is(nsGkAtoms::combobox)) {
    LocalAccessible* option = CurrentItem();
    if (!option) {
      uint32_t childCount = ChildCount();
      for (uint32_t idx = 0; idx < childCount; idx++) {
        LocalAccessible* child = mChildren.ElementAt(idx);
        if (child->IsListControl()) {
          option = child->GetSelectedItem(0);
          break;
        }
      }
    }

    if (option) nsTextEquivUtils::GetTextEquivFromSubtree(option, aValue);
  }
}

double LocalAccessible::MaxValue() const {
  double checkValue = AttrNumericValue(nsGkAtoms::aria_valuemax);
  return IsNaN(checkValue) && !NativeHasNumericValue() ? 100 : checkValue;
}

double LocalAccessible::MinValue() const {
  double checkValue = AttrNumericValue(nsGkAtoms::aria_valuemin);
  return IsNaN(checkValue) && !NativeHasNumericValue() ? 0 : checkValue;
}

double LocalAccessible::Step() const {
  return UnspecifiedNaN<double>();  // no mimimum increment (step) in ARIA.
}

double LocalAccessible::CurValue() const {
  double checkValue = AttrNumericValue(nsGkAtoms::aria_valuenow);
  if (IsNaN(checkValue) && !NativeHasNumericValue()) {
    double minValue = MinValue();
    return minValue + ((MaxValue() - minValue) / 2);
  }

  return checkValue;
}

bool LocalAccessible::SetCurValue(double aValue) {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->valueRule == eNoValue) return false;

  const uint32_t kValueCannotChange = states::READONLY | states::UNAVAILABLE;
  if (State() & kValueCannotChange) return false;

  double checkValue = MinValue();
  if (!IsNaN(checkValue) && aValue < checkValue) return false;

  checkValue = MaxValue();
  if (!IsNaN(checkValue) && aValue > checkValue) return false;

  nsAutoString strValue;
  strValue.AppendFloat(aValue);

  if (!mContent->IsElement()) return true;

  return NS_SUCCEEDED(mContent->AsElement()->SetAttr(
      kNameSpaceID_None, nsGkAtoms::aria_valuenow, strValue, true));
}

role LocalAccessible::ARIATransformRole(role aRole) const {
  // Beginning with ARIA 1.1, user agents are expected to use the native host
  // language role of the element when the region role is used without a name.
  // https://rawgit.com/w3c/aria/master/core-aam/core-aam.html#role-map-region
  //
  // XXX: While the name computation algorithm can be non-trivial in the general
  // case, it should not be especially bad here: If the author hasn't used the
  // region role, this calculation won't occur. And the region role's name
  // calculation rule excludes name from content. That said, this use case is
  // another example of why we should consider caching the accessible name. See:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1378235.
  if (aRole == roles::REGION) {
    nsAutoString name;
    Name(name);
    return name.IsEmpty() ? NativeRole() : aRole;
  }

  // XXX: these unfortunate exceptions don't fit into the ARIA table. This is
  // where the accessible role depends on both the role and ARIA state.
  if (aRole == roles::PUSHBUTTON) {
    if (nsAccUtils::HasDefinedARIAToken(mContent, nsGkAtoms::aria_pressed)) {
      // For simplicity, any existing pressed attribute except "" or "undefined"
      // indicates a toggle.
      return roles::TOGGLE_BUTTON;
    }

    if (mContent->IsElement() &&
        mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::aria_haspopup,
                                           nsGkAtoms::_true, eCaseMatters)) {
      // For button with aria-haspopup="true".
      return roles::BUTTONMENU;
    }

  } else if (aRole == roles::LISTBOX) {
    // A listbox inside of a combobox needs a special role because of ATK
    // mapping to menu.
    if (mParent && mParent->IsCombobox()) {
      return roles::COMBOBOX_LIST;
    } else {
      // Listbox is owned by a combobox
      Relation rel = RelationByType(RelationType::NODE_CHILD_OF);
      LocalAccessible* targetAcc = nullptr;
      while ((targetAcc = rel.Next())) {
        if (targetAcc->IsCombobox()) return roles::COMBOBOX_LIST;
      }
    }

  } else if (aRole == roles::OPTION) {
    if (mParent && mParent->Role() == roles::COMBOBOX_LIST) {
      return roles::COMBOBOX_OPTION;
    }

  } else if (aRole == roles::MENUITEM) {
    // Menuitem has a submenu.
    if (mContent->IsElement() &&
        mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::aria_haspopup,
                                           nsGkAtoms::_true, eCaseMatters)) {
      return roles::PARENT_MENUITEM;
    }

  } else if (aRole == roles::CELL) {
    // A cell inside an ancestor table element that has a grid role needs a
    // gridcell role
    // (https://www.w3.org/TR/html-aam-1.0/#html-element-role-mappings).
    const TableCellAccessible* cell = AsTableCell();
    if (cell) {
      TableAccessible* table = cell->Table();
      if (table && table->AsAccessible()->IsARIARole(nsGkAtoms::grid)) {
        return roles::GRID_CELL;
      }
    }
  }

  return aRole;
}

nsAtom* LocalAccessible::LandmarkRole() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->IsOfType(eLandmark)
             ? roleMapEntry->roleAtom
             : nullptr;
}

role LocalAccessible::NativeRole() const { return roles::NOTHING; }

uint8_t LocalAccessible::ActionCount() const {
  return GetActionRule() == eNoAction ? 0 : 1;
}

void LocalAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();

  if (aIndex != 0) return;

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

    case eCheckUncheckAction: {
      uint64_t state = State();
      if (state & states::CHECKED) {
        aName.AssignLiteral("uncheck");
      } else if (state & states::MIXED) {
        aName.AssignLiteral("cycle");
      } else {
        aName.AssignLiteral("check");
      }
      return;
    }

    case eJumpAction:
      aName.AssignLiteral("jump");
      return;

    case eOpenCloseAction:
      if (State() & states::COLLAPSED) {
        aName.AssignLiteral("open");
      } else {
        aName.AssignLiteral("close");
      }
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
      if (State() & states::COLLAPSED) {
        aName.AssignLiteral("expand");
      } else {
        aName.AssignLiteral("collapse");
      }
      return;
  }
}

bool LocalAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != 0) return false;

  if (GetActionRule() != eNoAction) {
    DoCommand();
    return true;
  }

  return false;
}

nsIContent* LocalAccessible::GetAtomicRegion() const {
  nsIContent* loopContent = mContent;
  nsAutoString atomic;
  while (loopContent &&
         (!loopContent->IsElement() ||
          !loopContent->AsElement()->GetAttr(kNameSpaceID_None,
                                             nsGkAtoms::aria_atomic, atomic))) {
    loopContent = loopContent->GetParent();
  }

  return atomic.EqualsLiteral("true") ? loopContent : nullptr;
}

Relation LocalAccessible::RelationByType(RelationType aType) const {
  if (!HasOwnContent()) return Relation();

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();

  // Relationships are defined on the same content node that the role would be
  // defined on.
  switch (aType) {
    case RelationType::LABELLED_BY: {
      Relation rel(
          new IDRefsIterator(mDoc, mContent, nsGkAtoms::aria_labelledby));
      if (mContent->IsHTMLElement()) {
        rel.AppendIter(new HTMLLabelIterator(Document(), this));
      }
      rel.AppendIter(new XULLabelIterator(Document(), mContent));

      return rel;
    }

    case RelationType::LABEL_FOR: {
      Relation rel(new RelatedAccIterator(Document(), mContent,
                                          nsGkAtoms::aria_labelledby));
      if (mContent->IsXULElement(nsGkAtoms::label)) {
        rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::control));
      }

      return rel;
    }

    case RelationType::DESCRIBED_BY: {
      Relation rel(
          new IDRefsIterator(mDoc, mContent, nsGkAtoms::aria_describedby));
      if (mContent->IsXULElement()) {
        rel.AppendIter(new XULDescriptionIterator(Document(), mContent));
      }

      return rel;
    }

    case RelationType::DESCRIPTION_FOR: {
      Relation rel(new RelatedAccIterator(Document(), mContent,
                                          nsGkAtoms::aria_describedby));

      // This affectively adds an optional control attribute to xul:description,
      // which only affects accessibility, by allowing the description to be
      // tied to a control.
      if (mContent->IsXULElement(nsGkAtoms::description)) {
        rel.AppendIter(new IDRefsIterator(mDoc, mContent, nsGkAtoms::control));
      }

      return rel;
    }

    case RelationType::NODE_CHILD_OF: {
      Relation rel;
      // This is an ARIA tree or treegrid that doesn't use owns, so we need to
      // get the parent the hard way.
      if (roleMapEntry && (roleMapEntry->role == roles::OUTLINEITEM ||
                           roleMapEntry->role == roles::LISTITEM ||
                           roleMapEntry->role == roles::ROW)) {
        rel.AppendTarget(GetGroupInfo()->ConceptualParent());
      }

      // If this is an OOP iframe document, we can't support NODE_CHILD_OF
      // here, since the iframe resides in a different process. This is fine
      // because the client will then request the parent instead, which will be
      // correctly handled by platform code.
      if (XRE_IsContentProcess() && IsRoot()) {
        dom::Document* doc =
            const_cast<LocalAccessible*>(this)->AsDoc()->DocumentNode();
        dom::BrowsingContext* bc = doc->GetBrowsingContext();
        MOZ_ASSERT(bc);
        if (!bc->Top()->IsInProcess()) {
          return rel;
        }
      }

      // If accessible is in its own Window, or is the root of a document,
      // then we should provide NODE_CHILD_OF relation so that MSAA clients
      // can easily get to true parent instead of getting to oleacc's
      // ROLE_WINDOW accessible which will prevent us from going up further
      // (because it is system generated and has no idea about the hierarchy
      // above it).
      nsIFrame* frame = GetFrame();
      if (frame) {
        nsView* view = frame->GetView();
        if (view) {
          nsIScrollableFrame* scrollFrame = do_QueryFrame(frame);
          if (scrollFrame || view->GetWidget() || !frame->GetParent()) {
            rel.AppendTarget(LocalParent());
          }
        }
      }

      return rel;
    }

    case RelationType::NODE_PARENT_OF: {
      // ARIA tree or treegrid can do the hierarchy by @aria-level, ARIA trees
      // also can be organized by groups.
      if (roleMapEntry && (roleMapEntry->role == roles::OUTLINEITEM ||
                           roleMapEntry->role == roles::LISTITEM ||
                           roleMapEntry->role == roles::ROW ||
                           roleMapEntry->role == roles::OUTLINE ||
                           roleMapEntry->role == roles::LIST ||
                           roleMapEntry->role == roles::TREE_TABLE)) {
        return Relation(new ItemIterator(this));
      }

      return Relation();
    }

    case RelationType::CONTROLLED_BY:
      return Relation(new RelatedAccIterator(Document(), mContent,
                                             nsGkAtoms::aria_controls));

    case RelationType::CONTROLLER_FOR: {
      Relation rel(
          new IDRefsIterator(mDoc, mContent, nsGkAtoms::aria_controls));
      rel.AppendIter(new HTMLOutputIterator(Document(), mContent));
      return rel;
    }

    case RelationType::FLOWS_TO:
      return Relation(
          new IDRefsIterator(mDoc, mContent, nsGkAtoms::aria_flowto));

    case RelationType::FLOWS_FROM:
      return Relation(
          new RelatedAccIterator(Document(), mContent, nsGkAtoms::aria_flowto));

    case RelationType::MEMBER_OF: {
      if (Role() == roles::RADIOBUTTON) {
        /* If we see a radio button role here, we're dealing with an aria
         * radio button (because input=radio buttons are
         * HTMLRadioButtonAccessibles) */
        Relation rel = Relation();
        LocalAccessible* currParent = LocalParent();
        while (currParent && currParent->Role() != roles::RADIO_GROUP) {
          currParent = currParent->LocalParent();
        }

        if (currParent && currParent->Role() == roles::RADIO_GROUP) {
          /* If we found a radiogroup parent, search for all
           * roles::RADIOBUTTON children and add them to our relation.
           * This search will include the radio button this method
           * was called from, which is expected. */
          Pivot p = Pivot(currParent);
          PivotRoleRule rule(roles::RADIOBUTTON);
          Accessible* match = p.Next(currParent, rule);
          while (match) {
            MOZ_ASSERT(match->IsLocal(),
                       "We shouldn't find any remote accs while building our "
                       "relation!");
            rel.AppendTarget(match->AsLocal());
            match = p.Next(match, rule);
          }
        }

        /* By webkit's standard, aria radio buttons do not get grouped
         * if they lack a group parent, so we return an empty
         * relation here if the above check fails. */

        return rel;
      }

      return Relation(mDoc, GetAtomicRegion());
    }

    case RelationType::LINKS_TO: {
      Relation rel = Relation();
      if (Role() == roles::LINK) {
        dom::HTMLAnchorElement* anchor =
            dom::HTMLAnchorElement::FromNode(mContent);
        if (!anchor) {
          return rel;
        }
        // If this node is an anchor element, query its hash to find the
        // target.
        nsAutoString hash;
        anchor->GetHash(hash);
        if (hash.IsEmpty()) {
          return rel;
        }

        // GetHash returns an ID or name with a leading '#', trim it so we can
        // search the doc by ID or name alone.
        hash.Trim("#");
        if (dom::Element* elm = mContent->OwnerDoc()->GetElementById(hash)) {
          rel.AppendTarget(mDoc->GetAccessibleOrContainer(elm));
        } else if (nsCOMPtr<nsINodeList> list =
                       mContent->OwnerDoc()->GetElementsByName(hash)) {
          // Loop through the named nodes looking for the first anchor
          uint32_t length = list->Length();
          for (uint32_t i = 0; i < length; i++) {
            nsIContent* node = list->Item(i);
            if (node->IsHTMLElement(nsGkAtoms::a)) {
              rel.AppendTarget(mDoc->GetAccessibleOrContainer(node));
              break;
            }
          }
        }
      }

      return rel;
    }

    case RelationType::SUBWINDOW_OF:
    case RelationType::EMBEDS:
    case RelationType::EMBEDDED_BY:
    case RelationType::POPUP_FOR:
    case RelationType::PARENT_WINDOW_OF:
      return Relation();

    case RelationType::DEFAULT_BUTTON: {
      if (mContent->IsHTMLElement()) {
        // HTML form controls implements nsIFormControl interface.
        nsCOMPtr<nsIFormControl> control(do_QueryInterface(mContent));
        if (control) {
          if (dom::HTMLFormElement* form = control->GetFormElement()) {
            nsCOMPtr<nsIContent> formContent =
                do_QueryInterface(form->GetDefaultSubmitElement());
            return Relation(mDoc, formContent);
          }
        }
      } else {
        // In XUL, use first <button default="true" .../> in the document
        dom::Document* doc = mContent->OwnerDoc();
        nsIContent* buttonEl = nullptr;
        if (doc->AllowXULXBL()) {
          nsCOMPtr<nsIHTMLCollection> possibleDefaultButtons =
              doc->GetElementsByAttribute(u"default"_ns, u"true"_ns);
          if (possibleDefaultButtons) {
            uint32_t length = possibleDefaultButtons->Length();
            // Check for button in list of default="true" elements
            for (uint32_t count = 0; count < length && !buttonEl; count++) {
              nsIContent* item = possibleDefaultButtons->Item(count);
              RefPtr<nsIDOMXULButtonElement> button =
                  item->IsElement() ? item->AsElement()->AsXULButton()
                                    : nullptr;
              if (button) {
                buttonEl = item;
              }
            }
          }
          return Relation(mDoc, buttonEl);
        }
      }
      return Relation();
    }

    case RelationType::CONTAINING_DOCUMENT:
      return Relation(mDoc);

    case RelationType::CONTAINING_TAB_PANE: {
      nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(GetNode());
      if (docShell) {
        // Walk up the parent chain without crossing the boundary at which item
        // types change, preventing us from walking up out of tab content.
        nsCOMPtr<nsIDocShellTreeItem> root;
        docShell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
        if (root) {
          // If the item type is typeContent, we assume we are in browser tab
          // content. Note, this includes content such as about:addons,
          // for consistency.
          if (root->ItemType() == nsIDocShellTreeItem::typeContent) {
            return Relation(nsAccUtils::GetDocAccessibleFor(root));
          }
        }
      }
      return Relation();
    }

    case RelationType::CONTAINING_APPLICATION:
      return Relation(ApplicationAcc());

    case RelationType::DETAILS:
      return Relation(
          new IDRefsIterator(mDoc, mContent, nsGkAtoms::aria_details));

    case RelationType::DETAILS_FOR:
      return Relation(
          new RelatedAccIterator(mDoc, mContent, nsGkAtoms::aria_details));

    case RelationType::ERRORMSG:
      return Relation(
          new IDRefsIterator(mDoc, mContent, nsGkAtoms::aria_errormessage));

    case RelationType::ERRORMSG_FOR:
      return Relation(
          new RelatedAccIterator(mDoc, mContent, nsGkAtoms::aria_errormessage));

    default:
      return Relation();
  }
}

void LocalAccessible::GetNativeInterface(void** aNativeAccessible) {}

void LocalAccessible::DoCommand(nsIContent* aContent,
                                uint32_t aActionIndex) const {
  class Runnable final : public mozilla::Runnable {
   public:
    Runnable(const LocalAccessible* aAcc, nsIContent* aContent, uint32_t aIdx)
        : mozilla::Runnable("Runnable"),
          mAcc(aAcc),
          mContent(aContent),
          mIdx(aIdx) {}

    // XXX Cannot mark as MOZ_CAN_RUN_SCRIPT because the base class change
    //     requires too big changes across a lot of modules.
    MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
      if (mAcc) {
        MOZ_KnownLive(mAcc)->DispatchClickEvent(MOZ_KnownLive(mContent), mIdx);
      }
      return NS_OK;
    }

    void Revoke() {
      mAcc = nullptr;
      mContent = nullptr;
    }

   private:
    RefPtr<const LocalAccessible> mAcc;
    nsCOMPtr<nsIContent> mContent;
    uint32_t mIdx;
  };

  nsIContent* content = aContent ? aContent : mContent.get();
  nsCOMPtr<nsIRunnable> runnable = new Runnable(this, content, aActionIndex);
  NS_DispatchToMainThread(runnable);
}

void LocalAccessible::DispatchClickEvent(nsIContent* aContent,
                                         uint32_t aActionIndex) const {
  if (IsDefunct()) return;

  RefPtr<PresShell> presShell = mDoc->PresShellPtr();

  // Scroll into view.
  presShell->ScrollContentIntoView(aContent, ScrollAxis(), ScrollAxis(),
                                   ScrollFlags::ScrollOverflowHidden);

  AutoWeakFrame frame = aContent->GetPrimaryFrame();
  if (!frame) return;

  // Compute x and y coordinates.
  nsPoint point;
  nsCOMPtr<nsIWidget> widget = frame->GetNearestWidget(point);
  if (!widget) return;

  nsSize size = frame->GetSize();

  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  int32_t x = presContext->AppUnitsToDevPixels(point.x + size.width / 2);
  int32_t y = presContext->AppUnitsToDevPixels(point.y + size.height / 2);

  // Simulate a touch interaction by dispatching touch events with mouse events.
  nsCoreUtils::DispatchTouchEvent(eTouchStart, x, y, aContent, frame, presShell,
                                  widget);
  nsCoreUtils::DispatchMouseEvent(eMouseDown, x, y, aContent, frame, presShell,
                                  widget);
  nsCoreUtils::DispatchTouchEvent(eTouchEnd, x, y, aContent, frame, presShell,
                                  widget);
  nsCoreUtils::DispatchMouseEvent(eMouseUp, x, y, aContent, frame, presShell,
                                  widget);
}

void LocalAccessible::ScrollToPoint(uint32_t aCoordinateType, int32_t aX,
                                    int32_t aY) {
  nsIFrame* frame = GetFrame();
  if (!frame) return;

  nsIntPoint coords =
      nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType, this);

  nsIFrame* parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent())) {
    nsCoreUtils::ScrollFrameToPoint(parentFrame, frame, coords);
  }
}

void LocalAccessible::AppendTextTo(nsAString& aText, uint32_t aStartOffset,
                                   uint32_t aLength) {
  // Return text representation of non-text accessible within hypertext
  // accessible. Text accessible overrides this method to return enclosed text.
  if (aStartOffset != 0 || aLength == 0) return;

  nsIFrame* frame = GetFrame();
  if (!frame) {
    if (nsCoreUtils::IsDisplayContents(mContent)) {
      aText += kEmbeddedObjectChar;
    }
    return;
  }

  MOZ_ASSERT(mParent,
             "Called on accessible unbound from tree. Result can be wrong.");

  if (frame->IsBrFrame()) {
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

void LocalAccessible::Shutdown() {
  // Mark the accessible as defunct, invalidate the child count and pointers to
  // other accessibles, also make sure none of its children point to this
  // parent
  mStateFlags |= eIsDefunct;

  int32_t childCount = mChildren.Length();
  for (int32_t childIdx = 0; childIdx < childCount; childIdx++) {
    mChildren.ElementAt(childIdx)->UnbindFromParent();
  }
  mChildren.Clear();

  mEmbeddedObjCollector = nullptr;

  if (mParent) mParent->RemoveChild(this);

  mContent = nullptr;
  mDoc = nullptr;
  if (SelectionMgr() && SelectionMgr()->AccessibleWithCaret(nullptr) == this) {
    SelectionMgr()->ResetCaretOffset();
  }
}

// LocalAccessible protected
void LocalAccessible::ARIAName(nsString& aName) const {
  // aria-labelledby now takes precedence over aria-label
  nsresult rv = nsTextEquivUtils::GetTextEquivFromIDRefs(
      this, nsGkAtoms::aria_labelledby, aName);
  if (NS_SUCCEEDED(rv)) {
    aName.CompressWhitespace();
  }

  if (aName.IsEmpty() && mContent->IsElement() &&
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_label,
                                     aName)) {
    aName.CompressWhitespace();
  }
}

// LocalAccessible protected
void LocalAccessible::ARIADescription(nsString& aDescription) const {
  // aria-describedby takes precedence over aria-description
  nsresult rv = nsTextEquivUtils::GetTextEquivFromIDRefs(
      this, nsGkAtoms::aria_describedby, aDescription);
  if (NS_SUCCEEDED(rv)) {
    aDescription.CompressWhitespace();
  }

  if (aDescription.IsEmpty() && mContent->IsElement() &&
      mContent->AsElement()->GetAttr(
          kNameSpaceID_None, nsGkAtoms::aria_description, aDescription)) {
    aDescription.CompressWhitespace();
  }
}

// LocalAccessible protected
ENameValueFlag LocalAccessible::NativeName(nsString& aName) const {
  if (mContent->IsHTMLElement()) {
    LocalAccessible* label = nullptr;
    HTMLLabelIterator iter(Document(), this);
    while ((label = iter.Next())) {
      nsTextEquivUtils::AppendTextEquivFromContent(this, label->GetContent(),
                                                   &aName);
      aName.CompressWhitespace();
    }

    if (!aName.IsEmpty()) return eNameOK;

    NameFromAssociatedXULLabel(mDoc, mContent, aName);
    if (!aName.IsEmpty()) {
      return eNameOK;
    }

    nsTextEquivUtils::GetNameFromSubtree(this, aName);
    return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
  }

  if (mContent->IsXULElement()) {
    XULElmName(mDoc, mContent, aName);
    if (!aName.IsEmpty()) return eNameOK;

    nsTextEquivUtils::GetNameFromSubtree(this, aName);
    return aName.IsEmpty() ? eNameOK : eNameFromSubtree;
  }

  if (mContent->IsSVGElement()) {
    // If user agents need to choose among multiple 'desc' or 'title'
    // elements for processing, the user agent shall choose the first one.
    for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
         childElm = childElm->GetNextSibling()) {
      if (childElm->IsSVGElement(nsGkAtoms::title)) {
        nsTextEquivUtils::AppendTextEquivFromContent(this, childElm, &aName);
        return eNameOK;
      }
    }
  }

  return eNameOK;
}

// LocalAccessible protected
void LocalAccessible::NativeDescription(nsString& aDescription) {
  bool isXUL = mContent->IsXULElement();
  if (isXUL) {
    // Try XUL <description control="[id]">description text</description>
    XULDescriptionIterator iter(Document(), mContent);
    LocalAccessible* descr = nullptr;
    while ((descr = iter.Next())) {
      nsTextEquivUtils::AppendTextEquivFromContent(this, descr->GetContent(),
                                                   &aDescription);
    }
  }
}

// LocalAccessible protected
void LocalAccessible::BindToParent(LocalAccessible* aParent,
                                   uint32_t aIndexInParent) {
  MOZ_ASSERT(aParent, "This method isn't used to set null parent");
  MOZ_ASSERT(!mParent, "The child was expected to be moved");

#ifdef A11Y_LOG
  if (mParent) {
    logging::TreeInfo("BindToParent: stealing accessible", 0, "old parent",
                      mParent, "new parent", aParent, "child", this, nullptr);
  }
#endif

  mParent = aParent;
  mIndexInParent = aIndexInParent;

  if (mParent->HasNameDependent() || mParent->IsXULListItem() ||
      RelationByType(RelationType::LABEL_FOR).Next() ||
      nsTextEquivUtils::HasNameRule(mParent, eNameFromSubtreeRule)) {
    mContextFlags |= eHasNameDependent;
  } else {
    mContextFlags &= ~eHasNameDependent;
  }
  if (mParent->HasDescriptionDependent() ||
      RelationByType(RelationType::DESCRIPTION_FOR).Next()) {
    mContextFlags |= eHasDescriptionDependent;
  } else {
    mContextFlags &= ~eHasDescriptionDependent;
  }

  // Add name/description dependent flags for dependent content once
  // a name/description provider is added to doc.
  Relation rel = RelationByType(RelationType::LABELLED_BY);
  LocalAccessible* relTarget = nullptr;
  while ((relTarget = rel.Next())) {
    if (!relTarget->HasNameDependent()) {
      relTarget->ModifySubtreeContextFlags(eHasNameDependent, true);
    }
  }

  rel = RelationByType(RelationType::DESCRIBED_BY);
  while ((relTarget = rel.Next())) {
    if (!relTarget->HasDescriptionDependent()) {
      relTarget->ModifySubtreeContextFlags(eHasDescriptionDependent, true);
    }
  }

  mContextFlags |=
      static_cast<uint32_t>((mParent->IsAlert() || mParent->IsInsideAlert())) &
      eInsideAlert;

  // if a new column header is being added, invalidate the table's header cache.
  TableCellAccessible* cell = AsTableCell();
  if (cell && Role() == roles::COLUMNHEADER) {
    TableAccessible* table = cell->Table();
    if (table) {
      table->GetHeaderCache().Clear();
    }
  }
}

// LocalAccessible protected
void LocalAccessible::UnbindFromParent() {
  mParent = nullptr;
  mIndexInParent = -1;
  mIndexOfEmbeddedChild = -1;
  if (IsProxy()) MOZ_CRASH("this should never be called on proxy wrappers");

  delete mBits.groupInfo;
  mBits.groupInfo = nullptr;
  mContextFlags &= ~eHasNameDependent & ~eInsideAlert;
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public methods

RootAccessible* LocalAccessible::RootAccessible() const {
  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(GetNode());
  NS_ASSERTION(docShell, "No docshell for mContent");
  if (!docShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  docShell->GetInProcessRootTreeItem(getter_AddRefs(root));
  NS_ASSERTION(root, "No root content tree item");
  if (!root) {
    return nullptr;
  }

  DocAccessible* docAcc = nsAccUtils::GetDocAccessibleFor(root);
  return docAcc ? docAcc->AsRoot() : nullptr;
}

nsIFrame* LocalAccessible::GetFrame() const {
  return mContent ? mContent->GetPrimaryFrame() : nullptr;
}

nsINode* LocalAccessible::GetNode() const { return mContent; }

dom::Element* LocalAccessible::Elm() const {
  return dom::Element::FromNodeOrNull(mContent);
}

void LocalAccessible::Language(nsAString& aLanguage) {
  aLanguage.Truncate();

  if (!mDoc) return;

  nsCoreUtils::GetLanguageFor(mContent, nullptr, aLanguage);
  if (aLanguage.IsEmpty()) {  // Nothing found, so use document's language
    mDoc->DocumentNode()->GetHeaderData(nsGkAtoms::headerContentLanguage,
                                        aLanguage);
  }
}

bool LocalAccessible::InsertChildAt(uint32_t aIndex, LocalAccessible* aChild) {
  if (!aChild) return false;

  if (aIndex == mChildren.Length()) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    mChildren.AppendElement(aChild);
  } else {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    mChildren.InsertElementAt(aIndex, aChild);

    MOZ_ASSERT(mStateFlags & eKidsMutating, "Illicit children change");

    for (uint32_t idx = aIndex + 1; idx < mChildren.Length(); idx++) {
      mChildren[idx]->mIndexInParent = idx;
    }
  }

  if (aChild->IsText()) {
    mStateFlags |= eHasTextKids;
  }

  aChild->BindToParent(this, aIndex);
  return true;
}

bool LocalAccessible::RemoveChild(LocalAccessible* aChild) {
  MOZ_DIAGNOSTIC_ASSERT(aChild, "No child was given");
  MOZ_DIAGNOSTIC_ASSERT(aChild->mParent, "No parent");
  MOZ_DIAGNOSTIC_ASSERT(aChild->mParent == this, "Wrong parent");
  MOZ_DIAGNOSTIC_ASSERT(aChild->mIndexInParent != -1,
                        "Unbound child was given");
  MOZ_DIAGNOSTIC_ASSERT((mStateFlags & eKidsMutating) || aChild->IsDefunct() ||
                            aChild->IsDoc() || IsApplication(),
                        "Illicit children change");

  int32_t index = static_cast<uint32_t>(aChild->mIndexInParent);
  if (mChildren.SafeElementAt(index) != aChild) {
    MOZ_ASSERT_UNREACHABLE("A wrong child index");
    index = mChildren.IndexOf(aChild);
    if (index == -1) {
      MOZ_ASSERT_UNREACHABLE("No child was found");
      return false;
    }
  }

  aChild->UnbindFromParent();
  mChildren.RemoveElementAt(index);

  for (uint32_t idx = index; idx < mChildren.Length(); idx++) {
    mChildren[idx]->mIndexInParent = idx;
  }

  return true;
}

void LocalAccessible::RelocateChild(uint32_t aNewIndex,
                                    LocalAccessible* aChild) {
  MOZ_DIAGNOSTIC_ASSERT(aChild, "No child was given");
  MOZ_DIAGNOSTIC_ASSERT(aChild->mParent == this,
                        "A child from different subtree was given");
  MOZ_DIAGNOSTIC_ASSERT(aChild->mIndexInParent != -1,
                        "Unbound child was given");
  MOZ_DIAGNOSTIC_ASSERT(
      aChild->mParent->LocalChildAt(aChild->mIndexInParent) == aChild,
      "Wrong index in parent");
  MOZ_DIAGNOSTIC_ASSERT(
      static_cast<uint32_t>(aChild->mIndexInParent) != aNewIndex,
      "No move, same index");
  MOZ_DIAGNOSTIC_ASSERT(aNewIndex <= mChildren.Length(),
                        "Wrong new index was given");

  RefPtr<AccHideEvent> hideEvent = new AccHideEvent(aChild, false);
  if (mDoc->Controller()->QueueMutationEvent(hideEvent)) {
    aChild->SetHideEventTarget(true);
  }

  mEmbeddedObjCollector = nullptr;
  mChildren.RemoveElementAt(aChild->mIndexInParent);

  uint32_t startIdx = aNewIndex, endIdx = aChild->mIndexInParent;

  // If the child is moved after its current position.
  if (static_cast<uint32_t>(aChild->mIndexInParent) < aNewIndex) {
    startIdx = aChild->mIndexInParent;
    if (aNewIndex == mChildren.Length() + 1) {
      // The child is moved to the end.
      mChildren.AppendElement(aChild);
      endIdx = mChildren.Length() - 1;
    } else {
      mChildren.InsertElementAt(aNewIndex - 1, aChild);
      endIdx = aNewIndex;
    }
  } else {
    // The child is moved prior its current position.
    mChildren.InsertElementAt(aNewIndex, aChild);
  }

  for (uint32_t idx = startIdx; idx <= endIdx; idx++) {
    mChildren[idx]->mIndexInParent = idx;
    mChildren[idx]->mIndexOfEmbeddedChild = -1;
  }

  for (uint32_t idx = 0; idx < mChildren.Length(); idx++) {
    mChildren[idx]->mStateFlags |= eGroupInfoDirty;
  }

  RefPtr<AccShowEvent> showEvent = new AccShowEvent(aChild);
  DebugOnly<bool> added = mDoc->Controller()->QueueMutationEvent(showEvent);
  MOZ_ASSERT(added);
  aChild->SetShowEventTarget(true);
}

LocalAccessible* LocalAccessible::LocalChildAt(uint32_t aIndex) const {
  LocalAccessible* child = mChildren.SafeElementAt(aIndex, nullptr);
  if (!child) return nullptr;

#ifdef DEBUG
  LocalAccessible* realParent = child->mParent;
  NS_ASSERTION(!realParent || realParent == this,
               "Two accessibles have the same first child accessible!");
#endif

  return child;
}

uint32_t LocalAccessible::ChildCount() const { return mChildren.Length(); }

int32_t LocalAccessible::IndexInParent() const { return mIndexInParent; }

uint32_t LocalAccessible::EmbeddedChildCount() {
  if (mStateFlags & eHasTextKids) {
    if (!mEmbeddedObjCollector) {
      mEmbeddedObjCollector.reset(new EmbeddedObjCollector(this));
    }
    return mEmbeddedObjCollector->Count();
  }

  return ChildCount();
}

LocalAccessible* LocalAccessible::GetEmbeddedChildAt(uint32_t aIndex) {
  if (mStateFlags & eHasTextKids) {
    if (!mEmbeddedObjCollector) {
      mEmbeddedObjCollector.reset(new EmbeddedObjCollector(this));
    }
    return mEmbeddedObjCollector.get()
               ? mEmbeddedObjCollector->GetAccessibleAt(aIndex)
               : nullptr;
  }

  return LocalChildAt(aIndex);
}

int32_t LocalAccessible::GetIndexOfEmbeddedChild(LocalAccessible* aChild) {
  if (mStateFlags & eHasTextKids) {
    if (!mEmbeddedObjCollector) {
      mEmbeddedObjCollector.reset(new EmbeddedObjCollector(this));
    }
    return mEmbeddedObjCollector.get()
               ? mEmbeddedObjCollector->GetIndexAt(aChild)
               : -1;
  }

  return GetIndexOf(aChild);
}

////////////////////////////////////////////////////////////////////////////////
// HyperLinkAccessible methods

bool LocalAccessible::IsLink() const {
  // Every embedded accessible within hypertext accessible implements
  // hyperlink interface.
  return mParent && mParent->IsHyperText() && !IsText();
}

uint32_t LocalAccessible::StartOffset() {
  MOZ_ASSERT(IsLink(), "StartOffset is called not on hyper link!");

  HyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nullptr;
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

uint32_t LocalAccessible::EndOffset() {
  MOZ_ASSERT(IsLink(), "EndOffset is called on not hyper link!");

  HyperTextAccessible* hyperText = mParent ? mParent->AsHyperText() : nullptr;
  return hyperText ? (hyperText->GetChildOffset(this) + 1) : 0;
}

uint32_t LocalAccessible::AnchorCount() {
  MOZ_ASSERT(IsLink(), "AnchorCount is called on not hyper link!");
  return 1;
}

LocalAccessible* LocalAccessible::AnchorAt(uint32_t aAnchorIndex) {
  MOZ_ASSERT(IsLink(), "GetAnchor is called on not hyper link!");
  return aAnchorIndex == 0 ? this : nullptr;
}

already_AddRefed<nsIURI> LocalAccessible::AnchorURIAt(
    uint32_t aAnchorIndex) const {
  MOZ_ASSERT(IsLink(), "AnchorURIAt is called on not hyper link!");
  return nullptr;
}

void LocalAccessible::ToTextPoint(HyperTextAccessible** aContainer,
                                  int32_t* aOffset, bool aIsBefore) const {
  if (IsHyperText()) {
    *aContainer = const_cast<LocalAccessible*>(this)->AsHyperText();
    *aOffset = aIsBefore ? 0 : (*aContainer)->CharacterCount();
    return;
  }

  const LocalAccessible* child = nullptr;
  const LocalAccessible* parent = this;
  do {
    child = parent;
    parent = parent->LocalParent();
  } while (parent && !parent->IsHyperText());

  if (parent) {
    *aContainer = const_cast<LocalAccessible*>(parent)->AsHyperText();
    *aOffset = (*aContainer)
                   ->GetChildOffset(child->IndexInParent() +
                                    static_cast<int32_t>(!aIsBefore));
  }
}

////////////////////////////////////////////////////////////////////////////////
// SelectAccessible

void LocalAccessible::SelectedItems(nsTArray<LocalAccessible*>* aItems) {
  AccIterator iter(this, filters::GetSelected);
  LocalAccessible* selected = nullptr;
  while ((selected = iter.Next())) aItems->AppendElement(selected);
}

uint32_t LocalAccessible::SelectedItemCount() {
  uint32_t count = 0;
  AccIterator iter(this, filters::GetSelected);
  LocalAccessible* selected = nullptr;
  while ((selected = iter.Next())) ++count;

  return count;
}

LocalAccessible* LocalAccessible::GetSelectedItem(uint32_t aIndex) {
  AccIterator iter(this, filters::GetSelected);
  LocalAccessible* selected = nullptr;

  uint32_t index = 0;
  while ((selected = iter.Next()) && index < aIndex) index++;

  return selected;
}

bool LocalAccessible::IsItemSelected(uint32_t aIndex) {
  uint32_t index = 0;
  AccIterator iter(this, filters::GetSelectable);
  LocalAccessible* selected = nullptr;
  while ((selected = iter.Next()) && index < aIndex) index++;

  return selected && selected->State() & states::SELECTED;
}

bool LocalAccessible::AddItemToSelection(uint32_t aIndex) {
  uint32_t index = 0;
  AccIterator iter(this, filters::GetSelectable);
  LocalAccessible* selected = nullptr;
  while ((selected = iter.Next()) && index < aIndex) index++;

  if (selected) selected->SetSelected(true);

  return static_cast<bool>(selected);
}

bool LocalAccessible::RemoveItemFromSelection(uint32_t aIndex) {
  uint32_t index = 0;
  AccIterator iter(this, filters::GetSelectable);
  LocalAccessible* selected = nullptr;
  while ((selected = iter.Next()) && index < aIndex) index++;

  if (selected) selected->SetSelected(false);

  return static_cast<bool>(selected);
}

bool LocalAccessible::SelectAll() {
  bool success = false;
  LocalAccessible* selectable = nullptr;

  AccIterator iter(this, filters::GetSelectable);
  while ((selectable = iter.Next())) {
    success = true;
    selectable->SetSelected(true);
  }
  return success;
}

bool LocalAccessible::UnselectAll() {
  bool success = false;
  LocalAccessible* selected = nullptr;

  AccIterator iter(this, filters::GetSelected);
  while ((selected = iter.Next())) {
    success = true;
    selected->SetSelected(false);
  }
  return success;
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool LocalAccessible::IsWidget() const { return false; }

bool LocalAccessible::IsActiveWidget() const {
  if (FocusMgr()->HasDOMFocus(mContent)) return true;

  // If text entry of combobox widget has a focus then the combobox widget is
  // active.
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && roleMapEntry->Is(nsGkAtoms::combobox)) {
    uint32_t childCount = ChildCount();
    for (uint32_t idx = 0; idx < childCount; idx++) {
      LocalAccessible* child = mChildren.ElementAt(idx);
      if (child->Role() == roles::ENTRY) {
        return FocusMgr()->HasDOMFocus(child->GetContent());
      }
    }
  }

  return false;
}

bool LocalAccessible::AreItemsOperable() const {
  return HasOwnContent() && mContent->IsElement() &&
         mContent->AsElement()->HasAttr(kNameSpaceID_None,
                                        nsGkAtoms::aria_activedescendant);
}

LocalAccessible* LocalAccessible::CurrentItem() const {
  // Check for aria-activedescendant, which changes which element has focus.
  // For activedescendant, the ARIA spec does not require that the user agent
  // checks whether pointed node is actually a DOM descendant of the element
  // with the aria-activedescendant attribute.
  nsAutoString id;
  if (HasOwnContent() && mContent->IsElement() &&
      mContent->AsElement()->GetAttr(kNameSpaceID_None,
                                     nsGkAtoms::aria_activedescendant, id)) {
    dom::Element* activeDescendantElm = IDRefsIterator::GetElem(mContent, id);
    if (activeDescendantElm) {
      if (mContent->IsInclusiveDescendantOf(activeDescendantElm)) {
        // Don't want a cyclical descendant relationship. That would be bad.
        return nullptr;
      }

      DocAccessible* document = Document();
      if (document) return document->GetAccessible(activeDescendantElm);
    }
  }
  return nullptr;
}

void LocalAccessible::SetCurrentItem(const LocalAccessible* aItem) {
  nsAtom* id = aItem->GetContent()->GetID();
  if (id) {
    nsAutoString idStr;
    id->ToString(idStr);
    mContent->AsElement()->SetAttr(
        kNameSpaceID_None, nsGkAtoms::aria_activedescendant, idStr, true);
  }
}

LocalAccessible* LocalAccessible::ContainerWidget() const {
  if (HasARIARole() && mContent->HasID()) {
    for (LocalAccessible* parent = LocalParent(); parent;
         parent = parent->LocalParent()) {
      nsIContent* parentContent = parent->GetContent();
      if (parentContent && parentContent->IsElement() &&
          parentContent->AsElement()->HasAttr(
              kNameSpaceID_None, nsGkAtoms::aria_activedescendant)) {
        return parent;
      }

      // Don't cross DOM document boundaries.
      if (parent->IsDoc()) break;
    }
  }
  return nullptr;
}

void LocalAccessible::Announce(const nsAString& aAnnouncement,
                               uint16_t aPriority) {
  RefPtr<AccAnnouncementEvent> event =
      new AccAnnouncementEvent(this, aAnnouncement, aPriority);
  nsEventShell::FireEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible protected methods

void LocalAccessible::LastRelease() {
  // First cleanup if needed...
  if (mDoc) {
    Shutdown();
    NS_ASSERTION(!mDoc,
                 "A Shutdown() impl forgot to call its parent's Shutdown?");
  }
  // ... then die.
  delete this;
}

LocalAccessible* LocalAccessible::GetSiblingAtOffset(int32_t aOffset,
                                                     nsresult* aError) const {
  if (!mParent || mIndexInParent == -1) {
    if (aError) *aError = NS_ERROR_UNEXPECTED;

    return nullptr;
  }

  if (aError &&
      mIndexInParent + aOffset >= static_cast<int32_t>(mParent->ChildCount())) {
    *aError = NS_OK;  // fail peacefully
    return nullptr;
  }

  LocalAccessible* child = mParent->LocalChildAt(mIndexInParent + aOffset);
  if (aError && !child) *aError = NS_ERROR_UNEXPECTED;

  return child;
}

void LocalAccessible::ModifySubtreeContextFlags(uint32_t aContextFlags,
                                                bool aAdd) {
  Pivot pivot(this);
  LocalAccInSameDocRule rule;
  for (Accessible* anchor = this; anchor; anchor = pivot.Next(anchor, rule)) {
    MOZ_ASSERT(anchor->IsLocal());
    LocalAccessible* acc = anchor->AsLocal();
    if (aAdd) {
      acc->mContextFlags |= aContextFlags;
    } else {
      acc->mContextFlags &= ~aContextFlags;
    }
  }
}

double LocalAccessible::AttrNumericValue(nsAtom* aAttr) const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->valueRule == eNoValue) {
    return UnspecifiedNaN<double>();
  }

  nsAutoString attrValue;
  if (!mContent->IsElement() ||
      !mContent->AsElement()->GetAttr(kNameSpaceID_None, aAttr, attrValue)) {
    return UnspecifiedNaN<double>();
  }

  nsresult error = NS_OK;
  double value = attrValue.ToDouble(&error);
  return NS_FAILED(error) ? UnspecifiedNaN<double>() : value;
}

uint32_t LocalAccessible::GetActionRule() const {
  if (!HasOwnContent() || (InteractiveState() & states::UNAVAILABLE)) {
    return eNoAction;
  }

  // Return "click" action on elements that have an attached popup menu.
  if (mContent->IsXULElement()) {
    if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::popup)) {
      return eClickAction;
    }
  }

  // Has registered 'click' event handler.
  bool isOnclick = nsCoreUtils::HasClickListener(mContent);

  if (isOnclick) return eClickAction;

  // Get an action based on ARIA role.
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && roleMapEntry->actionRule != eNoAction) {
    return roleMapEntry->actionRule;
  }

  // Get an action based on ARIA attribute.
  if (nsAccUtils::HasDefinedARIAToken(mContent, nsGkAtoms::aria_expanded)) {
    return eExpandAction;
  }

  return eNoAction;
}

AccGroupInfo* LocalAccessible::GetGroupInfo() const {
  if (IsProxy()) MOZ_CRASH("This should never be called on proxy wrappers");

  if (mBits.groupInfo) {
    if (HasDirtyGroupInfo()) {
      mBits.groupInfo->Update();
      mStateFlags &= ~eGroupInfoDirty;
    }

    return mBits.groupInfo;
  }

  mBits.groupInfo = AccGroupInfo::CreateGroupInfo(this);
  mStateFlags &= ~eGroupInfoDirty;
  return mBits.groupInfo;
}

already_AddRefed<AccAttributes> LocalAccessible::BundleFieldsForCache() {
  RefPtr<AccAttributes> fields = new AccAttributes();
  nsAutoString name;
  int32_t nameFlag = Name(name);
  fields->SetAttribute(nsGkAtoms::explicit_name, nameFlag);
  fields->SetAttribute(nsGkAtoms::name, name);

  return fields.forget();
}

void LocalAccessible::MaybeFireFocusableStateChange(bool aPreviouslyFocusable) {
  bool isFocusable = (State() & states::FOCUSABLE);
  if (isFocusable != aPreviouslyFocusable) {
    RefPtr<AccEvent> focusableChangeEvent =
        new AccStateChangeEvent(this, states::FOCUSABLE, isFocusable);
    mDoc->FireDelayedEvent(focusableChangeEvent);
  }
}

void LocalAccessible::GetPositionAndSizeInternal(int32_t* aPosInSet,
                                                 int32_t* aSetSize) {
  AccGroupInfo* groupInfo = GetGroupInfo();
  if (groupInfo) {
    *aPosInSet = groupInfo->PosInSet();
    *aSetSize = groupInfo->SetSize();
  }
}

int32_t LocalAccessible::GetLevelInternal() {
  int32_t level = nsAccUtils::GetDefaultLevel(this);

  if (!IsBoundToParent()) return level;

  roles::Role role = Role();
  if (role == roles::OUTLINEITEM) {
    // Always expose 'level' attribute for 'outlineitem' accessible. The number
    // of nested 'grouping' accessibles containing 'outlineitem' accessible is
    // its level.
    level = 1;

    LocalAccessible* parent = this;
    while ((parent = parent->LocalParent())) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::OUTLINE) break;
      if (parentRole == roles::GROUPING) ++level;
    }

  } else if (role == roles::LISTITEM) {
    // Expose 'level' attribute on nested lists. We support two hierarchies:
    // a) list -> listitem -> list -> listitem (nested list is a last child
    //   of listitem of the parent list);
    // b) list -> listitem -> group -> listitem (nested listitems are contained
    //   by group that is a last child of the parent listitem).

    // Calculate 'level' attribute based on number of parent listitems.
    level = 0;
    LocalAccessible* parent = this;
    while ((parent = parent->LocalParent())) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::LISTITEM) {
        ++level;
      } else if (parentRole != roles::LIST && parentRole != roles::GROUPING) {
        break;
      }
    }

    if (level == 0) {
      // If this listitem is on top of nested lists then expose 'level'
      // attribute.
      parent = LocalParent();
      uint32_t siblingCount = parent->ChildCount();
      for (uint32_t siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        LocalAccessible* sibling = parent->LocalChildAt(siblingIdx);

        LocalAccessible* siblingChild = sibling->LocalLastChild();
        if (siblingChild) {
          roles::Role lastChildRole = siblingChild->Role();
          if (lastChildRole == roles::LIST ||
              lastChildRole == roles::GROUPING) {
            return 1;
          }
        }
      }
    } else {
      ++level;  // level is 1-index based
    }
  } else if (role == roles::COMMENT) {
    // For comments, count the ancestor elements with the same role to get the
    // level.
    level = 1;

    LocalAccessible* parent = this;
    while ((parent = parent->LocalParent())) {
      roles::Role parentRole = parent->Role();
      if (parentRole == roles::COMMENT) {
        ++level;
      }
    }
  }

  return level;
}

void LocalAccessible::StaticAsserts() const {
  static_assert(
      eLastStateFlag <= (1 << kStateFlagsBits) - 1,
      "LocalAccessible::mStateFlags was oversized by eLastStateFlag!");
  static_assert(
      eLastContextFlag <= (1 << kContextFlagsBits) - 1,
      "LocalAccessible::mContextFlags was oversized by eLastContextFlag!");
}

////////////////////////////////////////////////////////////////////////////////
// KeyBinding class

// static
uint32_t KeyBinding::AccelModifier() {
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

void KeyBinding::ToPlatformFormat(nsAString& aValue) const {
  nsCOMPtr<nsIStringBundle> keyStringBundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      mozilla::components::StringBundle::Service();
  if (stringBundleService) {
    stringBundleService->CreateBundle(
        "chrome://global-platform/locale/platformKeys.properties",
        getter_AddRefs(keyStringBundle));
  }

  if (!keyStringBundle) return;

  nsAutoString separator;
  keyStringBundle->GetStringFromName("MODIFIER_SEPARATOR", separator);

  nsAutoString modifierName;
  if (mModifierMask & kControl) {
    keyStringBundle->GetStringFromName("VK_CONTROL", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kAlt) {
    keyStringBundle->GetStringFromName("VK_ALT", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kShift) {
    keyStringBundle->GetStringFromName("VK_SHIFT", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kMeta) {
    keyStringBundle->GetStringFromName("VK_META", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  aValue.Append(mKey);
}

void KeyBinding::ToAtkFormat(nsAString& aValue) const {
  nsAutoString modifierName;
  if (mModifierMask & kControl) aValue.AppendLiteral("<Control>");

  if (mModifierMask & kAlt) aValue.AppendLiteral("<Alt>");

  if (mModifierMask & kShift) aValue.AppendLiteral("<Shift>");

  if (mModifierMask & kMeta) aValue.AppendLiteral("<Meta>");

  aValue.Append(mKey);
}
