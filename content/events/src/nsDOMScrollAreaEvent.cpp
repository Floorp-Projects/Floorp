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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Frostig <froystig@cs.stanford.edu>
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

#include "base/basictypes.h"
#include "IPC/IPCMessageUtils.h"

#include "nsDOMScrollAreaEvent.h"
#include "nsGUIEvent.h"
#include "nsClientRect.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"

nsDOMScrollAreaEvent::nsDOMScrollAreaEvent(nsPresContext *aPresContext,
                                           nsScrollAreaEvent *aEvent)
  : nsDOMUIEvent(aPresContext, aEvent)
{
  mClientArea.SetLayoutRect(aEvent ? aEvent->mArea : nsRect());
}

nsDOMScrollAreaEvent::~nsDOMScrollAreaEvent()
{
  if (mEventIsInternal && mEvent) {
    if (mEvent->eventStructType == NS_SCROLLAREA_EVENT) {
      delete static_cast<nsScrollAreaEvent *>(mEvent);
      mEvent = nsnull;
    }
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMScrollAreaEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMScrollAreaEvent, nsDOMUIEvent)

DOMCI_DATA(ScrollAreaEvent, nsDOMScrollAreaEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMScrollAreaEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScrollAreaEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ScrollAreaEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)


NS_IMETHODIMP
nsDOMScrollAreaEvent::GetX(float *aX)
{
  return mClientArea.GetLeft(aX);
}

NS_IMETHODIMP
nsDOMScrollAreaEvent::GetY(float *aY)
{
  return mClientArea.GetTop(aY);
}

NS_IMETHODIMP
nsDOMScrollAreaEvent::GetWidth(float *aWidth)
{
  return mClientArea.GetWidth(aWidth);
}

NS_IMETHODIMP
nsDOMScrollAreaEvent::GetHeight(float *aHeight)
{
  return mClientArea.GetHeight(aHeight);
}

NS_IMETHODIMP
nsDOMScrollAreaEvent::InitScrollAreaEvent(const nsAString &aEventType,
                                          PRBool aCanBubble,
                                          PRBool aCancelable,
                                          nsIDOMAbstractView *aView,
                                          PRInt32 aDetail,
                                          float aX, float aY,
                                          float aWidth, float aHeight)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aEventType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  mClientArea.SetRect(aX, aY, aWidth, aHeight);

  return NS_OK;
}

void
nsDOMScrollAreaEvent::Serialize(IPC::Message* aMsg,
                                PRBool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("scrollareaevent"));
  }

  nsDOMEvent::Serialize(aMsg, PR_FALSE);

  float val;
  mClientArea.GetLeft(&val);
  IPC::WriteParam(aMsg, val);
  mClientArea.GetTop(&val);
  IPC::WriteParam(aMsg, val);
  mClientArea.GetWidth(&val);
  IPC::WriteParam(aMsg, val);
  mClientArea.GetHeight(&val);
  IPC::WriteParam(aMsg, val);
}

PRBool
nsDOMScrollAreaEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  NS_ENSURE_TRUE(nsDOMEvent::Deserialize(aMsg, aIter), PR_FALSE);

  float x, y, width, height;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &x), PR_FALSE);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &y), PR_FALSE);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &width), PR_FALSE);
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &height), PR_FALSE);
  mClientArea.SetRect(x, y, width, height);

  return PR_TRUE;
}

nsresult
NS_NewDOMScrollAreaEvent(nsIDOMEvent **aInstancePtrResult,
                         nsPresContext *aPresContext,
                         nsScrollAreaEvent *aEvent)
{
  nsDOMScrollAreaEvent *ev = new nsDOMScrollAreaEvent(aPresContext, aEvent);

  if (!ev) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(ev, aInstancePtrResult);
}
