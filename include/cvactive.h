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

#ifndef CV_ACTIVE
#define CV_ACTIVE

#include "net.h"

/* define a constant to be passed to CV_MakeMultipleDocumentStream
 * as the data_object to signify that it should return
 * MK_END_OF_MULTIPART_MESSAGE when it gets to the end
 * of the multipart instead of waiting for the complete
 * function to be called
 */
#define CVACTIVE_SIGNAL_AT_END_OF_MULTIPART 999

XP_BEGIN_PROTOS

extern NET_StreamClass * 
CV_MakeMultipleDocumentStream (int         format_out,
                               void       *data_object,
                               URL_Struct *URL_s,
                               MWContext  *window_id);
XP_END_PROTOS

#endif /* CV_ACTIVE */
