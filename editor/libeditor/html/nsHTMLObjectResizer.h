/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman (glazman@netscape.com) (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIDOMMouseMotionListener.h"

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

class ResizerMouseMotionListener: public nsIDOMMouseMotionListener
{
public:
  ResizerMouseMotionListener(nsIHTMLEditor * aEditor);
  virtual ~ResizerMouseMotionListener();


/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent);

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
