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

#include "structs.h"
#include "ntypes.h"
#include "proto.h"
#include "net.h"
#include "plevent.h"

PRThread *mozilla_thread;
PREventQueue* mozilla_event_queue;

int
main(int argc,
     char *argv)
{
  URL_Struct *url;
  MWContext *context = XP_NewContext();

  url = NET_CreateURLStruct("http://www.netscape.com", NET_NORMAL_RELOAD);
}
