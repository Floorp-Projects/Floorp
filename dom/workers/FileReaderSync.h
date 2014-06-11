/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_filereadersync_h__
#define mozilla_dom_workers_filereadersync_h__

#include "Workers.h"

class nsIInputStream;
class nsIDOMBlob;

namespace mozilla {
class ErrorResult;

namespace dom {
class GlobalObject;
template<typename> class Optional;
}
}

BEGIN_WORKERS_NAMESPACE

class FileReaderSync MOZ_FINAL
{
  NS_INLINE_DECL_REFCOUNTING(FileReaderSync)

private:
  // Private destructor, to discourage deletion outside of Release():
  ~FileReaderSync()
  {
  }

  nsresult ConvertStream(nsIInputStream *aStream, const char *aCharset,
                         nsAString &aResult);

public:
  static already_AddRefed<FileReaderSync>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx);

  void ReadAsArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aScopeObj,
                         JS::Handle<JSObject*> aBlob,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv);
  void ReadAsBinaryString(JS::Handle<JSObject*> aBlob, nsAString& aResult,
                          ErrorResult& aRv);
  void ReadAsText(JS::Handle<JSObject*> aBlob,
                  const Optional<nsAString>& aEncoding,
                  nsAString& aResult, ErrorResult& aRv);
  void ReadAsDataURL(JS::Handle<JSObject*> aBlob, nsAString& aResult,
                     ErrorResult& aRv);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_filereadersync_h__
