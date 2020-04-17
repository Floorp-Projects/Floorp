/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReportBody_h
#define mozilla_dom_ReportBody_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {

class JSONWriter;

namespace dom {

class ReportBody : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReportBody)

  explicit ReportBody(nsPIDOMWindowInner* aWindow);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual void ToJSON(JSONWriter& aJSONWriter) const = 0;

 protected:
  virtual ~ReportBody();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReportBody_h
