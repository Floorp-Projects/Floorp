/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGB2312ToUnicodeV2_h___
#define nsGB2312ToUnicodeV2_h___

#include "nsUCSupport.h"
#include "gbku.h"
#include "nsGBKToUnicode.h"
//----------------------------------------------------------------------
// Class nsGB2312ToUnicodeV2 [declaration]

/**
 * A character set converter from GB2312 to Unicode.
 *
 * @created         06/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsGB2312ToUnicodeV2 : public nsGB18030ToUnicode
{
public:
		  
  /**
   * Class constructor.
   */
  nsGB2312ToUnicodeV2(){}
};
  

#endif /* nsGB2312ToUnicodeV2_h___ */
