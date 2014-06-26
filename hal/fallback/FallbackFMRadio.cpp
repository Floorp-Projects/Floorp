/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

namespace mozilla {
namespace hal_impl {

void
EnableFMRadio(const hal::FMRadioSettings& aInfo)
{}

void
DisableFMRadio()
{}

void
FMRadioSeek(const hal::FMRadioSeekDirection& aDirection)
{}

void
GetFMRadioSettings(hal::FMRadioSettings* aInfo)
{
  aInfo->country() = hal::FM_RADIO_COUNTRY_UNKNOWN;
  aInfo->upperLimit() = 0;
  aInfo->lowerLimit() = 0;
  aInfo->spaceType() = 0;
  aInfo->preEmphasis() = 0;
}

void
SetFMRadioFrequency(const uint32_t frequency)
{}

uint32_t
GetFMRadioFrequency()
{
  return 0;
}

bool
IsFMRadioOn()
{
  return false;
}

uint32_t
GetFMRadioSignalStrength()
{
  return 0;
}

void
CancelFMRadioSeek()
{}

void
EnableRDS(uint32_t aMask)
{}

void
DisableRDS()
{}

} // hal_impl
} // namespace mozilla
