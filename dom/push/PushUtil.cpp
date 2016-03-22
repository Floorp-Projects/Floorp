/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushUtil.h"

namespace mozilla {
namespace dom {

/* static */ bool
PushUtil::CopyArrayBufferToArray(const ArrayBuffer& aBuffer,
                                 nsTArray<uint8_t>& aArray)
{
  aBuffer.ComputeLengthAndData();
  return aArray.SetLength(aBuffer.Length(), fallible) &&
         aArray.ReplaceElementsAt(0, aBuffer.Length(), aBuffer.Data(),
                                  aBuffer.Length(), fallible);
}

/* static */ bool
PushUtil::CopyBufferSourceToArray(
  const OwningArrayBufferViewOrArrayBuffer& aSource, nsTArray<uint8_t>& aArray)
{
  if (aSource.IsArrayBuffer()) {
    return CopyArrayBufferToArray(aSource.GetAsArrayBuffer(), aArray);
  }
  if (aSource.IsArrayBufferView()) {
    const ArrayBufferView& view = aSource.GetAsArrayBufferView();
    view.ComputeLengthAndData();
    return aArray.SetLength(view.Length(), fallible) &&
           aArray.ReplaceElementsAt(0, view.Length(), view.Data(),
                                    view.Length(), fallible);
  }
  MOZ_CRASH("Uninitialized union: expected buffer or view");
}

/* static */ void
PushUtil::CopyArrayToArrayBuffer(JSContext* aCx,
                                 const nsTArray<uint8_t>& aArray,
                                 JS::MutableHandle<JSObject*> aValue,
                                 ErrorResult& aRv)
{
  if (aArray.IsEmpty()) {
    aValue.set(nullptr);
    return;
  }
  JS::Rooted<JSObject*> buffer(aCx, ArrayBuffer::Create(aCx,
                                                        aArray.Length(),
                                                        aArray.Elements()));
  if (NS_WARN_IF(!buffer)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  aValue.set(buffer);
}

} // namespace dom
} // namespace mozilla
