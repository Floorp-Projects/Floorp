/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/*
 * Routines for reading classes from a ZIP/JAR file in the class path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XP_MAC
#include "macstdlibextras.h"  /* for strncasecmp */
#endif
#include <fcntl.h>
#if !(XP_MAC)
#include <sys/types.h>
#else
#include <types.h>
//#include "NSstring.h"
#endif
#if !(XP_MAC)
#include <sys/stat.h>
#else
#include <stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>  /* for SEEK_SET and SEEK_END on some platforms */
#endif

#include "prlog.h"
#include "prio.h"
#include "prdtoa.h"
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"
#endif
#include "prtypes.h"
#include "nsZip.h"
#include "zlib.h"
#include "xp.h"						/* for XP_STRDUP */
#include "xp_qsort.h"
#include "prmem.h"
#include "prerror.h"

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/*
 * Header signatures
 */
#define SIGSIZ 4
#define LOCSIG "PK\003\004"
#define CENSIG "PK\001\002"
#define ENDSIG "PK\005\006"

/*
 * Header sizes including signatures
 */
#define LOCHDRSIZ 30
#define CENHDRSIZ 46
#define ENDHDRSIZ 22

/*
 * Header field access macros
 */
#define CH(b, n) ((unsigned short)(((unsigned char HUGEP *)(b))[n]))
#define SH(b, n) ((unsigned long)(CH(b, n) | (CH(b, n+1) << 8)))
#define LG(b, n) (SH(b, n) | (SH(b, n+2) << 16))

/*
 * Macros for getting local file header (LOC) fields
 */
#define LOCFLG(b) SH(b, 6)	    /* encrypt flags */
#define LOCHOW(b) SH(b, 8)	    /* compression method */
#define LOCCRC(b) LG(b, 14)	    /* uncompressed file crc-32 value */
#define LOCSIZ(b) LG(b, 18)	    /* compressed size */
#define LOCLEN(b) LG(b, 22)	    /* uncompressed size */
#define LOCNAM(b) SH(b, 26)	    /* filename size */
#define LOCEXT(b) SH(b, 28)	    /* extra field size */

/*
 * Macros for getting central directory header (CEN) fields
 */
#define CENHOW(b) SH(b, 10)	    /* compression method */
#define CENTIM(b) LG(b, 12)	    /* file modification time (DOS format) */
#define CENSIZ(b) LG(b, 20)	    /* compressed size */
#define CENLEN(b) LG(b, 24)	    /* uncompressed size */
#define CENNAM(b) SH(b, 28)	    /* length of filename */
#define CENEXT(b) SH(b, 30)	    /* length of extra field */
#define CENCOM(b) SH(b, 32)	    /* file comment length */
#define CENOFF(b) LG(b, 42)	    /* offset of local header */

/*
 * Macros for getting end of central directory header (END) fields
 */
#define ENDSUB(b) SH(b, 8)	    /* number of entries on this disk */
#define ENDTOT(b) SH(b, 10)	    /* total number of entries */
#define ENDSIZ(b) LG(b, 12)	    /* central directory size */
#define ENDOFF(b) LG(b, 16)	    /* central directory offset */
#define ENDCOM(b) SH(b, 20)	    /* size of zip file comment */


/*
 * Supported compression types
 */
#define STORED		0
#define DEFLATED	8

PR_BEGIN_EXTERN_C


#ifdef NETSCAPE

#ifdef XP_PC   
#define strncasecmp(s1, s2, len)  strnicmp((s1),(s2),(len))
#endif /* XP_PC */

/*
 * The following 2 macros are necessary for Win16 since they operate
 * on huge memory (which is unsupported by the runtime library)...
 */
#if !defined(XP_PC) || defined(_WIN32)
#define NS_ZIP_STRNCMP(buf, key, len)  strncmp((buf),(key),(len))
#define NS_ZIP_STRNCPY(dst, src, len)  strncpy((dst),(src),(len))
#define NS_ZIP_STRCPY(dst, src)        strcpy((dst), (src))

#else   /* WIN16 only */
static int NS_ZIP_STRNCMP(char HUGEP *buf, char *key, int len)  
{
    while(len--) {
        char c1, c2;

        c1 = *buf++;
        c2 = *key++;
        if (c1 != c2) {
            return (c1 < c2) ? -1 : 1;
        }
    }
    return 0;
}

static char HUGEP *NS_ZIP_STRCPY(char HUGEP *dst, char * src)
{
    char HUGEP *ret = dst;

    if (src && dst) {
        while(*src) {
            *dst++ = *src++;
        }
        *dst = *src;
    }
    return ret;
}

#define NS_ZIP_STRNCPY(dst, src, len)  hmemcpy((dst),(src),(len))

#endif /* XP_PC || _WIN32 */
#endif /* NETSCAPE */

static void
ns_zip_errmsg(const char *msg)
{
#ifdef DEBUG
    PRFileDesc* prfd = PR_GetSpecialFD(2);
    PR_Write(prfd, msg, strlen(msg));
#endif
}

static void
ns_ziperr(ns_zip_t *zip, const char *msg)
{
#ifdef DEBUG
    ns_zip_errmsg("Zip Error: ");
    ns_zip_errmsg(zip->fn);
    ns_zip_errmsg(": ");
    ns_zip_errmsg(msg);
    ns_zip_errmsg("\n");
#endif
}

/*#define ZTRACE*/
#ifdef ZTRACE
FILE *ns_zlog = NULL;
#endif

/* single open fd cache 
 * this relies on the fact that none of the public routines in here
 * depend on seek state between operations */

static ns_zip_t *ns_zip_cache = NULL;
static PRMonitor *ns_zlock = NULL;

static PRBool nsEnsureZip(ns_zip_t *zip)
{
    if(zip != ns_zip_cache) {
      PRFileDesc *fd;
      if(ns_zip_cache) {
	  PR_Close(ns_zip_cache->fd);
          ns_zip_cache->fd = 0;
#ifdef ZTRACE
	  if (ns_zlog) fprintf(ns_zlog, "close %s\n", ns_zip_cache->fn);
#endif
      }
#ifdef ZTRACE
      if (ns_zlog) fprintf(ns_zlog,"open %s\n", zip->fn);
#endif
      fd = PR_Open(zip->fn, PR_RDONLY, 0);
      if (FD_IS_ERROR(fd)) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16 dlls
	 */
	  perror(zip->fn);
#endif
	  ns_zip_cache = NULL;
	  return PR_FALSE;
      }
      zip->fd = fd;
      ns_zip_cache = zip;
    }
    return PR_TRUE;
}


#if defined(XP_PC) && !defined(_WIN32)
/*
 * Read exactly 'len' bytes of data into 'buf'. Return PR_TRUE if all bytes
 * could be read, otherwise return PR_FALSE.
 */
static PRBool
nsZipReadFully(PRFileDesc *fd, void *buf, PRInt32 len)
{
    char HUGEP *bp = (char HUGEP *)buf;
    PRInt32 segBlock;

    PR_ASSERT( !IsBadHugeWritePtr((void HUGEP *)buf, len) );

    do {
        segBlock = (len >= 0x10000L) ? (0x10000L - OFFSETOF(bp)) : len;
        len -= segBlock;

        /*
         * Read a segment (64K) into the buffer...  
         */
        while (segBlock > 0) {
            PRInt32 rqst, n;
    
            rqst = (segBlock > 4096) ? 4096 : segBlock;
    	
            n = PR_Read(fd, bp, rqst);
            if (n <= 0) {
                return PR_FALSE;
            }
            bp += n;
            segBlock -= n;
        }
    } while (len > 0);

    return PR_TRUE;
}

#else
/*
 * Read exactly 'len' bytes of data into 'buf'. Return PR_TRUE if all bytes
 * could be read, otherwise return PR_FALSE.
 */
static PRBool
nsZipReadFully(PRFileDesc *fd, void *buf, PRInt32 len)
{
    char *bp = buf;

    while (len > 0) {
	PRInt32 n = PR_Read(fd, bp, len);
	if (n <= 0) {
	    return PR_FALSE;
	}
	bp += n;
	len -= n;
    }
    return PR_TRUE;
}
#endif

#define INBUFSIZ 64

/*
 * Find end of central directory (END) header in zip file and set the file
 * pointer to start of header. Return PR_FALSE if the END header could not be
 * found, otherwise return PR_TRUE.
 */
static PRBool
nsZipFindEnd(ns_zip_t *zip, char *endbuf)
{
    char buf[INBUFSIZ+SIGSIZ], *bp;
    PRUint32 len, off, mark;

    /* Need to search backwards from end of file */
    if ((len = PR_Seek(zip->fd, 0, SEEK_END)) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
	perror(zip->fn);
#endif
	return PR_FALSE;
    }
    /*
     * Set limit on how far back we need to search. The END header must be
     * located within the last 64K bytes of the file.
     */
    mark = max(0, len - 0xffff);
    /*
     * Search backwards INBUFSIZ bytes at a time from end of file stopping
     * when the END header signature has been found. Since the signature
     * may straddle a buffer boundary, we need to stash the first SIGSIZ-1
     * bytes of the previous record at the tail of the current record so
     * that the search can overlap.
     */
    memset(buf, 0, SIGSIZ);
    for (off = len; off > mark; ) {
	long n = min(off - mark, INBUFSIZ);
	memcpy(buf + n, buf, SIGSIZ);
	if (PR_Seek(zip->fd, off -= n, SEEK_SET) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
	    perror(zip->fn);
#endif
	    return PR_FALSE;
	}
	if (!nsZipReadFully(zip->fd, buf, n)) {
	    ns_ziperr(zip, "Fatal read error while searching for END record");
	    return PR_FALSE;
	}
	for (bp = buf + n - 1; bp >= buf; --bp) {
	    if (strncmp(bp, ENDSIG, SIGSIZ) == 0) {
		long endoff = off + (long)bp - (long)buf;
		if (len - endoff < ENDHDRSIZ) {
		    continue;
		}
		if ((buf+n-bp) >= ENDHDRSIZ) {
		    memcpy(endbuf, bp, ENDHDRSIZ);
		} else {
		    if (PR_Seek(zip->fd, endoff, SEEK_SET) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
 		    perror(zip->fn);
#endif
		    }
		    if (!nsZipReadFully(zip->fd, endbuf, ENDHDRSIZ)) {
			ns_ziperr(zip, "Read error while searching for END record");
			return PR_FALSE;
		    }
		}
		if (endoff + ENDHDRSIZ + ENDCOM(endbuf) != len) {
		    continue;
		}
		if (PR_Seek(zip->fd, endoff, SEEK_SET) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
		    perror(zip->fn);
#endif
		    return PR_FALSE;
		}
		zip->endoff = endoff;
		return PR_TRUE;
	    }
	}
    }
    return PR_FALSE;
}

/*
 * Convert DOS time to UNIX time
 */
static time_t
ns_zip_unixtime(int dostime)
{
    struct tm tm;

    tm.tm_sec = (dostime << 1) & 0x3e;
    tm.tm_min = (dostime >> 5) & 0x3f;
    tm.tm_hour = (dostime >> 11) & 0x1f;
    tm.tm_mday = (dostime >> 16) & 0x1f;
    tm.tm_mon = ((dostime >> 21) & 0xf) - 1;
    tm.tm_year = ((dostime >> 25) & 0x7f) + 1980;
    return mktime(&tm);
}

static int
ns_zip_direlcmp(const void *d1, const void *d2)
{
    return strcmp(((direl_t *)d1)->fn, ((direl_t *)d2)->fn);
}

/*
 * Initialize zip file reader, read in central directory and construct the
 * lookup table for locating zip file members.
 */
static PRBool
ns_zip_initReader(ns_zip_t *zip)
{
    char endbuf[ENDHDRSIZ];
    char HUGEP *cenbuf;
    char HUGEP *cp;
    PRUint32 locoff;
    PRUint32 i, cenlen;

    /* Seek to END header and read it in */
    if (!nsZipFindEnd(zip, endbuf)) {
	ns_ziperr(zip, "Unable to locate end-of-central-directory record");
	return PR_FALSE;
    }
    /* Verify END signature */
    if (strncmp(endbuf, ENDSIG, SIGSIZ) != 0) {
	ns_ziperr(zip, "Invalid END signature");
	return PR_FALSE;
    }
    /* Get real offset of central directory */
    zip->cenoff = zip->endoff - ENDSIZ(endbuf);
    /*
     * Since there may be a stub prefixed to the ZIP or JAR file, the
     * the start offset of the first local file (LOC) header will be
     * the difference between the real offset of the first CEN header
     * and what is specified in the END header.
     */
    if (zip->cenoff < ENDOFF(endbuf)) {
	ns_ziperr(zip, "Invalid end-of-central directory header");
	return PR_FALSE;
    }
    locoff = zip->cenoff - ENDOFF(endbuf);
    /* Get length of central directory */
    cenlen = ENDSIZ(endbuf);
    /* Check offset and length */
    if (zip->cenoff + cenlen != zip->endoff) {
	ns_ziperr(zip, "Invalid end-of-central-directory header");
	return PR_FALSE;
    }
    /* Get total number of central directory entries */
    zip->nel = ENDTOT(endbuf);
    /* Check entry count */
    if (zip->nel * CENHDRSIZ > cenlen) {
	ns_ziperr(zip, "Invalid end-of-central-directory header");
	return PR_FALSE;
    }
    /* Verify that zip file contains only one drive */
    if (ENDSUB(endbuf) != zip->nel) {
	ns_ziperr(zip, "Cannot contain more than one drive");
	return PR_FALSE;
    }
    /* Seek to first CEN header */
    if (PR_Seek(zip->fd, zip->cenoff, SEEK_SET) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
	perror(zip->fn);
#endif
	return PR_FALSE;
    }
    /* Allocate file lookup table */
    CHECK_SIZE_LIMIT( zip->nel * sizeof(direl_t) );
    if ((zip->dir = PR_MALLOC((size_t)(zip->nel * sizeof(direl_t)))) == 0) {
	ns_ziperr(zip, "Out of memory allocating lookup table");
	return PR_FALSE;
    }
    /* Allocate temporary buffer for contents of central directory */
    if ((cenbuf = PR_MALLOC(cenlen)) == 0) {
	ns_ziperr(zip, "Out of memory allocating central directory buffer");
	return PR_FALSE;
    }
    /* Read central directory */
    if (!nsZipReadFully(zip->fd, cenbuf, cenlen)) {
	ns_ziperr(zip, "Fatal error while reading central directory");
        PR_Free(cenbuf);
	return PR_FALSE;
    }
    /* Scan each header in central directory */
    for (i = 0, cp = cenbuf; i < zip->nel; i++) {
	direl_t *dp = &zip->dir[i];
	PRUint32 n;
	/* Verify signature of CEN header */
	if (NS_ZIP_STRNCMP(cp, CENSIG, SIGSIZ) != 0) {
	    ns_ziperr(zip, "Invalid central directory header signature");
            PR_Free(cenbuf);
	    return PR_FALSE;
	}
	/* Get file name */
	n = CENNAM(cp);

        CHECK_SIZE_LIMIT( n+1 );
	if ((dp->fn = PR_MALLOC((size_t)n + 1)) == 0) {
	    ns_ziperr(zip, "Out of memory reading CEN header file name");
            PR_Free(cenbuf);
	    return PR_FALSE;
	}
	NS_ZIP_STRNCPY(dp->fn, cp + CENHDRSIZ, (size_t) n);
	dp->fn[n] = '\0';
	/* Get compression method */
	dp->method = CENHOW(cp);
	/* Get real offset of LOC header */
	dp->off = locoff + CENOFF(cp);
	/* Get uncompressed file size */
	dp->len = CENLEN(cp);
	/* Get compressed file size */
	dp->size = CENSIZ(cp);
	/* Check offset and size */
	if (dp->off + CENSIZ(cp) > zip->cenoff) {
	    ns_ziperr(zip, "Invalid CEN header");
            PR_Free(cenbuf);
	    return PR_FALSE;
	}
	/* Get file modification time */
	dp->mod = (int) CENTIM(cp);
	/* Skip to next CEN header */
	cp += CENHDRSIZ + CENNAM(cp) + CENEXT(cp) + CENCOM(cp);
	if (cp > cenbuf + cenlen) {
	    ns_ziperr(zip, "Invalid CEN header");
            PR_Free(cenbuf);
	    return PR_FALSE;
	}
    }
    /* Free temporary buffer */
    PR_Free(cenbuf);
    /* Sort directory elements by name */
    XP_QSORT(zip->dir, (size_t) zip->nel, sizeof(direl_t), ns_zip_direlcmp);
    return PR_TRUE;
}

PR_PUBLIC_API(PRBool)
ns_zip_lock(void)
{
    if(ns_zlock == NULL)
    {
	ns_zlock = PR_NewMonitor();
	if(ns_zlock == NULL)
	    return PR_FALSE;
    } 
    PR_EnterMonitor(ns_zlock);
    return PR_TRUE;
}

PR_PUBLIC_API(void)
ns_zip_unlock(void)
{
    PR_ASSERT(ns_zlock != NULL);
  
    if(ns_zlock != NULL)
    {
        PR_ExitMonitor(ns_zlock);
    }
}

/*
 * Open ns_zip_t file and initialize file lookup table.
 */
PR_PUBLIC_API(ns_zip_t *)
ns_zip_open(const char *fn)
{
    PRFileDesc *fd;
    ns_zip_t *zip;

    if (!ns_zip_lock())
      return NULL;
#ifdef ZTRACE
    if(ns_zlog == NULL) {
      ns_zlog = fopen("ns_zlog.txt", "w");
      if (FD_IS_ERROR(fd)) {
	ns_zlog = NULL;
      }
    }
#endif

    if(ns_zip_cache)
    {
#ifdef ZTRACE
      if (ns_zlog) fprintf(ns_zlog, "close %s\n", ns_zip_cache->fn);
#endif
      PR_Close(ns_zip_cache->fd);
      ns_zip_cache->fd = 0;
      ns_zip_cache = 0;
    }

#ifdef ZTRACE
    if (ns_zlog) fprintf(ns_zlog, "zip open %s\n", fn);
#endif
    fd = PR_Open(fn, PR_RDONLY, 0);
    if (FD_IS_ERROR(fd)) {
	ns_zip_unlock();
	return 0;
    }
    if ((zip = PR_Calloc(1, sizeof(ns_zip_t))) == 0) {
	ns_zip_errmsg("Out of memory");
	ns_zip_unlock();
	return 0;
    }
    if ((zip->fn = XP_STRDUP(fn)) == 0) {
	ns_zip_errmsg("Out of memory");
	ns_zip_unlock();
	return 0;
    }
    zip->fd = fd;
    if (!ns_zip_initReader(zip)) {
	PR_Free(zip->fn);
	PR_Free(zip);
	PR_Close(fd);
	ns_zip_unlock();
	return 0;
    }

    ns_zip_cache = zip;
    ns_zip_unlock();
    return zip;
}

/*
 * Close zip file and free lookup table
 */
PR_PUBLIC_API(void)
ns_zip_close(ns_zip_t *zip)
{
    PRUint32 i;

    ns_zip_lock();
#ifdef ZTRACE
    if (ns_zlog) fprintf(ns_zlog, "zip close %s\n", zip->fn);
#endif
    PR_Free(zip->fn);
    if(ns_zip_cache == zip) {
	PR_Close(zip->fd);
        ns_zip_cache = NULL;
    } else {
        PR_ASSERT(zip->fd == 0);
    }
    ns_zip_unlock();

    for (i = 0; i < zip->nel; i++) {
	PR_Free(zip->dir[i].fn);
    }
    PR_Free(zip->dir);
    PR_Free(zip);
}

static direl_t *
ns_zip_lookup(ns_zip_t *zip, const char *fn)
{
    direl_t key;
    key.fn = (char *)fn;
    return bsearch(&key, zip->dir, (size_t) zip->nel, sizeof(direl_t), ns_zip_direlcmp);
}

/*
 * Return file status for ns_zip_t file member
 */
PR_PUBLIC_API(PRBool)
ns_zip_stat(ns_zip_t *zip, const char *fn, struct stat *sbuf)
{
    direl_t *dp;

    if(zip == NULL)
        return PR_FALSE;

    dp = ns_zip_lookup(zip, fn);
#ifdef ZTRACE
    if(ns_zlog) fprintf(ns_zlog, "stat %s %s\n", dp ? "found" : "failed", fn);
#endif
    if (dp == 0) {
	return PR_FALSE;
    }
    memset(sbuf, 0, sizeof(struct stat));
    sbuf->st_size = dp->len;
#if 0
    /* no one uses this stuff yet */
    sbuf->st_mode = 444;
    sbuf->st_mtime = ns_zip_unixtime(dp->mod);
    sbuf->st_atime = sbuf->st_mtime;
    sbuf->st_ctime = sbuf->st_mtime;
#endif
    return PR_TRUE;
}

static PRBool
ns_zip_inflateFully(PRFileDesc *fd, PRUint32 size, void *buf, PRUint32 len, char **msg)
{
    static z_stream *strm;
#if  defined(XP_PC) && !defined(_WIN32)
    char tmp[16000];
    char tmpBuf[4096];
#else
    char tmp[32000];
#endif /* defined(XP_PC) && !defined(_WIN32) */
    PRBool ok = PR_TRUE;

    if (strm == 0) {
	strm = PR_Calloc(1, sizeof(z_stream));
	if (inflateInit2(strm, -MAX_WBITS) != Z_OK) {
	    *msg = strm->msg;
	    PR_Free(strm);
	    return PR_FALSE;
	}
    }

#if  !defined(XP_PC) || defined(_WIN32)
    strm->next_out  = buf;
    strm->avail_out = len;
#endif

    while (strm->total_in <= size) {
	PRInt32 n = size - strm->total_in;
	if (n > 0) {
	    n = PR_Read(fd, tmp, n > sizeof(tmp) ? sizeof(tmp) : n);
	    if (n == 0) {
		*msg = "Unexpected EOF";
		ok = PR_FALSE;
		goto stop;
	    }
	    if (n < 0) {
                (void) PR_GetErrorText(*msg);
		ok = PR_FALSE;
		goto stop;
	    }
	}
	strm->next_in = (Bytef *)tmp;
	strm->avail_in = n;

	do {

#if  defined(XP_PC) && !defined(_WIN32)
            strm->next_out  = tmpBuf;
            strm->avail_out = sizeof(tmpBuf);
#endif /* defined(XP_PC) && !defined(_WIN32) */

	    switch (inflate(strm, Z_PARTIAL_FLUSH)) {
	    case Z_OK:
#if  defined(XP_PC) && !defined(_WIN32)
	    {
		int inflatedLength;
		inflatedLength = strm->next_out - tmpBuf;
		NS_ZIP_STRNCPY((char HUGEP *)buf, (char HUGEP *)tmpBuf, inflatedLength);
		((char HUGEP *)buf)+=inflatedLength;
	    }
#endif /* defined(XP_PC) && !defined(_WIN32) */
		break;
	    case Z_STREAM_END:
#if  defined(XP_PC) && !defined(_WIN32)
	    {
		int inflatedLength;
		inflatedLength = strm->next_out - tmpBuf;
		NS_ZIP_STRNCPY((char HUGEP *)buf, (char HUGEP *)tmpBuf, inflatedLength);
		((char HUGEP *)buf)+=inflatedLength;
            }
#endif /* defined(XP_PC) && !defined(_WIN32) */
		if (strm->total_in != size || strm->total_out != len) {
		    *msg = "Invalid entry compressed size";
		    ok = PR_FALSE;
		}
		goto stop;
	    default:
		*msg = strm->msg;
		goto stop;
	    }
	} while (strm->avail_in > 0);
    }
    *msg = "Invalid entry compressed size";
    ok = PR_FALSE;

stop:
    inflateReset(strm);
    return ok;
}

/*
 * Read contents of zip file member into buffer 'buf'. If the size of the
 * data should exceed 'len' then nothing is read and PR_FALSE is returned.
 */
PR_PUBLIC_API(PRBool)
ns_zip_get(ns_zip_t *zip, const char *fn, void HUGEP *buf, PRInt32 len)
{
    char locbuf[LOCHDRSIZ];
    long off;
    direl_t *dp = ns_zip_lookup(zip, fn);

    if (!ns_zip_lock())
	return PR_FALSE;
    if(!nsEnsureZip(zip)) {
	ns_zip_unlock();
	return PR_FALSE;
    }

#ifdef ZTRACEX
    if(ns_zlog) fprintf(ns_zlog, "get %s\n", fn);
#endif
    /* Look up file member */
    if (dp == 0) {
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Check buffer length */
    if (len != (PRInt32) dp->len) {
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Seek to beginning of LOC header */
    if (PR_Seek(zip->fd, dp->off, SEEK_SET) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
	perror(zip->fn);
#endif
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Read LOC header */
    if (!nsZipReadFully(zip->fd, locbuf, LOCHDRSIZ)) {
	ns_ziperr(zip, "Fatal error while reading LOC header");
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Verify signature */
    if (strncmp(locbuf, LOCSIG, SIGSIZ) != 0) {
	ns_ziperr(zip, "Invalid LOC header signature");
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Make sure file is not encrypted */
    if ((LOCFLG(locbuf) & 1) == 1) {
	ns_ziperr(zip, "Member is encrypted");
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Get offset of file data */
    off = dp->off + LOCHDRSIZ + LOCNAM(locbuf) + LOCEXT(locbuf);
    if (off + dp->size > zip->cenoff) {
	ns_ziperr(zip, "Invalid LOC header");
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Seek to file data */
    if (PR_Seek(zip->fd, off, SEEK_SET) == -1) {
#if !defined(XP_PC) || defined(_WIN32)
	/*
	 * perror is not defined for win16
	 */
	perror(zip->fn);
#endif
	ns_zip_unlock();
	return PR_FALSE;
    }
    /* Read file data */
    switch (dp->method) {
    case STORED:
	if (!nsZipReadFully(zip->fd, buf, dp->len)) {
	    ns_ziperr(zip, "Fatal error while reading LOC data");
	    ns_zip_unlock();
	    return PR_FALSE;
	}
	break;
    case DEFLATED: {
	char *msg = 0;
	if (!ns_zip_inflateFully(zip->fd, dp->size, buf, dp->len, &msg)) {
	    if (msg != 0) {
		ns_ziperr(zip, msg);
	    } else {
		ns_ziperr(zip, "Error while reading compressed zip file entry");
	    }
	    ns_zip_unlock();
	    return PR_FALSE;
	}
	break;
    }
    default:
	ns_ziperr(zip, "Unsupported compression method");
	ns_zip_unlock();
	return PR_FALSE;
    }

    ns_zip_unlock();
    return PR_TRUE;
}

/*
 * Return total number of files in the archive that match the suffix.
 * For example look for all files that end with ".sf"
 * XXX just look in META??
 */
PR_PUBLIC_API(PRUint32)
ns_zip_get_no_of_elements(ns_zip_t *zip, const char *fn_suffix)
{
    int suf_len = strlen(fn_suffix);
    PRUint32 nel;
    PRUint32 i;

    for (i=0, nel=0; i < zip->nel; i++) {
	char *fn = zip->dir[i].fn;
        int fn_len = strlen(fn);
        if (strncasecmp(&fn[fn_len - suf_len], fn_suffix, suf_len) == 0) {
            nel++;
        }
    }
    return nel;
}

/*
 * Return a list of files in the archive that match the filename suffix
 */
PR_PUBLIC_API(PRUint32)
ns_zip_list_files(ns_zip_t *zip, const char *fn_suffix, void HUGEP *buf, PRUint32 len)
{
    int suf_len = strlen(fn_suffix);
    PRUint32 nel;
    PRUint32 i;
    char HUGEP *buf_ptr = buf;
    char HUGEP *buf_end_ptr = buf_ptr + len;

    for (i=0, nel=0; i < zip->nel; i++) {
	char *fn = zip->dir[i].fn;
        int fn_len = strlen(fn);
        if ((strncasecmp(&fn[fn_len - suf_len], fn_suffix, suf_len) == 0) &&
            ((buf_ptr + fn_len) < buf_end_ptr)) {
            nel++;
            NS_ZIP_STRCPY(buf_ptr, fn);
            buf_ptr += fn_len+1;
        }
    }
    return nel;
}

PR_END_EXTERN_C

