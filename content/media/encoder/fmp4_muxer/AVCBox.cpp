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
VisualSampleEntry::Generate(uint32_t* aBoxSize)
{
  // both fields occupy 16 bits defined in 14496-2 6.2.3.
  width = mMeta.mVidMeta->Width;
  height = mMeta.mVidMeta->Height;

  uint32_t avc_box_size = 0;
  nsresult rv;
  rv = avcConfigBox->Generate(&avc_box_size);
  NS_ENSURE_SUCCESS(rv, rv);

  size += avc_box_size +
          sizeof(reserved) +
          sizeof(width) +
          sizeof(height) +
          sizeof(horizresolution) +
          sizeof(vertresolution) +
          sizeof(reserved2) +
          sizeof(frame_count) +
          sizeof(compressorName) +
          sizeof(depth) +
          sizeof(pre_defined);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
VisualSampleEntry::Write()
{
  BoxSizeChecker checker(mControl, size);
  SampleEntryBox::Write();

  mControl->Write(reserved, sizeof(reserved));
  mControl->Write(width);
  mControl->Write(height);
  mControl->Write(horizresolution);
  mControl->Write(vertresolution);
  mControl->Write(reserved2);
  mControl->Write(frame_count);
  mControl->Write(compressorName, sizeof(compressorName));
  mControl->Write(depth);
  mControl->Write(pre_defined);
  nsresult rv = avcConfigBox->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

VisualSampleEntry::VisualSampleEntry(ISOControl* aControl)
  : SampleEntryBox(NS_LITERAL_CSTRING("avc1"), Video_Track, aControl)
  , width(0)
  , height(0)
  , horizresolution(resolution_72_dpi)
  , vertresolution(resolution_72_dpi)
  , reserved2(0)
  , frame_count(1)
  , depth(video_depth)
  , pre_defined(-1)
{
  memset(reserved, 0 , sizeof(reserved));
  memset(compressorName, 0 , sizeof(compressorName));
  avcConfigBox = new AVCConfigurationBox(aControl);
  MOZ_COUNT_CTOR(VisualSampleEntry);
}

VisualSampleEntry::~VisualSampleEntry()
{
  MOZ_COUNT_DTOR(VisualSampleEntry);
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
