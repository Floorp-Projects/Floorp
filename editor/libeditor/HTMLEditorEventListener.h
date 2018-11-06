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

class HTMLEditorEventListener final : public EditorEventListener
{
public:
  HTMLEditorEventListener()
    : EditorEventListener()
    , mListeningToResizeEvent(false)
  {
  }

  virtual ~HTMLEditorEventListener()
  {
  }

  // nsIDOMEventListener
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD HandleEvent(dom::Event *aEvent) override;

  /**
   * Connect() fails if aEditorBase isn't an HTMLEditor instance.
   */
  virtual nsresult Connect(EditorBase* aEditorBase) override;

  virtual void Disconnect() override;

  /**
   * ListenToWindowResizeEvent() starts to listen to or stop listening to
   * "resize" events of the document.
   */
  nsresult ListenToWindowResizeEvent(bool aListen);

protected:
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult MouseDown(dom::MouseEvent* aMouseEvent) override;
  virtual nsresult MouseUp(dom::MouseEvent* aMouseEvent) override;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult MouseClick(WidgetMouseEvent* aMouseClickEvent) override;

  bool mListeningToResizeEvent;
};

} // namespace mozilla

#endif // #ifndef HTMLEditorEventListener_h
