/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
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

#ifndef NSPAINTREQUEST_H_
#define NSPAINTREQUEST_H_

#include "nsIDOMPaintRequest.h"
#include "nsIDOMPaintRequestList.h"
#include "nsPresContext.h"
#include "nsIDOMEvent.h"
#include "dombindings.h"

class nsPaintRequest : public nsIDOMPaintRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPAINTREQUEST

  nsPaintRequest() { mRequest.mFlags = 0; }

  void SetRequest(const nsInvalidateRequestList::Request& aRequest)
  { mRequest = aRequest; }

private:
  ~nsPaintRequest() {}

  nsInvalidateRequestList::Request mRequest;
};

class nsPaintRequestList MOZ_FINAL : public nsIDOMPaintRequestList,
                                     public nsWrapperCache
{
public:
  nsPaintRequestList(nsIDOMEvent *aParent) : mParent(aParent)
  {
    SetIsProxy();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPaintRequestList)
  NS_DECL_NSIDOMPAINTREQUESTLIST
  
  virtual JSObject* WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                               bool *triedToWrap)
  {
    return mozilla::dom::binding::PaintRequestList::create(cx, scope, this,
                                                           triedToWrap);
  }

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(nsIDOMPaintRequest* aElement) { mArray.AppendObject(aElement); }

  static nsPaintRequestList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMPaintRequestList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMClientRectList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMPaintRequestList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsPaintRequestList*>(aSupports);
  }

private:
  ~nsPaintRequestList() {}

  nsCOMArray<nsIDOMPaintRequest> mArray;
  nsCOMPtr<nsIDOMEvent> mParent;
};

#endif /*NSPAINTREQUEST_H_*/
