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
// mkmarimb.cpp -- Handles "castanet:" URLs for core Navigator, without
//               requiring libmsg. 
//

#include "mkutils.h" 

#include "mkgeturl.h"
#include "mkmarimb.h"
#include "xpgetstr.h"
#include "net.h"
#include "xp_str.h"
#include "client.h"

/* a stub */

MODULE_PRIVATE void
NET_InitMarimbaProtocol(void)
{
	return;
}


/* this is a stub converter

 */
PUBLIC NET_StreamClass *
NET_DoMarimbaApplication(int format_out, 
                         void *data_obj,
                             URL_Struct *url_struct, 
                         MWContext *context)
{
  return(NULL);

}

