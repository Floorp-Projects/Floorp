/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ISOControl.h"
#include "ISOMediaBoxes.h"
#include "AMRBox.h"
#include "ISOTrackMetadata.h"

namespace mozilla {

nsresult
AMRSampleEntry::Generate(uint32_t* aBoxSize)
{
  uint32_t box_size;
  nsresult rv = amr_special_box->Generate(&box_size);
  NS_ENSURE_SUCCESS(rv, rv);
  size += box_size;

  *aBoxSize = size;
  return NS_OK;
}

nsresult
AMRSampleEntry::Write()
{
  BoxSizeChecker checker(mControl, size);
  nsresult rv;
  rv = AudioSampleEntry::Write();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = amr_special_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

AMRSampleEntry::AMRSampleEntry(ISOControl* aControl)
  : AudioSampleEntry(NS_LITERAL_CSTRING("samr"), aControl)
{
  amr_special_box = new AMRSpecificBox(aControl);
  MOZ_COUNT_CTOR(AMRSampleEntry);
}

AMRSampleEntry::~AMRSampleEntry()
{
  MOZ_COUNT_DTOR(AMRSampleEntry);
}

nsresult
AMRSpecificBox::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  FragmentBuffer* frag = mControl->GetFragment(Audio_Track);
  rv = frag->GetCSD(amrDecSpecInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  size += amrDecSpecInfo.Length();
  *aBoxSize = size;

  return NS_OK;
}

nsresult
AMRSpecificBox::Write()
{
  BoxSizeChecker checker(mControl, size);
  Box::Write();
  mControl->Write(amrDecSpecInfo.Elements(), amrDecSpecInfo.Length());
  return NS_OK;
}

AMRSpecificBox::AMRSpecificBox(ISOControl* aControl)
  : Box(NS_LITERAL_CSTRING("damr"), aControl)
{
  MOZ_COUNT_CTOR(AMRSpecificBox);
}

AMRSpecificBox::~AMRSpecificBox()
{
  MOZ_COUNT_DTOR(AMRSpecificBox);
}

}
