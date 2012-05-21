/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICDETObserver_h__
#define nsICDETObserver_h__

#include "nsISupports.h"
#include "nsDetectionConfident.h"

// {12BB8F12-2389-11d3-B3BF-00805F8A6670}
#define NS_ICHARSETDETECTIONOBSERVER_IID \
{ 0x12bb8f12, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

/*
  Used to inform answer by nsICharsetDetector
 */
class nsICharsetDetectionObserver : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICHARSETDETECTIONOBSERVER_IID)
  NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICharsetDetectionObserver,
                              NS_ICHARSETDETECTIONOBSERVER_IID)

#endif /* nsICDETObserver_h__ */
