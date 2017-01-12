/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaContentType_h_
#define MediaContentType_h_

#include "mozilla/Maybe.h"
#include "nsString.h"

namespace mozilla {

// Structure containing pre-parsed content type parameters, e.g.:
// MIME type, optional codecs, etc.
class MediaContentType
{
public:
  // MIME type. Guaranteed not to be empty.
  const nsACString& GetMIMEType() const { return mMIMEType; }

  // Was there an explicit 'codecs' parameter provided?
  bool HaveCodecs() const { return mHaveCodecs; }
  // Codecs. May be empty if not provided or explicitly provided as empty.
  const nsAString& GetCodecs() const { return mCodecs; }

  // Sizes and rates.
  Maybe<int32_t> GetWidth() const { return GetMaybeNumber(mWidth); }
  Maybe<int32_t> GetHeight() const { return GetMaybeNumber(mHeight); }
  Maybe<int32_t> GetFramerate() const { return GetMaybeNumber(mFramerate); }
  Maybe<int32_t> GetBitrate() const { return GetMaybeNumber(mBitrate); }

private:
  friend Maybe<MediaContentType> MakeMediaContentType(const nsAString& aType);
  bool Populate(const nsAString& aType);

  Maybe<int32_t> GetMaybeNumber(int32_t aNumber) const
  {
    return (aNumber < 0) ? Maybe<int32_t>(Nothing()) : Some(int32_t(aNumber));
  }

  nsCString mMIMEType; // UTF8 MIME type.
  bool mHaveCodecs; // If false, mCodecs must be empty.
  nsString mCodecs;
  int32_t mWidth; // -1 if not provided.
  int32_t mHeight; // -1 if not provided.
  int32_t mFramerate; // -1 if not provided.
  int32_t mBitrate; // -1 if not provided.
};

Maybe<MediaContentType> MakeMediaContentType(const nsAString& aType);
Maybe<MediaContentType> MakeMediaContentType(const nsACString& aType);
Maybe<MediaContentType> MakeMediaContentType(const char* aType);

} // namespace mozilla

#endif // MediaContentType_h_
