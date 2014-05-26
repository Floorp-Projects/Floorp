/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CryptoBuffer.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {
namespace dom {

uint8_t*
CryptoBuffer::Assign(const uint8_t* aData, uint32_t aLength)
{
  return ReplaceElementsAt(0, Length(), aData, aLength);
}

uint8_t*
CryptoBuffer::Assign(const SECItem* aItem)
{
  MOZ_ASSERT(aItem);
  return Assign(aItem->data, aItem->len);
}

uint8_t*
CryptoBuffer::Assign(const ArrayBuffer& aData)
{
  return Assign(aData.Data(), aData.Length());
}

uint8_t*
CryptoBuffer::Assign(const ArrayBufferView& aData)
{
  return Assign(aData.Data(), aData.Length());
}

uint8_t*
CryptoBuffer::Assign(const ArrayBufferViewOrArrayBuffer& aData)
{
  if (aData.IsArrayBufferView()) {
    return Assign(aData.GetAsArrayBufferView());
  } else if (aData.IsArrayBuffer()) {
    return Assign(aData.GetAsArrayBuffer());
  }

  // If your union is uninitialized, something's wrong
  MOZ_ASSERT(false);
  SetLength(0);
  return nullptr;
}

uint8_t*
CryptoBuffer::Assign(const OwningArrayBufferViewOrArrayBuffer& aData)
{
  if (aData.IsArrayBufferView()) {
    return Assign(aData.GetAsArrayBufferView());
  } else if (aData.IsArrayBuffer()) {
    return Assign(aData.GetAsArrayBuffer());
  }

  // If your union is uninitialized, something's wrong
  MOZ_ASSERT(false);
  SetLength(0);
  return nullptr;
}

SECItem*
CryptoBuffer::ToSECItem()
{
  uint8_t* data = (uint8_t*) moz_malloc(Length());
  if (!data) {
    return nullptr;
  }

  SECItem* item = new SECItem();
  item->type = siBuffer;
  item->data = data;
  item->len = Length();

  memcpy(item->data, Elements(), Length());

  return item;
}

// "BigInt" comes from the WebCrypto spec
// ("unsigned long" isn't very "big", of course)
// Likewise, the spec calls for big-endian ints
bool
CryptoBuffer::GetBigIntValue(unsigned long& aRetVal)
{
  if (Length() > sizeof(aRetVal)) {
    return false;
  }

  aRetVal = 0;
  for (size_t i=0; i < Length(); ++i) {
    aRetVal = (aRetVal << 8) + ElementAt(i);
  }
  return true;
}

} // namespace dom
} // namespace mozilla
