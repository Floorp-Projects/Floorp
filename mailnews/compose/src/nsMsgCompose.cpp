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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsMsgCompose.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSelection.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsMsgI18N.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
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
#include "nsIMsgStringService.h"
#include "nsMsgComposeStringBundle.h"
#include "nsSpecialSystemDirectory.h"
#include "nsMsgSend.h"
#include "nsMsgCreate.h"
#include "nsMailHeaders.h"
#include "nsMsgPrompts.h"
#include "nsMimeTypes.h"
#include "nsICharsetConverterManager.h"
#include "nsTextFormatter.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsEscape.h"
#include "plstr.h"
#include "nsIDocShell.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsAbBaseCID.h"
#include "nsIAddrDatabase.h"
#include "nsIAddrBookSession.h"
#include "nsIAddressBook.h"
#include "nsIMIMEService.h"

// Defines....
static NS_DEFINE_CID(kMsgQuoteCID, NS_MSGQUOTE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgRecipientArrayCID, NS_MSGRECIPIENTARRAY_CID);
static NS_DEFINE_CID(kMimeServiceCID, NS_MIMESERVICE_CID);

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

	nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);;
	if (parser)
		rv= parser->RemoveDuplicateAddresses(msgCompHeaderInternalCharset(), addresses, anothersAddresses, removeAliasesToMe, newAddress);
	else
		rv = NS_ERROR_FAILURE;

	return rv;
}


nsMsgCompose::nsMsgCompose()
{
	NS_INIT_REFCNT();

  mEntityConversionDone = PR_FALSE;
	mQuotingToFollow = PR_FALSE;
	mWhatHolder = 1;
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
	if (mDocumentListener)
	{
		mDocumentListener->SetComposeObj(nsnull);      
		NS_RELEASE(mDocumentListener);
	}
	NS_IF_RELEASE(m_sendListener);
	NS_IF_RELEASE(m_compFields);
	NS_IF_RELEASE(mQuoteStreamListener);
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompose, NS_GET_IID(nsMsgCompose));

//
// Once we are here, convert the data which we know to be UTF-8 to UTF-16
// for insertion into the editor
//
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

nsresult
TranslateLineEndings(nsString &aString)
{
  // First, do sanity checking...if aString doesn't have
  // any CR's, then there is no reason to call this rest
  // of this 
  if (aString.FindChar(CR) < 0)
    return NS_OK;

  DoLineEndingConJobUnicode(aString);
  return NS_OK;
}

nsresult 
GetChildOffset(nsIDOMNode *aChild, nsIDOMNode *aParent, PRInt32 &aOffset)
{
  NS_ASSERTION((aChild && aParent), "bad args");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (aChild && aParent)
  {
    nsCOMPtr<nsIDOMNodeList> childNodes;
    result = aParent->GetChildNodes(getter_AddRefs(childNodes));
    if ((NS_SUCCEEDED(result)) && (childNodes))
    {
      PRInt32 i=0;
      for ( ; NS_SUCCEEDED(result); i++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        result = childNodes->Item(i, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(result)) && (childNode))
        {
          if (childNode.get()==aChild)
          {
            aOffset = i;
            break;
          }
        }
        else if (!childNode)
          result = NS_ERROR_NULL_POINTER;
      }
    }
    else if (!childNodes)
      result = NS_ERROR_NULL_POINTER;
  }
  return result;
}

nsresult 
GetNodeLocation(nsIDOMNode *inChild, nsCOMPtr<nsIDOMNode> *outParent, PRInt32 *outOffset)
{
  NS_ASSERTION((outParent && outOffset), "bad args");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (inChild && outParent && outOffset)
  {
    result = inChild->GetParentNode(getter_AddRefs(*outParent));
    if ( (NS_SUCCEEDED(result)) && (*outParent) )
    {
      result = GetChildOffset(inChild, *outParent, *outOffset);
    }
  }

  return result;
}

nsresult nsMsgCompose::ConvertAndLoadComposeWindow(nsIEditorShell *aEditorShell, nsString& aPrefix, nsString& aBuf, 
                                                   nsString& aSignature, PRBool aQuoted, PRBool aHTMLEditor)

{
  // First, get the nsIEditor interface for future use
  nsCOMPtr<nsIEditor> editor;
  nsCOMPtr<nsIDOMNode> nodeInserted;

  aEditorShell->GetEditor(getter_AddRefs(editor));

  TranslateLineEndings(aPrefix);
  TranslateLineEndings(aBuf);
  TranslateLineEndings(aSignature);

  if (editor)
    editor->EnableUndo(PR_FALSE);

  // Ok - now we need to figure out the charset of the aBuf we are going to send
  // into the editor shell. There are I18N calls to sniff the data and then we need
  // to call the new routine in the editor that will allow us to send in the charset
  //

  // Now, insert it into the editor...
  if ( (aQuoted) )
  {
    if (!aPrefix.IsEmpty())
    {
      if (aHTMLEditor)
        aEditorShell->InsertSource(aPrefix.GetUnicode());
      else
        aEditorShell->InsertText(aPrefix.GetUnicode());
    }

    if (!aBuf.IsEmpty())
    {
      if (!mCiteReference.IsEmpty())
        aEditorShell->InsertAsCitedQuotation(aBuf.GetUnicode(), mCiteReference.GetUnicode(),
                                             PR_TRUE,
                                             NS_ConvertASCIItoUCS2("UTF-8").GetUnicode(), getter_AddRefs(nodeInserted));
      else
        aEditorShell->InsertAsQuotation(aBuf.GetUnicode(), getter_AddRefs(nodeInserted));
    }

    if (!aSignature.IsEmpty())
    {
      if (aHTMLEditor)
        aEditorShell->InsertSource(aSignature.GetUnicode());
      else
        aEditorShell->InsertText(aSignature.GetUnicode());
    }
  }
  else
  {
    if (aHTMLEditor)
    {
      if (!aBuf.IsEmpty())
        aEditorShell->InsertSource(aBuf.GetUnicode());
      if (!aSignature.IsEmpty())
        aEditorShell->InsertSource(aSignature.GetUnicode());
    }
    else
    {
      if (!aBuf.IsEmpty())
        aEditorShell->InsertText(aBuf.GetUnicode());

      if (!aSignature.IsEmpty())
        aEditorShell->InsertText(aSignature.GetUnicode());
    }
  }
  
  if (editor)
  {
    if (aBuf.IsEmpty())
      editor->BeginningOfDocument();
    else
	    switch (GetReplyOnTop())
	    {
        // This should set the cursor after the body but before the sig
		    case 0	:
		    {
		      nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
          if (!htmlEditor)
          {
            editor->BeginningOfDocument();
            break;
          }

          nsCOMPtr<nsIDOMSelection> selection = nsnull; 
          nsCOMPtr<nsIDOMNode>      parent = nsnull; 
          PRInt32                   offset;
          nsresult                  rv;

          // get parent and offset of mailcite
          rv = GetNodeLocation(nodeInserted, &parent, &offset); 
          if ( NS_FAILED(rv) || (!parent))
          {
            editor->BeginningOfDocument();
            break;
          }

          // get selection
          editor->GetSelection(getter_AddRefs(selection));
          if (!selection)
          {
            editor->BeginningOfDocument();
            break;
          }

          // place selection after mailcite
          selection->Collapse(parent, offset+1);

          // insert a break at current selection
          htmlEditor->InsertBreak();

          // i'm not sure if you need to move the selection back to before the
          // break. expirement.
          selection->Collapse(parent, offset+1);
 
		      break;
		    }
		  
		  case 2	: 
		  {
		    editor->SelectAll();
		    break;
		  }
		  
      // This should set the cursor to the top!
      default	: editor->BeginningOfDocument();		break;
	  }
  }

  NotifyStateListeners(nsMsgCompose::eComposeFieldsReady);
  if (editor)
    editor->EnableUndo(PR_TRUE);
  SetBodyModified(PR_FALSE);

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
		
      nsCOMPtr<nsIDocShell> docShell;
		globalObj->GetDocShell(getter_AddRefs(docShell));
		nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
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
    case nsIMsgCompFormat::OppositeOfDefault:
      /* ask the identity which compose to use */
      if (m_identity) m_identity->GetComposeHtml(&m_composeHTML);
      /* then use the opposite */
      m_composeHTML = !m_composeHTML;
      break;
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
	nsCAutoString charsetStr;
	charsetStr.AssignWithConversion(charset);
	m_compFields->SetCharacterSet(charsetStr);
	
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
    nsXPIDLString fullName;
    nsXPIDLString organization;
    
    identity->GetEmail(getter_Copies(email));
    identity->GetFullName(getter_Copies(fullName));
    identity->GetOrganization(getter_Copies(organization));
    
	char * sender = nsnull;
	nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);
  if (parser) {
    // convert to UTF8 before passing to MakeFullAddress
    nsAutoString fullNameStr(fullName);
    char *fullNameUTF8 = fullNameStr.ToNewUTF8String();
		parser->MakeFullAddress(nsnull, fullNameUTF8, email, &sender);
    nsCRT::free(fullNameUTF8);
  }
  
	if (!sender)
		m_compFields->SetFrom(email);
	else
		m_compFields->SetFrom(sender);
	PR_FREEIF(sender);

    m_compFields->SetOrganization(organization);
    
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
      PRBool      newBody = PR_FALSE;
      char        *bodyString = (char *)m_compFields->GetBody();
      PRInt32     bodyLength;
      char        *attachment1_type = TEXT_HTML;  // we better be "text/html" at this point
      
      mMsgSend->SetWebShell(m_webShell);

      if (!mEntityConversionDone)
      {
        // Convert body to mail charset
        char      *outCString;
      
        if (  bodyString && *bodyString )
        {
          // Apply entity conversion then convert to a mail charset. 
          rv = nsMsgI18NSaveAsCharset(attachment1_type, m_compFields->GetCharacterSet(), 
                                      NS_ConvertASCIItoUCS2(bodyString).GetUnicode(), &outCString);
          if (NS_SUCCEEDED(rv)) 
          {
            bodyString = outCString;
            newBody = PR_TRUE;
            mEntityConversionDone = PR_TRUE;
          }
        }
      }
      
      bodyLength = PL_strlen(bodyString);
      
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
      
      // Cleanup converted body...
      if (newBody)
        PR_FREEIF(bodyString);

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
    nsAutoString format; format.AssignWithConversion(contentType);
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
    
    if (!mEntityConversionDone)
    {
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
          // body contains multilingual data, confirm send to the user
          if (NS_ERROR_UENC_NOMAPPING == rv) {
            PRBool proceedTheSend;
            rv = nsMsgAskBooleanQuestionByID(NS_MSG_MULTILINGUAL_SEND, &proceedTheSend);
            if (!proceedTheSend) {
              PR_FREEIF(outCString);
              return NS_ERROR_BUT_DONT_SHOW_ALERT;
            }
          }

          mEntityConversionDone = PR_TRUE;
			    m_compFields->SetBody(outCString);
			    PR_Free(outCString);
		    }
		    else
        {
          nsCAutoString msgbodyC;
          msgbodyC.AssignWithConversion(msgBody);
			    m_compFields->SetBody(msgbodyC);
        }
      }
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
		nsAutoString aCharset; aCharset.AssignWithConversion(msgCompHeaderInternalCharset());
		char *outCString;

		// Convert fields to UTF-8
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrTo, &outCString))) 
		{
			m_compFields->SetTo(outCString);
			PR_Free(outCString);
		}
		else 
		{
		  nsCAutoString addrToCStr; addrToCStr.AssignWithConversion(addrTo);
			m_compFields->SetTo(addrToCStr);
	  }

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrCc, &outCString))) 
		{
			m_compFields->SetCc(outCString);
			PR_Free(outCString);
		}
		else
		{
      nsCAutoString addrCcCStr; addrCcCStr.AssignWithConversion(addrCc);
      m_compFields->SetCc(addrCcCStr);
    }

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, addrBcc, &outCString))) 
		{
			m_compFields->SetBcc(outCString);
			PR_Free(outCString);
		}
		else 
		{
      nsCAutoString addrBccCStr; addrBccCStr.AssignWithConversion(addrBcc);
      m_compFields->SetBcc(addrBccCStr);
    }

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, newsgroup, &outCString))) 
		{
			m_compFields->SetNewsgroups(outCString);
			PR_Free(outCString);
		}
		else 
		{
      nsCAutoString newsgroupCStr; newsgroupCStr.AssignWithConversion(newsgroup);
      m_compFields->SetNewsgroups(newsgroupCStr);
    }
        
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, subject, &outCString))) 
		{
			m_compFields->SetSubject(outCString);
			PR_Free(outCString);
		}
		else 
		{
      nsCAutoString subjectCStr; subjectCStr.AssignWithConversion(subject);
      m_compFields->SetSubject(subjectCStr);
    }

		// Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		aCharset.AssignWithConversion(m_compFields->GetCharacterSet());
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, body, &outCString))) 
		{
			m_compFields->SetBody(outCString);
			PR_Free(outCString);
		}
		else
		{
      nsCAutoString bodyCStr; bodyCStr.AssignWithConversion(body);
      m_compFields->SetBody(bodyCStr);
    }

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
    m_editor->LoadUrl(NS_ConvertASCIItoUCS2("about:blank").GetUnicode());

    return NS_OK;
} 

nsresult nsMsgCompose::GetBodyModified(PRBool * modified)
{
	nsresult rv;

	if (! modified)
		return NS_ERROR_NULL_POINTER;

	*modified = PR_TRUE;
			
	if (m_editor)
	{
		nsCOMPtr<nsIEditor> editor;
		rv = m_editor->GetEditor(getter_AddRefs(editor));
		if (NS_SUCCEEDED(rv) && editor)
		{
			rv = editor->GetDocumentModified(modified);
			if (NS_FAILED(rv))
				*modified = PR_TRUE;
		}
	}

	return NS_OK; 	
}

nsresult nsMsgCompose::SetBodyModified(PRBool modified)
{
	nsresult  rv = NS_OK;

	if (m_editor)
	{
		nsCOMPtr<nsIEditor> editor;
		rv = m_editor->GetEditor(getter_AddRefs(editor));
		if (NS_SUCCEEDED(rv) && editor)
		{
			nsCOMPtr<nsIDOMDocument>  theDoc;
			rv = editor->GetDocument(getter_AddRefs(theDoc));
			if (NS_FAILED(rv))
				return rv;
  
			nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(theDoc, &rv);
			if (NS_FAILED(rv))
				return rv;

			if (modified)
			{
				PRInt32  modCount = 0;
				diskDoc->GetModCount(&modCount);
				if (modCount == 0)
					diskDoc->IncrementModCount(1);
			}
			else
				diskDoc->ResetModCount();
		}
	}

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

    mType = type;
	if (compFields)
	{
		if (m_compFields)
			rv = m_compFields->Copy(compFields);
		return rv;
	}
	
	if (m_identity)
	{
	    /* Setup reply-to field */
	    nsXPIDLCString replyTo;
	    m_identity->GetReplyTo(getter_Copies(replyTo));
	    m_compFields->SetReplyTo(replyTo);
	    
	    /* Setup bcc field */
	    PRBool  aBool;
	    nsString bccStr;
	    
	    m_identity->GetBccSelf(&aBool);
	    if (aBool)
	    {
	        nsXPIDLCString email;
	        m_identity->GetEmail(getter_Copies(email));
	        bccStr.AssignWithConversion(email);
	    }
	    
	    m_identity->GetBccOthers(&aBool);
	    if (aBool)
	    {
	        nsXPIDLCString bccList;
	        m_identity->GetBccList(getter_Copies(bccList));
	        if (bccStr.Length() > 0)
	            bccStr.AppendWithConversion(',');
	        bccStr.AppendWithConversion(bccList);
	    }
	    m_compFields->SetBcc(bccStr.GetUnicode());
	}

    /* In case of forwarding multiple messages, originalMsgURI will contains several URI separated by a comma. */
    /* we need to extract only the first URI*/
    nsAutoString  firstURI(originalMsgURI);
    PRInt32 offset = firstURI.FindChar(',');
    if (offset >= 0)
    	firstURI.Truncate(offset);
    
    nsCOMPtr<nsIMessage> message = getter_AddRefs(GetIMessageFromURI(firstURI.GetUnicode()));
    if ((NS_SUCCEEDED(rv)) && message)
    {
      nsXPIDLCString subject;
      nsAutoString subjectStr;
      nsAutoString aCharset;
      nsAutoString decodedString;
      nsAutoString encodedCharset;  // we don't use this
      char *aCString = nsnull;
    
      rv = message->GetCharSet(&aCharset);
      if (NS_FAILED(rv)) return rv;
      rv = message->GetSubject(getter_Copies(subject));
      if (NS_FAILED(rv)) return rv;
      
      switch (type)
      {
      default: break;        
      case nsIMsgCompType::Reply : 
      case nsIMsgCompType::ReplyAll:
      case nsIMsgCompType::ReplyToGroup:
      case nsIMsgCompType::ReplyToSenderAndGroup:
        {
          mQuotingToFollow = PR_TRUE;
          NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  	      if (NS_SUCCEEDED(rv))
		        prefs->GetBoolPref("mail.auto_quote", &mQuotingToFollow);
          
          // get an original charset, used for a label, UTF-8 is used for the internal processing
          if (!aCharset.IsEmpty())
          {
            nsCAutoString aCharsetCStr; aCharsetCStr.AssignWithConversion(aCharset);
            m_compFields->SetCharacterSet(aCharsetCStr);
           }
        
          subjectStr.AppendWithConversion("Re: ");
          subjectStr.AppendWithConversion(subject);
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(subjectStr, encodedCharset, decodedString)))
            m_compFields->SetSubject(decodedString.GetUnicode());
          else
            m_compFields->SetSubject(subjectStr.GetUnicode());

          nsXPIDLCString author;
          rv = message->GetAuthor(getter_Copies(author));		
          if (NS_FAILED(rv)) return rv;
          m_compFields->SetTo(author);

          nsString authorStr; authorStr.AssignWithConversion(author);
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(authorStr, encodedCharset, decodedString)))
            if (NS_SUCCEEDED(rv = ConvertFromUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), decodedString, &aCString)))
            {
              m_compFields->SetTo(aCString);
              PR_Free(aCString);
            }
          
            if (type == nsIMsgCompType::ReplyAll)
            {
              nsXPIDLCString recipients;
              rv = message->GetRecipients(getter_Copies(recipients));
              if (NS_FAILED(rv)) return rv;
              nsAutoString recipStr; recipStr.AssignWithConversion(recipients);
              CleanUpRecipients(recipStr);
              
              nsXPIDLCString ccList;
              rv = message->GetCcList(getter_Copies(ccList));
              if (NS_FAILED(rv)) return rv;
              
              nsAutoString ccListStr; ccListStr.AssignWithConversion(ccList);
              CleanUpRecipients(ccListStr);
              if (recipStr.Length() > 0 && ccListStr.Length() > 0)
                recipStr.AppendWithConversion(", ");
              recipStr += ccListStr;
              {
nsCAutoString recipStrCStr; recipStrCStr.AssignWithConversion(recipStr);
m_compFields->SetCc(recipStrCStr);
}
              
              if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(recipStr, encodedCharset, decodedString)))
                if (NS_SUCCEEDED(rv = ConvertFromUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), decodedString, &aCString)))
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
        {
        
          if (!aCharset.IsEmpty())
          {
            nsCAutoString aCharsetCStr; aCharsetCStr.AssignWithConversion(aCharset);
            m_compFields->SetCharacterSet(aCharsetCStr);
          }
        
          subjectStr.AppendWithConversion("[Fwd: ");
          subjectStr.AppendWithConversion(subject);
          subjectStr.AppendWithConversion("]");
        
          if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(subjectStr, encodedCharset, decodedString)))
            m_compFields->SetSubject(decodedString.GetUnicode());
          else
            m_compFields->SetSubject(subjectStr.GetUnicode());
        
          // Setup quoting callbacks for later...
          mQuotingToFollow = PR_FALSE;	//We don't need to quote the original message.
          m_compFields->SetAttachments(originalMsgURI);
        
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

  nsCOMPtr<nsIMessage> originalMsg = getter_AddRefs(GetIMessageFromURI(originalMsgURI));
  if (originalMsg && !quoteHeaders)
  {
    nsresult rv;
    nsXPIDLString author;

    // Setup the cite information....
    char    *msgID = nsnull;
    if (NS_SUCCEEDED(originalMsg->GetMessageId(&msgID)) && msgID)
    {
      mCiteReference.AssignWithConversion(msgID);
      PR_FREEIF(msgID);
    }

    rv = originalMsg->GetMime2DecodedAuthor(getter_Copies(author));
    if (NS_SUCCEEDED(rv))
    {
      char * authorName = nsnull;
      nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);

      if (parser)
      {
        nsAutoString aCharset; aCharset.AssignWithConversion(msgCompHeaderInternalCharset());
        char * utf8Author = nsnull;
        nsAutoString authorStr; authorStr.Assign(author);
        rv = ConvertFromUnicode(aCharset, authorStr, &utf8Author);
        if (NS_SUCCEEDED(rv) && utf8Author)
        {
          nsCAutoString acharsetC;
          acharsetC.AssignWithConversion(aCharset);
          rv = parser->ExtractHeaderAddressName(acharsetC, utf8Author, &authorName);
          if (NS_SUCCEEDED(rv))
            rv = ConvertToUnicode(aCharset, authorName, authorStr);
        }
        
        if (!utf8Author || NS_FAILED(rv))
        {
          nsCAutoString authorCStr;
          authorCStr.AssignWithConversion(author);
          rv = parser->ExtractHeaderAddressName(nsnull, authorCStr, &authorName);
          if (NS_SUCCEEDED(rv))
            authorStr.AssignWithConversion(authorName);
        }
        if (NS_SUCCEEDED(rv))
        {
          if (GetReplyOnTop() == 1)
            mCitePrefix.AppendWithConversion("<br><br>");

          mCitePrefix.Append(author);
          mCitePrefix.AppendWithConversion(" wrote:<br><html>");
        }
        if (authorName)
          PL_strfree(authorName);
        PR_FREEIF(utf8Author);
      }
    }
  }
  
  if (mCitePrefix.IsEmpty())
    mCitePrefix.AppendWithConversion("<br><br>--- Original Message ---<br><html>");
  
  NS_INIT_REFCNT(); 
}

nsresult
QuotingOutputStreamListener::ConvertToPlainText()
{
nsresult  rv = NS_OK;

  rv += ConvertBufToPlainText(mCitePrefix);
  rv += ConvertBufToPlainText(mMsgBody);
  rv += ConvertBufToPlainText(mSignature);
  return rv;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStartRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */)
{
	return NS_OK;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStopRequest(nsIChannel *aChannel, nsISupports * /* ctxt */, nsresult status, const PRUnichar * /* errorMsg */)
{
  nsresult rv = NS_OK;
  
  if (mComposeObj) 
  {
    MSG_ComposeType type = mComposeObj->GetMessageType();
    
    // Assign cite information if available...
    if (!mCiteReference.IsEmpty())
      mComposeObj->mCiteReference = mCiteReference;

    if (mHeaders && (type == nsIMsgCompType::Reply || type == nsIMsgCompType::ReplyAll ||
                     type == nsIMsgCompType::ReplyToGroup || type == nsIMsgCompType::ReplyToSenderAndGroup))
    {
      nsIMsgCompFields *compFields = nsnull;
      mComposeObj->GetCompFields(&compFields); //GetCompFields will addref, you need to release when your are done with it
      if (compFields)
      {
        nsAutoString aCharset; aCharset.AssignWithConversion(msgCompHeaderInternalCharset());
        nsAutoString replyTo;
        nsAutoString newgroups;
        nsAutoString followUpTo;
        nsAutoString messageId;
        nsAutoString references;
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
        
        mHeaders->ExtractHeader(HEADER_MESSAGE_ID, PR_FALSE, &outCString);
        if (outCString)
        {
          // Convert fields to UTF-8
          ConvertToUnicode(aCharset, outCString, messageId);
          PR_FREEIF(outCString);
        }
        
        mHeaders->ExtractHeader(HEADER_REFERENCES, PR_FALSE, &outCString);
        if (outCString)
        {
          // Convert fields to UTF-8
          ConvertToUnicode(aCharset, outCString, references);
          PR_FREEIF(outCString);
        }
        
        if (! replyTo.IsEmpty())
        {
          compFields->SetTo(replyTo.GetUnicode());
          toChanged = PR_TRUE;
        }
        
        if (! newgroups.IsEmpty())
        {
          if (type != nsIMsgCompType::Reply)
            compFields->SetNewsgroups(newgroups.GetUnicode());
          if (type == nsIMsgCompType::ReplyToGroup)
            compFields->SetTo(&emptyUnichar);
        }
        
        if (! followUpTo.IsEmpty())
        {
          compFields->SetNewsgroups(followUpTo.GetUnicode());
          if (type == nsIMsgCompType::Reply)
            compFields->SetTo(&emptyUnichar);
        }
        
        if (! references.IsEmpty())
          references.AppendWithConversion(' ');
        references += messageId;
        compFields->SetReferences(references.GetUnicode());
        
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
    
        // Ok, if we are here, we need to see if a charset was tagged
        // on this channel. If there WAS, then this charset needs to 
        // override the default charset that is set in compFields. This
        // is the case where you are replying to a message that has a
        // non US-ASCII charset. You are supposed to reply in that charset.
        // 
        char *contentType = nsnull;
        if (NS_SUCCEEDED(aChannel->GetContentType(&contentType)) && contentType)
        {
          char *workContentType = nsCRT::strdup(contentType);
          if (workContentType)
          {
            char *ptr = PL_strstr(workContentType, "charset=");
            if (ptr)
            {
              ptr += nsCRT::strlen("charset=");
              if (*ptr == '"')
                ptr++;

              char *ptr2 = ptr;
              while (*ptr2)
              {
                if ( (*ptr2 == ' ') || (*ptr2 == ';') || (*ptr2 == '"'))
                {
                  *ptr2 = '\0';
                  break;
                }

                ++ptr2;
              }

              // Re-label "us-ascii" to "ISO-8859-1" since the original body may contain
              // non us-ascii characters with eitity encoded.
              aCharset.AssignWithConversion(ptr);
              if (aCharset.EqualsIgnoreCase("us-ascii"))
                aCharset.AssignWithConversion("ISO-8859-1");
              else {
                // Use canonical charset name instead of using the charset name from the message header as is.
                // This is needed for charset menu item to have a check mark correctly, for example.
                nsCOMPtr <nsICharsetConverterManager2> ccm2 = do_GetService(kCharsetConverterManagerCID, &rv);
                if (NS_SUCCEEDED(rv)) {
                  nsCOMPtr <nsIAtom> charsetAtom;
                  rv = ccm2->GetCharsetAtom(aCharset.GetUnicode(), getter_AddRefs(charsetAtom));
                  if (NS_SUCCEEDED(rv)) {
                    nsAutoString canonicalCharset;
                    rv = charsetAtom->ToString(canonicalCharset);
                    if (NS_SUCCEEDED(rv))
                      aCharset = canonicalCharset;
                    else
                      rv = NS_OK;   // no cannonical charset name does not mean an error
                  }
                }
              }

              compFields->SetCharacterSet(aCharset.GetUnicode());
            }

            PR_FREEIF(workContentType);
          }

          PR_FREEIF(contentType);
        }

        NS_RELEASE(compFields);
      }
    }
    
    mMsgBody.AppendWithConversion("</html>");
    
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
  NS_ENSURE_ARG(inStr);

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
    nsAutoString    fmt; fmt.AssignWithConversion("%s");

    u = nsTextFormatter::smprintf(fmt.GetUnicode(), newBuf); // this converts UTF-8 to UCS-2 
    if (u)
    {
      PRInt32   newLen = nsCRT::strlen(u);
      mMsgBody.Append(u, newLen);
      PR_FREEIF(u);
    }
    else
      mMsgBody.AppendWithConversion(newBuf, numWritten);
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


NS_IMPL_ISUPPORTS(QuotingOutputStreamListener, NS_GET_IID(nsIStreamListener));
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
  
  // Create a mime parser (nsIStreamConverter)!
  rv = nsComponentManager::CreateInstance(kMsgQuoteCID, 
                                          NULL, NS_GET_IID(nsIMsgQuote), 
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
	PRUint16 i;
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

  if (aIID.Equals(NS_GET_IID(nsIMsgSendListener))) 
  {
	  *aInstancePtr = (nsIMsgSendListener *) this;                                                   
	  NS_ADDREF_THIS();
	  return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIMsgCopyServiceListener)))
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

NS_IMPL_ISUPPORTS(nsMsgDocumentStateListener, NS_GET_IID(nsIDocumentStateListener));

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

  // Now, do the appropriate startup operation...signature only
  // or quoted message and signature...
  if ( mComposeObj->QuotingToFollow() )
    return mComposeObj->BuildQuotedMessageAndSignature();
  else
    return mComposeObj->BuildBodyMessageAndSignature();
}

nsresult
nsMsgDocumentStateListener::NotifyDocumentWillBeDestroyed(void)
{
  if (mComposeObj)
    mComposeObj->m_editor = nsnull;	/* m_editor will be destroyed. Set it to null to */
									/* be sure we wont use it anymore. */
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
  nsAutoString    origBuf;

  rv = LoadDataFromFile(aSigFile, origBuf);
  if (NS_FAILED(rv))
    return rv;

  ConvertBufToPlainText(origBuf);
  aSigData = origBuf;
  return NS_OK;
}

nsresult
nsMsgCompose::ConvertTextToHTML(nsFileSpec& aSigFile, nsString &aSigData)
{
  nsresult    rv;
  nsAutoString    origBuf;

  rv = LoadDataFromFile(aSigFile, origBuf);
  if (NS_FAILED(rv))
    return rv;

  // Ok, once we are here, we need to escape the data to make sure that
  // we don't do HTML stuff with plain text sigs.
  //
  char *tString = origBuf.ToNewCString();
  if (tString)
  {
    char *escaped = nsEscapeHTML(tString);
    if (escaped) 
    {
      aSigData.AppendWithConversion(escaped);
      nsCRT::free(escaped);
    }
    else
    {
      aSigData.Append(origBuf);
    }

    nsCRT::free(tString);
  }
  else
  {
    aSigData.Append(origBuf);
  }

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

  sigData.AssignWithConversion(readBuf);
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
  // ...and that's not all! What we will also do now is look and see if
  // the file is an image file. If it is an image file, then we should 
  // insert the correct HTML into the composer to have it work, but if we
  // are doing plain text compose, we should insert some sort of message
  // saying "Image Signature Omitted" or something.
  //
  nsAutoString  urlStr;
  nsCOMPtr<nsIFileSpec> sigFileSpec;
  PRBool        useSigFile = PR_FALSE;
  PRBool        htmlSig = PR_FALSE;
  PRBool        imageSig = PR_FALSE;
  nsAutoString  sigData;

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
  char    *fileExt = nsMsgGetExtensionFromFileURL(NS_ConvertASCIItoUCS2(NS_STATIC_CAST(const char*, sigFilePath)));
  
  if ( (fileExt) && (*fileExt) )
  {
    // Now, most importantly, we need to figure out what the content type is for
    // this signature...if we can't, we assume text
    rv = NS_OK;
    char      *sigContentType = nsnull;
    NS_WITH_SERVICE(nsIMIMEService, mimeFinder, kMimeServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && mimeFinder && fileExt) 
    {
      mimeFinder->GetTypeFromExtension(fileExt, &(sigContentType));
      PR_FREEIF(fileExt);
    }
  
    if (sigContentType)
    {
      imageSig = (!PL_strncasecmp(sigContentType, "image/", 6));
      if (!imageSig)
        htmlSig = (!PL_strcasecmp(sigContentType, TEXT_HTML));
    }
    else
      htmlSig = ( (!PL_strcasecmp(fileExt, "HTM")) || (!PL_strcasecmp(fileExt, "HTML")) );

    PR_FREEIF(fileExt);
    PR_FREEIF(sigContentType);
  }
  
  // If we have an image signature, then we should put in the appropriate
  // HTML for inclusion, otherwise, just mention something about it.
  if (imageSig)
  {
    if (m_composeHTML)
    {
      aMsgBody->AppendWithConversion("<PRE>");
      aMsgBody->AppendWithConversion(CRLF);
      aMsgBody->AppendWithConversion("-- ");
      aMsgBody->AppendWithConversion(CRLF);
      aMsgBody->AppendWithConversion("</PRE>");
      aMsgBody->AppendWithConversion("<IMG SRC=\"file:///");
      aMsgBody->AppendWithConversion(testSpec);
      aMsgBody->AppendWithConversion("\" BORDER=0>");
    }
    else
    {
      nsCOMPtr<nsIMsgStringService> composebundle (do_GetService(NS_MSG_COMPOSESTRINGSERVICE_PROGID));
      nsXPIDLString omitString;
      composebundle->GetStringByID(NS_MSG_ATTACHMENT_TYPE_MISMATCH, getter_Copies(omitString));

      if (omitString)
      {
        aMsgBody->AppendWithConversion("-- ");
        aMsgBody->AppendWithConversion(CRLF);
        aMsgBody->Append(omitString);
        aMsgBody->AppendWithConversion(CRLF);
      }
    }

    return NS_OK;
  }
  else
  {
    // is this a text sig with an HTML editor?
    if ( (m_composeHTML) && (!htmlSig) )
      ConvertTextToHTML(testSpec, sigData);
    // is this a HTML sig with a text window?
    else if ( (!m_composeHTML) && (htmlSig) )
      ConvertHTMLToText(testSpec, sigData);
    else // We have a match...
      LoadDataFromFile(testSpec, sigData);  // Get the data!
  }

  // Now that sigData holds data...if any, append it to the body in a nice
  // looking manner
  //
  char      *htmlBreak = "<BR>";
  char      *dashes = "--";
  if (!sigData.IsEmpty())
  {
    if (m_composeHTML)
    {
      aMsgBody->AppendWithConversion(htmlBreak);
      if (!htmlSig)
      {
        aMsgBody->AppendWithConversion("<pre>");
        aMsgBody->AppendWithConversion(CRLF);
      }
    }
    else
      aMsgBody->AppendWithConversion(CRLF);
    
    aMsgBody->AppendWithConversion(dashes);

    if ( (!m_composeHTML) || ((m_composeHTML) && (!htmlSig)) )
      aMsgBody->AppendWithConversion(CRLF);
    else if (m_composeHTML)
      aMsgBody->AppendWithConversion(htmlBreak);
    
    aMsgBody->Append(sigData);

    if ( (m_composeHTML) && (!htmlSig) )
      aMsgBody->AppendWithConversion("</pre>");
  }

  return NS_OK;
}

nsresult
nsMsgCompose::BuildBodyMessageAndSignature()
{
  PRUnichar   *bod = nsnull;
  nsresult	  rv = NS_OK;

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

  /* Some time we want to add a signature and sometime we wont. Let's figure that now...*/
  PRBool addSignature;
  switch (mType)
  {
  	case nsIMsgCompType::New :
  	case nsIMsgCompType::Reply :				/* should not append! but just in case */
  	case nsIMsgCompType::ReplyAll :				/* should not append! but just in case */
  	case nsIMsgCompType::ForwardAsAttachment :	/* should not append! but just in case */
  	case nsIMsgCompType::ForwardInline :
  	case nsIMsgCompType::NewsPost :
  	case nsIMsgCompType::ReplyToGroup :
  	case nsIMsgCompType::ReplyToSenderAndGroup :
  		addSignature = PR_TRUE;
  		break;

  	case nsIMsgCompType::Draft :
  	case nsIMsgCompType::Template :
  		addSignature = PR_FALSE;
  		break;
  	
  	case nsIMsgCompType::MailToUrl :
  		addSignature = !(bod && *bod != 0);
  		break;

  	default :
  		addSignature = PR_FALSE;
  		break;
  }

  if (m_editor)
  {
    nsAutoString empty;
    nsAutoString bodStr(bod);
    nsAutoString tSignature;

    if (addSignature)
  	  ProcessSignature(m_identity, &tSignature);

    rv = ConvertAndLoadComposeWindow(m_editor, empty, bodStr, tSignature,
                                       PR_FALSE, m_composeHTML);
  }

  PR_FREEIF(bod);
  return rv;
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

nsresult nsMsgCompose::AttachmentPrettyName(const PRUnichar* url, PRUnichar** _retval)
{
	nsCAutoString unescapeURL; unescapeURL.AssignWithConversion(url);
	nsUnescape(unescapeURL);
	if (unescapeURL.IsEmpty())
	{
		*_retval = nsCRT::strdup(url);
		return NS_OK;
	}
	
	if (PL_strncasestr(unescapeURL, "file:", 5))
	{
		nsFileURL fileUrl(url);
		nsFileSpec fileSpec(fileUrl);
		char * leafName = fileSpec.GetLeafName();
		if (leafName && *leafName)
		{
    		nsAutoString tempStr;
    		nsresult rv = ConvertToUnicode(nsMsgI18NFileSystemCharset(), leafName, tempStr);
    		if (NS_FAILED(rv))
    			tempStr.AssignWithConversion(leafName);
			*_retval = tempStr.ToNewUnicode();
			nsCRT::free(leafName);
			return NS_OK;
		}
	}

	if (PL_strncasestr(unescapeURL, "http:", 5))
	{
		nsAutoString tempStr; tempStr.AssignWithConversion(&unescapeURL.mBuffer[7]);
		*_retval = tempStr.ToNewUnicode();
		return NS_OK;
	}

	*_retval = nsCRT::strdup(url);
	return NS_OK;
}

static nsresult OpenAddressBook(const char * dbUri, nsIAddrDatabase** aDatabase, nsIAbDirectory** aDirectory)
{
	if (!aDatabase || !aDirectory)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIAddressBook, addresBook, kAddrBookCID, &rv); 
  if (NS_SUCCEEDED(rv))
    rv = addresBook->GetAbDatabaseFromURI(dbUri, aDatabase);

	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if (NS_FAILED(rv)) 
		return rv;

	nsCOMPtr <nsIRDFResource> resource;
	rv = rdfService->GetResource(dbUri, getter_AddRefs(resource));
	if (NS_FAILED(rv)) 
		return rv;

	// query interface 
	rv = resource->QueryInterface(nsIAbDirectory::GetIID(), (void **)aDirectory);
	return rv;
}


nsresult nsMsgCompose::GetABDirectories(nsString& filePath, nsStringArray* directoriesArray, PRBool searchSubDirectory)
{
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr <nsIRDFResource> resource;
  char * strFileName = filePath.ToNewCString();
  rv = rdfService->GetResource(strFileName, getter_AddRefs(resource));
  Recycle(strFileName);
  if (NS_FAILED(rv)) return rv;

  // query interface 
  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
  if (NS_FAILED(rv)) return rv;

  if (!searchSubDirectory)
      return rv;
  
  nsCOMPtr<nsIEnumerator> subDirectories;
 	if (NS_SUCCEEDED(directory->GetChildNodes(getter_AddRefs(subDirectories))) && subDirectories)
	{
		nsCOMPtr<nsISupports> item;
		if (NS_SUCCEEDED(subDirectories->First()))
		{
		  do
		  {
        if (NS_SUCCEEDED(subDirectories->CurrentItem(getter_AddRefs(item))))
        {
          directory = do_QueryInterface(item, &rv);
          if (NS_SUCCEEDED(rv))
          {
            DIR_Server *server = nsnull;
            if (NS_SUCCEEDED(directory->GetServer(&server)) && server)
            {
              nsAutoString subFileName(filePath);
              if (subFileName.Last() != '/')
                  subFileName.AppendWithConversion("/");
              subFileName.AppendWithConversion(server->fileName);

              PRInt32 pos;
              if (subFileName.CompareWithConversion(kPersonalAddressbookUri) == 0)
                 pos = 0;
              else
                if (subFileName.CompareWithConversion(kCollectedAddressbookUri) == 0)
                  pos = directoriesArray->Count();
                else
                {
                  pos = directoriesArray->Count() - 1;
                  if (pos < 0)
                      pos = 0;
                }
              directoriesArray->InsertStringAt(subFileName, pos);
              rv = GetABDirectories(subFileName, directoriesArray, PR_TRUE);
            }
          }
        }
      } while (NS_SUCCEEDED(subDirectories->Next()));
    }
  }
  return rv;
}


nsresult nsMsgCompose::GetNoHtmlRecipients(const PRUnichar *recipients, PRUnichar **_retval)
{
    nsresult rv = NS_OK;
    *_retval = nsnull;
    PRInt32 j;
    PRInt32 i;
    PRInt32 nbrRecipients;
    nsXPIDLString emailAddr;
    
    nsAutoString recipientStr;
    if (recipients != nsnull)
        recipientStr = recipients;
    else
    {
        recipientStr.AppendWithConversion(m_compFields->GetTo());
        recipientStr.AppendWithConversion(',');
        recipientStr.AppendWithConversion(m_compFields->GetCc());
        recipientStr.AppendWithConversion(',');
        recipientStr.AppendWithConversion(m_compFields->GetBcc());
    }
    
    /*ducarroz: for now, I've hardcoded the addressbook DBs we are looking in it, will do much better later! */
    PRInt32 nbrOfAddrbook = 2;
    const char addrbookName[2][20] = {"abook.mab", "history.mab"};

    nsCOMPtr<nsIMsgRecipientArray> array;
    nsCOMPtr<nsIMsgRecipientArray> noHTMLArray;
    rv = nsComponentManager::CreateInstance(kMsgRecipientArrayCID, 
                                          NULL, NS_GET_IID(nsIMsgRecipientArray), 
                                          (void **) getter_AddRefs(noHTMLArray));
    if (NS_FAILED(rv))
        return rv;

    rv = m_compFields->SplitRecipients(recipientStr.GetUnicode(), PR_TRUE, getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
    	nsCOMPtr<nsIAddrDatabase> abDataBase;
        nsCOMPtr<nsIAbDirectory> abDirectory;   
    	nsCOMPtr <nsIAbCard> existingCard;

      nsStringArray* addrbookDirArray = (nsStringArray*) new nsStringArray;
      if (addrbookDirArray)
      {
        nsString dirPath;
        nsAutoString root; root.AssignWithConversion(kDirectoryRoot);
        GetABDirectories(root, addrbookDirArray, TRUE);

       for (j = 0; j < addrbookDirArray->Count(); j++)
        {
            array->GetCount(&nbrRecipients);
            if (nbrRecipients == 0)
                break;

            addrbookDirArray->StringAt(j, dirPath);
            char * dirPathC = dirPath.ToNewCString();
            rv = OpenAddressBook(dirPathC, getter_AddRefs(abDataBase), getter_AddRefs(abDirectory));
            Recycle(dirPathC);
            if (NS_FAILED(rv) || !abDataBase || !abDirectory)
                continue;
            
            for (i = 0; i < nbrRecipients; i ++)
            {
                rv = array->StringAt(i, getter_Copies(emailAddr));
                if (NS_FAILED(rv))
                    continue;
                nsCAutoString emailStr; emailStr.AssignWithConversion(emailAddr);

    			      rv = abDataBase->GetCardForEmailAddress(abDirectory, emailStr, getter_AddRefs(existingCard));
    			      if (NS_SUCCEEDED(rv) && existingCard)
    			      {
    			        PRBool bPlainText;
    			        rv = existingCard->GetSendPlainText(&bPlainText);
    			        if (NS_SUCCEEDED(rv))
    			        {
                        PRBool aBool;
    			          if (bPlainText)
    			          {
    			            //this guy doesn't want/support HTML message, move it in the noHTML array.
    			            noHTMLArray->AppendString(emailAddr, &aBool);
    			          }
    			          array->RemoveStringAt(i, &aBool);
            			  if (aBool)
            			  {
            			    nbrRecipients --;
            			    i --;
            			  }
    			        }
    			      }
            }
            if (abDataBase)
                abDataBase->Close(PR_FALSE);            
        }
        delete addrbookDirArray;
     }
    }
    //now, build the result
    recipientStr.SetLength(0);
    noHTMLArray->GetCount(&nbrRecipients);
    for (i = 0; i < nbrRecipients; i ++)
    {
        if (! recipientStr.IsEmpty())
            recipientStr.AppendWithConversion(',');
        noHTMLArray->StringAt(i, getter_Copies(emailAddr));
        recipientStr.Append(emailAddr);
    }
    //Remaining recipients which do not have an entry in the AB are considered as non HTML compliant
    array->GetCount(&nbrRecipients);
    for (i = 0; i < nbrRecipients; i ++)
    {
        if (! recipientStr.IsEmpty())
            recipientStr.AppendWithConversion(',');
        array->StringAt(i, getter_Copies(emailAddr));
        recipientStr.Append(emailAddr);
    }
    *_retval = recipientStr.ToNewUnicode();
    
    return NS_OK;
}

nsresult nsMsgCompose::GetNoHtmlNewsgroups(const PRUnichar *newsgroups, PRUnichar **_retval)
{
    //FIX ME: write me
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    *_retval = nsnull;
    return rv;
}

nsresult nsMsgCompose::ResetNodeEventHandlers(nsIDOMNode *node)
{
	// Because event handler attributes set into a node before this node is inserted 
	// into the DOM are not recognised (in fact not compiled), we need to parsed again
	// the whole node and reset event handlers.
    
    nsresult rv;
    nsAutoString aStr;
    PRUint32 i;
    PRUint32 nbrOfElements;
    nsCOMPtr<nsIDOMNode> pItem;

    if (nsnull == node)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIDOMNamedNodeMap> pAttributes;
    rv = node->GetAttributes(getter_AddRefs(pAttributes));
    if (NS_SUCCEEDED(rv) && pAttributes)
    {
        rv = pAttributes->GetLength(&nbrOfElements);
        if (NS_FAILED(rv))
            return rv;

        for (i = 0; i < nbrOfElements; i ++)
        {
            rv = pAttributes->Item(i, getter_AddRefs(pItem));
            if (NS_SUCCEEDED(rv) && pItem)
            {
                rv = pItem->GetNodeName(aStr);
                if (NS_SUCCEEDED(rv))
                {
                    if (aStr.Find("on", PR_FALSE, 0, 2) == 0)   //name start with "on"
                    {
                        rv = pItem->GetNodeValue(aStr);
                        if (NS_SUCCEEDED(rv))
                            rv = pItem->SetNodeValue(aStr);
                        //Do not abort if it failed, let do the next one...
                    }
                }
            }
        }
        
        PRBool hasChild;
        rv = node->HasChildNodes(&hasChild);
        if (NS_SUCCEEDED(rv) && hasChild)
        {
            nsCOMPtr<nsIDOMNodeList> children;
            rv = node->GetChildNodes(getter_AddRefs(children));
            if (NS_SUCCEEDED(rv) && children)
            {
                rv = children->GetLength(&nbrOfElements);
                if (NS_FAILED(rv))
                    return rv;
                
                for (i = 0; i < nbrOfElements; i ++)
                {
                    rv = children->Item(i, getter_AddRefs(pItem));
                    if (NS_SUCCEEDED(rv) && pItem)
                       ResetNodeEventHandlers(pItem);
                    //Do not abort if it failed, let do the next one...
                }
            }
        }
    }
    
    return rv;
}

