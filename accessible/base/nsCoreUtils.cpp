/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCoreUtils.h"

#include "nsAttrValue.h"
#include "nsIAccessibleTypes.h"

#include "mozilla/dom/Document.h"
#include "nsAccUtils.h"
#include "nsRange.h"
#include "nsXULElement.h"
#include "nsIDocShell.h"
#include "nsIObserverService.h"
#include "nsPresContext.h"
#include "nsIScrollableFrame.h"
#include "nsISelectionController.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/TouchEvents.h"
#include "nsView.h"
#include "nsGkAtoms.h"

#include "nsComponentManagerUtils.h"

#include "XULTreeElement.h"
#include "nsIContentInlines.h"
#include "nsTreeColumns.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLLabelElement.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/Selection.h"

using namespace mozilla;

using mozilla::dom::DOMRect;
using mozilla::dom::Element;
using mozilla::dom::Selection;
using mozilla::dom::XULTreeElement;

using mozilla::a11y::nsAccUtils;

////////////////////////////////////////////////////////////////////////////////
// nsCoreUtils
////////////////////////////////////////////////////////////////////////////////

bool nsCoreUtils::IsLabelWithControl(nsIContent* aContent) {
  dom::HTMLLabelElement* label = dom::HTMLLabelElement::FromNode(aContent);
  if (label && label->GetControl()) return true;

  return false;
}

bool nsCoreUtils::HasClickListener(nsIContent* aContent) {
  NS_ENSURE_TRUE(aContent, false);
  EventListenerManager* listenerManager =
      aContent->GetExistingListenerManager();

  return listenerManager &&
         (listenerManager->HasListenersFor(nsGkAtoms::onclick) ||
          listenerManager->HasListenersFor(nsGkAtoms::onmousedown) ||
          listenerManager->HasListenersFor(nsGkAtoms::onmouseup));
}

void nsCoreUtils::DispatchClickEvent(XULTreeElement* aTree, int32_t aRowIndex,
                                     nsTreeColumn* aColumn,
                                     const nsAString& aPseudoElt) {
  RefPtr<dom::Element> tcElm = aTree->GetTreeBody();
  if (!tcElm) return;

  Document* document = tcElm->GetUncomposedDoc();
  if (!document) return;

  RefPtr<PresShell> presShell = document->GetPresShell();
  if (!presShell) {
    return;
  }

  // Ensure row is visible.
  aTree->EnsureRowIsVisible(aRowIndex);

  // Calculate x and y coordinates.
  nsresult rv;
  nsIntRect rect =
      aTree->GetCoordsForCellItem(aRowIndex, aColumn, aPseudoElt, rv);
  if (NS_FAILED(rv)) {
    return;
  }

  RefPtr<DOMRect> treeBodyRect = tcElm->GetBoundingClientRect();
  int32_t tcX = (int32_t)treeBodyRect->X();
  int32_t tcY = (int32_t)treeBodyRect->Y();

  // Dispatch mouse events.
  AutoWeakFrame tcFrame = tcElm->GetPrimaryFrame();
  nsIFrame* rootFrame = presShell->GetRootFrame();

  nsPoint offset;
  nsCOMPtr<nsIWidget> rootWidget =
      rootFrame->GetView()->GetNearestWidget(&offset);

  RefPtr<nsPresContext> presContext = presShell->GetPresContext();

  int32_t cnvdX = presContext->CSSPixelsToDevPixels(tcX + int32_t(rect.x) + 1) +
                  presContext->AppUnitsToDevPixels(offset.x);
  int32_t cnvdY = presContext->CSSPixelsToDevPixels(tcY + int32_t(rect.y) + 1) +
                  presContext->AppUnitsToDevPixels(offset.y);

  // XUL is just desktop, so there is no real reason for senfing touch events.
  DispatchMouseEvent(eMouseDown, cnvdX, cnvdY, tcElm, tcFrame, presShell,
                     rootWidget);

  DispatchMouseEvent(eMouseUp, cnvdX, cnvdY, tcElm, tcFrame, presShell,
                     rootWidget);
}

void nsCoreUtils::DispatchMouseEvent(EventMessage aMessage, int32_t aX,
                                     int32_t aY, nsIContent* aContent,
                                     nsIFrame* aFrame, PresShell* aPresShell,
                                     nsIWidget* aRootWidget) {
  WidgetMouseEvent event(true, aMessage, aRootWidget, WidgetMouseEvent::eReal,
                         WidgetMouseEvent::eNormal);

  event.mRefPoint = LayoutDeviceIntPoint(aX, aY);

  event.mClickCount = 1;
  event.mButton = MouseButton::ePrimary;
  event.mInputSource = dom::MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;

  nsEventStatus status = nsEventStatus_eIgnore;
  aPresShell->HandleEventWithTarget(&event, aFrame, aContent, &status);
}

void nsCoreUtils::DispatchTouchEvent(EventMessage aMessage, int32_t aX,
                                     int32_t aY, nsIContent* aContent,
                                     nsIFrame* aFrame, PresShell* aPresShell,
                                     nsIWidget* aRootWidget) {
  nsIDocShell* docShell = nullptr;
  if (aPresShell->GetDocument()) {
    docShell = aPresShell->GetDocument()->GetDocShell();
  }
  if (!dom::TouchEvent::PrefEnabled(docShell)) {
    return;
  }

  WidgetTouchEvent event(true, aMessage, aRootWidget);

  // XXX: Touch has an identifier of -1 to hint that it is synthesized.
  RefPtr<dom::Touch> t = new dom::Touch(-1, LayoutDeviceIntPoint(aX, aY),
                                        LayoutDeviceIntPoint(1, 1), 0.0f, 1.0f);
  t->SetTouchTarget(aContent);
  event.mTouches.AppendElement(t);
  nsEventStatus status = nsEventStatus_eIgnore;
  aPresShell->HandleEventWithTarget(&event, aFrame, aContent, &status);
}

uint32_t nsCoreUtils::GetAccessKeyFor(nsIContent* aContent) {
  // Accesskeys are registered by @accesskey attribute only. At first check
  // whether it is presented on the given element to avoid the slow
  // EventStateManager::GetRegisteredAccessKey() method.
  if (!aContent->IsElement() || !aContent->AsElement()->HasAttr(
                                    kNameSpaceID_None, nsGkAtoms::accesskey)) {
    return 0;
  }

  nsPresContext* presContext = aContent->OwnerDoc()->GetPresContext();
  if (!presContext) return 0;

  EventStateManager* esm = presContext->EventStateManager();
  if (!esm) return 0;

  return esm->GetRegisteredAccessKey(aContent->AsElement());
}

nsIContent* nsCoreUtils::GetDOMElementFor(nsIContent* aContent) {
  if (aContent->IsElement()) return aContent;

  if (aContent->IsText()) return aContent->GetFlattenedTreeParent();

  return nullptr;
}

nsINode* nsCoreUtils::GetDOMNodeFromDOMPoint(nsINode* aNode, uint32_t aOffset) {
  if (aNode && aNode->IsElement()) {
    uint32_t childCount = aNode->GetChildCount();
    NS_ASSERTION(aOffset <= childCount, "Wrong offset of the DOM point!");

    // The offset can be after last child of container node that means DOM point
    // is placed immediately after the last child. In this case use the DOM node
    // from the given DOM point is used as result node.
    if (aOffset != childCount) return aNode->GetChildAt_Deprecated(aOffset);
  }

  return aNode;
}

bool nsCoreUtils::IsAncestorOf(nsINode* aPossibleAncestorNode,
                               nsINode* aPossibleDescendantNode,
                               nsINode* aRootNode) {
  NS_ENSURE_TRUE(aPossibleAncestorNode && aPossibleDescendantNode, false);

  nsINode* parentNode = aPossibleDescendantNode;
  while ((parentNode = parentNode->GetParentNode()) &&
         parentNode != aRootNode) {
    if (parentNode == aPossibleAncestorNode) return true;
  }

  return false;
}

nsresult nsCoreUtils::ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                                        uint32_t aScrollType) {
  ScrollAxis vertical, horizontal;
  ConvertScrollTypeToPercents(aScrollType, &vertical, &horizontal);

  return ScrollSubstringTo(aFrame, aRange, vertical, horizontal);
}

nsresult nsCoreUtils::ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                                        ScrollAxis aVertical,
                                        ScrollAxis aHorizontal) {
  if (!aFrame || !aRange) {
    return NS_ERROR_FAILURE;
  }

  nsPresContext* presContext = aFrame->PresContext();

  nsCOMPtr<nsISelectionController> selCon;
  aFrame->GetSelectionController(presContext, getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  RefPtr<dom::Selection> selection =
      selCon->GetSelection(nsISelectionController::SELECTION_ACCESSIBILITY);

  selection->RemoveAllRanges(IgnoreErrors());
  selection->AddRangeAndSelectFramesAndNotifyListeners(*aRange, IgnoreErrors());

  selection->ScrollIntoView(nsISelectionController::SELECTION_ANCHOR_REGION,
                            aVertical, aHorizontal,
                            Selection::SCROLL_SYNCHRONOUS);

  selection->CollapseToStart(IgnoreErrors());

  return NS_OK;
}

void nsCoreUtils::ScrollFrameToPoint(nsIFrame* aScrollableFrame,
                                     nsIFrame* aFrame,
                                     const LayoutDeviceIntPoint& aPoint) {
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(aScrollableFrame);
  if (!scrollableFrame) return;

  nsPoint point = LayoutDeviceIntPoint::ToAppUnits(
      aPoint, aFrame->PresContext()->AppUnitsPerDevPixel());
  nsRect frameRect = aFrame->GetScreenRectInAppUnits();
  nsPoint deltaPoint = point - frameRect.TopLeft();

  nsPoint scrollPoint = scrollableFrame->GetScrollPosition();
  scrollPoint -= deltaPoint;

  scrollableFrame->ScrollTo(scrollPoint, ScrollMode::Instant);
}

void nsCoreUtils::ConvertScrollTypeToPercents(uint32_t aScrollType,
                                              ScrollAxis* aVertical,
                                              ScrollAxis* aHorizontal) {
  WhereToScroll whereY, whereX;
  WhenToScroll whenY, whenX;
  switch (aScrollType) {
    case nsIAccessibleScrollType::SCROLL_TYPE_TOP_LEFT:
      whereY = WhereToScroll::Start;
      whenY = WhenToScroll::Always;
      whereX = WhereToScroll::Start;
      whenX = WhenToScroll::Always;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_RIGHT:
      whereY = WhereToScroll::End;
      whenY = WhenToScroll::Always;
      whereX = WhereToScroll::End;
      whenX = WhenToScroll::Always;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE:
      whereY = WhereToScroll::Start;
      whenY = WhenToScroll::Always;
      whereX = WhereToScroll::Nearest;
      whenX = WhenToScroll::IfNotFullyVisible;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_EDGE:
      whereY = WhereToScroll::End;
      whenY = WhenToScroll::Always;
      whereX = WhereToScroll::Nearest;
      whenX = WhenToScroll::IfNotFullyVisible;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_LEFT_EDGE:
      whereY = WhereToScroll::Nearest;
      whenY = WhenToScroll::IfNotFullyVisible;
      whereX = WhereToScroll::Start;
      whenX = WhenToScroll::Always;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_RIGHT_EDGE:
      whereY = WhereToScroll::Nearest;
      whenY = WhenToScroll::IfNotFullyVisible;
      whereX = WhereToScroll::End;
      whenX = WhenToScroll::Always;
      break;
    default:
      whereY = WhereToScroll::Center;
      whenY = WhenToScroll::IfNotFullyVisible;
      whereX = WhereToScroll::Center;
      whenX = WhenToScroll::IfNotFullyVisible;
  }
  *aVertical = ScrollAxis(whereY, whenY);
  *aHorizontal = ScrollAxis(whereX, whenX);
}

already_AddRefed<nsIDocShell> nsCoreUtils::GetDocShellFor(nsINode* aNode) {
  if (!aNode) return nullptr;

  nsCOMPtr<nsIDocShell> docShell = aNode->OwnerDoc()->GetDocShell();
  return docShell.forget();
}

bool nsCoreUtils::IsRootDocument(Document* aDocument) {
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = aDocument->GetDocShell();
  NS_ASSERTION(docShellTreeItem, "No document shell for document!");

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  docShellTreeItem->GetInProcessParent(getter_AddRefs(parentTreeItem));

  return !parentTreeItem;
}

bool nsCoreUtils::IsTopLevelContentDocInProcess(Document* aDocumentNode) {
  mozilla::dom::BrowsingContext* bc = aDocumentNode->GetBrowsingContext();
  return bc->IsContent() && (
                                // Tab document.
                                bc->IsTop() ||
                                // Out-of-process iframe.
                                !bc->GetParent()->IsInProcess());
}

bool nsCoreUtils::IsErrorPage(Document* aDocument) {
  nsIURI* uri = aDocument->GetDocumentURI();
  if (!uri->SchemeIs("about")) {
    return false;
  }

  nsAutoCString path;
  uri->GetPathQueryRef(path);

  constexpr auto neterror = "neterror"_ns;
  constexpr auto certerror = "certerror"_ns;

  return StringBeginsWith(path, neterror) || StringBeginsWith(path, certerror);
}

PresShell* nsCoreUtils::GetPresShellFor(nsINode* aNode) {
  return aNode->OwnerDoc()->GetPresShell();
}

bool nsCoreUtils::GetID(nsIContent* aContent, nsAString& aID) {
  return aContent->IsElement() &&
         aContent->AsElement()->GetAttr(nsGkAtoms::id, aID);
}

bool nsCoreUtils::GetUIntAttr(nsIContent* aContent, nsAtom* aAttr,
                              int32_t* aUInt) {
  if (!aContent->IsElement()) {
    return false;
  }
  return GetUIntAttrValue(nsAccUtils::GetARIAAttr(aContent->AsElement(), aAttr),
                          aUInt);
}

bool nsCoreUtils::GetUIntAttrValue(const nsAttrValue* aVal, int32_t* aUInt) {
  if (!aVal) {
    return false;
  }
  nsAutoString value;
  aVal->ToString(value);
  if (!value.IsEmpty()) {
    nsresult error = NS_OK;
    int32_t integer = value.ToInteger(&error);
    if (NS_SUCCEEDED(error) && integer > 0) {
      *aUInt = integer;
      return true;
    }
  }

  return false;
}

void nsCoreUtils::GetLanguageFor(nsIContent* aContent, nsIContent* aRootContent,
                                 nsAString& aLanguage) {
  aLanguage.Truncate();

  nsIContent* walkUp = aContent;
  while (walkUp && walkUp != aRootContent &&
         (!walkUp->IsElement() ||
          !walkUp->AsElement()->GetAttr(nsGkAtoms::lang, aLanguage))) {
    walkUp = walkUp->GetParent();
  }
}

XULTreeElement* nsCoreUtils::GetTree(nsIContent* aContent) {
  // Find DOMNode's parents recursively until reach the <tree> tag
  nsIContent* currentContent = aContent;
  while (currentContent) {
    if (currentContent->NodeInfo()->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
      return XULTreeElement::FromNode(currentContent);
    }
    currentContent = currentContent->GetFlattenedTreeParent();
  }

  return nullptr;
}

already_AddRefed<nsTreeColumn> nsCoreUtils::GetFirstSensibleColumn(
    XULTreeElement* aTree, FlushType aFlushType) {
  if (!aTree) {
    return nullptr;
  }

  RefPtr<nsTreeColumns> cols = aTree->GetColumns(aFlushType);
  if (!cols) {
    return nullptr;
  }

  RefPtr<nsTreeColumn> column = cols->GetFirstColumn();
  if (column && IsColumnHidden(column)) return GetNextSensibleColumn(column);

  return column.forget();
}

uint32_t nsCoreUtils::GetSensibleColumnCount(XULTreeElement* aTree) {
  uint32_t count = 0;
  if (!aTree) {
    return count;
  }

  RefPtr<nsTreeColumns> cols = aTree->GetColumns();
  if (!cols) {
    return count;
  }

  nsTreeColumn* column = cols->GetFirstColumn();

  while (column) {
    if (!IsColumnHidden(column)) count++;

    column = column->GetNext();
  }

  return count;
}

already_AddRefed<nsTreeColumn> nsCoreUtils::GetSensibleColumnAt(
    XULTreeElement* aTree, uint32_t aIndex) {
  if (!aTree) {
    return nullptr;
  }

  uint32_t idx = aIndex;

  nsCOMPtr<nsTreeColumn> column = GetFirstSensibleColumn(aTree);
  while (column) {
    if (idx == 0) return column.forget();

    idx--;
    column = GetNextSensibleColumn(column);
  }

  return nullptr;
}

already_AddRefed<nsTreeColumn> nsCoreUtils::GetNextSensibleColumn(
    nsTreeColumn* aColumn) {
  if (!aColumn) {
    return nullptr;
  }

  RefPtr<nsTreeColumn> nextColumn = aColumn->GetNext();

  while (nextColumn && IsColumnHidden(nextColumn)) {
    nextColumn = nextColumn->GetNext();
  }

  return nextColumn.forget();
}

already_AddRefed<nsTreeColumn> nsCoreUtils::GetPreviousSensibleColumn(
    nsTreeColumn* aColumn) {
  if (!aColumn) {
    return nullptr;
  }

  RefPtr<nsTreeColumn> prevColumn = aColumn->GetPrevious();

  while (prevColumn && IsColumnHidden(prevColumn)) {
    prevColumn = prevColumn->GetPrevious();
  }

  return prevColumn.forget();
}

bool nsCoreUtils::IsColumnHidden(nsTreeColumn* aColumn) {
  if (!aColumn) {
    return false;
  }

  Element* element = aColumn->Element();
  return element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                              nsGkAtoms::_true, eCaseMatters);
}

void nsCoreUtils::ScrollTo(PresShell* aPresShell, nsIContent* aContent,
                           uint32_t aScrollType) {
  ScrollAxis vertical, horizontal;
  ConvertScrollTypeToPercents(aScrollType, &vertical, &horizontal);
  aPresShell->ScrollContentIntoView(aContent, vertical, horizontal,
                                    ScrollFlags::ScrollOverflowHidden);
}

bool nsCoreUtils::IsHTMLTableHeader(nsIContent* aContent) {
  return aContent->NodeInfo()->Equals(nsGkAtoms::th) ||
         (aContent->IsElement() &&
          aContent->AsElement()->HasAttr(nsGkAtoms::scope));
}

bool nsCoreUtils::IsWhitespaceString(const nsAString& aString) {
  nsAString::const_char_iterator iterBegin, iterEnd;

  aString.BeginReading(iterBegin);
  aString.EndReading(iterEnd);

  while (iterBegin != iterEnd && IsWhitespace(*iterBegin)) ++iterBegin;

  return iterBegin == iterEnd;
}

bool nsCoreUtils::AccEventObserversExist() {
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  NS_ENSURE_TRUE(obsService, false);

  nsCOMPtr<nsISimpleEnumerator> observers;
  obsService->EnumerateObservers(NS_ACCESSIBLE_EVENT_TOPIC,
                                 getter_AddRefs(observers));
  NS_ENSURE_TRUE(observers, false);

  bool hasObservers = false;
  observers->HasMoreElements(&hasObservers);

  return hasObservers;
}

void nsCoreUtils::DispatchAccEvent(RefPtr<nsIAccessibleEvent> event) {
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obsService);

  obsService->NotifyObservers(event, NS_ACCESSIBLE_EVENT_TOPIC, nullptr);
}

bool nsCoreUtils::IsDisplayContents(nsIContent* aContent) {
  auto* element = Element::FromNodeOrNull(aContent);
  return element && element->IsDisplayContents();
}

bool nsCoreUtils::CanCreateAccessibleWithoutFrame(nsIContent* aContent) {
  auto* element = Element::FromNodeOrNull(aContent);
  if (!element) {
    return false;
  }
  if (!element->HasServoData() || Servo_Element_IsDisplayNone(element)) {
    // Out of the flat tree or in a display: none subtree.
    return false;
  }

  // If we aren't display: contents or option/optgroup we can't create an
  // accessible without frame. Our select combobox code relies on the latter.
  if (!element->IsDisplayContents() &&
      !element->IsAnyOfHTMLElements(nsGkAtoms::option, nsGkAtoms::optgroup)) {
    return false;
  }

  // Even if we're display: contents or optgroups, we might not be able to
  // create an accessible if we're in a content-visibility: hidden subtree.
  //
  // To check that, find the closest ancestor element with a frame.
  for (nsINode* ancestor = element->GetFlattenedTreeParentNode();
       ancestor && ancestor->IsContent();
       ancestor = ancestor->GetFlattenedTreeParentNode()) {
    if (nsIFrame* f = ancestor->AsContent()->GetPrimaryFrame()) {
      if (f->HidesContent(nsIFrame::IncludeContentVisibility::Hidden) ||
          f->IsHiddenByContentVisibilityOnAnyAncestor(
              nsIFrame::IncludeContentVisibility::Hidden)) {
        return false;
      }
      break;
    }
  }

  return true;
}

bool nsCoreUtils::IsDocumentVisibleConsideringInProcessAncestors(
    const Document* aDocument) {
  const Document* parent = aDocument;
  do {
    if (!parent->IsVisible()) {
      return false;
    }
  } while ((parent = parent->GetInProcessParentDocument()));
  return true;
}
