/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4ESDS_h_
#define MP4ESDS_h_

#include "nsTArray.h"
#include "MuxerOperation.h"

namespace mozilla {

class ISOControl;

/**
 * ESDS tag
 */
#define ESDescrTag        0x03

/**
 * 14496-1 '8.3.3 ES_Descriptor'.
 * It will get DecoderConfigDescriptor and SLConfigDescriptor from
 * AAC CSD data.
 */
class ES_Descriptor : public MuxerOperation {
public:
  // ISO BMFF members
  uint8_t tag;      // ESDescrTag
  uint8_t length;
  uint16_t ES_ID;
  std::bitset<1> streamDependenceFlag;
  std::bitset<1> URL_Flag;
  std::bitset<1> reserved;
  std::bitset<5> streamPriority;

  nsTArray<uint8_t> DecodeSpecificInfo;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;
  nsresult Find(const nsACString& aType,
                nsTArray<nsRefPtr<MuxerOperation>>& aOperations) MOZ_OVERRIDE;

  // ES_Descriptor methods
  ES_Descriptor(ISOControl* aControl);
  ~ES_Descriptor();

protected:
  ISOControl* mControl;
};

// 14496-14 5.6 'Sample Description Boxes'
// Box type: 'esds'
class ESDBox : public FullBox {
public:
  // ISO BMFF members
  nsRefPtr<ES_Descriptor> es_descriptor;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // ESDBox methods
  ESDBox(ISOControl* aControl);
  ~ESDBox();
};

// 14496-14 5.6 'Sample Description Boxes'
// Box type: 'mp4a'
class MP4AudioSampleEntry : public AudioSampleEntry {
public:
  // ISO BMFF members
  nsRefPtr<ESDBox> es;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  // MP4AudioSampleEntry methods
  MP4AudioSampleEntry(ISOControl* aControl);
  ~MP4AudioSampleEntry();
};

}

#endif // MP4ESDS_h_
