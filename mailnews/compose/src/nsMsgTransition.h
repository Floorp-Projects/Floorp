/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

//
// The goal is to get this file to ZERO and then all old world stuff will
// be gone...we're getting much closer :-)
//

#ifndef _nsMsgTransition_h_
#define _nsMsgTransition_h_

#define   NS_IMPL_IDS
#include "mimeenc.h"
#include "xpgetstr.h"
#include "xp_qsort.h"
#include "msgcom.h"
#include "rosetta_mailnews.h"
#include "nsMsgZapIt.h"

// These are transitional defines that will go away when we 
// define new NS_... error codes...nsSmptProtocol.cpp is the last 
// holdout.
//
#define MK_SMTP_SERVER_ERROR		-234
#define MK_TCP_READ_ERROR			-252
#define MK_COULD_NOT_LOGIN_TO_SMTP_SERVER -229
#define MK_COULD_NOT_GET_USERS_MAIL_ADDRESS -235
#define MK_OUT_OF_MEMORY			  -207
#define MK_POP3_PASSWORD_UNDEFINED	-313
#define MK_ERROR_SENDING_FROM_COMMAND -230
#define MK_ERROR_SENDING_RCPT_COMMAND -231
#define MK_ERROR_SENDING_DATA_COMMAND -232
#define MK_ERROR_SENDING_MESSAGE	  -233
#define MK_MIME_NO_RECIPIENTS		  -267

class MSG_Pane
{
public:
	void    *unused;
};

#define MIME_MakeFromField(a)  PL_strdup("testmsg@netscape.com")

#endif /* _nsMsgTransition_h_ */
