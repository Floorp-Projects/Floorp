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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
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

#include "nsIFocusEventSuppressor.h"
#include "nsTArray.h"

class nsFocusEventSuppressorService : public nsIFocusEventSuppressorService
{
public:
  NS_DECL_ISUPPORTS
  virtual void AddObserverCallback(nsFocusEventSuppressorCallback aCallback)
  {
    NS_AddFocusSuppressorCallback(aCallback);
  }
  virtual void Suppress()
  {
    NS_SuppressFocusEvent();
  }
  virtual void Unsuppress()
  {
    NS_UnsuppressFocusEvent();
  }
};

static nsTArray<nsFocusEventSuppressorCallback>* sCallbacks = nsnull;
static PRUint32 sFocusSuppressCount = 0;

NS_IMPL_ADDREF(nsFocusEventSuppressorService)
NS_IMPL_RELEASE(nsFocusEventSuppressorService)

NS_INTERFACE_MAP_BEGIN(nsFocusEventSuppressorService)
  NS_INTERFACE_MAP_ENTRY(nsIFocusEventSuppressorService)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
NS_AddFocusSuppressorCallback(nsFocusEventSuppressorCallback aCallback)
{
  if (aCallback) {
    if (!sCallbacks) {
      sCallbacks = new nsTArray<nsFocusEventSuppressorCallback>(2);
      if (!sCallbacks) {
        NS_WARNING("Out of memory!");
        return;
      }
    } else if (sCallbacks->Contains(aCallback)) {
      return;
    }
    sCallbacks->AppendElement(aCallback);
  }
}

void
NS_SuppressFocusEvent()
{
  ++sFocusSuppressCount;
  if (sFocusSuppressCount == 1 && sCallbacks) {
    for (PRUint32 i = 0; i < sCallbacks->Length(); ++i) {
      sCallbacks->ElementAt(i)(PR_TRUE);
    }
  }
}

void
NS_UnsuppressFocusEvent()
{
  --sFocusSuppressCount;
  if (sFocusSuppressCount == 0 && sCallbacks) {
    for (PRUint32 i = 0; i < sCallbacks->Length(); ++i) {
      sCallbacks->ElementAt(i)(PR_FALSE);
    }
  }
}

void
NS_ShutdownFocusSuppressor()
{
  delete sCallbacks;
  sCallbacks = nsnull;
}

nsresult
NS_NewFocusEventSuppressorService(nsIFocusEventSuppressorService** aResult)
{
  nsIFocusEventSuppressorService* it = new nsFocusEventSuppressorService();
  NS_ENSURE_TRUE(it, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aResult = it);
  return NS_OK;
}
