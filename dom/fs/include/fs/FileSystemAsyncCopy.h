/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_ASYNCCOPY_H_
#define DOM_FS_ASYNCCOPY_H_

#include "mozilla/MoveOnlyFunction.h"
#include "nsStreamUtils.h"

class nsIInputStream;
class nsIOutputStream;
class nsISerialEventTarget;

namespace mozilla::dom::fs {

nsresult AsyncCopy(nsIInputStream* aSource, nsIOutputStream* aSink,
                   nsISerialEventTarget* aIOTarget, const nsAsyncCopyMode aMode,
                   const bool aCloseSource, const bool aCloseSink,
                   std::function<void(uint32_t)>&& aProgressCallback,
                   MoveOnlyFunction<void(nsresult)>&& aCompleteCallback);

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_ASYNCCOPY_H_
