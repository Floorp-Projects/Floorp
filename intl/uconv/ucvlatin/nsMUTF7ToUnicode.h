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
