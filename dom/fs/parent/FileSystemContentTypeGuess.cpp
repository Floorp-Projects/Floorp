/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemContentTypeGuess.h"

#include "mozilla/dom/mime_guess_ffi_generated.h"
#include "nsString.h"

namespace mozilla::dom::fs {

ContentType FileSystemContentTypeGuess::FromPath(const Name& aPath) {
  NS_ConvertUTF16toUTF8 path(aPath);
  ContentType contentType;
  mimeGuessFromPath(&path, &contentType);

  if (contentType.IsEmpty()) {
    return VoidCString();
  }

  return contentType;
}

}  // namespace mozilla::dom::fs
