/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Report_h
#define mozilla_dom_Report_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class ReportBody;

class Report final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Report)

  Report(nsIGlobalObject* aGlobal, const nsAString& aType,
         const nsAString& aURL, ReportBody* aBody);

  already_AddRefed<Report> Clone();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  void GetType(nsAString& aType) const;

  void GetUrl(nsAString& aURL) const;

  ReportBody* GetBody() const;

 private:
  ~Report();

  nsCOMPtr<nsIGlobalObject> mGlobal;

  const nsString mType;
  const nsString mURL;
  RefPtr<ReportBody> mBody;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Report_h
