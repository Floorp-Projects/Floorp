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

//////////////////////////////////////////////////////////////////////////
//
// RDF folder and item utilities
//
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindFolderByName(HT_Resource root,
								  char * folder_name)
{
    if (!root || !folder_name)
    {
        return NULL;
    }

    HT_Resource header;

    char * header_name = HT_GetNodeName(root);

    if(XP_STRCMP(header_name,folder_name) == 0)
    {
        return root;
    }

    
    HT_SetOpenState(root, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(root);
    HT_Resource child;
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        if (HT_IsContainer(child))
        {
            header = ht_FindFolderByName(child, folder_name);

            if(header != NULL)
            {
                HT_DeleteCursor(child_cursor);
                return header;
            }
        }
    }
    HT_DeleteCursor(child_cursor);

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindItemByAddress(HT_Resource     root,
								   const char *    target_address)
{
    if (!root || !target_address)
    {
        return NULL;
    }

    char * entry_address = HT_GetNodeURL(root);

    if (entry_address && (XP_STRCMP(target_address,entry_address) == 0))
    {
        return root;
    }

    if (HT_IsContainer(root))
    {
        HT_Resource entry;

        HT_SetOpenState(entry, PR_TRUE);
        HT_Cursor child_cursor = HT_NewCursor(entry);
        HT_Resource child;
        while ( (child = HT_GetNextItem(child_cursor)) )
        {
            HT_Resource test_entry;

            test_entry = ht_FindItemByAddress(entry,target_address);
            
            if (test_entry)
            {
                HT_DeleteCursor(child_cursor);
                return test_entry;
            }
        }
        HT_DeleteCursor(child_cursor);
    }

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindNextItem(HT_Resource current)
{
    if (current)
    {
        HT_View ht_view = HT_GetView(current);
        uint32 index = HT_GetNodeIndex(ht_view, current);
        return HT_GetNthItem(ht_view, index + 1);
     }
    
     return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindPreviousItem(HT_Resource current)
{

    if (current)
    {
        HT_View ht_view = HT_GetView(current);
        uint32 index = HT_GetNodeIndex(ht_view, current);
        if (index > 0)
            return HT_GetNthItem(ht_view, index - 1);
    }

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_RDFUtils::ht_FolderHasFolderChildren(HT_Resource header)
{
    XP_ASSERT( header != NULL );
    XP_ASSERT( HT_IsContainer(header) );

    HT_SetOpenState(header, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(header);
    HT_Resource child;

    // Traverse until we find a header entry
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        if (HT_IsContainer(child)) 
        {
            HT_DeleteCursor(child_cursor);
            return True;
        }
    }
    HT_DeleteCursor(child_cursor);
    return False;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Guess the title for a url address
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::guessTitle(MWContext *		context,
						 const char *		address,
						 XP_Bool			sameShell,
						 char **			guessedTitleOut,
						 time_t *			guessedLastDateOut)
{
    XP_ASSERT( context != NULL );
    XP_ASSERT( address != NULL );
    XP_ASSERT( guessedTitleOut != NULL );
    XP_ASSERT( guessedLastDateOut != NULL );
    
    // Initialize the results to begin with
    *guessedLastDateOut    = 0;
    *guessedTitleOut       = NULL;

    // If the operation occurs in the same shell, look in the history.
    if (sameShell)
    {
        // Obtain the current history entry
        History_entry * hist_entry = SHIST_GetCurrent(&context->hist);

        // Make sure the history entry matches the given address, or else
        // the caller used a the same shell with a link that is not the 
        // the proxy icon (ie, in the html area).
        if (hist_entry && (XP_STRCMP(hist_entry->address,address) == 0))
        {
            *guessedTitleOut = hist_entry->title;
            *guessedLastDateOut = hist_entry->last_access;
        }
    }

#if 0
/* Currently don't for a static way to get the bookmarks */

    // Look in the bookmarks for a dup
    if (!*guessedTitleOut)
    {
        HT_Resource test_entry = NULL;
        HT_Resource root_entry = getRootFolder();

        if (root_entry)
        {
            test_entry = ht_FindItemByAddress(root_entry, address);
        }

        if (test_entry)
        {
            *guessedTitleOut = HT_GetNodeName(test_entry);
        }
    }
#endif

    // As a last resort use the address itself as a title
    if (!*guessedTitleOut)
    {
        *guessedTitleOut = (char *) address;
    }

    // Strip the leading http:// from the title
    if (XP_STRLEN(*guessedTitleOut) > 7)
    {
        if ((XP_STRNCMP(*guessedTitleOut,"http://",7) == 0) ||
            (XP_STRNCMP(*guessedTitleOut,"file://",7) == 0))
        {
            (*guessedTitleOut) += 7;
        }
    }
}
//////////////////////////////////////////////////////////////////////////
