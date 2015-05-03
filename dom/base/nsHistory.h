/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHistory_h___
#define nsHistory_h___

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMHistory.h"
#include "nsPIDOMWindow.h" // for GetParentObject
#include "nsStringFwd.h"
#include "nsWrapperCache.h"

class nsIDocShell;
class nsISHistory;
class nsIWeakReference;
class nsPIDOMWindow;

// Script "History" object
class nsHistory final : public nsIDOMHistory, // Empty, needed for extension
                                                  // backwards compat
                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsHistory)

public:
  explicit nsHistory(nsPIDOMWindow* aInnerWindow);

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t GetLength(mozilla::ErrorResult& aRv) const;
  void GetState(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                mozilla::ErrorResult& aRv) const;
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
  virtual ~nsHistory();

  nsIDocShell* GetDocShell() const;

  void PushOrReplaceState(JSContext* aCx, JS::Handle<JS::Value> aData,
                          const nsAString& aTitle, const nsAString& aUrl,
                          mozilla::ErrorResult& aRv, bool aReplace);

  already_AddRefed<nsISHistory> GetSessionHistory() const;

  nsCOMPtr<nsIWeakReference> mInnerWindow;
};

#endif /* nsHistory_h___ */
