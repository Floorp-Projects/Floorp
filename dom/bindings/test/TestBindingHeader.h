/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TestBindingHeader_h
#define TestBindingHeader_h

#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class TestInterface : public nsISupports,
		      public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  // We need a GetParentObject to make binding codegen happy
  virtual nsISupports* GetParentObject();

  // And now our actual WebIDL API
  int8_t GetReadonlyByte(ErrorResult&);
  int8_t GetWritableByte(ErrorResult&);
  void SetWritableByte(int8_t, ErrorResult&);
  void PassByte(int8_t, ErrorResult&);
  int8_t ReceiveByte(ErrorResult&);

  int16_t GetReadonlyShort(ErrorResult&);
  int16_t GetWritableShort(ErrorResult&);
  void SetWritableShort(int16_t, ErrorResult&);
  void PassShort(int16_t, ErrorResult&);
  int16_t ReceiveShort(ErrorResult&);

  int32_t GetReadonlyLong(ErrorResult&);
  int32_t GetWritableLong(ErrorResult&);
  void SetWritableLong(int32_t, ErrorResult&);
  void PassLong(int32_t, ErrorResult&);
  int16_t ReceiveLong(ErrorResult&);

  int64_t GetReadonlyLongLong(ErrorResult&);
  int64_t GetWritableLongLong(ErrorResult&);
  void SetWritableLongLong(int64_t, ErrorResult&);
  void PassLongLong(int64_t, ErrorResult&);
  int64_t ReceiveLongLong(ErrorResult&);

  uint8_t GetReadonlyOctet(ErrorResult&);
  uint8_t GetWritableOctet(ErrorResult&);
  void SetWritableOctet(uint8_t, ErrorResult&);
  void PassOctet(uint8_t, ErrorResult&);
  uint8_t ReceiveOctet(ErrorResult&);

  uint16_t GetReadonlyUnsignedShort(ErrorResult&);
  uint16_t GetWritableUnsignedShort(ErrorResult&);
  void SetWritableUnsignedShort(uint16_t, ErrorResult&);
  void PassUnsignedShort(uint16_t, ErrorResult&);
  uint16_t ReceiveUnsignedShort(ErrorResult&);

  uint32_t GetReadonlyUnsignedLong(ErrorResult&);
  uint32_t GetWritableUnsignedLong(ErrorResult&);
  void SetWritableUnsignedLong(uint32_t, ErrorResult&);
  void PassUnsignedLong(uint32_t, ErrorResult&);
  uint32_t ReceiveUnsignedLong(ErrorResult&);

  uint64_t GetReadonlyUnsignedLongLong(ErrorResult&);
  uint64_t GetWritableUnsignedLongLong(ErrorResult&);
  void SetWritableUnsignedLongLong(uint64_t, ErrorResult&);
  void PassUnsignedLongLong(uint64_t, ErrorResult&);
  uint64_t ReceiveUnsignedLongLong(ErrorResult&);
 
private:
  // We add signatures here that _could_ start matching if the codegen
  // got data types wrong.  That way if it ever does we'll have a call
  // to these private deleted methods and compilation will fail.
  void SetReadonlyByte(int8_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableByte(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassByte(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyShort(int16_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableShort(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassShort(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyLong(int32_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassLong(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyLongLong(int64_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableLongLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassLongLong(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyOctet(uint8_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableOctet(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassOctet(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyUnsignedShort(uint16_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedShort(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassUnsignedShort(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyUnsignedLong(uint32_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassUnsignedLong(T, ErrorResult&) MOZ_DELETE;

  void SetReadonlyUnsignedLongLong(uint64_t, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void SetWritableUnsignedLongLong(T, ErrorResult&) MOZ_DELETE;
  template<typename T>
  void PassUnsignedLongLong(T, ErrorResult&) MOZ_DELETE;
};

} // namespace dom
} // namespace mozilla

#endif /* TestBindingHeader_h */
