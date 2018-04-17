/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SRICheck_h
#define mozilla_dom_SRICheck_h

#include "nsCOMPtr.h"
#include "nsICryptoHash.h"

class nsIChannel;
class nsIConsoleReportCollector;

namespace mozilla {
namespace dom {

class SRIMetadata;

class SRICheck final
{
public:
  static const uint32_t MAX_METADATA_LENGTH = 24*1024;
  static const uint32_t MAX_METADATA_TOKENS = 512;

  /**
   * Parse the multiple hashes specified in the integrity attribute and
   * return the strongest supported hash.
   */
  static nsresult IntegrityMetadata(const nsAString& aMetadataList,
                                    const nsACString& aSourceFileURI,
                                    nsIConsoleReportCollector* aReporter,
                                    SRIMetadata* outMetadata);
};

// The SRICheckDataVerifier can be used in 2 different mode:
//
// 1. The streaming mode involves reading bytes from an input, and to use
//    the |Update| function to stream new bytes, and to use the |Verify|
//    function to check the hash of the content with the hash provided by
//    the metadata.
//
//    Optionally, one can serialize the verified hash with |ExportDataSummary|,
//    in a buffer in order to rely on the second mode the next time.
//
// 2. The pre-computed mode, involves reading a hash with |ImportDataSummary|,
//    which got exported by the SRICheckDataVerifier and potentially cached, and
//    then use the |Verify| function to check against the hash provided by the
//    metadata.
class SRICheckDataVerifier final
{
  public:
    SRICheckDataVerifier(const SRIMetadata& aMetadata,
                         const nsACString& aSourceFileURI,
                         nsIConsoleReportCollector* aReporter);

    // Append the following bytes to the content used to compute the hash. Once
    // all bytes are streamed, use the Verify function to check the integrity.
    nsresult Update(uint32_t aStringLen, const uint8_t* aString);

    // Verify that the computed hash corresponds to the metadata.
    nsresult Verify(const SRIMetadata& aMetadata, nsIChannel* aChannel,
                    const nsACString& aSourceFileURI,
                    nsIConsoleReportCollector* aReporter);

    bool IsComplete() const {
      return mComplete;
    }

    // Report the length of the computed hash and its type, such that we can
    // reserve the space for encoding it in a vector.
    uint32_t DataSummaryLength();
    static uint32_t EmptyDataSummaryLength();

    // Write the computed hash and its type in a pre-allocated buffer.
    nsresult ExportDataSummary(uint32_t aDataLen, uint8_t* aData);
    static nsresult ExportEmptyDataSummary(uint32_t aDataLen, uint8_t* aData);

    // Report the length of the computed hash and its type, such that we can
    // skip these data while reading a buffer.
    static nsresult DataSummaryLength(uint32_t aDataLen, const uint8_t* aData, uint32_t* length);

    // Extract the computed hash and its type, such that we can |Verify| if it
    // matches the metadata. The buffer should be at least the same size or
    // larger than the value returned by |DataSummaryLength|.
    nsresult ImportDataSummary(uint32_t aDataLen, const uint8_t* aData);

  private:
    nsCOMPtr<nsICryptoHash> mCryptoHash;
    nsAutoCString           mComputedHash;
    size_t                  mBytesHashed;
    uint32_t                mHashLength;
    int8_t                  mHashType;
    bool                    mInvalidMetadata;
    bool                    mComplete;

    nsresult EnsureCryptoHash();
    nsresult Finish();
    nsresult VerifyHash(const SRIMetadata& aMetadata, uint32_t aHashIndex,
                        const nsACString& aSourceFileURI,
                        nsIConsoleReportCollector* aReporter);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SRICheck_h
