/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_PRECISION_FORMAT_H_
#define WEBGL_SHADER_PRECISION_FORMAT_H_

#include "GLTypes.h"
#include "nsISupports.h"

namespace mozilla {

namespace ipc {
template <typename T>
struct PcqParamTraits;
};

class WebGLShaderPrecisionFormat final {
 public:
  WebGLShaderPrecisionFormat(GLint rangeMin, GLint rangeMax, GLint precision)
      : mRangeMin(rangeMin), mRangeMax(rangeMax), mPrecision(precision) {}

  GLint RangeMin() const { return mRangeMin; }
  GLint RangeMax() const { return mRangeMax; }
  GLint Precision() const { return mPrecision; }

 protected:
  friend struct mozilla::ipc::PcqParamTraits<WebGLShaderPrecisionFormat>;

  GLint mRangeMin;
  GLint mRangeMax;
  GLint mPrecision;
};

class ClientWebGLShaderPrecisionFormat final {
 public:
  NS_INLINE_DECL_REFCOUNTING(ClientWebGLShaderPrecisionFormat)
  bool WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto,
                  JS::MutableHandle<JSObject*> reflector);
  ClientWebGLShaderPrecisionFormat(GLint rangeMin, GLint rangeMax,
                                   GLint precision)
      : mSPF(rangeMin, rangeMax, precision) {}

  ClientWebGLShaderPrecisionFormat(const WebGLShaderPrecisionFormat& aOther)
      : mSPF(aOther.RangeMin(), aOther.RangeMax(), aOther.Precision()) {}

  // WebIDL WebGLShaderPrecisionFormat API
  GLint RangeMin() const { return mSPF.RangeMin(); }
  GLint RangeMax() const { return mSPF.RangeMax(); }
  GLint Precision() const { return mSPF.Precision(); }
  WebGLShaderPrecisionFormat& GetObject() { return mSPF; }

 protected:
  ~ClientWebGLShaderPrecisionFormat() {}
  WebGLShaderPrecisionFormat mSPF;
};

}  // namespace mozilla

#endif  // WEBGL_SHADER_PRECISION_FORMAT_H_
