/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "nsMsgAttachmentHandler.h"

#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgSend.h"
#include "nsMsgCompUtils.h"
#include "nsIPref.h"
#include "nsMsgEncoders.h"
#include "nsMsgI18N.h"
#include "nsURLFetcher.h"
#include "nsMimeTypes.h"
#include "nsIMsgStringService.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgCompCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsMsgPrompts.h"
#include "nsTextFormatter.h"
#include "nsIPrompt.h"
#include "nsMsgSimulateError.h"
#include "nsITextToSubURI.h"
#include "nsEscape.h"
#include "nsNetCID.h"


static  NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

///////////////////////////////////////////////////////////////////////////
// Mac Specific Attachment Handling for AppleDouble Encoded Files
///////////////////////////////////////////////////////////////////////////
#ifdef XP_MAC

#define AD_WORKING_BUFF_SIZE                  8192


extern void         MacGetFileType(nsFileSpec *fs, PRBool *useDefault, char **type, char **encoding);

#include "MoreFilesExtras.h"
#include "nsIInternetConfigService.h"

#endif /* XP_MAC */

//
// Class implementation...
//
nsMsgAttachmentHandler::nsMsgAttachmentHandler()
{
#if defined(DEBUG_ducarroz)
  printf("CREATE nsMsgAttachmentHandler: %x\n", this);
#endif
  mMHTMLPart = PR_FALSE;
  mPartUserOmissionOverride = PR_FALSE;
  mMainBody = PR_FALSE;

  m_charset = nsnull;
  m_override_type = nsnull;
  m_override_encoding = nsnull;
  m_desired_type = nsnull;
  m_description = nsnull;
  m_encoding = nsnull;
  m_real_name = nsnull;
  m_encoding = nsnull;
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
  m_encoder_data = nsnull;

  m_done = PR_FALSE;
  m_type = nsnull;
  m_size = 0;

  mCompFields = nsnull;   // Message composition fields for the sender
  mFileSpec = nsnull;
  mURL = nsnull;
  mRequest = nsnull;

  m_x_mac_type = nsnull;
  m_x_mac_creator = nsnull;

#ifdef XP_MAC
  mAppleFileSpec = nsnull;
#endif

  mDeleteFile = PR_FALSE;
  m_uri = nsnull;
}

nsMsgAttachmentHandler::~nsMsgAttachmentHandler()
{
#if defined(DEBUG_ducarroz)
  printf("DISPOSE nsMsgAttachmentHandler: %x\n", this);
#endif
#ifdef XP_MAC
  if (mAppleFileSpec)
    delete mAppleFileSpec;
#endif

  if (mFileSpec && mDeleteFile)
    mFileSpec->Delete(PR_FALSE);

  PR_FREEIF(m_uri);
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
    else if (*s < ' ' && *s != '\t' && *s != nsCRT::CR && *s != nsCRT::LF)
    {
      m_unprintable_count++;
      m_ctl_count++;
      if (*s == 0)
      m_null_count++;
    }

    if (*s == nsCRT::CR || *s == nsCRT::LF)
    {
      if (s+1 < end && s[0] == nsCRT::CR && s[1] == nsCRT::LF)
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
nsMsgAttachmentHandler::PickEncoding(const char *charset, nsIMsgSend *mime_delivery_state)
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
  
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
  
  if (!mMainBody && (forceB64 || mime_type_requires_b64_p (m_type)))
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
    
      else if (m_null_count)  /* If there are nulls, we must always encode,
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

      // If the Mail charset is multibyte, we force it to use Base64 for attachments.
      if ((!mMainBody && charset && nsMsgI18Nmultibyte_charset(charset)) &&
        ((PL_strcasecmp(m_type, TEXT_HTML) == 0) ||
        (PL_strcasecmp(m_type, TEXT_MDL) == 0) ||
        (PL_strcasecmp(m_type, TEXT_PLAIN) == 0) ||
        (PL_strcasecmp(m_type, TEXT_RICHTEXT) == 0) ||
        (PL_strcasecmp(m_type, TEXT_ENRICHED) == 0) ||
        (PL_strcasecmp(m_type, TEXT_VCARD) == 0) ||
        (PL_strcasecmp(m_type, APPLICATION_DIRECTORY) == 0) || /* text/x-vcard synonym */
        (PL_strcasecmp(m_type, TEXT_CSS) == 0) ||
        (PL_strcasecmp(m_type, TEXT_JSSS) == 0)))
      {
        needsB64 = PR_TRUE;
      }
      else if (charset && nsMsgI18Nstateful_charset(charset)) 
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
      mime_delivery_state);
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
      mime_delivery_state);
    PR_FREEIF(tailName);
    if (!m_encoder_data) return NS_ERROR_OUT_OF_MEMORY;
  }
  else if (!PL_strcasecmp(m_encoding, ENCODING_QUOTED_PRINTABLE))
  {
    m_encoder_data = MIME_QPEncoderInit(mime_encoder_output_fn,
      mime_delivery_state);
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
FetcherURLDoneCallback(nsresult aStatus, 
                       const char *aContentType,
                       const char *aCharset,
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
#ifdef XP_MAC
      //Do not change the type if we are dealing with an apple double file
      if (!ma->mAppleFileSpec)
#endif
      {
        PR_FREEIF(ma->m_type);
        ma->m_type = PL_strdup(aContentType);
      }
    }

    if (aCharset)
    {
      PR_FREEIF(ma->m_charset);
      ma->m_charset = PL_strdup(aCharset);
    }

    if ( (!PL_strcasecmp(ma->m_real_name, "ForwardedMessage.eml")) &&
         (!PL_strcasecmp(ma->m_type, MESSAGE_RFC822)) 
       )
    {
      char *subject = nsMsgParseSubjectFromFile(ma->mFileSpec);
      if (subject)
      {
        PR_FREEIF(ma->m_real_name);
        ma->m_real_name = subject;
      }
    }

    return ma->UrlExit(aStatus, aMsg);
  }
  else
    return NS_OK;
}

nsresult 
nsMsgAttachmentHandler::SnarfMsgAttachment(nsMsgCompFields *compFields)
{
  nsresult rv = NS_ERROR_INVALID_ARG;
  nsIMsgMessageService *messageService = nsnull;

  if (PL_strcasestr(m_uri, "_message:"))
  {
    mFileSpec = nsMsgCreateTempFileSpec("nsmail.tmp");
    mDeleteFile = PR_TRUE;
    mCompFields = compFields;
    PR_FREEIF(m_real_name);
    m_real_name = PL_strdup("ForwardedMessage.eml");
    PR_FREEIF(m_type);
    m_type = PL_strdup(MESSAGE_RFC822);
    PR_FREEIF(m_override_type);
    m_override_type = PL_strdup(MESSAGE_RFC822);
    if (!mFileSpec) 
    {
      rv = NS_ERROR_FAILURE;
      goto done;
    }

    nsCOMPtr<nsILocalFile> localFile;
    nsCOMPtr<nsIOutputStream> outputStream;
    NS_FileSpecToIFile(mFileSpec, getter_AddRefs(localFile));
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), localFile, -1, 00600);
    if (NS_FAILED(rv) || !outputStream || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_3))
    {
      if (m_mime_delivery_state)
      {
        nsCOMPtr<nsIMsgSendReport> sendReport;
        m_mime_delivery_state->GetSendReport(getter_AddRefs(sendReport));
        if (sendReport)
        {
          nsAutoString error_msg;
          nsAutoString path;
          mFileSpec->GetNativePathString(path);
          nsMsgBuildErrorMessageByID(NS_MSG_UNABLE_TO_OPEN_TMP_FILE, error_msg, &path, nsnull);
          sendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg.get(), PR_FALSE);
        }
      }
      rv =  NS_MSG_UNABLE_TO_OPEN_TMP_FILE;
      goto done;
    }
    mOutFile = do_QueryInterface(outputStream);
    
    nsCOMPtr<nsIURLFetcher> fetcher = do_CreateInstance(NS_URLFETCHER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !fetcher)
    {
      if (NS_SUCCEEDED(rv))
        rv =  NS_ERROR_UNEXPECTED;
      goto done;
    }

    rv = fetcher->Initialize(localFile, mOutFile, FetcherURLDoneCallback, this);
    rv = GetMessageServiceFromURI(m_uri, &messageService);
    if (NS_SUCCEEDED(rv) && messageService)
    {
      nsCAutoString uri(m_uri);
      uri.Append("?fetchCompleteMessage=true");
      nsCOMPtr<nsIStreamListener> strListener;
      fetcher->QueryInterface(NS_GET_IID(nsIStreamListener), getter_AddRefs(strListener));
      rv = messageService->DisplayMessage(uri.get(), strListener, nsnull, nsnull, nsnull, nsnull);
    }
  }
done:
  if (NS_FAILED(rv))
  {
      if (mOutFile)
      {
        mOutFile->Close();
        mOutFile = nsnull;
      }

      if (mFileSpec)
      {
        mFileSpec->Delete(PR_FALSE);
        delete mFileSpec;
        mFileSpec = nsnull;
      }
  }
  if (messageService)
      ReleaseMessageServiceFromURI(m_uri, messageService);

  return rv;
}

nsresult
nsMsgAttachmentHandler::SnarfAttachment(nsMsgCompFields *compFields)
{
  nsresult      status = 0;
  nsXPIDLCString url_string;

  NS_ASSERTION (! m_done, "Already done");

  if (!mURL)
    return SnarfMsgAttachment(compFields);

  mCompFields = compFields;

  // First, get as file spec and create the stream for the
  // temp file where we will save this data
    mFileSpec = nsMsgCreateTempFileSpec("nsmail.tmp");
  if (! mFileSpec )
    return (NS_ERROR_FAILURE);
  mDeleteFile = PR_TRUE;

  nsCOMPtr<nsILocalFile> localFile;
  nsCOMPtr<nsIOutputStream> outputStream;
  NS_FileSpecToIFile(mFileSpec, getter_AddRefs(localFile));
  status = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), localFile, -1, 00600);
  if (NS_FAILED(status) || !outputStream || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_3))
  {
    if (m_mime_delivery_state)
    {
      nsCOMPtr<nsIMsgSendReport> sendReport;
      m_mime_delivery_state->GetSendReport(getter_AddRefs(sendReport));
      if (sendReport)
      {
        nsAutoString error_msg;
        nsAutoString path;
        mFileSpec->GetNativePathString(path);
        nsMsgBuildErrorMessageByID(NS_MSG_UNABLE_TO_OPEN_TMP_FILE, error_msg, &path, nsnull);
        sendReport->SetMessage(nsIMsgSendReport::process_Current, error_msg.get(), PR_FALSE);
      }
    }
    mFileSpec->Delete(PR_FALSE);
    delete mFileSpec;
    mFileSpec = nsnull;
    return NS_MSG_UNABLE_TO_OPEN_TMP_FILE; 
  }
  mOutFile = do_QueryInterface(outputStream);

  mURL->GetSpec(getter_Copies(url_string));

#ifdef XP_MAC
  if ( !m_bogus_attachment && nsMsgIsLocalFile(url_string))
  {
    // convert the apple file to AppleDouble first, and then patch the
    // address in the url.
    char *src_filename = nsMsgGetLocalFileFromURL (url_string);
    if (!src_filename)
      return NS_ERROR_OUT_OF_MEMORY;

    //We need to retrieve the file type and creator...
    nsFileSpec scr_fileSpec(src_filename);
    FSSpec fsSpec = scr_fileSpec.GetFSSpec();;
    FInfo info;
    if (FSpGetFInfo (&fsSpec, &info) == noErr)
    {
      char filetype[32];
      PR_snprintf(filetype, sizeof(filetype), "%X", info.fdType);
      PR_FREEIF(m_x_mac_type);
      m_x_mac_type = PL_strdup(filetype);

      PR_snprintf(filetype, sizeof(filetype), "%X", info.fdCreator);
      PR_FREEIF(m_x_mac_creator);
      m_x_mac_creator = PL_strdup(filetype);
    }

    PRBool sendResourceFork = PR_TRUE;
    PRBool sendDataFork = PR_TRUE;
    PRBool icGaveNeededInfo = PR_FALSE;
 
    nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
    if (icService)
    {
      PRInt32 icFlags;
      nsresult rv = icService->GetFileMappingFlags(&fsSpec, PR_FALSE, &icFlags);
      if (NS_SUCCEEDED(rv) && icFlags != -1 && !(icFlags & nsIInternetConfigService::eIICMapFlag_NotOutgoingMask))
      {
        sendResourceFork = (icFlags & nsIInternetConfigService::eIICMapFlag_ResourceForkMask);
        sendDataFork = (icFlags & nsIInternetConfigService::eIICMapFlag_DataForkMask);
        
        icGaveNeededInfo = PR_TRUE;
      }
    }
    
    if (! icGaveNeededInfo)
    {
      // If InternetConfig cannot help us, then just try our best...
      long dataSize = 0;
      long resSize = 0;
  
      // first check if we have a resource fork
      if (::FSpGetFileSize(&fsSpec, &dataSize, &resSize) == noErr)
        sendResourceFork = (resSize > 0);

      // then, if we have a resource fork, check the filename extension, maybe we don't need the resource fork!
      if (sendResourceFork)
      {
        nsCOMPtr<nsIFileURL> fileUrl(do_CreateInstance(NS_STANDARDURL_CONTRACTID));
        if (fileUrl)
        {
          nsresult rv = fileUrl->SetSpec(url_string);
          if (NS_SUCCEEDED(rv))
          {
            char  *ext;
            rv = fileUrl->GetFileExtension(&ext);
            if (NS_SUCCEEDED(rv) && ext && *ext)
            {
              sendResourceFork =
                 PL_strcasecmp(ext, "TXT") &&
                 PL_strcasecmp(ext, "JPG") &&
                 PL_strcasecmp(ext, "GIF") &&
                 PL_strcasecmp(ext, "TIF") &&
                 PL_strcasecmp(ext, "HTM") &&
                 PL_strcasecmp(ext, "HTML") &&
                 PL_strcasecmp(ext, "ART") &&
                 PL_strcasecmp(ext, "XUL") &&
                 PL_strcasecmp(ext, "XML") &&
                 PL_strcasecmp(ext, "CSS") &&
                 PL_strcasecmp(ext, "JS");
            }
            PR_FREEIF(ext);
          }
        }
      }
    }

    // Only use appledouble if we aren't uuencoding.
    if( sendResourceFork && (! UseUUEncode_p()) )
    {
      char                *separator;
      nsInputFileStream   *myInputFile = new nsInputFileStream(scr_fileSpec);

      if ((!myInputFile) || (!myInputFile->is_open()))
        return NS_ERROR_OUT_OF_MEMORY;

      separator = mime_make_separator("ad");
      if (!separator)
      {
        delete myInputFile;
        PR_FREEIF(src_filename);
        return NS_ERROR_OUT_OF_MEMORY;
      }
            
      mAppleFileSpec = nsMsgCreateTempFileSpec("appledouble");
      if (!mAppleFileSpec) 
      {
        delete myInputFile;
        PR_FREEIF(separator);
        PR_FREEIF(src_filename);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      //
      // RICHIE_MAC - ok, here's the deal, we have a file that we need
      // to encode in appledouble encoding for the resource fork and put that
      // into the mAppleFileSpec location. Then, we need to patch the new file 
      // spec into the array and send this as part of the 2 part appledouble/mime
      // encoded mime part. 
      // 
      AppleDoubleEncodeObject     *obj = new (AppleDoubleEncodeObject);
      if (obj == NULL) 
      {
        delete mAppleFileSpec;
        PR_FREEIF(src_filename);
        PR_FREEIF(separator);
        return NS_ERROR_OUT_OF_MEMORY;
      }
   
      obj->fileStream = new nsIOFileStream(*mAppleFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
      if ( (!obj->fileStream) || (!obj->fileStream->is_open()) )
      {
        delete myInputFile;
        PR_FREEIF(src_filename);
        PR_FREEIF(separator);
        delete obj;
        return NS_ERROR_OUT_OF_MEMORY;
      }

      PRInt32     bSize = AD_WORKING_BUFF_SIZE;

      char  *working_buff = nsnull;
      while (!working_buff && (bSize >= 512))
      {
        working_buff = (char *)PR_CALLOC(bSize);
        if (!working_buff)
          bSize /= 2;
      }

      if (!working_buff)
      {
        PR_FREEIF(src_filename);
        PR_FREEIF(separator);
        delete obj;
        return NS_ERROR_OUT_OF_MEMORY;
      }

      obj->buff = working_buff;
      obj->s_buff = bSize;  

      //
      //  Setup all the need information on the apple double encoder.
      //
      ap_encode_init(&(obj->ap_encode_obj), src_filename, separator);

      PRInt32 count;

      status = noErr;
      m_size = 0;
      while (status == noErr)
      {      
        status = ap_encode_next(&(obj->ap_encode_obj), obj->buff, bSize, &count);
        if (status == noErr || status == errDone)
        {
          //
          // we got the encode data, so call the next stream to write it to the disk.
          //
          if (obj->fileStream->write(obj->buff, count) != count)
            status = NS_MSG_ERROR_WRITING_FILE;
        }
      } 
      
      delete myInputFile;  
      ap_encode_end(&(obj->ap_encode_obj), (status >= 0)); // if this is true, ok, false abort
      if (obj->fileStream)
        obj->fileStream->close();

      PR_FREEIF(obj->buff);               /* free the working buff.   */
      PR_FREEIF(obj);


      //
      // Now that we have morphed this file, we need to change where mURL is pointing.
      //
      NS_RELEASE(mURL);
      mURL = nsnull;

      char *newURLSpec = nsMsgPlatformFileToURL(*mAppleFileSpec);

      if (!newURLSpec) 
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
      if ( sendResourceFork )
      {
        // The only time we want to send just the data fork of a two-fork
        // Mac file is if uuencoding has been requested.
        NS_ASSERTION(UseUUEncode_p(), "not UseUUEncode_p");

        // For now, just do the encoding, but in the old world we would ask the
        // user about doing this conversion
        printf("...we could ask the user about this conversion, but for now, nahh..\n");
      }

      PRBool    useDefault;
      char      *macType, *macEncoding;
      if (m_type == NULL || !PL_strcasecmp (m_type, TEXT_PLAIN))
      {
# define TEXT_TYPE  0x54455854  /* the characters 'T' 'E' 'X' 'T' */
# define text_TYPE  0x74657874  /* the characters 't' 'e' 'x' 't' */

        if (info.fdType != TEXT_TYPE && info.fdType != text_TYPE)
        {
          MacGetFileType(&scr_fileSpec, &useDefault, &macType, &macEncoding);
          PR_FREEIF(m_type);
          m_type = macType;
        }
      }
      // don't bother to set the types if we failed in getting the file info.
    }

    PR_FREEIF(src_filename);
  }
#endif /* XP_MAC */

  //
  // Ok, here we are, we need to fire the URL off and get the data
  // in the temp file
  //
  // Create a fetcher for the URL attachment...
  NS_ADDREF(mURL);

  nsresult rv;
  nsCOMPtr<nsIURLFetcher> fetcher = do_CreateInstance(NS_URLFETCHER_CONTRACTID, &rv);
  if (NS_FAILED(rv) || !fetcher)
  {
    if (NS_SUCCEEDED(rv))
      return NS_ERROR_UNEXPECTED;
    else
      return rv;
  }

  status = fetcher->FireURLRequest(mURL, localFile, mOutFile, FetcherURLDoneCallback, this);
  if (NS_FAILED(status)) 
    return NS_ERROR_UNEXPECTED;

  return status;
}

nsresult
nsMsgAttachmentHandler::LoadDataFromFile(nsFileSpec& fSpec, nsString &sigData, PRBool charsetConversion)
{
  PRInt32       readSize;
  char          *readBuf;

  nsInputFileStream tempFile(fSpec);
  if (!tempFile.is_open())
    return NS_MSG_ERROR_WRITING_FILE;        
  
  readSize = fSpec.GetFileSize();
  readBuf = (char *)PR_Malloc(readSize + 1);
  if (!readBuf)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCRT::memset(readBuf, 0, readSize + 1);

  readSize = tempFile.read(readBuf, readSize);
  tempFile.close();

  if (charsetConversion)
  {
    nsAutoString charset; charset.AssignWithConversion(m_charset);
    if (NS_FAILED(ConvertToUnicode(charset, readBuf, sigData)))
      sigData.AssignWithConversion(readBuf);
  }
  else
      sigData.AssignWithConversion(readBuf);

  PR_FREEIF(readBuf);
  return NS_OK;
}

nsresult
nsMsgAttachmentHandler::Abort()
{
  NS_ASSERTION(m_mime_delivery_state != nsnull, "not-null m_mime_delivery_state");

  if (m_done)
    return NS_OK;

  if (mRequest)
    return mRequest->Cancel(NS_ERROR_ABORT);
  else
    if (m_mime_delivery_state)
    {
      m_mime_delivery_state->SetStatus(NS_ERROR_ABORT);
      m_mime_delivery_state->NotifyListenerOnStopSending(nsnull, NS_ERROR_ABORT, 0, nsnull);
    }

  return NS_OK;

}

nsresult
nsMsgAttachmentHandler::UrlExit(nsresult status, const PRUnichar* aMsg)
{
  NS_ASSERTION(m_mime_delivery_state != nsnull, "not-null m_mime_delivery_state");

  // Close the file, but don't delete the disk file (or the file spec.) 
  if (mOutFile)
  {
    mOutFile->Close();
    mOutFile = nsnull;
  }
  
  mRequest = nsnull;

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

  nsresult mimeDeliveryStatus;
  m_mime_delivery_state->GetStatus(&mimeDeliveryStatus);
  
  if (mimeDeliveryStatus == NS_ERROR_ABORT)
    status = NS_ERROR_ABORT;
 
  if (NS_FAILED(status) && status != NS_ERROR_ABORT && NS_SUCCEEDED(mimeDeliveryStatus))
  {
    // At this point, we should probably ask a question to the user 
    // if we should continue without this attachment.
    //
    PRBool            keepOnGoing = PR_TRUE;
    nsXPIDLCString    turl;
    nsXPIDLString     msg;
    PRUnichar         *printfString = nsnull;
    nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID));

    nsMsgDeliverMode mode = nsIMsgSend::nsMsgDeliverNow;
    m_mime_delivery_state->GetDeliveryMode(&mode);
    if (mode == nsIMsgSend::nsMsgSaveAsDraft || mode == nsIMsgSend::nsMsgSaveAsTemplate)
      composebundle->GetStringByID(NS_MSG_FAILURE_ON_OBJ_EMBED_WHILE_SAVING, getter_Copies(msg));
    else
      composebundle->GetStringByID(NS_MSG_FAILURE_ON_OBJ_EMBED_WHILE_SENDING, getter_Copies(msg));
 
    if (m_real_name && *m_real_name)
      printfString = nsTextFormatter::smprintf(msg, m_real_name);
    else
    if (NS_SUCCEEDED(mURL->GetSpec(getter_Copies(turl))) && (turl))
      {
        nsCAutoString unescapeUrl(turl);
        nsUnescape (NS_CONST_CAST(char*, unescapeUrl.get()));
        if (unescapeUrl.IsEmpty())
          printfString = nsTextFormatter::smprintf(msg, turl.get());
        else
          printfString = nsTextFormatter::smprintf(msg, unescapeUrl.get());
      }
    else
      printfString = nsTextFormatter::smprintf(msg, "?");

    nsCOMPtr<nsIPrompt> aPrompt;
    if (m_mime_delivery_state)
      m_mime_delivery_state->GetDefaultPrompt(getter_AddRefs(aPrompt));
    nsMsgAskBooleanQuestionByString(aPrompt, printfString, &keepOnGoing);
    PR_FREEIF(printfString);

    if (keepOnGoing)
    {
      status = 0;
      m_bogus_attachment = PR_TRUE; //That will cause this attachment to be ignored.
    }
    else
    {
      status = NS_ERROR_ABORT;
      m_mime_delivery_state->SetStatus(status);
      nsresult ignoreMe;
      m_mime_delivery_state->Fail(status, nsnull, &ignoreMe);
      m_mime_delivery_state->NotifyListenerOnStopSending(nsnull, status, 0, nsnull);
      SetMimeDeliveryState(nsnull);
      return status;
    }
  }

  m_done = PR_TRUE;

  //
  // Ok, now that we have the file here on disk, we need to see if there was 
  // a need to do conversion to plain text...if so, the magic happens here,
  // otherwise, just move on to other attachments...
  //
  if (NS_SUCCEEDED(status) && m_type && PL_strcasecmp(m_type, TEXT_PLAIN) ) 
  {
    if (m_desired_type && !PL_strcasecmp(m_desired_type, TEXT_PLAIN) )
    {
      //
      // Conversion to plain text desired.
      //
      PRInt32       width = 72;
      nsresult      rv;
      nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
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
      // Now use the converter service here to do the right 
      // thing and convert this data to plain text for us!
      //
      nsString      conData;

      if (NS_SUCCEEDED(LoadDataFromFile(*mFileSpec, conData, PR_TRUE)))
      {
        if (NS_SUCCEEDED(ConvertBufToPlainText(conData, UseFormatFlowed(m_charset))))
        {
          if (mDeleteFile)
          mFileSpec->Delete(PR_FALSE);

          nsOutputFileStream tempfile(*mFileSpec, PR_WRONLY | PR_CREATE_FILE, 00600);
          if (tempfile.is_open()) 
          {
            char    *tData = nsnull;
            nsAutoString charset; charset.AssignWithConversion(m_charset);
            if (NS_FAILED(ConvertFromUnicode(charset, conData, &tData)))
              tData = ToNewCString(conData);
            if (tData)
            {
              (void) tempfile.write(tData, nsCRT::strlen(tData));
              PR_FREEIF(tData);
            }
            tempfile.close();
          }
        }
      }

      PR_FREEIF(m_type);
      m_type = m_desired_type;
      m_desired_type = nsnull;
      PR_FREEIF(m_encoding);
      m_encoding = nsnull;
    }
  }

  PRUint32 pendingAttachmentCount = 0;
  m_mime_delivery_state->GetPendingAttachmentCount(&pendingAttachmentCount);
  NS_ASSERTION (pendingAttachmentCount > 0, "no more pending attachment");
  
  m_mime_delivery_state->SetPendingAttachmentCount(pendingAttachmentCount - 1);

  PRBool processAttachmentsSynchronously = PR_FALSE;
  m_mime_delivery_state->GetProcessAttachmentsSynchronously(&processAttachmentsSynchronously);
  if (NS_SUCCEEDED(status) && processAttachmentsSynchronously)
  {
    /* Find the next attachment which has not yet been loaded,
     if any, and start it going.
     */
    PRUint32 i;
    nsMsgAttachmentHandler *next = 0;
    nsMsgAttachmentHandler *attachments = nsnull;
    PRUint32 attachmentCount = 0;
    
    m_mime_delivery_state->GetAttachmentCount(&attachmentCount);
    if (attachmentCount)
      m_mime_delivery_state->GetAttachmentHandlers(&attachments);
      
    for (i = 0; i < attachmentCount; i++)
    {
      if (!attachments[i].m_done)
      {
        next = &attachments[i];
        //
        // rhp: We need to get a little more understanding to failed URL 
        // requests. So, at this point if most of next is NULL, then we
        // should just mark it fetched and move on! We probably ignored
        // this earlier on in the send process.
        //
        if ( (!next->mURL) && (!next->m_uri) )
        {
          attachments[i].m_done = PR_TRUE;
          m_mime_delivery_state->GetPendingAttachmentCount(&pendingAttachmentCount);
          m_mime_delivery_state->SetPendingAttachmentCount(pendingAttachmentCount - 1);
          next->mPartUserOmissionOverride = PR_TRUE;
          next = nsnull;
          continue;
        }

        break;
      }
    }

    if (next)
    {
      int status = next->SnarfAttachment(mCompFields);
      if (NS_FAILED(status))
      {
        nsresult ignoreMe;
        m_mime_delivery_state->Fail(status, nsnull, &ignoreMe);
        m_mime_delivery_state->NotifyListenerOnStopSending(nsnull, status, 0, nsnull);
        SetMimeDeliveryState(nsnull);
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  m_mime_delivery_state->GetPendingAttachmentCount(&pendingAttachmentCount);
  if (pendingAttachmentCount == 0)
  {
    // If this is the last attachment, then either complete the
    // delivery (if successful) or report the error by calling
    // the exit routine and terminating the delivery.
    if (NS_FAILED(status))
    {
      nsresult ignoreMe;
      m_mime_delivery_state->Fail(status, aMsg, &ignoreMe);
      m_mime_delivery_state->NotifyListenerOnStopSending(nsnull, status, aMsg, nsnull);
      SetMimeDeliveryState(nsnull);
      return NS_ERROR_UNEXPECTED;
    }
    else
    {
      status = m_mime_delivery_state->GatherMimeAttachments ();
      if (NS_FAILED(status))
      {
        nsresult ignoreMe;
        m_mime_delivery_state->Fail(status, aMsg, &ignoreMe);
        m_mime_delivery_state->NotifyListenerOnStopSending(nsnull, status, aMsg, nsnull);
        SetMimeDeliveryState(nsnull);
        return NS_ERROR_UNEXPECTED;
      }
    }
  }
  else
  {
    // If this is not the last attachment, but it got an error,
    // then report that error and continue 
    if (NS_FAILED(status))
    {
      nsresult ignoreMe;
      m_mime_delivery_state->Fail(status, aMsg, &ignoreMe);
    }
  }

  SetMimeDeliveryState(nsnull);
  return NS_OK;
}

PRBool 
nsMsgAttachmentHandler::UseUUEncode_p(void)
{
  if (mCompFields)
    return mCompFields->GetUuEncodeAttachments();
  else
    return PR_FALSE;
}

nsresult
nsMsgAttachmentHandler::GetMimeDeliveryState(nsIMsgSend** _retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = m_mime_delivery_state;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsMsgAttachmentHandler::SetMimeDeliveryState(nsIMsgSend* mime_delivery_state)
{
  /*
    Because setting m_mime_delivery_state to null could destroy ourself as
    m_mime_delivery_state it's our parent, we need to protect ourself against
    that!

    This extra comptr is necessary,
    see bug http://bugzilla.mozilla.org/show_bug.cgi?id=78967
  */
  nsCOMPtr<nsIMsgSend> temp = m_mime_delivery_state; /* Should lock our parent until the end of the function */
  m_mime_delivery_state = mime_delivery_state;
  return NS_OK;
}
