/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIUnicodeEncoder_h___
#define nsIUnicodeEncoder_h___

#include "nsISupports.h"

// Interface ID for our Unicode Encoder interface
// {2B2CA3D0-A4C9-11d2-8AA1-00600811A836}
NS_DECLARE_ID(kIUnicodeEncoderIID,
  0x2b2ca3d0, 0xa4c9, 0x11d2, 0x8a, 0xa1, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

/**
 * Interface for a Converter from Unicode into a Charset.
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicodeEncoder : public nsISupports
{
public:
};

#endif /* nsIUnicodeEncoder_h___ */
