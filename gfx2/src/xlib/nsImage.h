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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsIImage.h"

#define NS_IMAGE_CID \
  {0x73c72e6c, 0x1dd2, 0x11b2, \
    { 0x98, 0xb7, 0xae, 0x59, 0x35, 0xee, 0x63, 0xf5 }}

class nsImage : public nsIImage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMAGE

  nsImage();
  virtual ~nsImage();
  /* additional members */
};

