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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDefaultSOAPEncoder_h__
#define nsDefaultSOAPEncoder_h__

#include "nsISOAPEncoder.h"

class nsDefaultSOAPEncoder : public nsISOAPEncoder {
public:
  nsDefaultSOAPEncoder();
  virtual ~nsDefaultSOAPEncoder();

  NS_DECL_ISUPPORTS

  // nsISOAPEncoder  
  NS_DECL_NSISOAPENCODER
};

#define NS_DEFAULTSOAPENCODER_CID               \
{ /* 0b6e6ef0-56c4-11d4-9a5e-00104bdf5339 */    \
  0x0b6e6ef0, 0x56c4, 0x11d4,                   \
  {0x9a, 0x5e, 0x00, 0x10, 0x4b, 0xdf, 0x53, 0x39} }
#define NS_DEFAULTSOAPENCODER_PROGID NS_SOAPENCODER_PROGID_PREFIX "http://schemas.xmlsoap.org/soap/encoding/"

#endif
