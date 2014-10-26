/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include "ISOControl.h"
#include "ISOMediaBoxes.h"
#include "AVCBox.h"

namespace mozilla {

nsresult
AVCSampleEntry::Generate(uint32_t* aBoxSize)
{
  uint32_t avc_box_size = 0;
  nsresult rv;
  rv = avcConfigBox->Generate(&avc_box_size);
  NS_ENSURE_SUCCESS(rv, rv);

  size += avc_box_size;

  *aBoxSize = size;

  return NS_OK;
}

nsresult
AVCSampleEntry::Write()
{
  BoxSizeChecker checker(mControl, size);
  nsresult rv;
  rv = VisualSampleEntry::Write();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = avcConfigBox->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

AVCSampleEntry::AVCSampleEntry(ISOControl* aControl)
  : VisualSampleEntry(NS_LITERAL_CSTRING("avc1"), aControl)
{
  avcConfigBox = new AVCConfigurationBox(aControl);
  MOZ_COUNT_CTOR(AVCSampleEntry);
}

AVCSampleEntry::~AVCSampleEntry()
{
  MOZ_COUNT_DTOR(AVCSampleEntry);
}

AVCConfigurationBox::AVCConfigurationBox(ISOControl* aControl)
  : Box(NS_LITERAL_CSTRING("avcC"), aControl)
{
  MOZ_COUNT_CTOR(AVCConfigurationBox);
}

AVCConfigurationBox::~AVCConfigurationBox()
{
  MOZ_COUNT_DTOR(AVCConfigurationBox);
}

nsresult
AVCConfigurationBox::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  FragmentBuffer* frag = mControl->GetFragment(Video_Track);
  rv = frag->GetCSD(avcConfig);
  NS_ENSURE_SUCCESS(rv, rv);
  size += avcConfig.Length();
  *aBoxSize = size;
  return NS_OK;
}

nsresult
AVCConfigurationBox::Write()
{
  BoxSizeChecker checker(mControl, size);
  Box::Write();

  mControl->Write(avcConfig.Elements(), avcConfig.Length());

  return NS_OK;
}

}
