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

#ifndef nsIMsgParseMailMsgState_h___
#define nsIMsgParseMailMsgState_h___

#include "msgCore.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"

/* 0CC23170-1459-11d3-8097-006008128C4E */
#define NS_IPARSEMAILMSGSTATE_IID   \
{ 0xcc23170, 0x1459, 0x11d3, \
  {0x80, 0x97, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

typedef enum
{
  MBOX_PARSE_ENVELOPE,
  MBOX_PARSE_HEADERS,
  MBOX_PARSE_BODY
} MBOX_PARSE_STATE;

class nsIMsgParseMailMsgState : public nsISupports {
public:
	 static const nsIID& GetIID() { static nsIID iid = NS_IPARSEMAILMSGSTATE_IID; return iid; }

	 NS_IMETHOD SetMailDB(nsIMsgDatabase * aDatabase) = 0;
	 NS_IMETHOD Clear() = 0;
	 NS_IMETHOD SetState(MBOX_PARSE_STATE aState) = 0;
	 NS_IMETHOD SetEnvelopePos(PRUint32 aEnvelopePos) = 0;
	 NS_IMETHOD ParseAFolderLine(const char *line, PRUint32 lineLength) = 0;
	 NS_IMETHOD GetNewMsgHdr(nsIMsgDBHdr ** aMsgHeader) = 0;
	 NS_IMETHOD FinishHeader() = 0;
	 NS_IMETHOD GetAllHeaders(char **headers, PRInt32 *headersSize) = 0;
};

#endif /* nsIMsgParseMailMsgState_h___ */
