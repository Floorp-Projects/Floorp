/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsGB2312ToUnicodeV2_h___
#define nsGB2312ToUnicodeV2_h___

#include "nsUCvCnSupport.h"
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
  nsGB2312ToUnicodeV2(){};
};
  

#endif /* nsGB2312ToUnicodeV2_h___ */
