/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 *   Steve Morse <morse@netscape.com>
 */

#ifndef _NSFSDR_H_
#define _NSFSDR_H_

#include "nsISecretDecoderRing.h"

// ===============================================
// nsFSecretDecoderRing - "fake" implementation of nsISecretDecoderRing
// ===============================================

#define NS_FSDR_CLASSNAME "Fake Secret Decoder Ring"
#define NS_FSDR_CID \
  { 0x1ee28720, 0x2b93, 0x11d4, { 0xa0, 0xa4, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

#define NS_FSDR_CONTRACTID "@mozilla.org/security/fsdr;1"

class nsFSecretDecoderRing : public nsISecretDecoderRing
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECRETDECODERRING

  nsFSecretDecoderRing();
  virtual ~nsFSecretDecoderRing();

  nsresult init();

private:
  nsIPSMComponent *mPSM;

  static const char *kPSMComponentContractID;

  nsresult encode(const unsigned char *data, PRInt32 dataLen, char **_retval);
  nsresult decode(const char *data, unsigned char **result, PRInt32 * _retval);
};

#endif /* _NSFSDR_H_ */
