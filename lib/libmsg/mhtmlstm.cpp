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
/*   mhtmlstm.c --- generation of MIME HTML.
 */

//#ifdef MSG_SEND_MULTIPART_RELATED

#include "msg.h"
#include "net.h"
#include "structs.h"
#include "xp_core.h"
#include "mhtmlstm.h"
#include "xp_mem.h"
#include "xp_str.h"
#include "xp_mcom.h"
#include "xp_file.h"
#include "prprf.h"
#include "xpassert.h"
#include "msgsend.h"
#include "msgsendp.h"
#include "mimeenc.h"
#include "libi18n.h"

extern "C" {
	extern int MK_UNABLE_TO_OPEN_FILE;
}



/* Defined in libnet/mkcache.c */
extern "C" int NET_FindURLInCache(URL_Struct * URL_s, MWContext *ctxt);

/* Defined in layout/editor.cpp */
extern "C" XP_Bool EDT_IsSameURL(char *url1,char *url2,char *base1,char *base2);

/*
----------------------------------------------------------------------
MSG_MimeRelatedFileInfo
----------------------------------------------------------------------
*/


/*
----------------------------------------------------------------------
MSG_MimeRelatedStreamOut
----------------------------------------------------------------------
*/

class MSG_MimeRelatedStreamOut : public IStreamOut
{
public:
	MSG_MimeRelatedStreamOut(XP_File file);
	~MSG_MimeRelatedStreamOut(void);

    virtual void Write( char *pBuffer, int32 iCount );
	//static int WriteMimeData( const char *pBuffer, int32 iCount, void *closure);

    virtual EOutStreamStatus Status();

	virtual intn Close(void);

	XP_File m_file;
	XP_Bool m_hasWritten;
	//MimeEncoderData *m_encoder;
	EOutStreamStatus m_status;
};



MSG_MimeRelatedStreamOut::MSG_MimeRelatedStreamOut(XP_File file)
	: m_file(file), m_hasWritten(FALSE), m_status(EOS_NoError)
{
}

MSG_MimeRelatedStreamOut::~MSG_MimeRelatedStreamOut(void)
{
	Close();
}

void
MSG_MimeRelatedStreamOut::Write(char *pBuffer, int32 iCount)
{
//	if (!m_encoder)
//		(void) WriteMimeData(pBuffer, iCount, this);
//	else
//		(void) MimeEncoderWrite(m_encoder, pBuffer, iCount);

	if (m_status != EOS_NoError)
		return;

	// For now, pass through the information.
	XP_ASSERT(m_file);
	int numBytes = 0;
	if (m_file)
	{
		numBytes = XP_FileWrite(pBuffer, iCount, m_file);
		if (numBytes != iCount)
		{
			TRACEMSG(("MSG_MimeRelatedStreamOut::Write(): error: %d written != %ld to write",numBytes, (long)iCount));
			if (m_hasWritten)
				m_status = EOS_DeviceFull; // wrote in the past, so this means the disk is full
			else
				m_status = EOS_FileError; // couldn't even write once, so assume it's some other problem
		}
	}

	if (!m_hasWritten) m_hasWritten = TRUE;
}

//int
//MSG_MimeRelatedStreamOut::WriteMimeData(const char *pBuffer, int32 iCount, void *closure)
//{
//}

IStreamOut::EOutStreamStatus
MSG_MimeRelatedStreamOut::Status(void)
{
	return m_status;
}

intn
MSG_MimeRelatedStreamOut::Close(void)
{
	if (m_file)
	{
		XP_FileClose(m_file);
		m_file = 0;
	}
	return 0;
}

/*
----------------------------------------------------------------------
MSG_MimeRelatedSubpart
----------------------------------------------------------------------
*/

char *
MSG_MimeRelatedSubpart::GenerateCIDHeader(void)
{
	char *header;

	// If we have a Content-ID, generate that.
	header = (m_pContentID ? 
		PR_smprintf("Content-ID: <%s>", m_pContentID) : 0);
	// If we have no Content-ID but an original URL, send that.
	if (!header && m_pOriginalURL)
		header = PR_smprintf("Content-Location: %s", m_pOriginalURL);
	// If none of the above and we have a local URL, use that instead.
	if (!header && m_pLocalURL)
		header = PR_smprintf("Content-Location: %s", m_pLocalURL);
	if (!header)
		header = XP_STRDUP("");

	return header;
}

char *
MSG_MimeRelatedSubpart::GenerateEncodingHeader(void)
{
	char *header = NULL;

	if (m_pEncoding)
		header = PR_smprintf("Content-Transfer-Encoding: %s", m_pEncoding);
	else
		header = XP_STRDUP("");	

	return header;
}

char *
MSG_MimeRelatedSubpart::GetContentID(XP_Bool bAttachMIMEPrefix)
{
	char *result = NULL;

	if (m_pContentID)
	{
		result = PR_smprintf("%s%s", (bAttachMIMEPrefix ? "cid:" : ""),
							 m_pContentID);
	}

	return result;
}


int
MSG_MimeRelatedSubpart::GetStreamOut(IStreamOut **pReturn)
{
	int result = 0;

	if (!m_pStreamOut)
	{
		if (m_filename)
		{
			XP_File file = XP_FileOpen(m_filename, xpFileToPost, XP_FILE_WRITE_BIN);
			if (file)
			{
				m_pStreamOut = new MSG_MimeRelatedStreamOut(file);
			}
		}
	}
	if (!m_pStreamOut) 
		result = MK_UNABLE_TO_OPEN_FILE; /* -1; rb */
	*pReturn = (IStreamOut *) m_pStreamOut;

	return result;
}

int
MSG_MimeRelatedSubpart::CloseStreamOut(void)
{
	int result = 0;

	if (m_pStreamOut)
	{
		delete m_pStreamOut; // this will close the stream
		m_pStreamOut = NULL;
	}

	return result;
}

MSG_MimeRelatedSubpart::MSG_MimeRelatedSubpart(MSG_MimeRelatedSaver *parent,
											   char *pContentID,
											   char *pOriginal, char *pLocal,
											   char *pMime, int16 part_csid, char *pFilename)
	: MSG_SendPart(NULL, part_csid), m_pOriginalURL(NULL), 
	  m_pLocalURL(NULL), m_pParentFS(parent),
	  m_pContentID(NULL), m_pContentName(NULL), m_rootPart(FALSE)
{
	m_filetype = xpFileToPost;
	
	if (pOriginal)
		m_pOriginalURL = XP_STRDUP(pOriginal);
	if (pLocal)
		m_pLocalURL = XP_STRDUP(pLocal);
	if (pMime)
		m_type = XP_STRDUP(pMime);
	if (pContentID)
		m_pContentID = XP_STRDUP(pContentID);

	if ((!m_pOriginalURL) && (!m_type))
	{
		// Assume we're saving an untitled HTML document (the root part) if
		// we're given neither the name nor the type.
		m_type = XP_STRDUP(TEXT_HTML);
	}

	if (pFilename)
	{
		CopyString(&m_filename, pFilename);
		m_rootPart = TRUE;
	}
	else
	{
		// Generate a temp name for a file to which to write.
		char *tmp = WH_TempName(xpFileToPost, "nsmail");
		if (tmp)
		{
			CopyString(&m_filename, tmp);
			XP_FREE(tmp);
		}
	}

	// If we have a filename, create the file now so that
	// the Mac doesn't generate the same file name twice.
	if (m_filename)
	{
		XP_File fp = XP_FileOpen(m_filename, xpFileToPost, XP_FILE_WRITE_BIN);
		if (fp)
			XP_FileClose(fp);
	}

	//XP_ASSERT(m_type != NULL);
	XP_ASSERT(m_filename != NULL);
	//XP_ASSERT(m_pContentID != NULL);
}

MSG_MimeRelatedSubpart::~MSG_MimeRelatedSubpart(void)
{
	// Close any streams we may have had open.
	if (m_pStreamOut)
		delete m_pStreamOut;

	// Delete the file we represent.
	if (m_filename)
	{
		XP_FileRemove(m_filename, xpFileToPost);
	}

	XP_FREEIF(m_pOriginalURL);
	XP_FREEIF(m_pLocalURL);
	XP_FREEIF(m_pContentID);
	XP_FREEIF(m_pEncoding);
	XP_FREEIF(m_pContentName);

	m_pOriginalURL = m_pLocalURL = NULL;
}

int
MSG_MimeRelatedSubpart::WriteEncodedMessageBody(const char *buf, int32 size, 
												void *pPart)
{
	MSG_MimeRelatedSubpart *subpart = (MSG_MimeRelatedSubpart *) pPart;
	int returnVal = 0;

	XP_ASSERT(subpart->m_state != NULL);
	if (subpart->m_state)
		returnVal = mime_write_message_body(subpart->m_state, (char *) buf, size);

	return returnVal;
}

void
MSG_MimeRelatedSubpart::CopyURLInfo(const URL_Struct *pURL)
{
	char *suffix = NULL;
	if (pURL != NULL)
	{
		// Get the MIME type if we have it.
		if (pURL->content_type && *(pURL->content_type))
			SetType(pURL->content_type);

		// Look for a content name in this order:
		// 1. If we have a content name, use that as is.
		// 2. If we have a content type, find an extension 
		//    corresponding to the content type, and attach it to 
		//    the temp filename.
		// 3. If we have neither a content name nor a content type,
		//    duplicate the temp filename as is. (Yuck.)
		if (pURL->content_name && *(pURL->content_name))
			m_pContentName = XP_STRDUP(pURL->content_name);

		else if (pURL->content_type && (suffix = NET_cinfo_find_ext(pURL->content_type)) != NULL)
		{
			// We found an extension locally, add it to the temp name.
			char *end = XP_STRRCHR(m_filename, '.');

			if (end)
				*end = '\0';

			m_pContentName = PR_smprintf("%s.%s", m_filename, suffix);

			if (end)
				*end = '.';
		}
	}

	if (!m_pContentName)
		m_pContentName = XP_STRDUP(m_filename);
}

int
MSG_MimeRelatedSubpart::Write(void)
{
	// If we weren't given the mime type by the editor,
	// then attempt to deduce it from what information we can get.
	
	if ((m_pOriginalURL) && (!m_type))
	{
		// We weren't explicitly given the MIME type, so
		// we ask the cache if it knows anything about this URL.
 		URL_Struct testUrl;
		XP_MEMSET(&testUrl, 0, sizeof(URL_Struct));

 		testUrl.address = m_pOriginalURL;
 		int findResult = NET_FindURLInCache(&testUrl, 
 											m_pParentFS->GetContext());
 		if ((findResult != 0) && (testUrl.content_type))
 		{
 			// Got a MIME type from the cache.
 			m_type = XP_STRDUP(testUrl.content_type);
 		}
	}

	if ((m_pOriginalURL) && (!m_type))
	{
		// Either we didn't find the URL in the cache, or
		// we have it but don't know its MIME type.
		// So, we trot out our last resort: attempt to grok
		// the MIME type based on the filename. (Mr. Yuk says: "Yuk.")
		
		NET_cinfo *pMimeInfo = NET_cinfo_find_type(m_pOriginalURL);
		if ((pMimeInfo) && (pMimeInfo->type))
		{
			// Got a MIME type based on the filename.
			m_type = XP_STRDUP(pMimeInfo->type);
		}
	}

	if (!m_type)
	{
		// No matter what we've done, we will never figure out the type. 
		// So, we punt and call it an application/octet-stream.
		m_type = XP_STRDUP(APPLICATION_OCTET_STREAM);
	}

	// Determine what the encoding of this data should be depending
	// on the MIME type. This is fairly braindead: base64 encode anything
	// that isn't text.
	if (m_type && (!m_rootPart))
	{
		// Uuencode only if we have to, otherwise use base64
		if (m_pParentFS->m_pPane->
			GetCompBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK))
		{
			m_pEncoding = XP_STRDUP(ENCODING_UUENCODE);			
			SetEncoderData(MimeUUEncoderInit(m_pContentName ? m_pContentName : "",
#ifdef XP_OS2
											 (int (_Optlink*) (const char*,int32,void*))
#endif
											 WriteEncodedMessageBody, 
											 this));
		}
		else
		{
			m_pEncoding = XP_STRDUP(ENCODING_BASE64);
			SetEncoderData(MimeB64EncoderInit(
#ifdef XP_OS2
								(int (_Optlink*) (const char*,int32,void*))
#endif
								 WriteEncodedMessageBody,
								 this));
		}
	}

	// Horrible hack: if we got a local filename then we're the root lump,
	// hence we don't generate a content ID header in that case
	// (but we do in all other cases where we have a content ID)
	char *cidHeader = NULL;
	if ((!m_rootPart) && (m_pContentID))
	{
		cidHeader = GenerateCIDHeader();
		if (cidHeader)
		{
			AppendOtherHeaders(cidHeader);
			AppendOtherHeaders(CRLF);
			XP_FREE(cidHeader);
		}
	}

	if (m_pEncoding)
	{
		char *encHeader = GenerateEncodingHeader();
		if (encHeader)
		{
			AppendOtherHeaders(encHeader);
			AppendOtherHeaders(CRLF);
			XP_FREE(encHeader);
		}
	}
	
	if ((!m_rootPart) && (m_pOriginalURL))
	{
		char *fileHeader = PR_smprintf("Content-Disposition: inline; filename=\"%s\"",
										m_pContentName ? m_pContentName : "");
		if (fileHeader)
		{
			AppendOtherHeaders(fileHeader);
			AppendOtherHeaders(CRLF);
			XP_FREE(fileHeader);
		}
	}

	return MSG_SendPart::Write();
}

/*
----------------------------------------------------------------------
MSG_MimeRelatedParentPart
----------------------------------------------------------------------
*/

MSG_MimeRelatedParentPart::MSG_MimeRelatedParentPart(int16 part_csid)
: MSG_SendPart(NULL, part_csid)
{
}

MSG_MimeRelatedParentPart::~MSG_MimeRelatedParentPart(void)
{
}

/*
----------------------------------------------------------------------
MSG_MimeRelatedSaver
----------------------------------------------------------------------
*/

extern char * msg_generate_message_id(void);

// Constructor
MSG_MimeRelatedSaver::MSG_MimeRelatedSaver(MSG_CompositionPane *pane, 
										   MWContext *context, 
										   MSG_CompositionFields *fields,
										   XP_Bool digest_p, 
										   MSG_Deliver_Mode deliver_mode,
										   const char *body, 
										   uint32 body_length,
										   MSG_AttachedFile *attachedFiles,
										   DeliveryDoneCallback cb,
										   char **ppOriginalRootURL)
	: m_pContext(context), m_pBaseURL(NULL), m_pPane(pane),
	  m_pFields(fields), m_digest(digest_p), m_deliverMode(deliver_mode),
	  m_pBody(body), m_bodyLength(body_length), 
	  m_pAttachedFiles(attachedFiles), m_cbDeliveryDone(cb), m_pSourceBaseURL(NULL)

{
	// Generate the message ID.
	m_pMessageID = msg_generate_message_id();
	if (m_pMessageID)
	{
		// Massage the message ID so that it can be used for generating
		// part IDs. For now, just remove the angle brackets.
		m_pMessageID[strlen(m_pMessageID)-1] = '\0'; // shorten the end by 1
		char *temp = XP_STRDUP(&(m_pMessageID[1])); // and strip off the leading '<'
		if (temp)
		{
			XP_FREE(m_pMessageID);
			m_pMessageID = temp;
		}
	}
	XP_ASSERT(m_pMessageID);

	// Create the part object that we represent.
	m_pPart = new MSG_MimeRelatedParentPart(INTL_DefaultWinCharSetID(context));
	XP_ASSERT(m_pPart);

	if ((ppOriginalRootURL != NULL) && *(ppOriginalRootURL))
	{
		// Have a valid string of some sort, wait to be added.
	}
	else if (ppOriginalRootURL != NULL)
	{
		// ### mwelch This is a hack, required because EDT_SaveFileTo
		//            requires a source URL string, even if the document
		//            is currently untitled. The hack consists of adding
		//            the return parameter in the constructor, and passing
		//            back an improvised source URL if we were not given one.
		//
		// Autogenerate the title and pass it back.
		m_rootFilename = WH_TempName(xpFileToPost,"nsmail");
		XP_ASSERT(m_rootFilename);
		char * temp = WH_FileName(m_rootFilename, xpFileToPost);
		*ppOriginalRootURL = XP_PlatformFileToURL(temp);
		if (temp)
			XP_FREE( temp );
	}

	// Set our type to be multipart/related.
	m_pPart->SetType(MULTIPART_RELATED);
}

// Destructor
MSG_MimeRelatedSaver::~MSG_MimeRelatedSaver(void)
{
  XP_FREEIF(m_pSourceBaseURL);
  if (m_rootFilename) 
  {
	  XP_FileRemove(m_rootFilename, xpFileToPost);
	  XP_FREEIF(m_rootFilename);
  }
  XP_FREEIF(m_pMessageID);
}

intn MSG_MimeRelatedSaver::GetType()
{
  return ITapeFileSystem::MailSend;
}

MSG_MimeRelatedSubpart *
MSG_MimeRelatedSaver::GetSubpart(intn iFileIndex)
{
	XP_ASSERT(m_pPart != NULL);

	MSG_MimeRelatedSubpart *part = 
		(MSG_MimeRelatedSubpart *) m_pPart->GetChild(iFileIndex);

	return part;
}

// This function is called before anything else.
// Tell the file system the base URL it is going to see.
void
MSG_MimeRelatedSaver::SetSourceBaseURL(char *pURL)
{
  XP_FREEIF(m_pSourceBaseURL);
	m_pSourceBaseURL = XP_STRDUP(pURL);

#if 0
	MSG_MimeRelatedSubpart *part = NULL;
	// Remember the URL for later.
	m_pBaseURL = pURL;

	// Add this URL as the first in the list.
	if (m_pPart->GetNumChildren() == 0)
	{
		AddFile(pURL);
	}
	else
	{
		part = GetSubpart(0);
		if (part)
		{
			XP_FREEIF(part->m_pOriginalURL);
			part->m_pOriginalURL = (pURL == NULL) ? NULL : XP_STRDUP(pURL);
		}
	}

	// Fix the local URL/filename reference if we haven't already opened the file.
	if (!part) 
		part = GetSubpart(0);
	if (part && (part->m_pStreamOut == NULL))
	{
		XP_FREEIF(part->m_pLocalURL);
		part->m_pLocalURL = PR_smprintf("file:%s",part->GetFilename());
		XP_ASSERT(part->m_pLocalURL);

		part->SetOtherHeaders(""); // no headers for the lead part
	}
#endif
}

char* 
MSG_MimeRelatedSaver::GetSourceURL(intn iFileIndex)
{
	char *result = NULL;

	MSG_MimeRelatedSubpart *part = GetSubpart(iFileIndex);
	if (part->m_pOriginalURL) {
    // Try to make absolute relative to value set in MSG_MimeRelatedSaver::SetSourceBaseURL().
    if (m_pSourceBaseURL) {
      result = NET_MakeAbsoluteURL(m_pSourceBaseURL,part->m_pOriginalURL);
    }
    else {
  		result = XP_STRDUP(part->m_pOriginalURL);
    }
  }

	return result;
}

// Add a name to the file system.
// Returns the index of the file added (0 based). 
intn
MSG_MimeRelatedSaver::AddFile(char * pURL, char * pMimeType, int16 part_csid)
{
	intn returnValue = 0;
	MSG_MimeRelatedSubpart *newPart = NULL;
	int i;

	// See if a part with this url already exists.
	for(i=0;(i<m_pPart->GetNumChildren()) && (!newPart);i++)
	{
		newPart = GetSubpart(i);
    // Use EDT_IsSameURL to deal with case insensitivity on MAC and Win16
		if (!newPart || 
        !EDT_IsSameURL(newPart->m_pOriginalURL, pURL,m_pSourceBaseURL,m_pSourceBaseURL)) {
			newPart = NULL; // not this one, try the next one
    }
    else {
      // found it.
      returnValue = i;
    }
	}
	
	if (newPart == NULL)
	{
		// Generate a Content ID. This will look a lot like our message ID,
		// except that we will add the part number.
		XP_ASSERT(m_pMessageID != NULL);
		char *newPartID = PR_smprintf("part%ld.%s", (long) m_pPart->GetNumChildren(), 
									  m_pMessageID);
		XP_ASSERT(newPartID != NULL);

		char *newLocalURL = PR_smprintf("cid:%s",newPartID);
		XP_ASSERT(newLocalURL != NULL);

		returnValue = m_pPart->GetNumChildren();

		if (m_pPart->GetNumChildren() == 0)
			// This is the root file in the file system.
			newPart = new MSG_MimeRelatedSubpart(this, newPartID, pURL, 
												 pURL, TEXT_HTML, part_csid, 
												 m_rootFilename);
		else
			newPart = new MSG_MimeRelatedSubpart(this, newPartID, pURL, 
												 newLocalURL, pMimeType, part_csid	);

		if (newPart)
			m_pPart->AddChild(newPart);
		else
			returnValue = (intn) ITapeFileSystem::Error; // an error since 0 is the base URL (??)

		XP_FREEIF(newPartID);
		XP_FREEIF(newLocalURL);
	}

	return returnValue;
}

intn
MSG_MimeRelatedSaver::GetNumFiles(void)
{ 
	return m_pPart->GetNumChildren(); 
}

char *
MSG_MimeRelatedSaver::GetDestAbsURL()
{
  // No meaningful destination URL for sending mail.
  return NULL;
}

// Get the name of the relative url to place in the file.
char *
MSG_MimeRelatedSaver::GetDestURL(intn iFileIndex)
{
	char *result = NULL;

	MSG_MimeRelatedSubpart *thePart = GetSubpart(iFileIndex);
	if (thePart != NULL)
	{
		result = XP_STRDUP(thePart->m_pLocalURL);
	}

	return result;
}

char *
MSG_MimeRelatedSaver::GetDestPathURL(void)
{
	//return XP_STRDUP(""); // no path prefix for content IDs
    return NULL;
}

// Return the name to display when saving the file.
char *
MSG_MimeRelatedSaver::GetHumanName(intn iFileIndex)
{
	char *result = NULL;

	MSG_MimeRelatedSubpart *thePart = GetSubpart(iFileIndex);
	if (thePart != NULL)
	{
		result = thePart->m_pOriginalURL;
		char *end = XP_STRRCHR(result, '/');
		if (end)
			result = XP_STRDUP(++end);
		else
			result = XP_STRDUP(result);
	}
	return result;
}

// Open the output stream.
IStreamOut *
MSG_MimeRelatedSaver::OpenStream(intn iFileIndex)
{
	intn theError = 0;
	IStreamOut *pStream = NULL; // in case we fail

	// Create a stream object that can be written to.
	MSG_MimeRelatedSubpart *thePart = GetSubpart(iFileIndex);
	if (thePart)
	{
		theError = (intn) thePart->GetStreamOut(&pStream);
	}
	return pStream;
}

void 
MSG_MimeRelatedSaver::CopyURLInfo(intn iFileIndex, const URL_Struct *pURL)
{
	MSG_MimeRelatedSubpart *thePart = GetSubpart(iFileIndex);
	if (thePart != NULL)
		thePart->CopyURLInfo(pURL);
}

// Close the output stream.
// Called on completion. bSuccess is TRUE if completed successfully, 
// FALSE if it failed.
void
MSG_MimeRelatedSaver::Complete(Bool bSuccess, 
                                                       EDT_ITapeFileSystemComplete *pfComplete, void *pArg )
{
 	m_pEditorCompletionFunc = pfComplete;
	m_pEditorCompletionArg = pArg;

	// Call StartMessageDelivery (and should) if we
	// were told to at creation time.
	if (bSuccess)
	{
		// If we only generated a single HTML part, treat that as 
		// the root part.
		if (m_pPart->GetNumChildren() == 1)
		{
			MSG_SendPart *tempPart = m_pPart->DetachChild(0);
			delete m_pPart;
			m_pPart = tempPart;
		}

		msg_StartMessageDeliveryWithAttachments(m_pPane, this,
												m_pFields,
												m_digest, FALSE, 
												m_deliverMode,
												TEXT_HTML,
												m_pBody, m_bodyLength,
												m_pAttachedFiles,
												m_pPart,
#ifdef XP_OS2
												(void (_Optlink*) (MWContext*,void*,int,const char*))
#endif
												MSG_MimeRelatedSaver::UrlExit);
	}
	else
	{
		// delete the contained part since we failed
		delete m_pPart;
		m_pPart = NULL;

    // Call our UrlExit routine to perform cleanup.
    UrlExit(m_pPane->GetContext(), this, MK_INTERRUPTED, NULL);
	}
}

void
MSG_MimeRelatedSaver::UrlExit(MWContext *context, void *fe_data, int status,
							  const char *error_message)
{
  MSG_MimeRelatedSaver *saver = (MSG_MimeRelatedSaver *) fe_data;
  XP_ASSERT(saver != NULL);
  if (saver)
	{
	  if (saver->m_pEditorCompletionFunc)
		{
		  (*(saver->m_pEditorCompletionFunc))((status == 0), 
											  saver->m_pEditorCompletionArg);
		}
	  if (saver->m_cbDeliveryDone)
		{
		  (*(saver->m_cbDeliveryDone))(context,saver->m_pPane,
									   status,error_message);
		}
	}
  delete saver; // the part within stays around, we don't
}

void 
MSG_MimeRelatedSaver::CloseStream( intn iFileIndex )
{
	// Get the piece whose stream we will close.
	MSG_MimeRelatedSubpart *thePart = GetSubpart(iFileIndex);
	if (thePart)
	{
		thePart->CloseStreamOut();
	}
}

XP_Bool
MSG_MimeRelatedSaver::FileExists( intn /*iFileIndex*/ )
{
	return FALSE;
}

XP_Bool 
MSG_MimeRelatedSaver::IsLocalPersistentFile(intn /*iFileIndex*/) {
    return FALSE;
}

//#endif
