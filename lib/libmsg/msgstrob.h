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

#ifndef MSGSTROB_H
#define MSGSTROB_H

class XPStringObj
{
public:
						~XPStringObj();
						XPStringObj(const char *str = 0);

	XPStringObj & 		operator =(const XPStringObj &xpStringObj);	
	XPStringObj & 		operator =(const char *str);	

						operator const char*() const;	

	XPStringObj & 		operator +=(const char chr);	
	XPStringObj & 		operator +=(const char *str);	
	XPStringObj & 		operator +=(XPStringObj &xpStringObj);	

	char  				operator [](int charIdx);	
	void				SetStrPtr(char *str);
protected:
	friend char * 		operator +(const XPStringObj &xpStringObj, const char *str);
	friend char * 		operator +(const XPStringObj &xpStringObj1, const XPStringObj &xpStringObj2);
	friend char * 		operator +(const char *str,const XPStringObj &xpStringObj);

	char		*m_strPtr;
};

inline XPStringObj::~XPStringObj()
{
	if (m_strPtr)
		XP_FREE(m_strPtr);
}


inline XPStringObj::XPStringObj(const char *str)
{
	m_strPtr = (str) ? XP_STRDUP(str) : 0;
}

inline XPStringObj &XPStringObj::operator =(const XPStringObj &xpStringObj)
{

	XP_FREEIF(m_strPtr);
	m_strPtr = (xpStringObj.m_strPtr) ? XP_STRDUP(xpStringObj.m_strPtr) : 0;

	return *this;
}

inline XPStringObj &XPStringObj::operator =(const char *str)	
{
	XP_FREEIF(m_strPtr);
	m_strPtr = XP_STRDUP(str);
	return *this;
}

inline XPStringObj::operator const char *(void) const
{
	return m_strPtr;
}

inline XPStringObj &XPStringObj::operator +=(const char chr)
{
	char			str[2];

	*str = chr;
	str[1] = '\0';

	m_strPtr = XP_AppendStr(m_strPtr, str);
	return *this;
}

inline XPStringObj &XPStringObj::operator +=(const char *str)
{
	m_strPtr = XP_AppendStr(m_strPtr, str);
	return *this;
}

/*
 * This operator is envoked by compiler generated code when
 * concatenating another string on the end of this string.
 */
inline XPStringObj &XPStringObj::operator +=(XPStringObj &str)
{
	m_strPtr = XP_AppendStr(m_strPtr, str.m_strPtr);
	return *this;
}

inline char XPStringObj::operator [](int charIdx)
{
	XP_ASSERT(m_strPtr && charIdx < (int) XP_STRLEN(m_strPtr));
	return (m_strPtr) ? m_strPtr[charIdx] : 0;
}

inline void XPStringObj::SetStrPtr(char *str)
{
	XP_FREEIF(m_strPtr);
	m_strPtr = str;
}

#endif
