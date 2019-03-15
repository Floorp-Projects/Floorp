/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_nsIIPCSerializableInputStream_h
#define mozilla_ipc_nsIIPCSerializableInputStream_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

namespace dom {

class ContentChild;
class ContentParent;

}  // namespace dom

namespace ipc {

class FileDescriptor;
class InputStreamParams;
class PBackgroundChild;
class PBackgroundParent;

}  // namespace ipc

}  // namespace mozilla

#define NS_IIPCSERIALIZABLEINPUTSTREAM_IID           \
  {                                                  \
    0xb0211b14, 0xea6d, 0x40d4, {                    \
      0x87, 0xb5, 0x7b, 0xe3, 0xdf, 0xac, 0x09, 0xd1 \
    }                                                \
  }

class NS_NO_VTABLE nsIIPCSerializableInputStream : public nsISupports {
 public:
  typedef nsTArray<mozilla::ipc::FileDescriptor> FileDescriptorArray;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed,
                         mozilla::dom::ContentChild* aManager) = 0;

  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed,
                         mozilla::ipc::PBackgroundChild* aManager) = 0;

  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed,
                         mozilla::dom::ContentParent* aManager) = 0;

  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed,
                         mozilla::ipc::PBackgroundParent* aManager) = 0;

  virtual bool Deserialize(const mozilla::ipc::InputStreamParams& aParams,
                           const FileDescriptorArray& aFileDescriptors) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIPCSerializableInputStream,
                              NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

#define NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM                             \
  virtual void Serialize(mozilla::ipc::InputStreamParams&,                \
                         FileDescriptorArray&, bool, uint32_t, uint32_t*, \
                         mozilla::dom::ContentChild*) override;           \
                                                                          \
  virtual void Serialize(mozilla::ipc::InputStreamParams&,                \
                         FileDescriptorArray&, bool, uint32_t, uint32_t*, \
                         mozilla::ipc::PBackgroundChild*) override;       \
                                                                          \
  virtual void Serialize(mozilla::ipc::InputStreamParams&,                \
                         FileDescriptorArray&, bool, uint32_t, uint32_t*, \
                         mozilla::dom::ContentParent*) override;          \
                                                                          \
  virtual void Serialize(mozilla::ipc::InputStreamParams&,                \
                         FileDescriptorArray&, bool, uint32_t, uint32_t*, \
                         mozilla::ipc::PBackgroundParent*) override;      \
                                                                          \
  virtual bool Deserialize(const mozilla::ipc::InputStreamParams&,        \
                           const FileDescriptorArray&) override;

#define NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(_to)                          \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::dom::ContentChild* aManager) override {      \
    _to Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,          \
                  aSizeUsed, aManager);                                        \
  }                                                                            \
                                                                               \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::ipc::PBackgroundChild* aManager) override {  \
    _to Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,          \
                  aSizeUsed, aManager);                                        \
  }                                                                            \
                                                                               \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::dom::ContentParent* aManager) override {     \
    _to Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,          \
                  aSizeUsed, aManager);                                        \
  }                                                                            \
                                                                               \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::ipc::PBackgroundParent* aManager) override { \
    _to Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,          \
                  aSizeUsed, aManager);                                        \
  }                                                                            \
                                                                               \
  virtual bool Deserialize(const mozilla::ipc::InputStreamParams& aParams,     \
                           const FileDescriptorArray& aFileDescriptors)        \
      override {                                                               \
    return _to Deserialize(aParams, aFileDescriptors);                         \
  }

#define NS_FORWARD_SAFE_NSIIPCSERIALIZABLEINPUTSTREAM(_to)                     \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::dom::ContentChild* aManager) override {      \
    if (_to) {                                                                 \
      _to->Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,       \
                     aSizeUsed, aManager);                                     \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::ipc::PBackgroundChild* aManager) override {  \
    if (_to) {                                                                 \
      _to->Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,       \
                     aSizeUsed, aManager);                                     \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::dom::ContentParent* aManager) override {     \
    if (_to) {                                                                 \
      _to->Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,       \
                     aSizeUsed, aManager);                                     \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,             \
                         FileDescriptorArray& aFileDescriptors,                \
                         bool aDelayedStart, uint32_t aMaxSize,                \
                         uint32_t* aSizeUsed,                                  \
                         mozilla::ipc::PBackgroundParent* aManager) override { \
    if (_to) {                                                                 \
      _to->Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,       \
                     aSizeUsed, aManager);                                     \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual bool Deserialize(const mozilla::ipc::InputStreamParams& aParams,     \
                           const FileDescriptorArray& aFileDescriptors)        \
      override {                                                               \
    return _to ? _to->Deserialize(aParams, aFileDescriptors) : false;          \
  }

#endif  // mozilla_ipc_nsIIPCSerializableInputStream_h
