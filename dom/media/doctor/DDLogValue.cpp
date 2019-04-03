/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DDLogValue.h"

#include "mozilla/JSONWriter.h"

namespace mozilla {

struct LogValueMatcher {
  nsCString& mString;

  void operator()(const DDNoValue&) const {}
  void operator()(const DDLogObject& a) const { a.AppendPrintf(mString); }
  void operator()(const char* a) const { mString.AppendPrintf(R"("%s")", a); }
  void operator()(const nsCString& a) const {
    mString.AppendPrintf(R"(nsCString("%s"))", a.Data());
  }
  void operator()(bool a) const { mString.AppendPrintf(a ? "true" : "false"); }
  void operator()(int8_t a) const {
    mString.AppendPrintf("int8_t(%" PRIi8 ")", a);
  }
  void operator()(uint8_t a) const {
    mString.AppendPrintf("uint8_t(%" PRIu8 ")", a);
  }
  void operator()(int16_t a) const {
    mString.AppendPrintf("int16_t(%" PRIi16 ")", a);
  }
  void operator()(uint16_t a) const {
    mString.AppendPrintf("uint16_t(%" PRIu16 ")", a);
  }
  void operator()(int32_t a) const {
    mString.AppendPrintf("int32_t(%" PRIi32 ")", a);
  }
  void operator()(uint32_t a) const {
    mString.AppendPrintf("uint32_t(%" PRIu32 ")", a);
  }
  void operator()(int64_t a) const {
    mString.AppendPrintf("int64_t(%" PRIi64 ")", a);
  }
  void operator()(uint64_t a) const {
    mString.AppendPrintf("uint64_t(%" PRIu64 ")", a);
  }
  void operator()(double a) const { mString.AppendPrintf("double(%f)", a); }
  void operator()(const DDRange& a) const {
    mString.AppendPrintf("%" PRIi64 "<=(%" PRIi64 "B)<%" PRIi64 "", a.mOffset,
                         a.mBytes, a.mOffset + a.mBytes);
  }
  void operator()(const nsresult& a) const {
    nsCString name;
    GetErrorName(a, name);
    mString.AppendPrintf("nsresult(%s =0x%08" PRIx32 ")", name.get(),
                         static_cast<uint32_t>(a));
  }
  void operator()(const MediaResult& a) const {
    nsCString name;
    GetErrorName(a.Code(), name);
    mString.AppendPrintf("MediaResult(%s =0x%08" PRIx32 ", \"%s\")", name.get(),
                         static_cast<uint32_t>(a.Code()), a.Message().get());
  }
};

void AppendToString(const DDLogValue& aValue, nsCString& aString) {
  aValue.match(LogValueMatcher{aString});
}

struct LogValueMatcherJson {
  JSONWriter& mJW;
  const char* mPropertyName;

  void operator()(const DDNoValue&) const { mJW.NullProperty(mPropertyName); }
  void operator()(const DDLogObject& a) const {
    mJW.StringProperty(
        mPropertyName,
        nsPrintfCString(R"("%s[%p]")", a.TypeName(), a.Pointer()).get());
  }
  void operator()(const char* a) const { mJW.StringProperty(mPropertyName, a); }
  void operator()(const nsCString& a) const {
    mJW.StringProperty(mPropertyName, a.Data());
  }
  void operator()(bool a) const { mJW.BoolProperty(mPropertyName, a); }
  void operator()(int8_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(uint8_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(int16_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(uint16_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(int32_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(uint32_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(int64_t a) const { mJW.IntProperty(mPropertyName, a); }
  void operator()(uint64_t a) const { mJW.DoubleProperty(mPropertyName, a); }
  void operator()(double a) const { mJW.DoubleProperty(mPropertyName, a); }
  void operator()(const DDRange& a) const {
    mJW.StartArrayProperty(mPropertyName);
    mJW.IntElement(a.mOffset);
    mJW.IntElement(a.mOffset + a.mBytes);
    mJW.EndArray();
  }
  void operator()(const nsresult& a) const {
    nsCString name;
    GetErrorName(a, name);
    mJW.StringProperty(mPropertyName, name.get());
  }
  void operator()(const MediaResult& a) const {
    nsCString name;
    GetErrorName(a.Code(), name);
    mJW.StringProperty(mPropertyName,
                       nsPrintfCString(R"lit("MediaResult(%s, %s)")lit",
                                       name.get(), a.Message().get())
                           .get());
  }
};

void ToJSON(const DDLogValue& aValue, JSONWriter& aJSONWriter,
            const char* aPropertyName) {
  aValue.match(LogValueMatcherJson{aJSONWriter, aPropertyName});
}

}  // namespace mozilla
