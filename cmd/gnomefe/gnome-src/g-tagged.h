/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
  g-tagged.h -- tagged c objects with subclassing.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_tagged_h
#define _moz_tagged_h

#include "g-types.h"
#include "prtypes.h"

struct _MozTagged {
  PRUint32 tag;
};

extern void		moz_tagged_init(MozTagged *tagged);
extern void		moz_tagged_deinit(MozTagged *tagged);

extern void		moz_tagged_set_type(MozTagged *tagged, PRUint32 tag);
extern PRUint32		moz_tagged_get_type(MozTagged *tagged);

#endif
