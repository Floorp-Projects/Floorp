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
/*
 * cinfo.h: Content Information for a file, i.e. its type, etc.
 * 
 * See cinfo.c for dependency information. 
 * 
 * Rob McCool
 */

#ifndef MKFORMAT_H
#define MKFORMAT_H

#ifndef MKSTREAM_H
#include "mkstream.h"
#endif /* MKSTREAM_H */

/* ------------------------------ Constants ------------------------------- */


/*
 * This will be the first string in the file, followed by x.x version
 * where x is an integer.
 * 
 * If this magic string is not found, cinfo_merge will try to parse
 * the file as a NCSA httpd mime.types file.
 */

#define MCC_MT_MAGIC "#--MCOM MIME Information"
#define MCC_MT_MAGIC_LEN 24

#define NCC_MT_MAGIC "#--Netscape Communications Corporation MIME Information"
#define NCC_MT_MAGIC_LEN 40	/* Don't bother to check it all */

/* The character which separates extensions with cinfo_find */

#define CINFO_SEPARATOR '.'

/* The maximum length of a line in this file */

#define CINFO_MAX_LEN 1024

/* The hash function for the database. Hashed on extension. */
#include <ctype.h>
#define CINFO_HASH(s) (isalpha(s[0]) ? tolower(s[0]) - 'a' : 26)

/* The hash table size for that function */
#define CINFO_HASHSIZE 27


/* ------------------------------ Structures ------------------------------ */

/* see ../include/net.h for the NET_cinfo struct */

/* ------------------------------ Prototypes ------------------------------ */

/*
 * cinfo_find finds any content information for the given uri. The file name
 * is the string following the last / in the uri. Multiple extensions are
 * separated by CINFO_SEPARATOR. You may pass in a filename instead of uri.
 *
 * Returns a newly allocated cinfo structure with the information it
 * finds. The elements of this structure are coming right out of the types 
 * database and so if you change it or want to keep it around for long you
 * should strdup it. You should free only the structure itself when finished
 * with it.
 *
 * If there is no information for any one of the extensions it
 * finds, it will ignore that extension. If it cannot find information for
 * any of the extensions, it will return NULL.
 */
extern NET_cinfo *NET_cinfo_find(char *uri);
extern NET_cinfo *NET_cinfo_find_type(char *uri);
extern NET_cinfo *NET_cinfo_find_enc (char *uri);


/*
 * cinfo_lookup finds the information about the given content-type, and 
 * returns a cinfo structure so you can look up description and icon.
 */
NET_cinfo *NET_cinfo_lookup(char *type);

#endif /* MKFORMAT_H */
