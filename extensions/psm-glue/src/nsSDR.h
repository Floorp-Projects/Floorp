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
 */

#ifndef _NSSDR_H_
#define _NSSDR_H_

#include "nsISecretDecoderRing.h"

// ===============================================
// nsSecretDecoderRing - implementation of nsISecretDecoderRing
// ===============================================

#define NS_SDR_CLASSNAME "Secret Decoder Ring"
#define NS_SDR_CID \
  { 0xd9a0341, 0xce7, 0x11d4, { 0x9f, 0xdd, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }
#define NS_SDR_CONTRACTID "@mozilla.org/security/sdr;1"

class nsSecretDecoderRing : public nsISecretDecoderRing
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECRETDECODERRING

  nsSecretDecoderRing();
  virtual ~nsSecretDecoderRing();

  nsresult init();

private:
  nsIPSMComponent *mPSM;

  static const char *kPSMComponentContractID;

  nsresult encode(const unsigned char *data, PRInt32 dataLen, char **_retval);
  nsresult decode(const char *data, unsigned char **result, PRInt32 * _retval);
};

#endif /* _NSSDR_H_ */
