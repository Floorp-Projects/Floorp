/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
