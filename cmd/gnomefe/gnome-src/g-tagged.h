/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
