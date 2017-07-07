/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utilities for regression tests based on frame tree comparison */

#ifndef nsIFrameUtil_h___
#define nsIFrameUtil_h___

#include <stdio.h>
#include "nsISupports.h"

/* a6cf90d4-15b3-11d2-932e-00805f8add32 */
#define NS_IFRAME_UTIL_IID \
 { 0xa6cf90d6, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * Frame utility interface
 */
class nsIFrameUtil : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFRAME_UTIL_IID)
  /**
   * Compare two regression data dumps. The return status will be NS_OK
   * if the trees compare favoribly, otherwise the return will indicate
   * NS_ERROR_FAILURE. Other return status's will indicate some other
   * type of failure. The files, aFile1 and aFile2 are closed before
   * returning.
   * aRegressionOutput will vary output, 0 is full output, 1 is brief
   */
  NS_IMETHOD CompareRegressionData(FILE* aFile1, FILE* aFile2,int32_t aRegressionOutput) = 0;

  /**
   * Display the regression dump data stored in aInputFile1 to
   * aOutputFile . The file is closed before returning. If the
   * regression data is in error somehow then NS_ERROR_FAILURE will be
   * returned.
   */
  NS_IMETHOD DumpRegressionData(FILE* aInputFile, FILE* aOutputFile) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrameUtil, NS_IFRAME_UTIL_IID)

#endif /* nsIFrameUtil_h___ */
