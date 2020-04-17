/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FeaturePolicyViolationReportBody_h
#define mozilla_dom_FeaturePolicyViolationReportBody_h

#include "mozilla/dom/ReportBody.h"

namespace mozilla {
namespace dom {

class FeaturePolicyViolationReportBody final : public ReportBody {
 public:
  FeaturePolicyViolationReportBody(nsIGlobalObject* aGlobal,
                                   const nsAString& aFeatureId,
                                   const nsAString& aSourceFile,
                                   const Nullable<int32_t>& aLineNumber,
                                   const Nullable<int32_t>& aColumnNumber,
                                   const nsAString& aDisposition);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetFeatureId(nsAString& aFeatureId) const;

  void GetSourceFile(nsAString& aSourceFile) const;

  Nullable<int32_t> GetLineNumber() const;

  Nullable<int32_t> GetColumnNumber() const;

  void GetDisposition(nsAString& aDisposition) const;

 protected:
  void ToJSON(JSONWriter& aJSONWriter) const override;

 private:
  ~FeaturePolicyViolationReportBody();

  const nsString mFeatureId;
  const nsString mSourceFile;
  const Nullable<int32_t> mLineNumber;
  const Nullable<int32_t> mColumnNumber;
  const nsString mDisposition;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FeaturePolicyViolationReportBody_h
