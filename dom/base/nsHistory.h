/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHistory_h___
#define nsHistory_h___

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIDOMHistory.h"
#include "nsString.h"
#include "nsISHistory.h"
#include "nsIWeakReference.h"
#include "nsPIDOMWindow.h"

class nsIDocShell;

// Script "History" object
class nsHistory MOZ_FINAL : public nsIDOMHistory, // Empty, needed for extension
                                                  // backwards compat
                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsHistory)

public:
  nsHistory(nsPIDOMWindow* aInnerWindow);
  virtual ~nsHistory();

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  uint32_t GetLength(mozilla::ErrorResult& aRv) const;
  JS::Value GetState(JSContext* aCx, mozilla::ErrorResult& aRv) const;
  void Go(int32_t aDelta, mozilla::ErrorResult& aRv);
  void Back(mozilla::ErrorResult& aRv);
  void Forward(mozilla::ErrorResult& aRv);
  void PushState(JSContext* aCx, JS::Handle<JS::Value> aData,
                 const nsAString& aTitle, const nsAString& aUrl,
                 mozilla::ErrorResult& aRv);
  void ReplaceState(JSContext* aCx, JS::Handle<JS::Value> aData,
                    const nsAString& aTitle, const nsAString& aUrl,
                    mozilla::ErrorResult& aRv);

protected:
  nsIDocShell *GetDocShell() const {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mInnerWindow));
    if (!win)
      return nullptr;
    return win->GetDocShell();
  }

  void PushOrReplaceState(JSContext* aCx, JS::Value aData,
                          const nsAString& aTitle, const nsAString& aUrl,
                          mozilla::ErrorResult& aRv, bool aReplace);

  already_AddRefed<nsISHistory> GetSessionHistory() const;

  nsCOMPtr<nsIWeakReference> mInnerWindow;
};

#endif /* nsHistory_h___ */
