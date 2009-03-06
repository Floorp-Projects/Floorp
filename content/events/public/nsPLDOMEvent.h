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
 * Aaron Leventhal (aaronleventhal@moonset.net)
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef nsPLDOMEvent_h___
#define nsPLDOMEvent_h___

#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsINode.h"
#include "nsIDOMEvent.h"
#include "nsString.h"

/**
 * Use nsPLDOMEvent to fire a DOM event that requires safe a stable DOM.
 * For example, you may need to fire an event from within layout, but
 * want to ensure that the event handler doesn't mutate the DOM at
 * the wrong time, in order to avoid resulting instability.
 *
 * TODO: This should be renamed nsAsyncDOMEvent or something that does
 *       not include the substring "PL" that refers to the old PLEvent
 *       structure used with the old eventing system.  See bug 334573.
 */
 
class nsPLDOMEvent : public nsRunnable {
public:
  nsPLDOMEvent(nsINode *aEventNode, const nsAString& aEventType,
               PRBool aDispatchChromeOnly)
    : mEventNode(aEventNode), mEventType(aEventType),
      mDispatchChromeOnly(aDispatchChromeOnly)
  { }

  nsPLDOMEvent(nsINode *aEventNode, nsIDOMEvent *aEvent)
    : mEventNode(aEventNode), mEvent(aEvent), mDispatchChromeOnly(PR_FALSE)
  { }

  NS_IMETHOD Run();
  nsresult PostDOMEvent();
  nsresult RunDOMEventWhenSafe();

  nsCOMPtr<nsINode>     mEventNode;
  nsCOMPtr<nsIDOMEvent> mEvent;
  nsString              mEventType;
  PRBool                mDispatchChromeOnly;
};

#endif
