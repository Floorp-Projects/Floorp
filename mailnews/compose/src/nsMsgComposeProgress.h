/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 */

#include "nsIMsgComposeProgress.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIDOMWindowInternal.h"
#include "nsString.h"

class nsMsgComposeProgress : public nsIMsgComposeProgress
{
public: 
	NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGCOMPOSEPROGRESS
  NS_DECL_NSIWEBPROGRESSLISTENER

	nsMsgComposeProgress();
	virtual ~nsMsgComposeProgress();

private:
  nsresult ReleaseListeners(void);

  PRBool                            m_closeProgress;
  PRBool                            m_processCanceled;
  nsString                          m_pendingStatus;
  PRInt32                           m_pendingStateFlags;
  PRInt32                           m_pendingStateValue;
  nsCOMPtr<nsIDOMWindowInternal>    m_dialog;
  nsCOMPtr<nsISupportsArray>        m_listenerList;
};
