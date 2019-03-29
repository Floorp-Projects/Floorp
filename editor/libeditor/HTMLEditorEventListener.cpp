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
  return EditorEventListener::Connect(htmlEditor);
}

void HTMLEditorEventListener::Disconnect() {
  if (DetachedFromEditor()) {
    EditorEventListener::Disconnect();
  }

  if (mListeningToMouseMoveEventForResizers) {
    DebugOnly<nsresult> rvIgnored = ListenToMouseMoveEventForResizers(false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Failed to remove resize event listener of resizers");
  }
  if (mListeningToMouseMoveEventForGrabber) {
    DebugOnly<nsresult> rvIgnored = ListenToMouseMoveEventForGrabber(false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Failed to remove resize event listener of grabber");
  }
  if (mListeningToResizeEvent) {
    DebugOnly<nsresult> rvIgnored = ListenToWindowResizeEvent(false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Failed to remove resize event listener");
  }

  EditorEventListener::Disconnect();
}

NS_IMETHODIMP
HTMLEditorEventListener::HandleEvent(Event* aEvent) {
  WidgetEvent* internalEvent = aEvent->WidgetEventPtr();
  switch (internalEvent->mMessage) {
    case eMouseMove: {
      if (DetachedFromEditor()) {
        return NS_OK;
      }

      RefPtr<MouseEvent> mouseEvent = aEvent->AsMouseEvent();
      if (NS_WARN_IF(!mouseEvent)) {
        return NS_ERROR_FAILURE;
      }

      RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
      MOZ_ASSERT(htmlEditor);
      DebugOnly<nsresult> rvIgnored = htmlEditor->OnMouseMove(mouseEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Resizers failed to handle mousemove events");
      return NS_OK;
    }
    case eResize: {
      if (DetachedFromEditor()) {
        return NS_OK;
      }

      RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
      MOZ_ASSERT(htmlEditor);
      nsresult rv = htmlEditor->RefreshResizers();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    default:
      nsresult rv = EditorEventListener::HandleEvent(aEvent);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
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

  EventTarget* target = mEditorBase->AsHTMLEditor()->GetDOMEventTarget();
  if (NS_WARN_IF(!target)) {
    return NS_ERROR_FAILURE;
  }

  // Listen to mousemove events in the system group since web apps may stop
  // propagation before we receive the events.
  EventListenerManager* eventListenerManager =
      target->GetOrCreateListenerManager();
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

  nsCOMPtr<EventTarget> target = do_QueryInterface(window);
  if (NS_WARN_IF(!target)) {
    return NS_ERROR_FAILURE;
  }

  // Listen to resize events in the system group since web apps may stop
  // propagation before we receive the events.
  EventListenerManager* eventListenerManager =
      target->GetOrCreateListenerManager();
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

  RefPtr<EventTarget> target = aMouseEvent->GetTarget();
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<Element> element = do_QueryInterface(target);

  int32_t clientX = aMouseEvent->ClientX();
  int32_t clientY = aMouseEvent->ClientY();
  htmlEditor->OnMouseUp(clientX, clientY, element);

  return EditorEventListener::MouseUp(aMouseEvent);
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

  RefPtr<EventTarget> target = aMouseEvent->GetExplicitOriginalTarget();
  NS_ENSURE_TRUE(target, NS_ERROR_NULL_POINTER);
  nsCOMPtr<Element> element = do_QueryInterface(target);

  if (isContextClick || (buttonNumber == 0 && clickCount == 2)) {
    RefPtr<Selection> selection = htmlEditor->GetSelection();
    NS_ENSURE_TRUE(selection, NS_OK);

    // Get location of mouse within target node
    nsCOMPtr<nsINode> parent = aMouseEvent->GetRangeParent();
    NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

    int32_t offset = aMouseEvent->RangeOffset();

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

        IgnoredErrorResult err;
        nodeIsInSelection =
            range->IsPointInRange(*parent, offset, err) && !err.Failed();

        // Done when we find a range that we are in
        if (nodeIsInSelection) {
          break;
        }
      }
    }
    nsCOMPtr<nsINode> node = do_QueryInterface(target);
    if (node && !nodeIsInSelection) {
      if (!element) {
        if (isContextClick) {
          // Set the selection to the point under the mouse cursor:
          selection->Collapse(parent, offset);
        } else {
          // Get enclosing link if in text so we can select the link
          Element* linkElement =
              htmlEditor->GetElementOrParentByTagName(*nsGkAtoms::href, node);
          if (linkElement) {
            element = linkElement;
          }
        }
      }
      // Select entire element clicked on if NOT within an existing selection
      //   and not the entire body, or table-related elements
      if (element) {
        if (isContextClick && !HTMLEditUtils::IsImage(node)) {
          selection->Collapse(parent, offset);
        } else {
          htmlEditor->SelectElement(element);
        }
        if (DetachedFromEditor()) {
          return NS_OK;
        }
      }
    }
    // HACK !!! Context click places the caret but the context menu consumes
    // the event; so we need to check resizing state ourselves
    htmlEditor->CheckSelectionStateForAnonymousButtons();

    // Prevent bubbling if we changed selection or
    //   for all context clicks
    if (element || isContextClick) {
      aMouseEvent->PreventDefault();
      return NS_OK;
    }
  } else if (!isContextClick && buttonNumber == 0 && clickCount == 1) {
    // if the target element is an image, we have to display resizers
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();
    htmlEditor->OnMouseDown(clientX, clientY, element, aMouseEvent);
  }

  return EditorEventListener::MouseDown(aMouseEvent);
}

nsresult HTMLEditorEventListener::MouseClick(
    WidgetMouseEvent* aMouseClickEvent) {
  if (NS_WARN_IF(DetachedFromEditor())) {
    return NS_OK;
  }

  RefPtr<EventTarget> target = aMouseClickEvent->GetDOMEventTarget();
  if (NS_WARN_IF(!target)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Element> element = do_QueryInterface(target);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<HTMLEditor> htmlEditor = mEditorBase->AsHTMLEditor();
  MOZ_ASSERT(htmlEditor);
  htmlEditor->DoInlineTableEditingAction(*element);
  // DoInlineTableEditingAction might cause reframe
  // Editor is destroyed.
  if (htmlEditor->Destroyed()) {
    return NS_OK;
  }

  return EditorEventListener::MouseClick(aMouseClickEvent);
}

}  // namespace mozilla
