/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AMRBOX_h_
#define AMRBOX_h_

#include "nsTArray.h"
#include "MuxerOperation.h"

namespace mozilla {

class ISOControl;

// 3GPP TS 26.244 6.7 'AMRSpecificBox field for AMRSampleEntry box'
// Box type: 'damr'
class AMRSpecificBox : public Box {
public:
  // 3GPP members
  nsTArray<uint8_t> amrDecSpecInfo;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) override;
  nsresult Write() override;

  // AMRSpecificBox methods
  AMRSpecificBox(ISOControl* aControl);
  ~AMRSpecificBox();
};

// 3GPP TS 26.244 6.5 'AMRSampleEntry box'
// Box type: 'sawb'
class AMRSampleEntry : public AudioSampleEntry {
public:
  // 3GPP members
  RefPtr<AMRSpecificBox> amr_special_box;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) override;
  nsresult Write() override;

  // AMRSampleEntry methods
  AMRSampleEntry(ISOControl* aControl);
  ~AMRSampleEntry();
};

}

#endif // AMRBOX_h_
