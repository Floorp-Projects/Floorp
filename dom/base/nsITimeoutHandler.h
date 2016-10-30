/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsITimeoutHandler_h___
#define nsITimeoutHandler_h___

#include "nsISupports.h"

#define NS_ITIMEOUTHANDLER_IID \
{ 0xb071a1d3, 0xfd54, 0x40a8,  \
 { 0x91, 0x9f, 0xc8, 0xf3, 0x3e, 0xb8, 0x3c, 0xfe } }

class nsITimeoutHandler : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITIMEOUTHANDLER_IID)

  virtual nsresult Call() = 0;

  virtual void GetLocation(const char** aFileName, uint32_t* aLineNo,
                           uint32_t* aColumn) = 0;

  virtual void MarkForCC() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITimeoutHandler,
                              NS_ITIMEOUTHANDLER_IID)

#endif // nsITimeoutHandler_h___
