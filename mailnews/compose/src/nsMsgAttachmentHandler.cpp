/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgAttachmentHandler.h"
#include "nsMsgSend.h"
#include "nsMsgCompUtils.h"
#include "nsIPref.h"
#include "nsMsgEncoders.h"
#include "nsMsgI18N.h"
#include "nsURLFetcher.h"
#include "nsMimeTypes.h"
#include "nsMsgComposeStringBundle.h"
#include "nsXPIDLString.h"


static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

///////////////////////////////////////////////////////////////////////////
// RICHIE_MAC - for the Mac, when can we get rid of this stuff?
///////////////////////////////////////////////////////////////////////////
// AT SOME POINT, WE NEED TO FIGURE OUT A SOLUTION FOR THIS MAC ONLY STUFF
///////////////////////////////////////////////////////////////////////////

#ifdef XP_MAC
#include "errors.h"
#include "m_cvstrm.h"
//
// Need a routine for the Mac to see if this is a Mac file?
//
static PRBool
nsMsgIsMacFile(char *filename)
{
  return PR_FALSE;
}

//
// Need a routine for the Mac to get type information
//
void
MacGetFileType(nsFileSpec *fs, PRBool *useDefault, char **type, char **encoding)
{
  *useDefault = PR_TRUE;
}
#endif /* XP_MAC */

//
// Class implementation...
//
nsMsgAttachmentHandler::nsMsgAttachmentHandler()
{
  m_charset = NULL;
	m_override_type = NULL;
	m_override_encoding = NULL;
	m_desired_type = NULL;
	m_description = NULL;
	m_encoding = NULL;
	m_real_name = NULL;
	m_mime_delivery_state = NULL;
	m_encoding = NULL;
	m_already_encoded_p = PR_FALSE;
	m_decrypted_p = PR_FALSE;
	
  // For analyzing the attachment file...
  m_file_analyzed = PR_FALSE;
  m_ctl_count = 0;
	m_null_count = 0;
	m_current_column = 0;
	m_max_column = 0;
	m_lines = 0;
	m_unprintable_count = 0;
  m_highbit_count = 0;

  // Mime encoder...
  m_encoder_data = NULL;

  m_done = PR_FALSE;
	m_type = nsnull;
	m_size = 0;

  mCompFields = nsnull;   // Message composition fields for the sender
  mFileSpec = nsnull;
  mOutFile = nsnull;
  mURL = nsnull;
  mFetcher = nsnull;

  m_x_mac_type = nsnull;
	m_x_mac_creator = nsnull;

#ifdef XP_MAC
	mAppleFileSpec = nsnull;
#endif

  mDeleteFile = PR_FALSE;
}

nsMsgAttachmentHandler::~nsMsgAttachmentHandler()
{
}

void
nsMsgAttachmentHandler::AnalyzeDataChunk(const char *chunk, PRInt32 length)
{
  unsigned char *s = (unsigned char *) chunk;
  unsigned char *end = s + length;
  for (; s < end; s++)
	{
	  if (*s > 126)
		{
		  m_highbit_count++;
		  m_unprintable_count++;
		}
	  else if (*s < ' ' && *s != '\t' && *s != CR && *s != LF)
		{
		  m_unprintable_count++;
		  m_ctl_count++;
		  if (*s == 0)
			m_null_count++;
		}

	  if (*s == CR || *s == LF)
		{
		  if (s+1 < end && s[0] == CR && s[1] == LF)
			s++;
		  if (m_max_column < m_current_column)
			m_max_column = m_current_column;
		  m_current_column = 0;
		  m_lines++;
		}
	  else
		{
		  m_current_column++;
		}
	}
}

void
nsMsgAttachmentHandler::AnalyzeSnarfedFile(void)
{
	char chunk[256];
	PRInt32 numRead = 0;

  if (m_file_analyzed)
    return;

  if (mFileSpec)
	{
		nsInputFileStream fileHdl(*mFileSpec, PR_RDONLY, 0);
		if (fileHdl.is_open())
		{
			do
			{
				numRead = fileHdl.read(chunk, 256);
				if (numRead > 0)
					AnalyzeDataChunk(chunk, numRead);
			}
			while (numRead > 0);

      fileHdl.close();
      m_file_analyzed = PR_TRUE;
		}
	}
}

//
// Given a content-type and some info about the contents of the document,
// decide what encoding it should have.
//
int
nsMsgAttachmentHandler::PickEncoding(const char *charset)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  
  // use the boolean so we only have to test for uuencode vs base64 once
  PRBool needsB64 = PR_FALSE;
  PRBool forceB64 = PR_FALSE;
  
  if (m_already_encoded_p)
    goto DONE;
  
  AnalyzeSnarfedFile();

  /* Allow users to override our percentage-wise guess on whether
	the file is text or binary */
  if (NS_SUCCEEDED(rv) && prefs) 
    prefs->GetBoolPref ("mail.file_attach_binary", &forceB64);
  
  if (forceB64 || mime_type_requires_b64_p (m_type))
  {
  /* If the content-type is "image/" or something else known to be binary,
  always use base64 (so that we don't get confused by newline
  conversions.)
	   */
    needsB64 = PR_TRUE;
  }
  else
  {
  /* Otherwise, we need to pick an encoding based on the contents of
  the document.
	   */
    
    PRBool encode_p;
    
    if (m_max_column > 900)
      encode_p = PR_TRUE;
    else if (UseQuotedPrintable() && m_unprintable_count)
      encode_p = PR_TRUE;
    
      else if (m_null_count)	/* If there are nulls, we must always encode,
        because sendmail will blow up. */
        encode_p = PR_TRUE;
      else
        encode_p = PR_FALSE;
      
        /* MIME requires a special case that these types never be encoded.
      */
      if (!PL_strncasecmp (m_type, "message", 7) ||
        !PL_strncasecmp (m_type, "multipart", 9))
      {
        encode_p = PR_FALSE;
        if (m_desired_type && !PL_strcasecmp (m_desired_type, TEXT_PLAIN))
        {
          PR_Free (m_desired_type);
          m_desired_type = 0;
        }
      }

      // If the Mail charset is ISO_2022_JP we force it to use Base64 for attachments (bug#104255). 
      // Use 7 bit for other STATFUL charsets ( e.g. ISO_2022_KR). 
      if ((PL_strcasecmp(charset, "iso-2022-jp") == 0) &&
        (PL_strcasecmp(m_type, TEXT_HTML) == 0))
        needsB64 = PR_TRUE;
      else if((nsMsgI18Nstateful_charset(charset)) &&
        ((PL_strcasecmp(m_type, TEXT_HTML) == 0) ||
        (PL_strcasecmp(m_type, TEXT_MDL) == 0) ||
        (PL_strcasecmp(m_type, TEXT_PLAIN) == 0) ||
        (PL_strcasecmp(m_type, TEXT_RICHTEXT) == 0) ||
        (PL_strcasecmp(m_type, TEXT_ENRICHED) == 0) ||
        (PL_strcasecmp(m_type, TEXT_VCARD) == 0) ||
        (PL_strcasecmp(m_type, APPLICATION_DIRECTORY) == 0) || /* text/x-vcard synonym */
        (PL_strcasecmp(m_type, TEXT_CSS) == 0) ||
        (PL_strcasecmp(m_type, TEXT_JSSS) == 0) ||
        (PL_strcasecmp(m_type, MESSAGE_RFC822) == 0) ||
        (PL_strcasecmp(m_type, MESSAGE_NEWS) == 0)))
      {
        PR_FREEIF(m_encoding);
        m_encoding = PL_strdup (ENCODING_7BIT);
      }
      else if (encode_p &&
        m_size > 500 &&
        m_unprintable_count > (m_size / 10))
        /* If the document contains more than 10% unprintable characters,
        then that seems like a good candidate for base64 instead of
        quoted-printable.
        */
        needsB64 = PR_TRUE;
      else if (encode_p) {
        PR_FREEIF(m_encoding);
        m_encoding = PL_strdup (ENCODING_QUOTED_PRINTABLE);
      }
      else if (m_highbit_count > 0) {
        PR_FREEIF(m_encoding);
        m_encoding = PL_strdup (ENCODING_8BIT);
      }
      else {
        PR_FREEIF(m_encoding);
        m_encoding = PL_strdup (ENCODING_7BIT);
      }
  }
  
  if (needsB64)
  {
    //
    // We might have to uuencode instead of base64 the binary data.
    //
    PR_FREEIF(m_encoding);
    if (UseUUEncode_p())
      m_encoding = PL_strdup (ENCODING_UUENCODE);
    else
      m_encoding = PL_strdup (ENCODING_BASE64);
  }
  
  /* Now that we've picked an encoding, initialize the filter.
  */
  NS_ASSERTION(!m_encoder_data, "not-null m_encoder_data");
  if (!PL_strcasecmp(m_encoding, ENCODING_BASE64))
  {
    m_encoder_data = MIME_B64EncoderInit(mime_encoder_output_fn,
      m_mime_delivery_state);
    if (!m_encoder_data) return NS_ERROR_OUT_OF_MEMORY;
  }
  else if (!PL_strcasecmp(m_encoding, ENCODING_UUENCODE))
  {
    char        *tailName = NULL;
    nsXPIDLCString turl;
    
    if (mURL)
    {
      mURL->GetSpec(getter_Copies(turl));
      
      tailName = PL_strrchr(turl, '/');
      if (tailName) 
      {
        char * tmp = tailName;
        tailName = PL_strdup(tailName+1);
        PR_FREEIF(tmp);
      }
    }
    
    if (mURL && !tailName)
    {
      tailName = PL_strrchr(turl, '/');
      if (tailName) 
      {
        char * tmp = tailName;
        tailName = PL_strdup(tailName+1);
        PR_FREEIF(tmp);
      }
    }

    m_encoder_data = MIME_UUEncoderInit((char *)(tailName ? tailName : ""),
      mime_encoder_output_fn,
      m_mime_delivery_state);
    PR_FREEIF(tailName);
    if (!m_encoder_data) return NS_ERROR_OUT_OF_MEMORY;
  }
  else if (!PL_strcasecmp(m_encoding, ENCODING_QUOTED_PRINTABLE))
  {
    m_encoder_data = MIME_QPEncoderInit(mime_encoder_output_fn,
      m_mime_delivery_state);
    if (!m_encoder_data) return NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    m_encoder_data = 0;
  }  
  
  /* Do some cleanup for documents with unknown content type.
    There are two issues: how they look to MIME users, and how they look to
    non-MIME users.
    
      If the user attaches a "README" file, which has unknown type because it
      has no extension, we still need to send it with no encoding, so that it
      is readable to non-MIME users.
      
        But if the user attaches some random binary file, then base64 encoding
        will have been chosen for it (above), and in this case, it won't be
        immediately readable by non-MIME users.  However, if we type it as
        text/plain instead of application/octet-stream, it will show up inline
        in a MIME viewer, which will probably be ugly, and may possibly have
        bad charset things happen as well.
        
          So, the heuristic we use is, if the type is unknown, then the type is
          set to application/octet-stream for data which needs base64 (binary data)
          and is set to text/plain for data which didn't need base64 (unencoded or
          lightly encoded data.)
  */
DONE:
  if (!m_type || !*m_type || !PL_strcasecmp(m_type, UNKNOWN_CONTENT_TYPE))
  {
    PR_FREEIF(m_type);
    if (m_already_encoded_p)
      m_type = PL_strdup (APPLICATION_OCTET_STREAM);
    else if (m_encoding &&
			   (!PL_strcasecmp(m_encoding, ENCODING_BASE64) ||
         !PL_strcasecmp(m_encoding, ENCODING_UUENCODE)))
         m_type = PL_strdup (APPLICATION_OCTET_STREAM);
    else
      m_type = PL_strdup (TEXT_PLAIN);
  }
  return 0;
}

static nsresult
FetcherURLDoneCallback(nsIURI* aURL, nsresult aStatus, 
                       const char *aContentType,
                       PRInt32 totalSize, 
                       const PRUnichar* aMsg, void *tagData)
{
  nsMsgAttachmentHandler *ma = (nsMsgAttachmentHandler *) tagData;
  NS_ASSERTION(ma != nsnull, "not-null mime attachment");

  if (ma != nsnull)
  {
    ma->m_size = totalSize;
    if (aContentType)
    {
      PR_FREEIF(ma->m_type);
      ma->m_type = PL_strdup(aContentType);
    }

	  return ma->UrlExit(aStatus, aMsg);
  }
  else
    return NS_OK;
}

nsresult
nsMsgAttachmentHandler::SnarfAttachment(nsMsgCompFields *compFields)
{
  nsresult      status = 0;
  char          *tempName = nsnull;
  nsXPIDLCString url_string;

  NS_ASSERTION (! m_done, "Already done");

  if (!mURL)
    return NS_ERROR_INVALID_ARG;

  tempName = GenerateFileNameFromURI(mURL); // Make it a sane name

  mCompFields = compFields;

  // First, get as file spec and create the stream for the
  // temp file where we will save this data
  if ((!tempName) || (!*tempName))
    mFileSpec = nsMsgCreateTempFileSpec("nsmail.tmp");
  else
    mFileSpec = nsMsgCreateTempFileSpec(tempName);

  // Set a sane name for the attachment...
  if (tempName)
    m_real_name = PL_strdup(tempName);

  PR_FREEIF(tempName);

  if (! mFileSpec )
  	return (NS_ERROR_FAILURE);

  mOutFile = new nsOutputFileStream(*mFileSpec, PR_WRONLY | PR_CREATE_FILE);
  if (!mOutFile)
  {
    delete mFileSpec;
    mFileSpec = nsnull;
    return NS_MSG_UNABLE_TO_OPEN_TMP_FILE; 
  }

  mURL->GetSpec(getter_Copies(url_string));

#ifdef XP_MAC
  // do we need to add IMAP: to this list? nsMsgIsLocalFileURL returns PR_FALSE always for IMAP 
  if ( (nsMsgIsLocalFile(url_string) &&	    
	     (PL_strncasecmp(url_string, "mailbox:", 8) != 0)) )
	{
	  // convert the apple file to AppleDouble first, and then patch the
		// address in the url.
	  char  *src_filename = nsMsgGetLocalFileFromURL (url_string);
    if (!src_filename)
      return NS_ERROR_OUT_OF_MEMORY;

		// Only use appledouble if we aren't uuencoding.
	  if( nsMsgIsMacFile(src_filename) && (! UseUUEncode_p()) )
		{
		  char	*separator;

		  separator = mime_make_separator("ad");
		  if (!separator)
      {
        PR_FREEIF(src_filename);
			  return NS_ERROR_OUT_OF_MEMORY;
      }
						
		  mAppleFileSpec = nsMsgCreateTempFileSpec("nsmail.tmp");

      //
      // RICHIE_MAC - ok, here's the deal, we have a file that we need
      // to encode in appledouble encoding and put into the mAppleFileSpec
      // location, then patch the new file spec into the array and send this
      // file instead. 
      // 
      // To make this really work, we need something that will do the job of
      // the old fe_MakeAppleDoubleEncodeStream() stream in 4.x
      // 

      printf("...and then magic happens...which converts %s to appledouble encoding\n", (const char*)url_string);

      //
      // Now that we have morphed this file, we need to change where mURL is pointing.
      //
      NS_RELEASE(mURL);
      mURL = nsnull;

      const char *tmpName = mAppleFileSpec->GetNativePathCString(); // don't free this!      
      char *newURLSpec = nsMsgPlatformFileToURL(tmpName);

      if ( (!tmpName) || (!newURLSpec) )
      {
        PR_FREEIF(src_filename);
  		  PR_FREEIF(separator);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (NS_FAILED(nsMsgNewURL(&mURL, newURLSpec)))
      {
        PR_FREEIF(src_filename);
  		  PR_FREEIF(separator);
        PR_FREEIF(newURLSpec);
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      NS_ADDREF(mURL);
      PR_FREEIF(newURLSpec);
      
		  // Now after conversion, also patch the types.
      char        tmp[128];
		  PR_snprintf(tmp, sizeof(tmp), MULTIPART_APPLEDOUBLE ";\r\n boundary=\"%s\"", separator);
		  PR_FREEIF(separator);
  		PR_FREEIF (m_type);
  		m_type = PL_strdup(tmp);
		}
	  else
		{
      if (nsMsgIsMacFile(src_filename))
			{
				// The only time we want to send just the data fork of a two-fork
				// Mac file is if uuencoding has been requested.
				NS_ASSERTION(UseUUEncode_p(), "not UseUUEncode_p");

        // For now, just do the encoding, but in the old world we would ask the
        // user about doing this conversion
        printf("...we could ask the user about this conversion, but for now, nahh..\n");
			}

		  /* make sure the file type and create are set.	*/
		  char      filetype[32];
		  FSSpec    fsSpec;
		  FInfo     info;
		  PRBool 	  useDefault;
		  char	    *macType, *macEncoding;

      fsSpec = mAppleFileSpec->GetFSSpec();
		  if (FSpGetFInfo (&fsSpec, &info) == noErr)
			{
			  PR_snprintf(filetype, sizeof(filetype), "%X", info.fdType);
			  PR_FREEIF(m_x_mac_type);
			  m_x_mac_type    = PL_strdup(filetype);

			  PR_snprintf(filetype, sizeof(filetype), "%X", info.fdCreator);
			  PR_FREEIF(m_x_mac_creator);
			  m_x_mac_creator = PL_strdup(filetype);
			  if (m_type == NULL || !PL_strcasecmp (m_type, TEXT_PLAIN))
				{
# define TEXT_TYPE	0x54455854  /* the characters 'T' 'E' 'X' 'T' */
# define text_TYPE	0x74657874  /* the characters 't' 'e' 'x' 't' */

				  if (info.fdType != TEXT_TYPE && info.fdType != text_TYPE)
					{
					  MacGetFileType(mAppleFileSpec, &useDefault, &macType, &macEncoding);
					  PR_FREEIF(m_type);
					  m_type = macType;
					}
				}
			}
		  // don't bother to set the types if we failed in getting the file info.
		}

    PR_FREEIF(src_filename);
	  PR_FREEIF(src_filename);
	  src_filename = 0;
	}
#endif /* XP_MAC */

  //
  // Ok, here we are, we need to fire the URL off and get the data
  // in the temp file
  //
  // Create a fetcher for the URL attachment...
  mFetcher = new nsURLFetcher();
  if (!mFetcher)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(mURL);
  status = mFetcher->FireURLRequest(mURL, mOutFile, FetcherURLDoneCallback, this);
  if (NS_FAILED(status)) 
  {
    delete mFetcher;
    mFetcher = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  return status;
}

nsresult
nsMsgAttachmentHandler::UrlExit(nsresult status, const PRUnichar* aMsg)
{
  nsString  tMsg(aMsg);
  char      *eMsg = tMsg.ToNewCString();

  NS_ASSERTION(m_mime_delivery_state != NULL, "not-null m_mime_delivery_state");

  // Close the file, but don't delete the disk file (or the file spec.) 
  if (mOutFile)
  {
    mOutFile->close();

    delete mOutFile;
    mOutFile = nsnull;
  }

  // First things first, we are now going to see if this is an HTML
  // Doc and if it is, we need to see if we can determine the charset 
  // for this part by sniffing the HTML file.
  //
  if ( (m_type) &&  (*m_type) ) 
  {
    if (PL_strcasecmp(m_type, TEXT_HTML) == 0)
    {
      char *tmpCharset = (char *)nsMsgI18NParseMetaCharset(mFileSpec);
      if (tmpCharset[0] != '\0')
      {
        PR_FREEIF(m_charset);
        m_charset = PL_strdup(tmpCharset);
      }
    }
  }

  if (NS_FAILED(status))
  {
	  if (m_mime_delivery_state->m_status >= 0)
      m_mime_delivery_state->m_status = status;
  }

  m_done = PR_TRUE;

  //
  // Ok, now that we have the file here on disk, we need to see if there was 
  // a need to do conversion to plain text...if so, the magic happens here,
  // otherwise, just move on to other attachments...
  //
  if ( (m_type) && PL_strcasecmp(m_type, TEXT_PLAIN) ) 
  {
    if (m_desired_type && !PL_strcasecmp(m_desired_type, TEXT_PLAIN) )
	  {
	    //
      // Conversion to plain text desired.
	    //
      PRInt32       width = 72;
      nsresult      rv;
      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
      if (NS_SUCCEEDED(rv) && prefs) 
        prefs->GetIntPref("mailnews.wraplength", &width);
      // Let sanity reign!
	    if (width == 0) 
        width = 72;
	    else if (width < 10) 
        width = 10;
	    else if (width > 30000) 
        width = 30000;

      //
      // RICHIE - we need some converter service here to do the right 
      // thing and convert this data to plain text for us!
      //
      printf("...more magic happens and the data is converted to plain text!\n");

	    PR_FREEIF(m_type);
	    m_type = m_desired_type;
	    m_desired_type = nsnull;
	    PR_FREEIF(m_encoding);
	    m_encoding = nsnull;
	  }
  }

  NS_ASSERTION (m_mime_delivery_state->m_attachment_pending_count > 0, "no more pending attachment");
  m_mime_delivery_state->m_attachment_pending_count--;

  if (status >= 0 && m_mime_delivery_state->m_be_synchronous_p)
	{
	  /* Find the next attachment which has not yet been loaded,
		 if any, and start it going.
	   */
	  PRUint32 i;
	  nsMsgAttachmentHandler *next = 0;
	  for (i = 0; i < m_mime_delivery_state->m_attachment_count; i++)
    {
		  if (!m_mime_delivery_state->m_attachments[i].m_done)
		  {
        next = &m_mime_delivery_state->m_attachments[i];
        break;
		  }
    }

	  if (next)
		{
		  int status = next->SnarfAttachment(mCompFields);
		  if (NS_FAILED(status))
			{
			  m_mime_delivery_state->Fail(status, 0);
        delete eMsg;
			  return NS_ERROR_UNEXPECTED;
			}
		}
	}

  if (m_mime_delivery_state->m_attachment_pending_count == 0)
	{
	  // If this is the last attachment, then either complete the
		// delivery (if successful) or report the error by calling
		// the exit routine and terminating the delivery.
	  if (NS_FAILED(status))
		{
		  m_mime_delivery_state->Fail(status, eMsg);
		}
	  else
		{
		  m_mime_delivery_state->GatherMimeAttachments ();
		}
	}
  else
	{
	  // If this is not the last attachment, but it got an error,
		// then report that error and continue 
	  if (NS_FAILED(status))
		{
		  m_mime_delivery_state->Fail(status, eMsg);
		}
	}

  delete eMsg;
  return NS_OK;
}

PRBool 
nsMsgAttachmentHandler::UseUUEncode_p(void)
{
  if (mCompFields)
    return mCompFields->GetUUEncodeAttachments();
  else
    return PR_FALSE;
}
