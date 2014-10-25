/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CONTAINERPARSER_H_
#define MOZILLA_CONTAINERPARSER_H_

#include "nsTArray.h"

namespace mozilla {

class ContainerParser {
public:
  ContainerParser() : mHasInitData(false) {}
  virtual ~ContainerParser() {}

  // Return true if aData starts with an initialization segment.
  // The base implementation exists only for debug logging and is expected
  // to be called first from the overriding implementation.
  virtual bool IsInitSegmentPresent(const uint8_t* aData, uint32_t aLength);

  // Return true if aData starts with a media segment.
  // The base implementation exists only for debug logging and is expected
  // to be called first from the overriding implementation.
  virtual bool IsMediaSegmentPresent(const uint8_t* aData, uint32_t aLength);

  // Parse aData to extract the start and end frame times from the media
  // segment.  aData may not start on a parser sync boundary.  Return true
  // if aStart and aEnd have been updated.
  virtual bool ParseStartAndEndTimestamps(const uint8_t* aData, uint32_t aLength,
                                          int64_t& aStart, int64_t& aEnd);

  // Compare aLhs and rHs, considering any error that may exist in the
  // timestamps from the format's base representation.  Return true if aLhs
  // == aRhs within the error epsilon.
  virtual bool TimestampsFuzzyEqual(int64_t aLhs, int64_t aRhs);

  const nsTArray<uint8_t>& InitData();

  bool HasInitData()
  {
    return mHasInitData;
  }

  static ContainerParser* CreateForMIMEType(const nsACString& aType);

protected:
  nsTArray<uint8_t> mInitData;
  bool mHasInitData;
};

} // namespace mozilla
#endif /* MOZILLA_CONTAINERPARSER_H_ */
