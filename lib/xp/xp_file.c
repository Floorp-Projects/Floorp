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


#include "xp.h"
#include "plstr.h"
#include "prmon.h"
#include "rosetta.h"

#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"
#endif

#ifdef XP_UNIX
#include <fcntl.h>
#include "prefapi.h"
#endif

#ifdef XP_MAC
#include "xp_file_mac.h"
#endif
/* for ldap filenames - benjie
#define _XP_TMP_FILENAME_FOR_LDAP_ 
*/

XP_BEGIN_PROTOS


#if defined (XP_MAC) || defined(XP_UNIX)
/* Now that Mac is using stdio, we can share some of this.  Yay.
   Presumably Windows can share this too, but they keep these
   functions in the FE, so I don't know.
 */


int XP_Stat( const char* name, XP_StatStruct * outStat,  XP_FileType type )
{
	int		result = 0;
	
	char* newName = WH_FileName( name, type );
	if (!newName) return -1;
	result = stat( newName, outStat );
	XP_FREE(newName);
	return result;
}

XP_File XP_FileOpen( const char* name, XP_FileType type,
					 const XP_FilePerm permissions )
{
  char* newName = WH_FileName(name, type);
  XP_File result;
#ifdef XP_UNIX
  XP_StatStruct st;
  XP_Bool make_private_p = FALSE;
#endif

  /* One should never open newsrc for output directly. */
  XP_ASSERT (type != xpNewsRC || type != xpSNewsRC ||
			 !strchr (permissions, 'w'));

  if (newName == NULL)
	return NULL;

#ifdef XP_UNIX
  switch (type)
	{
	  /* These files are always private: if the user chmods them, we
		 make them -rw------ again the next time we use them, because
		 it's really important.
	   */
	case xpHTTPCookie:
#if defined(CookieManagement)
	case xpHTTPCookiePermission:
#endif
#if defined(SingleSignon)
	case xpHTTPSingleSignon:
#endif
	case xpKeyChain:
	case xpSignedAppletDB:
    case xpSARCache:
	case xpSARCacheIndex:		
	case xpCertDB:
	case xpCertDBNameIDX:
	case xpKeyDB:
	case xpSecModuleDB:
	  /* Always make tmp files private, because of their short lifetime. */
	case xpTemporary:
	case xpTemporaryNewsRC:
	case xpFileToPost:
	case xpPKCS12File:
	  if (strchr (permissions, 'w'))     /* opening for output */
		make_private_p = TRUE;
	  break;

	  /* These files are created private, but if the user then goes and
		 chmods them, we let their changes stick, because it's reasonable
		 for a user to decide to give away read permission on these
		 (though by default, it makes sense for them to be more private
		 than the user's other files.)
	   */
	case xpCacheFAT:
	case xpCache:
	case xpRegistry:
	case xpGlobalHistory:
	case xpHotlist:
	case xpBookmarks:
	case xpMailFolder:
	  if (strchr (permissions, 'w') &&   /* opening for output */
		  XP_Stat (newName, &st, type))  /* and it doesn't exist yet */
		make_private_p = TRUE;
	  break;


	case xpProxyConfig:
	case xpJSConfig:
	case xpSocksConfig:
	case xpNewsRC:
	case xpSNewsRC:
	case xpNewsgroups:
	case xpSNewsgroups:
	case xpURL:
	case xpMimeTypes:
	case xpSignature:
	case xpMailFolderSummary:
	case xpFolderCache:
	case xpJSMailFilters:
	case xpJSHTMLFilters:
	case xpMailSort:
	case xpNewsSort:
	case xpMailPopState:
	case xpMailFilterLog:
	case xpNewsFilterLog:
	case xpExtCache:
	case xpExtCacheIndex:
	case xpXoverCache:
	case xpMailSubdirectory:

	case xpVCardFile:
	case xpLDIFFile:
	case xpAddrBook:
	case xpAddrBookNew:
	case xpJSCookieFilters:
	  /* These files need not be more private than any other, so we
		 always create them with the default umask. */
	  break;
	default:
	  XP_ASSERT(0);
	}
#endif /* XP_UNIX */

#ifndef MCC_PROXY
  /* At this point, we'd better have an absolute path, because passing a
	 relative path to fopen() is undefined.  (For the client, at least -
	 I gather the proxy is different?) */
#endif/* !MCC_PROXY */

  result = fopen(newName, permissions);
  
#ifdef XP_MAC
 	if (result != 0)
 	{
 		int err;
 		
 		err = setvbuf(	result, 		/* file to buffer */
 						NULL,			/* allocate the buffer for us */
 						_IOFBF, 		/* fully buffer */
 						8 * 1024);		/* 8k buffer */
 						
		XP_ASSERT(err == 0); 						
 	}
#endif  

#ifdef XP_UNIX
  if (make_private_p && result)
#ifdef SCO_SV
	chmod (newName, S_IRUSR | S_IWUSR); /* rw only by owner */
#else
	fchmod (fileno (result), S_IRUSR | S_IWUSR); /* rw only by owner */
#endif
#endif /* XP_UNIX */

  XP_FREE(newName);
  return result; 
}

extern XP_Bool XP_FileNameContainsBadChars (const char *name)
{
#ifdef XP_MAC
	char *badChars = ":";
#else /*XP_UNIX*/
	char *badChars = "/";
#endif
	int i, j;
	for (i = 0; i < XP_STRLEN(name); i++)
		for (j = 0; j < XP_STRLEN(badChars); j++)
			if (name[i] == badChars[j])
				return TRUE;
	return FALSE;
}

#endif /* XP_MAC || XP_UNIX */

#ifdef XP_UNIX

/* This is just like fclose(), except that it does fflush() and fsync() first,
   to ensure that the bits have made it to the disk before we return.
 */
int XP_FileClose(XP_File file)
{
  int filedes;

  /* This busniess with `status' and `err' is an attempt to preserve the
	 very first non-0 errno we get, while still proceeding to close the file
	 if the fflush/fsync failed for some (weird?) reason. */
  int status, err = 0;
  XP_ASSERT(file);
  if (!file) return -1;
  status = fflush(file);              /* Push the stdio buffer to the disk. */
  if (status != 0) err = errno;

  filedes = XP_Fileno (file);
  /*
   * Only fsync when the file is not read-only, otherwise
   * HPUX and IRIX return an error, and Linux syncs the disk anyway.
   */
  if (fcntl(filedes, F_GETFL) != O_RDONLY) {
    status = fsync(filedes);   /* Wait for disk buffers to go out. */
    if (status != 0 && err == 0) err = errno;
  }

  status = fclose(file);              /* Now close it. */
  if (status != 0 && err == 0) err = errno;

  errno = err;
  return status ? status : err ? -1 : 0;
}

#endif /* XP_UNIX */


XP_END_PROTOS

#ifdef XP_UNIX

#include <unistd.h>		/* for getpid */

#ifdef TRACE
#define Debug 1
#define DO_TRACE(x) if (xp_file_debug) XP_TRACE(x)


int xp_file_debug = 0;
#else
#define DO_TRACE(x)
#endif

int XP_FileRemove(const char * name, XP_FileType type)
{
	char * newName = WH_FileName(name, type);
	int result;
	if (!newName) return -1;
	result = remove(newName);
	XP_FREE(newName);
	if (result != 0)
	{
	  /* #### Uh, what is this doing?  Of course errno is set! */
		int err = errno;
		if (err != 0)
			return -1;
	}
	
	return result;
}


int XP_FileRename(const char * from, XP_FileType fromtype,
				  const char * to, XP_FileType totype)
{
	char * fromName = WH_FileName(from, fromtype);
	char * toName = WH_FileName(to, totype);
	int res = 0;
	if (fromName && toName)
		res = rename(fromName, toName);
	else
		res = -1;
	if (fromName)
		XP_FREE(fromName);
	if (toName)
		XP_FREE(toName);
	return res;
}


/* Create a new directory */

int XP_MakeDirectory(const char* name, XP_FileType type)
{
  int result;
  static XP_Bool madeXoverDir = FALSE;
  mode_t mode;
  switch (type) {
  case xpMailFolder:
	mode = 0700;
	break;
  default:
	mode = 0755;
	break;
  }
  if (type == xpXoverCache) {
	/* Once per session, we'll check that the xovercache directory is
	   created before trying to make any subdirectories of it.  Sigh. ###*/
	if (!madeXoverDir) {
	  madeXoverDir = TRUE;
	  XP_MakeDirectory("", type);
	}
  }
#ifdef __linux
  {
	/* Linux is a chokes if the parent of the
	   named directory is a symbolic link. */
	char rp[MAXPATHLEN];
	char *s, *s0 = WH_FileName (name, type);
	if (!s0) return -1;

	/* realpath is advertised to return a char* (rp) on success, and on
	   failure, return 0, set errno, and leave a partial path in rp.
	   But on Linux 1.2.3, it doesn't -- it returns 0, leaves the result
	   in rp, and doesn't set errno.  So, if errno is unset, assume all
	   is well.
	 */
	rp[0] = 0;
	errno = 0;
	s = realpath (s0, rp);
	XP_FREE(s0);
	/* WTF??? if (errno) return -1; */
	if (!s) s = rp;
	if (!*s) return -1;
	result = mkdir (s, mode);
  }
#elif defined(SUNOS4) || defined(BSDI)
  {
	char rp[MAXPATHLEN];
	char *s = WH_FileName (name, type);
	char *tmp;
	if (!s) return -1;

	/* Take off all trailing slashes, because some systems (SunOS 4.1.2)
	   don't think mkdir("/tmp/x/") means mkdir("/tmp/x"), sigh... */
	PR_snprintf (rp, MAXPATHLEN-1, "%s", s);
	XP_FREE(s);
	while ((tmp = XP_STRRCHR(rp, '/')) && tmp[1] == '\0')
	  *tmp = '\0';

	result = mkdir (rp, mode);
  }
#else /* !__linux && !SUNOS4 */
  DO_TRACE(("XP_MakeDirectory called: creating: %s", name));
  {
	  char* filename = WH_FileName(name, type);
	  if (!filename) return -1;
	  result = mkdir(filename, mode);
	  XP_FREE(filename);
  }
#endif
  return result;
}

int XP_RemoveDirectory (const char *name, XP_FileType type)
{
  char *tmp = WH_FileName(name, type);
  int ret;
  if (!tmp) return -1;
  ret = rmdir(tmp);
  XP_FREE(tmp);
  return ret;
}

/*
** This function deletes a directory and everything under it.
** Deleting directories with "non-file" files, such as links,
** will produce behavior of XP_RemoveFile, since that's what is
** ultimately called.
**
** Return values: zero on failure, non-zero for success.
*/
int XP_RemoveDirectoryRecursive(const char *name, XP_FileType type)
{
	XP_DirEntryStruct *entry;
	XP_StatStruct     statbuf;
	int 	 	  status;
	char*		child;
	char* 		dot    = ".";
	char* 		dotdot = "..";
	int			ret = 1;

	XP_Dir dir = XP_OpenDir(name, type);  
	if (!dir) return 0;

	/*
	** Go through the directory entries and delete appropriately
	*/
	while ((entry = XP_ReadDir(dir)))
	{
		/*
		** Allocate space for current name + path separator + directory name  + NULL
		*/
		child = XP_ALLOC( strlen(name) + 2 + strlen(entry->d_name) );

		XP_STRCAT(child,"/");
		XP_STRCAT(child,entry->d_name);

		if (!(status = XP_Stat(child, &statbuf, type)))
		{
			if (entry->d_name == dot || entry->d_name == dotdot)
			{
				/* Do nothing, rmdir will clean these up */
			}
			else
				if (S_ISDIR(statbuf.st_mode))
				{
					/* Recursive call to clean out subdirectory */
					if (!XP_RemoveDirectoryRecursive(child, type))
						ret = 0;
				}
					else
					{
						/* Everything that's not a directory is a file! */
						if (XP_FileRemove(child, type) != 0)
							ret = 0;
					}
		}

		XP_FREE(child);
	}

	/* OK, remove the top-level directory if we can */
	if (XP_RemoveDirectory(name, type) != 0)
		ret = 0;

	XP_CloseDir(dir);

	return ret;
}


int XP_FileTruncate(const char* name, XP_FileType type, int32 length)
{
  char* filename = WH_FileName(name, type);
  int result = 0;
  if (!filename) return -1;
  result = truncate(filename, length);
  XP_FREE(filename);
  return result;
}



/* Writes to a file
 */
int XP_FileWrite (const void* source, int32 count, XP_File file)
{
    int32		realCount;

    if ( count < 0 )
        realCount = XP_STRLEN( source );
    else
        realCount = count;

	return( fwrite( source, 1, realCount, file ) );
}

/* The user can set these on the preferences dialogs; the front end
   informs us of them by setting these variables.  We do "something
   sensible" if 0...
   */
PUBLIC char *FE_UserNewsHost = 0;	/* clone of NET_NewsHost, mostly... */
PUBLIC char *FE_UserNewsRC = 0;
PUBLIC char *FE_TempDir = 0;
PUBLIC char *FE_CacheDir = 0;
PUBLIC char *FE_SARCacheDir = 0;
PUBLIC char *FE_GlobalHist = 0;


/* these should probably be promoted to FE_* exported functions */
/* but then other front-ends would (kind of) be required to     */
/* implement them.  Unix front ends will already have to        */
/* this or they will break--other front ends wouldn't have to   */
/* necessarily implement the FE_* versions because they'll      */
/* never be called.                                             */
extern char *fe_GetConfigDir(void);

static const char *
xp_unix_config_directory(char* buf)
{
  char *configdir = fe_GetConfigDir();
  strcpy(buf, configdir);
  free(configdir);

  return buf;
}

static int
xp_unix_sprintf_stat( char * buf,
                      const char * dirName,
                      const char * lang,
                      const char * fileName)
{
    int               result;
    int               len;
    XP_StatStruct     outStat;

    if (dirName == NULL || 0 == (len = strlen(dirName)) || len > 900)
	return -1;

    while (--len > 0 && dirName[len] == '/')
	/* do nothing */;		/* strip trailing slashes */
    strncpy(buf, dirName, ++len);
    buf[len++] = '/';
    buf[len]   =  0;
    if (lang != NULL && *lang == 0)
	lang  = NULL;
    if (lang != NULL) 
	sprintf(buf+len, "%.100s/%s", lang, fileName);
    else
	strcpy(buf+len, fileName);
    result = stat( buf, &outStat );
    if (result >= 0 && !S_ISREG(outStat.st_mode))
	result = -1;
    if (result < 0 && lang != NULL)
	result = xp_unix_sprintf_stat(buf, dirName, NULL, fileName);
    return result;
}

/* returns a unmalloced static string 
 * that is only available for temporary use.
 */
PUBLIC char *
xp_FileName (const char *name, XP_FileType type, char* buf, char* configBuf)
{
  const char *conf_dir = xp_unix_config_directory(configBuf);

  switch (type)
	{
	case xpSARCacheIndex:
	  {
	  	const char *sar_cache_dir = FE_SARCacheDir;
		if (!sar_cache_dir || !*sar_cache_dir)
		  sar_cache_dir = conf_dir;
		if (sar_cache_dir [strlen (sar_cache_dir) - 1] == '/')
		  sprintf (buf, "%.900sarchive.fat", sar_cache_dir);
		else
		  sprintf (buf, "%.900s/archive.fat", sar_cache_dir);

		name = buf;
		break;
	  }
	case xpSARCache:
	  {
		/* WH_TempName() returns relative pathnames for the cache files,
		   so that relative paths get written into the cacheFAT database.
		   WH_FileName() converts them to absolute if they aren't already
		   so that the logic of XP_FileOpen() and XP_FileRename() and who
		   knows what else is simpler.
		 */
                if (name != NULL && *name == '/')
                  break ;

                {
			char *tmp = FE_SARCacheDir;
			if (!tmp || !*tmp) tmp = "/tmp";
			if (tmp [strlen (tmp) - 1] == '/')
			  sprintf (buf, "%.500s%.500s", tmp, name);
			else
			  sprintf (buf, "%.500s/%.500s", tmp, name);
			name = buf;
		  }
		break;
	  }
	case xpCacheFAT:
	  {
	  	const char *cache_dir = FE_CacheDir;
		if (!cache_dir || !*cache_dir)
		  cache_dir = conf_dir;
		if (cache_dir [strlen (cache_dir) - 1] == '/')
		  sprintf (buf, "%.900sindex.db", cache_dir);
		else
		  sprintf (buf, "%.900s/index.db", cache_dir);

		name = buf;
		break;
	  }
	case xpCache:
	  {
		/* WH_TempName() returns relative pathnames for the cache files,
		   so that relative paths get written into the cacheFAT database.
		   WH_FileName() converts them to absolute if they aren't already
		   so that the logic of XP_FileOpen() and XP_FileRename() and who
		   knows what else is simpler.
		 */
		if (*name != '/')
		  {
			char *tmp = FE_CacheDir;
			if (!tmp || !*tmp) tmp = "/tmp";
			if (tmp [strlen (tmp) - 1] == '/')
			  sprintf (buf, "%.500s%.500s", tmp, name);
			else
			  sprintf (buf, "%.500s/%.500s", tmp, name);
			name = buf;
		  }
		break;
	  }

	case xpHTTPCookie:
	  {
#ifndef OLD_UNIX_FILES
		sprintf (buf, "%.900s/cookies", conf_dir);
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.900s/.netscape-cookies", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	  }
#if defined(CookieManagement)
	case xpHTTPCookiePermission:
	  {
#ifndef OLD_UNIX_FILES
                sprintf (buf, "%.900s/cookperm", conf_dir);
#else  /* OLD_UNIX_FILES */
                sprintf (buf, "%.900s/.netscape-cookperm", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	  }
#endif

#if defined(SingleSignon)
	case xpHTTPSingleSignon:
	  {
#ifndef OLD_UNIX_FILES
                sprintf (buf, "%.900s/signon", conf_dir);
#else  /* OLD_UNIX_FILES */
                sprintf (buf, "%.900s/.netscape-signon", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	  }
#endif
	case xpRegistry:
	  {
		if ( name == NULL || *name == '\0' ) {
#ifndef OLD_UNIX_FILES
			sprintf (buf, "%.900s/registry", conf_dir);
#else  /* OLD_UNIX_FILES */
			sprintf (buf, "%.900s/.netscape-registry", conf_dir);
#endif /* OLD_UNIX_FILES */
			name = buf;
		}
		else {
			XP_ASSERT( name[0] == '/' );
		}
		break;
	  }
	case xpProxyConfig:
	  {
		sprintf(buf, "%.900s/proxyconf", conf_dir);
		name = buf;
		break;
	  }
	case xpJSConfig:
	  {
		sprintf(buf, "%.900s/failover.jsc", conf_dir);
		name = buf;
		break;
	  }
	case xpTemporary:
	  {
		if (*name != '/')
		  {
			char *tmp = FE_TempDir;
			if (!tmp || !*tmp) tmp = "/tmp";
			if (tmp [strlen (tmp) - 1] == '/')
			  sprintf (buf, "%.500s%.500s", tmp, name);
			else
			  sprintf (buf, "%.500s/%.500s", tmp, name);
			name = buf;
		  }
		break;
	  }
	case xpNewsRC:
	case xpSNewsRC:
	case xpTemporaryNewsRC:
	  {
		/* In this case, `name' is "" or "host" or "host:port". */

		char *home = getenv ("HOME");
		const char *newsrc_dir = ((FE_UserNewsRC && *FE_UserNewsRC)
								  ? FE_UserNewsRC
								  : (home ? home : ""));
		const char *basename = (type == xpSNewsRC ? ".snewsrc" : ".newsrc");
		const char *suffix = (type == xpTemporaryNewsRC ? ".tmp" : "");
		if (*name)
		  sprintf (buf, "%.800s%.1s%.8s-%.128s%.4s",
				   newsrc_dir,
				   (newsrc_dir[XP_STRLEN(newsrc_dir)-1] == '/' ? "" : "/"),
				   basename, name, suffix);
		else
		  sprintf (buf, "%.800s%.1s%.128s%.4s",
				   newsrc_dir,
				   (newsrc_dir[XP_STRLEN(newsrc_dir)-1] == '/' ? "" : "/"),
				   basename, suffix);

		name = buf;
		break;
	  }
	case xpNewsgroups:
	case xpSNewsgroups:
	  {
#ifndef OLD_UNIX_FILES
		sprintf (buf, "%.800s/%snewsgroups-%.128s", 
				 conf_dir, 
				 type == xpSNewsgroups ? "s" : "", 
				 name);
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.800s/.netscape-%snewsgroups-%.128s", 
				 conf_dir, 
				 type == xpSNewsgroups ? "s" : "", 
				 name);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	  }

	case xpExtCacheIndex:
#ifndef OLD_UNIX_FILES
		sprintf (buf, "%.900s/cachelist", conf_dir);
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.900s/.netscape-cache-list", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;

	case xpGlobalHistory:
		name = FE_GlobalHist;
		break;

	case xpCertDB:
#ifndef OLD_UNIX_FILES
		if ( name ) {
			sprintf (buf, "%.900s/cert%s.db", conf_dir, name);
		} else {
			sprintf (buf, "%.900s/cert.db", conf_dir);
		}
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.900s/.netscape-certdb", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	case xpCertDBNameIDX:
#ifndef OLD_UNIX_FILES
		sprintf (buf, "%.900s/cert-nameidx.db", conf_dir);
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.900s/.netscape-certdb-nameidx", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	case xpKeyDB:
#ifndef OLD_UNIX_FILES
		if ( name ) {
			sprintf (buf, "%.900s/key%s.db", conf_dir, name);
		} else {
			sprintf (buf, "%.900s/key.db", conf_dir);
		}
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.900s/.netscape-keydb", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	case xpSecModuleDB:
		sprintf (buf, "%.900s/secmodule.db", conf_dir);
		name = buf;
		break;

	case xpSignedAppletDB:
	  {
#ifndef OLD_UNIX_FILES
		if ( name ) {
			sprintf (buf, "%.900s/signedapplet%s.db", conf_dir, name);
		} else {
			sprintf (buf, "%.900s./signedapplet.db", conf_dir);
		}
#else  /* OLD_UNIX_FILES */
		sprintf (buf, "%.900s/.netscape-signedappletdb", conf_dir);
#endif /* OLD_UNIX_FILES */
		name = buf;
		break;
	  }

	case xpFileToPost:
	case xpSignature:
	  	/* These are always absolute pathnames. 
		 * BUT, the user can type in whatever so
		 * we can't assert if it doesn't begin
		 * with a slash
		 */
		break;

	case xpExtCache:
	case xpKeyChain:
	case xpURL:
	case xpHotlist:
	case xpBookmarks:
	case xpMimeTypes:
	case xpSocksConfig:
	case xpMailFolder:
    case xpUserPrefs:
#ifdef BSDI
	    /* In bsdi, mkdir fails if the directory name is terminated
	     * with a '/'. - dp
	     */
	    if (name[strlen(name)-1] == '/') {
		strcpy(buf, name);
		buf[strlen(buf)-1] = '\0';
		name = buf;
	    }
#endif
#ifndef MCC_PROXY
	  /*
	   * These are always absolute pathnames for the Navigator.
	   * Only the proxy (servers) may have pathnames relative
	   * to their current working directory (the servers chdir
	   * to their admin/config directory on startup.
	   *
	   */
	  if (name) XP_ASSERT (name[0] == '/');
#endif	/* ! MCC_PROXY */

	  break;

	case xpMailFolderSummary:
	  /* Convert /a/b/c/foo to /a/b/c/.foo.summary (note leading dot) */
	  {
		const char *slash;
		slash = strrchr (name, '/');
		if (name) XP_ASSERT (name[0] == '/');
		XP_ASSERT (slash);
		if (!slash) return 0;
		XP_MEMCPY (buf, name, slash - name + 1);
		buf [slash - name + 1] = '.';
		XP_STRCPY (buf + (slash - name) + 2, slash + 1);
		XP_STRCAT (buf, ".summary");
		name = buf;
		break;
	  }

	case xpAddrBookNew:
	  /* Convert foo.db to /a/b/c/foo.db */
	  {
		if ( name ) {
			sprintf (buf, "%.900s/%s", conf_dir, name);
		} else {
			sprintf (buf, "%.900s/abook.nab", conf_dir);
		}
#if defined(DEBUG_tao)
		printf("\n  xpAddrBookNew, xp_FileName, buf=%s\n", buf);
#endif
		name = buf;

		break;
	  }

	case xpAddrBook:
	  /* Convert /a/b/c/foo to /a/b/c/foo.db (note leading dot) */
	  {
		  /* Tao_27jan97
		   */
		  char *dot = NULL;
		  int len = 0;
		  const char *base = NULL;

		  if (name) 
			  XP_ASSERT (name[0] == '/');

		  dot = XP_STRRCHR(name, '.');
		  if (dot) {

			  len = dot - name + 1;
			  PL_strncpyz(buf, name, len);
		  }/* if */

		  XP_STRCAT (buf, ".nab");


		  /* Tao_02jun97 don't convert addrbook.db
		   * reuse len, dot
		   */
		  base = XP_STRRCHR(name, '/');
		  if (base && *base == '/')
			  base++;			  

#if defined(DEBUG_tao)
		  printf("\n++++  xpAddrBook, before xp_FileName=%s\n", name);
#endif

		  if (!base || XP_STRCMP(base, "addrbook.db"))
			  /* not old addrbook.db file
			   */
			  name = buf;
#if defined(DEBUG_tao)
		  printf("\n  xpAddrBook, xp_FileName=%s\n", name);
#endif
		break;
	  }

	case xpVCardFile:
	  /* Convert /a/b/c/foo to /a/b/c/foo.vcf (note leading dot) */
	  {
#if 1
		  /* Tao_27jan97
		   */
		  char *dot = NULL;
		  int len = 0;

		  if (name) 
			  XP_ASSERT (name[0] == '/');

		  dot = XP_STRRCHR(name, '.');
		  if (dot) {
			  len = dot - name + 1;
			  PL_strncpyz(buf, name, len);
		  }/* if */

		  XP_STRCAT (buf, ".vcf");
		  name = buf;
#if defined(DEBUG_tao_)
		  printf("\n  xp_FileName=%s\n", name);
#endif
#else
		const char *slash;
		slash = strrchr (name, '/');
		if (name) XP_ASSERT (name[0] == '/');
		XP_ASSERT (slash);
		if (!slash) return 0;
		XP_MEMCPY (buf, name, slash - name + 1);
		XP_STRCAT (buf, ".vcf");
		name = buf;
#endif
		break;
	  }

	case xpLDIFFile:
	  /* Convert /a/b/c/foo to /a/b/c/foo.ldif (note leading dot) */
	  {
#if 1
		  /* Tao_27jan97
		   */
		  char *dot = NULL;
		  int len = 0;

		  if (name) 
			  XP_ASSERT (name[0] == '/');

		  dot = XP_STRRCHR(name, '.');
		  if (dot) {
			  len = dot - name + 1;
			  PL_strncpyz(buf, name, len);
		  }/* if */

		  XP_STRCAT (buf, ".ldif");
		  name = buf;
#if defined(DEBUG_tao_)
		  printf("\n  xp_FileName=%s\n", name);
#endif

#else
		const char *slash;
		slash = strrchr (name, '/');
		if (name) XP_ASSERT (name[0] == '/');
		XP_ASSERT (slash);
		if (!slash) return 0;
		XP_MEMCPY (buf, name, slash - name + 1);
		XP_STRCAT (buf, ".ldif");
		name = buf;
#endif
		break;
	  }

	case xpJSMailFilters:
	  sprintf(buf, "%.900s/filters.js", conf_dir);
	  name = buf;
	  break;

	case xpJSHTMLFilters:
	  sprintf(buf, "%.900s/hook.js", conf_dir);
	  name = buf;
	  break;

	case xpNewsSort:
		sprintf(buf, "%.800s/xover-cache/%.128snetscape-newsrule", conf_dir, name);
		break;

	case xpMailSort:
#ifndef OLD_UNIX_FILES
		if (name && strlen(name) > 0) 
			sprintf(buf, "%.900s/imap/%s/mailrule", conf_dir, name);
		else
			sprintf(buf, "%.900s/mailrule", conf_dir);
#else  /* OLD_UNIX_FILES */
	  sprintf(buf, "%.900s/.netscape-mailrule", conf_dir);
#endif /* OLD_UNIX_FILES */
	  name = buf;
	  break;
	case xpMailPopState:
#ifndef OLD_UNIX_FILES
	  sprintf(buf, "%.900s/popstate", conf_dir);
#else  /* OLD_UNIX_FILES */
	  sprintf(buf, "%.900s/.netscape-popstate", conf_dir);
#endif /* OLD_UNIX_FILES */
	  name = buf;
	  break;
	case xpMailFilterLog:
	  sprintf(buf, "%.900s/.netscape-mailfilterlog", conf_dir);
	  name = buf;
	  break;
	case xpNewsFilterLog:
	  sprintf(buf, "%.900s/.netscape-newsfilterlog", conf_dir);
	  name = buf;
	  break;
    case xpMailSubdirectory:
    {
     char * pEnd = strrchr(name, '/');
     strcpy(buf, name);

      /* strip off the extension */
       if(!pEnd)
        pEnd = buf;

      pEnd = strchr(pEnd, '.');
      if(pEnd)
        *pEnd = '\0';
      strcat(buf, ".sbd/");
      name = buf;
    }
    break;
	case xpXoverCache:
	  sprintf(buf, "%.800s/xover-cache/%.128s", conf_dir, name);
	  name = buf;
	  break;

	case xpNewsHostDatabase:
	  sprintf(buf, "%.800s/newsdb", conf_dir);
	  name = buf;
	  break;
          
#ifdef OLD_MOZ_MAIL_NEWS
	case xpImapRootDirectory:
	{
        char prefbuf[1024];
        int len = sizeof prefbuf / sizeof *prefbuf;
        if ((PREF_GetCharPref("mail.imap.root_dir",
                              prefbuf, &len) == PREF_NOERROR)
            && *prefbuf == '/')
	            /* guard against assert: line 806, file xp_file.c */
            PL_strncpyz(buf, prefbuf, len);
            /* Copy back to the buffer that was passed in.
             * We couldn't have PREF_GetCharPref() just put it there
             * initially because the size of buf wasn't passed in
             * (it happens to be 1024) and PREF_GetCharPref() insists
             * on having a size. Sigh.
             */
        else
        {
            char *home = getenv ("HOME");
            sprintf(buf, "%s/ns_imap", (home ? home : ""));
        }
		name = buf;
		break;
	}
        
	case xpImapServerDirectory:
	{
        char prefbuf[1024];
        int len = sizeof prefbuf / sizeof *prefbuf;
        if ((PREF_GetCharPref("mail.imap.root_dir",
                              prefbuf, &len) == PREF_NOERROR)
            && *prefbuf == '/')
	            /* guard against assert: line 806, file xp_file.c */
            sprintf(buf, "%s/%s", prefbuf, name);
        else
        {
            char *home = getenv ("HOME");
            sprintf(buf, "%s/ns_imap/%s", (home ? home : ""), name);
        }
		name = buf;
		break;
	}

	case xpFolderCache:
		sprintf (buf, "%s/summary.dat", conf_dir);
		name = buf;
		break;

#endif /* OLD_MOZ_MAIL_NEWS */

#ifdef MOZ_SECURITY
     case xpCryptoPolicy:
     {
         extern void fe_GetProgramDirectory(char *path, int len);
         char *policyFN = "moz40p3";
         char *mozHome  = getenv("MOZILLA_FIVE_HOME");
         char *lang     = getenv("LANG");
         char  dirName[1024];

         name = buf;
         if (!xp_unix_sprintf_stat(buf, conf_dir, lang, policyFN))
             break;
         if (!xp_unix_sprintf_stat(buf, mozHome, lang, policyFN))
             break;
         fe_GetProgramDirectory(dirName, sizeof dirName);
         if (!xp_unix_sprintf_stat(buf, dirName, lang, policyFN))
             break;

         /* couldn't find it, but must leave a valid file name in buf */
         sprintf(buf, "%.900s/%s", conf_dir, policyFN);
         break;
     }
#endif /* MOZ_SECURITY */

	case xpSecurityModule:
     	{
         extern void fe_GetProgramDirectory(char *path, int len);
#ifdef HPUX
         char *secModuleFN = "cmnav.sl";
#else
#ifdef SUNOS4
         char *secModuleFN = "cmnav.so.1.0";
#else
         char *secModuleFN = "cmnav.so";
#endif
#endif
    	HG06196
	break;
	}

	case xpPKCS12File:
	  /* Convert /a/b/c/foo to /a/b/c/foo.p12 (note leading dot) */
	  {
		  int len = 0;

		  if(name) {
			XP_ASSERT(name[0] == '/');

			/* only append if there is enough space in the buffer */	
			/* this should be changed if the size of the buf changes */
			if((XP_STRLEN(name) + 4) <= 1020) {

				/* include NULL in length */
				len = XP_STRLEN(name) + 1;
				PL_strncpyz(buf, name, len);

				/* we want to concatenate ".p12" if it is not the
				 * last 4 characters of the name already.  
				 * If len is less than 5 (including the terminating
				 * NULL), it is not ".p12".  
				 * If the len is > 5 it may have the .p12, so we want
				 * to check and only append .p12 if the name 
				 * specified does not end in .p12.
				 * only side effect -- this allows for the filename
				 * ".p12" which is fine.
				 */
				if((len >= 5) && PL_strcasecmp(&(name[len-4-1]), ".p12")) {
					XP_STRCAT(buf, ".p12");
				} else if(len < 5) {
					/* can't be ".p12", so we append ".p12" */
					XP_STRCAT(buf, ".p12");
				}
		        name = buf;
			}
		   }
		
		   break;
	  }

	case xpJSCookieFilters:
		{
			sprintf(buf, "%.900s/cookies.js", conf_dir);
			name = buf;
			break;
		}

	case xpLIPrefs:
		{
			sprintf(buf, "%.900s/liprefs.js", conf_dir);
			name = buf;
			break;
		}

	default:
	  abort ();
	}

  return (char *) name;
}

char * xp_FilePlatformName(const char * name, char* path)
{
	if ((name == NULL) || (XP_STRLEN(name) > 1000))
		return NULL;
	XP_STRCPY(path, name);
	return path;
}

char * XP_PlatformFileToURL (const char *name)
{
	char *prefix = "file://";
	char *retVal = XP_ALLOC (XP_STRLEN(name) + XP_STRLEN(prefix) + 1);
	if (retVal)
	{
		XP_STRCPY (retVal, "file://");
		XP_STRCAT (retVal, name);
	}
	return retVal;
}

char *XP_PlatformPartialPathToXPPartialPath(const char *platformPath)
{
	/* using the unix XP_PlatformFileToURL, there is no escaping! */
	return XP_STRDUP(platformPath);
}


#define CACHE_SUBDIRS

char*
XP_TempDirName(void)
{
	char *tmp = FE_TempDir;
	if (!tmp || !*tmp) tmp = "/tmp";
	return XP_STRDUP(tmp);
}

char *
xp_TempName (XP_FileType type, const char * prefix, char* buf, char* buf2, unsigned int *count)
{
#define NS_BUFFER_SIZE	1024
  char *value = buf;
  time_t now;

  *buf = 0;
  XP_ASSERT (type != xpTemporaryNewsRC);

  if (type == xpCache || type == xpSARCache)
	{
	  /* WH_TempName() must return relative pathnames for the cache files,
		 so that relative paths get written into the cacheFAT database,
		 making the directory relocatable.
	   */
	  *buf = 0;
	}
  else  if ( (type == xpURL) && prefix )
	{
	   if ( XP_STRRCHR(prefix, '/') )
           {
	      XP_StatStruct st;

	      XP_SPRINTF (buf, "%.500s", prefix);
	      if (XP_Stat (buf, &st, xpURL))
		    XP_MakeDirectoryR (buf, xpURL);
	      prefix = "su"; 
           }
	}
	else
	{
	  char *tmp = FE_TempDir;
	  if (!tmp || !*tmp) tmp = "/tmp";
	  XP_SPRINTF (buf, "%.500s", tmp);

	  if (!prefix || !*prefix)
		prefix = "tmp";
	}

  XP_ASSERT (!XP_STRCHR (prefix, '/'));
  if (*buf && buf[XP_STRLEN (buf)-1] != '/')
	XP_STRCAT (buf, "/");

  /* It's good to have the cache file names be pretty long, with a bunch of
	 inputs; this makes the variant part be 15 chars long, consisting of the
	 current time (in seconds) followed by a counter (to differentiate
	 documents opened in the same second) followed by the current pid (to
	 differentiate simultanious processes.)  This organization of the bits
	 has the effect that they are ordered the same lexicographically as by
	 creation time.

	 If name length was an issue we could cut the character count a lot by
	 printing them in base 72 [A-Za-z0-9@%-_=+.,~:].
   */
  now = time ((time_t *) 0);
  sprintf (buf2,
		   "%08X%03X%04X",
		   (unsigned int) now,
		   (unsigned int) *count,
		   (unsigned int) (getpid () & 0xFFFF));

  if (++(*count) > 4095) (*count) = 0; /* keep it 3 hex digits */

#ifdef CACHE_SUBDIRS
  if (type == xpCache || type == xpSARCache)
	{
	  XP_StatStruct st;
	  char *s;
	  char *tmp = (type == xpCache) ? FE_CacheDir : FE_SARCacheDir;
	  if (!tmp || !*tmp) tmp = "/tmp";
	  sprintf (buf, "%.500s", tmp);
	  if (buf [XP_STRLEN(buf)-1] != '/')
		XP_STRCAT (buf, "/");

	  s = buf + XP_STRLEN (buf);

	  value = s;		/* return a relative path! */

	  /* The name of the subdirectory is the bottom 5 bits of the time part,
		 in hex (giving a total of 32 directories.) */
	  sprintf (s, "%02X", (now & 0x1F));

	  if (XP_Stat (buf, &st, xpURL))		/* create the dir if necessary */
		XP_MakeDirectory (buf, type);

	  s[2] = '/';
	  s[3] = 0;
	}
#endif /* !CACHE_SUBDIRS */

  XP_STRNCAT (value, prefix, NS_BUFFER_SIZE - XP_STRLEN(value));
  XP_STRNCAT (value, buf2, NS_BUFFER_SIZE - XP_STRLEN(value));

  /* Tao
   */
  if (type == xpAddrBook) {
	  XP_STRNCAT (value, ".nab", NS_BUFFER_SIZE - XP_STRLEN(value));
  }/* if */

  value[NS_BUFFER_SIZE - 1] = '\0'; /* just in case */

  DO_TRACE(("WH_TempName called: returning: %s", value));

  return(value);
}


#ifdef OLD_MOZ_MAIL_NEWS
/* XP_GetNewsRCFiles returns a null terminated array
 * of pointers to malloc'd filename's.  Each filename
 * represents a different newsrc file.
 *
 * return only the filename since the path is not needed
 * or wanted.
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

#define NEWSRC_PREFIX ".newsrc"
#define SNEWSRC_PREFIX ".snewsrc"

#include "nslocks.h"
#include "prnetdb.h"

PUBLIC char **
XP_GetNewsRCFiles(void)
{
	XP_Dir dir_ptr;
	XP_DirEntryStruct *dir_entry;
	char **array;
	int count=0;
	char * dirname;
	char * name;
	int len, len2;
	
	/* assume that there will never be more than 256 hosts
	 */
#define MAX_NEWSRCS 256
	array = (char **) XP_ALLOC(sizeof(char *) * (MAX_NEWSRCS+1));

	if(!array)
		return(NULL);

	XP_MEMSET(array, 0, sizeof(char *) * (MAX_NEWSRCS+1));

    name = WH_FileName ("", xpNewsRC);

	/* truncate the last slash */
	dirname = XP_STRRCHR(name, '/');
	if(dirname)
		*dirname = '\0';
	dirname = name;

	if(!dirname)
		dirname = "";

	dir_ptr = XP_OpenDir(dirname, xpNewsRC);
	XP_FREE(name);
    if(dir_ptr == NULL)
      {
		XP_FREE(array);
		return(NULL);
      }

	len = XP_STRLEN(NEWSRC_PREFIX);
	len2 = XP_STRLEN(SNEWSRC_PREFIX);

	while((dir_entry = XP_ReadDir(dir_ptr)) != NULL
			&& count < MAX_NEWSRCS)
	  {
		char *name = dir_entry->d_name;
		char *suffix, *port;
		if (!XP_STRNCMP (name, NEWSRC_PREFIX, len))
		  suffix = name + len;
		else if (!XP_STRNCMP (name, SNEWSRC_PREFIX, len2))
		  suffix = name + len2;
		else
		  continue;

		if (suffix [strlen (suffix) - 1] == '~' ||
			suffix [strlen (suffix) - 1] == '#')
		  continue;

		if (*suffix != 0 && *suffix != '-')
		  /* accept ".newsrc" and ".newsrc-foo" but ignore
			 .newsrcSomethingElse" */
		  continue;

		if (*suffix &&
			(suffix[1] == 0 || suffix[1] == ':'))  /* reject ".newsrc-" */
		  continue;

		port = XP_STRCHR (suffix, ':');
		if (!port)
		  port = suffix + strlen (suffix);

		if (*port)
		  {
			/* If there is a "port" part, reject this file if it contains
			   non-numeric characters (meaning reject ".newsrc-host:99.tmp")
			 */
			char *s;
			for (s = port+1; *s; s++)
			  if (*s < '0' || *s > '9')
				break;
			if (*s)
			  continue;
		  }

		if (suffix != port)
		  {
			/* If there's a host name in the file, check that it is a
			   resolvable host name.

			   If this turns out to be a bad idea because of socks or
			   some other reason, then we should at least check that
			   it looks vaguely like a host name - no non-hostname
			   characters, and so on.
			 */
			char host [255];
			PRHostEnt *ok, hpbuf;
			char dbbuf[PR_NETDB_BUF_SIZE];
			if (port - suffix >= sizeof (host))
			  continue;
			strncpy (host, suffix + 1, port - suffix - 1);
			host [port - suffix - 1] = 0;

            if (PR_GetHostByName (host, dbbuf, sizeof(dbbuf), &hpbuf) == PR_SUCCESS)
			    ok = &hpbuf;
			else
			    ok = NULL;

			if (!ok)
			  continue;
		  }

		StrAllocCopy(array[count++], dir_entry->d_name+1);
	  }

	XP_CloseDir (dir_ptr);
	return(array);
}

#endif

XP_Dir XP_OpenDir(const char * name, XP_FileType type)
{
	XP_Dir dir = NULL;
	char *pathName = NULL;
	switch (type)
	{
	case xpMailSubdirectory:
		pathName = PR_smprintf ("%s%s", name, ".sbd");
		if (pathName)
		{
			dir = opendir (pathName);
			XP_FREEIF(pathName);
		}
		else
			dir = NULL;
		break;
	default:
		dir = opendir (name);
	}
	return dir;
}

#endif /* XP_UNIX */

#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2)

/*
 * Make all the directories specified in the path
 */
int XP_MakeDirectoryR(const char* name, XP_FileType type)
{
	char separator;
	int result = 0;
    int skipfirst;
	char * finalName;
	
#ifdef XP_WIN
	separator = '\\';
    skipfirst = 3;
#elif defined XP_UNIX
	separator = '/';
    skipfirst = 1;
#endif
	finalName = WH_FileName(name, type);
	if ( finalName )
	{
		char * dirPath;
		char * currentEnd;
		int err = 0;
		XP_StatStruct s;
        dirPath = XP_STRDUP( finalName ); /* XXX: why?! */
        if (dirPath == NULL) 
        {
            XP_FREE( finalName );
            return -1;
        }

#ifdef XP_WIN
        /* WH_FileName() must return a drive letter else skipfirst is bogus */
        XP_ASSERT( dirPath[1] == ':' );
#endif
        currentEnd = XP_STRCHR(dirPath + skipfirst, separator);
		/* Loop through every part of the directory path */
		while (currentEnd != 0)
		{
			char savedChar;
			savedChar = currentEnd[0];
			currentEnd[0] = 0;
			if ( XP_Stat(dirPath, &s, xpURL ) != 0)
				err = XP_MakeDirectory(dirPath, xpURL);
			if ( err != 0)
			{
				XP_ASSERT( FALSE );	/* Could not create the directory? */
				break;
			}
			currentEnd[0] = savedChar;
			currentEnd = XP_STRCHR( &currentEnd[1], separator);
		}
		if ( err == 0 )
		/* If the path is not terminated with / */
		{
			if ( dirPath[XP_STRLEN( dirPath) - 1] != separator )
				if ( XP_Stat(dirPath, &s, xpURL ) != 0)
					err = XP_MakeDirectory(dirPath, xpURL);
		}
		if ( 0 != err )
			result = err;
		if ( dirPath )
			XP_FREE( dirPath );
	}
	else
		result = -1;
	if ( finalName )
		XP_FREE( finalName );
	XP_ASSERT( result == 0 );	/* For debugging only */
	return result;
}

#endif /* XP_WIN || XP_UNIX || XP_OS2 */
/*
	If we go to some sort of system-wide canonical format, routines like this make sense.
	This is intended to work on our (mail's) canonical internal file name format.
	Currently, this looks like Unix format, e.g., "/directory/subdir".
	It is NOT intended to be used on native file name formats.
	The only current use is in spool.c, to determine if a rule folder
	is an absolute or relative path (relative to mail folder directory),
	so if this gets yanked, please fix spool.c  DMB 2/13/96

	spool.c has been retired. This routine has been adopted by lib/libmisc/dirprefs.c.
	This is only used for windows platform for now. jefft 7-24-97
*/
XP_Bool XP_FileIsFullPath(const char * name)
{
#ifdef XP_WIN
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	if (NULL == name) return FALSE;
	_splitpath(name, drive, dir, NULL, NULL);
	if (*drive != 0 || *dir == '\\') return TRUE;
	return FALSE;
#else
	XP_ASSERT(FALSE);
	/* return (*name == '/'); */
	return FALSE;
#endif
}

/******************************************************************************/
/* Thread-safe entry points: */

#if !defined(XP_WIN) && !defined(XP_OS2)

PRMonitor* _pr_TempName_lock = NULL;

PUBLIC char *
WH_FileName (const char *name, XP_FileType type)
{
	static char buf [1024];				/* protected by _pr_TempName_lock */
	static char configBuf [1024];		/* protected by _pr_TempName_lock */
	char* result;
	if (_pr_TempName_lock == NULL) {
		_pr_TempName_lock = PR_NewNamedMonitor("TempName-lock");
	}
	PR_EnterMonitor(_pr_TempName_lock);
	/* reset
	 */
	buf[0] = '\0';
	result = xp_FileName(name, type, buf, configBuf);
	if (result)
		result = XP_STRDUP(result);
	PR_ExitMonitor(_pr_TempName_lock);
	return result;
}

char * 
WH_FilePlatformName(const char * name)
{
	char* result;
	static char path[300];	/* Names longer than 300 are not dealt with in our stdio */
	if (_pr_TempName_lock == NULL) {
		_pr_TempName_lock = PR_NewNamedMonitor("TempName-lock");
	}
	PR_EnterMonitor(_pr_TempName_lock);
	result = xp_FilePlatformName(name, path);
	PR_ExitMonitor(_pr_TempName_lock);
	if (result)
		result = XP_STRDUP(result);
		
	return result;
}

char * 
WH_TempName(XP_FileType type, const char * prefix)
{
#define NS_BUFFER_SIZE	1024
	static char buf [NS_BUFFER_SIZE];	/* protected by _pr_TempName_lock */
	static char buf2 [100];
	static unsigned int count = 0;
	char* result;
	if (_pr_TempName_lock == NULL) {
		_pr_TempName_lock = PR_NewNamedMonitor("TempName-lock");
	}
	PR_EnterMonitor(_pr_TempName_lock);
	result = xp_TempName(type, prefix, buf, buf2, &count);
	if (result)
		result = XP_STRDUP(result);
	PR_ExitMonitor(_pr_TempName_lock);
	return result;
}

#endif /* !XP_WIN && !XP_OS2 */

#ifndef MOZ_USER_DIR
#define MOZ_USER_DIR ".mozilla"
#endif

char *fe_GetConfigDir(void) {
  char *home = getenv("HOME");
  if (home) {
    char *config_dir;

    int len = strlen(home);
    len += strlen("/") + strlen(MOZ_USER_DIR) + 1;

    config_dir = (char *)XP_CALLOC(len, sizeof(char));
    /* we really should use XP_STRN*_SAFE but this is MODULAR_NETLIB */
    XP_STRCPY(config_dir, home);
    XP_STRCAT(config_dir, "/");
    XP_STRCAT(config_dir, MOZ_USER_DIR); 
    return config_dir;
  }
  
  return strdup("/tmp");
}



/******************************************************************************/
