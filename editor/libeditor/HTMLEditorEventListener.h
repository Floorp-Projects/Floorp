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

#ifdef DEBUG
  // WARNING: You must be use HTMLEditor or its sub class for this class.
  virtual nsresult Connect(EditorBase* aEditorBase) override;
#endif

protected:
  virtual nsresult MouseDown(nsIDOMMouseEvent* aMouseEvent) override;
  virtual nsresult MouseUp(nsIDOMMouseEvent* aMouseEvent) override;
  virtual nsresult MouseClick(nsIDOMMouseEvent* aMouseEvent) override;

  inline HTMLEditor* GetHTMLEditor();
};

} // namespace mozilla

#endif // #ifndef HTMLEditorEventListener_h
