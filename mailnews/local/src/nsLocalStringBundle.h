/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */ 

#ifndef _nsLocalStringBundle_H__
#define _nsLocalStringBundle_H__

NS_BEGIN_EXTERN_C

PRUnichar     *LocalGetStringByID(PRInt32 stringID);

NS_END_EXTERN_C



#define	IMAP_OUT_OF_MEMORY                                 -1000
#define	LOCAL_STATUS_SELECTING_MAILBOX                      4000
#define	LOCAL_STATUS_DOCUMENT_DONE							4001
#define LOCAL_STATUS_RECEIVING_MESSAGE_OF					4002

#endif /* _nsImapStringBundle_H__ */
