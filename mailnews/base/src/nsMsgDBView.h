/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#ifndef _nsMsgDBView_H_
#define _nsMsgDBView_H_

#include "nsIMsgDBView.h"
#include "nsIMsgDatabase.h"
#include "nsMsgLineBuffer.h" // for nsByteArray
#include "nsMsgKeyArray.h"

// I think this will be an abstract implementation class.
// The classes that implement the outliner support will probably
// inherit from this class.
class nsMsgDBView : public nsIMsgDBView
{
public:
  nsMsgDBView();
  virtual ~nsMsgDBView();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGDBVIEW

protected:
  nsresult ExpandByIndex(nsMsgViewIndex index, PRUint32 *pNumExpanded);
  nsresult ExpandAll();

	PRInt32	  GetSize(void) {return(m_keys.GetSize());}

  // notification api's
	void	EnableChangeUpdates();
	void	DisableChangeUpdates();
	void	NoteChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							   nsMsgViewNotificationCodeValue changeType);
	void	NoteStartChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							   nsMsgViewNotificationCodeValue changeType);
	void	NoteEndChange(nsMsgViewIndex firstlineChanged, PRInt32 numChanged, 
							   nsMsgViewNotificationCodeValue changeType);

  
  nsMsgKeyArray m_keys;
  nsUInt32Array m_flags;
  nsByteArray   m_levels;

  nsCOMPtr <nsIMsgDatabase> m_db;
  PRBool		m_sortValid;
	nsMsgViewSortTypeValue	m_sortType;
  nsMsgViewSortOrderValue m_sortOrder;
  nsMsgDBViewTypeValue m_viewType;
};

#endif
