/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GEOLOCATIONUTIL_H
#define GEOLOCATIONUTIL_H

#include "nsPrintfCString.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
/**
 * NI data encoding scheme
 */
typedef int GpsNiEncodingType;
#define GPS_ENC_NONE                   0
#define GPS_ENC_SUPL_GSM_DEFAULT       1
#define GPS_ENC_SUPL_UTF8              2
#define GPS_ENC_SUPL_UCS2              3
#define GPS_ENC_UNKNOWN                -1

double CalculateDeltaInMeter(double aLat, double aLon, double aLastLat, double aLastLon);
bool DecodeGsmDefaultToString(UniquePtr<char[]> &aByteArray, nsString &aWideStr);
nsString DecodeNIString(const char* aMessage, GpsNiEncodingType aEncodingType);
}

#endif
