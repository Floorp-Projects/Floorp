/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
#define DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_

#include "mozilla/dom/WritableStream.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams;

class FileSystemWritableFileStream final : public WritableStream {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemWritableFileStream,
                                           WritableStream)

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> Write(
      const ArrayBufferViewOrArrayBufferOrBlobOrUSVStringOrWriteParams& aData,
      ErrorResult& aError);

  already_AddRefed<Promise> Seek(uint64_t aPosition, ErrorResult& aError);

  already_AddRefed<Promise> Truncate(uint64_t aSize, ErrorResult& aError);

 private:
  ~FileSystemWritableFileStream() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMWRITABLEFILESTREAM_H_
