/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
   addrbk.c	- a hack at the new address book STUFF

	       Created:    Benjie Chen - 6/8/96
	       Copied lots of code from hot.c
	       Then deleted lots of code from hot.c
	       Then deleted lots of code from here
	       Then faked bunch of code here

	       Modified:   Major changes - 7/17
	       		   - I am separating the addressing window stuff
			     from the address book stuff and putting the
			     addressing window stuff off mailcompose window
			     to allow multiple addressing windows and
			     multiple mailcompose window instances - Benjie
*/



#include "mozilla.h"
#include "xp_file.h"
#include "addrbk.h"

#ifdef MOZ_MAIL_NEWS

/* list of DIR_Server 
*/
static ABook*       AddrBook = NULL; 
extern ABook*       fe_GetABook(MWContext *); 

/*****************************************************
 * Actual code begins here 
 */
extern ABook* fe_GetABook(MWContext *context) 
{
  return AddrBook;
}

extern XP_List* FE_GetDirServers()
{
  	char tmp[256];
	XP_List* m_directories = XP_ListNew();

	/* database file is not html anymore ... 
	 */
	PR_snprintf(tmp, sizeof (tmp), "abook.nab"); 
	DIR_GetServerPreferences (&m_directories, tmp);
	return m_directories;
}

ABook* FE_GetAddressBook(MSG_Pane *pane)
{
	return AddrBook;
}

void FE_InitAddrBook() 
{

	static XP_List *directories = NULL;
	char     oldFile[1024];
	XP_File  oldFp = 0;

  	char     tmp[1024];
  	char    *home = getenv("HOME");
  	/*DIR_Server *dir;*/

  	if (!home) home = "";

	PR_snprintf(oldFile, sizeof (oldFile), "%.900s/.netscape/addrbook.db", home); 
	oldFp = XP_FileOpen(oldFile, xpAddrBook, "r");
	if (oldFp) {
		char    newFile[256];
		XP_File newFp = 0;

		/* extern int XP_FileClose(XP_File file);
		 */
		XP_FileClose(oldFp);

		PR_snprintf(newFile, sizeof (newFile), "%s", "abook.nab");
		newFp = XP_FileOpen(newFile, xpAddrBookNew, "r");
		if (!newFp) {
			/* Rename file for backward compatibility reason
			 *   extern int XP_FileRename(const char * from, XP_FileType fromtype,
			 *     const char * to,   XP_FileType totype);
			 */
			XP_FileRename(oldFile, xpAddrBook,
						  newFile, xpAddrBookNew);
		}/* !newFp */
		else
			XP_FileClose(newFp);
	
	}/* if */

	/* all right, lets do the list of directories and stuff */
	directories = XP_ListNew();

	/* first the addressbook stuff */
	/* database file is not html anymore ... */
	PR_snprintf(tmp, sizeof (tmp), "abook.nab"); 

	DIR_GetServerPreferences (&directories, tmp);
	PR_snprintf(tmp, sizeof (tmp), 
				"%.900s/.netscape/address-book.html", home); 
	{
#ifndef MOZ_NEWADDR
		DIR_Server *pabDir = NULL;
		DIR_GetPersonalAddressBook(directories, &pabDir);
		AB_InitializeAddressBook(pabDir, &AddrBook, tmp);
#endif
	}
}

void FE_CloseAddrBook() {
#ifndef MOZ_NEWADDR
  AB_CloseAddressBook(&AddrBook);
#endif
}

#else
extern XP_List* FE_GetDirServers()
{
  return 0 ;
}

#endif

