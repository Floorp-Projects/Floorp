/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _WINBIF_H_
#define _WINBIF_H_

#define NSNotifyWindowClassName	"NSMailNotifier"	// class name of the NSNotify window
#define NSMailNotifyMsg			"NSMailNotifyMsg"	// registered message sent to the NSNotify window


// wParam values for NSMailNotifyMsg
#define NSMAIL_GetMailCompleted			1		// client just finished getting new mail (biff should set state to no new msgs)




#endif		// _WINBIF_H_
