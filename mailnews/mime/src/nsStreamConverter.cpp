/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Brodie Thiesfield <brofield@jellycan.com>
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
#include "nsCOMPtr.h"
#include <stdio.h>
#include "mimecom.h"
#include "modmimee.h"
#include "nscore.h"
#include "nsStreamConverter.h"
#include "comi18n.h"
#include "prmem.h"
#include "prprf.h"
#include "prlog.h"
#include "plstr.h"
#include "mimemoz2.h"
#include "nsMimeTypes.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsMemory.h"
#include "nsIPipe.h"
#include "nsMimeStringResources.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsNetUtil.h"
#include "nsIMsgQuote.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "mozITXTToHTMLConv.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMsgWindow.h"
#include "nsStreamConverter.h"
#include "nsICategoryManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

#define PREF_MAIL_DISPLAY_GLYPH "mail.display_glyph"
#define PREF_MAIL_DISPLAY_STRUCT "mail.display_struct"

////////////////////////////////////////////////////////////////
// Bridge routines for new stream converter XP-COM interface 
////////////////////////////////////////////////////////////////

extern "C" void  *
mime_bridge_create_draft_stream(nsIMimeEmitter      *newEmitter,
                                nsStreamConverter   *newPluginObj2,
                                nsIURI              *uri,
                                nsMimeOutputType    format_out);

extern "C" void  *
bridge_create_stream(nsIMimeEmitter      *newEmitter,
                     nsStreamConverter   *newPluginObj2,
                     nsIURI              *uri,
                     nsMimeOutputType    format_out,
                     PRUint32            whattodo,
                     nsIChannel          *aChannel)
{
  if  ( (format_out == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
        (format_out == nsMimeOutput::nsMimeMessageEditorTemplate) )
    return mime_bridge_create_draft_stream(newEmitter, newPluginObj2, uri, format_out);
  else 
    return mime_bridge_create_display_stream(newEmitter, newPluginObj2, uri, format_out, whattodo,
                                             aChannel);
}

void
bridge_destroy_stream(void *newStream)
{
  nsMIMESession     *stream = (nsMIMESession *)newStream;
  if (!stream)
    return;
  
  PR_FREEIF(stream);
}

void          
bridge_set_output_type(void *bridgeStream, nsMimeOutputType aType)
{
  nsMIMESession *session = (nsMIMESession *)bridgeStream;

  if (session)
  {
    // BAD ASSUMPTION!!!! NEED TO CHECK aType
    struct mime_stream_data *msd = (struct mime_stream_data *)session->data_object;
    if (msd)
      msd->format_out = aType;     // output format type
  }
}

nsresult
bridge_new_new_uri(void *bridgeStream, nsIURI *aURI, PRInt32 aOutputType)
{
  nsMIMESession *session = (nsMIMESession *)bridgeStream;
  const char    **fixup_pointer = nsnull;

  if (session)
  {
    if (session->data_object)
    {
      PRBool   *override_charset = nsnull;
      char    **default_charset = nsnull;
      char    **url_name = nsnull;

      if  ( (aOutputType == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
            (aOutputType == nsMimeOutput::nsMimeMessageEditorTemplate) )
      {
        struct mime_draft_data *mdd = (struct mime_draft_data *)session->data_object;
        if (mdd->options)
        {
          default_charset = &(mdd->options->default_charset);
          override_charset = &(mdd->options->override_charset);
          url_name = &(mdd->url_name);
        }
      }
      else
      {
        struct mime_stream_data *msd = (struct mime_stream_data *)session->data_object;

        if (msd->options)
        {
          default_charset = &(msd->options->default_charset);
          override_charset = &(msd->options->override_charset);
          url_name = &(msd->url_name);
          fixup_pointer = &(msd->options->url);
        }
      }

      if ( (default_charset) && (override_charset) && (url_name) )
      {
        // 
        // set the default charset to be the folder charset if we have one associated with
        // this url...
        nsCOMPtr<nsIMsgI18NUrl> i18nUrl (do_QueryInterface(aURI));
        if (i18nUrl)
        {
          nsXPIDLCString charset;

          // check to see if we have a charset override...and if we do, set that field appropriately too...
          nsresult rv = i18nUrl->GetCharsetOverRide(getter_Copies(charset));
          if (NS_SUCCEEDED(rv) && !charset.IsEmpty() ) {
            *override_charset = PR_TRUE;
            *default_charset = ToNewCString(charset);
          }
          else
          {
            i18nUrl->GetFolderCharset(getter_Copies(charset));
            if (!charset.IsEmpty())
              *default_charset = ToNewCString(charset);
          }

          // if there is no manual override and a folder charset exists
          // then check if we have a folder level override
          if (!(*override_charset) && *default_charset && **default_charset)
          {
            PRBool folderCharsetOverride;
            rv = i18nUrl->GetFolderCharsetOverride(&folderCharsetOverride);
            if (NS_SUCCEEDED(rv) && folderCharsetOverride)
              *override_charset = PR_TRUE;

            // notify the default to msgWindow (for the menu check mark)
            // do not set the default in case of nsMimeMessageDraftOrTemplate
            // or nsMimeMessageEditorTemplate because it is already set 
            // when the message is displayed and doing it again may overwrite 
            // the correct MIME charset parsed from the message header
            if (aOutputType != nsMimeOutput::nsMimeMessageDraftOrTemplate &&
                aOutputType != nsMimeOutput::nsMimeMessageEditorTemplate)
            {
              nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(aURI));
              if (msgurl)
              {
                nsCOMPtr<nsIMsgWindow> msgWindow;
                msgurl->GetMsgWindow(getter_AddRefs(msgWindow));
                if (msgWindow)
                {
                  msgWindow->SetMailCharacterSet(*default_charset);
                  msgWindow->SetCharsetOverride(*override_charset);
                }
              }
            }

            // if the pref says always override and no manual override then set the folder charset to override
            if (!*override_charset) {
              nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
              if (pPrefBranch)
              {
                PRBool  force_override;
                rv = pPrefBranch->GetBoolPref("mailnews.force_charset_override", &force_override);
                if (NS_SUCCEEDED(rv) && force_override) 
                {
                  *override_charset = PR_TRUE;
                }
              }
            }
          }
        }
        nsCAutoString urlString;
        if (NS_SUCCEEDED(aURI->GetSpec(urlString)))
        {
          if (!urlString.IsEmpty())
          {
            CRTFREEIF(*url_name);
            *url_name = ToNewCString(urlString);
            if (!(*url_name))
              return NS_ERROR_OUT_OF_MEMORY;

            // rhp: Ugh, this is ugly...but it works.
            if (fixup_pointer)
              *fixup_pointer = (const char *)*url_name;
          }
        }
      }
    }
  }

  return NS_OK;
}

static int
mime_headers_callback ( void *closure, MimeHeaders *headers )
{
  // We get away with this because this doesn't get called on draft operations.
  struct mime_stream_data *msd = (struct mime_stream_data *)closure;

  PR_ASSERT ( msd && headers );
  if ( !msd || ! headers ) 
    return 0;

  PR_ASSERT ( msd->headers == NULL );
  msd->headers = MimeHeaders_copy ( headers );
  return 0;
}

nsresult          
bridge_set_mime_stream_converter_listener(void *bridgeStream, nsIMimeStreamConverterListener* listener,
                                          nsMimeOutputType aOutputType)
{
  nsMIMESession *session = (nsMIMESession *)bridgeStream;

  if ( (session) && (session->data_object) )
  {
    if  ( (aOutputType == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
          (aOutputType == nsMimeOutput::nsMimeMessageEditorTemplate) )
    {
      struct mime_draft_data *mdd = (struct mime_draft_data *)session->data_object;
      if (mdd->options)
      {
        if (listener)
        {
          mdd->options->caller_need_root_headers = PR_TRUE;
          mdd->options->decompose_headers_info_fn = mime_headers_callback;
        }
        else
        {
          mdd->options->caller_need_root_headers = PR_FALSE;
          mdd->options->decompose_headers_info_fn = nsnull;
        }
      }
    }
    else
    {
      struct mime_stream_data *msd = (struct mime_stream_data *)session->data_object;
      
      if (msd->options)
      {
        if (listener)
        {
          msd->options->caller_need_root_headers = PR_TRUE;
          msd->options->decompose_headers_info_fn = mime_headers_callback;
        }
        else
        {
          msd->options->caller_need_root_headers = PR_FALSE;
          msd->options->decompose_headers_info_fn = nsnull;
        }
      }
    }
  }

  return NS_OK;
}

// find a query element in a url and return a pointer to it's data
// (query must be in the form "query=")
static const char * 
FindQueryElementData(const char * aUrl, const char * aQuery)
{
  if (aUrl && aQuery)
  {
    size_t queryLen = 0; // we don't call strlen until we need to
    aUrl = PL_strcasestr(aUrl, aQuery);
    while (aUrl)
    {
      if (!queryLen) 
        queryLen = strlen(aQuery);
      if (*(aUrl-1) == '&' || *(aUrl-1) == '?')
        return aUrl + queryLen;
      aUrl = PL_strcasestr(aUrl + queryLen, aQuery);
    }
  }
  return nsnull;
}

// case-sensitive test for string prefixing. If |string| is prefixed
// by |prefix| then a pointer to the next character in |string| following
// the prefix is returned. If it is not a prefix then |nsnull| is returned.
static const char * 
SkipPrefix(const char *aString, const char *aPrefix)
{
  while (*aPrefix)
    if (*aPrefix++ != *aString++)
       return nsnull;
  return aString;
}

//
// Utility routines needed by this interface
//
nsresult
nsStreamConverter::DetermineOutputFormat(const char *aUrl, nsMimeOutputType *aNewType)
{
  // sanity checking
  NS_ENSURE_ARG_POINTER(aNewType);
  if (!aUrl || !*aUrl)
  {
    // default to html for the entire document
    *aNewType = nsMimeOutput::nsMimeMessageQuoting;
    mOutputFormat = "text/html";
    return NS_OK;
  }

  // shorten the url that we test for the query strings by skipping directly
  // to the part where the query strings begin.
  const char *queryPart = PL_strchr(aUrl, '?');

  // First, did someone pass in a desired output format. They will be able to
  // pass in any content type (i.e. image/gif, text/html, etc...but the "/" will
  // have to be represented via the "%2F" value
  const char *format = FindQueryElementData(queryPart, "outformat=");
  if (format)
  {
    //NOTE: I've done a file contents search of every file (*.*) in the mozilla 
    // directory tree and there is not a single location where the string "outformat"
    // is added to any URL. It appears that this code has been orphaned off by a change 
    // elsewhere and is no longer required. It will be removed in the future unless 
    // someone complains.
    NS_ABORT_IF_FALSE(PR_FALSE, "Is this code actually being used?");

    while (*format == ' ')
      ++format;

    if (*format)
    {
      mOverrideFormat = "raw";

      // set mOutputFormat to the supplied format, ensure that we replace any
      // %2F strings with the slash character
      const char *nextField = PL_strpbrk(format, "&; ");
      mOutputFormat.Assign(format, nextField ? nextField - format : -1);
      mOutputFormat.ReplaceSubstring("%2F", "/");
      mOutputFormat.ReplaceSubstring("%2f", "/");
  
      // Don't muck with this data!
      *aNewType = nsMimeOutput::nsMimeMessageRaw;
      return NS_OK;
    }
  }

  // is this is a part that should just come out raw
  const char *part = FindQueryElementData(queryPart, "part=");
  if (part)
  {
    // default for parts
    mOutputFormat = "raw";
    *aNewType = nsMimeOutput::nsMimeMessageRaw;

    // if we are being asked to fetch a part....it should have a 
    // content type appended to it...if it does, we want to remember
    // that as mOutputFormat
    const char * typeField = FindQueryElementData(queryPart, "type=");
    if (typeField)
    {
      // store the real content type...mOutputFormat gets deleted later on...
      // and make sure we only get our own value.
      char *nextField = PL_strchr(typeField, '&');
      mRealContentType.Assign(typeField, nextField ? nextField - typeField : -1);

      if (mRealContentType.LowerCaseEqualsLiteral("message/rfc822"))
      {
        mRealContentType = "x-message-display";
        mOutputFormat = "text/html";
        *aNewType = nsMimeOutput::nsMimeMessageBodyDisplay;
      }
      else if (mRealContentType.LowerCaseEqualsLiteral("x-message-display"))
      {
        mRealContentType = "";
        mOutputFormat = "text/html";
        *aNewType = nsMimeOutput::nsMimeMessageBodyDisplay;
      }
    }

    return NS_OK;
  }

  // if using the header query
  const char *header = FindQueryElementData(queryPart, "header=");
  if (header)
  {
    struct HeaderType {
      const char * headerType;
      const char * outputFormat;
      nsMimeOutputType mimeOutputType;
    };

    // place most commonly used options at the top
    static const struct HeaderType rgTypes[] = 
    {
      { "filter",    "text/html",  nsMimeOutput::nsMimeMessageFilterSniffer },
      { "quotebody", "text/html",  nsMimeOutput::nsMimeMessageBodyQuoting },
      { "print",     "text/html",  nsMimeOutput::nsMimeMessagePrintOutput },
      { "only",      "text/xml",   nsMimeOutput::nsMimeMessageHeaderDisplay },
      { "none",      "text/html",  nsMimeOutput::nsMimeMessageBodyDisplay },
      { "quote",     "text/html",  nsMimeOutput::nsMimeMessageQuoting },
      { "saveas",    "text/html",  nsMimeOutput::nsMimeMessageSaveAs },
      { "src",       "text/plain", nsMimeOutput::nsMimeMessageSource }
    };

    // find the requested header in table, ensure that we don't match on a prefix
    // by checking that the following character is either null or the next query element
    const char * remainder;
    for (int n = 0; n < NS_ARRAY_LENGTH(rgTypes); ++n)
    {
      remainder = SkipPrefix(header, rgTypes[n].headerType);
      if (remainder && (*remainder == '\0' || *remainder == '&'))
      {
        mOutputFormat = rgTypes[n].outputFormat;
        *aNewType = rgTypes[n].mimeOutputType;
        return NS_OK;
      }
    }
  }
  
  // default to html for just the body
  mOutputFormat = "text/html";
  *aNewType = nsMimeOutput::nsMimeMessageBodyDisplay;

  return NS_OK;
}

nsresult 
nsStreamConverter::InternalCleanup(void)
{
  if (mBridgeStream)
  {
    bridge_destroy_stream(mBridgeStream);
    mBridgeStream = nsnull;
  }

  return NS_OK;
}

/* 
 * Inherited methods for nsMimeConverter
 */
nsStreamConverter::nsStreamConverter()
{
  // Init member variables...
  mWrapperOutput = PR_FALSE;
  mBridgeStream = NULL;
  mOutputFormat = "text/html";
  mAlreadyKnowOutputType = PR_FALSE;
  mForwardInline = PR_FALSE;
  
  mPendingRequest = nsnull;
  mPendingContext = nsnull;
}

nsStreamConverter::~nsStreamConverter()
{
  InternalCleanup();
}

NS_IMPL_THREADSAFE_ADDREF(nsStreamConverter)
NS_IMPL_THREADSAFE_RELEASE(nsStreamConverter)

NS_INTERFACE_MAP_BEGIN(nsStreamConverter)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamConverter)
   NS_INTERFACE_MAP_ENTRY(nsIMimeStreamConverter)
NS_INTERFACE_MAP_END

///////////////////////////////////////////////////////////////
// nsStreamConverter definitions....
///////////////////////////////////////////////////////////////

NS_IMETHODIMP nsStreamConverter::Init(nsIURI *aURI, nsIStreamListener * aOutListener, nsIChannel *aChannel)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsresult rv = NS_OK; 
  mOutListener = aOutListener;
  
  // mscott --> we need to look at the url and figure out what the correct output type is...
  nsMimeOutputType newType = mOutputType;
  if (!mAlreadyKnowOutputType)
  {
    nsCAutoString urlSpec;
    rv = aURI->GetSpec(urlSpec);
    DetermineOutputFormat(urlSpec.get(), &newType);
    mAlreadyKnowOutputType = PR_TRUE;
    mOutputType = newType;  
  }
  
  switch (newType)
  {
    case nsMimeOutput::nsMimeMessageSplitDisplay:    // the wrapper HTML output to produce the split header/body display
      mWrapperOutput = PR_TRUE;
      mOutputFormat = "text/html";
      break;
    case nsMimeOutput::nsMimeMessageHeaderDisplay:   // the split header/body display
      mOutputFormat = "text/xml";
      break;
    case nsMimeOutput::nsMimeMessageBodyDisplay:   // the split header/body display
      mOutputFormat = "text/html";
      break;
      
    case nsMimeOutput::nsMimeMessageQuoting:      // all HTML quoted output
    case nsMimeOutput::nsMimeMessageSaveAs:       // Save as operation
    case nsMimeOutput::nsMimeMessageBodyQuoting:  // only HTML body quoted output
    case nsMimeOutput::nsMimeMessagePrintOutput:  // all Printing output
      mOutputFormat = "text/html";
      break;
      
    case nsMimeOutput::nsMimeMessageDecrypt:  
    case nsMimeOutput::nsMimeMessageRaw:              // the raw RFC822 data and attachments
      mOutputFormat = "raw";
      break;
      
    case nsMimeOutput::nsMimeMessageSource:      // the raw RFC822 data (view source) and attachments
      mOutputFormat = "text/plain";
      mOverrideFormat = "raw";
      break;
      
    case nsMimeOutput::nsMimeMessageDraftOrTemplate:       // Loading drafts & templates
      mOutputFormat = "message/draft";
      break;
      
    case nsMimeOutput::nsMimeMessageEditorTemplate:       // Loading templates into editor
      mOutputFormat = "text/html";
      break;

    case nsMimeOutput::nsMimeMessageFilterSniffer: // output all displayable part as raw 
      mOutputFormat = "text/html";
      break;

    default:
      NS_ASSERTION(0, "this means I made a mistake in my assumptions");
  }
  
  
  // the following output channel stream is used to fake the content type for people who later
  // call into us..
  nsXPIDLCString contentTypeToUse;
  GetContentType(getter_Copies(contentTypeToUse));
  // mscott --> my theory is that we don't need this fake outgoing channel. Let's use the
  // original channel and just set our content type ontop of the original channel...

  aChannel->SetContentType(contentTypeToUse);

  //rv = NS_NewInputStreamChannel(getter_AddRefs(mOutgoingChannel), aURI, nsnull, contentTypeToUse, -1);
  //if (NS_FAILED(rv)) 
  //    return rv;
  
  // Set system principal for this document, which will be dynamically generated 
  
  // We will first find an appropriate emitter in the repository that supports 
  // the requested output format...note, the special exceptions are nsMimeMessageDraftOrTemplate
  // or nsMimeMessageEditorTemplate where we don't need any emitters
  //
  
  if ( (newType != nsMimeOutput::nsMimeMessageDraftOrTemplate) && 
    (newType != nsMimeOutput::nsMimeMessageEditorTemplate) )
  {
    nsCAutoString categoryName ("@mozilla.org/messenger/mimeemitter;1?type=");
    if (!mOverrideFormat.IsEmpty())
      categoryName += mOverrideFormat;
    else
      categoryName += mOutputFormat;
    
    nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLCString contractID;
      catman->GetCategoryEntry("mime-emitter", categoryName.get(), getter_Copies(contractID));
      if (!contractID.IsEmpty())
        categoryName = contractID;
    }

    mEmitter = do_CreateInstance(categoryName.get(), &rv);

    if ((NS_FAILED(rv)) || (!mEmitter))
    {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
    
  // now we want to create a pipe which we'll use for converting the data...
  rv = NS_NewPipe(getter_AddRefs(mInputStream), getter_AddRefs(mOutputStream),
                  NS_STREAM_CONVERTER_SEGMENT_SIZE,
                  /* PR_UINT32_MAX */  NS_STREAM_CONVERTER_BUFFER_SIZE, 
                  PR_TRUE, PR_TRUE);
  
  // initialize our emitter
  if (NS_SUCCEEDED(rv) && mEmitter)
  {
    mEmitter->Initialize(aURI, aChannel, newType);
    mEmitter->SetPipe(mInputStream, mOutputStream);
    mEmitter->SetOutputListener(aOutListener);
  }
  
  PRUint32 whattodo = mozITXTToHTMLConv::kURLs;
  PRBool enable_emoticons = PR_TRUE;
  PRBool enable_structs = PR_TRUE;

  nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (pPrefBranch)
  {
    rv = pPrefBranch->GetBoolPref(PREF_MAIL_DISPLAY_GLYPH,&enable_emoticons);
    if (NS_FAILED(rv) || enable_emoticons) 
    {
      whattodo = whattodo | mozITXTToHTMLConv::kGlyphSubstitution;
    }
    rv = pPrefBranch->GetBoolPref(PREF_MAIL_DISPLAY_STRUCT,&enable_structs);
    if (NS_FAILED(rv) || enable_structs) 
    {
      whattodo = whattodo | mozITXTToHTMLConv::kStructPhrase;
    }
  }

  if (mOutputType == nsMimeOutput::nsMimeMessageSource)
    return NS_OK;
  else
  {
    mBridgeStream = bridge_create_stream(mEmitter, this, aURI, newType, whattodo, aChannel);
    if (!mBridgeStream)
      return NS_ERROR_OUT_OF_MEMORY;
    else
    {
      SetStreamURI(aURI);

      //Do we need to setup an Mime Stream Converter Listener?
      if (mMimeStreamConverterListener)
        bridge_set_mime_stream_converter_listener((nsMIMESession *)mBridgeStream, mMimeStreamConverterListener, mOutputType);

      return NS_OK;
    }
  }
}

NS_IMETHODIMP nsStreamConverter::GetContentType(char **aOutputContentType)
{
  if (!aOutputContentType)
    return NS_ERROR_NULL_POINTER;

  // since this method passes a string through an IDL file we need to use nsMemory to allocate it 
  // and not nsCRT::strdup!
  //  (1) check to see if we have a real content type...use it first...
  if (!mRealContentType.IsEmpty())
    *aOutputContentType = ToNewCString(mRealContentType);
  else if (mOutputFormat.LowerCaseEqualsLiteral("raw"))
  {
    *aOutputContentType = (char *) nsMemory::Clone(UNKNOWN_CONTENT_TYPE, sizeof(UNKNOWN_CONTENT_TYPE));
  }
  else
    *aOutputContentType = ToNewCString(mOutputFormat);
  return NS_OK;
}

// 
// This is the type of output operation that is being requested by libmime. The types
// of output are specified by nsIMimeOutputType enum
// 
nsresult 
nsStreamConverter::SetMimeOutputType(nsMimeOutputType aType)
{
  mAlreadyKnowOutputType = PR_TRUE;
  mOutputType = aType;
  if (mBridgeStream)
    bridge_set_output_type(mBridgeStream, aType);
  return NS_OK;
}

NS_IMETHODIMP nsStreamConverter::GetMimeOutputType(nsMimeOutputType *aOutFormat)
{
  nsresult rv = NS_OK;
  if (aOutFormat)
    *aOutFormat = mOutputType;
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

// 
// This is needed by libmime for MHTML link processing...this is the URI associated
// with this input stream
// 
nsresult 
nsStreamConverter::SetStreamURI(nsIURI *aURI)
{
  mURI = aURI;
  if (mBridgeStream)
    return bridge_new_new_uri((nsMIMESession *)mBridgeStream, aURI, mOutputType);
  else
    return NS_OK;
}

nsresult
nsStreamConverter::SetMimeHeadersListener(nsIMimeStreamConverterListener *listener, nsMimeOutputType aType)
{
   mMimeStreamConverterListener = listener;
   bridge_set_mime_stream_converter_listener((nsMIMESession *)mBridgeStream, listener, aType);
   return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::SetForwardInline(PRBool forwardInline)
{
  mForwardInline = forwardInline;
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::GetForwardInline(PRBool *result)
{
  if (!result) return NS_ERROR_NULL_POINTER;
  *result = mForwardInline;
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::GetIdentity(nsIMsgIdentity * *aIdentity)
{
  if (!aIdentity) return NS_ERROR_NULL_POINTER;
  /*
  We don't have an identity for the local folders account,
    we will return null but it is not an error!
  */
    *aIdentity = mIdentity;
    NS_IF_ADDREF(*aIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::SetIdentity(nsIMsgIdentity * aIdentity)
{
  mIdentity = aIdentity;
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::SetOriginalMsgURI(const char * originalMsgURI)
{
  mOriginalMsgURI = originalMsgURI;
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::GetOriginalMsgURI(char ** result)
{
  if (!result) return NS_ERROR_NULL_POINTER;
  *result = ToNewCString(mOriginalMsgURI);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Methods for nsIStreamListener...
/////////////////////////////////////////////////////////////////////////////
//
// Notify the client that data is available in the input stream.  This
// method is called whenver data is written into the input stream by the
// networking library...
//
nsresult 
nsStreamConverter::OnDataAvailable(nsIRequest     *request, 
                                   nsISupports    *ctxt, 
                                   nsIInputStream *aIStream, 
                                   PRUint32       sourceOffset, 
                                   PRUint32       aLength)
{
  nsresult        rc=NS_OK;     // should this be an error instead?
  PRUint32        readLen = aLength;
  PRUint32        written;

  // If this is the first time through and we are supposed to be 
  // outputting the wrapper two pane URL, then do it now.
  if (mWrapperOutput)
  {
    char        outBuf[1024];
const char output[] = "\
<HTML>\
<FRAMESET ROWS=\"30%%,70%%\">\
<FRAME NAME=messageHeader SRC=\"%s?header=only\">\
<FRAME NAME=messageBody SRC=\"%s?header=none\">\
</FRAMESET>\
</HTML>";

    nsCAutoString url;
    if (NS_FAILED(mURI->GetSpec(url)))
      return NS_ERROR_FAILURE;
  
    PR_snprintf(outBuf, sizeof(outBuf), output, url.get(), url.get());
    
    if (mEmitter)
      mEmitter->Write(outBuf, strlen(outBuf), &written);

    // rhp: will this stop the stream???? Not sure.    
    return NS_ERROR_FAILURE;
  }

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  readLen = aLength;
  aIStream->Read(buf, aLength, &readLen);

  // We need to filter out any null characters else we will have a lot of trouble
  // as we use c string everywhere in mime
  char * readPtr;
  char * endPtr = buf + readLen;

  // First let see if the stream contains null characters
  for (readPtr = buf; readPtr < endPtr && *readPtr; readPtr ++)
    ;

  // Did we find a null character? Then, we need to cleanup the stream
  if (readPtr < endPtr)
  {
    char * writePtr = readPtr;
    for (readPtr ++; readPtr < endPtr; readPtr ++)
    {
      if (!*readPtr)
        continue;

      *writePtr = *readPtr;
      writePtr ++;
    }
    readLen = writePtr - buf;
  }

  if (mOutputType == nsMimeOutput::nsMimeMessageSource)
  {
    rc = NS_OK;
    if (mEmitter)
      rc = mEmitter->Write(buf, readLen, &written);
  }
  else if (mBridgeStream)
  {
    nsMIMESession   *tSession = (nsMIMESession *) mBridgeStream;
    rc = tSession->put_block((nsMIMESession *)mBridgeStream, buf, readLen);
  }

  PR_FREEIF(buf);
  return rc;
}

/////////////////////////////////////////////////////////////////////////////
// Methods for nsIRequestObserver 
/////////////////////////////////////////////////////////////////////////////
//
// Notify the observer that the URL has started to load.  This method is
// called only once, at the beginning of a URL load.
//
nsresult 
nsStreamConverter::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
#ifdef DEBUG_rhp
    printf("nsStreamConverter::OnStartRequest()\n");
#endif

#ifdef DEBUG_mscott
  mConvertContentTime = PR_IntervalNow();
#endif

  // here's a little bit of hackery....
  // since the mime converter is now between the channel
  // and the 
  if (request)
  {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel)
    {
      nsXPIDLCString contentType;
      GetContentType(getter_Copies(contentType));

      channel->SetContentType(contentType);
    }
  }

  // forward the start request to any listeners                                 
  if (mOutListener) 
  {
    if (mOutputType == nsMimeOutput::nsMimeMessageRaw)
    {
      //we need to delay the on start request until we have figure out the real content type
      mPendingRequest = request;
      mPendingContext = ctxt;
    }
    else
      mOutListener->OnStartRequest(request, ctxt);
  }

  return NS_OK;                                                                 
}

//
// Notify the observer that the URL has finished loading.  This method is 
// called once when the networking library has finished processing the 
//
nsresult 
nsStreamConverter::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
#ifdef DEBUG_rhp
    printf("nsStreamConverter::OnStopRequest()\n");
#endif

  //
  // Now complete the stream!
  //
  if (mBridgeStream)
  {
    nsMIMESession   *tSession = (nsMIMESession *) mBridgeStream;
    
    if (mMimeStreamConverterListener)
    {

      MimeHeaders    **workHeaders = nsnull;

      if  ( (mOutputType == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
            (mOutputType == nsMimeOutput::nsMimeMessageEditorTemplate) )
      {
        struct mime_draft_data *mdd = (struct mime_draft_data *)tSession->data_object;
        if (mdd)
          workHeaders = &(mdd->headers);
      }
      else
      {
        struct mime_stream_data *msd = (struct mime_stream_data *)tSession->data_object;
        if (msd)
          workHeaders = &(msd->headers);
      }

      if (workHeaders)
      {
        nsresult rv;
        nsCOMPtr<nsIMimeHeaders> mimeHeaders = do_CreateInstance(NS_IMIMEHEADERS_CONTRACTID, &rv);
        
        if (NS_SUCCEEDED(rv))
        {
          if (*workHeaders)
            mimeHeaders->Initialize((*workHeaders)->all_headers, (*workHeaders)->all_headers_fp);
          mMimeStreamConverterListener->OnHeadersReady(mimeHeaders);
        }
        else
          mMimeStreamConverterListener->OnHeadersReady(nsnull);
      }

      mMimeStreamConverterListener = nsnull; // release our reference
    }
    
    tSession->complete((nsMIMESession *)mBridgeStream);
  }

  // 
  // Now complete the emitter and do necessary cleanup!
  //
  if (mEmitter)    
  {
    mEmitter->Complete();
  }

  // First close the output stream...
  if (mOutputStream)
    mOutputStream->Close();

  // Make sure to do necessary cleanup!
  InternalCleanup();

#ifdef DEBUG_mscott
  // print out the mime timing information BEFORE we flush to layout
  // otherwise we'll be including layout information.
  printf("Time Spent in mime:    %d ms\n", PR_IntervalToMilliseconds(PR_IntervalNow() - mConvertContentTime));
#endif

  // forward on top request to any listeners
  if (mOutListener)
    mOutListener->OnStopRequest(request, ctxt, status);
    

  mAlreadyKnowOutputType = PR_FALSE;

  // since we are done converting data, lets close all the objects we own...
  // this helps us fix some circular ref counting problems we are running into...
  Close(); 

  // Time to return...
  return NS_OK;
}

nsresult nsStreamConverter::Close()
{
  mOutgoingChannel = nsnull;
  mEmitter = nsnull;
  mOutListener = nsnull;
  return NS_OK;
}

// nsIStreamConverter implementation

// No syncronous conversion at this time.
NS_IMETHODIMP nsStreamConverter::Convert(nsIInputStream  *aFromStream,
                                         const char *aFromType,
                                         const char *aToType,
                                         nsISupports     *aCtxt, 
                                         nsIInputStream **_retval) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP nsStreamConverter::AsyncConvertData(const char   *aFromType, 
                                                  const char   *aToType,
                                                  nsIStreamListener *aListener, 
                                                  nsISupports       *aCtxt) 
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgQuote> aMsgQuote = do_QueryInterface(aCtxt, &rv);
  nsCOMPtr<nsIChannel> aChannel;
  
  if (aMsgQuote)
  {
    nsCOMPtr<nsIMimeStreamConverterListener> quoteListener;
    rv = aMsgQuote->GetQuoteListener(getter_AddRefs(quoteListener));
    if (quoteListener)
      SetMimeHeadersListener(quoteListener, nsMimeOutput::nsMimeMessageQuoting);
    rv = aMsgQuote->GetQuoteChannel(getter_AddRefs(aChannel));
  }
  else
  {
    aChannel = do_QueryInterface(aCtxt, &rv);
  }

  NS_ASSERTION(aChannel && NS_SUCCEEDED(rv), "mailnews mime converter has to have the channel passed in...");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> aUri;
  aChannel->GetURI(getter_AddRefs(aUri));
  return Init(aUri, aListener, aChannel);
}

NS_IMETHODIMP nsStreamConverter::FirePendingStartRequest()
{
  if (mPendingRequest && mOutListener)
  {
  	mOutListener->OnStartRequest(mPendingRequest, mPendingContext);
    mPendingRequest = nsnull;
    mPendingContext = nsnull; 
  }
  return NS_OK;
}
