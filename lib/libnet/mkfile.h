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

#ifndef MKFILE_H
#define MKFILE_H

#include "mkfsort.h"

extern void NET_InitFileProtocol(void);

extern int 
NET_PrintDirectory(SortStruct **sort_base, NET_StreamClass * stream, char * path, URL_Struct *URL_s);

extern NET_StreamClass *
net_CloneWysiwygLocalFile(MWContext *window_id, URL_Struct *URL_s,
			  uint32 nbytes, const char * wysiwyg_url,
			  const char * base_href);

#endif /* MKFILE_H */
