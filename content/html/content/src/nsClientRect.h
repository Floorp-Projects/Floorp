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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is
 * Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef NSCLIENTRECT_H_
#define NSCLIENTRECT_H_

#include "nsIDOMClientRect.h"
#include "nsIDOMClientRectList.h"
#include "nsCOMArray.h"
#include "nsRect.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"

class nsClientRect : public nsIDOMClientRect
{
public:
  NS_DECL_ISUPPORTS

  nsClientRect();
  void SetRect(float aX, float aY, float aWidth, float aHeight) {
    mX = aX; mY = aY; mWidth = aWidth; mHeight = aHeight;
  }
  virtual ~nsClientRect() {}
  
  NS_DECL_NSIDOMCLIENTRECT

  void SetLayoutRect(const nsRect& aLayoutRect);

protected:
  float mX, mY, mWidth, mHeight;
};

class nsClientRectList MOZ_FINAL : public nsIDOMClientRectList,
                                   public nsWrapperCache
{
public:
  nsClientRectList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsProxy();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsClientRectList)

  NS_DECL_NSIDOMCLIENTRECTLIST
  
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(nsIDOMClientRect* aElement) { mArray.AppendObject(aElement); }

  static nsClientRectList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMClientRectList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMClientRectList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMClientRectList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsClientRectList*>(aSupports);
  }

protected:
  virtual ~nsClientRectList() {}

  nsCOMArray<nsIDOMClientRect> mArray;
  nsCOMPtr<nsISupports> mParent;
};

#endif /*NSCLIENTRECT_H_*/
