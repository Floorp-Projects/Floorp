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

#ifndef _MsgMPane_H_
#define _MsgMPane_H_

#include "rosetta.h"

#include "msgpane.h"
#include "msgsec.h"

class MSG_ThreadPane;
class MessageDB;

class MSG_MessagePane : public MSG_Pane 
{
public:
	MSG_MessagePane(MWContext* context, MSG_Master* master);
	virtual ~MSG_MessagePane();

	virtual MSG_PaneType GetPaneType();

	int SetMessagePaneCallbacks(MSG_MessagePaneCallbacks* c, void* closure);
	MSG_MessagePaneCallbacks* GetMessagePaneCallbacks(void** closure);

	virtual MessageDBView *GetMsgView() {return m_view;}
	virtual void SetMsgView(MessageDBView *view) {m_view = view;}
	virtual void SwitchView(MessageDBView *);

	virtual void StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where, int32 num);

	virtual void EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where, int32 num);

	virtual MSG_FolderInfo *GetFolder();
	virtual void SetFolder(MSG_FolderInfo *info);

	virtual PaneListener *GetListener();

	virtual void NotifyPrefsChange(NotifyCode code);

	virtual MsgERR	LoadFolder(MSG_FolderInfo* f) ;
	MsgERR LoadMessage(MSG_FolderInfo* folder, MessageKey key, Net_GetUrlExitFunc *loadUrlExitFunction,
		XP_Bool allow_content_change);


	// This is a true hack.  If called, then this message pane is really just
	// being used to gather info in order to make a composition based on this
	// message.  Cause the message to get loaded, then go launch the
	// composition, and then destroy this message pane.

	MsgERR MakeComposeFor(MSG_FolderInfo* folder, MessageKey key,
						  MSG_CommandType type);
	MsgERR LoadMessage(const char *url);

	virtual int OpenMessageSock(const char *folder_name, const char *msg_id,
								int32 msgnum, void *folder_ptr, void **message_ptr,
								int32 *content_length);

	char* GeneratePartialMessageBlurb(URL_Struct* url,
									  void* closure,
									  MimeHeaders* headers);

	void ActivateReplyOptions(MimeHeaders* headers);

	void PrepareToIncUIDL(URL_Struct* url, const char* uidl);

	// new address book requires us to pull "add to address book" out of commmand status so we can include a dest AB 
	virtual MsgERR AddToAddressBook(MSG_CommandType command, MSG_ViewIndex*indices, int32 numIndices, AB_ContainerInfo * destAB);

	virtual MsgERR DoCommand(MSG_CommandType command,
						   MSG_ViewIndex* indices, int32 numindices);
	virtual MsgERR GetCommandStatus(MSG_CommandType command,
								  const MSG_ViewIndex* indices, int32 numindices,
								  XP_Bool *selectable_p,
								  MSG_COMMAND_CHECK_STATE *selected_p,
								  const char **display_string,
								  XP_Bool *plural_p);

	MsgERR DeleteMessages (MSG_ViewIndex* indices, int32 numindices);
	MsgERR SetViewFromUrl (const char *url);
	// return a url_struct with a url that refers to this pane.
	URL_Struct *  ConstructUrlForMessage(MessageKey key = MSG_MESSAGEKEYNONE);

	int AddAttachment(const char *type, int typelen, const char *name, 
		int namelen, const char *url, uint32 partnumber, 
		const char *description, const char *x_mac_type,
		const char *x_mac_creator);

	void GetCurMessage(MSG_FolderInfo** folder, MessageKey* key,
					   MSG_ViewIndex *index);

	int GetViewedAttachments(MSG_AttachmentData** data, XP_Bool* iscomplete);
	void FreeAttachmentList(MSG_AttachmentData* data);

	HG82224

	XP_Bool GetThreadLineByKey(MessageKey key, MSG_MessageLine* data);

	XP_Bool ShouldRot13Message();
	MsgERR ComposeMessage(MSG_CommandType type, Net_GetUrlExitFunc* func = NULL);
	
	static void PostDeleteLoadExitFunction(URL_Struct *URL_s, int status, MWContext *window_id);

    // used to load next message when using imap delete model
    void SetPostDeleteLoadKey(MessageKey key) { m_postDeleteLoadKey = key; }
    MessageKey GetPostDeleteLoadKey() { return m_postDeleteLoadKey; }
protected:
	char* MakeForwardMessageUrl(XP_Bool quote_original);

	void ClearDerivedStrings();
	MsgERR DeleteMessage ();
	MsgERR AddURLToAddressBook(char* url);
	MsgERR InitializeFolderAndView (const char *folderName);

	static void FinishedLoading(URL_Struct*, int status, MWContext* context);
	static void FinishComposeFor(URL_Struct*, int status, MWContext* context);
	static void DestroyPane(URL_Struct*, int status, MWContext* context);

	static void DoIncWithUIDL_s(URL_Struct *url, int status,
								MWContext *context);
	void DoIncWithUIDL(URL_Struct *url, int status, MWContext *context);
	virtual void IncorporateShufflePartial(URL_Struct *url, int status,
										   MWContext *context);



	MSG_MessagePaneCallbacks* m_callbacks;
	void* m_callbackclosure;

	MSG_FolderInfo* m_folder;
    MessageKey m_key;
    
    // used to load next message when using imap delete model
    MessageKey m_postDeleteLoadKey;

    MessageDBView* m_view;  // the database view this message belongs to 
	PaneListener m_PaneListener;


	// These are a bunch of URL's and strings that we remember from the message
	// (we grab this info as we are streaming the stuff to be translated to
	// HTML and rendered).

	char* m_mailToSenderUrl;		// URL to use for "Reply to sender."
	char* m_mailToAllUrl;			// URL to use for "Reply to all."
	char* m_postReplyUrl;			// URL to use for "Post reply."
	char* m_postAndMailUrl;		// URL to use for "Post and mail reply."
	char* m_displayedMessageSubject; // The subject of the displayed message.
	char* m_displayedMessageId;	// The message-id of the displayed message.

	XP_Bool m_cancellationAllowed;	// For news only -- whether it's OK to
								    // cancel this message. 

	XP_Bool m_justmakecompose;
	MSG_CommandType m_composetype;

	HG22821

	Net_GetUrlExitFunc* m_doneLoadingFunc;
	XP_Bool m_doneLoading;
	XP_Bool m_rot13p;
};


#endif /* _MsgMPane_H_ */
