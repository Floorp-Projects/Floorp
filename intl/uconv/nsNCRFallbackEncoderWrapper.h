/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNCRFallbackEncoderWrapper_h_
#define nsNCRFallbackEncoderWrapper_h_

#include "nsIUnicodeEncoder.h"

class nsNCRFallbackEncoderWrapper
{
public:
  explicit nsNCRFallbackEncoderWrapper(const nsACString& aEncoding);
  ~nsNCRFallbackEncoderWrapper();

  /**
   * Convert text to bytes with decimal numeric character reference replacement
   * for unmappables.
   *
   * @param aUtf16 UTF-16 input
   * @param aBytes conversion output
   * @return true on success and false on failure (OOM)
   */
  bool Encode(const nsAString& aUtf16,
              nsACString& aBytes);

private:
  bool WriteNCR(nsACString& aBytes, uint32_t& aDstWritten, int32_t aUnmappable);

  nsCOMPtr<nsIUnicodeEncoder> mEncoder;
};

#endif /* nsNCRFallbackEncoderWrapper_h_ */
