/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* mimeiimg.h --- definition of the MimeInlineImage class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
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
