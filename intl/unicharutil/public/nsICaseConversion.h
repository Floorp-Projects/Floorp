/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsICaseConversion_h__
#define nsICaseConversion_h__


#include "nsISupports.h"
#include "nscore.h"

// {07D3D8E0-9614-11d2-B3AD-00805F8A6670}
#define NS_ICASECONVERSION_IID \
{ 0x7d3d8e0, 0x9614, 0x11d2, \
    { 0xb3, 0xad, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

class nsICaseConversion : public nsISupports {

public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICASECONVERSION_IID)

  // Convert one Unicode character into upper case
  NS_IMETHOD ToUpper( char16_t aChar, char16_t* aReturn) = 0;

  // Convert one Unicode character into lower case
  NS_IMETHOD ToLower( char16_t aChar, char16_t* aReturn) = 0;

  // Convert one Unicode character into title case
  NS_IMETHOD ToTitle( char16_t aChar, char16_t* aReturn) = 0;

  // Convert an array of Unicode characters into upper case
  NS_IMETHOD ToUpper( const char16_t* anArray, char16_t* aReturn, uint32_t aLen) = 0;

  // Convert an array of Unicode characters into lower case
  NS_IMETHOD ToLower( const char16_t* anArray, char16_t* aReturn, uint32_t aLen) = 0;

  // case-insensitive char16_t* comparison - aResult returns similar
  // to strcasecmp
  NS_IMETHOD CaseInsensitiveCompare(const char16_t* aLeft, const char16_t* aRight, uint32_t aLength, int32_t* aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICaseConversion, NS_ICASECONVERSION_IID)

#endif  /* nsICaseConversion_h__ */
