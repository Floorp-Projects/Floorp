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
 *     Ben Bucksch <mozilla@bucksch.org>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */

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
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsISupportsArray.h"
#include "nsIIOService.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsIPrompt.h"

// Defines....
static NS_DEFINE_CID(kMsgQuoteCID, NS_MSGQUOTE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgRecipientArrayCID, NS_MSGRECIPIENTARRAY_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

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

  // For TagConvertible
  // Read and cache pref
  mConvertStructs = PR_FALSE;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
    prefs->GetBoolPref("converter.html2txt.structs", &mConvertStructs);

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
        aEditorShell->InsertAsCitedQuotation(aBuf.GetUnicode(),
                               mCiteReference.GetUnicode(),
                               PR_TRUE,
                               NS_ConvertASCIItoUCS2("UTF-8").GetUnicode(),
                               getter_AddRefs(nodeInserted));
      else
        aEditorShell->InsertAsQuotation(aBuf.GetUnicode(),
                                        getter_AddRefs(nodeInserted));
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

nsresult nsMsgCompose::Initialize(nsIDOMWindowInternal *aWindow,
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
    nsCOMPtr<nsIDocShell> docshell;
		nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(aWindow));
		if (!globalObj)
			return NS_ERROR_FAILURE;
		
		globalObj->GetDocShell(getter_AddRefs(docshell));
    nsCOMPtr<nsIDocShellTreeItem>  treeItem(do_QueryInterface(docshell));
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    rv = treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
    if (NS_FAILED(rv)) return rv;

    m_baseWindow = do_QueryInterface(treeOwner);
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

nsresult nsMsgCompose::_SendMsg(MSG_DeliverMode deliverMode, nsIMsgIdentity *identity)
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

            if ( (deliverMode == nsIMsgCompDeliverMode::Now) ||
                 (deliverMode == nsIMsgCompDeliverMode::Later) )  
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
      PRUint32 listeners;
      nsIMsgSendListener **tArray = m_sendListener->CreateListenerArray(&listeners);
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
                    tArray, listeners);           // listener array
      
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
    // rhp:
    // We shouldn't close the window if we are just saving a draft or a template
    // so do this check
    if ( (deliverMode != nsIMsgSend::nsMsgSaveAsDraft) &&
         (deliverMode != nsIMsgSend::nsMsgSaveAsTemplate) )
      ShowWindow(PR_FALSE);
  }
  else
    NotifyStateListeners(eSaveAndSendProcessDone);

		
  return rv;
}

nsresult nsMsgCompose::SendMsg(MSG_DeliverMode deliverMode,  nsIMsgIdentity *identity)
{
	nsresult rv = NS_OK;

  // i'm assuming the compose window is still up at this point...
  nsCOMPtr<nsIPrompt> prompt;
  if (m_window)
     m_window->GetPrompter(getter_AddRefs(prompt));

	if (m_editor && m_compFields && !m_composeHTML)
	{
    // The plain text compose window was used
    const char contentType[] = "text/plain";
		nsAutoString msgBody;
		PRUnichar *bodyText = NULL;
    nsAutoString format; format.AssignWithConversion(contentType);
    PRUint32 flags = nsIDocumentEncoder::OutputFormatted;

    const char *charset = m_compFields->GetCharacterSet();
    if(UseFormatFlowed(charset))
        flags |= nsIDocumentEncoder::OutputFormatFlowed;
    
    if (!mEntityConversionDone)
    {
      rv = m_editor->GetContentsAs(format.GetUnicode(), flags, &bodyText);
		  
      if (NS_SUCCEEDED(rv) && NULL != bodyText)
      {
		    msgBody = bodyText;
        nsMemory::Free(bodyText);

		    // Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		    char *outCString = NULL;
        rv = nsMsgI18NSaveAsCharset(contentType, m_compFields->GetCharacterSet(), 
                                    msgBody.GetUnicode(), &outCString);
		    if (NS_SUCCEEDED(rv) && NULL != outCString) 
		    {
          // body contains multilingual data, confirm send to the user
          if (NS_ERROR_UENC_NOMAPPING == rv) {
            PRBool proceedTheSend;
            rv = nsMsgAskBooleanQuestionByID(prompt, NS_MSG_MULTILINGUAL_SEND, &proceedTheSend);
            if (!proceedTheSend) {
              PR_FREEIF(outCString);
              return NS_ERROR_BUT_DONT_SHOW_ALERT;
            }
          }

          if ( (deliverMode == nsIMsgCompDeliverMode::Now) ||
               (deliverMode == nsIMsgCompDeliverMode::Later) )
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

  rv = _SendMsg(deliverMode, identity);
	if (NS_FAILED(rv))
	{
		ShowWindow(PR_TRUE);
    if (rv != NS_ERROR_BUT_DONT_SHOW_ALERT)
		nsMsgDisplayMessageByID(prompt, rv);
	}
	
	return rv;
}


nsresult
nsMsgCompose::SendMsgEx(MSG_DeliverMode deliverMode,
                        nsIMsgIdentity *identity,
                        const PRUnichar *addrTo, const PRUnichar *addrCc,
                        const PRUnichar *addrBcc, const PRUnichar *newsgroup,
                        const PRUnichar *subject, const PRUnichar *body)
{
	nsresult rv = NS_OK;

	if (m_compFields && identity) 
	{ 
		nsAutoString aCharset; aCharset.AssignWithConversion(msgCompHeaderInternalCharset());
		char *outCString;

		// Convert fields to UTF-8
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, nsAutoString(addrTo), &outCString))) 
		{
			m_compFields->SetTo(outCString);
			PR_Free(outCString);
		}
		else 
		{
		  nsCAutoString addrToCStr; addrToCStr.AssignWithConversion(addrTo);
			m_compFields->SetTo(addrToCStr);
	  }

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, nsAutoString(addrCc), &outCString))) 
		{
			m_compFields->SetCc(outCString);
			PR_Free(outCString);
		}
		else
		{
      nsCAutoString addrCcCStr; addrCcCStr.AssignWithConversion(addrCc);
      m_compFields->SetCc(addrCcCStr);
    }

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, nsAutoString(addrBcc), &outCString))) 
		{
			m_compFields->SetBcc(outCString);
			PR_Free(outCString);
		}
		else 
		{
      nsCAutoString addrBccCStr; addrBccCStr.AssignWithConversion(addrBcc);
      m_compFields->SetBcc(addrBccCStr);
    }

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, nsAutoString(newsgroup), &outCString))) 
		{
			m_compFields->SetNewsgroups(outCString);
			PR_Free(outCString);
		}
		else 
		{
      nsCAutoString newsgroupCStr; newsgroupCStr.AssignWithConversion(newsgroup);
      m_compFields->SetNewsgroups(newsgroupCStr);
    }
        
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, nsAutoString(subject), &outCString))) 
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
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, nsAutoString(body), &outCString))) 
		{
			m_compFields->SetBody(outCString);
			PR_Free(outCString);
		}
		else
		{
      nsCAutoString bodyCStr; bodyCStr.AssignWithConversion(body);
      m_compFields->SetBody(bodyCStr);
    }

		rv = _SendMsg(deliverMode, identity);
	}
	else
		rv = NS_ERROR_NOT_INITIALIZED;

	if (NS_FAILED(rv))
	{
		ShowWindow(PR_TRUE);
    if (rv != NS_ERROR_BUT_DONT_SHOW_ALERT)
    {
      // i'm assuming the compose window is still up at this point...
      nsCOMPtr<nsIPrompt> prompt;
      if (m_window)
        m_window->GetPrompter(getter_AddRefs(prompt));
			nsMsgDisplayMessageByID(prompt, rv);
    }
	}
	return rv;
}

nsresult nsMsgCompose::CloseWindow()
{
    nsresult rv = NS_OK;
    if (m_baseWindow) {
        m_editor = nsnull;	      /* m_editor will be destroyed during the Close Window. Set it to null to */
							      /* be sure we wont use it anymore. */

        nsIBaseWindow * aWindow = m_baseWindow;
        m_baseWindow = nsnull;
        rv = aWindow->Destroy();              
    }

    // Need to relelase the mComposeObj...
    mMsgSend = nsnull;
  	return rv;
}

nsresult nsMsgCompose::ShowWindow(PRBool show)
{
	if (m_baseWindow) {
        return m_baseWindow->SetVisibility(show);
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

nsresult nsMsgCompose::GetDomWindow(nsIDOMWindowInternal * *aDomWindow)
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
      nsXPIDLCString charset;

      char *aCString = nsnull;
    
      rv = message->GetCharset(getter_Copies(charset));

      aCharset.AssignWithConversion(charset);

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

          // HACK: if we are replying to a message and that message used a charset over ride
          // (as speciifed in the top most window (assuming the reply originated from that window)
          // then use that over ride charset instead of the charset specified in the message
	        nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_PROGID);          
          if (mailSession)
          {
            nsCOMPtr<nsIMsgWindow>    msgWindow;
          	mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
            if (msgWindow)
            {
              nsXPIDLString mailCharset;
              msgWindow->GetMailCharacterSet(getter_Copies(mailCharset));
              if (mailCharset && (* (const PRUnichar *) mailCharset) )
                aCharset = mailCharset;
            }
          }
          
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
}

QuotingOutputStreamListener::QuotingOutputStreamListener(const PRUnichar * originalMsgURI,
                                                         PRBool quoteHeaders,
                                                         nsIMsgIdentity *identity) 
{ 
  mQuoteHeaders = quoteHeaders;
  mIdentity = identity;

  // For the built message body...

  nsCOMPtr<nsIMessage> originalMsg = getter_AddRefs(GetIMessageFromURI(originalMsgURI));
  if (originalMsg && !quoteHeaders)
  {
    nsresult rv;

    // Setup the cite information....
    nsXPIDLCString myGetter;
    if (NS_SUCCEEDED(originalMsg->GetMessageId(getter_Copies(myGetter))))
    {
      nsCString unencodedURL(myGetter);
           // would be nice, if nsXPIDL*String were ns*String
      NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv)
      if (!unencodedURL.IsEmpty() && NS_SUCCEEDED(rv) && serv)
      {
        if (NS_SUCCEEDED(serv->Escape(unencodedURL.GetBuffer(),
                 nsIIOService::url_FileBaseName | nsIIOService::url_Forced,
                 getter_Copies(myGetter))))
        {
          mCiteReference.AssignWithConversion(myGetter);
          mCiteReference.Insert(NS_LITERAL_STRING("mid:"), 0);
        }
        else
          mCiteReference.Truncate();
      }
    }

    if (GetReplyOnTop() == 1) 
      mCitePrefix += NS_LITERAL_STRING("<br><br>");

    nsXPIDLString author;
    rv = originalMsg->GetMime2DecodedAuthor(getter_Copies(author));
    if (NS_SUCCEEDED(rv))
    {
      char * authorName = nsnull;
      nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);

      if (parser)
      {
        nsAutoString aCharset;
        aCharset.AssignWithConversion(msgCompHeaderInternalCharset());
        char * utf8Author = nsnull;
        nsAutoString authorStr; authorStr.Assign(author);
        rv = ConvertFromUnicode(aCharset, authorStr, &utf8Author);
        if (NS_SUCCEEDED(rv) && utf8Author)
        {
          nsCAutoString acharsetC;
          acharsetC.AssignWithConversion(aCharset);
          rv = parser->ExtractHeaderAddressName(acharsetC, utf8Author,
                                                &authorName);
          if (NS_SUCCEEDED(rv))
            rv = ConvertToUnicode(aCharset, authorName, authorStr);
        }
        else
        {
          nsCAutoString authorCStr;
          authorCStr.AssignWithConversion(author);
          rv = parser->ExtractHeaderAddressName(nsnull, authorCStr,
                                                &authorName);
          if (NS_SUCCEEDED(rv))
            authorStr.AssignWithConversion(authorName);
        }
        PR_FREEIF(utf8Author);
        if (authorName)
          PL_strfree(authorName);

        if (NS_SUCCEEDED(rv) && !authorStr.IsEmpty())
          mCitePrefix.Append(authorStr);
        else
          mCitePrefix.Append(author);
        mCitePrefix.AppendWithConversion(" wrote:<br><html>");  //XXX I18n?
      }
    }
  }

  if (mCitePrefix.IsEmpty())
    mCitePrefix.AppendWithConversion("<br><br>--- Original Message ---<br><html>");  //XXX I18n?
  
  NS_INIT_REFCNT(); 
}

/**
 * The formatflowed parameter directs if formatflowed should be used in the conversion.
 * format=flowed (RFC 2646) is a way to represent flow in a plain text mail, without
 * disturbing the plain text.
 */
nsresult
QuotingOutputStreamListener::ConvertToPlainText(PRBool formatflowed /* = PR_FALSE */)
{
  nsresult  rv = NS_OK;

  rv += ConvertBufToPlainText(mCitePrefix, formatflowed);
  rv += ConvertBufToPlainText(mMsgBody, formatflowed);
  rv += ConvertBufToPlainText(mSignature, formatflowed);
  return rv;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStartRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */)
{
	return NS_OK;
}

NS_IMETHODIMP QuotingOutputStreamListener::OnStopRequest(nsIChannel *aChannel, nsISupports * /* ctxt */, nsresult status, const PRUnichar * /* errorMsg */)
{
  nsresult rv = NS_OK;
  nsAutoString aCharset;
  
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
        aCharset.AssignWithConversion(msgCompHeaderInternalCharset());
        nsAutoString recipient;
        nsAutoString cc;
        nsAutoString replyTo;
        nsAutoString newgroups;
        nsAutoString followUpTo;
        nsAutoString messageId;
        nsAutoString references;
        char *outCString = nsnull;
        PRUnichar emptyUnichar = 0;
        PRBool needToRemoveDup = PR_FALSE;
        
        if (type == nsIMsgCompType::ReplyAll)
        {
          mHeaders->ExtractHeader(HEADER_TO, PR_TRUE, &outCString);
          if (outCString)
          {
            // Convert fields from UTF-8
            ConvertToUnicode(aCharset, outCString, recipient);
            PR_FREEIF(outCString);
          }
              
          mHeaders->ExtractHeader(HEADER_CC, PR_TRUE, &outCString);
          if (outCString)
          {
            // Convert fields from UTF-8
            ConvertToUnicode(aCharset, outCString, cc);
            PR_FREEIF(outCString);
          }
              
          if (recipient.Length() > 0 && cc.Length() > 0)
            recipient.AppendWithConversion(", ");
          recipient += cc;
          compFields->SetCc(recipient.GetUnicode());

          needToRemoveDup = PR_TRUE;
        }
              
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
          needToRemoveDup = PR_TRUE;
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
        
        if (needToRemoveDup)
        {
          //Remove duplicate addresses between TO && CC
          char * resultStr;
          nsMsgCompFields* _compFields = (nsMsgCompFields*)compFields;
          if (NS_SUCCEEDED(rv))
          {
            nsCString addressToBeRemoved(_compFields->GetTo());
            if (mIdentity)
		        {
		          nsXPIDLCString email;
		          mIdentity->GetEmail(getter_Copies(email));
		          addressToBeRemoved += ", ";
		          addressToBeRemoved += NS_CONST_CAST(char*, (const char *)email);
					  }

            rv= RemoveDuplicateAddresses(_compFields->GetCc(), (char *)addressToBeRemoved, PR_TRUE, &resultStr);
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
    
    mMsgBody.AppendWithConversion("</html>");
    
    // Now we have an HTML representation of the quoted message.
    // If we are in plain text mode, we need to convert this to plain
    // text before we try to insert it into the editor. If we don't, we
    // just get lots of HTML text in the message...not good.
    //
    // XXX not m_composeHTML? /BenB
    PRBool composeHTML = PR_TRUE;
    mComposeObj->GetComposeHTML(&composeHTML);
    if (!composeHTML)
    {
      // Downsampling. The charset should only consist of ascii.
      char *target_charset = aCharset.ToNewCString();
      PRBool formatflowed = UseFormatFlowed(target_charset);
      ConvertToPlainText(formatflowed);
      Recycle(target_charset);
    }

    mComposeObj->ProcessSignature(mIdentity, &mSignature);
    
    nsIEditorShell *editor = nsnull;
    if (NS_SUCCEEDED(mComposeObj->GetEditor(&editor)) && editor)
    {
      mComposeObj->ConvertAndLoadComposeWindow(editor, mCitePrefix, mMsgBody, mSignature, PR_TRUE, composeHTML);
    }
  }

  mComposeObj = null_nsCOMPtr();	//We are done with it, therefore release it.
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

  nsXPIDLString msgCharSet;
  m_compFields->GetCharacterSet(getter_Copies(msgCharSet));

  rv = mQuote->QuoteMessage(originalMsgURI, what != 1, mQuoteStreamListener, msgCharSet);
  return rv;
}

//CleanUpRecipient will remove un-necesary "<>" when a recipient as an address without name
void nsMsgCompose::CleanUpRecipients(nsString& recipients)
{
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

nsIMsgSendListener **
nsMsgComposeSendListener::CreateListenerArray(PRUint32 *aListeners)
{
  nsIMsgSendListener **tArray = (nsIMsgSendListener **)PR_Malloc(sizeof(nsIMsgSendListener *) * 2);
  if (!tArray)
    return nsnull;
  nsCRT::memset(tArray, 0, sizeof(nsIMsgSendListener *) * 2);
  tArray[0] = this;
  *aListeners = 2;
  return tArray;
}

NS_IMPL_QUERY_INTERFACE2(nsMsgComposeSendListener,
                         nsIMsgSendListener,
                         nsIMsgCopyServiceListener)

nsMsgComposeSendListener::nsMsgComposeSendListener(void) 
{ 
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
			    {
			      mComposeObj->NotifyStateListeners(nsMsgCompose::eSaveAndSendProcessDone);
            mComposeObj->CloseWindow();
          }
        }
      }
      else
      {
			  mComposeObj->NotifyStateListeners(nsMsgCompose::eSaveAndSendProcessDone);
        mComposeObj->CloseWindow();  // if we fail on the simple GetFcc call, close the window to be safe and avoid
                                     // windows hanging around to prevent the app from exiting.
      }

      NS_IF_RELEASE(compFields);
		}
		else
		{
#ifdef NS_DEBUG
			printf("nsMsgComposeSendListener: the message send operation failed!\n");
#endif
			mComposeObj->NotifyStateListeners(nsMsgCompose::eSaveAndSendProcessDone);
			mComposeObj->ShowWindow(PR_TRUE);

      // Need to relelase the mComposeObj...
      mComposeObj->mMsgSend = nsnull;
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
			mComposeObj->NotifyStateListeners(nsMsgCompose::eSaveAndSendProcessDone);
      // We should only close the window if we are done. Things like templates
      // and drafts aren't done so their windows should stay open
      if ( (mDeliverMode != nsIMsgSend::nsMsgSaveAsDraft) &&
           (mDeliverMode != nsIMsgSend::nsMsgSaveAsTemplate) )
        mComposeObj->CloseWindow();
		}
		else
		{
#ifdef NS_DEBUG
			printf("nsMsgComposeSendListener: the message copy operation failed!\n");
#endif
			mComposeObj->NotifyStateListeners(nsMsgCompose::eSaveAndSendProcessDone);
			mComposeObj->ShowWindow(PR_TRUE);

      // Need to relelase the mComposeObj...
      mComposeObj->mMsgSend = nsnull;
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

  ConvertBufToPlainText(origBuf,PR_FALSE);
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
  PRUnichar *escaped = nsEscapeHTML2(origBuf.GetUnicode());
  if (escaped) 
  {
    aSigData.Append(escaped);
    nsCRT::free(escaped);
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

  if (NS_FAILED(ConvertToUnicode(nsMsgI18NFileSystemCharset(), readBuf, sigData)))
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
  nsAutoString sigOutput;

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
    nsCOMPtr<nsIMIMEService> mimeFinder (do_GetService(NS_MIMESERVICE_PROGID, &rv));
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

  static const char      htmlBreak[] = "<BR>";
  static const char      dashes[] = "-- ";
  static const char      htmlsigopen[] = "<div class=\"moz-signature\">";
  static const char      htmlsigclose[] = "</div>";    /* XXX: Due to a bug in
                             4.x' HTML editor, it will not be able to
                             break this HTML sig, if quoted (for the user to
                             interleave a comment). */
  static const char      preopen[] = "<pre class=\"moz-signature\">";
  static const char      preclose[] = "</pre>";

  if (imageSig)
  {
    // We have an image signature. If we're using the in HTML composer, we
    // should put in the appropriate HTML for inclusion, otherwise, do nothing.
    if (m_composeHTML)
    {
      sigOutput.AppendWithConversion(htmlBreak);
      sigOutput.AppendWithConversion(htmlsigopen);
      sigOutput.AppendWithConversion(dashes);
      sigOutput.AppendWithConversion(htmlBreak);
      sigOutput.AppendWithConversion("<img src=\"file:///");
           /* XXX pp This gives me 4 slashes on Unix, that's at least one to
              much. Better construct the URL with some service. */
      sigOutput.AppendWithConversion(testSpec);
      sigOutput.AppendWithConversion("\" border=0>");
      sigOutput.AppendWithConversion(htmlsigclose);
    }
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
  if (!sigData.IsEmpty())
  {
    if (m_composeHTML)
    {
      sigOutput.AppendWithConversion(htmlBreak);
      if (htmlSig)
        sigOutput.AppendWithConversion(htmlsigopen);
      else
        sigOutput.AppendWithConversion(preopen);
    }
    else
      sigOutput.AppendWithConversion(CRLF);

    sigOutput.AppendWithConversion(dashes);

    if ( (!m_composeHTML) || ((m_composeHTML) && (!htmlSig)) )
      sigOutput.AppendWithConversion(CRLF);
    else if (m_composeHTML)
      sigOutput.AppendWithConversion(htmlBreak);

    sigOutput.Append(sigData);
         //DELETEME: I18N: Converting down (2byte->1byte) OK?

    if (m_composeHTML)
    {
      if (htmlSig)
        sigOutput.AppendWithConversion(htmlsigclose);
      else
        sigOutput.AppendWithConversion(preclose);
    }
  }

  aMsgBody->Append(sigOutput);
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
  for (i = 0; i < numListeners;i++)
  {
    nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mStateListeners->ElementAt(i));
    nsCOMPtr<nsIMsgComposeStateListener> thisListener = do_QueryInterface(iSupports);
    if (thisListener)
    {
      switch (aNotificationType)
      {
        case eComposeFieldsReady:
          thisListener->NotifyComposeFieldsReady();
          break;
        
        case eSaveAndSendProcessDone:
          thisListener->SendAndSaveProcessDone();
          break;
        
        default:
          NS_NOTREACHED("Unknown notification");
          break;
      }
    }
  }

  return NS_OK;
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
	  nsAutoString urlString(url);
		nsFileURL fileUrl(urlString);
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
		unescapeURL.Cut(0, 7);

	*_retval = unescapeURL.ToNewUnicode();
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


nsresult nsMsgCompose::GetABDirectories(char * dirUri, nsISupportsArray* directoriesArray, PRBool searchSubDirectory)
{
  static PRBool collectedAddressbookFound;
  if (nsCRT::strcmp(dirUri, kDirectoryRoot) == 0)
    collectedAddressbookFound = PR_FALSE;

  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(dirUri, getter_AddRefs(resource));
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
            PRBool bIsMailList;

            if (NS_SUCCEEDED(directory->GetIsMailList(&bIsMailList)) && bIsMailList)
              continue;

            char* uri;
            rv = directory->GetDirUri(&uri);
            if (NS_FAILED(rv))
              return rv;

            PRInt32 pos;
            if (nsCRT::strcmp(uri, kPersonalAddressbookUri) == 0)
              pos = 0;
            else
            {
              PRUint32 count = 0;
              directoriesArray->Count(&count);

              if (PL_strcmp(uri, kCollectedAddressbookUri) == 0)
              {
                collectedAddressbookFound = PR_TRUE;
                pos = count;
              }
              else
              {
                if (collectedAddressbookFound && count > 1)
                  pos = count - 1;
                else
                  pos = count;
              }
            }

            directoriesArray->InsertElementAt(directory, pos);
            rv = GetABDirectories(uri, directoriesArray, PR_TRUE);

            PR_Free(uri);
          }
        }
      } while (NS_SUCCEEDED(subDirectories->Next()));
    }
  }
  return rv;
}

nsresult nsMsgCompose::BuildMailListArray(nsIAddrDatabase* database, nsIAbDirectory* parentDir, nsISupportsArray* array)
{
  nsresult rv;

  nsCOMPtr<nsIAbDirectory> directory;
  nsCOMPtr<nsIEnumerator> subDirectories;

 	if (NS_SUCCEEDED(parentDir->GetChildNodes(getter_AddRefs(subDirectories))) && subDirectories)
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
            PRBool bIsMailList;

            if (NS_SUCCEEDED(directory->GetIsMailList(&bIsMailList)) && bIsMailList)
            {
              nsXPIDLString listName;
              nsXPIDLString listDescription;

              directory->GetListName(getter_Copies(listName));
              directory->GetDescription(getter_Copies(listDescription));

              nsMsgMailList* mailList = new nsMsgMailList(nsAutoString((const PRUnichar*)listName),
                    nsAutoString((const PRUnichar*)listDescription), directory);
              if (!mailList)
                return NS_ERROR_OUT_OF_MEMORY;
              NS_ADDREF(mailList);

              rv = array->AppendElement(mailList);
              if (NS_FAILED(rv))
                return rv;

              NS_RELEASE(mailList);
            }
          }
        }
      } while (NS_SUCCEEDED(subDirectories->Next()));
    }
  }
  return rv;
}


nsresult nsMsgCompose::GetMailListAddresses(nsString& name, nsISupportsArray* mailListArray, nsISupportsArray** addressesArray)
{
  nsresult rv;
  nsCOMPtr<nsIEnumerator> enumerator;

  rv = mailListArray->Enumerate(getter_AddRefs(enumerator));
  if (NS_SUCCEEDED(rv))
  {
    for (rv = enumerator->First(); NS_SUCCEEDED(rv); rv = enumerator->Next())
    {
      nsMsgMailList* mailList;
      rv = enumerator->CurrentItem((nsISupports**)&mailList);
      if (NS_SUCCEEDED(rv) && mailList)
      {
        if (name.EqualsIgnoreCase(mailList->mFullName))
        {
          if (!mailList->mDirectory)
            return NS_ERROR_FAILURE;

          mailList->mDirectory->GetAddressLists(addressesArray);
          NS_RELEASE(mailList);
          return NS_OK;
        }
        NS_RELEASE(mailList);
      }
    }
  }

  return NS_ERROR_FAILURE;
}


nsresult nsMsgCompose::CheckAndPopulateRecipients(PRBool populateMailList, PRBool returnNoHTMLRecipients, PRUnichar **_retval)
{
	#define MAX_OF_RECIPIENT_ARRAY		3

  if (!populateMailList && (!returnNoHTMLRecipients || !_retval))
    return NS_ERROR_INVALID_ARG;

  nsresult rv = NS_OK;
  if (_retval)
    *_retval = nsnull;
  PRInt32 i;
  PRInt32 j;
  PRInt32 k;

  /* First, build an array with original recipients */
  nsCOMPtr<nsISupportsArray> recipientsList[MAX_OF_RECIPIENT_ARRAY];

	PRUnichar* originalRecipients[MAX_OF_RECIPIENT_ARRAY];
	m_compFields->GetTo(&originalRecipients[0]);
	m_compFields->GetCc(&originalRecipients[1]);
	m_compFields->GetBcc(&originalRecipients[2]);

  nsCOMPtr<nsIMsgRecipientArray> addressArray;
  nsCOMPtr<nsIMsgRecipientArray> emailArray;
	for (i = 0; i < MAX_OF_RECIPIENT_ARRAY; i ++)
	{
		rv = m_compFields->SplitRecipientsEx(originalRecipients[i], getter_AddRefs(addressArray), getter_AddRefs(emailArray));
		if (NS_SUCCEEDED(rv))
		{
      PRInt32 nbrRecipients;
      nsXPIDLString emailAddr;
      nsXPIDLString addr;
			addressArray->GetCount(&nbrRecipients);

      rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_PROGID, nsnull, 
                  NS_GET_IID(nsISupportsArray), getter_AddRefs(recipientsList[i]));
      if (NS_FAILED(rv))
        return rv;

      for (j = 0; j < nbrRecipients; j ++)
      {
        rv = addressArray->StringAt(j, getter_Copies(addr));
        if (NS_FAILED(rv))
          return rv;

        rv = emailArray->StringAt(j, getter_Copies(emailAddr));
        if (NS_FAILED(rv))
          return rv;

        nsMsgRecipient* recipient = new nsMsgRecipient(nsAutoString(addr), nsAutoString(emailAddr));
        if (!recipient)
           return  NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(recipient);

        rv = recipientsList[i]->AppendElement(recipient);
        NS_RELEASE(recipient);
        if (NS_FAILED(rv))
          return rv;
       }
		}
    else
      return rv;
	}

  /* Then look them up in the Addressbooks*/

  PRBool stillNeedToSearch = PR_TRUE;
  nsCOMPtr<nsIAddrDatabase> abDataBase;
  nsCOMPtr<nsIAbDirectory> abDirectory;   
  nsCOMPtr <nsIAbCard> existingCard;
  nsCOMPtr <nsISupportsArray> mailListAddresses;
  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);
  nsCOMPtr<nsISupportsArray> mailListArray;
  rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_PROGID, nsnull, 
              NS_GET_IID(nsISupportsArray), getter_AddRefs(mailListArray));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISupportsArray> addrbookDirArray;
  rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_PROGID, nsnull, 
                NS_GET_IID(nsISupportsArray), getter_AddRefs(addrbookDirArray));
  if (NS_SUCCEEDED(rv) && addrbookDirArray)
  {
    nsString dirPath;
    GetABDirectories(kDirectoryRoot, addrbookDirArray, PR_TRUE);
    PRUint32 nbrRecipients;

    PRUint32 nbrAddressbook;
    addrbookDirArray->Count(&nbrAddressbook);
    for (k = 0; k < (PRInt32)nbrAddressbook && stillNeedToSearch; k ++)
    {
      nsCOMPtr<nsISupports> item;
      addrbookDirArray->GetElementAt(k, getter_AddRefs(item));
      abDirectory = do_QueryInterface(item, &rv);
      if (NS_FAILED(rv))
        return rv;

      char* uri;
      rv = abDirectory->GetDirUri(&uri);
      if (NS_FAILED(rv))
        return rv;

      rv = OpenAddressBook(uri, getter_AddRefs(abDataBase), getter_AddRefs(abDirectory));
      if (NS_FAILED(rv) || !abDataBase || !abDirectory)
        continue;

      /* Collect all mailing list defined in this AddresBook */
      rv = BuildMailListArray(abDataBase, abDirectory, mailListArray);
      if (NS_FAILED(rv))
        return rv;

      stillNeedToSearch = PR_FALSE;
      for (i = 0; i < MAX_OF_RECIPIENT_ARRAY; i ++)
      {
        recipientsList[i]->Count(&nbrRecipients);
        for (j = 0; j < (PRInt32)nbrRecipients; j ++, recipientsList[i]->Count(&nbrRecipients))
        {
          nsMsgRecipient* recipient = NS_STATIC_CAST(nsMsgRecipient*, recipientsList[i]->ElementAt(j));
          if (recipient && !recipient->mProcessed)
          {
            /* First check if it's a mailing list */
            if (NS_SUCCEEDED(GetMailListAddresses(recipient->mAddress, mailListArray, getter_AddRefs(mailListAddresses))))
            {
              if (populateMailList)
              {
                  PRUint32 nbrAddresses = 0;
                  for (mailListAddresses->Count(&nbrAddresses); nbrAddresses > 0; nbrAddresses --)
                  {
			              item = mailListAddresses->ElementAt(nbrAddresses - 1);
			              existingCard = do_QueryInterface(item, &rv);
			              if (NS_FAILED(rv))
				              return rv;

			              nsXPIDLString pDisplayName;
			              nsXPIDLString pEmail;
                    nsAutoString fullNameStr;

                    PRBool bIsMailList;
			              rv = existingCard->GetIsMailList(&bIsMailList);
			              if (NS_FAILED(rv))
				              return rv;

			              rv = existingCard->GetDisplayName(getter_Copies(pDisplayName));
			              if (NS_FAILED(rv))
				              return rv;

                    if (bIsMailList)
                      rv = existingCard->GetNotes(getter_Copies(pEmail));
                    else
                      rv = existingCard->GetPrimaryEmail(getter_Copies(pEmail));
			              if (NS_FAILED(rv))
				              return rv;

                    if (parser)
                    {
                      char * fullAddress = nsnull;
                      char * utf8Name = nsAutoString((const PRUnichar*)pDisplayName).ToNewUTF8String();
                      char * utf8Email = nsAutoString((const PRUnichar*)pEmail).ToNewUTF8String();

                      parser->MakeFullAddress(nsnull, utf8Name, utf8Email, &fullAddress);
                      if (fullAddress && *fullAddress)
                      {
                        /* We need to convert back the result from UTF-8 to Unicode */
		                    (void)ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), fullAddress, fullNameStr);
                        PR_Free(fullAddress);
                      }
                      Recycle(utf8Name);
                      Recycle(utf8Email);
                    }
                    if (fullNameStr.IsEmpty())
                    {
                      //oops, parser problem! I will try to do my best...
                      fullNameStr = pDisplayName;
                      fullNameStr.AppendWithConversion(" <");
                      if (bIsMailList)
                      {
                        if (pEmail && pEmail[0] != 0)
                          fullNameStr += pEmail;
                        else
                          fullNameStr += pDisplayName;
                      }
                      else
                        fullNameStr += pEmail;
                      fullNameStr.AppendWithConversion(">");
                    }

                    if (fullNameStr.IsEmpty())
                      continue;

                    /* Now we need to insert the new address into the list of recipient */
                    nsMsgRecipient* newRecipient = new nsMsgRecipient(fullNameStr, nsAutoString(pEmail));
                    if (!recipient)
                       return  NS_ERROR_OUT_OF_MEMORY;
                    NS_ADDREF(newRecipient);

                    if (bIsMailList)
                    {
                      //TODO: we must to something to avoid recursivity
                      stillNeedToSearch = PR_TRUE;
                    }
                    else
                    {
    			            PRBool bPlainText;
    			            rv = existingCard->GetSendPlainText(&bPlainText);
    			            if (NS_SUCCEEDED(rv))
    			            {
                        newRecipient->mAcceptHtml = ! bPlainText;
                        newRecipient->mProcessed = PR_TRUE;
                      }
                    }
                    rv = recipientsList[i]->InsertElementAt(newRecipient, j + 1);
                    NS_RELEASE(newRecipient);
                    if (NS_FAILED(rv))
                      return rv;
                  }
                  rv = recipientsList[i]->RemoveElementAt(j);
                 j --;
              }
              else
                recipient->mProcessed = PR_TRUE;

              NS_RELEASE(recipient);
              continue;
            }

            /* Then if we have a card for this email address */
            nsCAutoString emailStr; emailStr.AssignWithConversion(recipient->mEmail);
   			    rv = abDataBase->GetCardForEmailAddress(abDirectory, emailStr, getter_AddRefs(existingCard));
    			  if (NS_SUCCEEDED(rv) && existingCard)
            {
    			    PRBool bPlainText;
    			    rv = existingCard->GetSendPlainText(&bPlainText);
    			    if (NS_SUCCEEDED(rv))
    			    {
                recipient->mAcceptHtml = ! bPlainText;
                recipient->mProcessed = PR_TRUE;
              }
            }
            else
              stillNeedToSearch = PR_TRUE;
            NS_RELEASE(recipient);
          }
        }
      }

      if (abDataBase)
        abDataBase->Close(PR_FALSE);
    }
  }

  /* Finally return the list of non HTML recipient if requested and/or rebuilt the recipient field*/
    nsAutoString recipientsStr;
    nsAutoString nonHtmlRecipientsStr;

    for (i = 0; i < MAX_OF_RECIPIENT_ARRAY; i ++)
    {
      recipientsStr.SetLength(0);
      PRUint32 nbrRecipients;
      recipientsList[i]->Count(&nbrRecipients);
      for (j = 0; j < (PRInt32)nbrRecipients; j ++)
      {
        nsMsgRecipient* recipient = NS_STATIC_CAST(nsMsgRecipient*, recipientsList[i]->ElementAt(j));
        if (recipient)
        {
          if (populateMailList)
          {
            if (! recipientsStr.IsEmpty())
              recipientsStr.AppendWithConversion(',');
            recipientsStr.Append(recipient->mAddress);
          }

          if (returnNoHTMLRecipients && !recipient->mAcceptHtml)
          {
            if (! nonHtmlRecipientsStr.IsEmpty())
              nonHtmlRecipientsStr.AppendWithConversion(',');
            nonHtmlRecipientsStr.Append(recipient->mEmail);
          }

          NS_RELEASE(recipient);
        }
      }

      if (populateMailList)
      {
        PRUnichar * str = recipientsStr.ToNewUnicode();
        switch (i)
        {
          case 0 : m_compFields->SetTo(str);  break;
          case 1 : m_compFields->SetCc(str);  break;
          case 2 : m_compFields->SetBcc(str); break;
        }
        Recycle(str);
      }
  }

  if (returnNoHTMLRecipients)
    *_retval = nonHtmlRecipientsStr.ToNewUnicode();

	return rv;
}

nsresult nsMsgCompose::GetNoHtmlNewsgroups(const PRUnichar *newsgroups, PRUnichar **_retval)
{
    //FIX ME: write me
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    *_retval = nsnull;
    return rv;
}

/* Decides which tags trigger which convertible mode, i.e. here is the logic
   for BodyConvertible */
// Helper function. Parameters are not checked.
nsresult nsMsgCompose::TagConvertible(nsIDOMNode *node,  PRInt32 *_retval)
{
    nsresult rv;

    *_retval = nsIMsgCompConvertible::No;

    nsAutoString elementStr;
    rv = node->GetNodeName(elementStr);
    if (NS_FAILED(rv))
      return rv;
    nsCAutoString element; element.AssignWithConversion(elementStr);

    nsCOMPtr<nsIDOMNode> pItem;
    if      (
              element.EqualsIgnoreCase("#text") ||
              element.EqualsIgnoreCase("br") ||
              element.EqualsIgnoreCase("p") ||
              element.EqualsIgnoreCase("pre") ||
              element.EqualsIgnoreCase("tt") ||
              element.EqualsIgnoreCase("html") ||
              element.EqualsIgnoreCase("head") ||
              element.EqualsIgnoreCase("title") ||
              element.EqualsIgnoreCase("body")
            )
    {
      *_retval = nsIMsgCompConvertible::Plain;
    }
    else if (
              //element.EqualsIgnoreCase("blockquote") || // see below
              element.EqualsIgnoreCase("ul") ||
              element.EqualsIgnoreCase("ol") ||
              element.EqualsIgnoreCase("li") ||
              element.EqualsIgnoreCase("dl") ||
              element.EqualsIgnoreCase("dt") ||
              element.EqualsIgnoreCase("dd")
            )
    {
      *_retval = nsIMsgCompConvertible::Yes;
    }
    else if (
              //element.EqualsIgnoreCase("a") || // see below
              element.EqualsIgnoreCase("h1") ||
              element.EqualsIgnoreCase("h2") ||
              element.EqualsIgnoreCase("h3") ||
              element.EqualsIgnoreCase("h4") ||
              element.EqualsIgnoreCase("h5") ||
              element.EqualsIgnoreCase("h6") ||
              element.EqualsIgnoreCase("hr") ||
              (
                mConvertStructs
                &&
                (
                  element.EqualsIgnoreCase("em") ||
                  element.EqualsIgnoreCase("strong") ||
                  element.EqualsIgnoreCase("code") ||
                  element.EqualsIgnoreCase("b") ||
                  element.EqualsIgnoreCase("i") ||
                  element.EqualsIgnoreCase("u")
                )
              )
            )
    {
      *_retval = nsIMsgCompConvertible::Altering;
    }
    else if (element.EqualsIgnoreCase("blockquote"))
    {
      // Skip <blockquote type=cite>
      *_retval = nsIMsgCompConvertible::Yes;

      nsCOMPtr<nsIDOMNamedNodeMap> pAttributes;
      if (NS_SUCCEEDED(node->GetAttributes(getter_AddRefs(pAttributes)))
          && pAttributes)
      {
        nsAutoString typeName; typeName.AssignWithConversion("type");
        if (NS_SUCCEEDED(pAttributes->GetNamedItem(typeName,
                                                   getter_AddRefs(pItem)))
            && pItem)
        {
          nsAutoString typeValue;
          if (NS_SUCCEEDED(pItem->GetNodeValue(typeValue)))
          {
            typeValue.StripChars("\"");
            if (typeValue.EqualsWithConversion("cite", PR_TRUE))
              *_retval = nsIMsgCompConvertible::Plain;
          }
        }
      }
    }
    else if (
              element.EqualsIgnoreCase("div") ||
              element.EqualsIgnoreCase("span") ||
              element.EqualsIgnoreCase("a")
            )
    {
      /* Do some special checks for these tags. They are inside this |else if|
         for performance reasons */
      nsCOMPtr<nsIDOMNamedNodeMap> pAttributes;

      /* First, test, if the <a>, <div> or <span> is inserted by our
         [TXT|HTML]->HTML converter */
      /* This is for an edge case: A Mozilla user replies to plaintext per HTML
         and the recipient of that HTML msg, also a Mozilla user, replies to
         that again. Then we'll have to recognize the stuff inserted by our
         TXT->HTML converter. */
      if (NS_SUCCEEDED(node->GetAttributes(getter_AddRefs(pAttributes)))
          && pAttributes)
      {
        nsAutoString className;
        className.AssignWithConversion("class");
        if (NS_SUCCEEDED(pAttributes->GetNamedItem(className,
                                                   getter_AddRefs(pItem)))
            && pItem)
        {
          nsAutoString classValue;
          if (NS_SUCCEEDED(pItem->GetNodeValue(classValue))
              && (classValue.EqualsIgnoreCase("moz-txt", 7) ||
                  classValue.EqualsIgnoreCase("\"moz-txt", 8)))
          {
            *_retval = nsIMsgCompConvertible::Plain;
            return rv;  // Inconsistent :-(
          }
        }
      }

      // Maybe, it's an <a> element inserted by another recognizer (e.g. 4.x')
      if (element.EqualsIgnoreCase("a"))
      {
        /* Ignore anchor tag, if the URI is the same as the text
           (as inserted by recognizers) */
        *_retval = nsIMsgCompConvertible::Altering;

        if (NS_SUCCEEDED(node->GetAttributes(getter_AddRefs(pAttributes)))
            && pAttributes)
        {
          nsAutoString hrefName; hrefName.AssignWithConversion("href");
          if (NS_SUCCEEDED(pAttributes->GetNamedItem(hrefName,
                                                     getter_AddRefs(pItem)))
              && pItem)
          {
            nsAutoString hrefValue;
            PRBool hasChild;
            if (NS_SUCCEEDED(pItem->GetNodeValue(hrefValue))
                && NS_SUCCEEDED(node->HasChildNodes(&hasChild)) && hasChild)
            {
              nsCOMPtr<nsIDOMNodeList> children;
              if (NS_SUCCEEDED(node->GetChildNodes(getter_AddRefs(children)))
                  && children
                  && NS_SUCCEEDED(children->Item(0, getter_AddRefs(pItem)))
                  && pItem)
              {
                nsAutoString textValue;
                if (NS_SUCCEEDED(pItem->GetNodeValue(textValue))
                    && textValue == hrefValue)
                  *_retval = nsIMsgCompConvertible::Plain;
              }
            }
          }
        }
      }

      // Lastly, test, if it is just a "simple" <div> or <span>
      else if (
                element.EqualsIgnoreCase("div") ||
                element.EqualsIgnoreCase("span")
              )
      {
        /* skip only if no style attribute */
        *_retval = nsIMsgCompConvertible::Plain;

        if (NS_SUCCEEDED(node->GetAttributes(getter_AddRefs(pAttributes)))
            && pAttributes)
        {
          nsAutoString styleName;
          styleName.AssignWithConversion("style");
          if (NS_SUCCEEDED(pAttributes->GetNamedItem(styleName,
                                                     getter_AddRefs(pItem)))
              && pItem)
          {
            nsAutoString styleValue;
            if (NS_SUCCEEDED(pItem->GetNodeValue(styleValue))
                && !styleValue.IsEmpty())
              *_retval = nsIMsgCompConvertible::No;
          }
        }
      }
    }

    return rv;
}

nsresult nsMsgCompose::BodyConvertible(nsIDOMNode *node,  PRInt32 *_retval)
{
    NS_ENSURE_TRUE(node && _retval, NS_ERROR_NULL_POINTER);

    nsresult rv;
    PRInt32 result;

    // Check this node
    rv = TagConvertible(node, &result);
    if (NS_FAILED(rv))
        return rv;

    // Walk tree recursively to check the children
    PRBool hasChild;
    if (NS_SUCCEEDED(node->HasChildNodes(&hasChild)) && hasChild)
    {
      nsCOMPtr<nsIDOMNodeList> children;
      if (NS_SUCCEEDED(node->GetChildNodes(getter_AddRefs(children)))
          && children)
      {
        PRUint32 nbrOfElements;
        rv = children->GetLength(&nbrOfElements);
        for (PRUint32 i = 0; NS_SUCCEEDED(rv) && i < nbrOfElements; i++)
        {
          nsCOMPtr<nsIDOMNode> pItem;
          if (NS_SUCCEEDED(children->Item(i, getter_AddRefs(pItem)))
              && pItem)
          {
            PRInt32 curresult;
            rv = BodyConvertible(pItem, &curresult);
            if (NS_SUCCEEDED(rv) && curresult > result)
              result = curresult;
          }
        }
      }
    }

    *_retval = result;
    return rv;
}

nsresult nsMsgCompose::SetSignature(nsIMsgIdentity *identity)
{
  nsresult rv;
  
  if (! m_editor)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditor> editor;
  rv = m_editor->GetEditor(getter_AddRefs(editor));
  if (NS_FAILED(rv) || nsnull == editor)
    return rv;

  nsCOMPtr<nsIDOMElement> rootElement;
  rv = editor->GetRootElement(getter_AddRefs(rootElement));
  if (NS_FAILED(rv) || nsnull == rootElement)
    return rv;

  //First look for the current signature, if we have one
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNode> tempNode;
  nsAutoString tagLocalName;

  rv = rootElement->GetLastChild(getter_AddRefs(node));
  if (NS_SUCCEEDED(rv) && nsnull != node)
  {
    if (m_composeHTML)
    {
      /* In html, the signature is inside an element with
         class="moz-signature", it's must be the last node */
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
      if (element)
      {
        nsAutoString attributeName;
        nsAutoString attributeValue;
        attributeName.AssignWithConversion("class");

        rv = element->GetAttribute(attributeName, attributeValue);
        if (NS_SUCCEEDED(rv))
        {
          if (attributeValue.Find("moz-signature", PR_TRUE) != kNotFound)
          {
            //Now, I am sure I get the right node!
            editor->BeginTransaction();
            node->GetPreviousSibling(getter_AddRefs(tempNode));
            rv = editor->DeleteNode(node);
            if (NS_FAILED(rv))
            {
              editor->EndTransaction();
              return rv;
            }

            //Also, remove the <br> right before the signature.
            if (tempNode)
            {
              tempNode->GetLocalName(tagLocalName);
              if (tagLocalName.EqualsWithConversion("BR"))
                editor->DeleteNode(tempNode);
            }
            editor->EndTransaction();
          }
        }
      }
    }
    else
    {
      //In plain text, we have to walk back the dom look for the pattern <br>-- <br>
      PRUint16 nodeType;
      PRInt32 searchState = 0; //0=nothing, 1=br 2='-- '+br, 3=br+'-- '+br

      do
      {
        node->GetNodeType(&nodeType);
        node->GetLocalName(tagLocalName);
        switch (searchState)
        {
          case 0: 
            if (nodeType == nsIDOMNode::ELEMENT_NODE && tagLocalName.EqualsWithConversion("BR"))
              searchState = 1;
            break;

          case 1:
            searchState = 0;
            if (nodeType == nsIDOMNode::TEXT_NODE)
            {
              nsString nodeValue;
              node->GetNodeValue(nodeValue);
              if (nodeValue.EqualsWithConversion("-- "))
                searchState = 2;
            }
            else
              if (nodeType == nsIDOMNode::ELEMENT_NODE && tagLocalName.EqualsWithConversion("BR"))
              {
                searchState = 1;
                break;
              }
            break;

          case 2:
            if (nodeType == nsIDOMNode::ELEMENT_NODE && tagLocalName.EqualsWithConversion("BR"))
              searchState = 3;
            else
              searchState = 0;               
            break;
        }

        tempNode = node;
      } while (searchState != 3 && NS_SUCCEEDED(tempNode->GetPreviousSibling(getter_AddRefs(node))) && node);
      
      if (searchState == 3)
      {
        //Now, I am sure I get the right node!
        editor->BeginTransaction();
        do
        {
          node->GetNextSibling(getter_AddRefs(tempNode));
          rv = editor->DeleteNode(node);
          if (NS_FAILED(rv))
          {
            editor->EndTransaction();
            return rv;
          }

          node = tempNode;
        } while (node);
        editor->EndTransaction();
      }
    }
  }

  //Then add the new one if needed
  nsAutoString aSignature;
  ProcessSignature(identity, &aSignature);
  TranslateLineEndings(aSignature);

  if (!aSignature.IsEmpty())
  {
    editor->BeginTransaction();
    editor->EndOfDocument();
    if (m_composeHTML)
      rv = m_editor->InsertSource(aSignature.GetUnicode());
    else
      rv = m_editor->InsertText(aSignature.GetUnicode());
    editor->EndTransaction();
  }

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

NS_IMPL_ADDREF(nsMsgRecipient)
NS_IMPL_RELEASE(nsMsgRecipient)

NS_INTERFACE_MAP_BEGIN(nsMsgRecipient)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISupports)
NS_INTERFACE_MAP_END

nsMsgRecipient::nsMsgRecipient() :
  mAcceptHtml(PR_FALSE),
  mProcessed(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}
 
nsMsgRecipient::nsMsgRecipient(nsString fullAddress, nsString email, PRBool acceptHtml, PRBool processed) :
  mAddress(fullAddress),
  mEmail(email),
  mAcceptHtml(acceptHtml),
  mProcessed(processed)
{
    NS_INIT_ISUPPORTS();
}

nsMsgRecipient::~nsMsgRecipient()
{
}


NS_IMPL_ADDREF(nsMsgMailList)
NS_IMPL_RELEASE(nsMsgMailList)

NS_INTERFACE_MAP_BEGIN(nsMsgMailList)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISupports)
NS_INTERFACE_MAP_END


nsMsgMailList::nsMsgMailList()
{
    NS_INIT_ISUPPORTS();
}
 
nsMsgMailList::nsMsgMailList(nsString listName, nsString listDescription, nsIAbDirectory* directory) :
  mDirectory(directory)
{
  NS_INIT_ISUPPORTS();
 
  nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);

  if (parser)
  {
    char * fullAddress = nsnull;
    char * utf8Name = listName.ToNewUTF8String();
    char * utf8Email;
    if (listDescription.IsEmpty())
      utf8Email = listName.ToNewUTF8String();   
    else
      utf8Email = listDescription.ToNewUTF8String();   

    parser->MakeFullAddress(nsnull, utf8Name, utf8Email, &fullAddress);
    if (fullAddress && *fullAddress)
    {
      /* We need to convert back the result from UTF-8 to Unicode */
		  (void)ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), fullAddress, mFullName);
      PR_Free(fullAddress);
    }
    Recycle(utf8Name);
    Recycle(utf8Email);
  }

  if (mFullName.IsEmpty())
  {
      //oops, parser problem! I will try to do my best...
      mFullName = listName;
      mFullName.AppendWithConversion(" <");
      if (listDescription.IsEmpty())
        mFullName += listName;
      else
        mFullName += listDescription;
      mFullName.AppendWithConversion(">");
  }

  mDirectory = directory;
}

nsMsgMailList::~nsMsgMailList()
{
}
