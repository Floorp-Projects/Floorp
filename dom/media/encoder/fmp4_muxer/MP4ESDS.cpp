/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include "ISOControl.h"
#include "ISOMediaBoxes.h"
#include "MP4ESDS.h"

namespace mozilla {

nsresult
MP4AudioSampleEntry::Generate(uint32_t* aBoxSize)
{
  uint32_t box_size;
  nsresult rv = es->Generate(&box_size);
  NS_ENSURE_SUCCESS(rv, rv);
  size += box_size;

  *aBoxSize = size;
  return NS_OK;
}

nsresult
MP4AudioSampleEntry::Write()
{
  BoxSizeChecker checker(mControl, size);
  nsresult rv;
  rv = AudioSampleEntry::Write();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = es->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

MP4AudioSampleEntry::MP4AudioSampleEntry(ISOControl* aControl)
  : AudioSampleEntry(NS_LITERAL_CSTRING("mp4a"), aControl)
{
  es = new ESDBox(aControl);
  MOZ_COUNT_CTOR(MP4AudioSampleEntry);
}

MP4AudioSampleEntry::~MP4AudioSampleEntry()
{
  MOZ_COUNT_DTOR(MP4AudioSampleEntry);
}

nsresult
ESDBox::Generate(uint32_t* aBoxSize)
{
  uint32_t box_size;
  es_descriptor->Generate(&box_size);
  size += box_size;
  *aBoxSize = size;
  return NS_OK;
}

nsresult
ESDBox::Write()
{
  WRITE_FULLBOX(mControl, size)
  es_descriptor->Write();
  return NS_OK;
}

ESDBox::ESDBox(ISOControl* aControl)
  : FullBox(NS_LITERAL_CSTRING("esds"), 0, 0, aControl)
{
  es_descriptor = new ES_Descriptor(aControl);
  MOZ_COUNT_CTOR(ESDBox);
}

ESDBox::~ESDBox()
{
  MOZ_COUNT_DTOR(ESDBox);
}

nsresult
ES_Descriptor::Find(const nsACString& aType,
                    nsTArray<nsRefPtr<MuxerOperation>>& aOperations)
{
  // ES_Descriptor is not a real ISOMediaBox, so we return nothing here.
  return NS_OK;
}

nsresult
ES_Descriptor::Write()
{
  mControl->Write(tag);
  mControl->Write(length);
  mControl->Write(ES_ID);
  mControl->WriteBits(streamDependenceFlag.to_ulong(), streamDependenceFlag.size());
  mControl->WriteBits(URL_Flag.to_ulong(), URL_Flag.size());
  mControl->WriteBits(reserved.to_ulong(), reserved.size());
  mControl->WriteBits(streamPriority.to_ulong(), streamPriority.size());
  mControl->Write(DecodeSpecificInfo.Elements(), DecodeSpecificInfo.Length());

  return NS_OK;
}

nsresult
ES_Descriptor::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  //   14496-1 '8.3.4 DecoderConfigDescriptor'
  //   14496-1 '10.2.3 SL Packet Header Configuration'
  FragmentBuffer* frag = mControl->GetFragment(Audio_Track);
  rv = frag->GetCSD(DecodeSpecificInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  length = sizeof(ES_ID) + 1;
  length += DecodeSpecificInfo.Length();

  *aBoxSize = sizeof(tag) + sizeof(length) + length;
  return NS_OK;
}

ES_Descriptor::ES_Descriptor(ISOControl* aControl)
  : tag(ESDescrTag)
  , length(0)
  , ES_ID(0)
  , streamDependenceFlag(0)
  , URL_Flag(0)
  , reserved(0)
  , streamPriority(0)
  , mControl(aControl)
{
  MOZ_COUNT_CTOR(ES_Descriptor);
}

ES_Descriptor::~ES_Descriptor()
{
  MOZ_COUNT_DTOR(ES_Descriptor);
}

}
