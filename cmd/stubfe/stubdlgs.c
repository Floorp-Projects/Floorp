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

/* 
   stubdlgs.c --- stub fe handling of dialog requests from backend (prompt for
                  password, alerts, messages, etc.)
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "fe_proto.h"

/*
** FE_Alert - put up an alert dialog containing the given msg, over
** context.
**
** This method should return immediately, without waiting for user
** input.
*/
void
FE_Alert(MWContext *context,
	 const char *msg)
{
}

/*
** FE_Message - put up an information dialog containing the given msg,
** over context.
**
** This method should return immediately, without waiting for user
** input.
*/
void
FE_Message(MWContext *context,
	   const char *msg)
{
}


/*
** FE_Confirm - Put up an confirmation dialog (with Yes and No
** buttons) and return TRUE if Yes/Ok was clicked and FALSE if
** No/Cancel was clicked.
**
** This method should not return until the user has clicked on one of
** the buttons.
*/
Bool
FE_Confirm(MWContext *context,
	   const char *msg)
{
}

/*
** FE_Prompt - Put up a prompt dialog with the given message, a text
** field (with the value defaulted to dflt).  The user's response
** should be returned.
**
** This method should not return until the user has clicked Ok (the
** return value should not be NULL) or they clicked Cancel (the return
** value should be NULL.)
*/
char*
FE_Prompt(MWContext *context,
	  const char *msg,
	  const char *dflt)
{
}

/*
** FE_PromptPassword - Put up a prompt dialog with the given message,
** a text field.  The user's response should be returned.
**
** The text field should not show the characters as the user types them.
** Display them as X's, *'s, spaces, etc.
**
** This method should not return until the user has clicked Ok (the
** return value should not be NULL) or they clicked Cancel (the return
** value should be NULL.)
*/
char*
FE_PromptPassword(MWContext *context,
		  const char *msg)
{
}

/*
** FE_PromptMessageSubject - Put up a prompt dialog with the given
** message, a text field.  The user's response should be returned.
**
** The default value in the text field should be "(No Subject)",
** localized to the user's locale.
**
** This method should not return until the user has clicked Ok (the
** return value should not be NULL) or they clicked Cancel (the return
** value should be NULL.)
*/
char*
FE_PromptMessageSubject(MWContext *context)
{
}

/*
** FE_PromptUsernameAndPassword - Put up a prompt dialog with the given
** message, a two text fields.  It should return TRUE if Ok was clicked
** and FALSE if Cancel was clicked.
**
** The password text field should not show the characters as the user
** types them.  Display them as X's, *'s, spaces, etc.
**
** This method should not return until the user has clicked Ok or Cancel.
*/
Bool
FE_PromptUsernameAndPassword(MWContext *context,
			     const char *msg,
			     char **username,
			     char **password)
{
}


/*
** FE_PromptForFileNam - Put up a file selection dialog with the given
** prompt string, 
**
** the file selection box should open up viewing default_path.
**
** if file_must_exist_p, the user should not be allowed to close the
** dialog with an invalid path selected.
**
** if directories_allowed_p, directories can be selected.
**
** After the user has clicked ok or cancel or given some other gesture
** to bring down the filesb, the ReadFileNameCallbackFunction should
** be called with the context, the textual path name, and the closure, as in
** 'fn(context, path, closure);'
**
** Lastly, the function should return 0 if the path was acceptable and -1 if it
** was not.
**
** This method should not return until the user has clicked Ok or Cancel.
*/
int
FE_PromptForFileName(MWContext *context,
		     const char *prompt_string,
		     const char *default_path,
		     XP_Bool file_must_exist_p,
		     XP_Bool directories_allowed_p,
		     ReadFileNameCallbackFunction fn,
		     void *closure)
{
}

/*
** FE_SaveDialog - Put up a dialog that basically says "I'm saving
** <foo> right now", cancel?
**
** This function should not block, but should return after putting up the dialog.
**
** If the user clicks cancel, the callback should call EDT_SaveCancel.
**
** Note: This function has been overloaded for use in publishing as well.  There
** are three instances where this function will be called:
**   1) Saving remote files to disk.
**   2) Preparing to publish files remotely.
**   3) Publishing files to a remote server.
*/
void
FE_SaveDialogCreate(MWContext *context,
		    int file_count,
		    ED_SaveDialogType save_type)
{
}

/*
** FE_SaveDialogSetFilename - for a save dialog that has been put up above the given
** context, set the filename being saved/published.
*/
void
FE_SaveDialogSetFilename(MWContext *context,
			 char *filename)
{
}

/*
** FE_SaveDialogDestroy - the backend calls this function to let us
** know that the save/publish operation has completed.  We should
** destroy the save dialog that has been used above the given context.
*/
void
FE_SaveDialogDestroy(MWContext *context,
		     int status,
		     char *filename)
{
}

/*
** FE_SaveFileExistsDialog - put up the standard dialog saying:
** "<foo> exists - overwrite?"  Yes to All, Yes, No, No to All.
**
** return ED_SAVE_OVERWRITE_THIS if the user clicks Yes.
** return ED_SAVE_OVERWRITE_ALL if the user clicks Yes to All.
** return ED_SAVE_DONT_OVERWRITE_THIS if the user clicks No.
** return ED_SAVE_DONT_OVERWRITE_ALL if the user clicks No to All.
*/
ED_SaveOption
FE_SaveFileExistsDialog(MWContext *context,
			char *filename)
{
}

/*
** FE_SaveErrorContinueDialog - put up a dialog that gives some
** textual represenation of the error status, and allow the user
** to decide if they want to continue or not.
**
** Return TRUE if we should continue, and FALSE if we shouldn't.
*/
Bool
FE_SaveErrorContinueDialog(MWContext *context,
			   char *filename,
			   ED_FileError error)
{
}
