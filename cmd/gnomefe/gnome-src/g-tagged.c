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
  g-tagged.c -- tagged c objects with subclassing.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#include "g-tagged.h"

void
moz_tagged_init(MozTagged *tagged)
{
  /* nothing to do here... */
}

void
moz_tagged_deinit(MozTagged *tagged)
{
  /* nothing to do here... */
}

void
moz_tagged_set_type(MozTagged *tagged,
		    PRUint32 tag)
{
  tagged->tag |= tag;
}

PRUint32
moz_tagged_get_type(MozTagged *tagged)
{
  return tagged->tag;
}
