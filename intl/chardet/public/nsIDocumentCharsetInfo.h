/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIDocumentCharsetInfo_h__
#define nsIDocumentCharsetInfo_h__

#include "nsISupports.h"
#include "nsIAtom.h"

// {2D40B291-01E1-11d4-9D0E-0050040007B2}
#define NS_IDOCUMENTCHARSETINFO_IID \
  {0x2d40b291, 0x1e1, 0x11d4, {0x9d, 0xe, 0x0, 0x50, 0x4, 0x0, 0x7, 0xb2}}

// {D25E0511-2BAE-11d4-9D10-0050040007B2}
#define NS_DOCUMENTCHARSETINFO_CID \
  {0xd25e0511, 0x2bae, 0x11d4, {0x9d, 0x10, 0x0, 0x50, 0x4, 0x0, 0x7, 0xb2}}

#define NS_DOCUMENTCHARSETINFO_PID \
  "@mozilla.org/document-charset-info;1"

// XXX doc me
// XXX make this interface IDL
// XXX mark the right params "const"

class nsIDocumentCharsetInfo : public nsISupports 
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENTCHARSETINFO_IID)

  NS_IMETHOD SetForcedCharset(nsIAtom * aCharset) = 0;
  NS_IMETHOD GetForcedCharset(nsIAtom ** aResult) = 0;

  NS_IMETHOD SetForcedDetector(PRBool aForced) = 0;
  NS_IMETHOD GetForcedDetector(PRBool * aResult) = 0;

  NS_IMETHOD SetParentCharset(nsIAtom * aCharset) = 0;
  NS_IMETHOD GetParentCharset(nsIAtom ** aResult) = 0;

  /**
   * You should NOT use this method!!! It will very soon be deprecated. I only 
   * added it here for convenience in the ongoing transition to Atoms. Use
   * SetParentCharset(nsIAtom *) instead.
   */
  NS_IMETHOD SetParentCharset(nsString * aCharset) = 0;
};

#endif // nsIDocumentCharsetInfo_h__
