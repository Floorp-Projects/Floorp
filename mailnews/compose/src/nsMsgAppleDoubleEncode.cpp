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

/*
*
*   apple-double.c
*	--------------
*
*  	  The codes to do apple double encoding/decoding.
*		
*		02aug95		mym		created.
*		27sep95		mym		Add the XP_Mac to ensure the cross-platform.
*		
*/
#include "nsID.h"
#include "nsCRT.h"
#include "nscore.h"
#include "msgCore.h"
#include "nsMsgAppleDouble.h"
#include "nsMsgAppleCodes.h"
#include "nsFileSpec.h"
#include "nsMsgCompUtils.h"

#ifdef XP_MAC

#pragma warn_unusedarg off
#include "m_cvstrm.h"

#pragma cplusplus on

OSErr my_FSSpecFromPathname(char* src_filename, FSSpec* fspec)
{
	/* don't resolve aliases... */
	return CFileMgr::FSSpecFromLocalUnixPath(src_filename, fspec, false);
}

char* my_PathnameFromFSSpec(FSSpec* fspec)
{
	return CFileMgr::GetURLFromFileSpec(*fspec);
}

//
// Returns true if the resource fork should be sent!
//
// RICHIEHACK: for now, this is a temporary solution that should be
// replaced by calls to the MIME service! We look for files that typically
// don't have resource forks and if it is one of these types, we go with
// it, but otherwise, we are going to do the fancy encoding!
//
PRBool	
nsMsgIsMacFile(char *aUrlString)
{
	Boolean returnValue = PR_FALSE;

  char  *ext = nsMsgGetExtensionFromFileURL(nsString(aUrlString));
  if ( (!ext) || (!*ext) )
    return PR_TRUE;

  if (
       (!PL_strcasecmp(ext, "JPG")) ||
       (!PL_strcasecmp(ext, "GIF")) ||
       (!PL_strcasecmp(ext, "TIF")) ||
       (!PL_strcasecmp(ext, "HTM")) ||
       (!PL_strcasecmp(ext, "HTML")) ||
       (!PL_strcasecmp(ext, "ART")) ||
       (!PL_strcasecmp(ext, "XUL")) ||
       (!PL_strcasecmp(ext, "XML")) ||
       (!PL_strcasecmp(ext, "XUL"))
     )
     return PR_FALSE;
  else
    return PR_TRUE;
}

void	
MacGetFileType(nsFileSpec   *path, 
               PRBool       *useDefault, 
               char         **fileType, 
               char         **encoding)
{
	if ((path == NULL) || (fileType == NULL) || (encoding == NULL))
		return;

	*useDefault = TRUE;
	*fileType = NULL;
	*encoding = NULL;

	char *pathPart = NET_ParseURL( path, GET_PATH_PART);
	if (pathPart == NULL)
		return;

	nsFilePath thePath(pathPart);
	nsNativeFileSpec spec(thePath);
	XP_FREE(pathPart);

	CMimeMapper * mapper = CPrefs::sMimeTypes.FindMimeType(spec);
	if (mapper != NULL)
	{
		*useDefault = FALSE;
		*fileType = nsCRT::strdup(mapper->GetMimeName());
	}
	else
	{
		FInfo		fndrInfo;
		OSErr err = FSpGetFInfo( &spec, &fndrInfo );
		if ( (err != noErr) || (fndrInfo.fdType == 'TEXT') )
      *fileType = nsCRT::strdup(APPLICATION_OCTET_STREAM);
		else
		{
			// Time to call IC to see if it knows anything
			ICMapEntry ICMapper;
			
			ICError  error = 0;
			CStr255 fileName( spec.name );
			error = CInternetConfigInterface::GetInternetConfigFileMapping(
					fndrInfo.fdType, fndrInfo.fdCreator,  fileName ,  &ICMapper );	
			if( error != icPrefNotFoundErr && StrLength(ICMapper.MIME_type) )
			{
				*useDefault = FALSE;
				CStr255 mimeName( ICMapper.MIME_type );
				*fileType = nsCRT::strdup( mimeName );
			}
			else
			{
				// That failed try using the creator type		
				mapper = CPrefs::sMimeTypes.FindCreator(fndrInfo.fdCreator);
				if( mapper)
				{
					*useDefault = FALSE;
					*fileType = nsCRT::strdup(mapper->GetMimeName());
				}
				else
				{
					// don't have a mime mapper
					*fileType = nsCRT::strdup(APPLICATION_OCTET_STREAM);
				}
			}
		}
	}
}

#pragma cplusplus reset

/*
*	ap_encode_init
*	--------------
*	
*	Setup the encode envirment
*/

int ap_encode_init( appledouble_encode_object *p_ap_encode_obj, 
	                  char                      *fname,
                    char                      *separator)
{
	FSSpec	fspec;
	
	if (my_FSSpecFromPathname(fname, &fspec) != noErr )
		return -1;
	
  nsCRT::memset(p_ap_encode_obj, 0, sizeof(appledouble_encode_object));
	
	/*
	**	Fill out the source file inforamtion.
	*/	
	nsCRT::memcpy(p_ap_encode_obj->fname, fspec.name+1, *fspec.name);
	p_ap_encode_obj->fname[*fspec.name] = '\0';
	p_ap_encode_obj->vRefNum = fspec.vRefNum;
	p_ap_encode_obj->dirId   = fspec.parID;
	
	p_ap_encode_obj->boundary = nsCRT::strdup(separator);
	return noErr;
}
/*
**	ap_encode_next
**	--------------
**		
**		return :
**			noErr	:	everything is ok
**			errDone	:	when encoding is done.
**			errors	:	otherwise.
*/
int ap_encode_next(
	appledouble_encode_object* p_ap_encode_obj, 
	char 	*to_buff, 
	PRInt32 	buff_size, 
	PRInt32* 	real_size)
{
	int status;
	
	/*
	** 	install the out buff now.
	*/
	p_ap_encode_obj->outbuff     = to_buff;
	p_ap_encode_obj->s_outbuff 	 = buff_size;
	p_ap_encode_obj->pos_outbuff = 0;
	
	/*
	**	first copy the outstandind data in the overflow buff to the out buffer. 
	*/
	if (p_ap_encode_obj->s_overflow)
	{
		status = write_stream(p_ap_encode_obj, 
								p_ap_encode_obj->b_overflow,
								p_ap_encode_obj->s_overflow);
		if (status != noErr)
			return status;
				
		p_ap_encode_obj->s_overflow = 0;
	}

	/*
	** go the next processing stage based on the current state. 
	*/
	switch (p_ap_encode_obj->state)
	{
		case kInit:
			/*
			** We are in the  starting position, fill out the header.
			*/
			status = fill_apple_mime_header(p_ap_encode_obj); 
			if (status != noErr)
				break;					/* some error happens */
				
			p_ap_encode_obj->state = kDoingHeaderPortion;
			status = ap_encode_header(p_ap_encode_obj, true); 
										/* it is the first time to calling 		*/							
			if (status == errDone)
			{
				p_ap_encode_obj->state = kDoneHeaderPortion;
			}
			else
			{
				break;					/* we need more work on header portion.	*/
			}			
				
			/*
			** we are done with the header, so let's go to the data port.
			*/
			p_ap_encode_obj->state = kDoingDataPortion;
			status = ap_encode_data(p_ap_encode_obj, true);		 	
										/* it is first time call do data portion */
							
			if (status == errDone)
			{
				p_ap_encode_obj->state  = kDoneDataPortion;
				status = noErr;
			}
			break;

		case kDoingHeaderPortion:
		
			status = ap_encode_header(p_ap_encode_obj, false); 			
										/* continue with the header portion.	*/
			if (status == errDone)
			{
				p_ap_encode_obj->state = kDoneHeaderPortion;
			}
			else
			{
				break;					/* we need more work on header portion.	*/				
			}
			
			/*
			** start the data portion.
			*/
			p_ap_encode_obj->state = kDoingDataPortion;
			status = ap_encode_data(p_ap_encode_obj, true); 					
										/* it is the first time calling 		*/
			if (status == errDone)
			{
				p_ap_encode_obj->state  = kDoneDataPortion;
				status = noErr;
			}
			break;

		case kDoingDataPortion:
		
			status = ap_encode_data(p_ap_encode_obj, false); 				
										/* it is not the first time				*/
													
			if (status == errDone)
			{
				p_ap_encode_obj->state = kDoneDataPortion;
				status = noErr;
			}
			break;

		case kDoneDataPortion:
#if 0
			status = write_stream(p_ap_encode_obj,
									"\n-----\n\n",
									8);
			if (status == noErr)
#endif
				status = errDone;		/* we are really done.					*/

			break;
	}
	
	*real_size = p_ap_encode_obj->pos_outbuff;
	return status;
}

/*
**	ap_encode_end
**	-------------
**
**	clear the apple encoding.
*/

int ap_encode_end(
	appledouble_encode_object *p_ap_encode_obj, 
	PRBool is_aborting)
{
	/*
	** clear up the apple doubler.
	*/
	if (p_ap_encode_obj == NULL)
		return noErr;

	if (p_ap_encode_obj->fileId)			/* close the file if it is open.	*/
		FSClose(p_ap_encode_obj->fileId);

	PR_FREEIF(p_ap_encode_obj->boundary);		/* the boundary string.				*/
	
	return noErr;
}

#endif	/* the ifdef of XP_MAC */

