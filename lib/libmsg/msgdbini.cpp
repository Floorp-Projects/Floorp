/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "msg.h"
#include "xp.h"
#include "prefapi.h"
#include "imap.h"
#include "msgdb.h"
#include "thrhead.h"
#include "msgdbapi.h"
// forward decls.

extern "C"
void MSG_InitMsgLib()	// one-time initialization of libmsg
{
	IMAP_StartupImap();

	MSG_InitDB();
	int32 maxArticles = 0;

	PREF_GetIntPref("news.max_articles", &maxArticles);
	NET_SetNumberOfNewsArticlesInListing(maxArticles);
}

extern "C"
void MSG_ShutdownMsgLib()
{
#ifdef DEBUG
	XP_ASSERT(MessageDB::GetNumInCache() == 0);	// better not be any open db's.
	if (MessageDB::GetNumInCache() != 0)
		MessageDB::DumpCache();
#endif
	MessageDB::CleanupCache();
	// make sure we're not leaking message headers.
#ifdef DEBUG_bienvenu
	XP_ASSERT(DBMessageHdr::numMessageHdrs == 0);
	if (DBMessageHdr::numMessageHdrs > 0)
		XP_Trace("%ld headers leaked\n", DBMessageHdr::numMessageHdrs);
	XP_ASSERT(DBThreadMessageHdr::m_numThreadHeaders == 0);
	if (DBThreadMessageHdr::m_numThreadHeaders > 0)
		XP_Trace("%ld neo thread headers leaked\n", DBThreadMessageHdr::m_numThreadHeaders);
#endif

	MSG_ShutdownDB();
    IMAP_ShutdownImap();
}

