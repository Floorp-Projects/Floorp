/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   David Bienvenu <bienvenu@nventure.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsMsgXFVirtualFolderDBView_H_
#define _nsMsgXFVirtualFolderDBView_H_

#include "nsMsgSearchDBView.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgSearchNotify.h"
#include "nsCOMArray.h"

class nsMsgXFVirtualFolderDBView : public nsMsgSearchDBView
{
public:
  nsMsgXFVirtualFolderDBView();
  virtual ~nsMsgXFVirtualFolderDBView();

  NS_IMETHOD SetSearchSession(nsIMsgSearchSession *aSearchSession);
  // we override all the methods, currently. Might change...
  NS_DECL_NSIMSGSEARCHNOTIFY

  virtual const char * GetViewName(void) {return "XFVirtualFolderView"; }
  NS_IMETHOD Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, 
        nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount);
  NS_IMETHOD Close();
  NS_IMETHOD GetViewType(nsMsgViewTypeValue *aViewType);
  NS_IMETHOD DoCommand(nsMsgViewCommandTypeValue command);
  virtual nsresult OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey parentKey, PRBool ensureListed);
  NS_IMETHOD GetMsgFolder(nsIMsgFolder **aMsgFolder);


protected:

  nsCOMPtr <nsIMsgFolder> m_viewFolder;
  nsWeakPtr m_searchSession;
  PRInt32 m_numUnread;
  PRInt32 m_numTotal;
};

#endif
