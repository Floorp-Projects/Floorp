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
#include <Types.r>
#undef BETA
#undef ALPHA
#include <SysTypes.r>

#include "resae.h"					// Apple event constants
#include "resgui.h"					// main window constants
#include "mversion.h"				// version numbers, application name, etc
#include "csid.h"

#include "PowerPlant.r"

// Use the Mercutio MDEF
#define	nsMenuProc	19999

resource 'MENU' ( 666, "Mercutio Menu" )
{ 
	666, nsMenuProc, 0, 0, "View", {}
};


type 'LONG' {
	array LongEntries {
		longint;	/* Just an int32 */
	};
};

// contain boolean values
type 'BOOL' {
	array BooleanEntries {
		byte;		/* boolean values */
	};
};

type 'MIME' {
	literal longint;	/* Application signature */
	literal longint;	/* File type */
	byte;				/* User action - Save = 0, Launch = 1, Internal = 2, Unknown = 3 */
	pstring;			/* Application name */
	pstring;			/* Mime type */
	pstring;			/* extension */
};

type 'SMIM' {
	literal longint;	/* Application signature */
	literal longint;	/* File Type */
	byte;				/* User action - Save = 0, Launch = 1, Internal = 2, Unknown = 3 */
	pstring;			/* Application name */
	pstring;			/* Mime type */
	pstring;			/* extension */
};



type 'Fnec' {
	integer = $$CountOf(FnecEntries);
	wide array FnecEntries	{
		pstring[31];		/* Language Group Name */
		pstring[31];		/* proportional font name */
		pstring[31];		/* fixed font name */
		unsigned integer;	/* proportional font size */
		unsigned integer;	/* fixed font size */
		unsigned integer;	/* character set encoding ID */
	    integer Script;		/* script id of font fallback */
		unsigned integer;	/* res id for Txtr Button */
		unsigned integer;	/* res id for Txtr Text Field */
	};
};

/* maps command numbers to cs id's */
type 'Csid' {
	integer = $$CountOf(table);
	wide array table	{
		unsigned integer;	/* win_csid  */
		unsigned integer;	/* doc_csid  */
		longint;			/* CmdNum */
	};
};



#include "custom.r"

/*-----------------------------------------------------------------------------
	These are all standard PowerPlant resources which we are modifying.
	Anything which we add which is not a replacement is in a different file.
-----------------------------------------------------------------------------*/

resource 'STR#' (200, "Standards", purgeable) {{
	APPLICATION_NAME, // used in Alert boxes
	"Save File As:",
	"CanÕt Undo"
}};


// ¥ÊOther stuff
resource 'STR#' ( HELP_URLS_MENU_STRINGS, "Help URL Menu Entries", purgeable ) {{
	"About Netscape",
	"About Plugins",
	"Registration Information",
	"(-",
	"Handbook",
	"Release Notes",
	"Frequently Asked Questions",
	"On Security",
	"(-",
	"How to Give Feedback",
	"How to Get Support",
	"How to Create Web Services",
}};

resource 'STR#' ( HELP_URLS_RESID, "Help URLs", purgeable ) {{
	ABOUT_LOCATION,
	ABOUT_PLUGINS_LOCATION,
	REGISTRATION_LOCATION,
	"",
	MANUAL_LOCATION,			// 
	VERSION_LOCATION,			// release notes
	FAQ_LOCATION,				// FAQ
	HELPONSEC_LOCATION,
	"",
	FEEDBACK_LOCATION,	// FEEDBACK
	SUPPORT_LOCATION,
	WEBSERVICES_LOCATION,
}};

resource 'STR#' ( WINDOW_TITLES_RESID, "" ) {{
	"Mail",
	"News",
	"Editor",
}};

// ¥Ênews group column headers (i18n)
resource 'STR#' ( 4010, "" ) {{
	"News Server",
	"#3080",						// subscribed icon
	"Unread",
	"Total",
}};

// ¥ news and mail article column headers (i18n)
resource 'STR#' ( 4011, "" ) {{
	"Subject",
	"#3082",						// flag icon
	"#3079",						// read icon
	"Sender",
	"Date",
	"Recipient",					
}};

// ¥Êmail folder column headers (i18n)
resource 'STR#' ( 6010, "" ) {{
	"Folder",
	"",
	"Unread",
	"Total",
}};

    	// central/mailmac.cp user messages
  resource 'STR '	( MAIL_WIN_ERR_RESID + 0, "Create mail window error", purgeable ) {
  	"Unexpected error while creating Mail/News window"
  };
  resource 'STR '	( MAIL_TOO_LONG_RESID + 0, "Mail text too long", purgeable ) {
  	"Included text is too long.\rIt has been truncated."
  };
  resource 'STR '	( MAIL_TMP_ERR_RESID + 0, "Mail tmp file error", purgeable ) {
  	"Could not create a temporary mail file"
  };
  resource 'STR '	( NO_EMAIL_ADDR_RESID + 0, "No email address1", purgeable ) {
  	"You have not set your email address in \"Preferences\"."
  };
  resource 'STR '	( NO_EMAIL_ADDR_RESID + 1, "No email address2", purgeable ) {
  	" The recipient of your message will not be able to reply"
  };
  resource 'STR '	( NO_EMAIL_ADDR_RESID + 2, "No email address3", purgeable ) {
  	" to your mail without it. Please do so before mailing."
  };
  resource 'STR '	( NO_SRVR_ADDR_RESID + 0, "Create Mail Tmp File1", purgeable ) {
  	"You have not set mail server address in \"Preferences\"."
  };
  resource 'STR '	( NO_SRVR_ADDR_RESID + 1, "Create Mail Tmp File2", purgeable ) {
  	" You cannot send mail without it.\n"
  };
  resource 'STR '	( NO_SRVR_ADDR_RESID + 2, "Create Mail Tmp File3", purgeable ) {
  	" Please set it before mailing."
  };
  resource 'STR '	( MAIL_SUCCESS_RESID + 0, "Mail succeeded", purgeable ) {
  	"Mail was successful"
  };
  resource 'STR '	( MAIL_NOT_SENT_RESID + 0, "Mail not sent", purgeable ) {
  	"Mail was not sent"
  };
  resource 'STR '	( MAIL_SENDING_RESID + 0, "Mail sending", purgeable ) {
  	"Sending mailÉ"
  };
  resource 'STR '	( NEWS_POST_RESID + 0, "News post", purgeable ) {
  	"newspost:"
  };
  resource 'STR '	( MAIL_SEND_ERR_RESID + 0, "Sending mail error", purgeable ) {
  	"Unexpected error while sending mail"
  };
  resource 'STR '	( MAIL_DELIVERY_ERR_RESID + 0, "Mail delivery error", purgeable ) {
  	"Mail delivery failed"
  };
  resource 'STR '	( DISCARD_MAIL_RESID + 0, "Discard mail?", purgeable ) {
  	"Mail has not been sent yet.\rAre you sure that you want to discard your mail message?"
  };
  resource 'STR '	( SECURITY_LEVEL_RESID + 0, "Security level", purgeable ) {
  	"This version supports %s security with %s."
  };
  resource 'STR '	( NO_MEM_LOAD_ERR_RESID + 0, "No memory on load", purgeable ) {
  	"Load has been interrupted because Netscape has run out of memory. You might want to increase its memory partition, or quit other applications to alleviate this problem."
  };
  resource 'STR '	( EXT_PROGRESS_RESID + 0, "Progress via ext app", purgeable ) {
  	"Progress displayed in external application"
  };
  resource 'STR '	( REVERT_PROGRESS_RESID + 0, "Revert progress to Netscape", purgeable ) {
  	"Reverting progress to Netscape"
  };
  resource 'STR '	( START_LOAD_RESID + 0, "Start load", purgeable ) {
  	"Start loading"
  };
  resource 'STR '	( NO_XACTION_RESID + 0, "No transaction ID", purgeable ) {
  	"Did not get the transaction ID: "
  };
  resource 'STR '	( LAUNCH_TELNET_RESID + 0, "Launch telnet", purgeable ) {
  	"Launching Telnet application"
  };
  resource 'STR '	( LAUNCH_TN3720_RESID + 0, "Launch TN3720", purgeable ) {
  	"Launching TN3270 application"
  };
  resource 'STR '	( TELNET_ERR_RESID + 0, "Telnet launch error", purgeable ) {
  	"Telnet launch failed"
  };
  resource 'STR '	( SAVE_AS_RESID + 0, "Save as", purgeable ) {
  	"Save this document as:"
  };
  resource 'STR '	( NETSITE_RESID	 + 0, "Netsite:", purgeable ) {
  	"Netsite:"
  };
  resource 'STR '	( LOCATION_RESID + 0, "Location:", purgeable ) {
  	"Location:"
  };
  resource 'STR '	( GOTO_RESID + 0, "Go To:", purgeable ) {
  	"Go To:"
  };
  resource 'STR '	( DOCUMENT_DONE_RESID + 0, "Document done", purgeable ) {
  	"Document: Done."
  };
  resource 'STR '	( LAYOUT_COMPLETE_RESID + 0, "Layout complete", purgeable ) {
  	"Layout: Complete."
  };
  resource 'STR '	( CONFORM_ABORT_RESID + 0, "Confirm abort", purgeable ) {
  	"Are you sure that you want to abort the current download?"
  };
  resource 'STR '	( SUBMIT_FORM_RESID + 0, "Submit form", purgeable ) {
  	"Submit form:%d,%d"
  };
  resource 'STR '	( SAVE_IMAGE_RESID + 0, "Save image as", purgeable ) {
  	"Save Image as:"
  };
  resource 'STR '	( SAVE_QUOTE_RESID + 0, "SaveÉ", purgeable ) {
  	"Save Ò"
  };
  resource 'STR '	( WILL_OPEN_WITH_RESID + 0, "Will open withÉ", purgeable ) {
  	"Will open with Ò"
  };
  resource 'STR '	( SAVE_AS_A_RESID + 0, "Save as aÉ", purgeable ) {
  	"Saving as a Ò"
  };
  resource 'STR '	( FILE_RESID + 0, "file", purgeable ) {
  		"Ó file."
  };
  resource 'STR '	( COULD_NOT_SAVE_RESID + 0, "Could not save", purgeable ) {
  	"Could not save "
  };
  resource 'STR '	( DISK_FULL_RESID + 0, "Disk full", purgeable ) {
  	" because the disk is full."
  };
  resource 'STR '	( DISK_ERR_RESID + 0, "Disk error", purgeable ) {
  			" because of a disk error."
  };
  resource 'STR '	( BOOKMARKS_RESID + 0, "Bookmarks", purgeable ) {
  	"Bookmarks"
  };
  resource 'STR '	( NOT_VISITED_RESID + 0, "Not visited", purgeable ) {
  	"Not visited"
  };
  resource 'STR '	( NO_FORM2HOTLIST_RESID + 0, "No add form to hotlist", purgeable ) {
  	"Cannot add the result of a form submission to the hotlist"
  };
  resource 'STR '	( NEW_ITEM_RESID + 0, "New item", purgeable ) {
  	"New Item"
  };
  resource 'STR '	( NEW_HEADER_RESID + 0, "New Folder", purgeable ) {
  	"New Folder"
  };
  resource 'STR '	( CONFIRM_RM_HDR_RESID + 0, "Confirm Remove Folder", purgeable ) {
  	"Are you sure that you want to remove the folder \""
  };
  resource 'STR '	( AND_ITEMS_RESID + 0, "Add items", purgeable ) {
  	"\" and its items?"
  };
  resource 'STR '	( SAVE_BKMKS_AS_RESID + 0, "Save bookmark as", purgeable ) {
  	"Save bookmarks as:"
  };
  resource 'STR '	( END_LIST_RESID + 0, "End of list", purgeable ) {
  	"End of List"
  };
  resource 'STR '	( ENTIRE_LIST_RESID + 0, "Entire list", purgeable ) {
  	"Entire List"
  };
  resource 'STR '	( NEW_RESID + 0, "new", purgeable ) {
  	"new"
  };
  resource 'STR '	( OTHER_RESID + 0, "Other", purgeable ) {
  	"Other"
  };
  resource 'STR '	( MEM_AVAIL_RESID + 0, "Memory available", purgeable ) {
  	"M available"
  };
  resource 'STR '	( SAVE_RESID + 0, "Save", purgeable ) {
  	"Save to disk"
  };
  resource 'STR '	( LAUNCH_RESID + 0, "Launch", purgeable ) {
  	"Launch"
  };
  resource 'STR '	( INTERNAL_RESID + 0, "Internal", purgeable ) {
  	"Netscape (internal)"
  };
  resource 'STR '	( UNKNOWN_RESID + 0, "", purgeable ) {
  	"Unknown: Prompt user"
  };
  resource 'STR '	( MEGA_RESID + 0, "", purgeable ) {
  	"M"
  };
  resource 'STR '	( KILO_RESID + 0, "", purgeable ) {
  	"K"
  };
  
  resource 'STR '	( PICK_COLOR_RESID + 0, "Pick color", purgeable ) {
  	"Pick a color"
  };
  resource 'STR '	( BAD_APP_LOCATION_RESID + 0, "Bad app location", purgeable ) {
  	"Finder desktop database reported improper location of the application \"",
  };
  resource 'STR '	( REBUILD_DESKTOP_RESID + 0, "Rebuild desktop", purgeable ) {
  	"\" to Netscape. Next time you start up, please rebuild your desktop."
  };
  resource 'STR '	( UNTITLED_RESID + 0, "Untitled", purgeable ) {
  	"(untitled)"
  };
  resource 'STR '	( REG_EVENT_ERR_RESID + 0, "Reg. Event Handling Err", purgeable ) {
  	"Error in handling RegisterEvent"
  };
  resource 'STR '	( APP_NOT_REG_RESID + 0, "App not registered", purgeable ) {
  	"This app was not registered"
  };
  resource 'STR '	( UNREG_EVENT_ERR_RESID + 0, "Unreg. Event Handling Err", purgeable ) {
  	"Error in handling UnregisterEvent"
  };
  resource 'STR '	( BOOKMARK_HTML_RESID + 0, "Bookmarks HTML", purgeable ) {
  	"MCOM-bookmarks.html"
  };
  resource 'STR '	( NO_DISKCACHE_DIR_RESID + 0, "No disk cache folder", purgeable ) {
  	"Preset disk cache folder could not be found.\r Default folder in Preferences will be used."
  };
  resource 'STR '	( NO_SIGFILE_RESID + 0, "No signature file", purgeable ) {
  	"The signature file specified in your preferences could not be found."
  };
  resource 'STR '	( NO_BACKDROP_RESID + 0, "No backdrop for prefs", purgeable ) {
  	"The backdrop file specified in your preferences could not be found."
  };
  resource 'STR '	( SELECT_RESID + 0, "Select", purgeable ) {
  	"Select "
  };
  resource 'STR '	( AE_ERR_RESID + 0, "AE error reply", purgeable ) {
  	"Netscape received AppleEvent error reply: "
  };
  resource 'STR ' ( CHARSET_RESID, "Message Charset", purgeable ) {
  	"x-mac-roman"
  };
  resource 'STR ' ( BROWSE_RESID, "Browse", purgeable ) {
  	"BrowseÉ"
  };
  
  resource 'STR ' ( ENCODING_CAPTION_RESID, "", purgeable ) {
  	"Encoding For "
  };

  resource 'STR ' ( NO_TWO_NETSCAPES_RESID, "", purgeable ) {
  	"You cannot run two versions of Netscape at once. "
	"This copy will quit now."
  };

resource 'STR ' ( PG_NUM_FORM_RESID, "", purgeable ) {
	"Page: %d"
};

resource 'STR ' ( REPLY_FORM_RESID, "", purgeable ) {
	"Re: "
};

resource 'STR ' ( MENU_SEND_NOW, "", purgeable ) {
	"Send Mail Now"
};

resource 'STR ' ( QUERY_SEND_OUTBOX, "", purgeable ) {
	"Your Outbox folder contains %d unsent messages. Send them now ?"
};

resource 'STR ' ( QUERY_SEND_OUTBOX_SINGLE, "", purgeable ) {
	"Your Outbox folder contains an unsent message. Send it now ?"
};

resource 'STR ' ( MENU_SEND_LATER, "", purgeable ) {
	"Send Mail Later"
};

resource 'STR ' ( MENU_SAVE_AS, "", purgeable ) {
	"Save asÉ"
};
resource 'STR ' ( MENU_SAVE_FRAME_AS, "", purgeable ) {
	"Save Frame asÉ"
};
resource 'STR ' ( MENU_PRINT, "", purgeable ) {
	"PrintÉ"
};
resource 'STR ' ( MENU_PRINT_FRAME, "", purgeable ) {
	"Print FrameÉ"
};
resource 'STR ' ( MENU_RELOAD, "", purgeable ) {
	"Reload"
};
resource 'STR ' ( MENU_SUPER_RELOAD, "", purgeable ) {
	"Super Reload"
};
resource 'STR ' ( MAC_PROGRESS_NET, "", purgeable ) {
 "Initializing networkÉ"
};
resource 'STR ' ( MAC_PROGRESS_PREFS, "", purgeable ) {
	"Reading PreferencesÉ"
};
resource 'STR ' ( MAC_PROGRESS_BOOKMARK, "", purgeable ) {
	"Reading BookmarksÉ"
};
resource 'STR ' ( MAC_PROGRESS_ADDRESS, "", purgeable ) {
	"Reading Address BookÉ"
};
resource 'STR ' ( MAC_PROGRESS_JAVAINIT, "", purgeable ) {
	"Initializing JavaÉ"
};
resource 'STR ' ( MAC_UPLOAD_TO_FTP, "MAC_UPLOAD_TO_FTP", purgeable ) {
	"Are you sure that  want to upload the dragged files to the FTP server?"
};	/*	JJE	*/
resource 'STR ' ( POP_USERNAME_ONLY, "POP_USERNAME_ONLY", purgeable ) {
	"Your POP User Name is just your user name (e.g. user), not your full POP address (e.g. user@internet.com)."
};	/*	JJE	*/
resource 'STR ' ( MAC_PLUGIN, "MAC_PLUGIN", purgeable ) {
	"Plugin:"
};	/*	JJE	*/
resource 'STR ' ( MAC_NO_PLUGIN, "MAC_NO_PLUGIN", purgeable ) {
	"No plugins"
};	/*	JJE	*/
resource 'STR ' ( MAC_REGISTER_PLUGINS, "MAC_REGISTER_PLUGINS", purgeable ) {
	"Registering plugins"
};	/*	JJE	*/
resource 'STR ' ( DOWNLD_CONT_IN_NEW_WIND, "DOWNLD_CONT_IN_NEW_WIND", purgeable ) {
	"Download continued in a new window."
};	/*	JJE	*/
resource 'STR ' ( ATTACH_NEWS_MESSAGE, "ATTACH_NEWS_MESSAGE", purgeable ) {
	"News message"
};	/*	JJE	*/
resource 'STR ' ( ATTACH_MAIL_MESSAGE, "ATTACH_MAIL_MESSAGE", purgeable ) {
	"Mail message"
};	/*	JJE	*/
resource 'STR ' ( MBOOK_NEW_ENTRY, "MBOOK_NEW_ENTRY", purgeable ) {
	"New entry"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_BACK_IN_FRAME, "MCLICK_BACK_IN_FRAME", purgeable ) {
	"Back in Frame"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_BACK, "MCLICK_BACK", purgeable ) {
	"Back"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_FWRD_IN_FRAME, "MCLICK_FWRD_IN_FRAME", purgeable ) {
	"Forward in Frame"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_FORWARD, "MCLICK_FORWARD", purgeable ) {
	"Forward"
};	/*	JJE	*/
resource 'STR ' ( MCLICK_SAVE_IMG_AS, "MCLICK_SAVE_IMG_AS", purgeable ) {
	"Save Image as:"
};	/*	JJE	*/
resource 'STR ' ( SUBMIT_FILE_WITH_DATA_FK, "SUBMIT_FILE_WITH_DATA_FK", purgeable ) {
	"You can only submit a file that has a data fork."
};	/*	JJE	*/
resource 'STR ' ( ABORT_CURR_DOWNLOAD, "ABORT_CURR_DOWNLOAD", purgeable ) {
	"Are you sure that you want to abort the current download?"
};	/*	JJE	*/
resource 'STR ' ( MACDLG_SAVE_AS, "MACDLG_SAVE_AS", purgeable ) {
	"Save as: "
};	/*	JJE	*/
resource 'STR ' ( THE_ERROR_WAS, "THE_ERROR_WAS", purgeable ) {
	"The error was:"
};	/*	JJE	*/
resource 'STR ' ( MBOOK_NEW_BOOKMARK, "MBOOK_NEW_BOOKMARK", purgeable ) {
	"New Bookmark"
};	/*	JJE	*/

resource 'STR ' ( MENU_MAIL_DOCUMENT, "", purgeable ) {
	"Mail DocumentÉ"
};	

resource 'STR ' ( MENU_MAIL_FRAME, "", purgeable ) {
	"Mail FrameÉ"
};	

resource 'STR ' ( DEFAULT_PLUGIN_URL, "", purgeable ) {
	"http://cgi.netscape.com/eng/mozilla/2.0/extensions/info.cgi"
};	

/* Bookmark Seperate string, For Japanese, please change to something like ---- */
resource 'STR ' ( MBOOK_SEPARATOR_STR, "MBOOK_SEPARATOR_STR", purgeable ) {
	"ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ"
};	

resource 'STR ' ( RECIPIENT, "", purgeable ) {
	"Recipient"
};

resource 'STR ' ( DELETE_MIMETYPE, "", purgeable ) {
	"Are you sure you want to delete this type? Netscape will not be able to display information of this type if you proceed."
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


/*-----------------------------------------------------------------------------
	Common Stuff
-----------------------------------------------------------------------------*/
/*
#define Script		smRoman=0, smJapanese, smTradChinese, smKorean,		\
					smArabic, smHebrew, smGreek, smCyrillic, smRSymbol,	\
					smDevanagari, smGurmukhi, smGujarati, smOriya,		\
					smBengali, smTamil, smTelugu, smKannada,			\
					smMalayalam, smSinhalese, smBurmese, smKhmer,		\
					smThai, smLaotian, smGeorgian, smArmenian,			\
					smSimpChinese, smTibetan, smMongolian, smGeez,		\
					smSlavic, smVietnamese, smExtArabic, smUninterp		\
*/
/*	 Language Group			Proportional			Fixed 		ProportionalFixed	Character					ScriptID for 	 Txtr		Txtr			*/
/*	 Name		   			Font Name	   		 	Font Name	    Size	Size	Set ID						Font Fallback 	 ButtonID	TextFieldID		*/

resource 'Fnec' (FNEC_RESID, "Font definitation for every encoding", purgeable) {
	{
	"Western",				"Times",				"Courier",		12,		10,		CS_MAC_ROMAN,				smRoman,	     4002,		4001;	
	"Japanese",				"×–¾’©‘Ì",	 			"Osaka|“™•",	12,		12,		CS_SJIS,					smJapanese,		 4004,		4003;
	"Central European",		"Times CE",				"Courier CE", 	12,		10,		CS_MAC_CE,					smSlavic, 		 4006,		4005;
	"Traditional Chinese",	"Apple LiSung Light",	"Taipei", 		16,		12,		CS_BIG5,					smTradChinese,	 4008,		4007;
	"Simplified Chinese",	"Song",					"*Beijing*",	16,		12,		CS_GB_8BIT,					smSimpChinese,	 4010,		4009;	
	"Korean",				"AppleGothic",			"Seoul",		12,		12,		CS_KSC_8BIT,				smKorean,		 4012,		4011;
	"Cyrillic",				"ğßìîé ğîï",			"ğßìîé",		12,		12,		CS_MAC_CYRILLIC,			smCyrillic,		 4016,		4015;
	"Greek",				"GrTimes",				"GrCourier",	12,		12,		CS_MAC_GREEK,				smRoman,		 4018,		4017;
	"Turkish",				"Times",				"Courier",		12,		10,		CS_MAC_TURKISH,				smRoman,	     4020,		4019;	
	"User-Defined",			"Times",				"Courier", 		12,		10,		CS_USER_DEFINED_ENCODING,	smRoman,		 4014,		4013;		
	}
};


resource 'Csid' (cmd2csid_tbl_ResID, "csIDs", purgeable) {
	{
  /* win_csid         doc_csid            cmdNumber */
    CS_MAC_ROMAN,     CS_ASCII,            cmd_ASCII,
    CS_MAC_ROMAN,     CS_LATIN1,             cmd_LATIN1,
    CS_SJIS,    	  CS_JIS,              cmd_JIS,
    CS_SJIS, 	      CS_SJIS,             cmd_SJIS,
    CS_SJIS,     	  CS_EUCJP,            cmd_EUCJP,
    CS_SJIS,     	  CS_SJIS_AUTO,          cmd_SJIS_AUTO,
    CS_MAC_ROMAN,     CS_MAC_ROMAN,          cmd_MAC_ROMAN,
    CS_MAC_CE,        CS_LATIN2,             cmd_LATIN2,
    CS_MAC_CE,        CS_MAC_CE,             cmd_MAC_CE,
    CS_MAC_CE,        CS_CP_1250,             cmd_CP1250,
    CS_BIG5,        CS_BIG5,             cmd_BIG5,
    CS_BIG5,        CS_CNS_8BIT,           cmd_CNS_8BIT,
    CS_GB_8BIT,       CS_GB_8BIT,            cmd_GB_8BIT,
    CS_KSC_8BIT,      CS_KSC_8BIT,           cmd_KSC_8BIT,
    CS_KSC_8BIT,      CS_2022_KR,            cmd_2022_KR,
    
    CS_MAC_CYRILLIC,  CS_MAC_CYRILLIC,       cmd_MAC_CYRILLIC,
    CS_MAC_CYRILLIC,  CS_8859_5,             cmd_8859_5,
    CS_MAC_CYRILLIC,  CS_CP_1251,            cmd_CP1251,
    CS_MAC_CYRILLIC,  CS_KOI8_R,             cmd_KOI8R,
    CS_MAC_GREEK,     CS_MAC_GREEK,          cmd_MAC_GREEK,
    CS_MAC_GREEK,     CS_8859_7,             cmd_8859_7,
    CS_MAC_TURKISH,   CS_MAC_TURKISH,        cmd_MAC_TURKISH,
    CS_MAC_TURKISH,   CS_8859_9,             cmd_8859_9,
    CS_USER_DEFINED_ENCODING,CS_USER_DEFINED_ENCODING, cmd_USER_DEFINED_ENCODING

	}
};


resource 'MIME' ( MIME_PREFS_FIRST_RESID, "image/gif", purgeable ) {
	'JVWR',
	'GIFf',
	2,				
	"JPEGView",
	"image/gif",
	"gif"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 1, "image/jpeg", purgeable ) {
	'JVWR',
	'JPEG',
	2,				/* User action - Save = 0, Launch = 1, Internal = 2, Unknown = 3 */
	"JPEGView",
	"image/jpeg",
	"jpg,jpeg,jpe"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 2, "image/pict", purgeable ) {
	'ttxt',
	'PICT',
	1,				/* User action - Save = 0, Launch = 1, Internal = 2, Unknown = 3 */
	"TeachText",
	"image/pict",
	"pic,pict"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 3, "image/tiff", purgeable ) {
	'JVWR',
	'TIFF',
	1,				/* User action - Save = 0, Launch = 1, Internal = 2, Unknown = 3 */
	"JPEGView",
	"image/tiff",
	"tif,tiff"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 4, "image/x-xbitmap", purgeable ) {
	'????',
	'ttxt', 
	2,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-xbitmap",
	"xbm"
};


resource 'MIME' ( MIME_PREFS_FIRST_RESID + 5, "audio/basic", purgeable ) {
	'SNDM',
	'ULAW',
	1,				/* User action - Save = 0, Launch = 1, Internal = 2, Unknown = 3 */
	"Sound Machine",
	"audio/basic",
	"au,snd"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 6, "audio/aiff", purgeable ) {
	'SNDM',
	'AIFF',
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Sound Machine",
	"audio/aiff",
	"aiff,aif"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 7, "audio/x-wav", purgeable ) {
	'SCPL',
	'WAVE', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"SoundApp", 
	"audio/x-wav",
	"wav"
};


resource 'MIME' ( MIME_PREFS_FIRST_RESID + 8, "video/quicktime", purgeable ) {
	'TVOD',
	'MooV',
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Simple Player",
	"video/quicktime",
	"qt,mov"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 9, "video/mpeg", purgeable ) {
	'mMPG',
	'MPEG',
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Sparkle",
	"video/mpeg",
	"mpg,mpeg,mpe"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 10, "video/x-msvideo", purgeable ) {
	'AVIC',
	'????',
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"AVI to QT Utility",
	"video/x-msvideo",
	"avi"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 11, "video/x-qtc", purgeable ) {
	'mtwh',
	'.qtc',
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Conferencing Helper Application",
	"video/x-qtc",
	"qtc"
};


resource 'MIME' ( MIME_PREFS_FIRST_RESID + 12, "application/mac-binhex40", purgeable ) {
	'SITx',
	'TEXT', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Stuffit Expander", 
	"application/mac-binhex40",
	"hqx"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 13, "application/x-stuffit", purgeable ) {
	'SITx',
	'SITD', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Stuffit Expander", 
	"application/x-stuffit",
	"sit"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 14, "application/x-macbinary", purgeable ) {
	'MB2P',
	'TEXT', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Mac BinaryII+", 
	"application/x-macbinary",
	"bin"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 15, "application/zip", purgeable ) {
	'ZIP ',
	'ZIP ', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"ZipIt", 
	"application/x-zip",
	"z,zip"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 16, "application/gzip", purgeable ) {
	'Gzip',
	'Gzip', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"MacGzip", 
	"application/x-gzip",
	"gz"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 17, "application/msword", purgeable ) {
	'MSWD',
	'WDBN', 
	0,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"MS Word", 
	"application/msword",
	"doc"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + 18, "application/x-excel", purgeable ) {
	'XCEL',
	'XLS5', 
	0,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"MS Excel", 
	"application/x-excel",
	"xls"
};


/* Crappy mimes are those mime types that will never ever be seen by the user */
#define CRAPPY_MIME 19

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME, "application/octet-stream", purgeable ) {
	'ttxt',
	'TEXT', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"TeachText", 
	"application/octet-stream",
	"exe"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+1, "application/postscript", purgeable ) {
	'ttxt',
	'TEXT', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"TeachText", 
	"application/postscript",
	"ai,eps,ps"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+2, "application/rtf", purgeable ) {
	'MSWD',
	'TEXT', 
	0,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"MS Word", 
	"application/rtf",
	"rtf"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+3, "application/x-compressed", purgeable ) {
	'LZIV',
	'ZIVM', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"MacCompress", 
	"application/x-compress",
	".Z"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+4, "application/x-tar", purgeable ) {
	'TAR ',
	'TARF', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"tar", 
	"application/x-tar",
	"tar"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+5, "image/x-cmu-raster", purgeable ) {
	'????',
	'ttxt', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-cmu-raster",
	"ras"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+6, "image/x-portable-anymap", purgeable ) {
	'????',
	'ttxt', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-portable-anymap",
	"pnm"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+7, "image/x-portable-anymap", purgeable ) {
	'????',
	'ttxt', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-portable-bitmap",
	"pbm"
};


resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+8, "image/x-portable-bitmap", purgeable ) {
	'????',
	'ttxt', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-portable-graymap",
	"pgm"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+9, "image/x-portable-graymap", purgeable ) {
	'????',
	'ttxt', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-portable-graymap",
	"pgm"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+10, "image/x-rgb", purgeable ) {
	'????',
	'ttxt', 
	3,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-rgb",
	"rgb"
};

resource 'MIME' ( MIME_PREFS_FIRST_RESID + CRAPPY_MIME+11, "image/x-xpixmap", purgeable ) {
	'????',
	'ttxt', 
	2,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Unknown", 
	"image/x-xpixmap",
	"xpm"
};


resource 'SMIM' ( 1, "Netscape/Source", purgeable ) {
	'ttxt',
	'TEXT', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"Teach Text", 
	PREF_SOURCE,
	""
};

resource 'SMIM' ( 2, "Netscape/Telnet", purgeable ) {
	'NCSA',
	'CONF', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"NCSA Telnet", 
	TELNET_APPLICATION_MIME_TYPE,
	"",
};

resource 'SMIM' ( 3, "Netscape/tn3270", purgeable ) {
	'GFTM',
	'GFTS', 
	1,				/* User action - Save = 0, Launch = 1, Internal =2, Unknown=3 */
	"tn3270", 
	TN3270_VIEWER_APPLICATION_MIME_TYPE,
	""
};

resource 'STR ' ( mPREFS_UNSPECIFIED_ERR_ALERT, "Unspecified Error", purgeable ) {
	"There was an error.  Preferences will not be saved.";
};

resource 'STR ' ( mPREFS_CANNOT_OPEN_SECOND_ALERT, "Cant Open Second Prefs File", purgeable ) {
	"You cannot open a second preferences file.";
};

resource 'STR ' ( mPREFS_CANNOT_OPEN_SECOND_ALERT + 1, "-", purgeable ) {
	"Quit Netscape and launch by double-clicking on the preferences file you wish to use.";
};

resource 'STR ' ( mPREFS_CANNOT_CREATE, "Cant Create Prefs File", purgeable ) {
	"The preferences file cannot be created.  Netscape will use the defaults.";
};

resource 'STR ' ( mPREFS_CANNOT_CREATE_PREFS_FOLDER, "Cant Create Prefs Folder", purgeable ) {
	"The preferences folder could not be created.  Netscape will use the defaults.";
};

resource 'STR ' ( mPREFS_CANNOT_READ, "Cant Read Preferences", purgeable ) {
	"The preferences file cannot be read.  Netscape will use the defaults.";
};

resource 'STR ' ( mPREFS_CANNOT_WRITE, "Cant Write Preferences", purgeable ) {
	"There was an error writing the preferences.  They will not be saved.";
};



// TRANSLATION RESOURCES
/* les tables pour ISO 8859-1 : A. Pirard (ULg) - mai 1992 */

data 'xlat' (128, "ISO1 -> MACR", purgeable) {
/*		translation ISO 8859-1 -> Macintosh			*/
/*		  x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF	*/
/*8x*/	$"A5AA E2C4 E3C9 A0E0 F6E4 C9DC CED9 DAB6"    /* ¥ª­°³·º½ÃÅÉÑÔÙÚ¶ */
/*9x*/	$"C6D4 D5D2 D3A5 D0D1 F7AA FBDD CFFF F5D9"    /* ÆÎâãäğö÷ùúûışÿõÄ */
/*Ax*/	$"CAC1 A2A3 DBB4 7CA4 ACA9 BBC7 C2D0 A8F8"    /* ÊÁ¢£Û´Ï¤¬©»ÇÂĞ¨ø */
/*Bx*/	$"A1B1 3233 ABB5 A6E1 FC31 BCC8 B9B8 B2C0"    /* ¡±ÓÒ«µ¦áüÕ¼È¹¸²À */
/*Cx*/	$"CBE7 E5CC 8081 AE82 E983 E6E8 EDEA EBEC"    /* ËçåÌ€®‚éƒæèíêëì */
/*Dx*/	$"DC84 F1EE EFCD 8578 AFF4 F2F3 86A0 DEA7"    /* Ü„ñîïÍ…×¯ôòó† Ş§ */
/*Ex*/	$"8887 898B 8A8C BE8D 8F8E 9091 9392 9495"    /* ˆ‡‰‹ŠŒ¾‘“’”• */
/*Fx*/	$"DD96 9897 999B 9AD6 BF9D 9C9E 9FE0 DFD8"    /* İ–˜—™›šÖ¿œŸàßØ */
};

data 'xlat' (129, "MACR -> ISO1", purgeable) {
/*		translation Macintosh -> ISO 8859-1			*/
/*		  x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF	*/
/*8x*/	$"C4C5 C7C9 D1D6 DCE1 E0E2 E4E3 E5E7 E9E8"    /* €‚ƒ„…†‡ˆ‰Š‹Œ */
/*9x*/	$"EAEB EDEC EEEF F1F3 F2F4 F6F5 FAF9 FBFC"    /* ‘’“”•–—˜™š›œŸ */
/*Ax*/	$"DDB0 A2A3 A780 B6DF AEA9 99B4 A882 C6D8"    /*  ¡¢£¤¥¦§¨©ª«¬­®¯ */
/*Bx*/	$"83B1 BE84 A5B5 8F85 BDBC 86AA BA87 E6F8"    /* °±²³´µ¶·¸¹º»¼½¾¿ */
/*Cx*/	$"BFA1 AC88 9F89 90AB BB8A A0C0 C3D5 91A6"    /* ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ */
/*Dx*/	$"AD8B B3B2 8CB9 F7D7 FF8D 8EA4 D0F0 DEFE"    /* ĞÑÒÓÔÕÖ×ØÙÚÛÜİŞß */
/*Ex*/	$"FDB7 9293 94C2 CAC1 CBC8 CDCE CFCC D3D4"    /* àáâãäåæçèéêëìíîï */
/*Fx*/	$"95D2 DADB D99E 9697 AF98 999A B89B 9C9D"    /* ğñòóôõö÷øùúûüışÿ */
};


data 'xlat' (130, "ISO2 -> MACCE", purgeable) {
/* translation 8859-2 -> MacCEuro */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF	*/
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"    /* ................ */
/*9x*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"    /* ................ */
/*Ax*/  $"CA84 FFFC A4BB E5A4 ACE1 53E8 8F2D EBFB"    /*  ¡.£.¥¦§¨©.«¬.®¯ */
/*Bx*/  $"A188 B2B8 27BC E6FF B8E4 73E9 90BD ECFD"    /* °±.³.µ¶·.¹.»¼.¾¿ */
/*Cx*/  $"D9E7 4141 80BD 8C43 8983 A245 9DEA 4991"    /* ÀÁ..ÄÅÆ.ÈÉÊ.ÌÍ.Ï */
/*Dx*/  $"44C1 C5EE EFCC 8578 DBF1 F2F4 86F8 54A7"    /* .ÑÒÓÔÕÖ.ØÙÚÛÜİ.ß */
/*Ex*/  $"DA87 6161 8ABE 8D63 8B8E AB65 9E92 6993"    /* àá..äåæ.èéê.ìí.ï */
/*Fx*/  $"64C4 CB97 99CE 9AD6 DEF3 9CF5 9FF9 74A1"    /* .ñòóôõö÷øùúûüı.. */
};

data 'xlat' (131, "MACCE -> ISO2", purgeable) {
/* translation MacCEuro -> 8859-2 */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF	*/
/*8x*/  $"C441 61C9 A1D6 DCE1 B1C8 E4E8 C6E6 E9AC"    /* €..ƒ„…†‡ˆ‰Š‹Œ */
/*9x*/  $"BCCF EDEF 4565 45F3 65F4 F66F FACC ECFC"    /* ‘’“...—.™š.œŸ */
/*Ax*/  $"A0B0 CA4C A72A A6DF 5263 54EA A8AD 6749"    /* .¡¢.¤..§...«¬... */
/*Bx*/  $"6949 B2B3 694B 6445 B34C 6CA5 B5C5 E54E"    /* ........¸..»¼½¾. */
/*Cx*/  $"6ED1 2DC3 F1D2 C63D 3FC9 A0F2 D54F F54F"    /* .Á..ÄÅ....ÊËÌ.Î. */
/*Dx*/  $"2D2D 2222 2727 F7D7 6FC0 E0D8 3D3F F852"    /* ......Ö..ÙÚÛ..Ş. */
/*Ex*/  $"72A9 2C2C B9A6 B6C1 ABBB CDAE BE55 D3D4"    /* .á..äåæçèéêëì.îï */
/*Fx*/  $"75D9 DAF9 DBFB 5575 DDFD 6BAF A3BF 47B7"    /* .ñòóôõ..øù.ûüı.ÿ */
};


read 'Tang' ( ABOUT_BIGLOGO_TANG, "biglogo.gif", purgeable) "biglogo.gif";	// Was splash.gif
read 'Tang' ( ABOUT_JAVALOGO_TANG, "javalogo.gif", purgeable) "javalogo.gif";
read 'Tang' ( ABOUT_RSALOGO_TANG, "rsalogo.gif", purgeable ) "rsalogo.gif";
read 'Tang' ( ABOUT_HYPE_TANG, "hype.au", purgeable ) "hype.au";
read 'Tang' ( ABOUT_QTLOGO_TANG, "qtlogo.gif", purgeable) "qt_logo.gif";

read 'TEXT' ( ABOUT_ABOUTPAGE_TEXT, "about-all.html", purgeable ) "about-all.html";
read 'TEXT' ( ABOUT_AUTHORS_TEXT, "authors.html", purgeable ) "authors.html";
read 'TEXT' ( ABOUT_MAIL_TEXT, "mail.msg", purgeable ) "mail.msg";
read 'TEXT' ( ABOUT_MOZILLA_TEXT, "mozilla.html", purgeable ) "mozilla.html";
read 'TEXT' ( ABOUT_PLUGINS_TEXT, "aboutplg.html", purgeable ) "aboutplg.html";


