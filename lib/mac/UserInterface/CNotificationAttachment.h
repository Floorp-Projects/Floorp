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

#include <LAttachment.h>
#include "PascalString.h"

class CNotificationAttachment : public LAttachment
{
	public:
						CNotificationAttachment();
		virtual			~CNotificationAttachment();
	
		static Boolean	IsNotificationPosted(void);

		void			Post(void);
		void			Remove(void);

	protected:
	
		virtual	void 	ExecuteSelf(
								MessageT		inMessage,
								void			*ioParam);

		NMRec			mNotification;
		Handle			mIconSuite;
		CStr255			mNoteString;
		
	 	static CNotificationAttachment* sCurrentNotifier;
};

inline Boolean CNotificationAttachment::IsNotificationPosted(void)
	{	return sCurrentNotifier != NULL;	}