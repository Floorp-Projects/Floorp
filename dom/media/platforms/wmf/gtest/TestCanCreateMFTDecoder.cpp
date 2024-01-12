/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>

#include "WMFDecoderModule.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

TEST(CanCreateMFTDecoder, NoIPC)
{ EXPECT_TRUE(WMFDecoderModule::CanCreateMFTDecoder(WMFStreamType::H264)); }
