/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_filereadersync_h__
#define mozilla_dom_workers_filereadersync_h__

#include "Workers.h"
#include "mozilla/dom/workers/bindings/DOMBindingBase.h"

#include "nsICharsetDetectionObserver.h"
#include "nsStringGlue.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"

class nsIInputStream;
class nsIDOMBlob;

BEGIN_WORKERS_NAMESPACE

class FileReaderSync MOZ_FINAL : public DOMBindingBase,
                                 public nsICharsetDetectionObserver
{
  nsCString mCharset;
  nsresult ConvertStream(nsIInputStream *aStream, const char *aCharset,
                         nsAString &aResult);
  nsresult GuessCharset(nsIInputStream *aStream, nsACString &aCharset);

public:
  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  static FileReaderSync*
  Constructor(const WorkerGlobalObject& aGlobal, ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED

  FileReaderSync(JSContext* aCx);

  JSObject* ReadAsArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aBlob,
                              ErrorResult& aRv);
  void ReadAsBinaryString(JS::Handle<JSObject*> aBlob, nsAString& aResult,
                          ErrorResult& aRv);
  void ReadAsText(JS::Handle<JSObject*> aBlob,
                  const Optional<nsAString>& aEncoding,
                  nsAString& aResult, ErrorResult& aRv);
  void ReadAsDataURL(JS::Handle<JSObject*> aBlob, nsAString& aResult,
                     ErrorResult& aRv);

  // From nsICharsetDetectionObserver
  NS_IMETHOD Notify(const char *aCharset, nsDetectionConfident aConf) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_filereadersync_h__
