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

#ifndef nsISO88596EToUnicode_h___
#define nsISO88596EToUnicode_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsISO88596EToUnicode [declaration]

/**
 * A character set converter from ISO88596 to Unicode.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsISO88596EToUnicode : public nsISO88596ToUnicode
{
public:

  /**
   * Class constructor.
   */
  nsISO88596EToUnicode();

};

#endif /* nsISO88596EToUnicode_h___ */
