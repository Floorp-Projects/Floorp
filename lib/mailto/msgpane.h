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


#ifndef _MsgPane_H_
#define _MsgPane_H_

#include "msg.h"
#include "errcode.h"
#include "msgzap.h"
#include "msgprnot.h"

class MSG_Master;
class MessageDBView;
class MSG_FolderInfo;
class MSG_NewsHost;
class ParseMailboxState;
class msg_Background;
class OfflineImapGoOnlineState;
class MSG_FolderInfoMail;
class MSG_PostDeliveryActionInfo;

struct tImapFilterClosure;

struct msg_incorporate_state;
struct MessageHdrStruct;


#ifdef MOZ_MAIL_NEWS
class PaneListener : public ChangeListener
{
public:
	PaneListener(MSG_Pane *pPane);
	virtual ~PaneListener();
	virtual void OnViewChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	virtual void OnViewStartChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	virtual void OnViewEndChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	virtual void OnKeyChange(MessageKey keyChanged, int32 flags, 
		ChangeListener * instigator);
	virtual void OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
	virtual void OnAnnouncerChangingView(ChangeAnnouncer * /* instigator */, MessageDBView * /* view */) ;
	virtual void StartKeysChanging();
	virtual void EndKeysChanging();

protected:
	MSG_Pane		*m_pPane;
	XP_Bool			m_keysChanging;	// are keys changing?
	XP_Bool			m_keyChanged;	// has a key changed since StartKeysChanging called?
};
#endif /* MOZ_MAIL_NEWS */

// If a MSG_Pane has its url chain ptr set to a non-null value,
// it calls the GetNextURL method whenever it finishes a url that is chainable.
// These include delivering queued mail,  get new mail, and retrieving
// messages for offline use, oddly enough - the three kinds of urls I need to queue.
// Sadly, neither the msg_Background or MSG_UrlQueue do what I want,
// because I need to chain network urls that have their own exit functions
// and indeed chain urls themselves.
class MSG_PaneURLChain
{
public:
	MSG_PaneURLChain(MSG_Pane *pane);
	virtual ~MSG_PaneURLChain();
	virtual int		GetNextURL();	// return 0 to stop chaining.
protected:
	MSG_Pane	*m_pane;
};

class MSG_Pane : public MSG_PrefsNotify {
public:

  // hack..
  // Find a pane of the given type that matches the given context.  If none,
  // find some other pane of the given type (if !contextMustMatch).
  static MSG_Pane* FindPane(MWContext* context,
							MSG_PaneType type = MSG_ANYPANE,
							XP_Bool contextMustMatch = FALSE);

  static XP_Bool PaneInMasterList(MSG_Pane *pane);


  static MSG_PaneType PaneTypeForURL(const char *url);
  XP_Bool		NavigationGoesToNextFolder(MSG_MotionType motionType);
  MSG_Pane(MWContext* context, MSG_Master* master);
  virtual ~MSG_Pane();

  void SetFEData(void*);
  void* GetFEData();

  virtual XP_Bool IsLinePane();
  virtual MSG_PaneType GetPaneType() ;
  virtual void NotifyPrefsChange(NotifyCode /*code*/);

  virtual MSG_Pane* GetParentPane();
  
  MSG_Pane* GetNextPane() {return m_nextPane;}

  MSG_Pane *GetFirstPaneForContext(MWContext *context);

  MSG_Pane *GetNextPaneForContext(MSG_Pane *pane, MWContext *context);

  virtual MWContext* GetContext();
  MSG_Prefs* GetPrefs();

  MSG_Master* GetMaster() {return m_master;}

  virtual MsgERR DoCommand(MSG_CommandType command,
						   MSG_ViewIndex* indices, int32 numindices);

  virtual MsgERR GetCommandStatus(MSG_CommandType command,
								  const MSG_ViewIndex* indices, int32 numindices,
								  XP_Bool *selectable_p,
								  MSG_COMMAND_CHECK_STATE *selected_p,
								  const char **display_string,
								  XP_Bool *plural_p);

  virtual MsgERR SetToggleStatus(MSG_CommandType command,
								 MSG_ViewIndex* indices, int32 numindices,
								 MSG_COMMAND_CHECK_STATE value);

  virtual MSG_COMMAND_CHECK_STATE GetToggleStatus(MSG_CommandType command,
												  MSG_ViewIndex* indices,
												  int32 numindices);

  MsgERR ComposeNewMessage();
  //ComposeMessageToMany calls ComposeNewMessage if nothing was selected
  //otherwise it builds a string containing selected groups to post to.
  MsgERR ComposeMessageToMany(MSG_ViewIndex* indices, int32 numIndices);

  virtual void InterruptContext(XP_Bool safetoo);

  char* CreateForwardSubject(MessageHdrStruct* header);

	// Removes this pane from the main pane list.  This is so that calls to
	// MSG_Master::FindPaneOfType() won't find this one (because, for example,
	// we know we're about to delete this one.)
	void UnregisterFromPaneList();

	// These routines should be used only by the msg_Background class.
	msg_Background* GetCurrentBackgroundJob() {return m_background;}
	void SetCurrentBackgroundJob(msg_Background* b) {m_background = b;}
	void SetShowingProgress(XP_Bool showingProgress) {m_showingProgress = showingProgress;}

	void SetRequestForReturnReceipt(XP_Bool isNeeded);
	XP_Bool GetRequestForReturnReceipt();

	void SetSendingMDNInProgress(XP_Bool inProgress);
	XP_Bool GetSendingMDNInProgress();

	char* MakeMailto(const char *to, const char *cc,
					const char *newsgroups,
					const char *subject, const char *references,
					const char *attachment, const char *host_data,
					XP_Bool xxx_p, XP_Bool sign_p);

protected:
  static MSG_Pane* MasterList;	
  MSG_Pane* m_nextInMasterList;	

  
  MSG_Pane* m_nextPane;			// Link of panes created with the same master.
  
  
  MSG_Master* m_master;
  MWContext* m_context;
  MSG_Prefs* m_prefs;
  void* m_fedata;
  int m_numstack;				// used for DEBUG, and to tell listeners
								// if we're in an update block.
  
  msg_Background* m_background;

  XP_Bool m_requestForReturnReceipt;
  XP_Bool	m_showingProgress;
  XP_Bool m_sendingMDNInProgress;
  MSG_PostDeliveryActionInfo *m_actionInfo;

  MWContext	*m_progressContext;
};


#endif /* _MsgPane_H_ */
