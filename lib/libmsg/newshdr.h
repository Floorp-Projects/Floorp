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
// news message header - subclass of DBMessagehdr
#ifndef NEWS_HDR_H_
#define NEWS_HDR_H_

#include "msghdr.h"

class NewsMessageHdr : public DBMessageHdr
{
public:
						/** Instance Methods **/
	NewsMessageHdr();
	NewsMessageHdr(MSG_HeaderHandle handle);

	int32				AddToArticle(const char *block, int32 length, MSG_DBHandle db);
	int32				ReadFromArticle(char *block, int32 length, int32 offset, MSG_DBHandle db);
	int32				GetOfflineMessageLength(MSG_DBHandle db);
	virtual void		PurgeOfflineMessage(MSG_DBHandle db);
	virtual	uint32		GetByteLength();
};


#endif
