/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef MKMARIMBA_H
#define MKMARIMBA_H

XP_BEGIN_PROTOS

extern void NET_InitMarimbaProtocol(void);

/* MIME Stuff */

typedef struct _NET_AppMarimbaStruct {
    MWContext *context;
    int32      content_length;
    int32      bytes_read;
    int32          bytes_alloc;
    char           *channelData;
} NET_ApplicationMarimbaStruct;


NET_StreamClass * NET_DoMarimbaApplication(int format_out, 
												  void *data_obj, 
												  URL_Struct *url_struct, 
												  MWContext *context);


XP_END_PROTOS

#endif 


