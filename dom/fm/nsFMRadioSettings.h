/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fm_radio_settings_h__
#define mozilla_dom_fm_radio_settings_h__

#include "nsIFMRadio.h"

class nsFMRadioSettings : public nsIFMRadioSettings
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFMRADIOSETTINGS

  nsFMRadioSettings(int32_t aUpperLimit, int32_t aLowerLimit, int32_t aChannelWidth);
private:
  ~nsFMRadioSettings();
  int32_t mUpperLimit;
  int32_t mLowerLimit;
  int32_t mChannelWidth;
};
#endif

