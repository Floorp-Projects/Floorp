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
/* Defines a base class designed to validate objects across the C to C++ API */

#ifndef _MSG_OPAQ_H
#define _MSG_OPAQ_H

struct msg_OpaqueObject
{
public:
	msg_OpaqueObject (uint32 magic);
	virtual ~msg_OpaqueObject ();
	XP_Bool IsValid ();
protected:
	virtual uint32 GetExpectedMagic() = 0;

	XP_Bool m_deleted;
	uint32 m_magic;
};

inline msg_OpaqueObject::msg_OpaqueObject (uint32 magic)
{
	m_magic = magic;
	m_deleted = FALSE;
}
inline msg_OpaqueObject::~msg_OpaqueObject ()
{
	m_deleted = TRUE;
}
inline XP_Bool msg_OpaqueObject::IsValid ()
{
	// might die simply calling thru the v-table. definitely invalid!
	return (this && !m_deleted) && (m_magic == GetExpectedMagic());
}

#endif


