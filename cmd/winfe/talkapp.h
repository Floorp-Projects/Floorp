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

#ifdef __cplusplus
extern "C" {
#endif

// #define VISIBLE_WINDOW

const char talk_className[] =       "__talk_window_class__";
const char talk_fileName[] =        "__talk_infofile_%x%x__";

const char talk_msgConnect[] =      "__talk_connect_message__";
const char talk_msgAppInfo[] =      "__talk_appinfo_message__";
const char talk_msgMessage[] =      "__talk_message_message__";

typedef enum
{
    //  talk_msgConnect
    //
    subHookerIsActive,
    subRegisterApp,
    subUnregisterApp,
    subStateChange,
    subInvalidWindow,

    //  talk_msgAppInfo
    //
    subGetAppFirst,
    subGetAppNext,
    subGetAppType,
}
    TSubMessage;

//  This function is implemented in talk, not in talkapp. The reason
//  it is listed here instead of talk.h is to keep it seperate from the
//  public functions that are listed there. This function is called only
//  by the owner of talkapp.exe.
//
LIBEXPORT unsigned long TALK_RegisterTalkApp(char* keyPath, char* exePath);

#ifdef __cplusplus
} // extern "C"
#endif
