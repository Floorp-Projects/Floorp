/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMUTF7ToUnicode_h___
#define nsMUTF7ToUnicode_h___

#include "nsUTF7ToUnicode.h"

//----------------------------------------------------------------------
// Class nsMUTF7ToUnicode [declaration]

/**
 * A character set converter from Modified UTF7 to Unicode.
 *
 * @created         18/May/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsMUTF7ToUnicode : public nsBasicUTF7Decoder 
{
public:

  /**
   * Class constructor.
   */
  nsMUTF7ToUnicode();

};

#endif /* nsMUTF7ToUnicode_h___ */
