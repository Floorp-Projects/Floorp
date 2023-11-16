/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditorEventListener.h"

#include "HTMLEditUtils.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/Selection.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsQueryObject.h"
#include "nsRange.h"

namespace mozilla {

using namespace dom;

nsresult HTMLEditorEventListener::Connect(EditorBase* aEditorBase) {
  // Guarantee that mEditorBase is always HTMLEditor.
  HTMLEditor* htmlEditor = HTMLEditor::GetFrom(aEditorBase);
  if (NS_WARN_IF(!htmlEditor)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = EditorEventListener::Connect(htmlEditor);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorEventListener::Connect() failed");
  return rv;
}

void HTMLEditorEventListener::Disconnect() {
  if (DetachedFromEditor()) {
    EditorEventListener::Disconnect();
  }

  if (mListeningToMouseMoveEventForResizers) {
    DebugOnly<nsresult> rvIgnored = ListenToMouseMoveEventForResizers(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditorEventListener::ListenToMouseMoveEventForResizers() failed, "
        "but ignored");
  }
  if (mListeningToMouseMoveEventForGrabber) {
    DebugOnly<nsresult> rvIgnored = ListenToMouseMoveEventForGrabber(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditorEventListener::ListenToMouseMoveEventForGrabber() failed, "
        "but ignored");
  }
  if (mListeningToResizeEvent) {
    DebugOnly<nsresult> rvIgnored = ListenToWindowResizeEvent(false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditorEventListener::ListenToWindowResizeEvent() failed, "
        "but ignored");
  }

  EditorEventListener::Disconnect();
}

NS_IMETHODIMP HTMLEditorEventListener::HandleEvent(Event* aEvent) {
  switch (aEvent->WidgetEventPtr()->mMessage) {
    case eMouseMove: {
      if (DetachedFromEditor()) {
        return NS_OK;
      }

      RefPtr<MouseEvent> mouseMoveEvent = aEvent->AsMouseEvent();
      if (NS_WARN_IF(!aEvent->WidgetEventPtr())) {
        return NS_ERROR_FAILURE;
      }

      RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
      DebugOnly<nsresult> rvIgnored =
          htmlEditor->UpdateResizerOrGrabberPositionTo(CSSIntPoint(
              mouseMoveEvent->ClientX(), mouseMoveEvent->ClientY()));
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::UpdateResizerOrGrabberPositionTo() failed, but ignored");
      return NS_OK;
    }
    case eResize: {
      if (DetachedFromEditor()) {
        return NS_OK;
      }

      RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
      nsresult rv = htmlEditor->RefreshResizers();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::RefreshResizers() failed");
      return rv;
    }
    default: {
      nsresult rv = EditorEventListener::HandleEvent(aEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorEventListener::HandleEvent() failed");
      return rv;
    }
  }
}

nsresult HTMLEditorEventListener::ListenToMouseMoveEventForResizersOrGrabber(
    bool aListen, bool aForGrabber) {
  MOZ_ASSERT(aForGrabber ? mListeningToMouseMoveEventForGrabber != aListen
                         : mListeningToMouseMoveEventForResizers != aListen);

  if (NS_WARN_IF(DetachedFromEditor())) {
    return aListen ? NS_ERROR_FAILURE : NS_OK;
  }

  if (aListen) {
    if (aForGrabber && mListeningToMouseMoveEventForResizers) {
      // We've already added mousemove event listener for resizers.
      mListeningToMouseMoveEventForGrabber = true;
      return NS_OK;
    }
    if (!aForGrabber && mListeningToMouseMoveEventForGrabber) {
      // We've already added mousemove event listener for grabber.
      mListeningToMouseMoveEventForResizers = true;
      return NS_OK;
    }
  } else {
    if (aForGrabber && mListeningToMouseMoveEventForResizers) {
      // We need to keep listening to mousemove event listener for resizers.
      mListeningToMouseMoveEventForGrabber = false;
      return NS_OK;
    }
    if (!aForGrabber && mListeningToMouseMoveEventForGrabber) {
      // We need to keep listening to mousemove event listener for grabber.
      mListeningToMouseMoveEventForResizers = false;
      return NS_OK;
    }
  }

  EventTarget* eventTarget = mEditorBase->AsHTMLEditor()->GetDOMEventTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return NS_ERROR_FAILURE;
  }

  // Listen to mousemove events in the system group since web apps may stop
  // propagation before we receive the events.
  EventListenerManager* eventListenerManager =
      eventTarget->GetOrCreateListenerManager();
  if (NS_WARN_IF(!eventListenerManager)) {
    return NS_ERROR_FAILURE;
  }

  if (aListen) {
    eventListenerManager->AddEventListenerByType(
        this, u"mousemove"_ns, TrustedEventsAtSystemGroupBubble());
    if (aForGrabber) {
      mListeningToMouseMoveEventForGrabber = true;
    } else {
      mListeningToMouseMoveEventForResizers = true;
    }
    return NS_OK;
  }

  eventListenerManager->RemoveEventListenerByType(
      this, u"mousemove"_ns, TrustedEventsAtSystemGroupBubble());
  if (aForGrabber) {
    mListeningToMouseMoveEventForGrabber = false;
  } else {
    mListeningToMouseMoveEventForResizers = false;
  }
  return NS_OK;
}

nsresult HTMLEditorEventListener::ListenToWindowResizeEvent(bool aListen) {
  if (mListeningToResizeEvent == aListen) {
    return NS_OK;
  }

  if (DetachedFromEditor()) {
    return aListen ? NS_ERROR_FAILURE : NS_OK;
  }

  Document* document = mEditorBase->AsHTMLEditor()->GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_FAILURE;
  }

  // Document::GetWindow() may return nullptr when HTMLEditor is destroyed
  // while the document is being unloaded.  If we cannot retrieve window as
  // expected, let's ignore it.
  nsPIDOMWindowOuter* window = document->GetWindow();
  if (!window) {
    NS_WARNING_ASSERTION(!aListen,
                         "There should be window when adding event listener");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(window);
  if (NS_WARN_IF(!eventTarget)) {
    return NS_ERROR_FAILURE;
  }

  // Listen to resize events in the system group since web apps may stop
  // propagation before we receive the events.
  EventListenerManager* eventListenerManager =
      eventTarget->GetOrCreateListenerManager();
  if (NS_WARN_IF(!eventListenerManager)) {
    return NS_ERROR_FAILURE;
  }

  if (aListen) {
    eventListenerManager->AddEventListenerByType(
        this, u"resize"_ns, TrustedEventsAtSystemGroupBubble());
    mListeningToResizeEvent = true;
    return NS_OK;
  }

  eventListenerManager->RemoveEventListenerByType(
      this, u"resize"_ns, TrustedEventsAtSystemGroupBubble());
  mListeningToResizeEvent = false;
  return NS_OK;
}

nsresult HTMLEditorEventListener::MouseUp(MouseEvent* aMouseEvent) {
  MOZ_ASSERT(aMouseEvent);
  MOZ_ASSERT(aMouseEvent->IsTrusted());

  if (DetachedFromEditor()) {
    return NS_OK;
  }

  // FYI: We need to notify HTML editor of mouseup even if it's consumed
  //      because HTML editor always needs to release grabbing resizer.
  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  htmlEditor->PreHandleMouseUp(*aMouseEvent);

  if (NS_WARN_IF(!aMouseEvent->GetTarget())) {
    return NS_ERROR_FAILURE;
  }
  // FYI: The event target must be an element node for conforming to the
  //      UI Events, but it may not be not an element node if it occurs
  //      on native anonymous node like a resizer.

  DebugOnly<nsresult> rvIgnored = htmlEditor->StopDraggingResizerOrGrabberAt(
      CSSIntPoint(aMouseEvent->ClientX(), aMouseEvent->ClientY()));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "HTMLEditor::StopDraggingResizerOrGrabberAt() failed, but ignored");

  nsresult rv = EditorEventListener::MouseUp(aMouseEvent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorEventListener::MouseUp() failed");
  return rv;
}

static bool IsAcceptableMouseEvent(const HTMLEditor& aHTMLEditor,
                                   MouseEvent* aMouseEvent) {
  // Contenteditable should disregard mousedowns outside it.
  // IsAcceptableInputEvent() checks it for a mouse event.
  WidgetMouseEvent* mousedownEvent =
      aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  MOZ_ASSERT(mousedownEvent);
  return aHTMLEditor.IsAcceptableInputEvent(mousedownEvent);
}

nsresult HTMLEditorEventListener::HandlePrimaryMouseButtonDown(
    HTMLEditor& aHTMLEditor, MouseEvent& aMouseEvent) {
  RefPtr<EventTarget> eventTarget = aMouseEvent.GetExplicitOriginalTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return NS_ERROR_FAILURE;
  }
  nsIContent* eventTargetContent = nsIContent::FromEventTarget(eventTarget);
  if (!eventTargetContent) {
    return NS_OK;
  }

  RefPtr<Element> toSelect;
  bool isElement = eventTargetContent->IsElement();
  int32_t clickCount = aMouseEvent.Detail();
  switch (clickCount) {
    case 1:
      if (isElement) {
        OwningNonNull<Element> element(*eventTargetContent->AsElement());
        DebugOnly<nsresult> rvIgnored =
            aHTMLEditor.StartToDragResizerOrHandleDragGestureOnGrabber(
                aMouseEvent, element);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "HTMLEditor::StartToDragResizerOrHandleDragGestureOnGrabber() "
            "failed, but ignored");
      }
      break;
    case 2:
      if (isElement) {
        toSelect = eventTargetContent->AsElement();
      }
      break;
    case 3:
      // Triple click selects `<a href>` instead of its container paragraph
      // as users may want to modify the anchor element.
      if (!isElement) {
        toSelect = aHTMLEditor.GetInclusiveAncestorByTagName(
            *nsGkAtoms::href, *eventTargetContent);
      }
      break;
  }
  if (toSelect) {
    DebugOnly<nsresult> rvIgnored = aHTMLEditor.SelectElement(toSelect);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::SelectElement() failed, but ignored");
    aMouseEvent.PreventDefault();
  }
  return NS_OK;
}

nsresult HTMLEditorEventListener::HandleSecondaryMouseButtonDown(
    HTMLEditor& aHTMLEditor, MouseEvent& aMouseEvent) {
  RefPtr<Selection> selection = aHTMLEditor.GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_OK;
  }

  int32_t offset = -1;
  nsCOMPtr<nsIContent> parentContent =
      aMouseEvent.GetRangeParentContentAndOffset(&offset);
  if (NS_WARN_IF(!parentContent) || NS_WARN_IF(offset < 0)) {
    return NS_ERROR_FAILURE;
  }

  if (nsContentUtils::IsPointInSelection(*selection, *parentContent,
                                         AssertedCast<uint32_t>(offset))) {
    return NS_OK;
  }

  RefPtr<EventTarget> eventTarget = aMouseEvent.GetExplicitOriginalTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return NS_ERROR_FAILURE;
  }

  Element* eventTargetElement = Element::FromEventTarget(eventTarget);

  // Select entire element clicked on if NOT within an existing selection
  //   and not the entire body, or table-related elements
  if (HTMLEditUtils::IsImage(eventTargetElement)) {
    // MOZ_KnownLive(eventTargetElement): Guaranteed by eventTarget.
    DebugOnly<nsresult> rvIgnored =
        aHTMLEditor.SelectElement(MOZ_KnownLive(eventTargetElement));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::SelectElement() failed, but ignored");
  }

  // HACK !!! Context click places the caret but the context menu consumes
  // the event; so we need to check resizing state ourselves
  DebugOnly<nsresult> rvIgnored =
      aHTMLEditor.CheckSelectionStateForAnonymousButtons();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "HTMLEditor::CheckSelectionStateForAnonymousButtons() "
                       "failed, but ignored");

  return NS_OK;
}

nsresult HTMLEditorEventListener::MouseDown(MouseEvent* aMouseEvent) {
  MOZ_ASSERT(aMouseEvent);
  MOZ_ASSERT(aMouseEvent->IsTrusted());

  if (NS_WARN_IF(!aMouseEvent) || DetachedFromEditor()) {
    return NS_OK;
  }

  // Even if it's not acceptable mousedown event (i.e., when mousedown
  // event is fired outside of the active editing host), we need to commit
  // composition because it will be change the selection to the clicked
  // point.  Then, we won't be able to commit the composition.
  if (!EnsureCommitComposition()) {
    return NS_OK;
  }

  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  htmlEditor->PreHandleMouseDown(*aMouseEvent);

  if (!IsAcceptableMouseEvent(*htmlEditor, aMouseEvent)) {
    return EditorEventListener::MouseDown(aMouseEvent);
  }

  if (aMouseEvent->Button() == MouseButton::ePrimary) {
    nsresult rv = HandlePrimaryMouseButtonDown(*htmlEditor, *aMouseEvent);
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else if (aMouseEvent->Button() == MouseButton::eSecondary) {
    nsresult rv = HandleSecondaryMouseButtonDown(*htmlEditor, *aMouseEvent);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsresult rv = EditorEventListener::MouseDown(aMouseEvent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorEventListener::MouseDown() failed");
  return rv;
}

nsresult HTMLEditorEventListener::MouseClick(
    WidgetMouseEvent* aMouseClickEvent) {
  if (NS_WARN_IF(DetachedFromEditor())) {
    return NS_OK;
  }

  RefPtr<Element> element =
      Element::FromEventTargetOrNull(aMouseClickEvent->GetDOMEventTarget());
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  DebugOnly<nsresult> rvIgnored =
      htmlEditor->DoInlineTableEditingAction(*element);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "HTMLEditor::DoInlineTableEditingAction() failed, but ignored");
  // DoInlineTableEditingAction might cause reframe
  // Editor is destroyed.
  if (NS_WARN_IF(htmlEditor->Destroyed())) {
    return NS_OK;
  }

  nsresult rv = EditorEventListener::MouseClick(aMouseClickEvent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorEventListener::MouseClick() failed");
  return rv;
}

}  // namespace mozilla
