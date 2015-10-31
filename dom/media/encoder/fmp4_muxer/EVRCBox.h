/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EVRCBOX_h_
#define EVRCBOX_h_

#include "nsTArray.h"
#include "MuxerOperation.h"

namespace mozilla {

class ISOControl;

// 3GPP TS 26.244 6.7 'EVRCSpecificBox field for EVRCSampleEntry box'
// Box type: 'devc'
class EVRCSpecificBox : public Box {
public:
  // 3GPP members
  nsTArray<uint8_t> evrcDecSpecInfo;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) override;
  nsresult Write() override;

  // EVRCSpecificBox methods
  EVRCSpecificBox(ISOControl* aControl);
  ~EVRCSpecificBox();
};

// 3GPP TS 26.244 6.5 'EVRCSampleEntry box'
// Box type: 'sevc'
class EVRCSampleEntry : public AudioSampleEntry {
public:
  // 3GPP members
  RefPtr<EVRCSpecificBox> evrc_special_box;

  // MuxerOperation methods
  nsresult Generate(uint32_t* aBoxSize) override;
  nsresult Write() override;

  // EVRCSampleEntry methods
  EVRCSampleEntry(ISOControl* aControl);
  ~EVRCSampleEntry();
};

}

#endif // EVRCBOX_h_
