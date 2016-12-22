/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaMIMETypes_h_
#define MediaMIMETypes_h_

#include "mozilla/Maybe.h"
#include "nsString.h"
#include "VideoUtils.h"

namespace mozilla {

// Class containing pointing at a media MIME "type/subtype" string literal.
// See IsMediaMIMEType for restrictions.
// Mainly used to help construct a MediaMIMEType through the statically-checked
// MEDIAMIMETYPE macro, or to compare a MediaMIMEType to a literal.
class DependentMediaMIMEType
{
public:
  // Construction from a literal. Checked in debug builds.
  // Use MEDIAMIMETYPE macro instead, for static checking.
  template <size_t N>
  explicit DependentMediaMIMEType(const char (&aType)[N])
    : mMIMEType(aType, N - 1)
  {
    MOZ_ASSERT(IsMediaMIMEType(aType, N - 1), "Invalid media MIME type");
  }

  // MIME "type/subtype".
  const nsDependentCString& AsDependentString() const { return mMIMEType; }

private:
  nsDependentCString mMIMEType;
};

// Instantiate a DependentMediaMIMEType from a literal. Statically checked.
#define MEDIAMIMETYPE(LIT)                                            \
  static_cast<const DependentMediaMIMEType&>(                         \
    []() {                                                            \
      static_assert(IsMediaMIMEType(LIT), "Invalid media MIME type"); \
      return DependentMediaMIMEType(LIT);                             \
    }())

// Class containing only pre-parsed lowercase media MIME type/subtype.
class MediaMIMEType
{
public:
  // Construction from a DependentMediaMIMEType, with its inherent checks.
  // Implicit so MEDIAMIMETYPE can be used wherever a MediaMIMEType is expected.
  MOZ_IMPLICIT MediaMIMEType(const DependentMediaMIMEType& aType)
    : mMIMEType(aType.AsDependentString())
  {}

  // MIME "type/subtype", always lowercase.
  const nsACString& AsString() const { return mMIMEType; }

  // Comparison with DependentMediaMIMEType.
  // Useful to compare to MEDIAMIMETYPE literals.
  bool operator==(const DependentMediaMIMEType& aOther) const
  {
    return mMIMEType.Equals(aOther.AsDependentString());
  }
  bool operator!=(const DependentMediaMIMEType& aOther) const
  {
    return !mMIMEType.Equals(aOther.AsDependentString());
  }

  bool operator==(const MediaMIMEType& aOther) const
  {
    return mMIMEType.Equals(aOther.mMIMEType);
  }
  bool operator!=(const MediaMIMEType& aOther) const
  {
    return !mMIMEType.Equals(aOther.mMIMEType);
  }

  // True if type starts with "application/".
  bool HasApplicationMajorType() const;
  // True if type starts with "audio/".
  // Note that some audio content could be stored in a "video/..." container!
  bool HasAudioMajorType() const;
  // True if type starts with "video/".
  // Note that this does not guarantee 100% that the content is actually video!
  // (e.g., "video/webm" could contain a vorbis audio track.)
  bool HasVideoMajorType() const;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  friend Maybe<MediaMIMEType> MakeMediaMIMEType(const nsAString& aType);
  friend class MediaExtendedMIMEType;
  explicit MediaMIMEType(const nsACString& aType);

  nsCString mMIMEType; // UTF8 MIME "type/subtype".
};

Maybe<MediaMIMEType> MakeMediaMIMEType(const nsAString& aType);
Maybe<MediaMIMEType> MakeMediaMIMEType(const nsACString& aType);
Maybe<MediaMIMEType> MakeMediaMIMEType(const char* aType);


// A list of codecs attached to a MediaExtendedMIMEType.
class MediaCodecs
{
public:
  MediaCodecs() {}
  // Construction from a comma-separated list of codecs. Unchecked.
  explicit MediaCodecs(const nsAString& aCodecs)
    : mCodecs(aCodecs)
  {}
  // Construction from a literal comma-separated list of codecs. Unchecked.
  template <size_t N>
  explicit MediaCodecs(const char (&aCodecs)[N])
    : mCodecs(NS_ConvertUTF8toUTF16(aCodecs, N - 1))
  {}

  bool IsEmpty() const { return mCodecs.IsEmpty(); }
  const nsAString& AsString() const { return mCodecs; }

  using RangeType =
    const StringListRange<nsString, StringListRangeEmptyItems::ProcessEmptyItems>;

  // Produces a range object with begin()&end(), can be used in range-for loops.
  // This will iterate through all codecs, even empty ones (except if the
  // original list was an empty string). Iterators dereference to
  // 'const nsDependentString', valid for as long as this MediaCodecs object.
  RangeType Range() const
  {
    return RangeType(mCodecs);
  };

  bool Contains(const nsAString& aCodec) const;
  bool ContainsAll(const MediaCodecs& aCodecs) const;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // UTF16 comma-separated list of codecs.
  // See http://www.rfc-editor.org/rfc/rfc4281.txt for the description
  // of the 'codecs' parameter.
  nsString mCodecs;
};


// Class containing pre-parsed media MIME type parameters, e.g.:
// MIME type/subtype, optional codecs, etc.
class MediaExtendedMIMEType
{
public:
  explicit MediaExtendedMIMEType(const MediaMIMEType& aType);
  explicit MediaExtendedMIMEType(MediaMIMEType&& aType);

  // MIME "type/subtype".
  const MediaMIMEType& Type() const { return mMIMEType; }

  // Was there an explicit 'codecs' parameter provided?
  bool HaveCodecs() const { return mHaveCodecs; }
  // Codecs. May be empty if not provided or explicitly provided as empty.
  const MediaCodecs& Codecs() const { return mCodecs; }

  // Sizes and rates.
  Maybe<int32_t> GetWidth() const { return GetMaybeNumber(mWidth); }
  Maybe<int32_t> GetHeight() const { return GetMaybeNumber(mHeight); }
  Maybe<int32_t> GetFramerate() const { return GetMaybeNumber(mFramerate); }
  Maybe<int32_t> GetBitrate() const { return GetMaybeNumber(mBitrate); }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  friend Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(const nsAString& aType);
  MediaExtendedMIMEType(const nsACString& aMIMEType,
                        bool aHaveCodecs, const nsAString& aCodecs,
                        int32_t aWidth, int32_t aHeight,
                        int32_t aFramerate, int32_t aBitrate);

  Maybe<int32_t> GetMaybeNumber(int32_t aNumber) const
  {
    return (aNumber < 0) ? Maybe<int32_t>(Nothing()) : Some(int32_t(aNumber));
  }

  MediaMIMEType mMIMEType; // MIME type/subtype.
  bool mHaveCodecs = false; // If false, mCodecs must be empty.
  MediaCodecs mCodecs;
  int32_t mWidth = -1; // -1 if not provided.
  int32_t mHeight = -1; // -1 if not provided.
  int32_t mFramerate = -1; // -1 if not provided.
  int32_t mBitrate = -1; // -1 if not provided.
};

Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(const nsAString& aType);
Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(const nsACString& aType);
Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(const char* aType);

} // namespace mozilla

#endif // MediaMIMETypes_h_
