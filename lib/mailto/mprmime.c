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

#ifdef MOZ_ENDER_MIME

#include "xp_core.h"
#include "mprmime.h"
#include "mimeenc.h"
#include "libi18n.h" /*for converting csids to strings*/

#define CONTENT_TYPE_MPR_MIMEREL        "Content-Type: multipart/related; "
#define CONTENT_TYPE_TXT_MIMEREL        "Content-Type: text/html; charset="
#define CONTENT_TYPE_MIMEREL            "Content-Type: "
#define CONTENT_ID_MIMEREL              "Content-ID: "
#define CONTENT_TRANS_MIMEREL           "Content-Transfer-Encoding: "
#define BASE64_MIMEREL                  "base64"
#define BIT7_MIMEREL                    "7bit"
#define BOUNDARYSTR_MIMEREL             "boundary="
#define XMOZSTATUS_MIMEREL              "X-Mozilla-Status: 8001"
#define CONTENT_DISPOSITION_MIMEREL     "Content-Disposition: inline;  filename="
#define READ_BUFFER_LEN                 255

/*
prototypes
*/
static XP_Bool output_text_file(GenericMimeRelatedData *p_genmime, int16 p_index);
static XP_Bool output_text_body(GenericMimeRelatedData *p_genmime, int16 p_index);
static XP_Bool output_base64_file(GenericMimeRelatedData *p_genmime, int16 p_index);



AttachmentFields *
AttachmentFields_Init(char *p_pFilename, char *p_pDispositionName, 
                      char *p_pContentType, char *p_pContentId)
{
    AttachmentFields *data = XP_NEW(AttachmentFields);
    if (!data || !p_pFilename || !p_pContentType || !p_pContentId) return 0;
    XP_MEMSET(data, 0, sizeof(*data));
    data->m_pFilename = p_pFilename;
    data->m_pDispositionName = p_pDispositionName;
    data->m_pContentType = p_pContentType;
    if (!data->m_pDispositionName)
        data->m_pDispositionName = XP_STRDUP(p_pFilename);
    data->m_pContentId = p_pContentId;
    return data;
}



static
XP_Bool
attachmentfields_validate(AttachmentFields *p_fields)
{
    if (!p_fields)
        return FALSE;
    if (!p_fields->m_pFilename)
        return FALSE; /*no filename is bad*/
    if (!p_fields->m_pDispositionName)
        return FALSE; /*no disposition is bad*/
    if (!p_fields->m_pContentType)
        return FALSE; /*no contenttype is bad*/
    if (!p_fields->m_pContentId)
        return FALSE; /*no yayaya*/
    return TRUE;
}



XP_Bool
AttachmentFields_Destroy(AttachmentFields *p_fields)
{
    if (!p_fields)
        return FALSE;
    XP_FREEIF(p_fields->m_pFilename);
    XP_FREEIF(p_fields->m_pDispositionName);
    XP_FREEIF(p_fields->m_pContentType);
    XP_FREEIF(p_fields);
    return TRUE;
}



/*
p_pBoundarySpecifier will be deleted in Destroy method. 
*/
GenericMimeRelatedData *
GenericMime_Init(char *p_pBoundarySpecifier, int (*output_fn) (const char *, int32, void *),
                 void *closure)
{
    GenericMimeRelatedData *data = XP_NEW(GenericMimeRelatedData);
    if (!data) return 0;
    XP_MEMSET(data, 0, sizeof(*data));
    data->m_pBoundarySpecifier = p_pBoundarySpecifier;
    data->write_buffer = output_fn;
    data->closure = closure;
    return data;
}



static XP_Bool
genericmime_validate(GenericMimeRelatedData *p_genmime)
{
    int i;/*counter*/
    if (!p_genmime)
        return FALSE;
    if (!(p_genmime->m_iNumTextFiles + p_genmime->m_iNumBase64Files))
        return FALSE; /*no files*/
    for( i=0; i<p_genmime->m_iNumTextFiles; i++)
    {
        if (!p_genmime->m_pTextFiles[i])
            return FALSE; /* no filename in position */
    }
    return TRUE;
}



XP_Bool
GenericMime_Destroy(GenericMimeRelatedData *p_gendata)
{
    int i;
    if (!p_gendata)
    {
        XP_ASSERT(0);
        return FALSE;
    }
    for( i=0; i<p_gendata->m_iNumTextFiles; i++)
    {
        XP_FREE(p_gendata->m_pTextFiles[i]);
    }
    XP_FREEIF(p_gendata->m_pTextFiles);

    for( i=0; i<p_gendata->m_iNumBase64Files; i++)
    {
        XP_FREEIF(p_gendata->m_pBase64Files[i]);
    }
    XP_FREEIF(p_gendata->m_pBase64Files);
    XP_FREEIF(p_gendata->m_pCsids);

    XP_FREEIF(p_gendata->m_pBoundarySpecifier);
    XP_FREE(p_gendata);
    return TRUE;
}



static XP_Bool
output_text_body(GenericMimeRelatedData *p_genmime, int16 p_index)
{
    char readbuffer[READ_BUFFER_LEN];           /*buffer to store incomming data*/
    int16 numread;                              /*number of bytes read in each pass*/
    XP_File t_inputfile;                       /*file pointer for input file*/

    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    if( (t_inputfile = XP_FileOpen(p_genmime->m_pTextFiles[p_index],xpFileToPost,XP_FILE_READ)) != NULL )
    {
        /* Attempt to read in READBUFLEN characters */
        while (!feof( t_inputfile ))
        {
            numread = XP_FileRead( readbuffer, READ_BUFFER_LEN, t_inputfile );
            if (ferror(t_inputfile))
            {
                XP_ASSERT(FALSE);
                break;
            }
            (*p_genmime->write_buffer)(readbuffer,numread,p_genmime->closure);
        }
        XP_FileClose( t_inputfile );
        (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
        (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    }
    return TRUE;
}



static XP_Bool
output_text_file(GenericMimeRelatedData *p_genmime, int16 p_index)
{
    char *charSet;                              /*used by text attachments when retrieving the charset str identifier*/

    if (p_genmime->m_pBoundarySpecifier) 
    {
        (*p_genmime->write_buffer)(p_genmime->m_pBoundarySpecifier,strlen(p_genmime->m_pBoundarySpecifier),p_genmime->closure);
        (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    }
    (*p_genmime->write_buffer)(CONTENT_TYPE_TXT_MIMEREL,strlen(CONTENT_TYPE_TXT_MIMEREL),p_genmime->closure);

    charSet = (char *)INTL_CsidToCharsetNamePt(p_genmime->m_pCsids[p_index]);
    if (charSet)
        (*p_genmime->write_buffer)(charSet,strlen(charSet),p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    (*p_genmime->write_buffer)(CONTENT_TRANS_MIMEREL,strlen(CONTENT_TRANS_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)(BIT7_MIMEREL,strlen(BIT7_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    return output_text_body(p_genmime,p_index);
}



static XP_Bool 
output_base64_file(GenericMimeRelatedData *p_genmime, int16 p_index)
{
    char readbuffer[READ_BUFFER_LEN];           /*buffer to store incomming data*/
    int16 numread;                              /*number of bytes read in each pass*/
    XP_File t_inputfile;                       /*file pointer for input file*/
    AttachmentFields *t_tempattach;             /*temporary pointer for the attachment loop*/
    MimeEncoderData *t_base64data;              /*saving using base64 requires one of these*/

    if (p_genmime->m_pBoundarySpecifier) 
    {
        (*p_genmime->write_buffer)(p_genmime->m_pBoundarySpecifier,strlen(p_genmime->m_pBoundarySpecifier),p_genmime->closure);
        (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    }
    t_tempattach = p_genmime->m_pBase64Files[p_index];
    if (!attachmentfields_validate(t_tempattach))
    {
        XP_ASSERT(FALSE);
        return FALSE;
    }

    (*p_genmime->write_buffer)(CONTENT_TYPE_MIMEREL,strlen(CONTENT_TYPE_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)(t_tempattach->m_pContentType,strlen(t_tempattach->m_pContentType),p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    (*p_genmime->write_buffer)(CONTENT_ID_MIMEREL,strlen(CONTENT_ID_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)("<",1,p_genmime->closure);
    (*p_genmime->write_buffer)(t_tempattach->m_pContentId ,strlen(t_tempattach->m_pContentId),p_genmime->closure);
    (*p_genmime->write_buffer)(">",1,p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    (*p_genmime->write_buffer)(CONTENT_TRANS_MIMEREL,strlen(CONTENT_TRANS_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)(BASE64_MIMEREL,strlen(BASE64_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    (*p_genmime->write_buffer)(CONTENT_DISPOSITION_MIMEREL,strlen(CONTENT_DISPOSITION_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)("\"",1,p_genmime->closure); /*begin quote*/
    (*p_genmime->write_buffer)(t_tempattach->m_pDispositionName,strlen(t_tempattach->m_pDispositionName),p_genmime->closure);
    (*p_genmime->write_buffer)("\"",1,p_genmime->closure); /*end quote*/
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    
    if( (t_inputfile = XP_FileOpen( t_tempattach->m_pFilename, xpFileToPost,XP_FILE_READ_BIN )) != NULL )
    {
        /* Attempt to read in READBUFLEN characters */
        t_base64data = MimeB64EncoderInit(p_genmime->write_buffer,p_genmime->closure);
        if (!t_base64data)
            return FALSE; /* bad?*/
        while (!feof( t_inputfile ))
        {
            numread = XP_FileRead( readbuffer, READ_BUFFER_LEN, t_inputfile );
            if (ferror(t_inputfile))
            {
                XP_ASSERT(FALSE);
                break;
            }
            MimeEncoderWrite(t_base64data,readbuffer,numread);
        }
        MimeEncoderDestroy(t_base64data,FALSE);
        XP_FileClose( t_inputfile );
        (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
        (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    }
    return TRUE;
}



XP_Bool
GenericMime_Begin(GenericMimeRelatedData *p_genmime)
{
    int16 i;                                      /*counter*/
    if (!genericmime_validate(p_genmime))  /*validation*/
    {
        XP_ASSERT(FALSE);
        return FALSE;
    }
    if (p_genmime->m_iNumTextFiles == 1 && p_genmime->m_iNumBase64Files == 0)
    {
        return output_text_body(p_genmime,0); /*output only the body part*/
    }
    (*p_genmime->write_buffer)(CONTENT_TYPE_MPR_MIMEREL,strlen(CONTENT_TYPE_MPR_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)(BOUNDARYSTR_MIMEREL,strlen(BOUNDARYSTR_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)("\"",1,p_genmime->closure); /*begin quote*/
    if (p_genmime->m_pBoundarySpecifier)
        (*p_genmime->write_buffer)(p_genmime->m_pBoundarySpecifier,strlen(p_genmime->m_pBoundarySpecifier),p_genmime->closure);
    (*p_genmime->write_buffer)("\"",1,p_genmime->closure); /*end quote*/
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    (*p_genmime->write_buffer)(XMOZSTATUS_MIMEREL,strlen(XMOZSTATUS_MIMEREL),p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    for (i = 0; i<p_genmime->m_iNumTextFiles; i++)
    {
        output_text_file(p_genmime,i);
    }

    /* base64 encoding all files that need it.*/
    for (i = 0; i<p_genmime->m_iNumBase64Files; i++)
    {
        if (!output_base64_file(p_genmime,i))
            return FALSE;
    }
    if (p_genmime->m_pBoundarySpecifier)
    {
        (*p_genmime->write_buffer)(p_genmime->m_pBoundarySpecifier,strlen(p_genmime->m_pBoundarySpecifier),p_genmime->closure);
        (*p_genmime->write_buffer)("--",2,p_genmime->closure);
    }
    (*p_genmime->write_buffer)("\n",1,p_genmime->closure);

    return TRUE;
}



/*
return number of text files after add.  
filename passed in will be deleted later by GenericMimeRelated Destroy function
you are relinquishing ownership of the p_filename pointer.
*/
int 
GenericMime_AddTextFile(GenericMimeRelatedData *p_gendata, char *p_filename, int16 p_csid)
{
    char **t_pfilenames;
    int16 *t_pcsids;
    if (!p_gendata || !p_filename)
    {
        XP_ASSERT(0);
        return -1;
    }
    t_pfilenames = p_gendata->m_pTextFiles;
    t_pcsids = p_gendata->m_pCsids;
    if ((!t_pfilenames || !t_pcsids) && p_gendata->m_iNumTextFiles) /*big problem*/
    {
        XP_ASSERT(0);
        return -1;
    }
    p_gendata->m_iNumTextFiles = p_gendata->m_iNumTextFiles+1;
    p_gendata->m_pTextFiles = (char **)XP_ALLOC(p_gendata->m_iNumTextFiles * (sizeof (char *)) );
    if (t_pfilenames)
        XP_MEMCPY(p_gendata->m_pTextFiles, t_pfilenames, sizeof(char *) * p_gendata->m_iNumTextFiles -1);
    p_gendata->m_pTextFiles[p_gendata->m_iNumTextFiles -1] = p_filename;

    p_gendata->m_pCsids = (int16 *)XP_ALLOC(p_gendata->m_iNumTextFiles * (sizeof (int16)) );
    if (t_pcsids)
        XP_MEMCPY(p_gendata->m_pCsids, t_pcsids, sizeof(int16) * p_gendata->m_iNumTextFiles -1);
    p_gendata->m_pCsids[p_gendata->m_iNumTextFiles -1] = p_csid;
    return p_gendata->m_iNumTextFiles;
}



/*
return number of text files after add.  
filename passed in will be deleted later by GenericMimeRelated Destroy function
you are relinquishing ownership of the p_filename pointer.
*/
int 
GenericMime_AddBase64File(GenericMimeRelatedData *p_gendata, AttachmentFields *p_fields)
{
    AttachmentFields **t_pfiles;
    if (!p_gendata || !p_fields)
    {
        XP_ASSERT(0);
        return -1;
    }
    t_pfiles = p_gendata->m_pBase64Files;
    if (!t_pfiles  && p_gendata->m_iNumBase64Files) /*big problem*/
    {
        XP_ASSERT(0);
        return -1;
    }
    p_gendata->m_iNumBase64Files = p_gendata->m_iNumBase64Files+1;
    p_gendata->m_pBase64Files = (AttachmentFields **)XP_ALLOC(p_gendata->m_iNumBase64Files * (sizeof (AttachmentFields *)) );
    if (t_pfiles)
        XP_MEMCPY(p_gendata->m_pBase64Files, t_pfiles, sizeof(char *) * p_gendata->m_iNumBase64Files -1);
    p_gendata->m_pBase64Files[p_gendata->m_iNumBase64Files -1] = p_fields;
    return p_gendata->m_iNumBase64Files;
}

#endif /* MOZ_ENDER_MIME */

