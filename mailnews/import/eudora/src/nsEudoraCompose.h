/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsEudoraCompose_h__
#define nsEudoraCompose_h__

#include "nsString.h"
#include "nsIFileSpec.h"
#include "nsVoidArray.h"

class nsIMsgSend;
class nsIMsgCompFields;
class nsIMsgIdentity;
class nsIMsgSendListener;
class nsIIOService;

#include "nsIMsgSend.h"


class nsEudoraCompose {
public:
	nsEudoraCompose();
	~nsEudoraCompose();

	nsresult	SendMessage( nsIFileSpec *pMsg);

	void		SetBody( const char *pBody, PRInt32 len) { m_pBody = pBody; m_bodyLen = len;}
	void		SetHeaders( const char *pHeaders, PRInt32 len) { m_pHeaders = pHeaders; m_headerLen = len;}
	void		SetAttachments( nsVoidArray *pAttachments) { m_pAttachments = pAttachments;}

private:
	nsresult	CreateComponents( void);
	nsresult	CreateIdentity( void);
	
	void		GetHeaderValue( const char *pHeader, nsString& val);
	void		ExtractCharset( nsString& str);
	void		ExtractType( nsString& str);

	nsMsgAttachedFile * GetLocalAttachments( void);
	void				CleanUpAttach( nsMsgAttachedFile *a, PRInt32 count);

private:
	nsVoidArray *			m_pAttachments;
	nsIMsgSendListener *	m_pListener;
	nsIMsgSend *			m_pMsgSend;
	nsIMsgCompFields *		m_pMsgFields;
	nsIMsgIdentity *		m_pIdentity;
	nsIIOService *			m_pIOService;
	PRInt32					m_headerLen;
	const char *			m_pHeaders;
	PRInt32					m_bodyLen;
	const char *			m_pBody;
};


#endif /* nsEudoraCompose_h__ */
