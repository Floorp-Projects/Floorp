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
#include "resgui.h"							// main window constants

include "::rsrc:communicator:Communicator.rsrc";

#define USE_RESOURCE_NAMES 0

    	// central/mailmac.cp user messages
  resource 'STR '	( MAIL_WIN_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Create mail window error"
#else
	""
#endif
  	, purgeable ) {
  	"Unexpected error while creating Message window"
  };
  resource 'STR '	( MAIL_TOO_LONG_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Mail text too long"
#else
	""
#endif
  	, purgeable ) {
  	"Included text is too long.\rIt has been truncated."
  };
  resource 'STR '	( MAIL_TMP_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Mail tmp file error"
#else
	""
#endif
  	, purgeable ) {
  	"Could not create a temporary mail file"
  };
  resource 'STR '	( NO_EMAIL_ADDR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"No email address1"
#else
	""
#endif
  	, purgeable ) {
  	"You have not set your email address in \"Preferences\"."
  };
  resource 'STR '	( NO_EMAIL_ADDR_RESID + 1,
#if USE_RESOURCE_NAMES
  	"No email address2"
#else
	""
#endif
  	, purgeable ) {
  	" The recipient of your message will not be able to reply"
  };
  resource 'STR '	( NO_EMAIL_ADDR_RESID + 2,
#if USE_RESOURCE_NAMES
  	"No email address3"
#else
	""
#endif
  	, purgeable ) {
  	" to your mail without it. Please do so before mailing."
  };
  resource 'STR '	( NO_SRVR_ADDR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Create Mail Tmp File1"
#else
	""
#endif
  	, purgeable ) {
  	"You have not set mail server address in \"Preferences\"."
  };
  resource 'STR '	( NO_SRVR_ADDR_RESID + 1,
#if USE_RESOURCE_NAMES
  	"Create Mail Tmp File2"
#else
	""
#endif
  	, purgeable ) {
  	" You cannot send mail without it.\n"
  };
  resource 'STR '	( NO_SRVR_ADDR_RESID + 2,
#if USE_RESOURCE_NAMES
  "Create Mail Tmp File3"
#else
	""
#endif
  , purgeable ) {
  	" Please set it before mailing."
  };
  resource 'STR '	( MAIL_SUCCESS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Mail succeeded"
#else
	""
#endif
  	, purgeable ) {
  	"Mail was successful"
  };
  resource 'STR '	( MAIL_NOT_SENT_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Mail not sent"
#else
	""
#endif
  	, purgeable ) {
  	"Mail was not sent"
  };
  resource 'STR '	( MAIL_SENDING_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Mail sending"
#else
	""
#endif
  	, purgeable ) {
  	"Sending mail…"
  };
  resource 'STR '	( NEWS_POST_RESID + 0,
#if USE_RESOURCE_NAMES
  	"News post"
#else
	""
#endif
  	, purgeable ) {
  	"newspost:"
  };
  resource 'STR '	( MAIL_SEND_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Sending mail error"
#else
	""
#endif
  	, purgeable ) {
  	"Unexpected error while sending mail"
  };
  resource 'STR '	( MAIL_DELIVERY_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Mail delivery error"
#else
	""
#endif
  	, purgeable ) {
  	"Mail delivery failed"
  };
  resource 'STR '	( DISCARD_MAIL_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Discard mail?"
#else
	""
#endif
  	, purgeable ) {
  	"Mail has not been sent yet.\rAre you sure that you want to discard your mail message?"
  };
  resource 'STR '	( SECURITY_LEVEL_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Security level"
#else
	""
#endif
  	, purgeable ) {
  	"This version supports %s security with %s."
  };
  resource 'STR '	( NO_MEM_LOAD_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  "No memory on load"
#else
	""
#endif
  , purgeable ) {
  	"Load has been interrupted because "PROGRAM_NAME" has run out of memory. You might want to increase its memory partition, or quit other applications to alleviate this problem."
  };
  resource 'STR '	( EXT_PROGRESS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Progress via ext app"
#else
	""
#endif
  	, purgeable ) {
  	"Progress displayed in external application"
  };
  resource 'STR '	( REVERT_PROGRESS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Revert progress to "PROGRAM_NAME
#else
	""
#endif
  	, purgeable ) {
  	"Reverting progress to "PROGRAM_NAME
  };
  resource 'STR '	( START_LOAD_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Start load"
#else
	""
#endif
  	, purgeable ) {
  	"Start loading"
  };
  resource 'STR '	( NO_XACTION_RESID + 0,
#if USE_RESOURCE_NAMES
  	"No transaction ID"
#else
	""
#endif
  	, purgeable ) {
  	"Did not get the transaction ID: "
  };
  resource 'STR '	( LAUNCH_TELNET_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Launch telnet"
#else
	""
#endif
  	, purgeable ) {
  	"Launching Telnet application"
  };
  resource 'STR '	( LAUNCH_TN3720_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Launch TN3720"
#else
	""
#endif
  	, purgeable ) {
  	"Launching TN3270 application"
  };
  resource 'STR '	( TELNET_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Telnet launch error"
#else
	""
#endif
  	, purgeable ) {
  	"Telnet launch failed"
  };
  resource 'STR '	( SAVE_AS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Save as"
#else
	""
#endif
  	, purgeable ) {
  	"Save this document as:"
  };
  resource 'STR '	( NETSITE_RESID	 + 0,
#if USE_RESOURCE_NAMES
  	"Netsite:"
#else
	""
#endif
  	, purgeable ) {
  	"Netsite:"
  };
  resource 'STR '	( LOCATION_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Location:"
#else
	""
#endif
  	, purgeable ) {
  	"Location:"
  };
  resource 'STR '	( GOTO_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Go To:"
#else
	""
#endif
  	, purgeable ) {
  	"Go To:"
  };
  resource 'STR '	( DOCUMENT_DONE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Document done"
#else
	""
#endif
  	, purgeable ) {
  	"Document: Done."
  };
  resource 'STR '	( LAYOUT_COMPLETE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Layout complete"
#else
	""
#endif
  	, purgeable ) {
  	"Layout: Complete."
  };
  resource 'STR '	( CONFORM_ABORT_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Confirm abort"
#else
	""
#endif
  	, purgeable ) {
  	"Are you sure that you want to abort the current download?"
  };
  resource 'STR '	( SUBMIT_FORM_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Submit form"
#else
	""
#endif
  	, purgeable ) {
  	"Submit form:%d,%d"
  };
  resource 'STR '	( SAVE_IMAGE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Save image as"
#else
	""
#endif
  	, purgeable ) {
  	"Save Image as:"
  };
  resource 'STR '	( SAVE_QUOTE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Save…"
#else
	""
#endif
  	, purgeable ) {
  	"Save “"
  };
  resource 'STR '	( WILL_OPEN_WITH_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Will open with…"
#else
	""
#endif
  	, purgeable ) {
  	"Will open with “"
  };
  resource 'STR '	( WILL_OPEN_TERM_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Will open terminator"
#else
	""
#endif
  	, purgeable ) {
  	"”."
  };
  resource 'STR '	( SAVE_AS_A_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Save as a…"
#else
	""
#endif
  	, purgeable ) {
  	"Saving as a “"
  };
  resource 'STR '	( FILE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"file"
#else
	""
#endif
  	, purgeable ) {
  		"” file."
  };
  resource 'STR '	( COULD_NOT_SAVE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Could not save"
#else
	""
#endif
  	, purgeable ) {
  	"Could not save "
  };
  resource 'STR '	( DISK_FULL_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Disk full"
#else
	""
#endif
  	, purgeable ) {
  	" because the disk is full."
  };
  resource 'STR '	( DISK_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Disk error"
#else
	""
#endif
  	, purgeable ) {
  			" because of a disk error."
  };
  resource 'STR '	( BOOKMARKS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Bookmarks"
#else
	""
#endif
  	, purgeable ) {
  	"Bookmarks"
  };
  resource 'STR '	( NOT_VISITED_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Not visited"
#else
	""
#endif
  	, purgeable ) {
  	"Not visited"
  };
  resource 'STR '	( NO_FORM2HOTLIST_RESID + 0,
#if USE_RESOURCE_NAMES
  	"No add form to hotlist"
#else
	""
#endif
  	, purgeable ) {
  	"Cannot add the result of a form submission to the hotlist"
  };
  resource 'STR '	( NEW_ITEM_RESID + 0,
#if USE_RESOURCE_NAMES
  	"New item"
#else
	""
#endif
  	, purgeable ) {
  	"New Item"
  };
  resource 'STR '	( NEW_HEADER_RESID + 0,
#if USE_RESOURCE_NAMES
  	"New Folder"
#else
	""
#endif
  	, purgeable ) {
  	"New Folder"
  };
  resource 'STR '	( CONFIRM_RM_HDR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Confirm Remove Folder"
#else
	""
#endif
  	, purgeable ) {
  	"Are you sure that you want to remove the folder \""
  };
  resource 'STR '	( AND_ITEMS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Add items"
#else
	""
#endif
  	, purgeable ) {
  	"\" and its items?"
  };
  resource 'STR '	( SAVE_BKMKS_AS_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Save bookmark as"
#else
	""
#endif
  	, purgeable ) {
  	"Save bookmarks as:"
  };
  resource 'STR '	( END_LIST_RESID + 0,
#if USE_RESOURCE_NAMES
  	"End of list"
#else
	""
#endif
  	, purgeable ) {
  	"End of List"
  };
  resource 'STR '	( ENTIRE_LIST_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Entire list"
#else
	""
#endif
  	, purgeable ) {
  	"Entire List"
  };
  resource 'STR '	( NEW_RESID + 0,
#if USE_RESOURCE_NAMES
  	"new"
#else
	""
#endif
  	, purgeable ) {
  	"new"
  };
  resource 'STR '	( OTHER_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Other"
#else
	""
#endif
  	, purgeable ) {
  	"Other"
  };
  resource 'STR '	( MEM_AVAIL_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Memory available"
#else
	""
#endif
  	, purgeable ) {
  	"M available"
  };
  resource 'STR '	( SAVE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Save"
#else
	""
#endif
  	, purgeable ) {
  	"Save to disk"
  };
  resource 'STR '	( LAUNCH_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Launch"
#else
	""
#endif
  	, purgeable ) {
  	"Launch"
  };
  resource 'STR '	( INTERNAL_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Internal"
#else
	""
#endif
  	, purgeable ) {
  	PROGRAM_NAME" (internal)"
  };
  resource 'STR '	( UNKNOWN_RESID + 0,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
  	"Unknown: Prompt user"
  };
  resource 'STR '	( MEGA_RESID + 0,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
  	"M"
  };
  resource 'STR '	( KILO_RESID + 0,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
  	"K"
  };
  
  resource 'STR '	( PICK_COLOR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Pick color"
#else
	""
#endif
  	, purgeable ) {
  	"Pick a color"
  };
  resource 'STR '	( BAD_APP_LOCATION_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Bad app location"
#else
	""
#endif
  	, purgeable ) {
  	"Finder desktop database reported improper location of the application \"",
  };
  resource 'STR '	( REBUILD_DESKTOP_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Rebuild desktop"
#else
	""
#endif
  	, purgeable ) {
  	"\" to "PROGRAM_NAME". Next time you start up, please rebuild your desktop."
  };
  resource 'STR '	( UNTITLED_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Untitled"
#else
	""
#endif
  	, purgeable ) {
  	"(untitled)"
  };
  resource 'STR '	( REG_EVENT_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Reg. Event Handling Err"
#else
	""
#endif
  	, purgeable ) {
  	"Error in handling RegisterEvent"
  };
  resource 'STR '	( APP_NOT_REG_RESID + 0,
#if USE_RESOURCE_NAMES
  	"App not registered"
#else
	""
#endif
  	, purgeable ) {
  	"This app was not registered"
  };
  resource 'STR '	( UNREG_EVENT_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Unreg. Event Handling Err"
#else
	""
#endif
  	, purgeable ) {
  	"Error in handling UnregisterEvent"
  };
  resource 'STR '	( BOOKMARK_HTML_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Bookmarks HTML"
#else
	""
#endif
  	, purgeable ) {
  	"MCOM-bookmarks.html"
  };
  resource 'STR '	( NO_DISKCACHE_DIR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"No disk cache folder"
#else
	""
#endif
  	, purgeable ) {
  	"Preset disk cache folder could not be found.\r Default folder in Preferences will be used."
  };
  resource 'STR '	( NO_SIGFILE_RESID + 0,
#if USE_RESOURCE_NAMES
  	"No signature file"
#else
	""
#endif
  	, purgeable ) {
  	"The signature file specified in your preferences could not be found."
  };
  resource 'STR '	( NO_BACKDROP_RESID + 0,
#if USE_RESOURCE_NAMES
  	"No backdrop for prefs"
#else
	""
#endif
  	, purgeable ) {
  	"The backdrop file specified in your preferences could not be found."
  };
  resource 'STR '	( SELECT_RESID + 0,
#if USE_RESOURCE_NAMES
  	"Select"
#else
	""
#endif
  	, purgeable ) {
  	"Select "
  };
  resource 'STR '	( AE_ERR_RESID + 0,
#if USE_RESOURCE_NAMES
  	"AE error reply"
#else
	""
#endif
  	, purgeable ) {
  	PROGRAM_NAME" received AppleEvent error reply: "
  };
  resource 'STR ' ( CHARSET_RESID,
#if USE_RESOURCE_NAMES
  	"Message Charset"
#else
	""
#endif
  	, purgeable ) {
  	"x-mac-roman"
  };
  resource 'STR ' ( BROWSE_RESID,
#if USE_RESOURCE_NAMES
  	"Browse"
#else
	""
#endif
  	, purgeable ) {
  	"Choose…"
  };
  
  resource 'STR ' ( ENCODING_CAPTION_RESID,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
  	"Encoding For "
  };

  resource 'STR ' ( NO_TWO_NETSCAPES_RESID,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
  	"You cannot run two versions of "PROGRAM_NAME" at once. "
	"This copy will quit now."
  };

resource 'STR ' ( PG_NUM_FORM_RESID,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Page: %d"
};

resource 'STR ' ( REPLY_FORM_RESID,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Re: %s"
};

resource 'STR ' ( MENU_SEND_NOW,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Send Mail Now"
};

resource 'STR ' ( QUERY_SEND_OUTBOX,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Your Outbox folder contains %d unsent messages. Send them now ?"
};

resource 'STR ' ( QUERY_SEND_OUTBOX_SINGLE,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Your Outbox folder contains an unsent message. Send it now ?"
};

resource 'STR ' ( MENU_SEND_LATER,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Send Mail Later"
};

resource 'STR ' ( MENU_SAVE_AS,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Save as…"
};
resource 'STR ' ( MENU_SAVE_FRAME_AS,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Save Frame as…"
};
resource 'STR ' ( MENU_PRINT,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Print…"
};
resource 'STR ' ( MENU_PRINT_FRAME,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Print Frame…"
};
resource 'STR ' ( MENU_RELOAD,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Reload"
};
resource 'STR ' ( MENU_SUPER_RELOAD,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Super Reload"
};
resource 'STR ' ( MAC_PROGRESS_NET,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
 "Initializing network…"
};
resource 'STR ' ( MAC_PROGRESS_PREFS,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Reading Preferences…"
};
resource 'STR ' ( MAC_PROGRESS_BOOKMARK,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Reading Bookmarks…"
};
resource 'STR ' ( MAC_PROGRESS_ADDRESS,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Reading Address Book…"
};
resource 'STR ' ( MAC_PROGRESS_JAVAINIT,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Initializing Java…"
};
resource 'STR ' ( MAC_UPLOAD_TO_FTP,
#if USE_RESOURCE_NAMES
  	"MAC_UPLOAD_TO_FTP"
#else
	""
#endif
  	, purgeable ) {
	"Are you sure that you want to upload the dragged files to the FTP server?"
};	/*	JJE	*/
resource 'STR ' ( POP_USERNAME_ONLY,
#if USE_RESOURCE_NAMES
  	"POP_USERNAME_ONLY"
#else
	""
#endif
  	, purgeable ) {
	"Your POP User Name is just your user name (e.g. user), not your full POP address (e.g. user@internet.com)."
};	/*	JJE	*/
resource 'STR ' ( MAC_PLUGIN,
#if USE_RESOURCE_NAMES
  	"MAC_PLUGIN"
#else
	""
#endif
  	, purgeable ) {
	"Plug-in:"
};	/*	JJE	*/
resource 'STR ' ( MAC_NO_PLUGIN,
#if USE_RESOURCE_NAMES
  	"MAC_NO_PLUGIN"
#else
	""
#endif
  	, purgeable ) {
	"No plug-ins"
};	/*	JJE	*/
resource 'STR ' ( MAC_REGISTER_PLUGINS,
#if USE_RESOURCE_NAMES
  	"MAC_REGISTER_PLUGINS"
#else
	""
#endif
  	, purgeable ) {
	"Registering plug-ins"
};	/*	JJE	*/
resource 'STR ' ( DOWNLD_CONT_IN_NEW_WIND,
#if USE_RESOURCE_NAMES
  	"DOWNLD_CONT_IN_NEW_WIND"
#else
	""
#endif
  	, purgeable ) {
	"Download continued in a new window."
};	/*	JJE	*/
resource 'STR ' ( ATTACH_NEWS_MESSAGE,
#if USE_RESOURCE_NAMES
  	"ATTACH_NEWS_MESSAGE"
#else
	""
#endif
  	, purgeable ) {
	"News message"
};	/*	JJE	*/
resource 'STR ' ( ATTACH_MAIL_MESSAGE,
#if USE_RESOURCE_NAMES
  	"ATTACH_MAIL_MESSAGE"
#else
	""
#endif
  	, purgeable ) {
	"Mail message"
};	/*	JJE	*/
resource 'STR ' ( MBOOK_NEW_ENTRY,
#if USE_RESOURCE_NAMES
  	"MBOOK_NEW_ENTRY"
#else
	""
#endif
  	, purgeable ) {
	"New entry"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_BACK_IN_FRAME,
#if USE_RESOURCE_NAMES
  	"MCLICK_BACK_IN_FRAME"
#else
	""
#endif
  	, purgeable ) {
	"Back in Frame"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_BACK,
#if USE_RESOURCE_NAMES
  	"MCLICK_BACK"
#else
	""
#endif
  	, purgeable ) {
	"Back"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_FWRD_IN_FRAME,
#if USE_RESOURCE_NAMES
  	"MCLICK_FWRD_IN_FRAME"
#else
	""
#endif
  	, purgeable ) {
	"Forward in Frame"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_FORWARD,
#if USE_RESOURCE_NAMES
  	"MCLICK_FORWARD"
#else
	""
#endif
  	, purgeable ) {
	"Forward"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_SAVE_IMG_AS,
#if USE_RESOURCE_NAMES
  	"MCLICK_SAVE_IMG_AS"
#else
	""
#endif
  	, purgeable ) {
	"Save Image as:"
};	/*	JJE	*/
resource 'STR ' ( SUBMIT_FILE_WITH_DATA_FK,
#if USE_RESOURCE_NAMES
  	"SUBMIT_FILE_WITH_DATA_FK"
#else
	""
#endif
  	, purgeable ) {
	"You can only submit a file that has a data fork."
};	/*	JJE	*/
resource 'STR ' ( ABORT_CURR_DOWNLOAD,
#if USE_RESOURCE_NAMES
  	"ABORT_CURR_DOWNLOAD"
#else
	""
#endif
  	, purgeable ) {
	"Are you sure that you want to abort the current download?"
};	/*	JJE	*/
resource 'STR ' ( MACDLG_SAVE_AS,
#if USE_RESOURCE_NAMES
  	"MACDLG_SAVE_AS"
#else
	""
#endif
  	, purgeable ) {
	"Save as: "
};	/*	JJE	*/
resource 'STR ' ( THE_ERROR_WAS,
#if USE_RESOURCE_NAMES
  	"THE_ERROR_WAS"
#else
	""
#endif
  	, purgeable ) {
	"The error was: "
};	/*	JJE	*/
resource 'STR ' ( MBOOK_NEW_BOOKMARK,
#if USE_RESOURCE_NAMES
  	"MBOOK_NEW_BOOKMARK"
#else
	""
#endif
  	, purgeable ) {
	"New Bookmark"
};	/*	JJE	*/

resource 'STR ' ( MENU_MAIL_DOCUMENT,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Mail Document…"
};	

resource 'STR ' ( MENU_MAIL_FRAME,
#if USE_RESOURCE_NAMES
  	""
#else
	""
#endif
  	, purgeable ) {
	"Mail Frame…"
};	

/* Bookmark Seperate string, For Japanese, please change to something like ---- */
resource 'STR ' ( MBOOK_SEPARATOR_STR,
#if USE_RESOURCE_NAMES
	"MBOOK_SEPARATOR_STR"
#else
	""
#endif
	, purgeable ) {
	"————————————————————"
};	

resource 'STR ' ( RECIPIENT, "", purgeable ) {
	"Recipient"
};

resource 'STR ' ( DELETE_MIMETYPE, "", purgeable ) {
	"Are you sure you want to delete this type? "PROGRAM_NAME" will not be able to display information of this type if you proceed."
};

/* mail attachments are given a content-description string,
 like "Microsoft Word Document".  This suffix comes after
 the application name */
 
resource 'STR ' ( DOCUMENT_SUFFIX, "", purgeable ) {
	"Document"
};

resource 'STR ' ( PASSWORD_CHANGE_STRING, "", purgeable ) {
	"Change Password"
};

resource 'STR ' ( PASSWORD_SET_STRING, "", purgeable ) {
	"Set Password"
};

resource 'STR ' ( IMAGE_SUBMIT_FORM, "", purgeable ) {
	"Submit form:%d,%d"
};

resource 'STR ' ( OTHER_FONT_SIZE, "Other Font Size", purgeable ) {
	"Other (^0)…"
};

resource 'STR ' ( FILE_NAME_NONE, "File Name None", purgeable ) {
	"<none>"
};

resource 'STR ' ( MENU_BACK, "Back", purgeable ) {
	"Back"
};

resource 'STR ' ( MENU_BACK_ONE_HOST, "Back One Site", purgeable ) {
	"Back One Site"
};

resource 'STR ' ( MENU_FORWARD, "Forward", purgeable ) {
	"Forward"
};

resource 'STR ' ( MENU_FORWARD_ONE_HOST, "Forward One Site", purgeable ) {
	"Forward One Site"
};


resource 'STR ' ( MEMORY_ERROR_LAUNCH, "", purgeable ) {
	"There is not enough memory to open ^0.  Try quitting some other applications."
};

resource 'STR ' ( FNF_ERROR_LAUNCH, "", purgeable ) {
	"^0 could not be opened because the application could not be found."
};

resource 'STR ' ( MISC_ERROR_LAUNCH, "", purgeable ) {
	"An unknown error occurred trying to open ^0."
};

resource 'STR ' ( ERROR_LAUNCH_IBM3270, "", purgeable ) {
	"Could not launch IBM Host-on-Demand because the file " IBM3270_FILE " in the folder " IBM3270_FOLDER " could not be found.";
};

resource 'STR ' ( ERROR_OPEN_PROFILE_MANAGER, "", purgeable ) {
	"Please quit "PROGRAM_NAME" before opening the Profile Manager.";
};

resource 'STR ' ( CALENDAR_APP_NAME,
#if USE_RESOURCE_NAMES
	"Netscape Calendar"
#else
	""
#endif
	, purgeable ) {
	"Netscape Calendar";
};

resource 'STR ' ( IMPORT_APP_NAME,
#if USE_RESOURCE_NAMES
	"Netscape Import"
#else
	""
#endif
	, purgeable ) {
	"Netscape Import";
};

resource 'STR ' ( AIM_APP_NAME,
#if USE_RESOURCE_NAMES
	"AOL Instant Messenger"
#else
	""
#endif
	, purgeable ) {
	"AOL Instant Messenger";
};

#if 0
resource 'STR ' ( CONFERENCE_APP_NAME,
#if USE_RESOURCE_NAMES
	"Netscape Conference"
#else
	""
#endif
	, purgeable ) {
	"Netscape Conference";
};
#endif

resource 'STR ' ( NETSCAPE_TELNET,
#if USE_RESOURCE_NAMES
	"Netscape Telnet"
#else
	""
#endif
	, purgeable ) {
	"Netscape Telnet";
};

resource 'STR ' ( NETSCAPE_TELNET_NAME_ARG,
#if USE_RESOURCE_NAMES
	"name= \""
#else
	""
#endif
	, purgeable ) {
	"name= \"";
};

resource 'STR ' ( NETSCAPE_TELNET_HOST_ARG,
#if USE_RESOURCE_NAMES
	"host= \""
#else
	""
#endif
	, purgeable ) {
	"host= \"";
};

resource 'STR ' ( NETSCAPE_TELNET_PORT_ARG,
#if USE_RESOURCE_NAMES
	"port= "
#else
	""
#endif
	, purgeable ) {
	"port= ";
};

resource 'STR ' ( NETSCAPE_TN3270,
#if USE_RESOURCE_NAMES
	"Netscape Tn3270"
#else
	""
#endif
	, purgeable ) {
	"Netscape Tn3270";
};

resource 'STR ' ( CLOSE_WINDOW,
#if USE_RESOURCE_NAMES
	"Close"
#else
	""
#endif
	, purgeable ) {
	"Close";
};

resource 'STR ' ( CLOSE_ALL_WINDOWS,
#if USE_RESOURCE_NAMES
	"Close All"
#else
	""
#endif
	, purgeable ) {
	"Close All";
};

resource 'STR ' ( NO_PRINTER_RESID,
#if USE_RESOURCE_NAMES
	"No printer"
#else
	""
#endif
	, purgeable ) {
	"Your print request could not be completed because no printer has been "
	"selected.  Please use the Chooser to select a printer.";
};

resource 'STR ' ( MALLOC_HEAP_LOW_RESID,
#if USE_RESOURCE_NAMES
	"Malloc Heap Low"
#else
	""
#endif
	, purgeable ) {
	PROGRAM_NAME" is running out of memory.  It is recommended that you quit "
	"other running applications or close "PROGRAM_NAME" windows to continue running "
	"this program.";
};

resource 'STR ' ( JAVA_DISABLED_RESID,
#if USE_RESOURCE_NAMES
	"Java Disabled"
#else
	""
#endif
	, purgeable ) {
	"Java has been disabled as "PROGRAM_NAME" is low on memory..";
};
