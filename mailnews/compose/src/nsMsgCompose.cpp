/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsMsgCompose.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMDocument.h"
#include "nsMsgI18N.h"
#include "nsMsgCompCID.h"
#include "nsMsgSend.h"
#include "nsIMessenger.h"	//temporary!
#include "nsIMessage.h"		//temporary!
#include "nsMsgQuote.h"
#include "nsIPref.h"
#include "nsIDocumentEncoder.h"    // for editor output flags
#include "nsXPIDLString.h"
#include "nsIMsgHeaderParser.h"
#include "nsMsgCompUtils.h"
#include "nsMsgComposeStringBundle.h"
#include "nsSpecialSystemDirectory.h"
#include "nsMsgSend.h"
#include "nsMsgCreate.h"
#include "nsMailHeaders.h"
#include "nsMsgPrompts.h"
#include "nsMimeTypes.h"
#include "nsICharsetConverterManager.h"
#include "nsTextFormater.h"
#include "nsIEditor.h"

// XXX temporary so we can use the current identity hack -alecf
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

// Defines....
static NS_DEFINE_CID(kMsgQuoteCID, NS_MSGQUOTE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

static PRInt32 GetReplyOnTop()
{
	PRInt32 reply_on_top = 1;
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  	if (NS_SUCCEEDED(rv))
		prefs->GetIntPref("mailnews.reply_on_top", &reply_on_top);
	return reply_on_top;
}

static nsresult RemoveDuplicateAddresses(const char * addresses, const char * anothersAddresses, PRBool removeAliasesToMe, char** newAddress)
{
	nsresult rv;

	nsCOMPtr<nsIMsgHeaderParser> parser;
	nsComponentManager::CreateInstance(kHeaderParserCID,
	                           nsnull,
	                           nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(),
	                           getter_AddRefs(parser));
	if (parser)
		rv= parser->RemoveDuplicateAddresses(msgCompHeaderInternalCharset(), addresses, anothersAddresses, removeAliasesToMe, newAddress);
	else
		rv = NS_ERROR_FAILURE;

	return rv;
}


nsMsgCompose::nsMsgCompose()
{
	NS_INIT_REFCNT();

	mQuotingToFollow = PR_FALSE;
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
	mType = nsIMsgCompType::New;

	// Get the default charset from pref, use this as a mail charset.
	char * default_mail_charset = nsMsgI18NGetDefaultMailCharset();
	if (default_mail_charset)
	{
   		m_compFields->SetCharacterSet(default_mail_charset);
    	PR_Free(default_mail_charset);
  	}

	m_composeHTML = PR_FALSE;
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

//
// Once we are here, convert the data which we know to be UTF-8 to UTF-16
// for insertion into the editor
//
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

nsresult
TranslateLineEndings(nsString &aString)
{
  PRUnichar   *transBuf = nsnull;

  // First, do sanity checking...if aString doesn't have
  // any CR's, then there is no reason to call this rest
  // of this 
  if (aString.FindChar(CR) < 0)
    return NS_OK;
  
  transBuf = aString.ToNewUnicode();
  if (transBuf)
  {
    DoLineEndingConJobUnicode(transBuf, aString.Length());
    aString.SetString(transBuf);
    PR_FREEIF(transBuf);
  }

  return NS_OK;
}

nsresult nsMsgCompose::ConvertAndLoadComposeWindow(nsIEditorShell *aEditorShell, nsString aPrefix, nsString aBuf, 
                                                   nsString aSignature, PRBool aQuoted, PRBool aHTMLEditor)
{
  // First, get the nsIEditor interface for future use
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));

  TranslateLineEndings(aPrefix);
  TranslateLineEndings(aBuf);
  TranslateLineEndings(aSignature);

  if (editor)
    editor->EnableUndo(PR_FALSE);

  // Now, insert it into the editor...
  if ( (aQuoted) )
  {
    if (aPrefix != "")
    {
      if (aHTMLEditor)
        aEditorShell->InsertSource(aPrefix.GetUnicode());
      else
        aEditorShell->InsertText(aPrefix.GetUnicode());
    }

    if (aBuf != "")
      aEditorShell->InsertAsQuotation(aBuf.GetUnicode(), 0);

    if (aSignature != "")
    {
      aEditorShell->InsertSource(aSignature.GetUnicode());
    }
  }
  else
  {
    if (aHTMLEditor)
    {
      if (aBuf != "")
        aEditorShell->InsertSource(aBuf.GetUnicode());
      if (aSignature != "")
        aEditorShell->InsertSource(aSignature.GetUnicode());
    }
    else
    {
      if (aBuf != "")
        aEditorShell->InsertText(aBuf.GetUnicode());
      if (aSignature != "")
        aEditorShell->InsertText(aSignature.GetUnicode());
    }
  }
  
  if (editor)
  {
	  switch (GetReplyOnTop())
	  {
      // RICHIE SHERRY - have to save where the cursor was!
      // This should set the cursor after the body but before the sig
		  case 0	: editor->EndOfDocument();					break;

		  case 2	: editor->SelectAll();						  break;

      // This should set the cursor to the top!
      default	: editor->BeginningOfDocument();		break;
	  }
  }

  NotifyStateListeners(nsMsgCompose::eComposeFieldsReady);
  if (editor)
    editor->EnableUndo(PR_TRUE);

  return NS_OK;
}

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

nsresult nsMsgCompose::Initialize(nsIDOMWindow *aWindow,
                                  const PRUnichar *originalMsgURI,
                                  MSG_ComposeType type,
                                  MSG_ComposeFormat format,
                                  nsIMsgCompFields *compFields,
                                  nsIMsgIdentity *identity)
{
	nsresult rv = NS_OK;

  m_identity = identity;
  
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
		case nsIMsgCompFormat::HTML		: m_composeHTML = PR_TRUE;					break;
		case nsIMsgCompFormat::PlainText	: m_composeHTML = PR_FALSE;					break;
    default							:
      /* ask the identity which compose to use */
      if (m_identity) m_identity->GetComposeHtml(&m_composeHTML);
      break;

	}

	 CreateMessage(originalMsgURI, type, format, compFields);

	return rv;
}

nsresult nsMsgCompose::SetDocumentCharset(const PRUnichar *charset) 
{
	// Set charset, this will be used for the MIME charset labeling.
	m_compFields->SetCharacterSet(nsAutoCString(charset));
	
	return NS_OK;
}

nsresult nsMsgCompose::RegisterStateListener(nsIMsgComposeStateListener *stateListener)
{
  nsresult rv = NS_OK;
  
  if (!stateListener)
    return NS_ERROR_NULL_POINTER;
  
  if (!mStateListeners)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(mStateListeners));
    if (NS_FAILED(rv)) return rv;
  }
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(stateListener, &rv);
  if (NS_FAILED(rv)) return rv;

  // note that this return value is really a PRBool, so be sure to use
  // NS_SUCCEEDED or NS_FAILED to check it.
  return mStateListeners->AppendElement(iSupports);
}

nsresult nsMsgCompose::UnregisterStateListener(nsIMsgComposeStateListener *stateListener)
{
  if (!stateListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  
  // otherwise, see if it exists in our list
  if (!mStateListeners)
    return (nsresult)PR_FALSE;      // yeah, this sucks, but I'm emulating the behaviour of
                                    // nsISupportsArray::RemoveElement()

  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(stateListener, &rv);
  if (NS_FAILED(rv)) return rv;

  // note that this return value is really a PRBool, so be sure to use
  // NS_SUCCEEDED or NS_FAILED to check it.
  return mStateListeners->RemoveElement(iSupports);
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
      
      PR_Free(tArray);
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

	if (m_editor && m_compFields && !m_composeHTML)
	{
    const char contentType[] = "text/plain";
		nsAutoString msgBody;
		PRUnichar *bodyText = NULL;
    nsString format(contentType);
    PRUint32 flags = nsIDocumentEncoder::OutputFormatted;

    nsresult rv2;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv2);
    if (NS_SUCCEEDED(rv2)) {
      PRBool sendflowed;
      rv2=prefs->GetBoolPref("mailnews.send_plaintext_flowed", &sendflowed);
      if(!(NS_SUCCEEDED(rv2) && !sendflowed))
        // Unless explicitly forbidden...
        flags |= nsIDocumentEncoder::OutputFormatFlowed;
    }
    
    rv = m_editor->GetContentsAs(format.GetUnicode(), flags, &bodyText);
		
    if (NS_SUCCEEDED(rv) && NULL != bodyText)
    {
		  msgBody = bodyText;
      nsAllocator::Free(bodyText);

		  // Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		  char *outCString = NULL;
      rv = nsMsgI18NSaveAsCharset(contentType, m_compFields->GetCharacterSet(), 
                                  msgBody.GetUnicode(), &outCString);
		  if (NS_SUCCEEDED(rv) && NULL != outCString) 
		  {
			  m_compFields->SetBody(outCString);
			  PR_Free(outCString);
		  }
		  else
			  m_compFields->SetBody(nsAutoCString(msgBody));
    }
	}

  rv = _SendMsg(deliverMode, identity, callback);
	if (NS_FAILED(rv))
	{
		ShowWindow(PR_TRUE);
    	if (rv != NS_ERROR_BUT_DONT_SHOW_ALERT)
			nsMsgDisplayMessageByID(rv);
	}
	
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

	if (NS_FAILED(rv))
	{
		ShowWindow(PR_TRUE);
    	if (rv != NS_ERROR_BUT_DONT_SHOW_ALERT)
			nsMsgDisplayMessageByID(rv);
	}
	return rv;
}

nsresult nsMsgCompose::CloseWindow()
{
	if (m_webShellWin)
	{
    m_editor = nsnull;	      /* m_editor will be destroyed during the Close Window. Set it to null to */
							                /* be sure we wont use it anymore. */
		nsIWebShellWindow *saveWin = m_webShellWin;
		m_webShellWin = nsnull;
		saveWin->Close();
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

  // Now, lets init the editor here!
  // Just get a blank editor started...
  m_editor->LoadUrl(nsString("about:blank").GetUnicode());

  return NS_OK;
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
  
  return prefs->GetIntPref("mailnews.wraplength", aWrapLength);
}

nsresult nsMsgCompose::CreateMessage(const PRUnichar * originalMsgURI,
                                     MSG_ComposeType type,
                                     MSG_ComposeFormat format,
                                     nsIMsgCompFields * compFields)
{
  nsresult rv = NS_OK;

  if (compFields)
  {
  	  if (m_compFields)
        m_compFields->Copy(compFields);
      return rv;
  }

    /* In case of forwarding multiple messages, originalMsgURI will contains several URI separated by a comma. */
    /* we need to extract only the first URI*/
    nsString  firstURI(originalMsgURI);
    PRInt32 offset = firstURI.FindChar(',');
    if (offset >= 0)
    	firstURI.Truncate(offset);
    
    nsCOMPtr<nsIMessage> message = getter_AddRefs(GetIMessageFromURI(firstURI.GetUnicode()));
    if ((NS_SUCCEEDED(rv)) && message)
    {
      nsString aString = "";
      nsString bString = "";
      nsString aCharset = "";
      nsString decodedString;
      nsString encodedCharset;  // we don't use this
      char *aCString = nsnull;
    
      rv = message->GetCharSet(&aCharset);
      if (NS_FAILED(rv)) return rv;
      rv = message->GetSubject(&aString);
      if (NS_FAILED(rv)) return rv;
      
      mType = type;
      switch (type)
      {
      default: break;        
      case nsIMsgCompType::Reply : 
      case nsIMsgCompType::ReplyAll:
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
        
          rv = message->GetAuthor(&aString);		
          if (NS_FAILED(rv)) return rv;
          m_compFields->SetTo(nsAutoCString(aString));
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(aString, encodedCharset, decodedString)))
            if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
            {
              m_compFields->SetTo(aCString);
              PR_Free(aCString);
            }
          
            if (type == nsIMsgCompType::ReplyAll)
            {
              nsString cString, dString;
              rv = message->GetRecipients(&cString);
	      if (NS_FAILED(rv)) return rv; 
              CleanUpRecipients(cString);
              rv = message->GetCCList(&dString);
	      if (NS_FAILED(rv)) return rv; 
              CleanUpRecipients(dString);
              if (cString.Length() > 0 && dString.Length() > 0)
                cString = cString + ", ";
              cString = cString + dString;
              m_compFields->SetCc(nsAutoCString(cString));
              
              if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(cString, encodedCharset, decodedString)))
                if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
                {
					char * resultStr = nsnull;
					nsCString addressToBeRemoved = m_compFields->GetTo();
					  	
		            if (m_identity)
		            {
		                nsXPIDLCString email;
		                m_identity->GetEmail(getter_Copies(email));
		                addressToBeRemoved += ", ";
		                addressToBeRemoved += NS_CONST_CAST(char*, (const char *)email);
					}
		      		rv= RemoveDuplicateAddresses(aCString, (char *)addressToBeRemoved, PR_TRUE, &resultStr);
		      		if (NS_SUCCEEDED(rv))
					{
		                PR_Free(aCString);
		                aCString = resultStr;
		            }

                  	m_compFields->SetCc(aCString);
                    PR_Free(aCString);
                }
            }
          
            // Setup quoting callbacks for later...
            mWhatHolder = 1;
            mQuoteURI = originalMsgURI;
          
            break;
        }
      case nsIMsgCompType::ForwardAsAttachment:
      case nsIMsgCompType::ForwardInline:
        {
        
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
          if (type == nsIMsgCompType::ForwardAsAttachment)
          {
          	mQuotingToFollow = PR_FALSE;	//We don't need to quote the original message.
          	m_compFields->SetAttachments(originalMsgURI);
          }
          else
          {
          	mQuotingToFollow = PR_TRUE;
            mWhatHolder = 2;
			mQuoteURI = originalMsgURI;
          }
        
          break;
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

QuotingOutputStreamListener::QuotingOutputStreamListener(const PRUnichar * originalMsgURI,
                                                         PRBool quoteHeaders,
                                                         nsIMsgIdentity *identity) 
{ 
  mComposeObj = nsnull;
  mQuoteHeaders = quoteHeaders;
  mIdentity = identity;

  // For the built message body...
  mMsgBody = "";
  mCitePrefix = "";
  mSignature = "";

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
      {
        nsString aCharset(msgCompHeaderInternalCharset());
        char * utf8Author = nsnull;
        rv = ConvertFromUnicode(aCharset, author, &utf8Author);
        if (NS_SUCCEEDED(rv) && utf8Author)
        {
          rv = parser->ExtractHeaderAddressName(nsAutoCString(aCharset), utf8Author, &authorName);
          if (NS_SUCCEEDED(rv))
            rv = ConvertToUnicode(aCharset, authorName, author);
        }
        
        if (!utf8Author || NS_FAILED(rv))
        {
          rv = parser->ExtractHeaderAddressName(nsnull, nsAutoCString(author), &authorName);
          if (NS_SUCCEEDED(rv))
            author = authorName;
        }
        if (NS_SUCCEEDED(rv))
        {
          if (GetReplyOnTop() == 1)
            mCitePrefix.Append("<br><br>");

          mCitePrefix.Append(author);
          mCitePrefix.Append(" wrote:<br><html>");
        }
        if (authorName)
          PL_strfree(authorName);
        PR_FREEIF(utf8Author);
      }
    }
  }
  
  if (mCitePrefix.IsEmpty())
    mCitePrefix.Append("<br><br>--- Original Message ---<br><html>");
  
  NS_INIT_REFCNT(); 
}

nsresult
QuotingOutputStreamListener::ConvertToPlainText()
{
nsresult  rv = NS_OK;

  rv += ConvertBufToPlainText(mCitePrefix, "UTF-8");
  rv += ConvertBufToPlainText(mMsgBody, "UTF-8");
  rv += ConvertBufToPlainText(mSignature, "UTF-8");
  return rv;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStartRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */)
{
	return NS_OK;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStopRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */, nsresult status, const PRUnichar * /* errorMsg */)
{
  nsresult rv = NS_OK;
  
  if (mComposeObj) 
  {
    MSG_ComposeType type = mComposeObj->GetMessageType();
    
    if (mHeaders && (type == nsIMsgCompType::Reply || type == nsIMsgCompType::ReplyAll))
    {
      nsIMsgCompFields *compFields = nsnull;
      mComposeObj->GetCompFields(&compFields); //GetCompFields will addref, you need to release when your are done with it
      if (compFields)
      {
        nsString aCharset(msgCompHeaderInternalCharset());
	      	nsString replyTo;
          nsString newgroups;
          nsString followUpTo;
          char *outCString = nsnull;
          PRUnichar emptyUnichar = 0;
          PRBool toChanged = PR_FALSE;
          
          mHeaders->ExtractHeader(HEADER_REPLY_TO, PR_FALSE, &outCString);
          if (outCString)
          {
            // Convert fields to UTF-8
            ConvertToUnicode(aCharset, outCString, replyTo);
            PR_FREEIF(outCString);
          }
          
          mHeaders->ExtractHeader(HEADER_NEWSGROUPS, PR_FALSE, &outCString);
          if (outCString)
          {
            // Convert fields to UTF-8
            ConvertToUnicode(aCharset, outCString, newgroups);
            PR_FREEIF(outCString);
          }
          
          mHeaders->ExtractHeader(HEADER_FOLLOWUP_TO, PR_FALSE, &outCString);
          if (outCString)
          {
            // Convert fields to UTF-8
            ConvertToUnicode(aCharset, outCString, followUpTo);
            PR_FREEIF(outCString);
          }
          
          if (! replyTo.IsEmpty())
          {
            compFields->SetTo(replyTo.GetUnicode());
            toChanged = PR_TRUE;
          }
          
          if (! newgroups.IsEmpty())
          {
            compFields->SetNewsgroups(newgroups.GetUnicode());
            if (type == nsIMsgCompType::Reply)
              compFields->SetTo(&emptyUnichar);
          }
          
          if (! followUpTo.IsEmpty())
          {
            compFields->SetNewsgroups(followUpTo.GetUnicode());
            if (type == nsIMsgCompType::Reply)
              compFields->SetTo(&emptyUnichar);
          }
          
          if (toChanged)
          {
          	//Remove duplicate addresses between TO && CC
          	char * resultStr;
          	nsMsgCompFields* _compFields = (nsMsgCompFields*)compFields;
  			if (NS_SUCCEEDED(rv))
  			{
			    rv= RemoveDuplicateAddresses(_compFields->GetCc(), _compFields->GetTo(), PR_FALSE, &resultStr);
	          	if (NS_SUCCEEDED(rv))
	          	{
	          		_compFields->SetCc(resultStr);
	          		PR_Free(resultStr);
	          	}
			}
          }
       
          NS_RELEASE(compFields);
      }
    }
    
    mMsgBody += "</html>";
    
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
    
    mComposeObj->ProcessSignature(mIdentity, &mSignature);
    
    nsIEditorShell *editor = nsnull;
    if (NS_SUCCEEDED(mComposeObj->GetEditor(&editor)) && editor)
    {
      mComposeObj->ConvertAndLoadComposeWindow(editor, mCitePrefix, mMsgBody, mSignature, PR_TRUE, compHTML);
    }
  }
    
  NS_IF_RELEASE(mComposeObj);	//We are done with it, therefore release it.
  return rv;
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
	if (rv == NS_BASE_STREAM_WOULD_BLOCK)
		rv = NS_OK;
	newBuf[numWritten] = '\0';
	if (NS_SUCCEEDED(rv) && numWritten > 0)
	{
    PRUnichar       *u = nsnull; 
    nsAutoString    fmt("%s");

    u = nsTextFormater::smprintf(fmt.GetUnicode(), newBuf); // this converts UTF-8 to UCS-2 
    if (u)
    {
      mMsgBody.Append(u);
      PR_FREEIF(u);
    }
    else
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

nsresult
QuotingOutputStreamListener::SetMimeHeaders(nsIMimeHeaders * headers)
{
  mHeaders = headers;
  return NS_OK;
}


NS_IMPL_ISUPPORTS(QuotingOutputStreamListener, nsCOMTypeInfo<nsIStreamListener>::GetIID());
////////////////////////////////////////////////////////////////////////////////////
// END OF QUOTING LISTENER
////////////////////////////////////////////////////////////////////////////////////

MSG_ComposeType nsMsgCompose::GetMessageType()
{
	return mType;
}

nsresult
nsMsgCompose::QuoteOriginalMessage(const PRUnichar *originalMsgURI, PRInt32 what) // New template
{
  nsresult    rv;

  mQuotingToFollow = PR_FALSE;

  nsString    tmpURI(originalMsgURI);
  
  // Create a mime parser (nsIStreamConverter)!
  rv = nsComponentManager::CreateInstance(kMsgQuoteCID, 
                                          NULL, nsCOMTypeInfo<nsIMsgQuote>::GetIID(), 
                                          (void **) getter_AddRefs(mQuote)); 
  if (NS_FAILED(rv) || !mQuote)
    return NS_ERROR_FAILURE;

  // Create the consumer output stream.. this will receive all the HTML from libmime
  mQuoteStreamListener =
    new QuotingOutputStreamListener(originalMsgURI, what != 1, m_identity);
  
  if (!mQuoteStreamListener)
  {
#ifdef NS_DEBUG
    printf("Failed to create mQuoteStreamListener\n");
#endif
    return NS_ERROR_FAILURE;
  }
  NS_ADDREF(mQuoteStreamListener);

  NS_ADDREF(this);
  mQuoteStreamListener->SetComposeObj(this);

  rv = mQuote->QuoteMessage(originalMsgURI, what != 1, mQuoteStreamListener);
  return rv;
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
  // Ok, now the document has been loaded, so we are ready to setup
  // the compose window and let the user run hog wild!

  //
  // Have to check to see if there is a body stored in the 
  // comp fields...
  //
  PRBool            bodyInCompFields = PR_FALSE;
  nsMsgCompFields   *compFields = nsnull; 
  nsIMsgIdentity    *identity = nsnull;
  PRBool            compHTML = PR_FALSE;
	nsIEditorShell    *editor = nsnull;

  mComposeObj->GetCompFields(&compFields);
  mComposeObj->GetIdentity(&identity);
  mComposeObj->GetComposeHTML(&compHTML);
  mComposeObj->GetEditor(&editor);

  if (compFields)
  {
    PRUnichar     *bod;

    compFields->GetBody(&bod);
    if ((bod) && (*bod))
      bodyInCompFields = PR_TRUE;
  }

  // Now, do the appropriate startup operation...signature only
  // or quoted message and signature...
  if ( mComposeObj->QuotingToFollow() )
    return mComposeObj->BuildQuotedMessageAndSignature();
  else if (bodyInCompFields)
    return mComposeObj->BuildBodyMessage();
  else
  {
    nsresult    rv;
    nsString    tSignature = "";

    rv = mComposeObj->ProcessSignature(identity, &tSignature);
    if ((NS_SUCCEEDED(rv)) && editor)
    {
      mComposeObj->ConvertAndLoadComposeWindow(editor, "", "", tSignature, PR_FALSE, compHTML);
    }

    return rv;
  }
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
nsMsgCompose::ConvertHTMLToText(nsFileSpec& aSigFile, nsString &aSigData)
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
nsMsgCompose::ConvertTextToHTML(nsFileSpec& aSigFile, nsString &aSigData)
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
nsMsgCompose::LoadDataFromFile(nsFileSpec& fSpec, nsString &sigData)
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
// This will process the signature file for the user. This method
// will always append the results to the mMsgBody member variable.
//
nsresult
nsMsgCompose::ProcessSignature(nsIMsgIdentity *identity, nsString *aMsgBody)
{
  nsresult    rv;

  // Now, we can get sort of fancy. This is the time we need to check
  // for all sorts of user defined stuff, like signatures and editor
  // types and the like!
  //
  //    user_pref(".....signature_file", "y:\\sig.html");
  //    user_pref(".....use_signature_file", true);
  //
  // Note: We will have intelligent signature behavior in that we
  // look at the signature file first...if the extension is .htm or 
  // .html, we assume its HTML, otherwise, we assume it is plain text
  //
  nsString      urlStr;
  nsCOMPtr<nsIFileSpec> sigFileSpec;
  PRBool        useSigFile = PR_FALSE;
  PRBool        htmlSig = PR_FALSE;
  nsString      sigData = "";

  if (identity)
  {
    rv = identity->GetAttachSignature(&useSigFile);
    if (NS_SUCCEEDED(rv) && useSigFile) 
    {
      identity->GetSignature(getter_AddRefs(sigFileSpec));
    }
  }
  
  // Now, if they didn't even want to use a signature, we should
  // just return nicely.
  //
  if ((!useSigFile) || (!sigFileSpec))
    return NS_OK;

  nsFileSpec    testSpec;
  sigFileSpec->GetFileSpec(&testSpec);
  
  // If this file doesn't really exist, just bail!
  if (!testSpec.Exists())
    return NS_OK;

  // Once we get here, we need to figure out if we have the correct file
  // type for the editor.
  //
  nsFileURL sigFilePath(testSpec);
  char    *fileExt = nsMsgGetExtensionFromFileURL(nsString(sigFilePath));
  
  if ( (fileExt) && (*fileExt) )
  {
    htmlSig = ( (!PL_strcasecmp(fileExt, "HTM")) || (!PL_strcasecmp(fileExt, "HTML")) );
    PR_FREEIF(fileExt);
  }
  
  // is this a text sig with an HTML editor?
  if ( (m_composeHTML) && (!htmlSig) )
    ConvertTextToHTML(testSpec, sigData);
  // is this a HTML sig with a text window?
  else if ( (!m_composeHTML) && (htmlSig) )
    ConvertHTMLToText(testSpec, sigData);
  else // We have a match...
    LoadDataFromFile(testSpec, sigData);  // Get the data!

  // Now that sigData holds data...if any, append it to the body in a nice
  // looking manner
  //
  char      *htmlBreak = "<BR>";
  char      *dashes = "-- ";
  if (sigData != "")
  {
    if (m_composeHTML)
      aMsgBody->Append(htmlBreak);
    else
      aMsgBody->Append(CRLF);
    
    aMsgBody->Append(dashes);
    if (m_composeHTML)
      aMsgBody->Append(htmlBreak);
    else
      aMsgBody->Append(CRLF);
    
    aMsgBody->Append(sigData);
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

  // 
  // Now, we have the body so we can just blast it into the
  // composition editor window.
  //
  m_compFields->GetBody(&bod);
  if (bod)
  {
    ConvertAndLoadComposeWindow(m_editor, "", bod, "", PR_FALSE, m_composeHTML);
  }

  return NS_OK;
}

nsresult nsMsgCompose::NotifyStateListeners(TStateListenerNotification aNotificationType)
{
  if (!mStateListeners)
    return NS_OK;    // maybe there just aren't any.
 
  PRUint32 numListeners;
  nsresult rv = mStateListeners->Count(&numListeners);
  if (NS_FAILED(rv)) return rv;

  PRUint32 i;
  switch (aNotificationType)
  {
    case eComposeFieldsReady:
      for (i = 0; i < numListeners;i++)
      {
        nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mStateListeners->ElementAt(i));
        nsCOMPtr<nsIMsgComposeStateListener> thisListener = do_QueryInterface(iSupports);
        if (thisListener)
        {
          rv = thisListener->NotifyComposeFieldsReady();
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
    
    default:
      NS_NOTREACHED("Unknown notification");
  }

  return rv;
}
