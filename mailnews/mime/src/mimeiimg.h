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

#ifndef _MIMEIIMG_H_
#define _MIMEIIMG_H_

#include "mimeleaf.h"

/* The MimeInlineImage class implements those MIME image types which can be
   displayed inline (currently image/gif, image/jpeg, and image/xbm.)
 */

typedef struct MimeInlineImageClass MimeInlineImageClass;
typedef struct MimeInlineImage      MimeInlineImage;

struct MimeInlineImageClass {
  MimeLeafClass leaf;
};

extern MimeInlineImageClass mimeInlineImageClass;

struct MimeInlineImage {
  MimeLeaf leaf;

  /* Opaque data object for the backend-specific inline-image-display code
	 (internal-external-reconnect nastiness.) */
  void *image_data;
};

#endif /* _MIMEIIMG_H_ */
