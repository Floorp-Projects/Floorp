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
/**********************************************************************
 altmail.h

 Interface for alternate mailers.
**********************************************************************/

#ifndef __altmail_h
#define __altmail_h

#ifdef __cplusplus
extern "C" {
#endif

/* function pointers */
extern int (*altmail_RegisterMailClient)(void);
extern int (*altmail_UnRegisterMailClient)(void);
extern int (*altmail_OpenMailSession)(void* reserved1, void* reserved2);
extern int (*altmail_CloseMailSession)(void);
extern int (*altmail_ComposeMailMessage)(void* reserved, char* to, char* org, char* subject, char* body, char* cc, char* bcc);
extern int (*altmail_ShowMailBox)(void);
extern int (*altmail_ShowMessageCenter)(void);
extern char* (*altmail_GetMenuItemString)(void);
extern char* (*altmail_GetNewsMenuItemString)(void);
extern char* (*altmail_HandleNewsUrl)(char* url);

extern void AltMailInit(void);
extern void AltMailOpenSession(void);
extern void AltMailExit(void);

#ifdef __cplusplus
}
#endif

#endif
