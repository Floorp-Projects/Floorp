/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#ifndef __UMacUnicode__
#define __UMacUnicode__

#include "nsString.h"
#include "nsCOMPtr.h"

#include <UnicodeConverter.h>

class CPlatformUCSConversion {
public:
     CPlatformUCSConversion();
     virtual ~CPlatformUCSConversion(){};
     
     static CPlatformUCSConversion* GetInstance();
          
     NS_IMETHOD UCSToPlatform(const nsAString& aIn, nsACString& aOut);
     NS_IMETHOD UCSToPlatform(const nsAString& aIn, Str255& aOut);  
  
     NS_IMETHOD PlatformToUCS(const nsACString& ain, nsAString& aOut);  
     NS_IMETHOD PlatformToUCS(const Str255& aIn, nsAString& aOut);  

private:
     static CPlatformUCSConversion *mgInstance;
     static UnicodeToTextInfo sEncoderInfo;
     static TextToUnicodeInfo sDecoderInfo;
     
     nsresult PrepareEncoder();
     nsresult PrepareDecoder();
};

#endif // __UMacUnicode__
