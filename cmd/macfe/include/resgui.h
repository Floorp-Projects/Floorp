/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*-----------------------------------------------------------------------------
	resgui.h
	Module-Specific Interface Constants
-----------------------------------------------------------------------------*/
#ifndef __RESGUI__
#define __RESGUI__

#ifdef MOZ_COMMUNICATOR_NAME
#define PROGRAM_NAME "Communicator"
#else
#define PROGRAM_NAME "Navigator"
#endif // MOZ_COMMUNICATOR_NAME

#define emSignature 		'MOZZ'		// 'MOSS' for Netscape build, 'MOZZ' for free source build

// •• File types
#define emPrefsType			'pref'
#define emTextType			'TEXT'
#define emBookmarkFile		'URL '	// Bookmark file
#define emPluginFile 		'NSPL'
#define MAIL_MESSAGE_FLAVOR	'MSTB'
#define NEWS_ARTICLE_FLAVOR	'NSTB'
#define TEXT_FLAVOR			'TEXT'
#define emHelpType			'HELP'
#define emRegistry			'REGS'
#define emProfileType		'PRFL'
#define emNetprofileType	'PRFN'
#define emLDIFType			'LDIF'

//	Locale Boundle - use  for l10n
#define emLocaleBndl		'lBDL'

#define emKeyChain			'KCHN'	// Key chain
#define emMagicCookie		'COOK'	// Magic cookie
#define emCertificates		'CERT'	// Certificates
#define emCertIndex			'CERI'	// Certificate index
// DBM files have prefixes DBM
#define emGlobalHistory		'DBMG'	// Global history file
#define emCacheFat			'DBMC'	// CacheFAT
#define emExtCache			'DBME'	// External cache
// Address Book (neo database file)
#define emAddressBookDB		'NAB '
// Mail files
#define emMailFilterRules	'MRUL'
#define emMailFilterLog		'TEXT'
#define emMailFolderSummary	'MSUM'
#define emMailSubdirectory	'MSBD'
// News
#define emNewsrcFile		'NEWS'	// Newsrc file, just like Newswatcher's
#define emNewsrcDatabase	'DBNW'	// News database
#define emNewsXoverCache	'DBNX'	// News xover cache
#define emNewsHostDatabase	'DBNW'	// News host database

// •• Drag/clip flavors
#define emBookmarkFileDrag	'urlF'		// FSSpec for promiseHFS drags
#define emXPBookmarkClipboard 'urlX'	// Cross-platform bookmark copy/paste
#define emBookmarkDrag		'URLD'		// URL and a title drag for page proxy icon
										//  and in-page URLs
//#define emComposerNativeDrag 'CNDr'
#define emHTNodeDrag 'HTND'				// HT_Resource data for a nav center item

#define emContextPopupMenu	4100

// Stop Loading/Animations

#define STOP_STRING_LIST			9000
#define STOP_LOADING_INDEX			1
#define STOP_ANIMATIONS_INDEX		2

// •• menu commands/messages.  SEE ALSO MailNewsGroupWindow_Defines.h
//#define cmd_EditGeneral				1000	// Edit General Prefs
//#define cmd_EditMailNews			1001	// Edit Mail and News Prefs
#define cmd_Preferences				1001
#define cmd_EditNetwork				1002	// Edit Network Prefs
#define cmd_EditSecurity			1003	// Edit Security Prefs
#define cmd_Reload					1004	// Reload the URL in the top window
//#define cmd_GoForward				1005	// Go forward
#define cmd_Back					1006	// Go back
#define cmd_ViewSource				1007	// View source. Handled by hyperWindow
#define cmd_MailTo					1008	// Mail to
//#define cmd_Home					1009	// Go home
#define cmd_DelayImages				1010	// Delay image loading
#define cmd_LoadImages				1011	// Load all images
#define cmd_AddToBookmarks			1012	// Add to hotlist
#define cmd_ReloadImage				1013	// Reload images
#define cmd_Find					1014	// Find
#define cmd_FindAgain				1015	// Find again
#define cmd_MailNewsSearch			8100	// Mail/News Search Dialog command
#define cmd_Stop					1016
#define cmd_ToggleToolbar			1017
#define cmd_ToggleURLField			1018	// the Akbar terminology
#define cmd_ToggleLocationBar		1018	// The 4.0 and later terminology
#define cmd_TogglePersonalToolbar	1199
#define cmd_ToggleStatusArea		1019

#define cmd_ShowLocationBar			1500
#define cmd_HideLocationBar			1501

	/* nav center toggle in Netscape_Constants.h */
#define cmd_FancyFTP				1020
#define cmd_SaveDefaultCharset		1021
#define cmd_ShowDirectory			1022
#define cmd_DocumentInfo			1023
#define cmd_ShowSecurityBar			1024
#define LOGO_BUTTON					1025
#define cmd_CoBrandLogo				1026
#define cmd_DocumentEncoding		1027

/* THESE ARE DUPLICATES */
#define cmd_SortArticles			1028
#define cmd_MoveArticles			1029
#define	cmd_CopyArticles			1030
#define cmd_MessageHeaders			1031
/* END DUPLICATES */

#define cmd_MailNewsFolderWindow	1028	// Show mail window
#define cmd_Inbox					1029	// Show inbox
#define cmd_BookmarksWindow 		1030	// Show the bm pane of NavCenter in new window
#define cmd_HistoryWindow			1031	// Show history pane of NavCenter in new window
//#define cmd_OpenURL					1032
#define cmd_AddressBookWindow		1033
#define cmd_Redo					1034

#define cmd_ViewFrameSource			1035
#define cmd_ViewFrameInfo			1036

#define cmd_ToggleTaskBar			1037

#define cmd_NewFolder				1048	// Also mail folder, not just for bookmarks!

#if 0
/* bookmarks menus */
#define cmd_OpenBookmarks			1039
#define cmd_SetAddHeader			1040
#define cmd_ImportBookmarks			1041
#define	cmd_WhatsNew				1042
#define cmd_SelectAllBookmarks		1043
#define cmd_BookmarkProps			1044
#define cmd_GotoBookmark			1045
#define cmd_SortBookmarks			1046
#define	cmd_InsertBookmark			1047
#define cmd_InsertHeader			1048
#define cmd_InsertSeparator			1049
#define cmd_MakeAlias				1050
#define cmd_ViewInNetscape			1051
#define cmd_SetMenuHeader			1052
#define cmd_MoveBookmark			1053
#define cmd_NewWorkspace			1054
#endif

/* address book menus */
#define cmd_ImportAddressBook		1060
#define cmd_LaunchImportModule		1063

/* mail composition */
#define cmd_Send					1054
#define cmd_Queue					1055
#define cmd_PasteQuote				1056
#define cmd_Scramble				1057
#define cmd_ShowAllHeaders			1058
#define cmd_QueueOption				1059
#define cmd_ImmediateDelivery		1061
#define cmd_DeferredDelivery		1062

/* 	these need to be in the same order as
 	the panes in the view */
#define	cmd_ShowMailTo				1070
#define	cmd_ShowNewsgroupTo			1071
#define cmd_ShowReplyTo				1072
#define cmd_ShowMailCC				1073
#define cmd_ShowMailBCC				1074
#define	cmd_ShowFileCC				1075
#define	cmd_ShowFollowUpTo			1076

#define cmd_NetSearch				1080

#ifdef FORTEZZA
#define	cmd_FortezzaCard			1095
#define	cmd_FortezzaChange			1096
#define	cmd_FortezzaView			1097
#define	cmd_FortezzaInfo			1098
#define	cmd_FortezzaLogout			1099
#endif


/* mail reading */
#define	MAIL_MENU_BASE				1100
//#define cmd_OpenFolder				1100
//#define cmd_NewFolder				1101
//#define cmd_DeleteFolder			1102
#define cmd_OpenNewsHost			1103
#define cmd_SaveMessagesAs			1104
#define cmd_CopyMessagesInto		1105
#define cmd_MoveMessagesInto		1106
#define cmd_GetInfo					1107
#define cmd_ShowAllNewsgroups		1108
#define cmd_CheckNewNewsgroups		1109
#define cmd_Rot13Message			1110
#define cmd_GetNewMail				1111
#define cmd_DeliverQueuedMessages	1112

#define cmd_MailUndo				1115
#define cmd_MailRedo				1116
#define cmd_PostReply				1119
#define cmd_PostAndMailReply		1120
#define cmd_PostNew					1121
#define cmd_MailNew					1122
//#define cmd_ForwardMessage			1123
#define cmd_MarkMessage				1124
#define cmd_UnmarkAllMessages		1125
#define cmd_DeleteMessage			1126
#define cmd_CancelMessage			1127
#define cmd_SelectAllMessages		1128
#define cmd_UnselectAllMessages		1129
#define cmd_SelectThread			1130
#define cmd_SelectMarkedMessages	1131
#define cmd_MarkSelectedMessages	1132
//#define cmd_EmptyTrash				1133
#define cmd_CompressFolder			1134
#define cmd_CompressAllFolders		1135
#define cmd_MarkSelectedRead		1136
#define cmd_MarkSelectedUnread		1137
#define cmd_MarkThreadRead			1138
#define cmd_MarkAllRead				1139
#define cmd_ShowAllMessages			1140
#define cmd_SelectSelection			1141 /* No-op.  Handy for context menus */
//#define cmd_PreviousMessage			1142
#define cmd_FirstMessage			1143
#define cmd_LastMessage				1144
//#define cmd_NextUnreadMessage		1145
#define cmd_PreviousUnreadMessage	1146
#define cmd_FirstUnreadMessage		1147
#define cmd_LastUnreadMessage		1148
#define cmd_NextMarkedMessage		1149
#define cmd_PreviousMarkedMessage	1150
#define cmd_FirstMarkedMessage		1151
#define cmd_LastMarkedMessage		1152
#define cmd_ThreadMessages			1153
#define cmd_SortByMessageNumber		1155
#define cmd_Resort					1159
#define cmd_ShowAllMessageHeaders	1160
#define cmd_OpenSelectionNewWindow	1161
#define cmd_SendFormattedText		1162
#define cmd_QueueForLaterDelivery	1163
#define cmd_AttachAsText			1164
#define cmd_UnmarkSelectedMessages	1165
//	Renamed the unused "cmd_OpenSelectedFolder" command.  DDM 28-JAN-97
#define cmd_OpenSelection			1166
#define cmd_ShowOnlyUnreadMessages	1167
#define cmd_ShowSubscribedNewsgroups	1168
#define cmd_ShowActiveNewsgroups	1169
#define cmd_SaveMessagesAsAndDelete	1170
#define cmd_ForwardQuoted			1171
#define cmd_RemoveNewshost			1172
#define cmd_AddToAddressBook		1173
#define cmd_AddNewsgroup			1174
#define cmd_AddFromNewest			1175
#define cmd_AddFromOldest			1176
#define cmd_GetMoreMessages			1177
#define	cmd_GetAllMessages			1178
#define cmd_AttachInline			1179
#define cmd_AttachLinks				1180
#define	cmd_ShowSomeMessageHeaders	1181
#define cmd_ShowMicroMessageHeaders	1182
#define cmd_WrapLongLines			1183
#define cmd_SubscribeNewsgroups		1184
#define MAIL_MENU_LAST				cmd_SubscribeNewsgroups
#define cmd_UpdateMessageCount		1185
#define cmd_MailDocument			1190
#define cmd_AboutPlugins			1191
#define cmd_ReloadFrame				1192
#define cmd_FTPUpload				1193
#define cmd_SecurityInfo			1194
#define cmd_EditMessage				1195
#define cmd_ManageMailAccount		1196
#define cmd_NewNewsgroup			1197
#define cmd_ModerateNewsgroup		1198

/* Editor Window Menu */
#define cmd_BrowserWindow			1206

#ifdef EDITOR	/* editing commands */

/* Browser File Menu */
#define cmd_NewWindowEditor			1221
#define cmd_NewWindowEditorIFF		1200
#define	cmd_EditDocument			1201
#define	cmd_EditFrameSet			1220	// this works the same as EditDocument right now
#define cmd_EditFrame				1296
#define cmd_OpenFileEditor			1202

/* Browser Options Menu (Preferences) */
#define cmd_EditEditor				1203

/* Editor File Menu */
#define cmd_OpenURLEditor			1204
#define	cmd_BrowseDocument			1205

/* Editor Insert Menu */
#define cmd_Insert_Object			'InsO'
#define	cmd_Insert_Link				1207
#define	cmd_InsertEditLink			1257	/* based on selection: edit existing or insert new */
#define	cmd_Insert_Target			1208
#define	cmd_InsertEdit_Target		1274	/* based on selection: edit existing or insert new */
#define	cmd_Insert_Image			1209
#define cmd_InsertEditImage			1255	/* based on selection: edit existing or insert new */
#define	cmd_Insert_Line				1210
#define	cmd_InsertEditLine			1256	/* based on selection: edit existing or insert new */
#define	cmd_Insert_LineBreak		1211
#define	cmd_Insert_BreakBelowImage	1212

/* Editor Format Menu */
#define cmd_Format_FontHierMenu		'FONT'
#define	cmd_Format_Text				1214
#define	cmd_Format_PageTitle		1215
#define	cmd_Format_Document			1218
#define	cmd_Format_FontColor		1219
#define cmd_FormatColorsAndImage	1213	// html mail compose only

/* Editor Format Character Menu */
//#define	cmd_Format_Character_Normal			1220	// use cmd_Plain
//#define	cmd_Format_Character_Bold			1221	// use cmd_Bold
//#define	cmd_Format_Character_Italic			1222	// use cmd_Italic
#define	cmd_Format_Character_Nonbreaking	1223
#define	cmd_Format_Character_Superscript	1224
#define	cmd_Format_Character_Subscript		1225
#define	cmd_Format_Character_Blink			1226
#define	cmd_Format_Character_ClearAll		1227

/* Editor Format Paragraph Menu */
#define	cmd_Format_Paragraph_Normal			1228
#define	cmd_Format_Paragraph_Head1			1229
#define	cmd_Format_Paragraph_Head2			1230
#define	cmd_Format_Paragraph_Head3			1231
#define	cmd_Format_Paragraph_Head4			1232
#define	cmd_Format_Paragraph_Head5			1233
#define	cmd_Format_Paragraph_Head6			1234
#define	cmd_Format_Paragraph_Address		1235
#define	cmd_Format_Paragraph_Formatted		1236
#define	cmd_Format_Paragraph_List_Item		1237
#define	cmd_Format_Paragraph_Desc_Title		1238
#define	cmd_Format_Paragraph_Desc_Text		1239
#define	cmd_Format_Paragraph_Indent			1240
#define	cmd_Format_Paragraph_UnIndent		1241

/* Editor Format Font Size Menu */
#define cmd_Format_Font_Size_N2				1242
#define cmd_Format_Font_Size_N1				1243
#define cmd_Format_Font_Size_0				1244
#define cmd_Format_Font_Size_P1				1245
#define cmd_Format_Font_Size_P2				1246
#define cmd_Format_Font_Size_P3				1247
#define cmd_Format_Font_Size_P4				1248

/* Editor Options Menu */
#define	cmd_Toggle_Character_Toolbar		1250
#define	cmd_Toggle_Paragraph_Toolbar		1251

/* These are actually messages from toolbar buttons */
//#define cmd_IncreaseFontSize				1253	// use cmd_FontLarger
//#define cmd_DecreaseFontSize				1254	// use cmd_FontSmaller
//#define	cmd_AlignParagraphLeft				1258	// use cmd_JustifyLeft
//#define	cmd_AlignParagraphCenter			1259	// use cmd_JustifyCenter
//#define	cmd_AlignParagraphRight				1260	// use cmd_JustifyRight

#define cmd_Format_Character_Strikeout		1261

// messages from the Editor Dialog Boxes
#define msg_Clear_Text_Styles				1267
#define msg_Clear_All_Styles				1268
#define msg_Paragraph_Style_Popup			1269
#define msg_Paragraph_Addtnl_Style_Popup	1270
#define msg_List_Style_Popup				1271

// messages from the paragraph toolbar
#define msg_MakeNoList						1222
#define msg_MakeNumList						1272
#define msg_MakeUnumList					1273
// messages from the character toolbar
#define msg_Link_Browse_File				1275
#define msg_Link_Clear_Link					1276
#define msg_Default_Color_Radio				1280
#define msg_Custom_Color_Radio				1281
#define msg_Font_Face_Changed				1279
#define msg_Font_Size_Changed				1282
#define cmd_DisplayParagraphMarks			1283
#define cmd_Insert_Unknown_Tag				1284

//Table support
#define cmd_Insert_Table					1285
#define cmd_Delete_Table					1286
#define cmd_Format_Table					1287
#define cmd_Insert_Row						1288
#define cmd_Delete_Row						1289
#define cmd_Insert_Col						1290
#define cmd_Delete_Col						1291
#define cmd_Insert_Cell						1316
#define cmd_Delete_Cell						1317
//#define cmd_Insert_Caption					1292
//#define cmd_Delete_Caption					1293
#define cmd_Format_Cell						1294
#define cmd_Format_Row						1318
// #define cmd_DisplayTableBoundaries			1315
#define cmd_Select_Table					1319
#define cmd_DisplayTables					1324


#define cmd_Paragraph_Hierarchical_Menu		'Para'		// 1296
#define cmd_Font_Size_Hierarchical_Menu		'Size'
#define cmd_Align_Hierarchical_Menu			'Algn'

// font menu
#define cmd_FormatViewerFont				1253
#define cmd_FormatFixedFont					1254

#define cmd_New_Document_Template			1301
#define cmd_New_Document_Wizard				1302
#define cmd_Publish							1303
#define cmd_Refresh							1304
#define cmd_EditSource						1305
#define cmd_Remove_Links					1306
#define cmd_Format_Target					1307
#define cmd_Format_Unknown_Tag				1308
#define cmd_Format_DefaultFontColor			1309

#define msg_Doc_Advanced_Prop_New_Tag		1312
#define msg_Doc_Advanced_Prop_Set_Tag		1313
#define msg_Doc_Advanced_Prop_Delete_Tag	1314

#define cmd_GoDefaultPublishLocation		1325
#endif /* EDITOR */

#define msg_Apply		3001
#define msg_Defaults	3002

#define cmd_AttachToMessage			2006
#define cmd_IncludeOriginalText		2010

#define	HISTORY_MENU_BASE			2000
#define HISTORY_LIMIT				32
#define HISTORY_MENU_LAST			HISTORY_MENU_BASE + HISTORY_LIMIT

#define COPY_MOVE_BASE				2050	// reserved for entries in the "Copy to…"
#define COPY_MOVE_LAST				2200	// and "Move to…" submenus
											
#define TOOLS_MENU_BASE				2500	// Reserved for up to 3000 for tools menus
#define TOOLS_MENU_BASE_LAST		2989

#define FONT_MENU_BASE				12500	// Reserved 485 for font menu
#define FONT_MENU_BASE_LAST			12984

#define RECENT_EDIT_MENU_BASE		12985
#define RECENT_EDIT_MENU_BASE_LAST	12999

#define	COLOR_POPUP_MENU_BASE		13000	// Reserve 260 for color popup menu
#define	COLOR_POPUP_MENU_BASE_LAST	13259

#define cmd_EditorPluginStop		2991

#define BOOKMARKS_MENU_BASE			3050	// Reserved for up to 4000 for hotlist menus
#define BOOKMARKS_MENU_BASE_LAST	3999

#define cmd_LaunchCalendar			6600
#define cmd_Launch3270				6601
#define cmd_LaunchNetcaster			6602
#define cmd_LaunchAOLInstantMessenger 6603

#define cmd_LaunchConference		6610

#define cmd_ShowJavaConsole			7866

#define cmd_WINDOW_MENU_BASE 		9001	// 9001 through 9050 reserved for windows
#define cmd_WINDOW_MENU_BASE_LAST	9050


//#define cmd_Directory				1300

#define DIR_BUTTON_BASE				4000
#define DIR_MENU_BASE				4020
#define DIR_BUTTON_LAST				4050

#define CONTEXT_MENU_BASE			4070
#define	cmd_NEW_WINDOW_WITH_FRAME 	 4 + CONTEXT_MENU_BASE
// ----
#define	cmd_OPEN_LINK				 6 + CONTEXT_MENU_BASE
#define	cmd_NEW_WINDOW				 8 + CONTEXT_MENU_BASE
#define cmd_SAVE_LINK_AS			 9 + CONTEXT_MENU_BASE
#define	cmd_COPY_LINK_LOC			10 + CONTEXT_MENU_BASE
// ---
#define	cmd_VIEW_IMAGE				12 + CONTEXT_MENU_BASE
#define	cmd_SAVE_IMAGE_AS			13 + CONTEXT_MENU_BASE
#define	cmd_COPY_IMAGE				14 + CONTEXT_MENU_BASE
#define	cmd_COPY_IMAGE_LOC			15 + CONTEXT_MENU_BASE
#define	cmd_LOAD_IMAGE				16 + CONTEXT_MENU_BASE
#define CONTEXT_MENU_LAST			4999

// •• Cursors
#define curs_Hand			128
#define curs_VertDrag		129
#define curs_HoriDrag		130
#define curs_CopyDrag		132

// •• Text traits
#define FieldTextTxtr	 	4001

// •• Windows
#define Wind_NamePass		1005	// Name and password for FE_Name
#define Wind_Prompt			1006	// Prompt for FE_Prompt
#define Wind_Pass			1007	// Prompt for a password

#define		IBM3270_FOLDER			"3270"
#define		IBM3270_FILE			"HE3270EN.HTM"

// •• Preferences built-in types
#define PREF_SOURCE								"Netscape/DocumentSource"
#define	TELNET_APPLICATION_MIME_TYPE			"Netscape/Telnet"
#define	TELNET_APPLICATION_FILE_TYPE			'CONF'
#define HTML_VIEWER_APPLICATION_MIME_TYPE		"Netscape/Source"
#define HTML_VIEWER_APPLICATION_FILE_TYPE		'TEXT'
#define	TN3270_VIEWER_APPLICATION_MIME_TYPE		"Netscape/tn3270"
#define TN3270_VIEWER_APPLICATION_FILE_TYPE		'GFTS'

// Preference resources

#define STARTUP_BROWSER		1
#define STARTUP_NEWS		2
#define STARTUP_MAIL		3
#define STARTUP_VISIBLE		4
#define BROWSER_STARTUP_ID	1
#define MAIL_STARTUP_ID		2
#define NEWS_STARTUP_ID		4
#define ADDRESS_STARTUP_ID	8
#define NAVCENTER_STARTUP_ID 16

#define res_Strings1 2001
// 2002 and 2003 are taken by obsolete preferences
#define res_StringsBoolean 2004
#define res_StringsLong 2005
#define res_StringsColor 2006
#define res_Strings2 2007
#define res_PrintRecord 1

#define BOOLEAN_PREFS_RESID					129
#define LONG_PREFS_RESID					129
#define COLORS_PREFS_RESID					129
#define	STRING_PREFS_RESID					2001
#define FONT_PREFS_RESID					2002
#define PROT_HANDLER_PREFS_RESID			2003
#define MIME_PREFS_FIRST_RESID				129

// preferences error strings
#define mPREFS_UNSPECIFIED_ERR_ALERT		21000
#define mPREFS_CANNOT_OPEN_SECOND_ALERT		21004
#define mPREFS_CANNOT_CREATE				21008
#define mPREFS_CANNOT_CREATE_PREFS_FOLDER	21012
#define mPREFS_CANNOT_READ					21016
#define mPREFS_CANNOT_WRITE					21020
#define mPREFS_DUPLICATE_MIME				21024

#define	BUTTON_STRINGS_RESID				301
#define BUTTON_TOOLTIPS_RESID				302
#define PREF_WINDOWNAMES_RESID				303
#define HELPFILES_NAMES_RESID				304

#define	TOOLBAR_ICONS	0					// Toolbar display style possible values
#define TOOLBAR_TEXT	1
#define TOOLBAR_TEXT_AND_ICONS 2

#define	DEFAULT_BACKGROUND		0
#define	CUSTOM_BACKGROUND		1
#define GIF_FILE_BACKGROUND		2

#define	PRINT_REDUCED		XL
#define PRINT_CROPPED		1
#define PRINT_RESIZED		2

#define DLOG_SAVEAS			1040
#define DLOG_GETFOLDER		1041
#define DLOG_UPLOADAS		1043

#define HELP_URLS_RESID			2020
#define LOGO_BUTTON_URL_RESID	2022
#define HELP_URLS_MENU_STRINGS	2090
#define WINDOW_TITLES_RESID		2030
#define NEW_DOCUMENT_URL_RESID	2040

#define GENERIC_FONT_NAMES_RESID	3000

#define	BYTES_PER_MEG	1048576L

#define MAIL_SORT_BY_DATE			1
#define MAIL_SORT_BY_MESSAGE_NUMBER	2
#define MAIL_SORT_BY_SUBJECT			3
#define MAIL_SORT_BY_SENDER			4

#define SHOW_ALL_HEADERS			1
#define SHOW_SOME_HEADERS			2
#define SHOW_MICRO_HEADERS			3

#define NORMAL_PANE_CONFIG			0
#define STACKED_PANE_CONFIG			1
#define LEFT_PANE_CONFIG			2

#define	neverAgain		3

#define FE_STRINGS_BASE			500
#define MAIL_WIN_ERR_RESID		(FE_STRINGS_BASE +   0)
#define MAIL_TOO_LONG_RESID		(FE_STRINGS_BASE +   4)
#define MAIL_TMP_ERR_RESID		(FE_STRINGS_BASE +   8)
#define NO_EMAIL_ADDR_RESID		(FE_STRINGS_BASE +  12)
#define NO_SRVR_ADDR_RESID		(FE_STRINGS_BASE +  16)
#define MAIL_SUCCESS_RESID		(FE_STRINGS_BASE +  20)
#define MAIL_NOT_SENT_RESID		(FE_STRINGS_BASE +  24)
#define MAIL_SENDING_RESID		(FE_STRINGS_BASE +  30)
#define NEWS_POST_RESID			(FE_STRINGS_BASE +  34)
#define MAIL_SEND_ERR_RESID		(FE_STRINGS_BASE +  38)
#define MAIL_DELIVERY_ERR_RESID	(FE_STRINGS_BASE +  40)
#define DISCARD_MAIL_RESID		(FE_STRINGS_BASE +  44)
#define SECURITY_LEVEL_RESID	(FE_STRINGS_BASE +  48)
#define NO_MEM_LOAD_ERR_RESID	(FE_STRINGS_BASE +  52)
#define EXT_PROGRESS_RESID		(FE_STRINGS_BASE +  56)
#define REVERT_PROGRESS_RESID	(FE_STRINGS_BASE +  60)
#define START_LOAD_RESID		(FE_STRINGS_BASE +  64)
#define NO_XACTION_RESID		(FE_STRINGS_BASE +  68)
#define LAUNCH_TELNET_RESID		(FE_STRINGS_BASE +  70)
#define LAUNCH_TN3720_RESID		(FE_STRINGS_BASE +  74)
#define TELNET_ERR_RESID		(FE_STRINGS_BASE +  78)
#define SAVE_AS_RESID			(FE_STRINGS_BASE +  82)
#define NETSITE_RESID			(FE_STRINGS_BASE +  86)
#define LOCATION_RESID			(FE_STRINGS_BASE +  90)
#define GOTO_RESID				(FE_STRINGS_BASE +  94)
#define DOCUMENT_DONE_RESID		(FE_STRINGS_BASE +  98)
#define LAYOUT_COMPLETE_RESID	(FE_STRINGS_BASE + 102)
#define CONFORM_ABORT_RESID		(FE_STRINGS_BASE + 106)
#define SUBMIT_FORM_RESID		(FE_STRINGS_BASE + 110) 
#define SAVE_IMAGE_RESID		(FE_STRINGS_BASE + 114)
#define SAVE_QUOTE_RESID		(FE_STRINGS_BASE + 118) 
#define WILL_OPEN_WITH_RESID	(FE_STRINGS_BASE + 122)
#define WILL_OPEN_TERM_RESID	(FE_STRINGS_BASE + 123)
#define SAVE_AS_A_RESID			(FE_STRINGS_BASE + 126)
#define FILE_RESID				(FE_STRINGS_BASE + 130)
#define COULD_NOT_SAVE_RESID	(FE_STRINGS_BASE + 134)
#define DISK_FULL_RESID			(FE_STRINGS_BASE + 138)
#define DISK_ERR_RESID			(FE_STRINGS_BASE + 142)
#define BOOKMARKS_RESID			(FE_STRINGS_BASE + 146)
#define NOT_VISITED_RESID		(FE_STRINGS_BASE + 150)
#define NO_FORM2HOTLIST_RESID	(FE_STRINGS_BASE + 154)
#define NEW_ITEM_RESID			(FE_STRINGS_BASE + 158)
#define NEW_HEADER_RESID		(FE_STRINGS_BASE + 162)
#define CONFIRM_RM_HDR_RESID	(FE_STRINGS_BASE + 166)
#define AND_ITEMS_RESID			(FE_STRINGS_BASE + 170)
#define SAVE_BKMKS_AS_RESID		(FE_STRINGS_BASE + 174)
#define END_LIST_RESID			(FE_STRINGS_BASE + 178)
#define ENTIRE_LIST_RESID		(FE_STRINGS_BASE + 182)
#define NEW_RESID				(FE_STRINGS_BASE + 186)
#define OTHER_RESID				(FE_STRINGS_BASE + 190)
#define MEM_AVAIL_RESID			(FE_STRINGS_BASE + 194)
#define SAVE_RESID				(FE_STRINGS_BASE + 198)
#define LAUNCH_RESID			(FE_STRINGS_BASE + 202)
#define INTERNAL_RESID			(FE_STRINGS_BASE + 206)
#define UNKNOWN_RESID			(FE_STRINGS_BASE + 210)
#define MEGA_RESID				(FE_STRINGS_BASE + 214)
#define KILO_RESID				(FE_STRINGS_BASE + 215)
#define PICK_COLOR_RESID		(FE_STRINGS_BASE + 218)
#define BAD_APP_LOCATION_RESID	(FE_STRINGS_BASE + 222)
#define REBUILD_DESKTOP_RESID	(FE_STRINGS_BASE + 226)
#define UNTITLED_RESID			(FE_STRINGS_BASE + 230)
#define REG_EVENT_ERR_RESID		(FE_STRINGS_BASE + 234)
#define APP_NOT_REG_RESID		(FE_STRINGS_BASE + 238)
#define UNREG_EVENT_ERR_RESID	(FE_STRINGS_BASE + 242)
#define BOOKMARK_HTML_RESID		(FE_STRINGS_BASE + 246)
#define NO_DISKCACHE_DIR_RESID	(FE_STRINGS_BASE + 250)
#define NO_SIGFILE_RESID		(FE_STRINGS_BASE + 254)
#define NO_BACKDROP_RESID		(FE_STRINGS_BASE + 258)
#define SELECT_RESID			(FE_STRINGS_BASE + 262)
#define AE_ERR_RESID			(FE_STRINGS_BASE + 266)
#define CHARSET_RESID			(FE_STRINGS_BASE + 270)
#define BROWSE_RESID			(FE_STRINGS_BASE + 274)
#define ENCODING_CAPTION_RESID	(FE_STRINGS_BASE + 278)
#define NO_TWO_NETSCAPES_RESID	(FE_STRINGS_BASE + 282)
#define PG_NUM_FORM_RESID		(FE_STRINGS_BASE + 286)
#define MENU_SAVE_AS			(FE_STRINGS_BASE + 290)
#define MENU_SAVE_FRAME_AS		(FE_STRINGS_BASE + 294)
#define MENU_PRINT				(FE_STRINGS_BASE + 298)
#define MENU_PRINT_FRAME		(FE_STRINGS_BASE + 302)
#define MENU_RELOAD				(FE_STRINGS_BASE + 306)
#define MENU_SUPER_RELOAD		(FE_STRINGS_BASE + 310)
#define MAC_PROGRESS_PREFS		(FE_STRINGS_BASE + 314)
#define MAC_PROGRESS_NET		(FE_STRINGS_BASE + 318)
#define MAC_PROGRESS_BOOKMARK	(FE_STRINGS_BASE + 322)
#define MAC_PROGRESS_ADDRESS	(FE_STRINGS_BASE + 326)
#define MAC_PROGRESS_JAVAINIT	(FE_STRINGS_BASE + 328)
#define REPLY_FORM_RESID		(FE_STRINGS_BASE + 330)

#define	MAC_UPLOAD_TO_FTP		(FE_STRINGS_BASE + 334)
#define	POP_USERNAME_ONLY		(FE_STRINGS_BASE + 338)	
#define	THE_ERROR_WAS			(FE_STRINGS_BASE + 342)
#define	MAC_PLUGIN				(FE_STRINGS_BASE + 346)	
#define MAC_NO_PLUGIN			(FE_STRINGS_BASE + 350)	
#define	MAC_REGISTER_PLUGINS	(FE_STRINGS_BASE + 354)	
#define	DOWNLD_CONT_IN_NEW_WIND	(FE_STRINGS_BASE + 358)	
#define	ATTACH_NEWS_MESSAGE		(FE_STRINGS_BASE + 362)
#define	ATTACH_MAIL_MESSAGE		(FE_STRINGS_BASE + 366)
#define	MBOOK_NEW_ENTRY			(FE_STRINGS_BASE + 370)	
#define	MCLICK_BACK_IN_FRAME	(FE_STRINGS_BASE + 374)	
#define	MCLICK_BACK				(FE_STRINGS_BASE + 378)
#define	MCLICK_FWRD_IN_FRAME	(FE_STRINGS_BASE + 382)
#define	MCLICK_FORWARD			(FE_STRINGS_BASE + 386)
#define	MCLICK_SAVE_IMG_AS		(FE_STRINGS_BASE + 390)
#define	SUBMIT_FILE_WITH_DATA_FK	(FE_STRINGS_BASE + 394)
#define	ABORT_CURR_DOWNLOAD		(FE_STRINGS_BASE + 398)	
#define MACDLG_SAVE_AS			(FE_STRINGS_BASE + 402)	

#define	MBOOK_NEW_BOOKMARK		(FE_STRINGS_BASE + 406)	

#define MENU_SEND_NOW			(FE_STRINGS_BASE + 410)
#define MENU_SEND_LATER			(FE_STRINGS_BASE + 414)	
#define QUERY_SEND_OUTBOX		(FE_STRINGS_BASE + 418)
#define QUERY_SEND_OUTBOX_SINGLE (FE_STRINGS_BASE + 422)

#define MENU_MAIL_DOCUMENT		(FE_STRINGS_BASE + 426)
#define MENU_MAIL_FRAME			(FE_STRINGS_BASE + 430)

#define DEFAULT_PLUGIN_URL		(FE_STRINGS_BASE + 434)

#define MBOOK_SEPARATOR_STR		(FE_STRINGS_BASE + 438)
#define RECIPIENT				(FE_STRINGS_BASE + 442)

#define DOCUMENT_SUFFIX			(FE_STRINGS_BASE + 446)
#define PASSWORD_CHANGE_STRING	(FE_STRINGS_BASE + 447)
#define PASSWORD_SET_STRING		(FE_STRINGS_BASE + 448)
#define DELETE_MIMETYPE			(FE_STRINGS_BASE + 449)
#define ERROR_LAUNCH_IBM3270			(FE_STRINGS_BASE + 450)
#define ERROR_OPEN_PROFILE_MANAGER		(FE_STRINGS_BASE + 451)

#define MEMORY_ERROR_LAUNCH		(FE_STRINGS_BASE + 460)
#define FNF_ERROR_LAUNCH		(FE_STRINGS_BASE + 461)
#define MISC_ERROR_LAUNCH		(FE_STRINGS_BASE + 462)

#define CONFERENCE_APP_NAME		(FE_STRINGS_BASE + 470)
#define CALENDAR_APP_NAME		(FE_STRINGS_BASE + 471)
#define IMPORT_APP_NAME			(FE_STRINGS_BASE + 472)
#define AIM_APP_NAME			(FE_STRINGS_BASE + 473)

#define NO_SRC_EDITOR_PREF_SET	(FE_STRINGS_BASE + 500)
#define NO_IMG_EDITOR_PREF_SET	(FE_STRINGS_BASE + 504)
#define INVALID_PUBLISH_LOC		(FE_STRINGS_BASE + 508)

#define NO_DICTIONARY_FOUND		(FE_STRINGS_BASE + 520)
#define NO_SPELL_SHLB_FOUND		(FE_STRINGS_BASE + 524)

#define CLOSE_STR_RESID			(FE_STRINGS_BASE + 600)
#define DUPLICATE_TARGET		(FE_STRINGS_BASE + 604)
#define CUT_ACROSS_CELLS		(FE_STRINGS_BASE + 608)
#define NOT_HTML				(FE_STRINGS_BASE + 612)
#define BAD_TAG					(FE_STRINGS_BASE + 616)
#define EDITOR_ERROR_EDIT_REMOTE_IMAGE	(FE_STRINGS_BASE + 624)

#define EDITOR_PERCENT_WINDOW			(FE_STRINGS_BASE + 664)
#define EDITOR_PERCENT_PARENT_CELL      (FE_STRINGS_BASE + 665)
#define IMAGE_SUBMIT_FORM				(FE_STRINGS_BASE + 666)

#define OTHER_FONT_SIZE			(FE_STRINGS_BASE + 667)

#define FILE_NAME_NONE			(FE_STRINGS_BASE + 668)

#define MENU_BACK				(FE_STRINGS_BASE + 669)
#define MENU_BACK_ONE_HOST		(FE_STRINGS_BASE + 770)
#define MENU_FORWARD			(FE_STRINGS_BASE + 771)
#define MENU_FORWARD_ONE_HOST	(FE_STRINGS_BASE + 772)

#define NETSCAPE_TELNET				(FE_STRINGS_BASE + 773)
#define NETSCAPE_TELNET_NAME_ARG	(FE_STRINGS_BASE + 774)
#define NETSCAPE_TELNET_HOST_ARG	(FE_STRINGS_BASE + 775)
#define NETSCAPE_TELNET_PORT_ARG	(FE_STRINGS_BASE + 776)

#define NETSCAPE_TN3270				(FE_STRINGS_BASE + 777)

#define CLOSE_WINDOW				(FE_STRINGS_BASE + 778)
#define CLOSE_ALL_WINDOWS			(FE_STRINGS_BASE + 779)

#define NO_PRINTER_RESID		(FE_STRINGS_BASE + 800)

#define MALLOC_HEAP_LOW_RESID		(FE_STRINGS_BASE + 801)
#define JAVA_DISABLED_RESID			(FE_STRINGS_BASE + 802)


/*-----------------------------------------------------------------------------
	Generic Resources
-----------------------------------------------------------------------------*/
#define Wind_OtherSizeDialog	1500

#define View_MimeTable			'ptbl'	// Mime table

	// Mail to window
#define View_MailWindow			'mail'	// CMailWindow

// Universal pane constants
/*------------------------------------------------------------------------------
	Main window
-----------------------------------------------------------------------------*/

// Resources
#define HYPER_WINDOW_ID		1000
#define NEWS_WINDOW_ID		2000
#define MAIL_WINDOW_ID		3000
#define MAIL_SEND_WINDOW_ID 8001

//#ifdef EDITOR
// #define EDITOR_WINDOW_ID	10000
//#endif

// Hypertext widgets
#define SCROLLER_ID			(HYPER_WINDOW_ID + 1)
#define MAIN_LEDGE_ID		(HYPER_WINDOW_ID + 4)

/*-----------------------------------------------------------------------------
	Load Item / Open URL dialog box
-----------------------------------------------------------------------------*/
#define li_base_rez				27753
#define li_base					10000

#define liLoadItemWind			(li_base_rez + 0)

#define liDoLoad				(li_base + 11)	// Load button, Load message, Load command
#define liDoCancel				(li_base + 12)	// cancel
#define liItem					(li_base + 13)	// the edit text
#define liHistory				(li_base + 14)	// the history popup

/*-----------------------------------------------------------------------------
	Forms
	Views are outside of the window, need to be loaded into it
-----------------------------------------------------------------------------*/
#define formSubmitButton	4000
#define formResetButton		4001
#define formPlainButton		4002
#define formCheckbox		4003
#define formRadio			4004
#define formBigText			4005
#define formTextField		4006
#define formScrollingList	4007
#define formPasswordField	4008
#define formPopup			4009
#define pluginView			4010
#define formFilePicker		4011
#define formGAPopup			4012
#define formGARadio			4013
#define formGACheckbox		4014
#define formGASubmitButton	4015
#define formGAResetButton	4016
#define formGAPlainButton	4017
#define formBigTextScroll	4018

#define formScrollingListID 'list'	// ID of the subview that is the list withing the scroller
#define formBigTextID		'text'	// ID of the subview that is the TEXT withing the scroller

#define formPopupChosen		2003

/*-----------------------------------------------------------------------------
	Preferences
-----------------------------------------------------------------------------*/
#define prefBaseWindowID	5000	// Window ID of the base window

// Main preferences window
#define	prefSubViewBase 	5010

#define prefPopup			'Pchc'

// Subviews of Mime mapping
#define pref8mainContain	View_MimeTable
#define helpersEditMimeID	'edmi'		// "Edit..." button
#define helpersNewMimeID	'nemi'		// "New..." button
#define helpersDeleteMimeID	'demi'		// "Delete..." button

/*-----------------------------------------------------------------------------
	New mime mapper dialog box
-----------------------------------------------------------------------------*/
#define newMimeTypeID			5001
#define	pref8EditDescription	2
#define pref8EditType			3
#define pref8EditExtensions		5
#define pref8textAppName		6
#define pref8butAppSet			7
#define pref8dataMenu			8
#define pref8radioSave			9
#define pref8radioLaunch		10
#define pref8radioPlugin		11
#define pref8radioInternal		12
#define pref8radioUnknown		13
#define pref8AppMenuLabel		14
#define pref8PluginMenu			15


/*-----------------------------------------------------------------------------
	Mailto window
-----------------------------------------------------------------------------*/
#define mailBaseWindowID	8000	// Mailto window ID
#define mailCaptionTo		'toto'	// To: field
#define mailEditSubject		'subj'	// Subject:
#define mailTextMainText	'main'	// Text of the message

#define msg_DefaultDir				300
#define msg_TemporaryDir			301
#define msg_Telnet					302
#define msg_Source					303
#define msg_tn3270					304 
#define msg_CacheDir				305
#define msg_NewsRC					306
#define msg_PopupChanged			5000	// Main popup has changes
#define msg_Default					5001	// Set default choices
#define msg_WindowStyle				301
#define msg_FontStyle				302

#define msg_PFontChange				500
#define msg_BFontChange				501
#define msg_PSizeChange				502
#define msg_BSizeChange				503
#define msg_CharSetChange			504
#define msg_DefaultCharSetChange	505

#define msg_ChangeFontSize			-15381	//	This is the command number the

#define msg_Help					'help'	// HELP! 1751477360
#define msg_ExpireAfter				401
#define msg_HomePage				402
#define msg_CustomLinkColors		404
#define msg_CustomVisitedColors		405
#define msg_CustomTextColors		406
#define msg_DefaultBackColors		408
#define	msg_CustomBackColors		409
#define msg_GIFBack					410

#define msg_LinkColorChange			420
#define msg_VisitedColorChange		421
#define msg_TextColorChange			422
#define msg_BackColorChange			423

#define msg_SigFile					430
#define msg_Browse					1200
/* #define	mBrowseTitle				"\pBrowse…" */
#define msg_FolderChanged			1201
#define msg_ClearDiskCache			1202

#define msg_ConfigureSSLv2			510
#define msg_ConfigureSSLv3			511

#define msg_ExpireNow				421
#define msg_ArrowsHit				700

#define msg_ResetButton				2000	// Reset button was hit, 'this' is ioParam
#define msg_SubmitButton			2001	// Submit button was hit, 'this' is ioParam
#define msg_SubmitText				2002	// Return hit in edit field, 'this' is ioParam
#define msg_URLText					2003
#define	msg_URLKeystroke			2004
#define msg_LoadedNewTabPane		2005
#define msg_CertSelect				2021
#define msg_CertChoose				2022

#define msg_NewUserStateForZoom		4000	// nil ioParam

#define ENCODING_BASE			1400
#define	cmd_ASCII				1401
#define cmd_LATIN1				1402
#define cmd_JIS					1403
#define cmd_SJIS				1404
#define cmd_EUCJP				1405
#define cmd_SJIS_AUTO			1406
#define cmd_MAC_ROMAN			1407
#define cmd_LATIN2				1408
#define cmd_MAC_CE				1409
#define cmd_BIG5				1410
#define cmd_CNS_8BIT			1411
#define cmd_GB_8BIT				1412
#define cmd_KSC_8BIT			1413
#define cmd_2022_KR				1414
#define cmd_USER_DEFINED_ENCODING	1415
#define cmd_MAC_CYRILLIC		1416
#define cmd_8859_5				1417
#define cmd_CP1251				1418
#define cmd_KOI8R				1419
#define cmd_MAC_GREEK			1420
#define cmd_8859_7				1421
#define cmd_CP1250				1422
#define cmd_MAC_TURKISH			1423
#define cmd_8859_9				1424
#define cmd_UTF8				1425
#define cmd_UTF7				1426
#define cmd_CP1253				1427

#define ENCODING_CEILING			1499	// Can I assume we will allow at most 30 encodings?
											// No!!! reserve 100 - ftang
#define MAX_ENCODINGS_IN_PULLRIGHT	(ENCODING_CEILING - ENCODING_BASE)

#define FENC_RESTYPE				'Fnec'
#define FNEC_RESID					128
#define CSIDLIST_RESTYPE			'CsiL'
#define CSIDLIST_RESID				128


#define cmd2csid_tbl_ResID  		140
	
// •• about: resources
// if resources end in text they are 'TEXT' resources.
// Otherwise they are 'TANG'
#define ABOUT_ABOUTPAGE_TEXT 128
#define ABOUT_BIGLOGO_TANG	129
#define ABOUT_PLUGINS_TEXT	130
#define ABOUT_AUTHORS_TEXT	131
#define ABOUT_MAIL_TEXT		134
#define ABOUT_RSALOGO_TANG	135
#define ABOUT_MOZILLA_TEXT	136
#define ABOUT_HYPE_TANG		137
#define ABOUT_BLANK_TEXT	138

#define ABOUT_JAVALOGO_TANG	140
#define ABOUT_CUSTOM_TEXT	201			// optional resource added by EKit
#ifdef EDITOR
#define ABOUT_NEW_DOCUMENT	139
#endif
#define ABOUT_LICENSE		2890

#define ABOUT_QTLOGO_TANG       141
#ifdef FORTEZZA
#define ABOUT_LITRONIC_TANG	142
#endif

// new for communicator

#define ABOUT_INSOLOGO_TANG		132
#define ABOUT_LITRONIC_TANG		133
#define ABOUT_MCLOGO_TANG		150	
#define ABOUT_MMLOGO_TANG		151
#define ABOUT_NCCLOGO_TANG		152
#define ABOUT_ODILOGO_TANG		153
#define ABOUT_SYMLOGO_TANG		154
#define ABOUT_TDLOGO_TANG		155
#define ABOUT_VISILOGO_TANG		156
#define ABOUT_COSLOGO_TANG		157

// new for mozilla source release

#define ABOUT_MOZILLA_FLAME		158

// Alerts for the help system
// where the hell am I suppose to define these?

#define BAD_HELPDOC_ALERT	1050
#define BAD_MEDIADOC_ALERT	1051
#define HELPILIZE_FAILED	1052

// we use this next one in the browser now.
#define EDITDLG_SAVE_PROGRESS			5101
#ifdef EDITOR
#define EDITDLG_SAVE_FILE_EXISTS		5103
#define EDITDLG_LINE_FORMAT				5104
#define EDITDLG_TARGET					5105
#define EDITDLG_CONFIRM_OBLITERATE_LINK	5106
#define EDITDLG_SAVE_BEFORE_QUIT		5109
#define EDITDLG_COPYRIGHT_WARNING		5110
#define EDITDLG_SAVE_BEFORE_BROWSE		5111
#define EDITDLG_UNKNOWN_TAG				5113
#define EDITDLG_TABLE					5116
#define EDITDLG_PUBLISH					5117
#define EDITDLG_LIMITS					5118
#define EDITDLG_FILE_MODIFIED			5119
#define EDITDLG_AUTOSAVE				5120

// These are Tabbed dialog boxes used for the editor
// Each dialog box below has a Tab with several views
#define EDITDLG_TAB_BASE		5150
#define EDITDLG_IMAGE			5150
#define EDITDLG_TEXT_AND_LINK	5151
#define EDITDLG_TEXT_STYLE		5152
#define EDITDLG_DOC_INFO		5153
#define EDITDLG_TABLE_INFO		5154
#define EDITDLG_BKGD_AND_COLORS	5155

#endif // EDITOR

// xlat resource stuff. This is use for single byte codeset conversion
// 	For CS_MAC_ROMAN
#define xlat_LATIN1_TO_MAC_ROMAN	 128
#define xlat_MAC_ROMAN_TO_LATIN1	 129
// 	For CS_MAC_CE
#define xlat_LATIN2_TO_MAC_CE 		 130
#define xlat_MAC_CE_TO_LATIN2		 131
#define xlat_MAC_CE_TO_CP_1250		 (((CS_MAC_CE & 0xff) << 8 ) | (CS_CP_1250 & 0xff))
#define xlat_CP_1250_TO_MAC_CE		 (((CS_CP_1251 & 0xff) << 8 ) | (CS_MAC_CE & 0xff))

//	For CS_MAC_CYRILLIC
#define xlat_MAC_CYRILLIC_TO_CP_1251	(((CS_MAC_CYRILLIC & 0xff) << 8 ) | (CS_CP_1251 & 0xff))
#define xlat_CP_1251_TO_MAC_CYRILLIC	(((CS_CP_1251 & 0xff) << 8 ) | (CS_MAC_CYRILLIC & 0xff))
#define xlat_MAC_CYRILLIC_TO_8859_5		(((CS_MAC_CYRILLIC & 0xff) << 8 ) | (CS_8859_5 & 0xff))
#define xlat_8859_5_TO_MAC_CYRILLIC		(((CS_8859_5 & 0xff) << 8 ) | (CS_MAC_CYRILLIC & 0xff))
#define xlat_MAC_CYRILLIC_TO_KOI8_R		(((CS_MAC_CYRILLIC & 0xff) << 8 ) | (CS_KOI8_R & 0xff))
#define xlat_KOI8_R_TO_MAC_CYRILLIC		(((CS_KOI8_R & 0xff) << 8 ) | (CS_MAC_CYRILLIC & 0xff))

//	For CS_MAC_GREEK
#define xlat_MAC_GREEK_TO_8859_7		(((CS_MAC_GREEK & 0xff) << 8 ) | (CS_8859_7 & 0xff))
#define xlat_8859_7_TO_MAC_GREEK		(((CS_8859_7 & 0xff) << 8 ) | (CS_MAC_GREEK & 0xff))
#define xlat_MAC_GREEK_TO_CP_1253		(((CS_MAC_GREEK & 0xff) << 8 ) | (CS_CP_1253 & 0xff))
#define xlat_CP_1253_TO_MAC_GREEK		(((CS_CP_1253 & 0xff) << 8 ) | (CS_MAC_GREEK & 0xff))

// for CS_MAC_TURKISH
#define xlat_MAC_TURKISH_TO_8859_9		 (((CS_MAC_TURKISH & 0xff) << 8 ) | (CS_8859_9 & 0xff))
#define xlat_8859_9_TO_MAC_TURKISH		 (((CS_8859_9 & 0xff) << 8 ) | (CS_MAC_TURKISH & 0xff))
#endif
