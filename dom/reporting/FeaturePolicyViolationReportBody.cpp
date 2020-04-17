/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FeaturePolicyViolationReportBody.h"
#include "mozilla/dom/FeaturePolicyBinding.h"

namespace mozilla {
namespace dom {

FeaturePolicyViolationReportBody::FeaturePolicyViolationReportBody(
    nsIGlobalObject* aGlobal, const nsAString& aFeatureId,
    const nsAString& aSourceFile, const Nullable<int32_t>& aLineNumber,
    const Nullable<int32_t>& aColumnNumber, const nsAString& aDisposition)
    : ReportBody(aGlobal),
      mFeatureId(aFeatureId),
      mSourceFile(aSourceFile),
      mLineNumber(aLineNumber),
      mColumnNumber(aColumnNumber),
      mDisposition(aDisposition) {}

FeaturePolicyViolationReportBody::~FeaturePolicyViolationReportBody() = default;

JSObject* FeaturePolicyViolationReportBody::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FeaturePolicyViolationReportBody_Binding::Wrap(aCx, this, aGivenProto);
}

void FeaturePolicyViolationReportBody::GetFeatureId(
    nsAString& aFeatureId) const {
  aFeatureId = mFeatureId;
}

void FeaturePolicyViolationReportBody::GetSourceFile(
    nsAString& aSourceFile) const {
  aSourceFile = mSourceFile;
}

Nullable<int32_t> FeaturePolicyViolationReportBody::GetLineNumber() const {
  return mLineNumber;
}

Nullable<int32_t> FeaturePolicyViolationReportBody::GetColumnNumber() const {
  return mColumnNumber;
}

void FeaturePolicyViolationReportBody::GetDisposition(
    nsAString& aDisposition) const {
  aDisposition = mDisposition;
}

void FeaturePolicyViolationReportBody::ToJSON(JSONWriter& aWriter) const {
  aWriter.StringProperty("featureId", NS_ConvertUTF16toUTF8(mFeatureId).get());

  if (mSourceFile.IsEmpty()) {
    aWriter.NullProperty("sourceFile");
  } else {
    aWriter.StringProperty("sourceFile",
                           NS_ConvertUTF16toUTF8(mSourceFile).get());
  }

  if (mLineNumber.IsNull()) {
    aWriter.NullProperty("lineNumber");
  } else {
    aWriter.IntProperty("lineNumber", mLineNumber.Value());
  }

  if (mColumnNumber.IsNull()) {
    aWriter.NullProperty("columnNumber");
  } else {
    aWriter.IntProperty("columnNumber", mColumnNumber.Value());
  }

  aWriter.StringProperty("disposition",
                         NS_ConvertUTF16toUTF8(mDisposition).get());
}

}  // namespace dom
}  // namespace mozilla
