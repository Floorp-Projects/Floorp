/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


/*
 *  npassoc.h $Revision: 3.2 $
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

