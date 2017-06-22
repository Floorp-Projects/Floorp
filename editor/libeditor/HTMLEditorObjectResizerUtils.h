/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorObjectResizerUtils_h
#define HTMLEditorObjectResizerUtils_h

#include "mozilla/HTMLEditor.h"
#include "nsIDOMEventListener.h"
#include "nsISelectionListener.h"
#include "nsISupportsImpl.h"
#include "nsIWeakReferenceUtils.h"
#include "nsLiteralString.h"

class nsIHTMLEditor;

namespace mozilla {

#define kTopLeft       NS_LITERAL_STRING("nw")
#define kTop           NS_LITERAL_STRING("n")
#define kTopRight      NS_LITERAL_STRING("ne")
#define kLeft          NS_LITERAL_STRING("w")
#define kRight         NS_LITERAL_STRING("e")
#define kBottomLeft    NS_LITERAL_STRING("sw")
#define kBottom        NS_LITERAL_STRING("s")
#define kBottomRight   NS_LITERAL_STRING("se")

/******************************************************************************
 * mozilla::ResizerSelectionListener
 ******************************************************************************/

class ResizerSelectionListener final : public nsISelectionListener
{
public:
  explicit ResizerSelectionListener(HTMLEditor& aHTMLEditor);
  void Reset();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

protected:
  virtual ~ResizerSelectionListener() {}
  CachedWeakPtr<HTMLEditor, nsIHTMLEditor> mHTMLEditorWeak;
};

/******************************************************************************
 * mozilla::ResizerMouseMotionListener
 ******************************************************************************/

class ResizerMouseMotionListener final : public nsIDOMEventListener
{
public:
  explicit ResizerMouseMotionListener(HTMLEditor& aHTMLEditor);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  virtual ~ResizerMouseMotionListener() {}
  CachedWeakPtr<HTMLEditor, nsIHTMLEditor> mHTMLEditorWeak;
};

/******************************************************************************
 * mozilla::DocumentResizeEventListener
 ******************************************************************************/

class DocumentResizeEventListener final : public nsIDOMEventListener
{
public:
  explicit DocumentResizeEventListener(HTMLEditor& aHTMLEditor);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  virtual ~DocumentResizeEventListener() {}
  CachedWeakPtr<HTMLEditor, nsIHTMLEditor> mHTMLEditorWeak;
};

} // namespace mozilla

#endif // #ifndef HTMLEditorObjectResizerUtils_h
