/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_IXTFSERVICE_H__
#define __NS_IXTFSERVICE_H__

#include "nsISupports.h"

class nsIContent;
class nsINodeInfo;

// {4ac3826f-280e-4572-9ede-6c81a4797861}
#define NS_IXTFSERVICE_IID                             \
  { 0x4ac3826f, 0x280e, 0x4572, \
    { 0x9e, 0xde, 0x6c, 0x81, 0xa4, 0x79, 0x78, 0x61 } }
class nsIXTFService : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXTFSERVICE_IID)

    // try to create an xtf element based on namespace
    virtual nsresult CreateElement(nsIContent** aResult,
                                   already_AddRefed<nsINodeInfo> aNodeInfo) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXTFService, NS_IXTFSERVICE_IID)

//----------------------------------------------------------------------
// The one class implementing this interface:

// {4EC832DA-6AE7-4185-807B-DADDCB5DA37A}
#define NS_XTFSERVICE_CID                             \
  { 0x4ec832da, 0x6ae7, 0x4185, { 0x80, 0x7b, 0xda, 0xdd, 0xcb, 0x5d, 0xa3, 0x7a } }

#define NS_XTFSERVICE_CONTRACTID "@mozilla.org/xtf/xtf-service;1"

nsresult NS_NewXTFService(nsIXTFService** aResult);

#endif // __NS_IXTFSERVICE_H__

