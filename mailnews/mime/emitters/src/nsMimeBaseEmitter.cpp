/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henrik Gemal <gemal@gemal.dk>
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

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "stdio.h"
#include "nsMimeBaseEmitter.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"
#include "nsFileStream.h"
#include "nsMimeStringResources.h"
#include "msgCore.h"
#include "nsIMsgHeaderParser.h"
#include "nsIComponentManager.h"
#include "nsEmitterUtils.h"
#include "nsFileSpec.h"
#include "nsIRegistry.h"
#include "nsIMimeStreamConverter.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "prlog.h"
#include "prprf.h"

static PRLogModuleInfo * gMimeEmitterLogModule = nsnull;

#define   MIME_URL      "chrome://messenger/locale/mimeheader.properties"
static    NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static    NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static    NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

NS_IMPL_ISUPPORTS4(nsMimeBaseEmitter, 
                   nsIMimeEmitter, 
                   nsIInputStreamObserver, 
                   nsIOutputStreamObserver,
                   nsIInterfaceRequestor)

nsMimeBaseEmitter::nsMimeBaseEmitter()
{
  NS_INIT_REFCNT(); 

  // Initialize data output vars...
  mFirstHeaders = PR_TRUE;

  mBufferMgr = nsnull;
  mTotalWritten = 0;
  mTotalRead = 0;
  mInputStream = nsnull;
  mOutStream = nsnull;
  mOutListener = nsnull;
  mChannel = nsnull;

  // Display output control vars...
  mDocHeader = PR_FALSE;
  m_stringBundle = nsnull;
  mURL = nsnull;
  mHeaderDisplayType = nsMimeHeaderDisplayTypes::NormalHeaders;

  // Setup array for attachments
  mAttachCount = 0;
  mAttachArray = new nsVoidArray();
  mCurrentAttachment = nsnull;

  // Header cache...
  mHeaderArray = new nsVoidArray();

  // Embedded Header Cache...
  mEmbeddedHeaderArray = nsnull;

  // HTML Header Data...
//  mHTMLHeaders = "";
//  mCharset = "";

  // Init the body...
  mBodyStarted = PR_FALSE;
//  mBody = "";

  // This is needed for conversion of I18N Strings...
  nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                     NS_GET_IID(nsIMimeConverter), 
                                     getter_AddRefs(mUnicodeConverter));

  // Do prefs last since we can live without this if it fails...
  nsresult rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports**)&(mPrefs));
  if (! (mPrefs && NS_SUCCEEDED(rv)))
    return;

  if ((mPrefs && NS_SUCCEEDED(rv)))
    mPrefs->GetIntPref("mail.show_headers", &mHeaderDisplayType);

  if (!gMimeEmitterLogModule)
    gMimeEmitterLogModule = PR_NewLogModule("MIME");
}

nsMimeBaseEmitter::~nsMimeBaseEmitter(void)
{
  PRInt32 i;

  // Delete the buffer manager...
  if (mBufferMgr)
  {
    delete mBufferMgr;
    mBufferMgr = nsnull;
  }

  // Release the prefs service...
  if (mPrefs)
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);

  // Clean up the attachment array structures...
  if (mAttachArray)
  {
    for (i=0; i<mAttachArray->Count(); i++)
    {
      attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
      if (!attachInfo)
        continue;
    
      PR_FREEIF(attachInfo->contentType);
      PR_FREEIF(attachInfo->displayName);
      PR_FREEIF(attachInfo->urlSpec);
      PR_FREEIF(attachInfo);
    }
    delete mAttachArray;
  }

  // Cleanup allocated header arrays...
  CleanupHeaderArray(mHeaderArray);
  mHeaderArray = nsnull;

  CleanupHeaderArray(mEmbeddedHeaderArray);
  mEmbeddedHeaderArray = nsnull;
}

NS_IMETHODIMP nsMimeBaseEmitter::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  return QueryInterface(aIID, aInstancePtr);
}

void
nsMimeBaseEmitter::CleanupHeaderArray(nsVoidArray *aArray)
{
  if (!aArray)
    return;

  for (PRInt32 i=0; i<aArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)aArray->ElementAt(i);
    if (!headerInfo)
      continue;
    
    PR_FREEIF(headerInfo->name);
    PR_FREEIF(headerInfo->value);
    PR_FREEIF(headerInfo);
  }

  delete aArray;
}

//
// This is the next generation string retrieval call 
//
char *
nsMimeBaseEmitter::MimeGetStringByName(const char *aHeaderName)
{
	nsresult res = NS_OK;

	if (!m_stringBundle)
	{
		static const char propertyURL[] = MIME_URL;

		nsCOMPtr<nsIStringBundleService> sBundleService = 
		         do_GetService(kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(m_stringBundle));
		}
	}

	if (m_stringBundle)
	{
    nsAutoString  v;
    PRUnichar     *ptrv = nsnull;
    nsString      uniStr; uniStr.AssignWithConversion(aHeaderName);

    res = m_stringBundle->GetStringFromName(uniStr.get(), &ptrv);
    v = ptrv;

    if (NS_FAILED(res)) 
      return nsnull;
    
    // Here we need to return a new copy of the string
    // This returns a UTF-8 string so the caller needs to perform a conversion 
    // if this is used as UCS-2 (e.g. cannot do nsString(utfStr);
    //
    return ToNewUTF8String(v);
	}
	else
	{
    return nsnull;
	}
}

// 
// This will search a string bundle (eventually) to find a descriptive header 
// name to match what was found in the mail message. aHeaderName is passed in
// in all caps and a dropback default name is provided. The caller needs to free
// the memory returned by this function.
//
char *
nsMimeBaseEmitter::LocalizeHeaderName(const char *aHeaderName, const char *aDefaultName)
{
  char *retVal = MimeGetStringByName(aHeaderName);

  if (!retVal)
    return retVal;
  else
    return nsCRT::strdup(aDefaultName);
}

///////////////////////////////////////////////////////////////////////////
// nsIPipeObserver Interface
///////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMimeBaseEmitter::OnWrite(nsIOutputStream* out, PRUint32 aCount)
{
  return NS_OK;
}

NS_IMETHODIMP nsMimeBaseEmitter::OnEmpty(nsIInputStream* in)
{
  return NS_OK;
}


NS_IMETHODIMP nsMimeBaseEmitter::OnFull(nsIOutputStream* out)
{
  // the pipe is full so we should flush our data to the converter's listener
  // in order to make more room. 

  // since we only have one pipe per mime emitter, i can ignore the pipe param and use
  // my outStream object directly (it will be the same thing as what we'd get from aPipe.

  nsresult rv = NS_OK;
  if (mOutListener && mInputStream)
  {
      PRUint32 bytesAvailable = 0;
      rv = mInputStream->Available(&bytesAvailable);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Available failed");
      
      nsCOMPtr<nsIRequest> request = do_QueryInterface(mChannel);
      rv = mOutListener->OnDataAvailable(request, mURL, mInputStream, 0, bytesAvailable);

  }
  else 
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

NS_IMETHODIMP nsMimeBaseEmitter::OnClose(nsIInputStream* in)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// nsMimeBaseEmitter Interface
/////////////////////////////////////////////////////////////////////////// 
NS_IMETHODIMP
nsMimeBaseEmitter::SetPipe(nsIInputStream * aInputStream, nsIOutputStream *outStream)
{
  mInputStream = aInputStream;
  mOutStream = outStream;
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
NS_IMETHODIMP       
nsMimeBaseEmitter::Initialize(nsIURI *url, nsIChannel * aChannel, PRInt32 aFormat)
{
  // set the url
  mURL = url;
  mChannel = aChannel;

  // Create rebuffering object
  if (mBufferMgr)
  {
    delete mBufferMgr;
  }
  mBufferMgr = new MimeRebuffer();

  // Counters for output stream
  mTotalWritten = 0;
  mTotalRead = 0;
  mFormat = aFormat;

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::SetOutputListener(nsIStreamListener *listener)
{
  mOutListener = listener;
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::GetOutputListener(nsIStreamListener **listener)
{
  if (listener)
  {
    *listener = mOutListener;
    NS_IF_ADDREF(*listener);
  }
  return NS_OK;
}


// Attachment handling routines
nsresult
nsMimeBaseEmitter::StartAttachment(const char *name, const char *contentType, const char *url,
                                   PRBool aNotDownloaded)
{
  // Ok, now we will setup the attachment info 
  mCurrentAttachment = (attachmentInfoType *) PR_NEWZAP(attachmentInfoType);
  if ( (mCurrentAttachment) && mAttachArray)
  {
    ++mAttachCount;

    mCurrentAttachment->displayName = nsCRT::strdup(name);
    mCurrentAttachment->urlSpec = nsCRT::strdup(url);
    mCurrentAttachment->contentType = nsCRT::strdup(contentType);
    mCurrentAttachment->notDownloaded = aNotDownloaded;
  }

  return NS_OK;
}

nsresult
nsMimeBaseEmitter::EndAttachment()
{
  // Ok, add the attachment info to the attachment array...
  if ( (mCurrentAttachment) && (mAttachArray) )
  {
    mAttachArray->AppendElement(mCurrentAttachment);
    mCurrentAttachment = nsnull;
  }

  return NS_OK;
}

nsresult
nsMimeBaseEmitter::EndAllAttachments()
{
	return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::AddAttachmentField(const char *field, const char *value)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = nsCRT::strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::UtilityWriteCRLF(const char *buf)
{
  PRInt32     tmpLen = nsCRT::strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
  Write(CRLF, 2, &written);

  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  unsigned int        written = 0;
  nsresult rv = NS_OK;
  PRUint32            needToWrite;


  PR_LOG(gMimeEmitterLogModule, PR_LOG_ALWAYS, (buf));
  //
  // Make sure that the buffer we are "pushing" into has enough room
  // for the write operation. If not, we have to buffer, return, and get
  // it on the next time through
  //
  *amountWritten = 0;

  needToWrite = mBufferMgr->GetSize();
  // First, handle any old buffer data...
  if (needToWrite > 0)
  {
    rv = mOutStream->Write(mBufferMgr->GetBuffer(), 
                            needToWrite, &written);

    mTotalWritten += written;
    mBufferMgr->ReduceBuffer(written);
    *amountWritten = written;

    // if we couldn't write all the old data, buffer the new data
    // and return
    if (mBufferMgr->GetSize() > 0)
    {
      mBufferMgr->IncreaseBuffer(buf, size);
      return rv;
    }
  }


  // if we get here, we are dealing with new data...try to write
  // and then do the right thing...
  rv = mOutStream->Write(buf, size, &written);
  *amountWritten = written;
  mTotalWritten += written;

  if (written < size)
    mBufferMgr->IncreaseBuffer(buf+written, (size-written));

  return rv;
}

//
// Find a cached header! Note: Do NOT free this value!
//
char *
nsMimeBaseEmitter::GetHeaderValue(const char  *aHeaderName,
                                  nsVoidArray *aArray)
{
  PRInt32     i;
  char        *retVal = nsnull;

  if (!aArray)
    return nsnull;

  for (i=0; i<aArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)aArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) )
      continue;
    
    if (!nsCRT::strcasecmp(aHeaderName, headerInfo->name))
    {
      retVal = headerInfo->value;
      break;
    }
  }

  return retVal;
}

//
// This is called at the start of the header block for all header information in ANY
// AND ALL MESSAGES (yes, quoted, attached, etc...) 
//
// NOTE: This will be called even when headers are will not follow. This is
// to allow us to be notified of the charset of the original message. This is
// important for forward and reply operations
//
NS_IMETHODIMP
nsMimeBaseEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                               const char *outCharset)
{
  mDocHeader = rootMailHeader;

  // If this is not the mail messages header, then we need to create 
  // the mEmbeddedHeaderArray structure for use with this internal header 
  // structure.
  if (!mDocHeader)
  {
    if (mEmbeddedHeaderArray)
      CleanupHeaderArray(mEmbeddedHeaderArray);

    mEmbeddedHeaderArray = new nsVoidArray();
    if (!mEmbeddedHeaderArray)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // If the main doc, check on updated character set
  if (mDocHeader)
    UpdateCharacterSet(outCharset);

  mCharset.AssignWithConversion(outCharset);
  return NS_OK; 
}

// Ok, if we are here, and we have a aCharset passed in that is not
// UTF-8 or US-ASCII, then we should tag the mChannel member with this
// charset. This is because replying to messages with specified charset's
// need to be tagged as that charset by default.
//
NS_IMETHODIMP
nsMimeBaseEmitter::UpdateCharacterSet(const char *aCharset)
{
  if ( (aCharset) && (PL_strcasecmp(aCharset, "US-ASCII")) &&
        (PL_strcasecmp(aCharset, "ISO-8859-1")) &&
        (PL_strcasecmp(aCharset, "UTF-8")) )
  {
    char    *contentType = nsnull;
    
    if (NS_SUCCEEDED(mChannel->GetContentType(&contentType)) && contentType)
    {
      char *cPtr = (char *) PL_strcasestr(contentType, "charset=");

      if (cPtr)
      {
        char  *ptr = contentType;
        while (*ptr)
        {
          if ( (*ptr == ' ') || (*ptr == ';') ) 
          {
            if ((ptr + 1) >= cPtr)
            {
              *ptr = '\0';
              break;
            }
          }

          ++ptr;
        }
      }

      char *newContentType = (char *) PR_smprintf("%s; charset=%s", contentType, aCharset);
      if (newContentType)
      {
        mChannel->SetContentType(newContentType); 
        PR_FREEIF(newContentType);
      }
      
      PR_FREEIF(contentType);
    }
  }

  return NS_OK;
}

//
// This will be called for every header field regardless if it is in an
// internal body or the outer message. 
//
NS_IMETHODIMP
nsMimeBaseEmitter::AddHeaderField(const char *field, const char *value)
{
  if ( (!field) || (!value) )
    return NS_OK;

  nsVoidArray   *tPtr;
  if (mDocHeader)
    tPtr = mHeaderArray;
  else
    tPtr = mEmbeddedHeaderArray;

  // This is a header so we need to cache and output later.
  // Ok, now we will setup the header info for the header array!
  headerInfoType  *ptr = (headerInfoType *) PR_NEWZAP(headerInfoType);
  if ( (ptr) && tPtr)
  {
    ptr->name = nsCRT::strdup(field);
    ptr->value = nsCRT::strdup(value);
    tPtr->AppendElement(ptr);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// The following code is responsible for formatting headers in a manner that is
// identical to the normal XUL output.
////////////////////////////////////////////////////////////////////////////////

nsresult
nsMimeBaseEmitter::WriteHeaderFieldHTML(const char *field, const char *value)
{
  char  *newValue = nsnull;

  if ( (!field) || (!value) )
    return NS_OK;

  //
  // This is a check to see what the pref is for header display. If
  // We should only output stuff that corresponds with that setting.
  //
  if (!EmitThisHeaderForPrefSetting(mHeaderDisplayType, field))
    return NS_OK;

  if ( (mUnicodeConverter) && (mFormat != nsMimeOutput::nsMimeMessageSaveAs) )
  {
    nsXPIDLCString tValue;

    // we're going to need a converter to convert
    nsresult rv = mUnicodeConverter->DecodeMimeHeader(value, getter_Copies(tValue));
    if (NS_SUCCEEDED(rv) && tValue)
    {
      newValue = nsEscapeHTML(tValue);
    }
    else
    {
      newValue = nsEscapeHTML(value);
    }
  }
  else
  {
    newValue = nsCRT::strdup(value);
  }

  if (!newValue)
    return NS_OK;

  mHTMLHeaders.Append("<tr>");
  mHTMLHeaders.Append("<td>");

  if (mFormat == nsMimeOutput::nsMimeMessageSaveAs)
    mHTMLHeaders.Append("<b>");
  else
    mHTMLHeaders.Append("<div class=\"headerdisplayname\" style=\"display:inline;\">");

  // Here is where we are going to try to L10N the tagName so we will always
  // get a field name next to an emitted header value. Note: Default will always
  // be the name of the header itself.
  //
  nsCAutoString  newTagName(field);
  newTagName.CompressWhitespace(PR_TRUE, PR_TRUE);
  newTagName.ToUpperCase();

  char *l10nTagName = LocalizeHeaderName(newTagName.get(), field);
  if ( (!l10nTagName) || (!*l10nTagName) )
    mHTMLHeaders.Append(field);
  else
  {
    mHTMLHeaders.Append(l10nTagName);
    PR_FREEIF(l10nTagName);
  }

  mHTMLHeaders.Append(": ");
  if (mFormat == nsMimeOutput::nsMimeMessageSaveAs)
    mHTMLHeaders.Append("</b>");
  else
    mHTMLHeaders.Append("</div>");

  // Now write out the actual value itself and move on!
  //
  mHTMLHeaders.Append(newValue);
  mHTMLHeaders.Append("</td>");

  mHTMLHeaders.Append("</tr>");

  PR_FREEIF(newValue);
  return NS_OK;
}

nsresult
nsMimeBaseEmitter::WriteHeaderFieldHTMLPrefix()
{
  if ( 
      ( (mFormat == nsMimeOutput::nsMimeMessageSaveAs) && (mFirstHeaders) ) ||
      ( (mFormat == nsMimeOutput::nsMimeMessagePrintOutput) && (mFirstHeaders) )
     )
     /* DO NOTHING */ ;   // rhp: Do nothing...leaving the conditional like this so its 
                          //      easier to see the logic of what is going on. 
  else
    mHTMLHeaders.Append("<br><hr width=\"90%\" size=4><br>");

  mFirstHeaders = PR_FALSE;
  return NS_OK;
}

nsresult
nsMimeBaseEmitter::WriteHeaderFieldHTMLPostfix()
{
  mHTMLHeaders.Append("<br>");
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::WriteHTMLHeaders()
{
  WriteHeaderFieldHTMLPrefix();

  // Start with the subject, from date info!
  DumpSubjectFromDate();

  // Continue with the to and cc headers
  DumpToCC();

  // Do the rest of the headers, but these will only be written if
  // the user has the "show all headers" pref set
  DumpRestOfHeaders();

  WriteHeaderFieldHTMLPostfix();

  // Now, we need to either append the headers we built up to the 
  // overall body or output to the stream.
  if ( (!mDocHeader) && (mFormat == nsMimeOutput::nsMimeMessageXULDisplay) )
    mBody.Append(mHTMLHeaders);
  else
    UtilityWriteCRLF(mHTMLHeaders.get());

  mHTMLHeaders = "";

  return NS_OK;
}

nsresult
nsMimeBaseEmitter::DumpSubjectFromDate()
{
  mHTMLHeaders.Append("<table border=0 cellspacing=0 cellpadding=0 width=\"100%\" name=\"header-part1\">");

    // This is the envelope information
    OutputGenericHeader(HEADER_SUBJECT);
    OutputGenericHeader(HEADER_FROM);
    OutputGenericHeader(HEADER_DATE);

    // If we are Quoting a message, then we should dump the To: also
    if ( ( mFormat == nsMimeOutput::nsMimeMessageQuoting ) ||
         ( mFormat == nsMimeOutput::nsMimeMessageBodyQuoting ) )
      OutputGenericHeader(HEADER_TO);

  mHTMLHeaders.Append("</table>");
 
  return NS_OK;
}

nsresult
nsMimeBaseEmitter::DumpToCC()
{
  char * toField = GetHeaderValue(HEADER_TO, mHeaderArray);
  char * ccField = GetHeaderValue(HEADER_CC, mHeaderArray);
  char * bccField = GetHeaderValue(HEADER_BCC, mHeaderArray);
  char * newsgroupField = GetHeaderValue(HEADER_NEWSGROUPS, mHeaderArray);

  // only dump these fields if we have at least one of them! When displaying news
  // messages that didn't have a To or Cc field, we'd always get an empty box
  // which looked weird.
  if (toField || ccField || bccField || newsgroupField)
  {
    mHTMLHeaders.Append("<table border=0 cellspacing=0 cellpadding=0 width=\"100%\" name=\"header-part2\">");

    OutputGenericHeader(HEADER_TO);
    OutputGenericHeader(HEADER_CC);
    OutputGenericHeader(HEADER_BCC);
    OutputGenericHeader(HEADER_NEWSGROUPS);

    mHTMLHeaders.Append("</table>");
  }

  return NS_OK;
}

nsresult
nsMimeBaseEmitter::DumpRestOfHeaders()
{
  PRInt32     i;
  
  if (mHeaderDisplayType != nsMimeHeaderDisplayTypes::AllHeaders)
    return NS_OK;

  mHTMLHeaders.Append("<table border=0 cellspacing=0 cellpadding=0 width=\"100%\" name=\"header-part3\">");
  
  for (i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) ||
      (!headerInfo->value) || (!(*headerInfo->value)))
      continue;
    
    if ( (!nsCRT::strcasecmp(HEADER_SUBJECT, headerInfo->name)) ||
      (!nsCRT::strcasecmp(HEADER_DATE, headerInfo->name)) ||
      (!nsCRT::strcasecmp(HEADER_FROM, headerInfo->name)) ||
      (!nsCRT::strcasecmp(HEADER_TO, headerInfo->name)) ||
      (!nsCRT::strcasecmp(HEADER_CC, headerInfo->name)) )
      continue;
    
    WriteHeaderFieldHTML(headerInfo->name, headerInfo->value);
  }
  
  mHTMLHeaders.Append("</table>");
  return NS_OK;
}

nsresult
nsMimeBaseEmitter::OutputGenericHeader(const char *aHeaderVal)
{
  char      *val = nsnull;
  nsresult  rv;

  if (mDocHeader)
    val = GetHeaderValue(aHeaderVal, mHeaderArray);
  else
    val = GetHeaderValue(aHeaderVal, mEmbeddedHeaderArray);

  if (val)
  {
    rv = WriteHeaderFieldHTML(aHeaderVal, val);
    return rv;
  }
  else
    return NS_ERROR_FAILURE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// These are the methods that should be implemented by the child class!
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//
// This should be implemented by the child class if special processing
// needs to be done when the entire message is read.
//
NS_IMETHODIMP
nsMimeBaseEmitter::Complete()
{
  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  PRUint32      written;

  nsresult rv = NS_OK;
  while ( NS_SUCCEEDED(rv) && (mBufferMgr) && (mBufferMgr->GetSize() > 0))
    rv = Write("", 0, &written);

  if (mOutListener)
  {
    PRUint32 bytesInStream;
    nsresult rv2 = mInputStream->Available(&bytesInStream);
	NS_ASSERTION(NS_SUCCEEDED(rv2), "Available failed");
    nsCOMPtr<nsIRequest> request = do_QueryInterface(mChannel);
    rv2 = mOutListener->OnDataAvailable(request, mURL, mInputStream, 0, bytesInStream);
	NS_ASSERTION(NS_SUCCEEDED(rv2), "OnDataAvailable failed");
  }

  return NS_OK;
}

//
// This needs to do the right thing with the stored information. It only 
// has to do the output functions, this base class will take care of the 
// memory cleanup
//
NS_IMETHODIMP
nsMimeBaseEmitter::EndHeader()
{
  return NS_OK;
}

// body handling routines
NS_IMETHODIMP
nsMimeBaseEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMimeBaseEmitter::EndBody()
{
  return NS_OK;
}
