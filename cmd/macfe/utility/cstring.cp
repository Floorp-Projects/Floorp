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

#include "cstring.h"

#include "xpassert.h"

void
cstring::operator+= (const char *s)
{
	if (s != NULL && s[0] != '\0')
	{
		int newLength = length() + strlen(s);
		reserve (newLength+1, true);
		strcat (data(), s);
	}
}

void
cstring::truncAt (char c)
{
	char *cc = strchr (data(), c);
	if (cc) {
		*cc = 0;
		reserve (length()+1, true);
	}
}

char*
cstring::reserve (int len, Boolean preserve)
{
	char *dest = space.internal;
	if (haveExternalSpace)
		if (len > cstringMaxInternalLen) // can reuse space
			dest = space.external = (char*) realloc (space.external, len);
		else { // don't need external space
			char *oldExternal = space.external;
			if (preserve) { // can't guarantee preservation since it's shrinking
				memcpy (oldExternal, space.internal, cstringMaxInternalLen);
				dest[cstringMaxInternalLen] = 0;
			}
			free (oldExternal);
			haveExternalSpace = false;
		}
	else
		if (len > cstringMaxInternalLen) { // need new space
			dest= (char*) malloc (len);
			if (preserve)
				memcpy (dest, space.internal, cstringMaxInternalLen);
			space.external = dest;
			haveExternalSpace = true;
		}
	return dest; 
}

void
cstring::assign (const void *sd, int len)
{
	XP_ASSERT(sd != NULL || len == 0);
	char *dest = reserve (len+1, false);

	if (sd != NULL)
		memcpy (dest, sd, len);
		
	dest[len] = 0;
}


