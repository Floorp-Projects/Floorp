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
 * IBM Corporation
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 1999
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 12/09/1999   IBM Corp.       Support for IBM codepage 852
 *
 */

#ifndef nsCP852ToUnicode_h___
#define nsCP852ToUnicode_h___

#include "nsUCvIBMSupport.h"

//----------------------------------------------------------------------
// Class nsCP852ToUnicode [declaration]

/**
 * A character set converter from CP852 to Unicode.
 */
class nsCP852ToUnicode : public nsOneByteDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsCP852ToUnicode();
};

#endif /* nsCP852ToUnicode_h___ */
