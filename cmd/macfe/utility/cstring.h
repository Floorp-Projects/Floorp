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

#pragma once

#include "fredmem.h"

#define cstringMaxInternalLen 30

class cstring 
{
public:
	inline			cstring ()
					{ haveExternalSpace = false; space.internal[0] = 0; }
	inline			cstring (const char *s)
					{ haveExternalSpace = false; assign (s); }
	inline			cstring (const unsigned char *ps)
					{ haveExternalSpace = false; assign (ps+1, ps[0]); }
	inline			cstring (const cstring& s)
					{ haveExternalSpace = false; assign (s.data(), s.length() ); }
	inline			cstring( void* ptr, long len )
					{ haveExternalSpace = false; assign( ptr, len ); }
	inline 			~cstring () { reserve (0, false); }
	char&			operator[] (int i) { return data()[i]; }
	inline void		operator= (const char *s);
	inline void		operator= (const unsigned char *ps);
	inline void		operator= (const cstring& s);
	void			operator+= (const char *s);
	void			truncAt (char c);
	// comparison
	inline int		operator== (const cstring& s);
	inline int		operator== (const char *s);
	inline int		operator!= (const cstring& s);
	inline int		operator!= (const char *s);
	// query
	inline const char* data() const { return haveExternalSpace? space.external: space.internal; }
	inline operator const char* () const { return data(); }
/*	This is not a bug because
 inline operator char* () { return data(); }
cannot convert a "const cstring" to "char *"
This can be avoided by adding:
 inline operator const char* () const { return data(); }
*/
	inline char*	data() { return haveExternalSpace? space.external: space.internal; }
	inline 			operator char* () { return data(); }
	inline int		length() const { return strlen (data()); }
	char*			reserve (int len, Boolean preserve = false);
private:
	void 			assign (const char *s) { assign (s, s? strlen (s): 0); }
	void 			assign (const void *sd, int len);
	union {
		char*			external;
		char			internal [cstringMaxInternalLen+1];
	}				space;
	int				haveExternalSpace;
};

inline void		
cstring::operator= (const char *s) { 
	assign (s); 
}

inline void		
cstring::operator= (const unsigned char *ps) { 
	assign (ps+1, ps[0]);
}

inline void		
cstring::operator= (const cstring& s) { 
	assign (s.data(), s.length() );
}

inline int
cstring::operator== (const cstring& s) {
	return !strcmp (data(), s.data());
}

inline int
cstring::operator== (const char *s) {
	return !strcmp (data(), s);
}

inline int
cstring::operator!= (const cstring& s) {
	return strcmp (data(), s.data());
}

inline int
cstring::operator!= (const char *s) {
	return strcmp (data(), s);
}


