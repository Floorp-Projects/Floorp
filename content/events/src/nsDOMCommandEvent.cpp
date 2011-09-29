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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
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

#include "nsDOMCommandEvent.h"
#include "nsContentUtils.h"

nsDOMCommandEvent::nsDOMCommandEvent(nsPresContext* aPresContext,
                                     nsCommandEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent ? aEvent :
               new nsCommandEvent(PR_FALSE, nsnull, nsnull, nsnull))
{
  mEvent->time = PR_Now();
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  } else {
    mEventIsInternal = PR_TRUE;
  }
}

nsDOMCommandEvent::~nsDOMCommandEvent()
{
  if (mEventIsInternal && mEvent->eventStructType == NS_COMMAND_EVENT) {
    delete static_cast<nsCommandEvent*>(mEvent);
    mEvent = nsnull;
  }
}

DOMCI_DATA(CommandEvent, nsDOMCommandEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMCommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCommandEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CommandEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMCommandEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMCommandEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMCommandEvent::GetCommand(nsAString& aCommand)
{
  nsIAtom* command = static_cast<nsCommandEvent*>(mEvent)->command;
  if (command) {
    command->ToString(aCommand);
  } else {
    aCommand.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCommandEvent::InitCommandEvent(const nsAString& aTypeArg,
                                    bool aCanBubbleArg,
                                    bool aCancelableArg,
                                    const nsAString& aCommand)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<nsCommandEvent*>(mEvent)->command = do_GetAtom(aCommand);
  return NS_OK;
}

nsresult NS_NewDOMCommandEvent(nsIDOMEvent** aInstancePtrResult,
                               nsPresContext* aPresContext,
                               nsCommandEvent* aEvent)
{
  nsDOMCommandEvent* it = new nsDOMCommandEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
