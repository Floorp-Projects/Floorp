/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nshtmlobjectresizer__h
#define _nshtmlobjectresizer__h

#include "nsIDOMEventListener.h"
#include "nsISelectionListener.h"
#include "nsISupportsImpl.h"
#include "nsIWeakReferenceUtils.h"
#include "nsLiteralString.h"

class nsIHTMLEditor;

#define kTopLeft       NS_LITERAL_STRING("nw")
#define kTop           NS_LITERAL_STRING("n")
#define kTopRight      NS_LITERAL_STRING("ne")
#define kLeft          NS_LITERAL_STRING("w")
#define kRight         NS_LITERAL_STRING("e")
#define kBottomLeft    NS_LITERAL_STRING("sw")
#define kBottom        NS_LITERAL_STRING("s")
#define kBottomRight   NS_LITERAL_STRING("se")

// ==================================================================
// ResizerSelectionListener
// ==================================================================

class ResizerSelectionListener : public nsISelectionListener
{
public:

  explicit ResizerSelectionListener(nsIHTMLEditor * aEditor);
  void Reset();

  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSISELECTIONLISTENER

protected:
  virtual ~ResizerSelectionListener();

  nsWeakPtr mEditor;
};

// ==================================================================
// ResizerMouseMotionListener
// ==================================================================

class ResizerMouseMotionListener : public nsIDOMEventListener
{
public:
  explicit ResizerMouseMotionListener(nsIHTMLEditor * aEditor);

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  virtual ~ResizerMouseMotionListener();

  nsWeakPtr mEditor;

};

// ==================================================================
// DocumentResizeEventListener
// ==================================================================

class DocumentResizeEventListener: public nsIDOMEventListener
{
public:
  explicit DocumentResizeEventListener(nsIHTMLEditor * aEditor);

  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  virtual ~DocumentResizeEventListener();
  nsWeakPtr mEditor;

};

#endif /* _nshtmlobjectresizer__h */
