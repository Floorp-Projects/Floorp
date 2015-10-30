/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISOControl.h"
#include "ISOMediaBoxes.h"
#include "EVRCBox.h"
#include "ISOTrackMetadata.h"

namespace mozilla {

nsresult
EVRCSampleEntry::Generate(uint32_t* aBoxSize)
{
  uint32_t box_size;
  nsresult rv = evrc_special_box->Generate(&box_size);
  NS_ENSURE_SUCCESS(rv, rv);
  size += box_size;

  *aBoxSize = size;
  return NS_OK;
}

nsresult
EVRCSampleEntry::Write()
{
  BoxSizeChecker checker(mControl, size);
  nsresult rv;
  rv = AudioSampleEntry::Write();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = evrc_special_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

EVRCSampleEntry::EVRCSampleEntry(ISOControl* aControl)
  : AudioSampleEntry(NS_LITERAL_CSTRING("sevc"), aControl)
{
  evrc_special_box = new EVRCSpecificBox(aControl);
  MOZ_COUNT_CTOR(EVRCSampleEntry);
}

EVRCSampleEntry::~EVRCSampleEntry()
{
  MOZ_COUNT_DTOR(EVRCSampleEntry);
}

nsresult
EVRCSpecificBox::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  FragmentBuffer* frag = mControl->GetFragment(Audio_Track);
  rv = frag->GetCSD(evrcDecSpecInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  size += evrcDecSpecInfo.Length();
  *aBoxSize = size;

  return NS_OK;
}

nsresult
EVRCSpecificBox::Write()
{
  BoxSizeChecker checker(mControl, size);
  Box::Write();
  mControl->Write(evrcDecSpecInfo.Elements(), evrcDecSpecInfo.Length());
  return NS_OK;
}

EVRCSpecificBox::EVRCSpecificBox(ISOControl* aControl)
  : Box(NS_LITERAL_CSTRING("devc"), aControl)
{
  MOZ_COUNT_CTOR(EVRCSpecificBox);
}

EVRCSpecificBox::~EVRCSpecificBox()
{
  MOZ_COUNT_DTOR(EVRCSpecificBox);
}

}
