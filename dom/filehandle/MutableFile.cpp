/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MutableFile.h"

#include "nsDebug.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

MutableFileBase::MutableFileBase()
{
}

MutableFileBase::~MutableFileBase()
{
}

// virtual
already_AddRefed<nsISupports>
MutableFileBase::CreateStream(bool aReadOnly)
{
  nsresult rv;

  if (aReadOnly) {
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), mFile, -1, -1,
                                    nsIFileInputStream::DEFER_OPEN);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    return stream.forget();
  }

  nsCOMPtr<nsIFileStream> stream;
  rv = NS_NewLocalFileStream(getter_AddRefs(stream), mFile, -1, -1,
                             nsIFileStream::DEFER_OPEN);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return stream.forget();
}

} // namespace dom
} // namespace mozilla
