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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDOMEvent_h__
#define nsDOMEvent_h__

#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsISupports.h"
#include "nsIPrivateDOMEvent.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsRecycled.h"

class nsIContent;
class nsIScrollableView;

/* Currently we use a single-object recycler (nsRecycledSingle).
 * With our current usage pattern, we almost never have more than
 * a single nsDOMEvent active in memory at a time under normal circumstances.
 */
 
class nsDOMEvent : public nsIDOMEvent,
                   public nsIDOMNSEvent,
                   public nsIPrivateDOMEvent,
                   public nsRecycledSingle<nsDOMEvent>
{
public:

  // Note: this enum must be kept in sync with mEventNames in nsDOMEvent.cpp
  enum nsDOMEvents {
    eDOMEvents_mousedown=0,
    eDOMEvents_mouseup,
    eDOMEvents_click,
    eDOMEvents_dblclick,
    eDOMEvents_mouseover,
    eDOMEvents_mouseout,
    eDOMEvents_mousemove,
    eDOMEvents_contextmenu,
    eDOMEvents_keydown,
    eDOMEvents_keyup,
    eDOMEvents_keypress,
    eDOMEvents_focus,
    eDOMEvents_blur,
    eDOMEvents_load,
    eDOMEvents_beforeunload,
    eDOMEvents_unload,
    eDOMEvents_abort,
    eDOMEvents_error,
    eDOMEvents_submit,
    eDOMEvents_reset,
    eDOMEvents_change,
    eDOMEvents_select,
    eDOMEvents_input,
    eDOMEvents_paint,
    eDOMEvents_text,
    eDOMEvents_popupShowing,
    eDOMEvents_popupShown,
    eDOMEvents_popupHiding,
    eDOMEvents_popupHidden,
    eDOMEvents_close,
    eDOMEvents_command,
    eDOMEvents_broadcast,
    eDOMEvents_commandupdate,
    eDOMEvents_dragenter,
    eDOMEvents_dragover,
    eDOMEvents_dragexit,
    eDOMEvents_dragdrop,
    eDOMEvents_draggesture,
    eDOMEvents_resize,
    eDOMEvents_scroll,
    eDOMEvents_overflow,
    eDOMEvents_underflow,
    eDOMEvents_overflowchanged,
    eDOMEvents_subtreemodified,
    eDOMEvents_nodeinserted,
    eDOMEvents_noderemoved,
    eDOMEvents_noderemovedfromdocument,
    eDOMEvents_nodeinsertedintodocument,
    eDOMEvents_attrmodified,
    eDOMEvents_characterdatamodified,
    eDOMEvents_popupBlocked,
    eDOMEvents_DOMActivate,
    eDOMEvents_DOMFocusIn,
    eDOMEvents_DOMFocusOut
  };

  nsDOMEvent(nsPresContext* aPresContext, nsEvent* aEvent);
  virtual ~nsDOMEvent();

  NS_DECL_ISUPPORTS

  // nsIDOMEvent Interface
  NS_DECL_NSIDOMEVENT

  // nsIDOMNSEvent Interface
  NS_DECL_NSIDOMNSEVENT

  // nsIPrivateDOMEvent interface
  NS_IMETHOD    DuplicatePrivateData();
  NS_IMETHOD    SetTarget(nsIDOMEventTarget* aTarget);
  NS_IMETHOD    SetCurrentTarget(nsIDOMEventTarget* aCurrentTarget);
  NS_IMETHOD    SetOriginalTarget(nsIDOMEventTarget* aOriginalTarget);
  NS_IMETHOD    IsDispatchStopped(PRBool* aIsDispatchStopped);
  NS_IMETHOD    GetInternalNSEvent(nsEvent** aNSEvent);
  NS_IMETHOD    HasOriginalTarget(PRBool* aResult);
  NS_IMETHOD    SetTrusted(PRBool aTrusted);

  NS_IMETHOD    IsHandled(PRBool* aHandled);
  NS_IMETHOD    SetHandled(PRBool aHandled);

  static PopupControlState GetEventPopupControlState(nsEvent *aEvent);

  static void PopupAllowedEventsChanged();

  static void Shutdown();

protected:

  // Internal helper functions
  nsresult SetEventType(const nsAString& aEventTypeArg);
  static const char* GetEventName(PRUint32 aEventType);
  already_AddRefed<nsIDOMEventTarget> GetTargetFromFrame();

  nsEvent* mEvent;
  nsCOMPtr<nsPresContext> mPresContext;
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEventTarget> mCurrentTarget;
  nsCOMPtr<nsIDOMEventTarget> mOriginalTarget;
  nsCOMPtr<nsIDOMEventTarget> mExplicitOriginalTarget;
  nsCOMPtr<nsIDOMEventTarget> mTmpRealOriginalTarget;
  PRPackedBool mEventIsInternal;

  void* mScriptObject;
};

#define NS_FORWARD_TO_NSDOMEVENT \
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

#endif // nsDOMEvent_h__
