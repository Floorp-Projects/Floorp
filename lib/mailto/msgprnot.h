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

#ifndef _MsgPrNot_H_
#define _MsgPrNot_H_

#include "msgzap.h"
#include "rosetta.h"

class MSG_Prefs;

class MSG_PrefsNotify : public MSG_ZapIt {
public:
  // Codes to notify us that a preference item has changed.
	enum NotifyCode {
		Directory,
		CitationStyle,
		PlaintextFont,
		PopHost,
		AutoQuote,
		DefaultHeader,
		DefaultBCC,
		MailServerType,
		ImapOnlineDir,
		HG62422
		WrapLongLines,
		ChangeIMAPDeleteModel
	};

  virtual void NotifyPrefsChange(NotifyCode code) = 0;


};


#endif /* _MsgPrNot_H_ */
