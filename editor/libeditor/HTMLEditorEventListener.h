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
class HTMLEditor;

class HTMLEditorEventListener final : public EditorEventListener
{
public:
  HTMLEditorEventListener()
  {
  }

  virtual ~HTMLEditorEventListener()
  {
  }

  /**
   * Connect() fails if aEditorBase isn't an HTMLEditor instance.
   */
  virtual nsresult Connect(EditorBase* aEditorBase) override;

protected:
  virtual nsresult MouseDown(nsIDOMMouseEvent* aMouseEvent) override;
  virtual nsresult MouseUp(nsIDOMMouseEvent* aMouseEvent) override;
  virtual nsresult MouseClick(nsIDOMMouseEvent* aMouseEvent) override;
};

} // namespace mozilla

#endif // #ifndef HTMLEditorEventListener_h
