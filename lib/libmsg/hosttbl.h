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

#ifndef __msg_HostTable__
#define __msg_HostTable__ 1

/* This class manages the list of newshosts.  It opens the database at
   startup (or creates one from scratch if it's missing).  */


class MSG_NewsHost;
class CMozillaDatabase;

class msg_HostTable {
public:
	static msg_HostTable* Create(MSG_Master* master);

    ~msg_HostTable();

	int getNumHosts() {return m_numhosts;}
	MSG_NewsHost* getHost(int i) {
		XP_ASSERT(i >= 0 && i < m_numhosts);
		return m_hosts[i];
	}
	MSG_NewsHost* GetDefaultHost(XP_Bool createIfMissing);

	int32 GetHostList(MSG_NewsHost** result, int32 resultsize);

	MSG_NewsHost *AddNewsHost(const char* name, XP_Bool secure, int32 port,
							  const char* newsrcname);

	MSG_NewsHost *FindNewsHost (MSG_FolderInfo *container);

	int RemoveEntry(MSG_NewsHost* host);

	MSG_NewsHost *InferHostFromGroups (const char *groups);

protected:
    msg_HostTable(MSG_Master* master);

	MSG_Master* m_master;
	int32 m_numhosts;
	int32 m_maxhosts;
	MSG_NewsHost** m_hosts;
};



#endif /* __msg_HostTable__ */
