/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


#ifndef _XP_File_
#define _XP_File_

#include "xp_core.h"
#include "xp_path.h"

#define XP_FILE_NATIVE_PATH char *
#define XP_FILE_URL_PATH char*
/*-----------------------------------------------------------------------------
    XP_File.h
    Cross-Platform File API
    
	XP_File INTERFACE IS A COMMONLY MISUNDERSTOOD API.PLEASE READ THESE
	DOCS BEFORE USING THE API. The API is not self-documenting in any way,
	and broken in many.
    
	XP_File mimics the stdio file interface (fopen, fread & friends...). 
	The main difference is that to specify a file location, XP_File 
	uses a name in combination with an enum XP_FileType instead of full path.
	
		For example :
				stdio:		fopen(filename, permissions)
		maps to xp_file:	XP_FileOpen (name, XP_FileType type, XP_FilePerm permissions);
	
	Meaning of the name argument depends on the XP_FileType it is used with.
	
		For example:
		A)		XP_FileOpen(name, xpURL)
				name is the URL path to the file (/Drive/file.html)
		B)		XP_FileOpen(name, xpCache)
				name is the partial path from cache directory (expands to cache-directory/name)
		C)		XP_FileOpen(name, xpTemporary)
				name is the partial path from temporary directory (expands to cache-directory/name)

	Corollary: YOU CANNOT MIX AND MATCH NAMES AND TYPES
		For example:
				newTempFile = XP_TempName(name, xpTemporary);
				file =  XP_FileOpen(newTempFile, xpURL, "r+w");	// BAD!, you've just mixed xpURL and xpTemporary
		This is a very common error. It might work on some platforms, but it'll break others.
	
	FILE NAME MADNESS
	
	There are 3 basic ways to specify a file in the XP_File interface:

	TheXP_FileSpec
	
	char* - XP_FileType pair: 	used in most XP_File calls. the meaning of the name 
								depends on the enum. Common ones are:
								XP_FileType		/ 		name means:
								
								xpTemporary				partial path relative to temporary directory
								xpURL					full path in the standard URL format (file://)
								xpGlobalHistory			name is ignored. 

	XP_FILE_NATIVE_PATH

	char *						used in platform-specific code, platform-specific full path
								Windows:	"C:\Windows\regedit.exe"
								X:			"/h/users/atotic/home.html"
								Mac:		"Macintosh HD:System Folder:System"
	
	XP_FILE_URL_PATH					used in cross-platform code, when you need to manipulate the full path.
								It is a full path in the standard URL format: (file://<path>, but just the <path> part)
	
	XP_FILE_NATIVE_PATH and XP_FILE_URL_PATH are often confused
	
	To convert between these types:

	Conversion:		TheXP_FileSpec	-> XP_FILE_NATIVE_PATH		
	Call:			WH_FileName(name, type)
	Example:		WH_FileName("myName", xpTemporary)	-> "/u/tmp/myName" on Unix
														-> "C:\tmp\myName" on Windows
														-> "Mac HD:Temporary folder:myName" on Mac
	
	Conversion:		XP_FILE_NATIVE_PATH -> XP_FILE_URL_PATH
	Call:			XP_PlatformFileToURL(name)
	Example:		Windows:	XP_PlatformFileToURL("C:\tmp\myName")	-> "file:///C|/tmp/myName"
					Unix: 		XP_PlatformFileToURL(/u/tmp/myName")	-> "file:///u/tmp/myName"
					Mac:	XP_PlatformFileToURL("Mac HD:Temporary folder:myName")	-> "file:///Mac%20HD/Temporary%20folder/myName"
	
	You cannot convert anything into arbitrary TheXP_FileSpec, but you can use the XP_FILE_URL_PATH 
	in combination with xpURL enum.
	Example:		XP_FileOpen("C|/tmp/myName", xpURL) works
	
	COMMONLY USED CALLS NOT IN STDIO:

	WH_FileName(name, XP_FileType) - maps TheXP_FileSpec pair to XP_FILE_NATIVE_PATH.
	Use it when you need access to full paths in platform-specific code. For example:
		For example:
			cacheName = XP_FileName("", xpCacheFAT);
			fopen(cacheName);

	
	EXTENSIONS TO STDIO API:
	
	- XP_FileRename also moves files accross file systems (copy + delete)

	
	MISC API NOTES:
	
	The names of most of the calls are derived by prepending stdio call name
	with XP_File. (open -> XP_FileOpen, opendir -> XP_FileOpenDir). But warren
	was fixing up some bugs near the end of 3.0, and in his infinite wisdom renamed
	a few of them to WH_ instead of XP_. So, XP_FileName became WH_FileName. WH_ stands
	for Warren Harris. Whatever...
	
	Calls are not documented well. Most of the docs are in this header file. You 
	can usually get a hint about what the call should do by looking up the man
	pages of the equivalent stdio call.

	OLD DOCS (May mislead you, read at your peril!):
    Macintosh file system is not organized in the same hierarchy as UNIX and PC
    ':' is a separator character instead of '/', '~', '/../' constructs do not
    work.  Basically, you cannot easily open a file using a pathname.
    Because different types of files need to be stored in different 
    directories, the XP_FileOpen API takes a file type and file name as an
	argument.  Semantics of how file is opened vary with type.
    
    "Personal" files are stored in the user's "home" directory (~ on unix,
        launch point or system folder on Mac, dunno about Windows). File names
        may be ignored since there's no reason for multiple personal files.
    "Temporary" files are stored in /tmp or the equivalent
    "URL" is the only type that allows access to anywhere on the disk

    File Type           File name                 Semantics
    xpURL               url part after file:///   Platform specific
                        without #aklsdf
    xpGlobalHistory     -                         Opens personal global history
    xpKeyChain          -                         Opens personal key chain
    xpUserPrefs         -                         Opens personal prefs file
	xpJSMailFilters		-						  Opens personal js mail filters
    xpJSHTMLFilters	-			  Opens personal js HTML filters
    xpCache             name without path         Opens a cache file
	xpExtCache          Fully qualified path      Opens an external cache file
    xpTemporary         local filename            Opens a temporary file
    xpNewsrc            -                         Opens a .newsrc file
    xpSignature			-						  Opens a .signature file
    xpCertDB		-			  Opens cert DB
    xpCertDBNameIDX	-			  Opens cert name index
    xpKeyDB			-			  Opens key DB
    xpSignedAppletDB	-		  Signed applets DB filename
	xpJSCookieFilters   -         Opens personal js cookie filters

-----------------------------------------------------------------------------*/

#if defined(XP_UNIX) || defined(XP_BEOS)
# include <dirent.h>
# include <sys/stat.h>
#endif /* XP_UNIX */

#ifdef XP_MAC
# ifndef _UNIX
#  include <unix.h>
# endif
# include <Types.h>
# include <Files.h>
#endif /* XP_MAC */

#ifdef XP_WIN
# include "winfile.h"
#endif /* XP_WIN */

#ifdef XP_OS2
# include "dirent.h"
# include "sys/stat.h"
#endif /* XP_OS2 */

/*-----------------------------------------------------------------------------
	Types - NOT prototypes
	Protoypes go further down.
-----------------------------------------------------------------------------*/

typedef enum XP_FileType {
	xpAddrBookNew,
	xpAddrBook,
    xpUserPrefs, 
    xpKeyChain, 
    xpGlobalHistory, 
    xpGlobalHistoryList,
    xpHotlist,
    xpTemporary, 
    xpURL,
    xpCache,
	xpSARCache,				/* larubbio location independent cache */
    xpMimeTypes,
    xpCacheFAT,
	xpSARCacheIndex,			/* larubbio location independent cache */
    xpNewsRC,
    xpSNewsRC,
    xpTemporaryNewsRC,
    xpNewsgroups,
    xpSNewsgroups,
	xpNewsHostDatabase,
    xpHTTPCookie,
    xpProxyConfig,
    xpSocksConfig,
    xpSignature,
	xpNewsrcFileMap,
	xpFileToPost,
	xpMailFolder,
	xpMailFolderSummary,
	xpJSMailFilters,
	xpJSHTMLFilters,
	xpMailSort,					/* File that specifies which mail folders
								   to file which messages. */
	xpNewsSort,
	xpMailFilterLog,			/* log files for mail/news filters */
	xpNewsFilterLog,			
	xpMailPopState,
	xpMailSubdirectory,
	xpBookmarks,
    xpCertDB,
    xpCertDBNameIDX,
    xpKeyDB,
	xpSecModuleDB,
	xpExtCache,
	xpExtCacheIndex,				/* index of external indexes */
    xpRegistry,

	xpXoverCache,				/* Cache of XOVER files.  This
								   filename always takes the form
								   "hostname/groupname" (except for calls
								   to XP_MakeDirectory, where it is just
								   "hostname").*/
    xpEditColorScheme,			/* ifdef EDITOR   Families of "good" color combinations */
    xpJPEGFile,                 /* used for pictures in LDAP directory */
	xpVCardFile,				/* used for versit vCards */
	xpLDIFFile,					/* used for LDIF (LDAP Interchange format files */
	xpImapRootDirectory,
	xpImapServerDirectory,
	xpHTMLAddressBook,			/* old (HTML) address book */
	xpSignedAppletDB,			/* filename of signed applets DB */
	xpCryptoPolicy,             /* Export Policy control file */
	xpFolderCache,               /* for caching mail/news folder info */
    xpPKCS12File,		/* used for PKCS 12 certificate transport */
	xpJSCookieFilters,			/* Opens personal js cookie filters */
#if defined(CookieManagement)        
	xpHTTPCookiePermission,
#endif
#if defined(SingleSignon)
	xpHTTPSingleSignon,
#endif
    xpLIClientDB,
	xpLIPrefs,
	xpJSConfig,                  /* Javascript 'jsc' config cache file */
	xpSecurityModule	     /* security loadable module */
} XP_FileType;


#define XP_FILE_READ             "r"
#define XP_FILE_READ_BIN         "rb"
#define XP_FILE_WRITE            "w"
#define XP_FILE_WRITE_BIN        "wb"
#define XP_FILE_APPEND           "a"
#define XP_FILE_APPEND_BIN       "ab"
#define XP_FILE_UPDATE           "r+"
#define XP_FILE_TRUNCATE         "w+"
#ifdef SUNOS4
/* XXX SunOS4 hack -- make this universal by using r+b and w+b */
#define XP_FILE_UPDATE_BIN       "r+"
#define XP_FILE_TRUNCATE_BIN     "w+"
#else
#define XP_FILE_UPDATE_BIN       "rb+"
#define XP_FILE_TRUNCATE_BIN     "wb+"
#endif

#ifdef __BORLANDC__
	#define _stat stat
#endif
#ifdef XP_MAC
#define XP_STAT_READONLY( statinfo ) (0)
#else
#define XP_STAT_READONLY( statinfo ) ((statinfo.st_mode & S_IWRITE) == 0)
#endif


typedef FILE          * XP_File;
typedef char          * XP_FilePerm;

#ifdef XP_WIN
 typedef struct _stat   XP_StatStruct;
#endif

#if defined(XP_OS2)
 typedef struct stat    XP_StatStruct;
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
 typedef struct stat    XP_StatStruct;
#endif

#if defined (XP_MAC)
 typedef struct stat	XP_StatStruct;
#endif

#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)
 typedef DIR          * XP_Dir;
 typedef struct dirent  XP_DirEntryStruct;
#endif

#ifdef XP_MAC
	/* Mac has non-stdio definitions of XP_Dir and XP_DirEntryStruct */

typedef struct macdstat {
	char d_name[300];
} XP_DirEntryStruct;

typedef struct dirstruct {
	XP_DirEntryStruct dirent;
	FSSpec dirSpecs;
	short index;	/* Index for looping in XP_OpenDir */
	XP_FileType type;
}	*XP_Dir;

#endif /* XP_MAC */


/*-----------------------------------------------------------------------------
	Prototypes
-----------------------------------------------------------------------------*/

XP_BEGIN_PROTOS

/*
 * Given TheXP_FileSpec returns XP_FILE_NATIVE_PATH.
 * See docs on top for more info
 */
PUBLIC XP_FILE_NATIVE_PATH WH_FileName (const char *name, XP_FileType type);

/* 
 * Given XP_FILE_URL_PATH, returns XP_FILE_NATIVE_PATH
 * See docs on top for more info
 */
PUBLIC XP_FILE_NATIVE_PATH WH_FilePlatformName(const char * name);

/* 
 * Given XP_FILE_NATIVE_PATH, returns XP_FILE_URL_PATH
 * See docs on top for more info
 */
PUBLIC XP_FILE_URL_PATH XP_PlatformFileToURL (const XP_FILE_NATIVE_PATH platformName);

/* takes a portion of a local path and returns an allocated portion
 * of an XP path. This function was initially created to translate 
 * imap server folder names to xp names.
 */
PUBLIC char *XP_PlatformPartialPathToXPPartialPath (const char *platformPath);

/* Returns a pathname of a file suitable for use as temporary storage
 * Warning: you can only use the returned pathname in other XP_File calls
 * with the same enum.
 */
extern char * WH_TempName(XP_FileType type, const char * prefix);

/* Returns the path to the temp directory. */
extern char* XP_TempDirName(void);

/* Various other wrappers for stdio.h
 */
extern XP_File XP_FileOpen (const char* name, XP_FileType type,
							const XP_FilePerm permissions);

extern int XP_Stat(const char * name, XP_StatStruct * outStat,
				   XP_FileType type);

extern XP_Dir XP_OpenDir(const char * name, XP_FileType type);

extern void XP_CloseDir(XP_Dir dir);

extern XP_DirEntryStruct *XP_ReadDir(XP_Dir dir);

extern int XP_FileRename(const char * from, XP_FileType fromtype,
						 const char * to,   XP_FileType totype);

extern int XP_FileRemove(const char * name, XP_FileType type);

extern int XP_MakeDirectory(const char* name, XP_FileType type);

/* XP_MakeDirectoryR recursively creates all the directories needed */
extern int XP_MakeDirectoryR(const char* name, XP_FileType type);

extern int XP_RemoveDirectory(const char *name, XP_FileType type);

extern int XP_RemoveDirectoryRecursive(const char *name, XP_FileType type);

/* Truncate a file to be a given length.  It is important (especially on the
   Mac) to make sure not to ever call this if you have the file opened. */
extern int XP_FileTruncate(const char* name, XP_FileType type, int32 length);

extern int XP_FileWrite (const void* source, int32 count, XP_File file);

extern char * XP_FileReadLine(char * dest, int32 bufferSize, XP_File file);

extern long XP_FileTell( XP_File file );

extern int XP_FileFlush(  XP_File file );

extern int XP_FileClose(XP_File file);

/* Defines */

#define XP_Fileno fileno
#define XP_FileSeek(file,offset,whence) fseek ((file), (offset), (whence))
#define XP_FileRead(dest,count,file) fread ((dest), 1, (count), (file))
#define XP_FileTell(file) ftell(file)
#define XP_FileFlush(file) fflush(file)
/* varargs make it impossible to do any other way */
#define XP_FilePrintf fprintf

/* XP_GetNewsRCFiles returns a null terminated array of pointers to malloc'd
 * filename's.  Each filename represents a different newsrc file.
 *
 * return only the filename since the path is not needed or wanted.
 *
 * Netlib is expecting a string of the form:
 *  [s]newsrc-host.name.domain[:port]
 *
 * examples:
 *    newsrc-news.mcom.com
 *    snewsrc-flop.mcom.com
 *    newsrc-news.mcom.com:118
 *    snewsrc-flop.mcom.com:1191
 *
 * the port number is optional and should only be
 * there when different from the default.
 * the "s" in front of newsrc signifies that
 * security is to be used.
 *
 * return NULL on critical error or no files
 */
extern char ** XP_GetNewsRCFiles(void);


/* #ifdef EDITOR */
/* If pszLocalName is not NULL, we return the full pathname
 *   in local platform syntax. Caller must free this string.
 * Returns TRUE if file already exists
 * Windows version implemented in winfe\fegui.cpp
 */
extern Bool XP_ConvertUrlToLocalFile(const char * szURL, char **pszLocalName);

/* Construct a temporary filename in same directory as supplied "file:///" type URL
 * Used as write destination for saving edited document
 * User must free string.
 * Windows version implemented in winfe\fegui.cpp
 */ 
extern char * XP_BackupFileName( const char * szURL );

extern XP_Bool XP_FileIsFullPath(const char * name);

extern XP_Bool XP_FileNameContainsBadChars(const char *name);

#ifdef XP_MAC

/* XP_FileNumberOfFilesInDirectory returns the number of files in the specified
 * directory
 */
extern int XP_FileNumberOfFilesInDirectory(const char * dir_name, XP_FileType type);

#endif /* XP_MAC */

XP_END_PROTOS



#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)

/* Unix and Windows preferences communicate with the netlib through these
   global variables.  Netlib does "something sensible" if they are NULL.
 */
extern char *FE_SARCacheDir;
extern char *FE_UserNewsHost;
extern char *FE_UserNewsRC;
extern char *FE_TempDir;
extern char *FE_CacheDir;
extern char *FE_DownloadDir;
extern char *FE_GlobalHist;

#endif /* XP_UNIX || XP_WIN */


/* Each of the three patforms seem to have subtly different opinions
   about which functions should be aliases for their stdio counterparts,
   and which should be real functions.

   Note that on the platform in question, these #defines will override
   the prototypes above.
 */

#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)
# define XP_FileReadLine(destBuffer, bufferSize, file) \
	fgets(destBuffer, bufferSize, file)
#endif /* XP_UNIX || XP_WIN */

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
# define XP_ReadDir(DirPtr) readdir((DirPtr))
# define XP_CloseDir(DirPtr) closedir((DirPtr))
#endif /* XP_UNIX */

#if defined(XP_MAC) || defined(XP_WIN) || defined(XP_OS2)
 /* #### Note! This defn is dangerous because `count' is evaluated twice! */
# define XP_FileWrite(source, count, file)	\
	fwrite((void*)(source), 1, \
		   (size_t) ((count) == -1 ? strlen((char*)source) : (count)), \
		   (file))

# define XP_FileClose(file) fclose ((file))

#endif /* XP_MAC || XP_WIN */

#endif /* _XP_File_ */
