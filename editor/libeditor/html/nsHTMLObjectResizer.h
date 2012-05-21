/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nshtmlobjectresizer__h
#define _nshtmlobjectresizer__h

#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "nsISelection.h"
#include "nsString.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLObjectResizer.h"

#include "nsIDOMMouseEvent.h"

#include "nsIDOMEventListener.h"
#include "nsISelectionListener.h"

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

  ResizerSelectionListener(nsIHTMLEditor * aEditor);
  void Reset();
  virtual ~ResizerSelectionListener();

  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSISELECTIONLISTENER

protected:

  nsWeakPtr mEditor;
};

// ==================================================================
// ResizerMouseMotionListener
// ==================================================================

class ResizerMouseMotionListener : public nsIDOMEventListener
{
public:
  ResizerMouseMotionListener(nsIHTMLEditor * aEditor);
  virtual ~ResizerMouseMotionListener();


/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  nsWeakPtr mEditor;

};

// ==================================================================
// DocumentResizeEventListener
// ==================================================================

class DocumentResizeEventListener: public nsIDOMEventListener
{
public:
  DocumentResizeEventListener(nsIHTMLEditor * aEditor);
  virtual ~DocumentResizeEventListener();

  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  nsWeakPtr mEditor;

};

#endif /* _nshtmlobjectresizer__h */
