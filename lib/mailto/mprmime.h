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

#ifndef _MPRMIME_H
#define _MPRMIME_H

#ifdef MOZ_ENDER_MIME

XP_BEGIN_PROTOS

typedef struct _AttachmentFields
{
    char *m_pFilename;
    char *m_pDispositionName;
    char *m_pContentType;
    char *m_pContentId;
}AttachmentFields;

AttachmentFields * AttachmentFields_Init(char *p_pFilename, char *p_pDispositionName, 
                                         char *p_pContentType, char *p_pContentId);

XP_Bool AttachmentFields_Destroy(AttachmentFields *p_fields);


typedef struct _GenericMimeRelatedData
{
    char *m_pBoundarySpecifier; /*string that is used as the boundary marker in multipart related mime*/
    char **m_pTextFiles;    /* text files that will be added as attachments*/
    int16 *m_pCsids;        /* charset ids for each text file */
    int16 m_iNumTextFiles;  /* number of text files used as attachments.*/

    AttachmentFields **m_pBase64Files;  /*files to be put into base64*/
    int16 m_iNumBase64Files;
    int (*write_buffer) (const char *, int32, void *); /* where to write encoding */
    void *closure;          /*not deleted on destroy*/
}GenericMimeRelatedData;

GenericMimeRelatedData * GenericMime_Init(char *p_pBoundarySpecifier, int (*output_fn) (const char *, int32, void *),
					void *closure);
XP_Bool GenericMime_Destroy(GenericMimeRelatedData *p_gendata);


XP_Bool GenericMime_Begin(GenericMimeRelatedData *p_genmime);

/*
return number of text files after add.  
filename passed in will be deleted later by GenericMimeRelated Destroy function
you are relinquishing ownership of the p_filename pointer.
*/
int GenericMime_AddTextFile(GenericMimeRelatedData *p_gendata, char *p_filename, int16 p_csid);

/*
return number of base64 files after add.  
you are relinquishing ownership of attachment fields struct
*/
int GenericMime_AddBase64File(GenericMimeRelatedData *p_gendata, AttachmentFields *p_fields);


/*
typedef
*/
typedef  int (*MPR_MIME_OUTPUTFUNC) (const char *, int32, void *);

XP_END_PROTOS

#endif /*_MPRMIME_H*/
#endif /*MOZ_ENDER_MIME*/
