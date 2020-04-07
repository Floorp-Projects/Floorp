/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLIPDL_H_
#define WEBGLIPDL_H_

#include "WebGLTypes.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::webgl::ContextLossReason>
    : public ContiguousEnumSerializerInclusive<
          mozilla::webgl::ContextLossReason,
          mozilla::webgl::ContextLossReason::None,
          mozilla::webgl::ContextLossReason::Guilty> {};

// -

template <typename T>
bool ValidateParam(const T& val) {
  return ParamTraits<T>::Validate(val);
}

template <typename T>
struct ValidatedPlainOldDataSerializer : public PlainOldDataSerializer<T> {
  static void Write(Message* const msg, const T& in) {
    MOZ_ASSERT(ValidateParam(in));
    PlainOldDataSerializer<T>::Write(msg, in);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    if (!PlainOldDataSerializer<T>::Read(msg, itr, out)) return false;
    return ValidateParam(*out);
  }

  // static bool Validate(const T&) = 0;
};

// -

template <>
struct ParamTraits<mozilla::webgl::InitContextDesc> final
    : public ValidatedPlainOldDataSerializer<mozilla::webgl::InitContextDesc> {
  using T = mozilla::webgl::InitContextDesc;

  static bool Validate(const T& val) {
    return ValidateParam(val.options) && (val.size.x && val.size.y);
  }
};

template <>
struct ParamTraits<mozilla::WebGLContextOptions> final
    : public ValidatedPlainOldDataSerializer<mozilla::WebGLContextOptions> {
  using T = mozilla::WebGLContextOptions;

  static bool Validate(const T& val) {
    return ValidateParam(val.powerPreference);
  }
};

template <>
struct ParamTraits<mozilla::dom::WebGLPowerPreference> final
    : public ValidatedPlainOldDataSerializer<
          mozilla::dom::WebGLPowerPreference> {
  using T = mozilla::dom::WebGLPowerPreference;

  static bool Validate(const T& val) { return val <= T::High_performance; }
};

template <>
struct ParamTraits<mozilla::webgl::OpaqueFramebufferOptions> final
    : public PlainOldDataSerializer<mozilla::webgl::OpaqueFramebufferOptions> {
};

// -

template <>
struct ParamTraits<mozilla::webgl::InitContextResult> final {
  using T = mozilla::webgl::InitContextResult;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.error);
    WriteParam(msg, in.options);
    WriteParam(msg, in.limits);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->error) &&
           ReadParam(msg, itr, &out->options) &&
           ReadParam(msg, itr, &out->limits);
  }
};

template <>
struct ParamTraits<mozilla::webgl::ExtensionBits> final
    : public PlainOldDataSerializer<mozilla::webgl::ExtensionBits> {};

template <>
struct ParamTraits<mozilla::webgl::Limits> final
    : public PlainOldDataSerializer<mozilla::webgl::Limits> {};

}  // namespace IPC

#endif
