/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIKeyedStreamGenerator.h"
#include "nsIPasswordSink.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"

#define NS_SECURITY_LEVEL 1.0

// {87496b68-1dd2-11b2-b080-ccabe9894783}
#define NS_BASIC_STREAM_GENERATOR_CID \
{ 0x87496b68, 0x1dd2, 0x11b2, { 0xb0, 0x80, 0xcc, 0xab, 0xe9, 0x89, 0x47, 0x83 } }

#define NS_BASIC_STREAM_GENERATOR_CONTRACTID "@mozilla.org/keyed-stream-generator/basic;1"
#define NS_BASIC_STREAM_GENERATOR_CLASSNAME "Basic Keyed Stream Generator"

class nsBasicStreamGenerator : public nsIKeyedStreamGenerator
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYEDSTREAMGENERATOR

  nsBasicStreamGenerator();

 protected:
  virtual ~nsBasicStreamGenerator();

 private:
  static const char *mSignature; /* read only */
  float mLevel; /* read only */
  PRUint32 mSalt; /* not used for now */
  nsString mPassword;
  nsCOMPtr<nsIWeakReference> mWeakPasswordSink; /* nsIPasswordSink */
  PRInt32 mState;
};

