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
 * Doug Turner <dougt@dougt.org>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsDOMDeviceOrientationEvent.h"
#include "nsContentUtils.h"

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceOrientationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceOrientationEvent, nsDOMEvent)

DOMCI_DATA(DeviceOrientationEvent, nsDOMDeviceOrientationEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceOrientationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceOrientationEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceOrientationEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP nsDOMDeviceOrientationEvent::InitDeviceOrientationEvent(const nsAString & aEventTypeArg,
                                                                      bool aCanBubbleArg,
                                                                      bool aCancelableArg,
                                                                      double aAlpha,
                                                                      double aBeta,
                                                                      double aGamma,
                                                                      bool aAbsolute)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mAlpha = aAlpha;
  mBeta = aBeta;
  mGamma = aGamma;
  mAbsolute = aAbsolute;

  return NS_OK;
}

NS_IMETHODIMP nsDOMDeviceOrientationEvent::GetAlpha(double *aAlpha)
{
  NS_ENSURE_ARG_POINTER(aAlpha);

  *aAlpha = mAlpha;
  return NS_OK;
}

NS_IMETHODIMP nsDOMDeviceOrientationEvent::GetBeta(double *aBeta)
{
  NS_ENSURE_ARG_POINTER(aBeta);

  *aBeta = mBeta;
  return NS_OK;
}

NS_IMETHODIMP nsDOMDeviceOrientationEvent::GetGamma(double *aGamma)
{
  NS_ENSURE_ARG_POINTER(aGamma);

  *aGamma = mGamma;
  return NS_OK;
}

NS_IMETHODIMP nsDOMDeviceOrientationEvent::GetAbsolute(bool *aAbsolute)
{
  NS_ENSURE_ARG_POINTER(aAbsolute);

  *aAbsolute = mAbsolute;
  return NS_OK;
}

nsresult NS_NewDOMDeviceOrientationEvent(nsIDOMEvent** aInstancePtrResult,
                                         nsPresContext* aPresContext,
                                         nsEvent *aEvent) 
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsDOMDeviceOrientationEvent* it = new nsDOMDeviceOrientationEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
