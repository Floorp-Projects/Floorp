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
	
	m_composeHTML = prefs.GetUseHtml();

	NS_INIT_REFCNT();
}


nsMsgCompose::~nsMsgCompose()
{
}


/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompose, nsMsgCompose::GetIID());


nsresult nsMsgCompose::Initialize(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format)
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

	return rv;
}


nsresult nsMsgCompose::LoadFields()
{
	nsresult rv;

	if (!m_window || !m_webShell || !m_webShellWin || !m_compFields)
		return NS_ERROR_NOT_INITIALIZED;
		
    if (m_editor)
    {
/*
    	nsAutoString boby(m_compFields.GetBody());
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
    			
        		m_editor->LoadUrl(nsFileURL(aPath).GetURLString());
    		}
    	}
    	else
*/
    	{
    		if (m_composeHTML)
        		m_editor->LoadUrl("chrome://messengercompose/content/defaultHtmlBody.html");
        	else
        		m_editor->LoadUrl("chrome://messengercompose/content/defaultTextBody.html");
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
/*
						if (id == "msgTo") inputElement->SetValue(msgTo);
						if (id == "msgCc") inputElement->SetValue(msgCc);
						if (id == "msgBcc") inputElement->SetValue(msgBcc);
                        if (id == "msgNewsgroup") inputElement->SetValue(msgNewsgroup);
						if (id == "msgSubject") inputElement->SetValue(msgSubject);
*/
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
	m_compFields->SetCharacterSet(nsAutoCString(charset), NULL);
	
	return NS_OK;
}


nsresult nsMsgCompose::SendMessage(const PRUnichar *callback)
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
					if (m_composeHTML)
						m_editor->GetContentsAsHTML(msgBody);
					else
						m_editor->GetContentsAsText(msgBody);
					SendMessageEx(msgTo.GetUnicode(), msgCc.GetUnicode(), msgBcc.GetUnicode(),
						msgNewsgroup.GetUnicode(), msgSubject.GetUnicode(), msgBody.GetUnicode(), callback);          
				}
			}
		}
	}
	
	return rv;
}


nsresult nsMsgCompose::SendMessageEx(const PRUnichar *addrTo, const PRUnichar *addrCc,
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
		m_compFields->SetFrom((char *)pCompPrefs.GetUserEmail(), NULL);
		m_compFields->SetReplyTo((char *)pCompPrefs.GetReplyTo(), NULL);
		m_compFields->SetOrganization((char *)pCompPrefs.GetOrganization(), NULL);

		// Convert fields to UTF-8
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrTo, &outCString))) 
		{
			m_compFields->SetTo(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetTo(nsAutoCString(addrTo), NULL);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrCc, &outCString))) 
		{
			m_compFields->SetCc(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetCc(nsAutoCString(addrCc), NULL);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrBcc, &outCString))) 
		{
			m_compFields->SetBcc(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetBcc(nsAutoCString(addrBcc), NULL);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, newsgroup, &outCString))) 
		{
			m_compFields->SetNewsgroups(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetNewsgroups(nsAutoCString(newsgroup), NULL);
        
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, subject, &outCString))) 
		{
			m_compFields->SetSubject(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			m_compFields->SetSubject(nsAutoCString(subject), NULL);

		// Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		char *mail_charset;
		m_compFields->GetCharacterSet(&mail_charset);
		aCharset.SetString(mail_charset);
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, body, &outCString))) 
		{
			m_compFields->SetBody(outCString, NULL);
			PR_Free(outCString);
		}
		else
			m_compFields->SetBody(nsAutoCString(body), NULL);

		nsCOMPtr<nsIMsgSend>msgSend = do_QueryInterface(new nsMsgComposeAndSend);
		if (msgSend)
	    {
	        char    *bodyString = NULL;
	        PRInt32 bodyLength;

	        m_compFields->GetBody(&bodyString);
	        bodyLength = PL_strlen(bodyString);

	        msgSend->SendMessage(m_compFields, 
        			"",               					// const char *smtp,
					PR_FALSE,         					// PRBool                            digest_p,
					PR_FALSE,         					// PRBool                            dont_deliver_p,
					nsMsgDeliverNow,   					// nsMsgDeliverMode                  mode,
					m_composeHTML?TEXT_HTML:TEXT_PLAIN,  // const char                        *attachment1_type,
					bodyString,               			// const char                        *attachment1_body,
        			bodyLength,               			// PRUint32                          attachment1_body_length,
					NULL,             					// const struct nsMsgAttachmentData   *attachments,
					NULL,             					// const struct nsMsgAttachedFile     *preloaded_attachments,
					NULL,             					// nsMsgSendPart                     *relatedPart,
					NULL);            					// void  (*message_delivery_done_callback)(MWContext *context, void *fe_data,
									             		//                                         int status, const char *error_message))
	    }
	}
/*TODO
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
		rv = aEditor->QueryInterface(nsIDOMEditorAppCore::GetIID(), (void **)&m_editor);
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
