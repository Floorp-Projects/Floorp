/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*

This are the new classes defined to support multi-server conenctions
and biffing. But just because my dog's name has to be somewhere in here
(a log, very long tradition and story we hereby say 'nikidawg' So There.
You would be amazed how many programs/platforms have NIKI in their code...

  We declare a master class for the Biff, which handles the timeouts,
  adding and removing biff classes. Each folder of INBOX type should
  have a biff, altough it does not need to be actively beefing.

*/



class MSG_NikiBiff
{
public:

	MSG_NikiBiff();
	~MSG_NikiBiff();
	void MailCheck(void);
	void SetFolderName(char *name);
	void SetNext(MSG_NikiBiff *next);
	void SetInterval(int32 interval);
	void SetCheckFlag(XP_Bool check);
	void SetEnabled(XP_Bool enabled);
	void SetCallback(void *call);
	void SetStatFile(char *name);
	void SetPrefs(MSG_Prefs *prefs);
	void SetHostName(char *name);
	void SetContext(MWContext *context);
	void OldPOP3Biff(void);
	MSG_NikiBiff* GetNext(void);
	MSG_NikiBiff* RemoveSelf(void);
	time_t GetNextBiffTime(void);
	void PrefsChanged(const char* pref, void *closure);
	char* GetName(void);

private:
	char *m_folderName;
	char *m_host;
	MSG_Pane *m_pane;
	MSG_Master *m_master;
	MWContext *m_context;
	MSG_Prefs *m_prefs;
	int32 m_interval;
	char *m_biffStatFile;
	MSG_NikiBiff *m_next;
	time_t m_nextTime;
	XP_Bool m_needsToCheck;
	XP_Bool m_enabled;
	XP_Bool m_getHeaders;
	XP_Bool m_pop3GetsMail;
	void *m_callback;
};



class MSG_Biff_Master
{
public:

	MSG_Biff_Master();
	~MSG_Biff_Master();
	void Init(MWContext* context, MSG_Prefs* prefs);
	void FindNextBiffer(void);
	void TimeIsUp(void);
	static void Init_Biff_Master(MWContext* context, MSG_Prefs* prefs);
	static void TimerCall(void *closure);
	static int PR_CALLBACK PrefsChanged(const char* pref, void* closure);
	static void AddBiffFolder(char *name, XP_Bool checkMail, int interval, XP_Bool enabled, char *host);
	static void RemoveBiffFolder(char *name);
	static void MailCheckEnable(char *name, XP_Bool enable);
	static XP_Bool NikiCallingGetNewMail(void);
	static MSG_Pane* GetPane(void);
	void FindAndEnable(char *name, XP_Bool enable);
	void AddFolder(char *name, XP_Bool checkMail, int interval, XP_Bool enabled, char *host);
	void RemoveFolder(char *name);
	void PrefCalls(const char* pref, void* closure);
	void SetNikiBiffState(MSG_BIFF_STATE state);
	XP_Bool NikiBusy(void);
	MSG_BIFF_STATE GetNikiBiffState(void);
	MWContext* GetContext(void);
	MWContext* FindMailContext(void);



private:

	MSG_Prefs *m_prefs;
	MSG_Pane *m_nikiPane;
	MWContext *m_context;		// for old biff
	MWContext *m_nikiContext;	// for our new progress dialog
	MSG_Master *m_master;
	MSG_NikiBiff *m_biff;
	MSG_NikiBiff *m_currentBiff;
	void *m_timer;
	MSG_BIFF_STATE m_state;
	int32 m_serverType;
	XP_Bool m_nikiBusy;
};



