/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsMsgSpecialViews_H_
#define _nsMsgSpecialViews_H_

#include "nsMsgThreadedDBView.h"

class nsMsgThreadsWithUnreadDBView : public nsMsgThreadedDBView
{
public:
	nsMsgThreadsWithUnreadDBView();
	virtual ~nsMsgThreadsWithUnreadDBView();
	virtual const char * GetViewName(void) {return "ThreadsWithUnreadView"; }
    NS_IMETHOD Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount);
    NS_IMETHOD GetViewType(nsMsgViewTypeValue *aViewType);

	virtual PRBool		WantsThisThread(nsIMsgThread *threadHdr);
protected:
  virtual nsresult AddMsgToThreadNotInView(nsIMsgThread *threadHdr, nsIMsgDBHdr *msgHdr, PRBool ensureListed);

};

class nsMsgWatchedThreadsWithUnreadDBView : public nsMsgThreadedDBView
{
public:
    NS_IMETHOD GetViewType(nsMsgViewTypeValue *aViewType);
	virtual const char * GetViewName(void) {return "WatchedThreadsWithUnreadView"; }
	virtual PRBool		WantsThisThread(nsIMsgThread *threadHdr);
protected:
  virtual nsresult AddMsgToThreadNotInView(nsIMsgThread *threadHdr, nsIMsgDBHdr *msgHdr, PRBool ensureListed);

};
#ifdef DOING_CACHELESS_VIEW
// This view will initially be used for cacheless IMAP.
class nsMsgCachelessView : public nsMsgDBView
{
public:
						nsMsgCachelessView();
    NS_IMETHOD GetViewType(nsMsgViewTypeValue *aViewType);
	virtual 			~nsMsgCachelessView();
	virtual const char * 		GetViewName(void) {return "nsMsgCachelessView"; }
	NS_IMETHOD Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue viewType, PRInt32 *count);
	nsresult				SetViewSize(PRInt32 setSize); // Override
	virtual nsresult		AddNewMessages() ;
	virtual nsresult		AddHdr(nsIMsgDBHdr *msgHdr);
	// for news, xover line, potentially, for IMAP, imap line...
	virtual nsresult		AddHdrFromServerLine(char *line, nsMsgKey *msgId) ;
	virtual void		SetInitialSortState(void);
	virtual	nsresult		Init(PRUint32 *pCount);
protected:
	void				ClearPendingIds();

	nsIMsgFolder		*m_folder;
	nsMsgViewIndex		m_curStartSeq;
	nsMsgViewIndex		m_curEndSeq;
	PRBool				m_sizeInitialized;
};

#endif /* DOING_CACHELESS_VIEW */
#endif
