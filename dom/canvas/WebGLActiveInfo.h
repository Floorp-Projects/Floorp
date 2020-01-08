/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_ACTIVE_INFO_H_
#define WEBGL_ACTIVE_INFO_H_

#include "GLDefs.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"  // NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS
#include "nsISupportsImpl.h"  // NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING
#include "nsString.h"
#include "nsWrapperCache.h"
#include "WebGLTypes.h"

namespace mozilla {

namespace ipc {
template <typename T>
struct PcqParamTraits;
}

class WebGLContext;

class WebGLActiveInfo {
 public:
  // ActiveInfo state:
  uint32_t mElemCount;      // `size`
  GLenum mElemType;         // `type`
  nsCString mBaseUserName;  // `name`, but ASCII, and without any final "[0]".

  // Not actually part of ActiveInfo:
  bool mIsArray;
  uint8_t mElemSize;
  nsCString mBaseMappedName;  // Without any final "[0]".
  webgl::AttribBaseType mBaseType = webgl::AttribBaseType::Float;

  bool IsSampler() const;

  WebGLActiveInfo(GLint elemCount, GLenum elemType, bool isArray,
                  const nsACString& baseUserName,
                  const nsACString& baseMappedName);

  WebGLActiveInfo(const WebGLActiveInfo& aOther);

  /* GLES 2.0.25, p33:
   *   This command will return as much information about active
   *   attributes as possible. If no information is available, length will
   *   be set to zero and name will be an empty string. This situation
   *   could arise if GetActiveAttrib is issued after a failed link.
   *
   * It's the same for GetActiveUniform.
   */
  static WebGLActiveInfo CreateInvalid() { return WebGLActiveInfo(); }

  // WebIDL attributes
  GLint Size() const { return mElemCount; }

  GLenum Type() const { return mElemType; }

  void GetName(nsString& retval) const {
    CopyASCIItoUTF16(mBaseUserName, retval);
    if (mIsArray) retval.AppendLiteral("[0]");
  }

 protected:
  friend mozilla::ipc::PcqParamTraits<WebGLActiveInfo>;
  friend Maybe<WebGLActiveInfo>;

  explicit WebGLActiveInfo()
      : mElemCount(0),
        mElemType(0),
        mBaseUserName(""),
        mIsArray(false),
        mElemSize(0),
        mBaseMappedName("") {}
};

class ClientWebGLActiveInfo final : public WebGLActiveInfo,
                                    public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ClientWebGLActiveInfo)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ClientWebGLActiveInfo)

  virtual JSObject* WrapObject(JSContext* js,
                               JS::Handle<JSObject*> givenProto) override;

  ClientWebGLContext* GetParentObject() const { return mWebGL; }

  ClientWebGLActiveInfo(ClientWebGLContext* webgl, GLint elemCount,
                        GLenum elemType, bool isArray,
                        const nsACString& baseUserName,
                        const nsACString& baseMappedName)
      : WebGLActiveInfo(elemCount, elemType, isArray, baseUserName,
                        baseMappedName),
        mWebGL(webgl) {}

  ClientWebGLActiveInfo(ClientWebGLContext* webgl,
                        const WebGLActiveInfo& aOther)
      : WebGLActiveInfo(aOther), mWebGL(webgl) {}

  ClientWebGLContext* const mWebGL;

 protected:
  ~ClientWebGLActiveInfo() {}
};

//////////

bool IsElemTypeSampler(GLenum elemType);

}  // namespace mozilla

#endif  // WEBGL_ACTIVE_INFO_H_
