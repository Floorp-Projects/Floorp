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
#include "nsMsgCompPrefs.h"
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
#include "nsXPIDLString.h"

// XXX temporary so we can use the current identity hack -alecf
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

#if defined(XP_UNIX)
#define TEMP_PATH_DIR "/usr/tmp/"
#elif defined(XP_PC)
#define TEMP_PATH_DIR "c:\\temp\\"
#elif defined(XP_MAC)
#define TEMP_PATH_DIR ""
#elif defined(XP_BEOS)
#define TEMP_PATH_DIR "/tmp/"
#else
#error TEMP_PATH_DIR_NOT_DEFINED
#endif

#define TEMP_MESSAGE_IN  "tempMessage.eml"
#define TEMP_MESSAGE_OUT  "tempMessage.html"

nsMsgCompose::nsMsgCompose()
{
	m_window = nsnull;
	m_webShell = nsnull;
	m_webShellWin = nsnull;
	m_editor = nsnull;
	mOutStream=nsnull;
	m_compFields = do_QueryInterface(new nsMsgCompFields);

	// Get the default charset from pref, use this as a mail charset.
	char * default_mail_charset = nsMsgI18NGetDefaultMailCharset();
	if (default_mail_charset)
	{
   		m_compFields->SetCharacterSet(default_mail_charset, nsnull);
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
  NS_IF_RELEASE(mOutStream);
// ducarroz: we don't need to own the editor shell as JS does it for us.
//  NS_IF_RELEASE(m_editor);
}


/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompose, nsMsgCompose::GetIID());


nsresult nsMsgCompose::Initialize(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI,
	MSG_ComposeType type, MSG_ComposeFormat format, nsISupports *object)
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
	
	 CreateMessage(originalMsgURI, type, format, object); //object is temporary

	return rv;
}


nsresult nsMsgCompose::LoadFields()
{
	nsresult rv;

	if (!m_window || !m_webShell || !m_webShellWin || !m_compFields)
		return NS_ERROR_NOT_INITIALIZED;
		
    if (m_editor)
    {
      char *body;
      m_compFields->GetBody(&body);
    	nsAutoString msgBody(body);
    	if (msgBody.Length())
    	{
    		nsString fileName(TEMP_PATH_DIR);
    		fileName += TEMP_MESSAGE_OUT;
    		
    		nsFileSpec aPath(fileName);
    		nsOutputFileStream tempFile(aPath);
    		
    		if (tempFile.is_open())
    		{
    			tempFile.write(nsAutoCString(msgBody), msgBody.Length());
    			tempFile.close();
    			
    			nsAutoString  urlStr = nsFileURL(aPath).GetURLString();
        	m_editor->LoadUrl(urlStr.GetUnicode());
    		}
    	}
    	else
    	{
    	  nsAutoString  urlStr;
    		if (m_composeHTML)
        		urlStr = "chrome://messengercompose/content/defaultHtmlBody.html";
        	else
        		urlStr = "chrome://messengercompose/content/defaultTextBody.html";
        		
        	m_editor->LoadUrl(urlStr.GetUnicode());
    	}
    }

	nsCOMPtr<nsIDOMDocument> theDoc;
	rv= m_window->GetDocument(getter_AddRefs(theDoc));
	if (NS_SUCCEEDED(rv) && theDoc)
	{
		nsCOMPtr<nsIDOMNode> node;
		nsCOMPtr<nsIDOMNodeList> nodeList;
		nsCOMPtr<nsIDOMHTMLInputElement> inputElement;

		rv = theDoc->GetElementsByTagName("INPUT", getter_AddRefs(nodeList));
		if ((NS_SUCCEEDED(rv)) && nodeList)
		{
			PRUint32 count;
			PRUint32 i;
			nodeList->GetLength(&count);
			for (i = 0; i < count; i ++)
			{
				rv = nodeList->Item(i, getter_AddRefs(node));
				if ((NS_SUCCEEDED(rv)) && node)
				{
					nsString value;
					rv = node->QueryInterface(nsIDOMHTMLInputElement::GetIID(), getter_AddRefs(inputElement));
					if ((NS_SUCCEEDED(rv)) && inputElement)
					{
						nsString id;
						inputElement->GetId(id);
			            char *elementValue;
			            m_compFields->GetTo(&elementValue);
						if (id == "msgTo") inputElement->SetValue(elementValue);

			            m_compFields->GetCc(&elementValue);
						if (id == "msgCc") inputElement->SetValue(elementValue);

			            m_compFields->GetBcc(&elementValue);
						if (id == "msgBcc") inputElement->SetValue(elementValue);

			            m_compFields->GetNewsgroups(&elementValue);
			            if (id == "msgNewsgroup") inputElement->SetValue(elementValue);

			            m_compFields->GetSubject(&elementValue);
						if (id == "msgSubject") inputElement->SetValue(elementValue);
					}
                    
				}
			}
        }		
	}
	
	return rv;
}


nsresult nsMsgCompose::SetDocumentCharset(const PRUnichar *charset) 
{
	// Set charset, this will be used for the MIME charset labeling.
	m_compFields->SetCharacterSet(nsAutoCString(charset), nsnull);
	
	return NS_OK;
}


nsresult nsMsgCompose::SendMsg(MSG_DeliverMode deliverMode,
                               nsIMsgIdentity *identity,
                               const PRUnichar *callback)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsIDOMDocument> domDoc;
	nsCOMPtr<nsIDOMNode> node;
	nsCOMPtr<nsIDOMNodeList> nodeList;
	nsCOMPtr<nsIDOMHTMLInputElement> inputElement;

	nsAutoString msgTo;
	nsAutoString msgCc;
	nsAutoString msgBcc;
    nsAutoString msgNewsgroup;
	nsAutoString msgSubject;
	nsAutoString msgBody;

	if (nsnull != m_window) 
	{
		rv = m_window->GetDocument(getter_AddRefs(domDoc));
		if (NS_SUCCEEDED(rv) && domDoc) 
		{
			rv = domDoc->GetElementsByTagName("INPUT", getter_AddRefs(nodeList));
			if ((NS_SUCCEEDED(rv)) && nodeList)
			{
				PRUint32 count;
				PRUint32 i;
				nodeList->GetLength(&count);
				for (i = 0; i < count; i ++)
				{
					rv = nodeList->Item(i, getter_AddRefs(node));
					if ((NS_SUCCEEDED(rv)) && node)
					{
						nsString value;
						rv = node->QueryInterface(nsIDOMHTMLInputElement::GetIID(), getter_AddRefs(inputElement));
						if ((NS_SUCCEEDED(rv)) && inputElement)
						{
							nsString id;
							inputElement->GetId(id);
							if (id == "msgTo") inputElement->GetValue(msgTo);
							if (id == "msgCc") inputElement->GetValue(msgCc);
							if (id == "msgBcc") inputElement->GetValue(msgBcc);
							if (id == "msgSubject") inputElement->GetValue(msgSubject);
                            if (id == "msgNewsgroup") inputElement->GetValue(msgNewsgroup);
						}

					}
				}

				if (m_editor)
				{
				  PRUnichar *bodyText = NULL;
					if (m_composeHTML)
						m_editor->GetContentsAsHTML(&bodyText);
					else
						m_editor->GetContentsAsText(&bodyText);
					
					msgBody = bodyText;
					delete [] bodyText;
					
					SendMsgEx(deliverMode, identity, msgTo.GetUnicode(), msgCc.GetUnicode(), msgBcc.GetUnicode(),
						msgNewsgroup.GetUnicode(), msgSubject.GetUnicode(), msgBody.GetUnicode(), callback);          
				}
			}
		}
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

#ifdef DEBUG
	  printf("----------------------------\n");
	  printf("--  Sending Mail Message  --\n");
	  printf("----------------------------\n");
	  printf("To: %s  Cc: %s  Bcc: %s\n", (const char *)nsAutoCString(addrTo),
	  	(const char *)nsAutoCString(addrCc), (const char *)nsAutoCString(addrBcc));
	  printf("Subject: %s  \nMsg: %s\n", (const char *)nsAutoCString(subject), (const char *)nsAutoCString(body));
	  printf("----------------------------\n");
#endif //DEBUG

//	nsIMsgCompose *pMsgCompose; 
	if (m_compFields && identity) 
	{ 
		nsString aString;
		nsString aCharset(msgCompHeaderInternalCharset());
		char *outCString;

		// Pref values are supposed to be stored as UTF-8, so no conversion
    nsXPIDLCString email;
    nsXPIDLCString replyTo;
    nsXPIDLCString organization;

    identity->GetEmail(getter_Copies(email));
    identity->GetReplyTo(getter_Copies(replyTo));
    identity->GetOrganization(getter_Copies(organization));
    
		m_compFields->SetFrom(NS_CONST_CAST(char*, (const char *)email), nsnull);
		m_compFields->SetReplyTo(NS_CONST_CAST(char*, (const char *)replyTo),
                             nsnull);
		m_compFields->SetOrganization(NS_CONST_CAST(char*, (const char *)organization), nsnull);

		// Convert fields to UTF-8
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrTo, &outCString))) 
		{
			m_compFields->SetTo(outCString, nsnull);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetTo(nsAutoCString(addrTo), nsnull);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrCc, &outCString))) 
		{
			m_compFields->SetCc(outCString, nsnull);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetCc(nsAutoCString(addrCc), nsnull);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrBcc, &outCString))) 
		{
			m_compFields->SetBcc(outCString, nsnull);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetBcc(nsAutoCString(addrBcc), nsnull);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, newsgroup, &outCString))) 
		{
			m_compFields->SetNewsgroups(outCString, nsnull);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetNewsgroups(nsAutoCString(newsgroup), nsnull);
        
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, subject, &outCString))) 
		{
			m_compFields->SetSubject(outCString, nsnull);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetSubject(nsAutoCString(subject), nsnull);

		// Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		char *mail_charset;
		m_compFields->GetCharacterSet(&mail_charset);
		aCharset.SetString(mail_charset);
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, body, &outCString))) 
		{
			m_compFields->SetBody(outCString, nsnull);
			PR_Free(outCString);
		}
		else
			m_compFields->SetBody(nsAutoCString(body), nsnull);

		nsCOMPtr<nsIMsgSend>msgSend = do_QueryInterface(new nsMsgComposeAndSend);
		if (msgSend)
	    {
	        char    *bodyString = nsnull;
	        PRInt32 bodyLength;

	        m_compFields->GetBody(&bodyString);
	        bodyLength = PL_strlen(bodyString);

	        msgSend->CreateAndSendMessage(
                  identity,
                  m_compFields, 
					        PR_FALSE,         					// PRBool                            digest_p,
					        PR_FALSE,         					// PRBool                            dont_deliver_p,
					        (nsMsgDeliverMode)deliverMode,   	// nsMsgDeliverMode                  mode,
					        m_composeHTML?TEXT_HTML:TEXT_PLAIN,	// const char                        *attachment1_type,
					        bodyString,               			// const char                        *attachment1_body,
        			        bodyLength,               			// PRUint32                          attachment1_body_length,
					        nsnull,             				// const struct nsMsgAttachmentData   *attachments,
					        nsnull,             				// const struct nsMsgAttachedFile     *preloaded_attachments,
					        nsnull,             				// nsMsgSendPart                     *relatedPart,
                  nsnull);                   			// listener array
	    }
	}
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
		CloseWindow();

	return NS_OK;
}

nsresult nsMsgCompose::CloseWindow()
{
	if (m_webShellWin)
	{
		m_editor = nsnull;	/* m_editor will be destroyed during the Close Window. Set it to null to
							   be sure we wont use it anymore. */
		m_webShellWin->Close();
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
 m_editor = aEditor;
// ducarroz: we don't need to own the editor shell as JS does it for us.
// NS_ADDREF(m_editor);
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
	return NS_OK;
}


nsresult nsMsgCompose::GetComposeHTML(PRBool *aComposeHTML)
{
	*aComposeHTML = m_composeHTML;
	return NS_OK;
}


nsresult nsMsgCompose::GetWrapLength(PRInt32 *aWrapLength)
{
	nsMsgCompPrefs prefs;

	*aWrapLength = prefs.GetWrapColumn();
	return NS_OK;
}

nsresult nsMsgCompose::CreateMessage(const PRUnichar * originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format, nsISupports * object)
{
	nsresult rv = NS_OK;
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

        	message->GetCharSet(aCharset);
			message->GetSubject(aString);
			switch (type)
			{
                default	: break;        
                case MSGCOMP_TYPE_Reply : 
                case MSGCOMP_TYPE_ReplyAll:
                {
					// get an original charset, used for a label, UTF-8 is used for the internal processing
					if (!aCharset.Equals(""))
						m_compFields->SetCharacterSet(nsAutoCString(aCharset), nsnull);

					bString += "Re: ";
					bString += aString;
					m_compFields->SetSubject(nsAutoCString(bString), nsnull);
					if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(bString, encodedCharset, decodedString)))
						if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
						{
							m_compFields->SetSubject(aCString, NULL);
							PR_Free(aCString);
						}
                    
					message->GetAuthor(aString);		
					m_compFields->SetTo(nsAutoCString(aString), NULL);
					if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(aString, encodedCharset, decodedString)))
						if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
						{
							m_compFields->SetTo(aCString, NULL);
							PR_Free(aCString);
						}

					if (type == MSGCOMP_TYPE_ReplyAll)
					{
						nsString cString, dString;
						message->GetRecipients(cString);
						CleanUpRecipients(cString);
						message->GetCCList(dString);
						CleanUpRecipients(dString);
						if (cString.Length() > 0 && dString.Length() > 0)
							cString = cString + ", ";
						cString = cString + dString;
						m_compFields->SetCc(nsAutoCString(cString), NULL);
						if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(cString, encodedCharset, decodedString)))
							if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
							{
								m_compFields->SetCc(aCString, NULL);
								PR_Free(aCString);
							}
					}

          QuoteOriginalMessage(originalMsgURI, 1);

					break;
                }
                case MSGCOMP_TYPE_ForwardAsAttachment:
                case MSGCOMP_TYPE_ForwardInline:
                {
					if (!aCharset.Equals(""))
						m_compFields->SetCharacterSet(nsAutoCString(aCharset), nsnull);

					bString += "[Fwd: ";
					bString += aString;
					bString += "]";

					m_compFields->SetSubject(nsAutoCString(bString), nsnull);
					if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(bString, encodedCharset, decodedString)))
						if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
						{
							m_compFields->SetSubject(aCString, nsnull);
							PR_Free(aCString);
						}

                    if (type == MSGCOMP_TYPE_ForwardAsAttachment)
                        QuoteOriginalMessage(originalMsgURI, 0);
                    else
                        QuoteOriginalMessage(originalMsgURI, 2);
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
QuotingOutputStreamImpl::~QuotingOutputStreamImpl() 
{
  if (mComposeObj)
    NS_RELEASE(mComposeObj);
}

QuotingOutputStreamImpl::QuotingOutputStreamImpl(void) 
{ 
  mComposeObj = nsnull;
  mMsgBody = "<br><BLOCKQUOTE TYPE=CITE><html><br>--- Original Message ---<br><br>";
  NS_INIT_REFCNT(); 
}

nsresult
QuotingOutputStreamImpl::Close(void) 
{
  if (mComposeObj) 
  {
    mMsgBody += "</html></BLOCKQUOTE>";
    nsIMsgCompFields *aCompFields;
    mComposeObj->GetCompFields(&aCompFields);
    if (aCompFields)
      aCompFields->SetBody(nsAutoCString(mMsgBody), NULL);

    nsIEditorShell *aEditor = nsnull;
    mComposeObj->GetEditor(&aEditor);

    if (aEditor)
    {
      if (mMsgBody.Length())
      {
        // This is ugly...but hopefully effective...
        nsString fileName(TEMP_PATH_DIR);
        fileName += TEMP_MESSAGE_OUT;
        
        nsFileSpec aPath(fileName);
        nsOutputFileStream tempFile(aPath);
        
        if (tempFile.is_open())
        {
          tempFile.write(nsAutoCString(mMsgBody), mMsgBody.Length());
          tempFile.close();
          
          nsAutoString  urlStr = nsFileURL(aPath).GetURLString();
          aEditor->LoadUrl(urlStr.GetUnicode());
        }
      }
    }

  }
  
  return NS_OK;
}

nsresult
QuotingOutputStreamImpl::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount) 
{
  char *newBuf = (char *)PR_Malloc(aCount + 1);
  *aWriteCount = 0;
  if (!newBuf)
    return NS_ERROR_FAILURE;

  *aWriteCount = aCount;
  
  nsCRT::memcpy(newBuf, aBuf, aCount);
  newBuf[aCount] = '\0';
  mMsgBody += newBuf;
  printf("%s", newBuf);
  PR_FREEIF(newBuf);
  return NS_OK;
}

nsresult
QuotingOutputStreamImpl::Flush(void) 
{
  return NS_OK;
}

nsresult
QuotingOutputStreamImpl::SetComposeObj(nsMsgCompose *obj)
{
  mComposeObj = obj;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotingOutputStreamImpl, nsIOutputStream::GetIID());
////////////////////////////////////////////////////////////////////////////////////
// END OF QUOTING CONSUMER STREAM
////////////////////////////////////////////////////////////////////////////////////

// net service definitions....
static NS_DEFINE_CID(kMsgQuoteCID, NS_MSGQUOTE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsresult
nsMsgCompose::QuoteOriginalMessage(const PRUnichar *originalMsgURI, PRInt32 what) // New template
{
  nsresult    rv;

  //
  // For now, you need to set a pref to do the old quoting
  //
/*
  PRBool  oldQuoting = PR_FALSE;

  nsString    tmpURI(originalMsgURI);
  char        *compString = tmpURI.ToNewCString();

  if (compString)
  {
    if (PL_strncasecmp(compString, "mailbox_message:", 16) != 0)
    {
      oldQuoting = PR_TRUE;
    }
  }
  PR_FREEIF(compString);
 */

  PRBool  oldQuoting = PR_TRUE;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs) 
  {
    rv = prefs->GetBoolPref("mail.old_quoting", &oldQuoting);
  }


  if (oldQuoting)
  {
  	printf("nsMsgCompose: using old quoting function!");
    HackToGetBody(what);
    return NS_OK;
  }

  // Create a mime parser (nsIStreamConverter)!
  rv = nsComponentManager::CreateInstance(kMsgQuoteCID, 
                                          NULL, nsIMsgQuote::GetIID(), 
                                          (void **) getter_AddRefs(mQuote)); 
  if (NS_FAILED(rv) || !mQuote)
    return NS_ERROR_FAILURE;

  // Create the consumer output stream.. this will receive all the HTML from libmime
  mOutStream = new QuotingOutputStreamImpl();
  
  if (!mOutStream)
  {
    printf("Failed to create nsIOutputStream\n");
    return NS_ERROR_FAILURE;
  }
  NS_ADDREF(mOutStream);

  NS_ADDREF(this);
  mOutStream->SetComposeObj(this);

//  mBaseStream = do_QueryInterface(mOutStream);
  return mQuote->QuoteMessage(originalMsgURI, mOutStream);
}

void nsMsgCompose::HackToGetBody(PRInt32 what)
{
    char *buffer = (char *) PR_CALLOC(16384);
    if (buffer)
    {
    	nsString fileName(TEMP_PATH_DIR);
    	fileName += TEMP_MESSAGE_IN;
   
        nsFileSpec fileSpec(fileName);
        nsInputFileStream fileStream(fileSpec);

        nsString msgBody = (what == 2 && !m_composeHTML) ? "--------Original Message--------\r\n" 
            : ""; 

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
            msgBody += MSG_LINEBREAK;
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
        			offset = lowerMsgBody.Find('>', offset);
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
    		
    		msgBody.Insert(MSG_LINEBREAK, endBodyOffset);
    		if (startBodyOffset == 0)
    		{
    			msgBody.Insert("</html>", endBodyOffset);
    			msgBody.Insert(MSG_LINEBREAK, endBodyOffset);
    		}
    		msgBody.Insert("</blockquote>", endBodyOffset);
     		msgBody.Insert(MSG_LINEBREAK, endBodyOffset);
    		
    		msgBody.Insert(MSG_LINEBREAK, startBodyOffset);
    		msgBody.Insert("<blockquote TYPE=CITE>", startBodyOffset);
    		msgBody.Insert(MSG_LINEBREAK, startBodyOffset);
    		if (startBodyOffset == 0)
    		{
    			msgBody.Insert("<html>", startBodyOffset);
    			msgBody.Insert(MSG_LINEBREAK, startBodyOffset);
    			msgBody.Insert("<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">", startBodyOffset);
     		}
        }
        else
        {
			//ducarroz: today, we are not converting HTML to plain text if needed!
        }

		// m_compFields->SetBody(msgBody.ToNewCString(), NULL);
		// SetBody() strdup()'s cmsgBody.
		m_compFields->SetBody(nsAutoCString(msgBody), NULL);
        PR_Free(buffer);
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



