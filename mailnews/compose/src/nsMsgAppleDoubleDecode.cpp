/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

void 
DecodingDone( appledouble_decode_object* p_ap_decode_obj )
{
	FSSpec	fspec;
			
	fspec.vRefNum = p_ap_decode_obj->vRefNum;
	fspec.parID   = p_ap_decode_obj->dirId;
	fspec.name[0] = nsCRT::strlen(p_ap_decode_obj->fname);
	strcpy((char*)fspec.name+1, p_ap_decode_obj->fname);
	CMimeMapper * mapper = CPrefs::sMimeTypes.FindMimeType(fspec);
	if( mapper && (mapper->GetLoadAction() == CMimeMapper::Launch ) )
	{
		 LFileBufferStream file( fspec );
		 LaunchFile( &file );
	}	
}

#pragma cplusplus reset

#endif	/* the ifdef of XP_MAC */


/* 
** The initial of the apple double decoder.
**
**	 Set up the next output stream based on the input.
*/
int ap_decode_init(
	appledouble_decode_object* p_ap_decode_obj,
	PRBool	is_apple_single, 
	PRBool	write_as_binhex,
	void  	*closure)
{	
	memset(p_ap_decode_obj, 0, sizeof(appledouble_decode_object));
	
	/* presume first buff starts a line */
	p_ap_decode_obj->uu_starts_line = TRUE; 

	if (write_as_binhex)
	{
		p_ap_decode_obj->write_as_binhex = TRUE;
		p_ap_decode_obj->binhex_stream   = (NET_StreamClass*)closure;
		p_ap_decode_obj->data_size       = 0;
	}
	else
	{
		p_ap_decode_obj->write_as_binhex = FALSE;
		p_ap_decode_obj->binhex_stream   = NULL;
		
		p_ap_decode_obj->context = (MWContext*)closure;
	}
	
	p_ap_decode_obj->is_apple_single = is_apple_single;
	
	if (is_apple_single)
	{
		p_ap_decode_obj->encoding = kEncodeNone;
	}
	
	return NOERR;
}

static int ap_decode_state_machine(appledouble_decode_object* p_ap_decode_obj);
/*
*	process the buffer 
*/
int ap_decode_next(
	appledouble_decode_object* p_ap_decode_obj, 
	char 	*in_buff, 
	PRInt32 	buff_size)
{	
	/*
	** install the buff to the decoder.
	*/
	p_ap_decode_obj->inbuff   	= in_buff;
	p_ap_decode_obj->s_inbuff 	= buff_size;
	p_ap_decode_obj->pos_inbuff = 0;
	
	/*
	**	run off the decode state machine
	*/	
	return ap_decode_state_machine(p_ap_decode_obj);
}

PRIVATE int ap_decode_state_machine(
	appledouble_decode_object* p_ap_decode_obj)
{
	int 	status = NOERR;
	PRInt32 	size;
		
	switch (p_ap_decode_obj->state)
	{
		case kInit:
			/*
			**	Make sure that there are stuff in the buff 
			**		before we can parse the file head .
			*/
			if (p_ap_decode_obj->s_inbuff <=1 )
				return NOERR;
			
			if (p_ap_decode_obj->is_apple_single)
			{
				p_ap_decode_obj->state = kBeginHeaderPortion;
			}
			else
			{
				status = ap_seek_part_start(p_ap_decode_obj);
				if (status != errDone)
					return status;
	
				p_ap_decode_obj->state = kBeginParseHeader;
			}
			status = ap_decode_state_machine(p_ap_decode_obj);
			break;
		
		case kBeginSeekBoundary:
			p_ap_decode_obj->state = kSeekingBoundary;
			status = ap_seek_to_boundary(p_ap_decode_obj, TRUE);
			if (status == errDone)
			{
				p_ap_decode_obj->state = kBeginParseHeader;
				status = ap_decode_state_machine(p_ap_decode_obj);
			}	
			break;
			
		case kSeekingBoundary:
			status = ap_seek_to_boundary(p_ap_decode_obj, FALSE);
			if (status == errDone)
			{
				p_ap_decode_obj->state = kBeginParseHeader;
				status = ap_decode_state_machine(p_ap_decode_obj);
			}
			break;
		
		case kBeginParseHeader:
			p_ap_decode_obj->state = kParsingHeader;
			status = ap_parse_header(p_ap_decode_obj, TRUE);
			if (status == errDone)
			{
				if (p_ap_decode_obj->which_part == kDataPortion)
					p_ap_decode_obj->state = kBeginDataPortion;
				else if (p_ap_decode_obj->which_part == kHeaderPortion)
					p_ap_decode_obj->state = kBeginHeaderPortion;
				else
					p_ap_decode_obj->state = kFinishing;
	
				status = ap_decode_state_machine(p_ap_decode_obj);
			}
			break;
				
		case kParsingHeader:
			status = ap_parse_header(p_ap_decode_obj, FALSE);
			if (status == errDone)
			{
				if (p_ap_decode_obj->which_part == kDataPortion)
					p_ap_decode_obj->state = kBeginDataPortion;
				else if (p_ap_decode_obj->which_part == kHeaderPortion)
					p_ap_decode_obj->state = kBeginHeaderPortion;
				else
					p_ap_decode_obj->state = kFinishing;
										
				status = ap_decode_state_machine(p_ap_decode_obj);
			
			}
			break;
				
		case kBeginHeaderPortion:
			p_ap_decode_obj->state = kProcessingHeaderPortion;
			status = ap_decode_process_header(p_ap_decode_obj, TRUE);
			if (status == errDone)
			{
				if (p_ap_decode_obj->is_apple_single)
					p_ap_decode_obj->state = kBeginDataPortion;
				else
					p_ap_decode_obj->state = kBeginSeekBoundary;
					
				status = ap_decode_state_machine(p_ap_decode_obj);
			}
			break;
		case kProcessingHeaderPortion:
			status = ap_decode_process_header(p_ap_decode_obj, FALSE);
			if (status == errDone)
			{
				if (p_ap_decode_obj->is_apple_single)
					p_ap_decode_obj->state = kBeginDataPortion;
				else
					p_ap_decode_obj->state = kBeginSeekBoundary;
					
				status = ap_decode_state_machine(p_ap_decode_obj);
			}
			break;
		
		case kBeginDataPortion:
			p_ap_decode_obj->state = kProcessingDataPortion;
			status = ap_decode_process_data(p_ap_decode_obj, TRUE);
			if (status == errDone)
			{
				if (p_ap_decode_obj->is_apple_single)
					p_ap_decode_obj->state = kFinishing;
				else
					p_ap_decode_obj->state = kBeginSeekBoundary;
					
				status = ap_decode_state_machine(p_ap_decode_obj);
			}
			break;
		
		case kProcessingDataPortion:
			status = ap_decode_process_data(p_ap_decode_obj, FALSE);
			if (status == errDone)
			{
				if (p_ap_decode_obj->is_apple_single)
					p_ap_decode_obj->state = kFinishing;
				else
					p_ap_decode_obj->state = kBeginSeekBoundary;
		
				status = ap_decode_state_machine(p_ap_decode_obj);
			}
			break;
			
		case kFinishing:
			if (p_ap_decode_obj->write_as_binhex)
			{
				if (p_ap_decode_obj->tmpfd)
				{
					/*
					**	It is time to append the data fork to bin hex encoder.
					**	
					**	The reason behind this dirt work is resource fork is the last
					**	piece in the binhex, while it is the first piece in apple double. 
					*/
					p_ap_decode_obj->tmpFileStream->seek(PR_SEEK_SET, 0);
					
					while (p_ap_decode_obj->data_size > 0)
					{
						char buff[1024];
					
						size = PR_MIN(1024, p_ap_decode_obj->data_size);
						p_ap_decode_obj->tmpFileStream->read(buff, size);					
						status = (*p_ap_decode_obj->binhex_stream->put_block)
									(p_ap_decode_obj->binhex_stream->data_object, 
									buff, 
									size);
						
						p_ap_decode_obj->data_size -= size;
					}
				}
				
				if (p_ap_decode_obj->data_size <= 0)
				{
					/* CALL put_block with size == 0 to close a part. */
					status = (*p_ap_decode_obj->binhex_stream->put_block)
								(p_ap_decode_obj->binhex_stream->data_object, 
								NULL, 
								0);
					if (status != NOERR)
						break;
											
					/* and now we are really done.					*/
					status = errDone;
				}
				else
					status = NOERR;
			}
			break;
	}						
	return (status == errEOB) ? NOERR : status;	
}

int ap_decode_end(
	appledouble_decode_object* p_ap_decode_obj, 
	PRBool 	is_aborting)
{
	/*
	** clear up the apple doubler object.
	*/
	if (p_ap_decode_obj == NULL)
		return NOERR;
		
	PR_FREEIF(p_ap_decode_obj->boundary0);

#ifdef	XP_MAC
	if (p_ap_decode_obj->fileId)
		FSClose(p_ap_decode_obj->fileId);
	if( p_ap_decode_obj->vRefNum )
		FlushVol(nil, p_ap_decode_obj->vRefNum );
#endif

	if (p_ap_decode_obj->write_as_binhex)
	{
		/*		
		** make sure close the binhex stream too. 
		*/
		if (is_aborting)
		{
			(*p_ap_decode_obj->binhex_stream->abort)
				(p_ap_decode_obj->binhex_stream->data_object, 0);		
		}
		else
		{
			(*p_ap_decode_obj->binhex_stream->complete)
				(p_ap_decode_obj->binhex_stream->data_object);		
		}

		if (p_ap_decode_obj->tmpFileStream)
			p_ap_decode_obj->tmpFileStream->close();
		
		if (p_ap_decode_obj->tmpFileSpec)
		{
			p_ap_decode_obj->tmpFileSpec->Delete(PR_FALSE); /* remove tmp file if we used it	*/	
			delete p_ap_decode_obj->tmpFileSpec;
		}
	}
	else if (p_ap_decode_obj->fileStream)
	{
    p_ap_decode_obj->fileStream->close();
	}
#ifdef XP_MAC
	if( !is_aborting )
		DecodingDone( p_ap_decode_obj);
#endif
	return NOERR;

}
