/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptGlobalObjectOwner_h__
#define nsIScriptGlobalObjectOwner_h__

#include "nsISupports.h"

class nsIScriptGlobalObject;

#define NS_ISCRIPTGLOBALOBJECTOWNER_IID \
  {0xfd25ca8e, 0x6b63, 0x435f, \
    { 0xb8, 0xc6, 0xb8, 0x07, 0x68, 0xa4, 0x0a, 0xdc }}

/**
 * Implemented by any object capable of supplying a nsIScriptGlobalObject.
 * The implentor may create the script global object on demand.
 */

class nsIScriptGlobalObjectOwner : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTGLOBALOBJECTOWNER_IID)

  /**
   * Returns the script global object
   */
  virtual nsIScriptGlobalObject* GetScriptGlobalObject() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptGlobalObjectOwner,
                              NS_ISCRIPTGLOBALOBJECTOWNER_IID)

#endif /* nsIScriptGlobalObjectOwner_h__ */
