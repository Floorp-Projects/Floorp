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

#pragma once

#include <Printing.h>
#include <LFile.h>
#include <LListener.h>

#include "umimemap.h"
#include "prtypes.h"
// Constraints
#define SECONDS_PER_DAY		86400L
#define	CONNECTIONS_MIN		1L
#define CONNECTIONS_MAX		4L
#define BUFFER_SCALE		1024
#define BUFFER_MIN			1L * BUFFER_SCALE
#define BUFFER_MAX			32L * BUFFER_SCALE
#define DISK_CACHE_MIN		0L
#define DISK_CACHE_SCALE	1024L
#define FONT_SIZE_MIN		6L
#define FONT_SIZE_MAX		128L
#define EXPIRE_MIN			1L
#define EXPIRE_MAX			365L
#define NEWS_ARTICLES_MIN	10L
#define NEWS_ARTICLES_MAX	3500L

#define NO_SIGNATURE_FILE		0L

/*****************************************************************************
 * class CFolderSpec
 * Contains specs for the folder and a prototype spec for the file to be 
 * created inside this folder
 * All the setting of the specs should be done through access routines.\
 * Has the smarts to default to the proper folder if setting fails.
 *****************************************************************************/
struct CFolderSpec
{
public:
					CFolderSpec();
	// ¥ sets the specs for this folder
	OSErr			SetFolderSpec(FSSpec newSpec, int folderID);

	// ¥ gets the specs of the folder
	FSSpec			GetFolderSpec() {return fFolder;}

	// ¥ gets a prototype for the file inside this folder
	FSSpec			GetFilePrototype() {return fFilePrototype;}

	Boolean			Exists();
private:
	FSSpec			fFolder;		// Specs of the folder	(vol, parent, + folder name)
	FSSpec			fFilePrototype;	// Prototype specs for the file to be created inside this folder
};

// ¥ CCharSet is a struct for saving the font and size information associated
//		with a character set encoding

struct CCharSet
{
public:
	CStr31			fEncodingName;
	CStr31			fPropFont;
	CStr31			fFixedFont; 
	unsigned short	fPropFontSize;
	unsigned short	fFixedFontSize;
	unsigned short	fCSID;
	unsigned short  fFallbackFontScriptID;
	unsigned short	fTxtrButtonResID;
	unsigned short	fTxtrTextFieldResID;	
	unsigned short	fPropFontNum;
	unsigned short	fFixedFontNum;
};
typedef struct CChrarSet  CChrarSet;
struct CCharSetInRsrc
{
public:
	CStr31			fEncodingName;
	CStr31			fPropFont;
	CStr31			fFixedFont; 
	unsigned short	fPropFontSize;
	unsigned short	fFixedFontSize;
	unsigned short	fCSID;
	unsigned short  fFallbackFontScriptID;
	unsigned short	fTxtrButtonResID;
	unsigned short	fTxtrTextFieldResID;	
};
typedef struct CChrarSetInRsrc  CChrarSetInRsrc;

/*********************************************************************************
 * DOGBERT XP PREFERENCES / MULTI-USER PROFILES
 *
 * New preference file format: XP text file parsed by JavaScript via libpref
 *	(includes all user strings, bools, ints, and colors).
 * Some prefs (see below) are still written in old resource format.
 * Both old and new prefs are read & written to the existing Netscape Prefs file.
 *
 * Primary changes to this file:
 *	- added GetUserProfile() and InitPrefsFolder() to prompt user for desired
 *	  profile (if necessary) and use that to point to base Preferences folder.
 *	- changed order of PrefEnum entries and added XP prefName strings.
 *	- ReadPreference, if it detects new format, only needs to call 
 *	  PostProcessPrefChange for each pref to propagate the new value.
 *	  Calls to back-end will GO AWAY as modules fetch their own pref values.
 *	- ReadPreference otherwise reads old-format file.
 *	- WritePreference always writes new format.
 *	- CPrefs Get/Setters use libpref calls.
 *	- removed CPrefs storage of strings, bools, ints, and colors.
 *	- file/folder aliases accessed as libpref binary types but still stored
 *	  in CPrefs via sFileIDs.
 *
 * Some prefs are not yet supported in XP format.  
 *	- mime types
 *	- font encodings
 *	- window/pane positions
 *	- protocol handlers, etc.
 * I have the first 3 partly prototyped.  We at least need to support the first 
 * two in XP format for use by Admin Kit.
 *********************************************************************************/

/*********************************************************************************
 * class CPrefs
 * Static class that does all preferences related tasks
 * - reading/writing from a file
 * - initialization
 * - prefs variable access
 *********************************************************************************/
#define PREF_REVISION_NUMBER 11
class CPrefs	{
public:

 enum { msg_PrefsChanged = 'PrCh' };
 
 enum PrefEnum {
/*****************************************************************************
 * The law of enumeration
 * - Each preference kind must be contiguous
 *			This is so that we can use offsets to return the right value in GetBoolean/GetString...
 * - Every preference whose enum is defined must have its resource locations defined
 * - NEW: The entries below MUST be in the same order as the PrefLoc struct -
 *****************************************************************************/
// Folders/Files
	DownloadFolder,			// Where do we store downloads
	NetscapeFolder,			// Where the application is
	MainFolder,				// Current user's preferences folder (Netscape Ä or Users:)
	UsersFolder,			// Netscape Users folder (contains User Profiles)
	DiskCacheFolder,		// Disk cache files (inside Mozilla Ä)		
	SignatureFile,			// users signature file		
	GIFBackdropFile,		// file to use as the backdrop
	MailFolder,				// Mail folder
	NewsFolder,				// News folder
	SecurityFolder,			// Security folder
	MailCCFile,				// FCC for mail
	NewsCCFile,				// Fcc for news
	HTMLEditor,             // application to use to edit raw HTML (used in EDITOR)
	ImageEditor,            // application to use to edit images (used in EDITOR)
	RequiredGutsFolder,		// folder containing the required items for 
	SARCacheFolder,                     // Constellation Location-Independent Cache Directory TJ & ROB
	NetHelpFolder,          // Folder for NetHelp documents  EA
// Strings
	HomePage,			// Home page. strlen of 0 means do not load anything
	NewsHost,				// News host
	FTPProxy,				// Proxies
	GopherProxy,
	HTTPProxy,
	NewsProxy,
	WAISProxy,
	FTPProxyPort,
	GopherProxyPort,
	HTTPProxyPort,
	NewsProxyPort,
	WAISProxyPort,
	NoProxies,
	UserName,
	UserEmail,
	SMTPHost,
	SOCKSHost,
	SOCKSPort,
	Organization,
	SSLProxy,
	SSLProxyPort,
	PopHost,
	AcceptLanguage,			// string for HTTP AcceptLange header
	DefaultMailCC,
	DefaultNewsCC,
	ReplyTo,
	PopID,
	AutoProxyURL,			// URL for autoconfiguring proxy
	MailPassword,			// Mail password, encrypted
	Ciphers,				// ciphers that we know about
	EditorAuthor,			// Who should the author be for new editor documents (used in EDITOR)
	EditorNewDocTemplateLocation,	// When you create a new document from a template, where is the template (used in EDITOR)
	EditorBackgroundImage,	// what is the url for the background image for new editor documents (used in EDITOR)
	PublishLocation,		// location for one button publish (used in EDITOR)
	PublishBrowseLocation,	// location to test browse documents published with one button publish (used in EDITOR)
	PublishHistory0,
	PublishHistory1,
	PublishHistory2,
	PublishHistory3,
	PublishHistory4,
	PublishHistory5,
	PublishHistory6,
	PublishHistory7,
	PublishHistory8,
	PublishHistory9,
	DefaultPersonalCertificate,
// Booleans
	DelayImages,		// Auto load of images
	AnchorUnderline,		// Underline of anchors
	ShowAllNews,			// Show all news articles. NETLIB
	UseFancyFTP,			// Fancy FTP. NETLIB
	UseFancyNews,			// Fancy NNTP. NETLIB		
	ShowToolbar,			// Toolbar visible in main window?
	ShowStatus,				// Status visible in main window
	ShowURL,				// URL visible in main window
	LoadHomePage,			// Should we load home page for new
	ExpireNever,			// Do Links expire?
	DisplayWhileLoading,
	CustomLinkColors,		// do we want custom colors for links
	ShowDirectory,
	AgreedToLicense,		// has the user agreed to the license
	EnteringSecure,	
	LeavingSecure,
	ViewingMixed,
	SubmittingInsecure,
	ShowSecurity,
	CustomVisitedColors,	// custom unfollowed link colors?
	CustomTextColors,		// custom text colors?
	UseDocumentColors,		// use the document or user colors
	AutoDetectEncoding,		// auto detect the document encodings
	UseSigFile,				// should we use the signature file
	StrictlyMIME,			// should we always be MIME/quoted printable
	UseUtilityBackground,	// should we use utility background instead of stupid gray
	MailUseFixedWidthFont,	// Should mail messages be displayed with fixed width fonts?
	UseMailFCC,				// Should we Fcc for mail?
	UseNewsFCC,				// Should we Fcc for news?
	LimitMessageSize,		// Should we limit message size?
	LeaveMailOnServer,		// Leave mail on server?
	MailCCSelf,				// CC self on mail
	NewsCCSelf,				// CC self on news
	BiffOn,					// Check for mail
	UseMozPassword,			// Should we be using the password for Netscape
	ThreadMail,				// Thread mail?
	ThreadNews,				// Thread news?
	UseInlineViewSource,	// Should we use Lou's view source window?
	AutoQuoteOnReply,
	RememberMailPassword,	// Remember mail password?
	DefaultMailDelivery,
	EnableJavascript,		// Whether or not we allow JavaScript to execute
	ShowToolTips,
	UseInternetConfig,
	EnableJava,				// Whether or not we allow Java to execute
	AcceptCookies,
	UseEmailAsPassword,
	SubmitFormsByEmail,
	AllowSSLDiskCaching,
	EnableSSLv2,
	EnableSSLv3,
	EnableActiveScrolling,	// Dynamically update during thumb tracking
#ifdef FORTEZZA
	FortezzaTimeoutOn,	// Should fortezza time out
#endif
#ifdef EDITOR
	EditorUseCustomColors,	// Should new editor documents be set up to use custom colors (used in EDITOR)
	EditorUseBackgroundImage,	// Should new editor documents be set up to use a background image (used in EDITOR)
	PublishMaintainLinks,	// Maintain links when publishing a document (used in EDITOR)
	PublishKeepImages,		// Keep images with document when publishing it (used in EDITOR)
	ShowCopyright,			// Should we should the Copyright notice next time we "steal" something off the web (used in EDITOR)
	ShowFileEditToolbar,	// editor window toolbar
	ShowCharacterToolbar,	// editor window toolbar
	ShowParagraphToolbar,	// editor window toolbar
#endif // EDITOR
// Longs
	DiskCacheSize,			// Size of disk cache
	FileSortMethod,			// Method of sorting files. NETLIB
	ToolbarStyle,			// Toolbar display style
	DaysTilExpire,			// # days before links expire
	Connections,			// number of simultaneous connections to server
	BufferSize,				// size in K of network socket buffers
	PrintFlags,				// printing prefs
	NewsArticlesMax,		// maximum number of news articles to show
	CheckDocuments,			// when to check the network for document
	DefaultCharSetID,		// default character set ID
	DefaultFontEncoding,
	BackgroundColors,		// background color mode
	LicenseVersion,			// License version #
	MessageFontStyle,		// Equivalent to enum MSG_FONT in msgcom.h
	MessageFontSize,		// Equivalent to enum MSG_CITATION_SIZE in msgcom.h
	MaxMessageSize,			// Maximum message size for fetching
	TopLeftHeader,			// Top Left header for printing
	TopMidHeader,			// Top Mid header for printing
	TopRightHeader,			// Top Right header for printing
	BottomLeftFooter,		// Bottom Left footer for printing
	BottomMidFooter,		// Bottom Mid footer for printing
	BottomRightFooter,		// Bottom Right footer for printing
	PrintBackground,
	ProxyConfig,			// Status of proxy configuration enum NET_ProxyStyle
	StartupAsWhat,			// Mail/News/Browser/Visible
	StartupBitfield,		// What do we want to start up as if visible is set. Contains a bit set for every window we want. See BROWSER_WIN_ID in resgui.h
	BiffTimeout,			// Check mail how often
	AskMozPassword,			// Should we ask for the password (tri-value) 
	AskMozPassFrequency,	// The frequency
	SortMailBy,				// How do we sort mail. See SORT_BY defines in resgui.h
	SortNewsBy,				// How do we sort news
	NewsPaneConfig,			// News window pane configuration
	MailPaneConfig,			// Mail window pane configuration
	MailHeaderDisplay,
	NewsHeaderDisplay,
	AutoSaveTimeDelay,
#ifdef FORTEZZA
	FortezzaTimeout,	// When fortezza should time out
#endif
// Colors
	Black,
	White,
	Blue,
	Magenta,
	WindowBkgnd,
	Anchor,
	Visited, 
	TextFore,
	TextBkgnd,
    EditorText,                     // used to create new documents(used in EDITOR)
    EditorLink,                     // used to create new documents(used in EDITOR)
    EditorActiveLink,               // used to create new documents (used in EDITOR)
    EditorFollowedLink,             // used to create new documents (used in EDITOR)
    EditorBackground,               // used to create new documents (used in EDITOR)
    Citation,
// Print record
	PrintRecord,

// Termination
	LastPref
};


#define FIRSTPREF		DownloadFolder

#define FIRSTCOLOR		Black
#define FIRSTFOLDER		DownloadFolder
#define FIRSTLONG		DiskCacheSize
#define FIRSTBOOLEAN	DelayImages
#define FIRSTSTRING		HomePage
#define FIRSTFILESPEC	DownloadFolder

// ¥¥ Initialization, Reading Writing
	static void			Initialize();
	enum PrefErr	{	eAbort = -1,
						eNeedUpgrade,
						eOK,
						eRunAccountSetup	// ¥Êthis is totally scwewy
					};


	static PrefErr		DoRead(LFile * file, short fileType);
	static void			DoWrite();
	
	static void			SubscribeToPrefChanges( LListener *listener );
	static void			UnsubscribeToPrefChanges( LListener *listener);

private:
friend class CUserProfile;		// For ReadAllPreferences hack right now
	static void			ReadAllPreferences();
	static void			ReadPreference(short index);
	static void			InitializeUnsavedPref(short index);

	static void			WriteAllPreferences();
	static void			WritePreference(short index);
	
	static PrefErr		InitPrefsFolder(short fileType);
	static Boolean		FindRequiredGutsFolder(Boolean inLaunchWithPrefs);
	static void			OpenAnimationFile(FSSpec&, FSSpec&);
	static void			RegisterPrefCallbacks();
	static int			FSSpecPrefCallback(const char *prefString, void *enumValue);
	static int			ColorPrefCallback(const char *prefString, void *);

// ¥¥ÊUtility routines

public:
	// Put pref res file on top of resource chain. Does not open it
	static Boolean UsePreferencesResFile();
	// Put application res file on top
	static Boolean UseApplicationResFile();

// ¥¥ Access

public:
	static Boolean	GetBoolean(PrefEnum id);
	static Int32	GetLong(PrefEnum id);
	static CStr255	GetString(PrefEnum id);
	static char*	GetCharPtr( PrefEnum id );
	static const RGBColor& GetColor( PrefEnum id );	// --ML de-inlined

	static THPrint	GetPrintRecord();
	static FSSpec	GetFolderSpec( PrefEnum id );
	static FSSpec	GetFilePrototype(PrefEnum id);
	static char *	GetCachePath();
	
// Reading/writing of data associated with a window
	static Handle		ReadWindowData( short resID );
	static void			WriteWindowData( Handle data, short resID );


// ¥¥ Setting of preferences
	static void		SetModified();
	static Boolean	SetBoolean( Boolean value, PrefEnum id);
	static Boolean	SetLong( Int32 value, PrefEnum id);
	static Boolean	SetString( const char* newString, PrefEnum id);
	static Boolean	SetColor( const RGBColor& newColor, PrefEnum id);
	static Boolean	SetFolderSpec( const FSSpec& folderSpec, PrefEnum id);
	
	static Boolean	HasCoBrand();
	static Boolean	IsLocked( PrefEnum id );
	
	static FSSpec	GetTempFilePrototype(); // vRefNum and parID for a file in 'Temporary Items
	
	static Boolean	IsNewPrefFormat(short id);
	static char*	Concat(const char* base, const char* suffix);					

private:
	enum	{		kStaticStrCount = 8,
					kStaticStrLen = 256
			};
	static char*	GetStaticString();
	static void		PostInitializePref(PrefEnum id, Boolean changed);
private:
	static RGBColor * 	sColors;
	static CFolderSpec ** sFileIDs;
	static THPrint		sPrintRec;
	static char *		sCachePath;
	
	static	FSSpec		sTemporaryFolder;
	static 	Boolean		sRealTemporaryFolder;	// did FindFolder work ?

// ¥¥ MIME mappers
	// ¥ create a default MIME mapper for a given MIME type. doInsert inserts it into master
	//		MIME type list
private:
	static void			ReadMimeTypes();
	static void			Read1MimeTypes();

public:
	static CMimeMapper* CreateDefaultUnknownMapper( const CStr255& mimeType, Boolean doInsert );

	// ¥ like CreateDefaultUnknownMapper, except that it uses the application SIG
	static CMimeMapper* CreateDefaultAppMapper( FSSpec& fileSpec,	// application file
										const char* mimeType,		// mime type
										Boolean doInsert );
	// MIME types - created dynamically
	static CMimeList	sMimeTypes;		// List of CMimeMappers. CMimeList holds routines to manipulate them
	static	short		sPrefFileVersion;
	static Boolean		sViewSourceInline;

// ¥¥ Font encoding stuff	
	
	static void			ReadCharacterEncodings();
	static void			ReadXPFont(int16 csid, CCharSet* csFont);
	static Boolean		GetFont( UInt16 csid, CCharSet* font );
	static Boolean		GetFontAtIndex( unsigned long index, CCharSet* font );
	static int16 		CmdNumToWinCsid( int32 cmdNum);
	static int16 		CmdNumToDocCsid( int32 cmdNum);
	static int32 		WinCsidToCmdNum( int16 csid);

	static Boolean 		SetFont(const CStr255& EncodingName,const CStr255& PropFont,const CStr255& FixedFont,
						unsigned short PropFontSize, unsigned short FixedFontSize, unsigned short CSID, unsigned short FallbackFontScriptID, 
						unsigned short TxtrButtonResID, unsigned short TxtrTextFieldResID);
	static Int16 		GetButtonFontTextResIDs(unsigned short csid);
	static Int16 		GetTextFieldTextResIDs(unsigned short csid);
	static LArray*		GetCharSets() { return fCharSetFonts; }
	static short		GetProportionalFont(unsigned short csid);
	static short 		GetFixFont(unsigned short csid);
	static ScriptCode 	CsidToScript(int16 csid);

	static LArray*		fCustomLangList;
	static LArray*		fPreferLangList;

	static LArray*		fCharSetFonts;
	static CCharSet		fDefaultFont;
};

Boolean GetCharacterSet( LArray* fontSets, const CStr255& encodingName, CCharSet* font );
/*****************************************************************************
 * GLOBALS
 *****************************************************************************/

// ¥¥ File name constants. See custom.r for these strings, STR# 300
enum pref_Strings {	prefFolderName = 1
				,	prefFileName = 2
				,	globHistoryName = 3
				,	cacheFolderName = 4
				,	cacheLog = 5
				,	newsrc = 6
				,	bookmarkFile = 7
				,	mimeTypes = 8
				,	magicCookie = 9
				,	socksConfig = 10
				,	allNewsgroups = 11
				,	newsFileMap = 12 
				,	certDB = 13
				,	certDBNameIDX = 14
				,	mailFolderName = 15
				,	newsFolderName = 16
				,	securityFolderName = 17
				,	mailBoxExt = 18
				,	mailFilterFile = 19
				,	mailPopState = 20
				,	proxyConfig = 21
				,	keyDb = 22
				,	xoverCache = 23
				,	addressBookFile = 24
				,	mailCCfile = 25
				,	newsCCfile = 26
				,	extCacheFile = 27
				,	subdirExt = 28
				,	newsHostDatabase = 29
				,	configFile = 30
				,	userProfiles = 31
				,	mailFilterLog = 32
				,	theRegistry = 33
				,	prefRequiredGutsName = 34
                ,   sarCacheFolderName = 35
                ,   sarCacheIndex = 36
				,	ibm3270Folder = 37
				,	ibm3270File = 38
				,	htmlAddressBook = 39
				,	vCardExt = 40
				,	ldifExt1 = 41
				,	ldifExt2 = 42
				,	secModuleDb = 43
				,	usersFolderName = 44
				,	unknownAppName = 45
				,	aswName = 46
				,	nethelpFolderName = 47
				,	mailFolderCache = 48
				,	profileTemplateDir = 49
				,	cryptoPolicy = 50
				,	signedAppletDb = 51
				,	cookiePermissions = 52
				,	singleSignons = 53
				};
