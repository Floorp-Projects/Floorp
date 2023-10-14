/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorEventListener_h
#define HTMLEditorEventListener_h

#include "EditorEventListener.h"

#include "EditorForwards.h"
#include "HTMLEditor.h"

#include "nscore.h"

namespace mozilla {

namespace dom {
class Element;
}

class HTMLEditorEventListener final : public EditorEventListener {
 public:
  HTMLEditorEventListener()
      : mListeningToMouseMoveEventForResizers(false),
        mListeningToMouseMoveEventForGrabber(false),
        mListeningToResizeEvent(false) {}

  // nsIDOMEventListener
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD HandleEvent(dom::Event* aEvent) override;

  /**
   * Connect() fails if aEditorBase isn't an HTMLEditor instance.
   */
  virtual nsresult Connect(EditorBase* aEditorBase) override;

  virtual void Disconnect() override;

  /**
   * ListenToMouseMoveEventForResizers() starts to listen to or stop
   * listening to "mousemove" events for resizers.
   */
  nsresult ListenToMouseMoveEventForResizers(bool aListen) {
    if (aListen == mListeningToMouseMoveEventForResizers) {
      return NS_OK;
    }
    nsresult rv = ListenToMouseMoveEventForResizersOrGrabber(aListen, false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditorEventListener::"
                         "ListenToMouseMoveEventForResizersOrGrabber() failed");
    return rv;
  }

  /**
   * ListenToMouseMoveEventForResizers() starts to listen to or stop
   * listening to "mousemove" events for grabber to move absolutely
   * positioned element.
   */
  nsresult ListenToMouseMoveEventForGrabber(bool aListen) {
    if (aListen == mListeningToMouseMoveEventForGrabber) {
      return NS_OK;
    }
    nsresult rv = ListenToMouseMoveEventForResizersOrGrabber(aListen, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditorEventListener::"
                         "ListenToMouseMoveEventForResizersOrGrabber() failed");
    return rv;
  }

  /**
   * ListenToWindowResizeEvent() starts to listen to or stop listening to
   * "resize" events of the document.
   */
  nsresult ListenToWindowResizeEvent(bool aListen);

 protected:
  MOZ_CAN_RUN_SCRIPT virtual nsresult MouseDown(
      dom::MouseEvent* aMouseEvent) override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult MouseUp(
      dom::MouseEvent* aMouseEvent) override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult MouseClick(
      WidgetMouseEvent* aMouseClickEvent) override;

  nsresult ListenToMouseMoveEventForResizersOrGrabber(bool aListen,
                                                      bool aForGrabber);

  MOZ_CAN_RUN_SCRIPT nsresult HandlePrimaryMouseButtonDown(
      HTMLEditor& aHTMLEditor, dom::MouseEvent& aMouseEvent);
  MOZ_CAN_RUN_SCRIPT nsresult HandleSecondaryMouseButtonDown(
      HTMLEditor& aHTMLEditor, dom::MouseEvent& aMouseEvent);

  bool mListeningToMouseMoveEventForResizers;
  bool mListeningToMouseMoveEventForGrabber;
  bool mListeningToResizeEvent;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditorEventListener_h
