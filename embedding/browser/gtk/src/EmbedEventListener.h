/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Oleg Romashin <romaxa@gmail.com>
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

#ifndef __EmbedEventListener_h
#define __EmbedEventListener_h

#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMUIListener.h"

#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMFocusListener.h"
#include "EmbedContextMenuInfo.h"

#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIURI.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIDOM3Node.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsIFileURL.h"
#include "nsILocalFile.h"
#include "nsIFile.h"
#include "nsIWebBrowserPersist.h"
#include "nsCWebBrowserPersist.h"
#include "nsIWebProgressListener.h"
#include "nsISelectionController.h"
#include "nsIDOMMouseEvent.h"
#include "nsXPCOMStrings.h"
#include "nsCRTGlue.h"

class EmbedPrivate;

class EmbedEventListener : public nsIDOMKeyListener,
                           public nsIDOMMouseListener,
                           public nsIDOMUIListener,
                           public nsIDOMMouseMotionListener,
                           public nsIWebProgressListener,
                           public nsIDOMFocusListener
{
 public:

  EmbedEventListener();
  virtual ~EmbedEventListener();

  nsresult Init(EmbedPrivate *aOwner);

  NS_DECL_ISUPPORTS

//  NS_DECL_NSIDOMEVENTLISTENER
  // nsIDOMEventListener
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD HandleLink(nsIDOMNode* node);
  // nsIDOMKeyListener

  NS_IMETHOD KeyDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aDOMEvent);

  // nsIDOMMouseListener

  NS_IMETHOD MouseDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aDOMEvent);

  // nsIDOMUIListener

  NS_IMETHOD Activate(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD FocusIn(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD FocusOut(nsIDOMEvent* aDOMEvent);

  // nsIDOMMouseMotionListener
  NS_IMETHOD MouseMove(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent);
  EmbedContextMenuInfo* GetContextInfo() { return mCtxInfo; };

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
  NS_IMETHOD HandleSelection(nsIDOMMouseEvent* aDOMMouseEvent);

  nsresult   NewURI            (nsIURI **result,
                                const char *spec);
  nsresult   GetIOService      (nsIIOService **ioService);

  void       GeneratePixBuf    ();

  void       GetFaviconFromURI (const char*  aURI);
 private:

  EmbedPrivate *mOwner;
  EmbedContextMenuInfo *mCtxInfo;

  // Selection and some clipboard stuff
  nsCOMPtr<nsISelectionController> mCurSelCon;
  nsCOMPtr<nsISelectionController> mLastSelCon;
  PRBool mFocusInternalFrame;
  PRInt32 mClickCount;
};

#endif /* __EmbedEventListener_h */
