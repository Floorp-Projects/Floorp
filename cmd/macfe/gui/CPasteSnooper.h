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

// ===========================================================================
//	CPasteSnooper.h
//	an attachment for filtering characters from a paste command
// ===========================================================================

#pragma once

#include <LAttachment.h>

class CStr31;

#define ONLY_TAKE_FIRST_LINE	"\r\n", "\f\f"
#define MAKE_RETURNS_SPACES		"\r\n", "  "
#define DELETE_RETURNS			"\r\n", "\b\b"

class CPasteSnooper : public LAttachment {
public:

					// inFind is a pascal string of characters that you
					// don't want to allow the user to paste. inReplace is
					// a pascal string of characters to replace the inFind
					// characters with. ie every instance of inFind[1] is
					// replaced with inReplace[1]. to specify that a certain
					// character is to be simply deleted, put the character in
					// inFind at any location i and set inReplace[i] to
					// '\b'. The '\b' code is reserved for deleting characters
					// A '\f' in inReplace[i] means that we only take characters up
					// to the first occurence of inFind[i] and discard everything
					// after that. \a in inReplace[i] means that leading occurences
					// of inFind[i] should be stripped
					CPasteSnooper (char *inFind, char *inReplace);
			
	virtual			~CPasteSnooper ();

protected:
	// characters that we're not allowing the user to paste
	char			*mFind;
	// characters used to replace those found in mFilter. '\b' if
	// we're supposed to delete a char rather than replace it
	char			*mReplace;
	
protected:	
					// this creates a CPasteActionSnooper to really do the paste
	virtual	void	ExecuteSelf (MessageT inMessage, void *ioParam);
};
