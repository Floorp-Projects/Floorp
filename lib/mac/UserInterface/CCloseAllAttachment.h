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

// Written by Mark G. Young.

#ifndef	CCloseAllAttachment_H
#define	CCloseAllAttachment_H
#pragma once

#include <LAttachment.h>

class SCommandStatus;

class CCloseAllAttachment : public LAttachment
{
public:
	
	enum { class_ID = 'Clsa' };

	typedef LAttachment super;

						CCloseAllAttachment(
											ResIDT			inCloseString,
											ResIDT 			inCloseAllString);

						CCloseAllAttachment(
											LStream*		inStream);

	virtual 			~CCloseAllAttachment();
						
	virtual	void		ExecuteSelf(
											MessageT		inMessage,
											void*			ioParam);

protected:	
	virtual	void		FindCommandStatus(
											SCommandStatus*	ioCommandStatus);
							
	virtual	void		ObeyCommand();

	ResIDT				mCloseStringID;
	ResIDT				mCloseAllStringID;
};

#endif