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
#include "nsMsgCompose.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMDocument.h"
#include "nsMsgI18N.h"
#include "nsMsgSend.h"
#include "nsIMessenger.h"	//temporary!
#include "nsIMessage.h"		//temporary!
#include "nsMsgQuote.h"
#include "nsIPref.h"
#include "nsIDocumentEncoder.h"    // for editor output flags
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIMsgHeaderParser.h"
#include "nsHTMLToTXTSinkStream.h"
#include "CNavDTD.h"
#include "nsMsgCompUtils.h"
#include "nsMsgComposeStringBundle.h"
#include "nsSpecialSystemDirectory.h"
#include "nsMsgSend.h"
#include "nsMsgCreate.h"

// XXX temporary so we can use the current identity hack -alecf
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

#define TEMP_MESSAGE_IN       "tempMessage.eml"

// Defines....
static NS_DEFINE_CID(kMsgQuoteCID, NS_MSGQUOTE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

// RICHIE
// This is a temp hack and will go away soon!
//
PRBool
nsMsgCompose::UsingOldQuotingHack(const char *compString)
{
  nsresult rv;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs) 
    rv = prefs->GetBoolPref("mail.old_quoting", &mUseOldQuotingHack);

  if (compString)
  {
    if (PL_strncasecmp(compString, "mailbox_message:", 16) != 0)
    {
      mUseOldQuotingHack = PR_TRUE;
    }
  }

  return mUseOldQuotingHack;
}

nsMsgCompose::nsMsgCompose()
{
  mTempComposeFileSpec = nsnull;
  mQuotingToFollow = PR_FALSE;
  mSigFileSpec = nsnull;
  mUseOldQuotingHack = PR_FALSE;  // RICHIE - hack for old quoting
  mWhatHolder = 1;                // RICHIE - hack for old quoting
  mQuoteURI = "";
  mDocumentListener = nsnull;
  mMsgSend = nsnull;
	m_sendListener = nsnull;
	m_window = nsnull;
	m_webShell = nsnull;
	m_webShellWin = nsnull;
	m_editor = nsnull;
	mQuoteStreamListener=nsnull;
	m_compFields = new nsMsgCompFields;
	NS_IF_ADDREF(m_compFields);

	// Get the default charset from pref, use this as a mail charset.
	char * default_mail_charset = nsMsgI18NGetDefaultMailCharset();
	if (default_mail_charset)
	{
   		m_compFields->SetCharacterSet(default_mail_charset);
    	PR_Free(default_mail_charset);
  	}

  m_composeHTML = PR_FALSE;
  // temporary - m_composeHTML from the "current" identity
  // eventually we should know this when we open the compose window
  // -alecf
  nsresult rv;
  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return;
  
  nsCOMPtr<nsIMsgIdentity> identity;
  rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
  if (NS_FAILED(rv)) return;

	rv = identity->GetComposeHtml(&m_composeHTML);
  
	NS_INIT_REFCNT();
}


nsMsgCompose::~nsMsgCompose()
{
  if (m_editor) 
    m_editor->UnregisterDocumentStateListener(mDocumentListener);
  NS_IF_RELEASE(mDocumentListener);
	NS_IF_RELEASE(m_sendListener);
	NS_IF_RELEASE(m_compFields);
	NS_IF_RELEASE(mQuoteStreamListener);
}


/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompose, nsCOMTypeInfo<nsMsgCompose>::GetIID());

nsresult 
nsMsgCompose::SetQuotingToFollow(PRBool aVal)
{
  mQuotingToFollow = aVal;
  return NS_OK;
}

PRBool
nsMsgCompose::QuotingToFollow(void)
{
  return mQuotingToFollow;
}

nsresult
nsMsgCompose::LoadAsQuote(nsString  aTextToLoad)
{
  // Now Load the body...
  if (m_editor)
  {
    if (aTextToLoad.Length())
    {
      m_editor->InsertAsQuotation(aTextToLoad.GetUnicode());
    }
  }

  return NS_OK;
}

nsresult nsMsgCompose::Initialize(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI,
	MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgCompFields *compFields, nsISupports *object)
{
	nsresult rv = NS_OK;

	if (aWindow)
	{
		m_window = aWindow;
		nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(aWindow));
		if (!globalObj)
			return NS_ERROR_FAILURE;
		
		nsCOMPtr<nsIWebShell> webShell;
		globalObj->GetWebShell(getter_AddRefs(webShell));
		if (!webShell)
			return NS_ERROR_NOT_INITIALIZED;
		m_webShell = webShell;
		
		nsCOMPtr<nsIWebShellContainer> webShellContainer;
		m_webShell->GetContainer(*getter_AddRefs(webShellContainer));
		if (!webShellContainer)
			return NS_ERROR_NOT_INITIALIZED;

		nsCOMPtr<nsIWebShellWindow> webShellWin = do_QueryInterface(webShellContainer, &rv);
		m_webShellWin = webShellWin;
  	}
	
	switch (format)
	{
		case MSGCOMP_FORMAT_HTML		: m_composeHTML = PR_TRUE;					break;
		case MSGCOMP_FORMAT_PlainText	: m_composeHTML = PR_FALSE;					break;
    default							: /* m_composeHTML initialized in ctor */	break;

	}
	
	 CreateMessage(originalMsgURI, type, format, compFields, object); //object is temporary

	return rv;
}

nsresult nsMsgCompose::LoadFields()
{
  nsresult rv = NS_OK;
  
  // Not sure if this call is really necessary anymore...
  return rv;
}

nsresult nsMsgCompose::SetDocumentCharset(const PRUnichar *charset) 
{
	// Set charset, this will be used for the MIME charset labeling.
	m_compFields->SetCharacterSet(nsAutoCString(charset));
	
	return NS_OK;
}

nsresult nsMsgCompose::_SendMsg(MSG_DeliverMode deliverMode,
                               nsIMsgIdentity *identity,
                               const PRUnichar *callback)
{
  nsresult rv = NS_OK;
  
  if (m_compFields && identity) 
  {
    // Pref values are supposed to be stored as UTF-8, so no conversion
    nsXPIDLCString email;
    nsXPIDLCString fullName;
    nsXPIDLCString replyTo;
    nsXPIDLCString organization;
    
    identity->GetEmail(getter_Copies(email));
    identity->GetFullName(getter_Copies(fullName));
    identity->GetReplyTo(getter_Copies(replyTo));
    identity->GetOrganization(getter_Copies(organization));
    
	char * sender = nsnull;
	nsCOMPtr<nsIMsgHeaderParser> parser;
    nsComponentManager::CreateInstance(kHeaderParserCID,
                                            nsnull,
                                            nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(),
                                            getter_AddRefs(parser));
	if (parser)
		parser->MakeFullAddress(nsnull, NS_CONST_CAST(char*, (const char *)fullName),
										NS_CONST_CAST(char*, (const char *)email),
										&sender);
	if (!sender)
		m_compFields->SetFrom(NS_CONST_CAST(char*, (const char *)email));
	else
		m_compFields->SetFrom(sender);
	PR_FREEIF(sender);

	//Set the reply-to only if the user have not specified one in the message
	const char * reply = m_compFields->GetReplyTo();
	if (reply == nsnull || *reply == 0)
		m_compFields->SetReplyTo(NS_CONST_CAST(char*, (const char *)replyTo));
    m_compFields->SetOrganization(NS_CONST_CAST(char*, (const char *)organization));
    
#if defined(DEBUG_ducarroz) || defined(DEBUG_seth_)
    printf("----------------------------\n");
    printf("--  Sending Mail Message  --\n");
    printf("----------------------------\n");
    printf("from: %s\n", m_compFields->GetFrom());
    printf("To: %s  Cc: %s  Bcc: %s\n", m_compFields->GetTo(), m_compFields->GetCc(), m_compFields->GetBcc());
    printf("Newsgroups: %s\n", m_compFields->GetNewsgroups());
    printf("Subject: %s  \nMsg: %s\n", m_compFields->GetSubject(), m_compFields->GetBody());
    printf("Attachments: %s\n",m_compFields->GetAttachments());
    printf("----------------------------\n");
#endif //DEBUG

    nsMsgComposeAndSend *tMsgComp = new nsMsgComposeAndSend();
    if (!tMsgComp)
      return NS_ERROR_OUT_OF_MEMORY;

    mMsgSend = do_QueryInterface( tMsgComp );
    if (mMsgSend)
    {
      const char *bodyString = m_compFields->GetBody();
      PRInt32 bodyLength = PL_strlen(bodyString);
      
      
      // Create the listener for the send operation...
      m_sendListener = new nsMsgComposeSendListener();
      if (!m_sendListener)
        return NS_ERROR_FAILURE;
      
      NS_ADDREF(m_sendListener);
      // set this object for use on completion...
      m_sendListener->SetComposeObj(this);
      m_sendListener->SetDeliverMode(deliverMode);
      nsIMsgSendListener **tArray = m_sendListener->CreateListenerArray();
      if (!tArray)
      {
#ifdef DEBUG
        printf("Error creating listener array.\n");
#endif
        return NS_ERROR_FAILURE;
      }
      
      // If we are composing HTML, then this should be sent as
      // multipart/related which means we pass the editor into the
      // backend...if not, just pass nsnull
      //
      nsIEditorShell  *tEditor = nsnull;
      
      if (m_composeHTML)
      {
        tEditor = m_editor;
      }
      else
        tEditor = nsnull;    

      rv = mMsgSend->CreateAndSendMessage(
                    tEditor,
                    identity,
                    m_compFields, 
                    PR_FALSE,         					// PRBool                            digest_p,
                    PR_FALSE,         					// PRBool                            dont_deliver_p,
                    (nsMsgDeliverMode)deliverMode,   		// nsMsgDeliverMode                  mode,
                    nsnull,                     			// nsIMessage *msgToReplace, 
                    m_composeHTML?TEXT_HTML:TEXT_PLAIN,	// const char                        *attachment1_type,
                    bodyString,               			// const char                        *attachment1_body,
                    bodyLength,               			// PRUint32                          attachment1_body_length,
                    nsnull,             					// const struct nsMsgAttachmentData  *attachments,
                    nsnull,             					// const struct nsMsgAttachedFile    *preloaded_attachments,
                    nsnull,             					// nsMsgSendPart                     *relatedPart,
                    tArray);                   			// listener array
      
      delete tArray;
    }
    else
	    	rv = NS_ERROR_FAILURE;
  }
  else
    rv = NS_ERROR_NOT_INITIALIZED;
  
  if (NS_SUCCEEDED(rv))
  {
  /*TODO, don't close the window but just hide it, we will close it later when we receive a call back from the BE
  if (nsnull != mScriptContext) {
		const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;
    
      mScriptContext->EvaluateString(mScript, url, 0, rVal, &isUndefined);
      CloseWindow();
      }
      else // If we don't have a JS callback, then close the window by default!
    */
    // rhp:
    // We shouldn't close the window if we are just saving a draft or a template
    // so do this check
    if ( (deliverMode != nsMsgSaveAsDraft) && (deliverMode != nsMsgSaveAsTemplate) )
      ShowWindow(PR_FALSE);
  }
		
  return rv;
}

nsresult nsMsgCompose::SendMsg(MSG_DeliverMode deliverMode,
                               nsIMsgIdentity *identity,
                               const PRUnichar *callback)
{
	nsresult rv = NS_OK;

	if (m_editor && m_compFields)
	{
		nsAutoString msgBody;
		PRUnichar *bodyText = NULL;
    nsString format;
    PRUint32 flags = 0;
		if (m_composeHTML)
			format = "text/html";
		else
    {
      flags = nsIDocumentEncoder::OutputFormatted;
			format = "text/plain";
    }
    m_editor->GetContentsAs(format.GetUnicode(), flags, &bodyText);
		
		msgBody = bodyText;
		delete [] bodyText;

		// Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		char *outCString;
		nsString aCharset = m_compFields->GetCharacterSet();
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, msgBody, &outCString))) 
		{
			m_compFields->SetBody(outCString);
			PR_Free(outCString);
		}
		else
			m_compFields->SetBody(nsAutoCString(msgBody));
	}
	
	rv = _SendMsg(deliverMode, identity, callback);
	
	return rv;
}


nsresult
nsMsgCompose::SendMsgEx(MSG_DeliverMode deliverMode,
                        nsIMsgIdentity *identity,
                        const PRUnichar *addrTo, const PRUnichar *addrCc,
                        const PRUnichar *addrBcc, const PRUnichar *newsgroup,
                        const PRUnichar *subject, const PRUnichar *body,
                        const PRUnichar *callback)
{
	nsresult rv = NS_OK;

	if (m_compFields && identity) 
	{ 
		nsString aString;
		nsString aCharset(msgCompHeaderInternalCharset());
		char *outCString;

		// Convert fields to UTF-8
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrTo, &outCString))) 
		{
			m_compFields->SetTo(outCString);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetTo(nsAutoCString(addrTo));

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrCc, &outCString))) 
		{
			m_compFields->SetCc(outCString);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetCc(nsAutoCString(addrCc));

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrBcc, &outCString))) 
		{
			m_compFields->SetBcc(outCString);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetBcc(nsAutoCString(addrBcc));

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, newsgroup, &outCString))) 
		{
			m_compFields->SetNewsgroups(outCString);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetNewsgroups(nsAutoCString(newsgroup));
        
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, subject, &outCString))) 
		{
			m_compFields->SetSubject(outCString);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetSubject(nsAutoCString(subject));

		// Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		aCharset.SetString(m_compFields->GetCharacterSet());
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, body, &outCString))) 
		{
			m_compFields->SetBody(outCString);
			PR_Free(outCString);
		}
		else
			m_compFields->SetBody(nsAutoCString(body));

		rv = _SendMsg(deliverMode, identity, callback);
	}
	else
		rv = NS_ERROR_NOT_INITIALIZED;

	return rv;
}

nsresult nsMsgCompose::CloseWindow()
{
	if (m_webShellWin)
	{
    m_editor = nsnull;	      /* m_editor will be destroyed during the Close Window. Set it to null to */
							                /* be sure we wont use it anymore. */
		m_webShellWin->Close();
		m_webShellWin = nsnull;
	}

	return NS_OK;
}

nsresult nsMsgCompose::ShowWindow(PRBool show)
{
	if (m_webShellWin)
	{
		m_webShellWin->Show(show);
	}

	return NS_OK;
}

nsresult nsMsgCompose::GetEditor(nsIEditorShell * *aEditor) 
{ 
 *aEditor = m_editor; 
 return NS_OK; 
} 

nsresult nsMsgCompose::SetEditor(nsIEditorShell * aEditor) 
{ 
  // First, store the editor shell.
  m_editor = aEditor;

  //
  // Now this routine will create a listener for state changes
  // in the editor and set us as the compose object of interest.
  //
  mDocumentListener = new nsMsgDocumentStateListener();
  if (!mDocumentListener)
    return NS_ERROR_OUT_OF_MEMORY;

  mDocumentListener->SetComposeObj(this);      
  NS_ADDREF(mDocumentListener);

  // Make sure we setup to listen for editor state changes...
  m_editor->RegisterDocumentStateListener(mDocumentListener);

  //
  // Have to check to see if there is a body stored in the 
  // comp fields...
  //
  PRBool    bodyInCompFields = PR_FALSE;
  if (m_compFields)
  {
    PRUnichar     *bod;

    m_compFields->GetBody(&bod);
    if ((bod) && (*bod))
      bodyInCompFields = PR_TRUE;
  }

  // Now, do the appropriate startup operation...signature only
  // or quoted message and signature...
  if ( QuotingToFollow() )
    return BuildQuotedMessageAndSignature();
  else if (bodyInCompFields)
    return BuildBodyMessage();
  else
    return ProcessSignature(nsnull);
} 

nsresult nsMsgCompose::GetDomWindow(nsIDOMWindow * *aDomWindow)
{
	*aDomWindow = m_window;
	return NS_OK;
}


nsresult nsMsgCompose::GetCompFields(nsIMsgCompFields * *aCompFields)
{
	*aCompFields = (nsIMsgCompFields*)m_compFields;
	NS_IF_ADDREF(*aCompFields);
	return NS_OK;
}


nsresult nsMsgCompose::GetComposeHTML(PRBool *aComposeHTML)
{
	*aComposeHTML = m_composeHTML;
	return NS_OK;
}


nsresult nsMsgCompose::GetWrapLength(PRInt32 *aWrapLength)
{
  nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  return prefs->GetIntPref("mail.wraplength", aWrapLength);
}

nsresult nsMsgCompose::CreateMessage(const PRUnichar * originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format,
									 nsIMsgCompFields * compFields, nsISupports * object)
{
  nsresult rv = NS_OK;

  if (compFields)
  {
  	  if (m_compFields)
        m_compFields->Copy(compFields);
      return rv;
  }

  /* At this point, we have a list of URI of original message to reply to or forward but as the BE isn't ready yet,
  we still need to use the old patch... gather the information from the object and the temp file use to display the selected message*/
  if (object)
  {
    nsCOMPtr<nsIMessage> message;
    rv = object->QueryInterface(nsIMessage::GetIID(), getter_AddRefs(message));
    if ((NS_SUCCEEDED(rv)) && message)
    {
      nsString aString = "";
      nsString bString = "";
      nsString aCharset = "";
      nsString decodedString;
      nsString encodedCharset;  // we don't use this
      char *aCString;
    
      message->GetCharSet(&aCharset);
      message->GetSubject(&aString);
      switch (type)
      {
      default: break;        
      case MSGCOMP_TYPE_Reply : 
      case MSGCOMP_TYPE_ReplyAll:
        {
          mQuotingToFollow = PR_TRUE;
          // get an original charset, used for a label, UTF-8 is used for the internal processing
          if (!aCharset.Equals(""))
            m_compFields->SetCharacterSet(nsAutoCString(aCharset));
        
          bString += "Re: ";
          bString += aString;
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(bString, encodedCharset, decodedString)))
            m_compFields->SetSubject(decodedString.GetUnicode());
          else
            m_compFields->SetSubject(bString.GetUnicode());
        
          message->GetAuthor(&aString);		
          m_compFields->SetTo(nsAutoCString(aString));
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(aString, encodedCharset, decodedString)))
            if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
            {
              m_compFields->SetTo(aCString);
              PR_Free(aCString);
            }
          
            if (type == MSGCOMP_TYPE_ReplyAll)
            {
              nsString cString, dString;
              message->GetRecipients(&cString);
              CleanUpRecipients(cString);
              message->GetCCList(&dString);
              CleanUpRecipients(dString);
              if (cString.Length() > 0 && dString.Length() > 0)
                cString = cString + ", ";
              cString = cString + dString;
              m_compFields->SetCc(nsAutoCString(cString));
              if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(cString, encodedCharset, decodedString)))
                if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
                {
                  m_compFields->SetCc(aCString);
                  PR_Free(aCString);
                }
            }
          
            // Setup quoting callbacks for later...
            mWhatHolder = 1;
            mQuoteURI = originalMsgURI;
          
            break;
        }
      case MSGCOMP_TYPE_ForwardAsAttachment:
      case MSGCOMP_TYPE_ForwardInline:
        {
          mQuotingToFollow = PR_TRUE;
        
          if (!aCharset.Equals(""))
            m_compFields->SetCharacterSet(nsAutoCString(aCharset));
        
          bString += "[Fwd: ";
          bString += aString;
          bString += "]";
        
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(bString, encodedCharset, decodedString)))
            m_compFields->SetSubject(decodedString.GetUnicode());
          else
            m_compFields->SetSubject(bString.GetUnicode());
        
          // Setup quoting callbacks for later...
          mQuoteURI = originalMsgURI;
          if (type == MSGCOMP_TYPE_ForwardAsAttachment)
          {
            mWhatHolder = 0;
          }
          else
          {
            mWhatHolder = 2;
          }
        
          break;
        }
      }      
    }
  }	

  return rv;
}

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT IS THE STREAM CONSUMER OF THE HTML OUPUT
// FROM LIBMIME. THIS IS FOR QUOTING
////////////////////////////////////////////////////////////////////////////////////
QuotingOutputStreamListener::~QuotingOutputStreamListener() 
{
  if (mComposeObj)
    NS_RELEASE(mComposeObj);
}

QuotingOutputStreamListener::QuotingOutputStreamListener(const PRUnichar * originalMsgURI, PRBool quoteHeaders) 
{ 
  mComposeObj = nsnull;
  mQuoteHeaders = quoteHeaders;

  nsCOMPtr<nsIMessage> originalMsg = getter_AddRefs(GetIMessageFromURI(originalMsgURI));
  if (originalMsg && !quoteHeaders)
  {
    nsresult rv;
	nsString author;
	rv = originalMsg->GetMime2DecodedAuthor(&author);
	if (NS_SUCCEEDED(rv))
	{
	  char * authorName = nsnull;
	  nsCOMPtr<nsIMsgHeaderParser> parser;
      nsComponentManager::CreateInstance(kHeaderParserCID,
                                            nsnull,
                                            nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(),
                                            getter_AddRefs(parser));
	  if (parser)
	    if (NS_SUCCEEDED(parser->ExtractHeaderAddressName(nsnull, nsAutoCString(author), &authorName)))
		{
          mMsgBody = "<br><br>";
	      mMsgBody += authorName;
	      mMsgBody += " wrote:<br><BLOCKQUOTE TYPE=CITE><html>";
		}
	  if (authorName)
		PL_strfree(authorName);
	}
  }
  
  if (mMsgBody.IsEmpty())
    mMsgBody = "<br><br>--- Original Message ---<br><BLOCKQUOTE TYPE=CITE><html>";

  NS_INIT_REFCNT(); 
}

static nsresult
ConvertBufToPlainText(nsString &aConBuf, const char *charSet)
{
  nsresult    rv;
  nsString    convertedText;
  nsIParser   *parser;

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                          kCParserIID, (void **)&parser);
  if (NS_SUCCEEDED(rv) && parser)
  {
    nsHTMLToTXTSinkStream     *sink = nsnull;

    rv = NS_New_HTMLToTXT_SinkStream((nsIHTMLContentSink **)&sink, &convertedText, 0, 0);
    if (sink && NS_SUCCEEDED(rv)) 
    {  
        sink->DoFragment(PR_TRUE);
        parser->SetContentSink(sink);

        // Set the charset...
        // 
        if (charSet)
        {
          nsAutoString cSet(charSet);
          parser->SetDocumentCharset(cSet, kCharsetFromMetaTag);
        }
        
        nsIDTD* dtd = nsnull;
        rv = NS_NewNavHTMLDTD(&dtd);
        if (NS_SUCCEEDED(rv)) 
        {
          parser->RegisterDTD(dtd);
          rv = parser->Parse(aConBuf, 0, "text/html", PR_FALSE, PR_TRUE);           
        }
        NS_IF_RELEASE(dtd);
        NS_IF_RELEASE(sink);
    }

    NS_RELEASE(parser);
    //
    // Now assign the results if we worked!
    //
    if (NS_SUCCEEDED(rv))
      aConBuf = convertedText;
  }

  return rv;
}

nsresult
QuotingOutputStreamListener::ConvertToPlainText()
{
  return ConvertBufToPlainText(mMsgBody, "UTF-8");
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStartRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */)
{
	return NS_OK;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStopRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */, nsresult status, const PRUnichar * /* errorMsg */)
{
  if (mComposeObj) 
  {
    mMsgBody += "</html></BLOCKQUOTE>";

    // Now we have an HTML representation of the quoted message.
    // If we are in plain text mode, we need to convert this to plain
    // text before we try to insert it into the editor. If we don't, we
    // just get lots of HTML text in the message...not good.
    //
    PRBool composeHTML = PR_TRUE;
    mComposeObj->GetComposeHTML(&composeHTML);
    if (!composeHTML)
      ConvertToPlainText();


    //
    // Ok, now we have finished quoting so we should load this into the editor
    // window.
    // 

    PRBool    compHTML = PR_FALSE;
    mComposeObj->GetComposeHTML(&compHTML);
    mComposeObj->mTempComposeFileSpec = nsMsgCreateTempFileSpec(compHTML ? (char *)"nscomp.html" : (char *)"nscomp.txt");
    if (!mComposeObj->mTempComposeFileSpec)
      return NS_MSG_ERROR_WRITING_FILE;

    nsOutputFileStream tempFile(*(mComposeObj->mTempComposeFileSpec));
    if (!tempFile.is_open())
      return NS_MSG_ERROR_WRITING_FILE;        
    tempFile.write(nsAutoCString(mMsgBody), mMsgBody.Length());
    mComposeObj->ProcessSignature(&tempFile);
    tempFile.close();

    // Now load the URL...
    nsString          urlStr = nsMsgPlatformFileToURL(*(mComposeObj->mTempComposeFileSpec));
    nsIEditorShell    *editor;

    mComposeObj->GetEditor(&editor);
    if (editor)
      editor->LoadUrl(urlStr.GetUnicode());
    else
      return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnDataAvailable(nsIChannel * /* aChannel */, 
							 nsISupports *ctxt, nsIInputStream *inStr, 
                             PRUint32 sourceOffset, PRUint32 count)
{
	nsresult rv = NS_OK;
	if (!inStr)
		return NS_ERROR_NULL_POINTER;

	char *newBuf = (char *)PR_Malloc(count + 1);
	if (!newBuf)
		return NS_ERROR_FAILURE;

	PRUint32 numWritten = 0; 
	rv = inStr->Read(newBuf, count, &numWritten);
	newBuf[count] = '\0';
	if (NS_SUCCEEDED(rv))
	{
		mMsgBody += newBuf;
	}

	PR_FREEIF(newBuf);
	return rv;
}

nsresult
QuotingOutputStreamListener::SetComposeObj(nsMsgCompose *obj)
{
  mComposeObj = obj;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotingOutputStreamListener, nsCOMTypeInfo<nsIStreamListener>::GetIID());
////////////////////////////////////////////////////////////////////////////////////
// END OF QUOTING LISTENER
////////////////////////////////////////////////////////////////////////////////////

nsresult
nsMsgCompose::QuoteOriginalMessage(const PRUnichar *originalMsgURI, PRInt32 what) // New template
{
  nsresult    rv;

  mQuotingToFollow = PR_FALSE;

  nsString    tmpURI(originalMsgURI);
  char        *compString = tmpURI.ToNewCString();

  // RICHIE - this will get removed when we 
  UsingOldQuotingHack(compString);
  PR_FREEIF(compString);

  if (mUseOldQuotingHack)
  {
  	printf("nsMsgCompose: using old quoting function!");
    HackToGetBody(what);
    return NS_OK;
  }

  // Create a mime parser (nsIStreamConverter)!
  rv = nsComponentManager::CreateInstance(kMsgQuoteCID, 
                                          NULL, nsCOMTypeInfo<nsIMsgQuote>::GetIID(), 
                                          (void **) getter_AddRefs(mQuote)); 
  if (NS_FAILED(rv) || !mQuote)
    return NS_ERROR_FAILURE;

  // Create the consumer output stream.. this will receive all the HTML from libmime
  mQuoteStreamListener = new QuotingOutputStreamListener(originalMsgURI, what != 1);
  
  if (!mQuoteStreamListener)
  {
    printf("Failed to create mQuoteStreamListener\n");
    return NS_ERROR_FAILURE;
  }
  NS_ADDREF(mQuoteStreamListener);

  NS_ADDREF(this);
  mQuoteStreamListener->SetComposeObj(this);

  return mQuote->QuoteMessage(originalMsgURI, what != 1, mQuoteStreamListener);
}

void nsMsgCompose::HackToGetBody(PRInt32 what)
{
  char *buffer = (char *) PR_CALLOC(16384);
  if (buffer)
  {
	nsFileSpec fileSpec = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    fileSpec += TEMP_MESSAGE_IN;
    
    nsInputFileStream fileStream(fileSpec);
    
    nsString msgBody = (what == 2 && !m_composeHTML) ? "--------Original Message--------\r\n"  : ""; 
    
    // skip RFC822 header
    while (!fileStream.eof() && !fileStream.failed() &&
      fileStream.is_open())
    {
      fileStream.readline(buffer, 1024);
      if (*buffer == 0)
        break;
    }
    // copy message body
    while (!fileStream.eof() && !fileStream.failed() &&
      fileStream.is_open())
    {
      fileStream.readline(buffer, 1024);
      if (what == 1 && ! m_composeHTML)
        msgBody += "> ";
      msgBody += buffer;
      msgBody += CRLF;
    }
    
    if (m_composeHTML)
    {
      nsString lowerMsgBody (msgBody);
      lowerMsgBody.ToLowerCase();
      
      PRInt32 startBodyOffset;
      PRInt32 endBodyOffset = -1;
      PRInt32 offset;
      startBodyOffset = lowerMsgBody.Find("<html>");
      if (startBodyOffset != -1)	//it's an HTML body
      {
        //Does it have a <body> tag?
        offset = lowerMsgBody.Find("<body");
        if (offset != -1)
        {
          offset = lowerMsgBody.FindChar('>', PR_FALSE,offset);
          if (offset != -1)
          {
            startBodyOffset = offset + 1;
        				endBodyOffset = lowerMsgBody.RFind("</body>");
          }
        }
        if (endBodyOffset == -1)
          endBodyOffset = lowerMsgBody.RFind("</html>");        			
      }
      
      if (startBodyOffset == -1)
        startBodyOffset = 0;
    		if (endBodyOffset == -1)
          endBodyOffset = lowerMsgBody.Length();
        
        msgBody.Insert(CRLF, endBodyOffset);
        if (startBodyOffset == 0)
        {
          msgBody.Insert("</html>", endBodyOffset);
          msgBody.Insert(CRLF, endBodyOffset);
        }
        msgBody.Insert("</blockquote>", endBodyOffset);
        msgBody.Insert(CRLF, endBodyOffset);
        
        msgBody.Insert(CRLF, startBodyOffset);
        msgBody.Insert("<blockquote TYPE=CITE>", startBodyOffset);
        msgBody.Insert(CRLF, startBodyOffset);
        if (startBodyOffset == 0)
        {
          msgBody.Insert("<html>", startBodyOffset);
          msgBody.Insert(CRLF, startBodyOffset);
          msgBody.Insert("<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">", startBodyOffset);
        }
    }
    else
    {
      //ducarroz: today, we are not converting HTML to plain text if needed!
    }
    
    // m_compFields->SetBody(msgBody.ToNewCString());
    // SetBody() strdup()'s cmsgBody.
    m_compFields->SetBody(nsAutoCString(msgBody));
    PR_Free(buffer);
  
    ProcessSignature(nsnull);
  }
}

//CleanUpRecipient will remove un-necesary "<>" when a recipient as an address without name
void nsMsgCompose::CleanUpRecipients(nsString& recipients)
{
//	TODO...
	PRInt16 i;
	PRBool startANewRecipient = PR_TRUE;
	PRBool removeBracket = PR_FALSE;
	nsAutoString newRecipient;
	PRUnichar aChar;

	for (i = 0; i < recipients.Length(); i ++)
	{
		aChar = recipients[i];
		switch (aChar)
		{
			case '<'	:
				if (startANewRecipient)
					removeBracket = PR_TRUE;
				else
					newRecipient += aChar;
				startANewRecipient = PR_FALSE;
				break;

			case '>'	:
				if (removeBracket)
					removeBracket = PR_FALSE;
				else
					newRecipient += aChar;
				break;

			case ' '	:
				newRecipient += aChar;
				break;

			case ','	:
				newRecipient += aChar;
				startANewRecipient = PR_TRUE;
				removeBracket = PR_FALSE;
				break;

			default		:
				newRecipient += aChar;
				startANewRecipient = PR_FALSE;
				break;
		}	
	}
	recipients = newRecipient;
}

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for both the send operation and the copy operation. 
// We have to create this class to listen for message send completion and deal with
// failures in both send and copy operations
////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_ADDREF(nsMsgComposeSendListener)
NS_IMPL_RELEASE(nsMsgComposeSendListener)

nsIMsgSendListener ** nsMsgComposeSendListener::CreateListenerArray()
{
  nsIMsgSendListener **tArray = (nsIMsgSendListener **)PR_Malloc(sizeof(nsIMsgSendListener *) * 2);
  if (!tArray)
    return nsnull;
  nsCRT::memset(tArray, 0, sizeof(nsIMsgSendListener *) * 2);
  tArray[0] = this;
  return tArray;
}

NS_IMETHODIMP 
nsMsgComposeSendListener::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER;
  *aInstancePtr = NULL;

  if (aIID.Equals(nsCOMTypeInfo<nsIMsgSendListener>::GetIID())) 
  {
	  *aInstancePtr = (nsIMsgSendListener *) this;                                                   
	  NS_ADDREF_THIS();
	  return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIMsgCopyServiceListener>::GetIID()))
  {
	  *aInstancePtr = (nsIMsgCopyServiceListener *) this;
	  NS_ADDREF_THIS();
	  return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsMsgComposeSendListener::nsMsgComposeSendListener(void) 
{ 
  mComposeObj = nsnull;
	mDeliverMode = 0;

  NS_INIT_REFCNT(); 
}

nsMsgComposeSendListener::~nsMsgComposeSendListener(void) 
{
}

nsresult nsMsgComposeSendListener::SetComposeObj(nsMsgCompose *obj)
{
	mComposeObj = obj;
	return NS_OK;
}

nsresult nsMsgComposeSendListener::SetDeliverMode(MSG_DeliverMode deliverMode)
{
	mDeliverMode = deliverMode;
	return NS_OK;
}

nsresult
nsMsgComposeSendListener::OnStartSending(const char *aMsgID, PRUint32 aMsgSize)
{
#ifdef NS_DEBUG
  printf("nsMsgComposeSendListener::OnStartSending()\n");
#endif
  return NS_OK;
}
  
nsresult
nsMsgComposeSendListener::OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
  printf("nsMsgComposeSendListener::OnProgress()\n");
#endif
  return NS_OK;
}

nsresult
nsMsgComposeSendListener::OnStatus(const char *aMsgID, const PRUnichar *aMsg)
{
#ifdef NS_DEBUG
  printf("nsMsgComposeSendListener::OnStatus()\n");
#endif

  return NS_OK;
}
  
nsresult nsMsgComposeSendListener::OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
                                     nsIFileSpec *returnFileSpec)
{
	nsresult rv = NS_OK;

	if (mComposeObj)
	{
		if (NS_SUCCEEDED(aStatus))
		{
#ifdef NS_DEBUG
			printf("nsMsgComposeSendListener: Success on the message send operation!\n");
#endif
      nsIMsgCompFields *compFields = nsnull;
      mComposeObj->GetCompFields(&compFields); //GetCompFields will addref, you need to release when your are done with it

			// Close the window ONLY if we are not going to do a save operation
      PRUnichar *fieldsFCC = nsnull;
      if (NS_SUCCEEDED(compFields->GetFcc(&fieldsFCC)))
      {
        if (fieldsFCC && *fieldsFCC)
        {
			if (nsCRT::strcasecmp(fieldsFCC, "nocopy://") == 0)
            mComposeObj->CloseWindow();
        }
      }
      else
        mComposeObj->CloseWindow();  // if we fail on the simple GetFcc call, close the window to be safe and avoid
                                     // windows hanging around to prevent the app from exiting.

      NS_IF_RELEASE(compFields);
		}
		else
		{
#ifdef NS_DEBUG
			printf("nsMsgComposeSendListener: the message send operation failed!\n");
#endif
			mComposeObj->ShowWindow(PR_TRUE);
		}
	}

  return rv;
}

nsresult
nsMsgComposeSendListener::OnStartCopy()
{
#ifdef NS_DEBUG
  printf("nsMsgComposeSendListener::OnStartCopy()\n");
#endif

  return NS_OK;
}

nsresult
nsMsgComposeSendListener::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
  printf("nsMsgComposeSendListener::OnProgress() - COPY\n");
#endif
  return NS_OK;
}
  
nsresult
nsMsgComposeSendListener::OnStopCopy(nsresult aStatus)
{
	nsresult rv = NS_OK;

	if (mComposeObj)
	{
    // Ok, if we are here, we are done with the send/copy operation so
    // we have to do something with the window....SHOW if failed, Close
    // if succeeded
		if (NS_SUCCEEDED(aStatus))
		{
#ifdef NS_DEBUG
			printf("nsMsgComposeSendListener: Success on the message copy operation!\n");
#endif
      // We should only close the window if we are done. Things like templates
      // and drafts aren't done so their windows should stay open
      if ( (mDeliverMode != nsMsgSaveAsDraft) && (mDeliverMode != nsMsgSaveAsTemplate) )
        mComposeObj->CloseWindow();
		}
		else
		{
#ifdef NS_DEBUG
			printf("nsMsgComposeSendListener: the message copy operation failed!\n");
#endif
			mComposeObj->ShowWindow(PR_TRUE);
		}
	}

  return rv;
}

nsresult
nsMsgComposeSendListener::SetMessageKey(PRUint32 aMessageKey)
{
	return NS_OK;
}

nsresult
nsMsgComposeSendListener::GetMessageId(nsCString* aMessageId)
{
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// This is a class that will allow us to listen to state changes in the Ender 
// compose window. This is important since we must wait until we are told Ender
// is ready before we do various quoting operations
////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsMsgDocumentStateListener, nsCOMTypeInfo<nsIDocumentStateListener>::GetIID());

nsMsgDocumentStateListener::nsMsgDocumentStateListener(void)
{
  NS_INIT_REFCNT();
  mComposeObj = nsnull;
}

nsMsgDocumentStateListener::~nsMsgDocumentStateListener(void)
{
}

void        
nsMsgDocumentStateListener::SetComposeObj(nsMsgCompose *obj)
{
  mComposeObj = obj;
}

nsresult
nsMsgDocumentStateListener::NotifyDocumentCreated(void)
{
  // Ok, now the document has been loaded, so we are ready to let
  // the user run hog wild with editing...but first, lets cleanup
  // any temp files
  //
  if (mComposeObj->mSigFileSpec)
  {
    mComposeObj->mSigFileSpec->Delete(PR_FALSE);
    delete mComposeObj->mSigFileSpec;
    mComposeObj->mSigFileSpec = nsnull;
  }

  if (mComposeObj->mTempComposeFileSpec)
  {
    mComposeObj->mTempComposeFileSpec->Delete(PR_FALSE);
    delete mComposeObj->mTempComposeFileSpec;
    mComposeObj->mTempComposeFileSpec = nsnull;
  }

  // RICHIE - hack! This is only if we are using the old
  // quoting hack
  if (mComposeObj->mUseOldQuotingHack)
  {
    PRUnichar   *bod;

    nsIMsgCompFields *compFields;
    mComposeObj->GetCompFields(&compFields);
    compFields->GetBody(&bod);
    mComposeObj->LoadAsQuote(nsString(bod));
    PR_FREEIF(bod);
  }
  // RICHIE - hack! This is only if we are using the old
  // quoting hack

  return NS_OK;
}

nsresult
nsMsgDocumentStateListener::NotifyDocumentWillBeDestroyed(void)
{
  return NS_OK;
}

nsresult
nsMsgDocumentStateListener::NotifyDocumentStateChanged(PRBool nowDirty)
{
  return NS_OK;
}

nsresult
nsMsgCompose::ConvertHTMLToText(char *aSigFile, nsString &aSigData)
{
  nsresult    rv;
  nsString    origBuf;

  rv = LoadDataFromFile(aSigFile, origBuf);
  if (NS_FAILED(rv))
    return rv;

  ConvertBufToPlainText(origBuf, nsnull);
  aSigData = origBuf;
  return NS_OK;
}

nsresult
nsMsgCompose::ConvertTextToHTML(char *aSigFile, nsString &aSigData)
{
  nsresult    rv;
  nsString    origBuf;

  rv = LoadDataFromFile(aSigFile, origBuf);
  if (NS_FAILED(rv))
    return rv;

  aSigData = "<pre>";
  aSigData.Append(origBuf);
  aSigData.Append("</pre>");
  return NS_OK;
}

nsresult
nsMsgCompose::LoadDataFromFile(char *sigFilePath, nsString &sigData)
{
  nsFileSpec    fSpec(sigFilePath);
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

  tempFile.read(readBuf, readSize);
  tempFile.close();

  sigData = readBuf;
  PR_FREEIF(readBuf);
  return NS_OK;
}

nsresult
nsMsgCompose::BuildQuotedMessageAndSignature(void)
{
  // 
  // This should never happen...if it does, just bail out...
  //
  if (!m_editor)
    return NS_ERROR_FAILURE;

  // We will fire off the quote operation and wait for it to
  // finish before we actually do anything with Ender...
  return QuoteOriginalMessage(mQuoteURI.GetUnicode(), mWhatHolder);
}

//
// This will process the signature file for the user. If aAppendFile
// is passed in, then this will be treated as an operation following
// a quoting operation and the specified signature will be appended
// to the file passed in. If this argument is null, then this is a clean
// compose window and we should just create whatever we need to create 
// as a temp file and do a LoadURL operation.
//
nsresult
nsMsgCompose::ProcessSignature(nsOutputFileStream *aAppendFileStream)
{
  nsresult    rv;

  // 
  // This should never happen...if it does, just bail out...
  //
  if (!m_editor)
    return NS_ERROR_FAILURE;

  // Now, we can get sort of fancy. This is the time we need to check
  // for all sorts of user defined stuff, like signatures and editor
  // types and the like!
  //
  //    user_pref("mail.signature_file", "y:\\sig.html");
  //    user_pref("mail.use_signature_file", true);
  //
  // Note: We will have intelligent signature behavior in that we
  // look at the signature file first...if the extension is .htm or 
  // .html, we assume its HTML, otherwise, we assume it is plain text
  //
  nsString      urlStr;
  char          *sigFilePath = nsnull;
  PRBool        useSigFile = PR_FALSE;
  PRBool        htmlSig = PR_FALSE;
  nsString      sigData = "";

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs) 
  {
    rv = prefs->GetBoolPref("mail.use_signature_file", &useSigFile);
    if (NS_SUCCEEDED(rv) && useSigFile) 
    {
      rv = prefs->CopyCharPref("mail.signature_file", &sigFilePath);
    }
  }
  
  // Setup the urlStr to load to be the signature if the user wants it 
  // that way...
  //
  if ((useSigFile) && (sigFilePath))
  {
    nsFileSpec    testSpec(sigFilePath);
    
    if (!testSpec.Exists())
    {
      PR_FREEIF(sigFilePath);
      sigFilePath = nsnull;
      if (m_composeHTML)
        urlStr = "chrome://messengercompose/content/defaultHtmlBody.html";
      else
        urlStr = "chrome://messengercompose/content/defaultTextBody.txt";
    }
    else
    {
      // Once we get here, we need to figure out if we have the correct file
      // type for the editor.
      //
      char    *fileExt = nsMsgGetExtensionFromFileURL(nsString(sigFilePath));      

      if ( (fileExt) && (*fileExt) )
      {
        htmlSig = ( (!PL_strcasecmp(fileExt, "HTM")) || (!PL_strcasecmp(fileExt, "HTML")) );
        PR_FREEIF(fileExt);
      }

      // is this a text sig with an HTML editor?
      if ( (m_composeHTML) && (!htmlSig) )
        ConvertTextToHTML(sigFilePath, sigData);
      // is this a HTML sig with a text window?
      else if ( (!m_composeHTML) && (htmlSig) )
        ConvertHTMLToText(sigFilePath, sigData);
      else // We have a match...
      {
        if (!aAppendFileStream)   // Just load this URL
          urlStr = nsMsgPlatformFileToURL(testSpec);
        else
          LoadDataFromFile(sigFilePath, sigData);  // Get the data!
      }
    }
  }
  else
  {
    if (m_composeHTML)
      urlStr = "chrome://messengercompose/content/defaultHtmlBody.html";
    else
      urlStr = "chrome://messengercompose/content/defaultTextBody.txt";
  }

  PR_FREEIF(sigFilePath);

  // This is the "load signature alone" operation!
  char      *htmlBreak = "<BR>";
  char      *dashes = "--";
  if (!aAppendFileStream)
  {
    if ( (sigData == "") && (urlStr != "") ) // just load URL
    {
      m_editor->LoadUrl(urlStr.GetUnicode());
    }
    else   // Have to write data to a temp file then load the URL...
    {
      mSigFileSpec = nsMsgCreateTempFileSpec(m_composeHTML ? (char *)"sig.html" : (char *)"sig.txt");
      if (!mSigFileSpec)
        return NS_ERROR_FAILURE;

      nsOutputFileStream tempFile(*mSigFileSpec);
      if (!tempFile.is_open())
        return NS_MSG_ERROR_WRITING_FILE;        

      if (m_composeHTML)
        tempFile.write(htmlBreak, PL_strlen(htmlBreak));
      else
      {
        tempFile.write(CRLF, 2);
        tempFile.write(dashes, PL_strlen(dashes));
      }
      if (m_composeHTML)
        tempFile.write(htmlBreak, PL_strlen(htmlBreak));
      else
        tempFile.write(CRLF, 2);

      tempFile.write(nsAutoCString(sigData), sigData.Length());
      tempFile.close();
      urlStr = mSigFileSpec->GetNativePathCString();
      m_editor->LoadUrl(urlStr.GetUnicode());
    }
  }
  else    // This is where we are appending to the current file...
  {
    if (sigData != "")
    {
      if (!aAppendFileStream->is_open())
        return NS_MSG_ERROR_WRITING_FILE;

      if (m_composeHTML)
        aAppendFileStream->write(htmlBreak, PL_strlen(htmlBreak));
      else
        aAppendFileStream->write(CRLF, 2);
    
      aAppendFileStream->write(dashes, PL_strlen(dashes));
      if (m_composeHTML)
        aAppendFileStream->write(htmlBreak, PL_strlen(htmlBreak));
      else
        aAppendFileStream->write(CRLF, 2);
    
      aAppendFileStream->write(nsAutoCString(sigData), sigData.Length());
    }
    // else // We are punting on putting a default signature on a quoted message!
  }
  
  return NS_OK;
}

nsresult
nsMsgCompose::BuildBodyMessage()
{
  PRUnichar   *bod;

  // 
  // This should never happen...if it does, just bail out...
  //
  if (!m_editor)
    return NS_ERROR_FAILURE;

  // Since we have a body in the comp fields, we need to create a 
  // body of this old body...
  //  
  mTempComposeFileSpec = nsMsgCreateTempFileSpec(m_composeHTML ? (char *)"nscomp.html" : (char *)"nscomp.txt");
  if (!mTempComposeFileSpec)
    return NS_MSG_ERROR_WRITING_FILE;
  
  nsOutputFileStream tempFile(*(mTempComposeFileSpec));
  if (!tempFile.is_open())
    return NS_MSG_ERROR_WRITING_FILE;
 
  m_compFields->GetBody(&bod);

  nsString     tempBody(bod);
  tempFile.write(nsAutoCString(tempBody), tempBody.Length());
  tempFile.close();

  //
  // Now load the URL...
  //  
  nsString urlStr = nsMsgPlatformFileToURL(*mTempComposeFileSpec);
  m_editor->LoadUrl(urlStr.GetUnicode());
  return NS_OK;
}
