/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorEventListener_h
#define HTMLEditorEventListener_h

#include "EditorEventListener.h"
#include "nscore.h"

namespace mozilla {

class EditorBase;

class HTMLEditorEventListener final : public EditorEventListener {
 public:
  HTMLEditorEventListener()
      : EditorEventListener(),
        mListeningToMouseMoveEventForResizers(false),
        mListeningToMouseMoveEventForGrabber(false),
        mListeningToResizeEvent(false) {}

  virtual ~HTMLEditorEventListener() {}

  // nsIDOMEventListener
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD HandleEvent(dom::Event* aEvent) override;

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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  /**
   * ListenToWindowResizeEvent() starts to listen to or stop listening to
   * "resize" events of the document.
   */
  nsresult ListenToWindowResizeEvent(bool aListen);

 protected:
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult MouseDown(dom::MouseEvent* aMouseEvent) override;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult MouseUp(dom::MouseEvent* aMouseEvent) override;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult MouseClick(WidgetMouseEvent* aMouseClickEvent) override;

  nsresult ListenToMouseMoveEventForResizersOrGrabber(bool aListen,
                                                      bool aForGrabber);

  bool mListeningToMouseMoveEventForResizers;
  bool mListeningToMouseMoveEventForGrabber;
  bool mListeningToResizeEvent;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditorEventListener_h
