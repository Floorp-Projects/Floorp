/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ilya Konstantinov (mozilla-code@future.shiny.co.il)
 *
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

#ifndef nsDOMUIEvent_h__
#define nsDOMUIEvent_h__

#include "nsIDOMUIEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMAbstractView.h"
#include "nsIPrivateCompositionEvent.h"
#include "nsDOMEvent.h"

class nsDOMUIEvent : public nsIDOMUIEvent,
                     public nsIDOMNSUIEvent,
                     public nsIPrivateCompositionEvent,
                     public nsDOMEvent
{
public:
  nsDOMUIEvent(nsPresContext* aPresContext, nsGUIEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMUIEvent Interface
  NS_DECL_NSIDOMUIEVENT

  // nsIDOMNSUIEvent Interface
  NS_DECL_NSIDOMNSUIEVENT
  
  // nsIPrivateCompositionEvent interface
  NS_IMETHOD GetCompositionReply(nsTextEventReply** aReply);
  NS_IMETHOD GetReconversionReply(nsReconversionEventReply** aReply);
  
  // Forward to nsDOMEvent
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

protected:

  // Internal helper functions
  nsresult GetScrollInfo(nsIScrollableView** aScrollableView, float* aP2T,
                         float* aT2P);
  nsPoint GetClientPoint();
  nsPoint GetScreenPoint();
  
protected:
  nsCOMPtr<nsIDOMAbstractView> mView;
  PRInt32 mDetail;
};

#define NS_FORWARD_NSIDOMNSUIEVENT_NO_WHICH(_to) \
  NS_IMETHOD GetPreventDefault(PRBool *_retval) { return _to GetPreventDefault(_retval); } \
  NS_IMETHOD GetLayerX(PRInt32 *aLayerX) { return _to GetLayerX(aLayerX); } \
  NS_IMETHOD GetLayerY(PRInt32 *aLayerY) { return _to GetLayerY(aLayerY); } \
  NS_IMETHOD GetPageX(PRInt32 *aPageX) { return _to GetPageX(aPageX); } \
  NS_IMETHOD GetPageY(PRInt32 *aPageY) { return _to GetPageY(aPageY); } \
  NS_IMETHOD GetRangeParent(nsIDOMNode * *aRangeParent) { return _to GetRangeParent(aRangeParent); } \
  NS_IMETHOD GetRangeOffset(PRInt32 *aRangeOffset) { return _to GetRangeOffset(aRangeOffset); } \
  NS_IMETHOD GetCancelBubble(PRBool *aCancelBubble) { return _to GetCancelBubble(aCancelBubble); } \
  NS_IMETHOD SetCancelBubble(PRBool aCancelBubble) { return _to SetCancelBubble(aCancelBubble); } \
  NS_IMETHOD GetIsChar(PRBool *aIsChar) { return _to GetIsChar(aIsChar); }

#define NS_FORWARD_TO_NSDOMUIEVENT \
  NS_FORWARD_NSIDOMUIEVENT(nsDOMUIEvent::) \
  NS_FORWARD_NSIDOMNSUIEVENT(nsDOMUIEvent::) \
  NS_FORWARD_TO_NSDOMEVENT

#define NS_FORWARD_TO_NSDOMUIEVENT_NO_WHICH \
  NS_FORWARD_NSIDOMUIEVENT(nsDOMUIEvent::) \
  NS_FORWARD_NSIDOMNSUIEVENT_NO_WHICH(nsDOMUIEvent::) \
  NS_FORWARD_TO_NSDOMEVENT

#endif // nsDOMUIEvent_h__
