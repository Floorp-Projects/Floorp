/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PushUtil_h
#define mozilla_dom_PushUtil_h

#include "nsTArray.h"

#include "mozilla/dom/TypedArray.h"

namespace mozilla {
namespace dom {

class OwningArrayBufferViewOrArrayBuffer;

class PushUtil final
{
private:
  PushUtil() = delete;

public:
  static bool
  CopyArrayBufferToArray(const ArrayBuffer& aBuffer,
                         nsTArray<uint8_t>& aArray);

  static bool
  CopyArrayBufferViewToArray(const ArrayBufferView& aView,
                             nsTArray<uint8_t>& aArray);

  static bool
  CopyBufferSourceToArray(const OwningArrayBufferViewOrArrayBuffer& aSource,
                          nsTArray<uint8_t>& aArray);

  static void
  CopyArrayToArrayBuffer(JSContext* aCx, const nsTArray<uint8_t>& aArray,
                         JS::MutableHandle<JSObject*> aValue, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PushUtil_h
