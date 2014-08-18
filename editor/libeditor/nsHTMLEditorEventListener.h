/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLEditorEventListener_h__
#define nsHTMLEditorEventListener_h__

#include "nsEditorEventListener.h"
#include "nscore.h"

class nsEditor;
class nsHTMLEditor;
class nsIDOMEvent;

class nsHTMLEditorEventListener : public nsEditorEventListener
{
public:
  nsHTMLEditorEventListener() :
    nsEditorEventListener()
  {
  }

  virtual ~nsHTMLEditorEventListener()
  {
  }

#ifdef DEBUG
  // WARNING: You must be use nsHTMLEditor or its sub class for this class.
  virtual nsresult Connect(nsEditor* aEditor);
#endif

  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);

protected:
  inline nsHTMLEditor* GetHTMLEditor();
};

#endif // nsHTMLEditorEventListener_h__

