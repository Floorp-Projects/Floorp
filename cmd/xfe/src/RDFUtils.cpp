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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        RDFUtils.cpp                                            //
//                                                                      //
// Description:	Misc RDF XFE specific utilities.                        //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "RDFUtils.h"
#include "xp_str.h"
#include "xpassert.h"

//////////////////////////////////////////////////////////////////////////
//
// XFE Command utilities
//
// Is the URL a 'special' command url that translates to an FE command ?
//
//////////////////////////////////////////////////////////////////////////
/*static*/ XP_Bool
XFE_RDFUtils::ht_IsFECommand(HT_Resource item)
{
    const char* url = HT_GetNodeURL(item);

    return (XP_STRNCMP(url, "command:", 8) == 0);
}
//////////////////////////////////////////////////////////////////////////
/*static*/ CommandType
XFE_RDFUtils::ht_GetFECommand(HT_Resource item)
{
    const char* url = HT_GetNodeURL(item);

    if (url && XP_STRNCMP(url, "command:", 8) == 0)
    {
        return Command::convertOldRemote(url + 8);
    }
    else 
    {
        return NULL;
    }
}
//////////////////////////////////////////////////////////////////////////
