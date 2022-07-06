/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SRIMetadata_h
#define mozilla_dom_SRIMetadata_h

#include "nsTArray.h"
#include "nsString.h"
#include "SRICheck.h"

namespace mozilla::dom {

class SRIMetadata final {
  friend class SRICheck;

 public:
  static const uint32_t MAX_ALTERNATE_HASHES = 256;
  static const int8_t UNKNOWN_ALGORITHM = -1;

  /**
   * Create an empty metadata object.
   */
  SRIMetadata() : mAlgorithmType(UNKNOWN_ALGORITHM), mEmpty(true) {}

  /**
   * Split a string token into the components of an SRI metadata
   * attribute.
   */
  explicit SRIMetadata(const nsACString& aToken);

  /**
   * Returns true when this object's hash algorithm is weaker than the
   * other object's hash algorithm.
   */
  bool operator<(const SRIMetadata& aOther) const;

  /**
   * Not implemented. Should not be used.
   */
  bool operator>(const SRIMetadata& aOther) const;

  /**
   * Add another metadata's hash to this one.
   */
  SRIMetadata& operator+=(const SRIMetadata& aOther);

  /**
   * Returns true when the two metadata use the same hash algorithm.
   */
  bool operator==(const SRIMetadata& aOther) const;

  bool IsEmpty() const { return mEmpty; }
  bool IsMalformed() const { return mHashes.IsEmpty() || mAlgorithm.IsEmpty(); }
  bool IsAlgorithmSupported() const {
    return mAlgorithmType != UNKNOWN_ALGORITHM;
  }
  bool IsValid() const { return !IsMalformed() && IsAlgorithmSupported(); }

  uint32_t HashCount() const { return mHashes.Length(); }
  void GetHash(uint32_t aIndex, nsCString* outHash) const;
  void GetAlgorithm(nsCString* outAlg) const { *outAlg = mAlgorithm; }
  void GetHashType(int8_t* outType, uint32_t* outLength) const;

  const nsString& GetIntegrityString() const { return mIntegrityString; }

  // Return true if:
  // - this integrity metadata is empty, or
  // - the other integrity metadata has the same hash algorithm and also the
  // same set of values otherwise, return false.
  //
  // This method simply checks if the other integrity metadata is identical to
  // this one (if it exists), so that a load that has been checked against that
  // other integrity metadata implies that the current integrity metadata is
  // also satisfied.
  bool CanTrustBeDelegatedTo(const SRIMetadata& aOther) const;

 private:
  CopyableTArray<nsCString> mHashes;
  nsString mIntegrityString;
  nsCString mAlgorithm;
  int8_t mAlgorithmType;
  bool mEmpty;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SRIMetadata_h
