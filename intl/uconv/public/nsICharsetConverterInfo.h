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

#ifndef nsICharsetConverterInfo_h___
#define nsICharsetConverterInfo_h___

#include "nsString.h"
#include "nsISupports.h"

// Interface ID for our Converter Information interface
// {6A7730E0-8ED3-11d2-8A98-00600811A836}
NS_DECLARE_ID(kICharsetConverterInfoIID,
  0x6a7730e0, 0x8ed3, 0x11d2, 0x8a, 0x98, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

/**
 * Interface for getting the Charset Converter information.
 *
 * This info interface is a little more general, covering any conveter, not
 * only Unicode encoders/decoders. Now an interesting question is who is
 * supposed to implement it? For now: the Factory of the converter. That is
 * because of the way the converter info is gathered: a factory is a much 
 * smaller and easier to instantiate than a whole Converter object. However, 
 * this may change in future, as new moniker mechanisms are added to xp-com.
 *
 * @created         08/Dec/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsICharsetConverterInfo : public nsISupports
{
public:

  /**
   * Returns the character set this converter is converting from.
   *
   * @param aCharset    [OUT] a name/alias for the source charset
   */
  NS_IMETHOD GetCharsetSrc(nsString ** aCharset) = 0;

  /**
   * Returns the character set this converter is converting into.
   *
   * @param aCharset    [OUT] a name/alias for the destination charset
   */
  NS_IMETHOD GetCharsetDest(nsString ** aCharset) = 0;
};

#endif /* nsICharsetConverterInfo_h___ */
