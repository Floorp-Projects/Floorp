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

#ifndef nsISO88592ToUnicode_h___
#define nsISO88592ToUnicode_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsISO88592ToUnicode [declaration]

/**
 * A character set converter from ISO88592 to Unicode.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsISO88592ToUnicode : public nsOneByteDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsISO88592ToUnicode();

};

#endif /* nsISO88592ToUnicode_h___ */
