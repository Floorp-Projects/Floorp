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


#ifdef XP_UNIX
#define TEMP_PATH_DIR "/usr/tmp/"
#endif

#ifdef XP_PC
#define TEMP_PATH_DIR "c:\\temp\\"
#endif

#ifdef XP_MAC
#define TEMP_PATH_DIR ""
#endif
#define TEMP_MESSAGE_IN  "tempMessage.eml"
#define TEMP_MESSAGE_OUT  "tempMessage.html"

nsMsgCompose::nsMsgCompose()
{
	nsMsgCompPrefs prefs;

	m_window = nsnull;
	m_webShell = nsnull;
	m_webShellWin = nsnull;
	m_editor = nsnull;
	m_compFields = do_QueryInterface(new nsMsgCompFields);

	// Get the default charset from pref, use this as a mail charset.
	char * default_mail_charset = nsMsgI18NGetDefaultMailCharset();
	if (default_mail_charset)
	{
   		m_compFields->SetCharacterSet(default_mail_charset, nsnull);
    	PR_Free(default_mail_charset);
  	}
	
	m_composeHTML = prefs.GetUseHtml();

	NS_INIT_REFCNT();
}


nsMsgCompose::~nsMsgCompose()
{
}


/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompose, nsMsgCompose::GetIID());


nsresult nsMsgCompose::Initialize(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI,
	MSG_ComposeType type, MSG_ComposeFormat format, nsISupports *object)
{
	nsresult rv = NS_OK;
	nsMsgCompPrefs prefs;

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
		default							: m_composeHTML = prefs.GetUseHtml();		break;

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
    	nsAutoString msgBody(m_compFields->GetBody());
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
						if (id == "msgTo") inputElement->SetValue(m_compFields->GetTo());
						if (id == "msgCc") inputElement->SetValue(m_compFields->GetCc());
						if (id == "msgBcc") inputElement->SetValue(m_compFields->GetBcc());
                        if (id == "msgNewsgroup") inputElement->SetValue(m_compFields->GetNewsgroups());
						if (id == "msgSubject") inputElement->SetValue(m_compFields->GetSubject());
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


nsresult nsMsgCompose::SendMsg(MSG_DeliverMode deliverMode, const PRUnichar *callback)
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
					
					SendMsgEx(deliverMode, msgTo.GetUnicode(), msgCc.GetUnicode(), msgBcc.GetUnicode(),
						msgNewsgroup.GetUnicode(), msgSubject.GetUnicode(), msgBody.GetUnicode(), callback);          
				}
			}
		}
	}
	
	return rv;
}


nsresult nsMsgCompose::SendMsgEx(MSG_DeliverMode deliverMode, const PRUnichar *addrTo, const PRUnichar *addrCc,
	const PRUnichar *addrBcc, const PRUnichar *newsgroup, const PRUnichar *subject,
	const PRUnichar *body, const PRUnichar *callback)
{
	nsMsgCompPrefs pCompPrefs;

#ifdef DEBUG
	  printf("----------------------------\n");
	  printf("--  Sending Mail Message  --\n");
	  printf("----------------------------\n");
	  printf("To: %s  Cc: %s  Bcc: %s\n", addrTo, addrCc, addrBcc);
	  printf("Subject: %s  \nMsg: %s\n", subject, body);
	  printf("----------------------------\n");
#endif //DEBUG

//	nsIMsgCompose *pMsgCompose; 
	if (m_compFields) 
	{ 
		nsString aString;
		nsString aCharset(msgCompHeaderInternalCharset());
		char *outCString;

		// Pref values are supposed to be stored as UTF-8, so no conversion
		m_compFields->SetFrom((char *)pCompPrefs.GetUserEmail(), nsnull);
		m_compFields->SetReplyTo((char *)pCompPrefs.GetReplyTo(), nsnull);
		m_compFields->SetOrganization((char *)pCompPrefs.GetOrganization(), nsnull);

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

	        msgSend->CreateAndSendMessage(m_compFields, 
					PR_FALSE,         					// PRBool                            digest_p,
					PR_FALSE,         					// PRBool                            dont_deliver_p,
					(nsMsgDeliverMode)deliverMode,   	// nsMsgDeliverMode                  mode,
					m_composeHTML?TEXT_HTML:TEXT_PLAIN,	// const char                        *attachment1_type,
					bodyString,               			// const char                        *attachment1_body,
        			bodyLength,               			// PRUint32                          attachment1_body_length,
					nsnull,             				// const struct nsMsgAttachmentData   *attachments,
					nsnull,             				// const struct nsMsgAttachedFile     *preloaded_attachments,
					nsnull,             				// nsMsgSendPart                     *relatedPart,
	          		nsnull,                   			// callback function defined in nsMsgComposeBE.h
	          		nsnull);                  			// tagged FE data that will be passed to the FE
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
		m_webShellWin->Close();

	return NS_OK;
}


nsresult nsMsgCompose::GetEditor(/*nsIDOMEditorAppCore*/nsISupports * *aEditor)
{
	*aEditor = nsnull;
	return NS_OK;
}


nsresult nsMsgCompose::SetEditor(/*nsIDOMEditorAppCore*/nsISupports * aEditor)
{
	nsresult rv;
	if (aEditor)
		rv = aEditor->QueryInterface(nsIEditorShell::GetIID(), (void **)&m_editor);
	else
		return NS_ERROR_NULL_POINTER;

	return rv;
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
						message->GetCCList(dString);
						if (cString.Length() > 0 && dString.Length() > 0)
							cString = cString + ", ";
						cString = cString + dString;
						m_compFields->SetCc(nsAutoCString(cString), NULL);
						if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(cString, encodedCharset, decodedString)))
							if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString)))
							{
								m_compFields->SetTo(aCString, NULL);
								PR_Free(aCString);
							}
					}
					HackToGetBody(1);
					break;
                }
                case MSGCOMP_TYPE_ForwardAsAttachment:
                case MSGCOMP_TYPE_ForwardInline:
                case MSGCOMP_TYPE_ForwardQuoted:
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
                        HackToGetBody(0);
                    else if (type == MSGCOMP_TYPE_ForwardInline)
                        HackToGetBody(2);
                    else
                        HackToGetBody(1);
                    break;
                }
			}
		}
	}	
	return rv;
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

