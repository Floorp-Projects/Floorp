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
#include "PowerPlant.r"
#include "Types.r"
#undef ALPHA
#undef BETA
#include "SysTypes.r"

#include "resae.h"							// Apple event constants
#include "resgui.h"							// main window constants
#include "mversion.h"						// version numbers, application name, etc
#include "csid.h"

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
	"Can’t Undo"
}};


// • Other stuff

// -- These are the FIXED items in the Help menu.  Items that 
// may be configured by the Admin Kit are defined in config.js
resource 'STR#' ( HELP_URLS_MENU_STRINGS, "Help URL Menu Entries", purgeable ) {{
	"-",
#ifndef MOZ_COMMUNICATOR_ABOUT
	"About Navigator",
#else
	"About Communicator",
#endif
}};

resource 'STR#' ( HELP_URLS_RESID, "Help URLs", purgeable ) {{
	"", // separator
	ABOUT_LOCATION,
}};


resource 'STR#' ( STOP_STRING_LIST, "Stop Strings", purgeable ) {{
	"Stop Loading",
	"Stop Animations",
}};

resource 'STR#' ( WINDOW_TITLES_RESID, "" ) {{
	"Mail",
	"Newsgroups",
	"Composer",
	"Address Book for ^0",		// filled in with profile name
	"Bookmarks for ^0",
	"Preferences for ^0",
}};

// • Note: NEW_DOCUMENT_URL_RESID and DEFAULT_PLUGIN_URL moved to config.js

resource 'STR#' ( HELPFILES_NAMES_RESID, "" ) {{
	"Navigator Gold Help",					// help file name
	"Navigator Gold Help Media",								// media file name
}};

// Generic font family names
resource 'STR#' ( GENERIC_FONT_NAMES_RESID, "Generic Font Names", purgeable ) {{
	"Times",				// serif
	"Helvetica",			// san-serif
	"Zapf Chancery",		// cursive
	"New Century Schlbk",	// fantasy (??)
	"Courier",				// monospace
}};

// • news group column headers (i18n)
resource 'STR#' ( 4010, "" ) {{
	"Groups Server",
	"#3080",						// subscribed icon
	"Unread",
	"Total",
}};

// • news and mail article column headers (i18n)
resource 'STR#' ( 4011, "" ) {{
	"Subject",
	"#3082",						// flag icon
	"#3079",						// read icon
	"Sender",
	"Date",
	"Recipient",					
}};

// • mail folder column headers (i18n)
resource 'STR#' ( 6010, "" ) {{
	"Folder",
	"",
	"Unread",
	"Total",
}};

// Miscellaneous JRM-perpetrated strings
resource 'STR#' ( 7099, "" ) {{
	// Terms for the first parameter of the thread window name (Mozilla %s %s)
	"Folder"
,	"Newsgroup"
	 // Notification alert
,	PROGRAM_NAME" requires your attention at this time."
,	"%2.1fMB" // format string for Megabytes
,	"%ldK" // format string for Kilobytes
,	"%ld"	// format string for bytes
	// setup not done when trying to create a mail window
,	"The command could not be completed, because you have not set up your ^0 preferences."
	" Would you like to set up those preferences now?"
,	"Message counts unavailable"
	// Used by FE_ShowPropertySheetFor() when backend wants to add new entry
,	"Create a new card for ^0 in personal address book?" 
,	"<unknown person>"
,	"Message Center for ^0"
,	"^0 ^1"	// Given name, family name.
,	"Not encrypted"
,	"Encrypted"
,	"Moving Messages" // 15
,	"Loading Folder" // 16
,	"Open Selected Items"
,	"Preparing window ^0 of ^1…" // 18
,	"Copying Messages" //19
,	"Bookmark Added"	// 20, shown in status bar when dbl-click page proxy.
,	"Mail Server"		// 21, shown in string #7 above
,	"Groups Server"		// 22	''		''		''
,	"Identity"			// 23	''		''		''
,	"Mail & Groups"		// 24	''		''		''
,	"You have changed the mail server type. You must quit and restart Communicator "
	"for this to take effect."		// 25
,	"Directories"		// 26 shown in Offline Picker
}};

resource 'STR#' ( 7098, "" ) {{
// Command could not be completed because ^0.
	"there was not enough memory" // 1
,	"of an unknown error" // 2
,	"the disk is full" // 3
,	"a file could not be found" // 4
,	"there is no such folder" // 5
,	"there is no disk by that name" // 6
,	"a file name was too long or contained bad characters" // 7
,	"the disk is locked" // 8
,	"a file was busy and could not be written to" // 9
,	"a file by that name already exists" // 10
,	"a file was already open for writing" // 11
,	"you do not have the correct permission" // 12
}};

// • Multi-user profile dialog strings
resource 'STR#' ( 900, "", purgeable ) {{
	"Enter a new name for the profile:",
	"Do you want to delete this profile?  No files will be removed from your disk.",
	"Error reading user profiles: ",
	"Error creating profile: ",
	"Default Profile",
	"The folder for profile “^0” could not be found.  Do you want to locate it?",
	"Quit",
	"Done",
	"Next >",
	"The required configuration file “" CONFIG_FILENAME "” could not be found in " ESSENTIALS_FOLDER ".  Please reinstall the software or contact your administrator.",
	"The configuration file “" CONFIG_FILENAME "” is invalid.  Please reinstall the software or contact your administrator.",
	"Account Setup",
	"Create Profile",
}};

// • Boolean operators and search menu items
resource 'STR#' ( 901, "", purgeable ) {{
	"and", 			// 1
	"or",			// 2
	"Customize…"	// 3
}};

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
	"Central European",		"Times CE",				"Courier CE", 	12,		10,		CS_MAC_CE,					smSlavic, 		 4006,		4005;
	"Japanese",				"ç◊ñæí©ëÃ",	 			"OsakaÅ|ìôïù",	12,		12,		CS_SJIS,					smJapanese,		 4004,		4003;
	"Traditional Chinese",	"Apple LiSung Light",	"Taipei", 		16,		12,		CS_BIG5,					smTradChinese,	 4008,		4007;
	"Simplified Chinese",	"Song",					"*Beijing*",	16,		12,		CS_GB_8BIT,					smSimpChinese,	 4010,		4009;	
	"Korean",				"AppleGothic",			"Seoul",		12,		12,		CS_KSC_8BIT,				smKorean,		 4012,		4011;
	"Cyrillic",				"èﬂÏÓÈ èÓÔ",			"èﬂÏÓÈ",		12,		12,		CS_MAC_CYRILLIC,			smCyrillic,		 4016,		4015;
	"Greek",				"GrTimes",				"GrCourier",	12,		12,		CS_MAC_GREEK,				smRoman,		 4018,		4017;
	"Turkish",				"Times",				"Courier",		12,		10,		CS_MAC_TURKISH,				smRoman,	     4020,		4019;	
	"Unicode",				"Times",				"Courier",		12,		10,		CS_UTF8,					smRoman,	     4024,		4023;		
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
    CS_KSC_8BIT,      CS_KSC_8BIT_AUTO,      cmd_KSC_8BIT,
    CS_KSC_8BIT,      CS_2022_KR,            cmd_2022_KR,
    
    CS_MAC_CYRILLIC,  CS_MAC_CYRILLIC,       cmd_MAC_CYRILLIC,
    CS_MAC_CYRILLIC,  CS_8859_5,             cmd_8859_5,
    CS_MAC_CYRILLIC,  CS_CP_1251,            cmd_CP1251,
    CS_KOI8_R,  	  CS_KOI8_R,             cmd_KOI8R,
    CS_MAC_GREEK,     CS_MAC_GREEK,          cmd_MAC_GREEK,
    CS_MAC_GREEK,     CS_8859_7,             cmd_8859_7,
    CS_MAC_GREEK,     CS_CP_1253,            cmd_CP1253,
    CS_MAC_TURKISH,   CS_MAC_TURKISH,        cmd_MAC_TURKISH,
    CS_MAC_TURKISH,   CS_8859_9,             cmd_8859_9,
    CS_UTF8,  		  CS_UTF8,               cmd_UTF8,
    CS_UTF8,  		  CS_UTF7,               cmd_UTF7,
    CS_USER_DEFINED_ENCODING,CS_USER_DEFINED_ENCODING, cmd_USER_DEFINED_ENCODING

	}
};


/* -- 4.0 xp prefs --
 * Default mime types have been removed from here and
 * moved to xp initializers in libpref/src/init/all.js
 */

resource 'STR ' ( mPREFS_UNSPECIFIED_ERR_ALERT, "Unspecified Error", purgeable ) {
	"There was an error.  Preferences will not be saved.";
};

resource 'STR ' ( mPREFS_CANNOT_OPEN_SECOND_ALERT, "Cant Open Second Prefs File", purgeable ) {
	"You cannot open a second preferences file.";
};

resource 'STR ' ( mPREFS_CANNOT_OPEN_SECOND_ALERT + 1, "-", purgeable ) {
	"Quit "PROGRAM_NAME" and launch by double-clicking on the preferences file you wish to use.";
};

resource 'STR ' ( mPREFS_CANNOT_CREATE, "Cant Create Prefs File", purgeable ) {
	"The preferences file cannot be created.  "PROGRAM_NAME" will use the defaults.";
};

resource 'STR ' ( mPREFS_CANNOT_CREATE_PREFS_FOLDER, "Cant Create Prefs Folder", purgeable ) {
	"The preferences folder could not be created.  "PROGRAM_NAME" will use the defaults.";
};

resource 'STR ' ( mPREFS_CANNOT_READ, "Cant Read Preferences", purgeable ) {
	"The preferences file cannot be read.  "PROGRAM_NAME" will use the defaults.";
};

resource 'STR ' ( mPREFS_CANNOT_WRITE, "Cant Write Preferences", purgeable ) {
	"There was an error writing the preferences.  They will not be saved.";
};

resource 'STR ' ( mPREFS_DUPLICATE_MIME, "Duplicate mimetype", purgeable ) {
	"The specified MIME type already exists.";
};


// TRANSLATION RESOURCES
/* les tables pour ISO 8859-1 : A. Pirard (ULg) - mai 1992 */

data 'xlat' (128, "ISO1 -> MACR", purgeable) {
/*      translation ISO 8859-1 -> Macintosh                     */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF       */
/*8x*/  $"2A2A E2C4 E3C9 A0E0 F6E4 53DC CE2A 2A2A"
/*9x*/  $"2AD4 D5D2 D3A5 D0D1 F7AA 73DD CF2A 2AD9"
/*Ax*/  $"CAC1 A2A3 DBB4 7CA4 ACA9 BBC7 C2D0 A8F8"
/*Bx*/  $"A1B1 3233 ABB5 A6E1 FC31 BCC8 2A2A 2AC0"
/*Cx*/  $"CBE7 E5CC 8081 AE82 E983 E6E8 EDEA EBEC"
/*Dx*/  $"DC84 F1EE EFCD 8578 AFF4 F2F3 86A0 DEA7"
/*Ex*/  $"8887 898B 8A8C BE8D 8F8E 9091 9392 9495"
/*Fx*/  $"DD96 9897 999B 9AD6 BF9D 9C9E 9FE0 DFD8"
};


data 'xlat' (129, "MACR -> ISO1", purgeable) {
/*      translation Macintosh -> ISO 8859-1                     */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF       */
/*8x*/  $"C4C5 C7C9 D1D6 DCE1 E0E2 E4E3 E5E7 E9E8"
/*9x*/  $"EAEB EDEC EEEF F1F3 F2F4 F6F5 FAF9 FBFC"
/*Ax*/  $"DDB0 A2A3 A795 B6DF AEA9 99B4 A82A C6D8"
/*Bx*/  $"2AB1 2A2A A5B5 2A2A 2A2A 2AAA BA2A E6F8"
/*Cx*/  $"BFA1 AC2A 833D 2AAB BB85 A0C0 C3D5 8C9C"
/*Dx*/  $"9697 9394 9192 F72A FF9F 2FA4 D0F0 DEFE"
/*Ex*/  $"FDB7 8284 89C2 CAC1 CBC8 CDCE CFCC D3D4"
/*Fx*/  $"2AD2 DADB D969 8898 AF2A B7B0 B82A 2A2A"
};


data 'xlat' (130, "ISO2 -> MACCE", purgeable) {
/* translation 8859-2 -> MacCEuro */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF	*/
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"    /* ................ */
/*9x*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"    /* ................ */
/*Ax*/  $"CA84 FFFC A4BB E5A4 ACE1 53E8 8F2D EBFB"    /* †°.£.•¶ß®©.´¨.ÆØ */
/*Bx*/  $"A188 B2B8 27BC E6FF B8E4 73E9 90BD ECFD"    /* ∞±.≥.µ∂∑.π.ªº.æø */
/*Cx*/  $"D9E7 4141 80BD 8C43 8983 A245 9DEA 4991"    /* ¿¡..ƒ≈∆.»… .ÃÕ.œ */
/*Dx*/  $"44C1 C5EE EFCC 8578 DBF1 F2F4 86F8 54A7"    /* .—“”‘’÷.ÿŸ⁄€‹›.ﬂ */
/*Ex*/  $"DA87 6161 8ABE 8D63 8B8E AB65 9E92 6993"    /* ‡·..‰ÂÊ.ËÈÍ.ÏÌ.Ô */
/*Fx*/  $"64C4 CB97 99CE 9AD6 DEF3 9CF5 9FF9 74A1"    /* .ÒÚÛÙıˆ˜¯˘˙˚¸˝.. */
};

data 'xlat' (131, "MACCE -> ISO2", purgeable) {
/* translation MacCEuro -> 8859-2 */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF	*/
/*8x*/  $"C441 61C9 A1D6 DCE1 B1C8 E4E8 C6E6 E9AC"    /* Ä..ÉÑÖÜáàâäãåçéè */
/*9x*/  $"BCCF EDEF 4565 45F3 65F4 F66F FACC ECFC"    /* êëíì...ó.ôö.úùûü */
/*Ax*/  $"A0B0 CA4C A72A A6DF 5263 54EA A8AD 6749"    /* .°¢.§..ß...´¨... */
/*Bx*/  $"6949 B2B3 694B 6445 B34C 6CA5 B5C5 E54E"    /* ........∏..ªºΩæ. */
/*Cx*/  $"6ED1 2DC3 F1D2 C63D 3FC9 A0F2 D54F F54F"    /* .¡..ƒ≈.... ÀÃ.Œ. */
/*Dx*/  $"2D2D 2222 2727 F7D7 6FC0 E0D8 3D3F F852"    /* ......÷..Ÿ⁄€..ﬁ. */
/*Ex*/  $"72A9 2C2C B9A6 B6C1 ABBB CDAE BE55 D3D4"    /* .·..‰ÂÊÁËÈÍÎÏ.ÓÔ */
/*Fx*/  $"75D9 DAF9 DBFB 5575 DDFD 6BAF A3BF 47B7"    /* .ÒÚÛÙı..¯˘.˚¸˝.ˇ */
};


/*
read 'Tang' ( ABOUT_BIGLOGO_TANG, "biglogo.gif", purgeable) "biglogo.gif";	// Was splash.gif
read 'Tang' ( ABOUT_JAVALOGO_TANG, "javalogo.gif", purgeable) "javalogo.gif";
read 'Tang' ( ABOUT_RSALOGO_TANG, "rsalogo.gif", purgeable ) "rsalogo.gif";
read 'Tang' ( ABOUT_HYPE_TANG, "hype.au", purgeable ) "hype.au";
read 'Tang' ( ABOUT_QTLOGO_TANG, "qtlogo.gif", purgeable) "qt_logo.gif";

read 'Tang' ( ABOUT_INSOLOGO_TANG, "insologo.gif", purgeable) "insologo.gif";
read 'Tang' ( ABOUT_LITRONIC_TANG, "litronic.gif", purgeable) "litronic.gif";
read 'Tang' ( ABOUT_MCLOGO_TANG, "mclogo.gif", purgeable) "mclogo.gif";
read 'Tang' ( ABOUT_MMLOGO_TANG, "mmlogo.gif", purgeable) "mmlogo.gif";
read 'Tang' ( ABOUT_NCCLOGO_TANG, "ncclogo.gif", purgeable) "ncclogo.gif";
read 'Tang' ( ABOUT_ODILOGO_TANG, "odilogo.gif", purgeable) "odilogo.gif";
read 'Tang' ( ABOUT_SYMLOGO_TANG, "symlogo.gif", purgeable) "symlogo.gif";
read 'Tang' ( ABOUT_TDLOGO_TANG, "tdlogo.gif", purgeable) "tdlogo.gif";
read 'Tang' ( ABOUT_VISILOGO_TANG, "visilogo.gif", purgeable) "visilogo.gif";
read 'Tang' ( ABOUT_COSLOGO_TANG, "coslogo.jpg", purgeable) "coslogo.jpg";
*/

read 'Tang' ( ABOUT_MOZILLA_FLAME, "flamer.gif", purgeable) "flamer.gif";
read 'TEXT' ( ABOUT_ABOUTPAGE_TEXT, "about-all.html", purgeable ) "about-all.html";
/*
read 'TEXT' ( ABOUT_AUTHORS_TEXT, "authors.html", purgeable ) "authors2.html";
*/
read 'TEXT' ( ABOUT_MOZILLA_TEXT, "mozilla.html", purgeable ) "mozilla.html";
read 'TEXT' ( ABOUT_PLUGINS_TEXT, "aboutplg.html", purgeable ) "aboutplg.html";
read 'TEXT' ( ABOUT_MAIL_TEXT, "mail.msg", purgeable ) "mail.msg";


data 'TEXT' ( ABOUT_BLANK_TEXT, purgeable )
{
	" "
};	// Gone is the glorious "Homey don't play dat!"
