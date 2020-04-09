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
  if (NS_WARN_IF(!aEditorBase)) {
    return NS_ERROR_INVALID_ARG;
  }
  // Guarantee that mEditorBase is always HTMLEditor.
  HTMLEditor* htmlEditor = aEditorBase->AsHTMLEditor();
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
      MOZ_ASSERT(htmlEditor);
      DebugOnly<nsresult> rvIgnored = htmlEditor->OnMouseMove(mouseMoveEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "HTMLEditor::OnMouseMove() failed, but ignored");
      return NS_OK;
    }
    case eResize: {
      if (DetachedFromEditor()) {
        return NS_OK;
      }

      RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
      MOZ_ASSERT(htmlEditor);
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
        this, NS_LITERAL_STRING("mousemove"),
        TrustedEventsAtSystemGroupBubble());
    if (aForGrabber) {
      mListeningToMouseMoveEventForGrabber = true;
    } else {
      mListeningToMouseMoveEventForResizers = true;
    }
    return NS_OK;
  }

  eventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mousemove"), TrustedEventsAtSystemGroupBubble());
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
        this, NS_LITERAL_STRING("resize"), TrustedEventsAtSystemGroupBubble());
    mListeningToResizeEvent = true;
    return NS_OK;
  }

  eventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("resize"), TrustedEventsAtSystemGroupBubble());
  mListeningToResizeEvent = false;
  return NS_OK;
}

nsresult HTMLEditorEventListener::MouseUp(MouseEvent* aMouseEvent) {
  if (DetachedFromEditor()) {
    return NS_OK;
  }

  // FYI: We need to notify HTML editor of mouseup even if it's consumed
  //      because HTML editor always needs to release grabbing resizer.
  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);

  if (NS_WARN_IF(!aMouseEvent->GetTarget())) {
    return NS_ERROR_FAILURE;
  }
  // FYI: The event target must be an element node for conforming to the
  //      UI Events, but it may not be not an element node if it occurs
  //      on native anonymous node like a resizer.

  int32_t clientX = aMouseEvent->ClientX();
  int32_t clientY = aMouseEvent->ClientY();
  DebugOnly<nsresult> rvIgnored = htmlEditor->OnMouseUp(clientX, clientY);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "HTMLEditor::OnMouseUp() failed, but ignored");

  nsresult rv = EditorEventListener::MouseUp(aMouseEvent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorEventListener::MouseUp() failed");
  return rv;
}

nsresult HTMLEditorEventListener::MouseDown(MouseEvent* aMouseEvent) {
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

  WidgetMouseEvent* mousedownEvent =
      aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  MOZ_ASSERT(mousedownEvent);

  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);

  // Contenteditable should disregard mousedowns outside it.
  // IsAcceptableInputEvent() checks it for a mouse event.
  if (!htmlEditor->IsAcceptableInputEvent(mousedownEvent)) {
    return EditorEventListener::MouseDown(aMouseEvent);
  }

  // Detect only "context menu" click
  // XXX This should be easier to do!
  // But eDOMEvents_contextmenu and eContextMenu is not exposed in any event
  // interface :-(
  int16_t buttonNumber = aMouseEvent->Button();

  bool isContextClick = buttonNumber == 2;

  int32_t clickCount = aMouseEvent->Detail();

  RefPtr<EventTarget> originalEventTarget =
      aMouseEvent->GetExplicitOriginalTarget();
  if (NS_WARN_IF(!originalEventTarget)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Element> originalEventTargetElement =
      do_QueryInterface(originalEventTarget);
  if (isContextClick || (buttonNumber == 0 && clickCount == 2)) {
    RefPtr<Selection> selection = htmlEditor->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_OK;
    }

    // Get location of mouse within target node
    int32_t offset = -1;
    nsCOMPtr<nsIContent> parentContent =
        aMouseEvent->GetRangeParentContentAndOffset(&offset);
    if (NS_WARN_IF(!parentContent)) {
      return NS_ERROR_FAILURE;
    }

    // Detect if mouse point is within current selection for context click
    bool nodeIsInSelection = false;
    if (isContextClick && !selection->IsCollapsed()) {
      uint32_t rangeCount = selection->RangeCount();

      for (uint32_t i = 0; i < rangeCount; i++) {
        RefPtr<nsRange> range = selection->GetRangeAt(i);
        if (!range) {
          // Don't bail yet, iterate through them all
          continue;
        }

        IgnoredErrorResult ignoredError;
        nodeIsInSelection =
            range->IsPointInRange(*parentContent, offset, ignoredError) &&
            !ignoredError.Failed();
        NS_WARNING_ASSERTION(!ignoredError.Failed(),
                             "nsRange::IsPointInRange() failed");

        // Done when we find a range that we are in
        if (nodeIsInSelection) {
          break;
        }
      }
    }
    nsCOMPtr<nsIContent> originalEventTargetContent =
        originalEventTargetElement;
    if (!originalEventTargetContent) {
      originalEventTargetContent = do_QueryInterface(originalEventTarget);
    }
    if (originalEventTargetContent && !nodeIsInSelection) {
      RefPtr<Element> element = originalEventTargetElement.get();
      if (!originalEventTargetContent->IsElement()) {
        if (isContextClick) {
          // Set the selection to the point under the mouse cursor:
          DebugOnly<nsresult> rvIgnored =
              selection->Collapse(parentContent, offset);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                               "Selection::Collapse() failed, but ignored");
        } else {
          // Get enclosing link if in text so we can select the link
          Element* linkElement = htmlEditor->GetInclusiveAncestorByTagName(
              *nsGkAtoms::href, *originalEventTargetContent);
          if (linkElement) {
            element = linkElement;
          }
        }
      }
      // Select entire element clicked on if NOT within an existing selection
      //   and not the entire body, or table-related elements
      if (element) {
        if (isContextClick &&
            !HTMLEditUtils::IsImage(originalEventTargetContent)) {
          DebugOnly<nsresult> rvIgnored =
              selection->Collapse(parentContent, offset);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                               "Selection::Collapse() failed, but ignored");
        } else {
          DebugOnly<nsresult> rvIgnored = htmlEditor->SelectElement(element);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rvIgnored),
              "HTMLEditor::SelectElement() failed, but ignored");
        }
        if (DetachedFromEditor()) {
          return NS_OK;
        }
      }
      // XXX The variable name may become a lie, but the name is useful for
      //     warnings above.
      originalEventTargetElement = element;
    }
    // HACK !!! Context click places the caret but the context menu consumes
    // the event; so we need to check resizing state ourselves
    DebugOnly<nsresult> rvIgnored =
        htmlEditor->CheckSelectionStateForAnonymousButtons();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::CheckSelectionStateForAnonymousButtons() "
                         "failed, but ignored");

    // Prevent bubbling if we changed selection or
    //   for all context clicks
    if (originalEventTargetElement || isContextClick) {
      aMouseEvent->PreventDefault();
      return NS_OK;
    }
  } else if (!isContextClick && buttonNumber == 0 && clickCount == 1) {
    // if the target element is an image, we have to display resizers
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();
    DebugOnly<nsresult> rvIgnored = htmlEditor->OnMouseDown(
        clientX, clientY, originalEventTargetElement, aMouseEvent);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::OnMouseDown() failed, but ignored");
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

  EventTarget* eventTarget = aMouseClickEvent->GetDOMEventTarget();
  if (NS_WARN_IF(!eventTarget)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Element> element = do_QueryInterface(eventTarget);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);
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
