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
 *  npassoc.h $Revision: 3.1 $
 *  xp filetype associations
 */

#ifndef _NPASSOC_H
#define _NPASSOC_H

#include "xp_core.h"

typedef struct _NPFileTypeAssoc {
    char*	type;				/* a MIME type */
    char*	description;		/* Intelligible description */
    char**  extentlist;			/* a NULL-terminated list of file extensions */
    char*	extentstring;		/* the same extensions, as a single string */
    void*   fileType;			/* platform-specific file selector magic  */
    struct _NPFileTypeAssoc* pNext;
} NPFileTypeAssoc;


XP_BEGIN_PROTOS

extern NPFileTypeAssoc*	NPL_NewFileAssociation(const char *type, const char *extensions,
								const char *description, void *fileType);
extern void*			NPL_DeleteFileAssociation(NPFileTypeAssoc *fassoc);
extern void				NPL_RegisterFileAssociation(NPFileTypeAssoc *fassoc);
extern NPFileTypeAssoc*	NPL_RemoveFileAssociation(NPFileTypeAssoc *fassoc);
extern NPFileTypeAssoc* NPL_GetFileAssociation(const char *type);

XP_END_PROTOS

#endif /* _NPASSOC_H */

