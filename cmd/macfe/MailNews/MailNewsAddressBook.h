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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// MailNewsAddressBook.h

#pragma once

//#define MOZ_NEWADDR
#ifdef MOZ_NEWADDR
	#include "CAddressBookWindows.h"
#else
class CComposeAddressTableView;

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/

typedef struct ABook ABook;
typedef struct DIR_Server DIR_Server;
typedef struct _XP_List XP_List;
typedef UInt32 ABID;
class CNamePropertiesWindow;
class CListPropertiesWindow;
#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class CAddressBookWindow;

struct SAddressDragInfo
{
	ABID id;
	DIR_Server* dir;
};

class CAddressBookManager
{
public:

							// Should be called when the application starts up
	static void				OpenAddressBookManager(void);
							// Should be called when the application closes
	static void				CloseAddressBookManager(void);
	
	static void				ImportLDIF(const FSSpec& inSpec);
	
	static CAddressBookWindow*	ShowAddressBookWindow(void);
	
	static XP_List			*GetDirServerList(void);
	static void				SetDirServerList(XP_List *inList, Boolean inSavePrefs = true);
	static DIR_Server		*GetPersonalBook(void);
	static ABook			*GetAddressBook(void);

	static void 			FailAddressError(Int32 inError);
	static CNamePropertiesWindow	*FindNamePropertiesWindow(ABID inEntryID);
	static CNamePropertiesWindow	*GetNamePropertiesWindow(ABID inEntryID, Boolean inOptionKeyDown);
	static CListPropertiesWindow	*FindListPropertiesWindow(ABID inEntryID);
	static CListPropertiesWindow	*GetListPropertiesWindow(ABID inEntryID, Boolean inOptionKeyDown);
	static void			DoPickerDialog(  CComposeAddressTableView* initialTable );

private:

	static void				RegisterAddressBookClasses(void);
	static int				DirServerListChanged(const char*, void*)
	{
		sDirServerListChanged = true;
		return 0;
	}

	// Instance variables
	
	static XP_List			*sDirServerList;
	static Boolean			sDirServerListChanged;
	static ABook			*sAddressBook;		// Really, the global address book database
												// used in conjunction with all address books
												// (local and remote) associated with the
												// application. This object is opened at application
												// startup and not closed until the application closes. 
};
#endif // MOZ_NEWADDR
