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

#include "nsPLDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"

nsPLDOMEvent::nsPLDOMEvent(nsINode *aEventNode, nsEvent &aEvent)
  : mEventNode(aEventNode), mDispatchChromeOnly(PR_FALSE)
{
  bool trusted = NS_IS_TRUSTED_EVENT(&aEvent);
  nsEventDispatcher::CreateEvent(nsnull, &aEvent, EmptyString(),
                                 getter_AddRefs(mEvent));
  NS_ASSERTION(mEvent, "Should never fail to create an event");
  nsCOMPtr<nsIPrivateDOMEvent> priv = do_QueryInterface(mEvent);
  NS_ASSERTION(priv, "Should also not fail to QI to nsIDOMEventPrivate");
  priv->DuplicatePrivateData();
  priv->SetTrusted(trusted);
}

NS_IMETHODIMP nsPLDOMEvent::Run()
{
  if (!mEventNode) {
    return NS_OK;
  }

  if (mEvent) {
    NS_ASSERTION(!mDispatchChromeOnly, "Can't do that");
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mEventNode);
    bool defaultActionEnabled; // This is not used because the caller is async
    target->DispatchEvent(mEvent, &defaultActionEnabled);
  } else {
    nsIDocument* doc = mEventNode->GetOwnerDoc();
    if (doc) {
      if (mDispatchChromeOnly) {
        nsContentUtils::DispatchChromeEvent(doc, mEventNode, mEventType,
                                            mBubbles, PR_FALSE);
      } else {
        nsContentUtils::DispatchTrustedEvent(doc, mEventNode, mEventType,
                                             mBubbles, PR_FALSE);
      }
    }
  }

  return NS_OK;
}

nsresult nsPLDOMEvent::PostDOMEvent()
{
  return NS_DispatchToCurrentThread(this);
}

void nsPLDOMEvent::RunDOMEventWhenSafe()
{
  nsContentUtils::AddScriptRunner(this);
}

nsLoadBlockingPLDOMEvent::~nsLoadBlockingPLDOMEvent()
{
  if (mBlockedDoc) {
    mBlockedDoc->UnblockOnload(PR_TRUE);
  }
}
