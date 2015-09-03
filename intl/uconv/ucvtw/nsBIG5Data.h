/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBIG5Data_h_
#define nsBIG5Data_h_

class nsBIG5Data
{
public:
  static char16_t LowBits(size_t aPointer);
  static bool     IsAstral(size_t aPointer);
  static size_t   FindPointer(char16_t aLowBits, bool aIsAstral);
};

#endif /* nsBIG5Data_h_ */

