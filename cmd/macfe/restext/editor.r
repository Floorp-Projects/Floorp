 /*
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
#include "Types.r"
#include "resgui.h"					// main window constants

resource 'STR '	( NO_SRC_EDITOR_PREF_SET + 0, "", purgeable ) {
"You have not set a default application to use when opening HTML files.  Please choose an application to use."
};

resource 'STR '	( NO_IMG_EDITOR_PREF_SET + 0, "", purgeable ) {
"You have not set a default application to use when opening graphics files.  Please choose an application to use."
};

resource 'STR '	( INVALID_PUBLISH_LOC + 0, "", purgeable ) {
"The URL of the location you want to publish to must begin with “ftp://” or “http://”."
};

resource 'STR '	( NO_DICTIONARY_FOUND + 0, "", purgeable ) {
"The dictionary for the spelling checker is not installed."
};

resource 'STR '	( NO_SPELL_SHLB_FOUND + 0, "", purgeable ) {
"The Spelling Checker tool (NSSpellChecker) is not installed."
};

resource 'STR '	( CLOSE_STR_RESID + 0, "", purgeable ) {
"Close"
};

resource 'STR '	( DUPLICATE_TARGET + 0, "", purgeable ) {
"That target name already exists.  "
};

resource 'STR '	( DUPLICATE_TARGET + 1, "", purgeable ) {
"Please choose another one."
};

resource 'STR '	( CUT_ACROSS_CELLS + 0, "", purgeable ) {
"You have selected more than one cell.  "
};

resource 'STR '	( CUT_ACROSS_CELLS + 1, "", purgeable ) {
"You can’t change the contents of multiple cells at once.  Change your selection to include only one cell and repeat this action."
};

resource 'STR '	( NOT_HTML + 0, "", purgeable ) {
"Page Composer can’t edit this kind of file."
};

resource 'STR '	( BAD_TAG + 0, "", purgeable ) {
"You need to enclose HTML tags in angle brackets: <TAG>."
};

resource 'STR ' ( EDITOR_ERROR_EDIT_REMOTE_IMAGE + 0, "", purgeable ) {
"The image you are trying to edit is not on your hard disk.  You need to save this image to your hard disk before you can edit it."
};

resource 'STR ' ( EDITOR_PERCENT_WINDOW, "", purgeable ) {
"% of window"
};

resource 'STR ' ( EDITOR_PERCENT_PARENT_CELL, "", purgeable ) {
"% of current cell"
};
