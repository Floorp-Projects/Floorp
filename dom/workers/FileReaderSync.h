/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_filereadersync_h__
#define mozilla_dom_filereadersync_h__

#include "Workers.h"

class nsIInputStream;

namespace mozilla {
class ErrorResult;

namespace dom {
class Blob;
class GlobalObject;
template<typename> class Optional;

class FileReaderSync final
{
  NS_INLINE_DECL_REFCOUNTING(FileReaderSync)

private:
  // Private destructor, to discourage deletion outside of Release():
  ~FileReaderSync()
  {
  }

  nsresult ConvertStream(nsIInputStream *aStream, const char *aCharset,
                         nsAString &aResult);

  nsresult SyncRead(nsIInputStream* aStream, char* aBuffer,
                    uint32_t aBufferSize, uint32_t* aRead);

public:
  static already_AddRefed<FileReaderSync>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  void ReadAsArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aScopeObj,
                         Blob& aBlob, JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv);
  void ReadAsBinaryString(Blob& aBlob, nsAString& aResult, ErrorResult& aRv);
  void ReadAsText(Blob& aBlob, const Optional<nsAString>& aEncoding,
                  nsAString& aResult, ErrorResult& aRv);
  void ReadAsDataURL(Blob& aBlob, nsAString& aResult, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_filereadersync_h__
