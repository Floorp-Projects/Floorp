/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsHyperTextAccessibleWrap.h"

#include "nsEventShell.h"

NS_IMPL_ISUPPORTS_INHERITED0(nsHyperTextAccessibleWrap,
                             nsHyperTextAccessible)

IMPL_IUNKNOWN_INHERITED2(nsHyperTextAccessibleWrap,
                         nsAccessibleWrap,
                         ia2AccessibleHypertext,
                         CAccessibleEditableText);

nsresult
nsHyperTextAccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  PRUint32 eventType = aEvent->GetEventType();

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_REMOVED ||
      eventType == nsIAccessibleEvent::EVENT_TEXT_INSERTED) {
    nsAccessible *accessible = aEvent->GetAccessible();
    if (accessible) {
      nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryObject(accessible));
      if (winAccessNode) {
        void *instancePtr = NULL;
        nsresult rv = winAccessNode->QueryNativeInterface(IID_IAccessibleText,
                                                          &instancePtr);
        if (NS_SUCCEEDED(rv)) {
          NS_IF_RELEASE(gTextEvent);
          NS_IF_ADDREF(gTextEvent = downcast_accEvent(aEvent));

          (static_cast<IUnknown*>(instancePtr))->Release();
        }
      }
    }
  }

  return nsHyperTextAccessible::HandleAccEvent(aEvent);
}

nsresult
nsHyperTextAccessibleWrap::GetModifiedText(bool aGetInsertedText,
                                           nsAString& aText,
                                           PRUint32 *aStartOffset,
                                           PRUint32 *aEndOffset)
{
  aText.Truncate();
  *aStartOffset = 0;
  *aEndOffset = 0;

  if (!gTextEvent)
    return NS_OK;

  bool isInserted = gTextEvent->IsTextInserted();
  if (aGetInsertedText != isInserted)
    return NS_OK;

  nsAccessible *targetAcc = gTextEvent->GetAccessible();
  if (targetAcc != this)
    return NS_OK;

  *aStartOffset = gTextEvent->GetStartOffset();
  *aEndOffset = *aStartOffset + gTextEvent->GetLength();
  gTextEvent->GetModifiedText(aText);

  return NS_OK;
}

