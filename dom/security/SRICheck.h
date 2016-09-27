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
class nsIUnicharStreamLoader;
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

  /**
   * Process the integrity attribute of the element.  A result of false
   * must prevent the resource from loading.
   */
  static nsresult VerifyIntegrity(const SRIMetadata& aMetadata,
                                  nsIUnicharStreamLoader* aLoader,
                                  const nsAString& aString,
                                  const nsACString& aSourceFileURI,
                                  nsIConsoleReportCollector* aReporter);
};

class SRICheckDataVerifier final
{
  public:
    SRICheckDataVerifier(const SRIMetadata& aMetadata,
                         const nsACString& aSourceFileURI,
                         nsIConsoleReportCollector* aReporter);

    nsresult Update(uint32_t aStringLen, const uint8_t* aString);
    nsresult Verify(const SRIMetadata& aMetadata, nsIChannel* aChannel,
                    const nsACString& aSourceFileURI,
                    nsIConsoleReportCollector* aReporter);

  private:
    nsCOMPtr<nsICryptoHash> mCryptoHash;
    nsAutoCString           mComputedHash;
    size_t                  mBytesHashed;
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
