/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIDOMComposeAppCore.h"
#include "nsComposeAppCore.h"
#include "nsIScriptObjectOwner.h"
#include "nsAppCoresCIDs.h"
#include "nsIDOMBaseAppCore.h"
#include "nsIDOMAppCoresManager.h"
#include "nsJSComposeAppCore.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIScriptContextOwner.h"
#include "nsMsgSend.h"

/* rhp - for access to webshell */
#include "prmem.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMElement.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsMsgCompPrefs.h"
#include "nsIMessenger.h"
#include "nsIMessage.h"
#include "nsXPIDLString.h"

#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsMsgCompPrefs.h"

// jefft
#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentViewer.h"
#include "nsIRDFResource.h"
#include "nsINetService.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "jsapi.h"
#include "nsMsgI18N.h"

static NS_DEFINE_IID(kIDOMAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
static NS_DEFINE_CID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDocumentIID, nsIDocument::GetIID());
static NS_DEFINE_IID(kIMsgComposeIID, NS_IMSGCOMPOSE_IID); 
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID); 

static NS_DEFINE_IID(kIMsgCompFieldsIID, NS_IMSGCOMPFIELDS_IID); 
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 

static NS_DEFINE_IID(kIMsgSendIID, NS_IMSGSEND_IID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID);


static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID); 


// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

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

class nsComposeAppCore : public nsIDOMComposeAppCore,
                         public nsIScriptObjectOwner,
						 public nsIXULWindowCallbacks
{
  
public:
	nsComposeAppCore();
	virtual ~nsComposeAppCore();

	NS_DECL_ISUPPORTS
    NS_IMETHOD    GetId(nsString& aId);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset);


	// nsIScriptObjectOwner
	NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
	NS_IMETHOD SetScriptObject(void* aScriptObject);

	// nsIXULWindowCallbacks
	// this method will be called by the window creation code after
	// the UI has been loaded, before the window is visible, and before
	// the equivalent JavaScript (XUL "onConstruction" attribute) callback.
	NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);

	// like ConstructBeforeJavaScript, but called after the onConstruction callback
	NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell);


	// nsIComposeAppCore
	NS_IMETHOD Dispose(void);
	NS_IMETHOD CloseWindow(void);
	NS_IMETHOD CompleteCallback(nsAutoString& aScript);
	NS_IMETHOD SetWindow(nsIDOMWindow* aWin);
	NS_IMETHOD SetEditor(nsIDOMEditorAppCore *editor);
	NS_IMETHOD NewMessage(nsAutoString& aUrl,
							nsAutoString& args,
							nsIDOMXULTreeElement *tree,
							nsIDOMNodeList *nodeList,
							PRInt32 messageType);
	NS_IMETHOD SendMsg(nsAutoString& aAddrTo, nsAutoString& aAddrCc,
                           nsAutoString& aAddrBcc, 
                           nsAutoString& aAddrNewsgroup,
                           nsAutoString& aSubject,
                           nsAutoString& aMsg);
	NS_IMETHOD SendMessage2(PRInt32 * _retval);
	NS_IMETHOD GetUseHtml(PRBool *_retval);
	NS_IMETHOD SetUseHtml(PRBool aUseHtml);
	NS_IMETHOD GetWrapColumn(PRInt32 * _retval);

protected:
  
	nsIScriptContext *	GetScriptContext(nsIDOMWindow * aWin);
	void SetWindowFields(nsIDOMDocument* domDoc, nsString& to, nsString& cc, nsString& bcc, nsString& newsgroup, nsString& subject, nsString& body);

	nsString mId;
	nsString mScript;
	void *mScriptObject;
	nsIScriptContext		*mScriptContext;

	/* rhp - need this to drive message display */
	nsIDOMWindow			*mWindow;
	nsIWebShell				*mWebShell;
	nsIWebShellWindow		*mWebShellWindow;
	nsIDOMEditorAppCore     *mEditor;

	nsIMsgCompFields 		*mMsgCompFields;
	nsIMsgSend 				*mMsgSend;

	nsAutoString 			mArgs;

	PRBool					mUseHtml;
	PRInt32					mWrapColumn;
	
    // ****** Hack Alert ***** Hack Alert ***** Hack Alert *****
    void HackToGetBody(PRInt32 what);
};


static const int APP_DEBUG = 1;
static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIContentViewer> cv;
    rv = shell ? shell->GetContentViewer(getter_AddRefs(cv))
               : NS_ERROR_NULL_POINTER;
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( APP_DEBUG ) {
                            char *p = value.ToNewCString();
                            delete [] p;
                        }
                        if ( rv != NS_OK ) {
                             if (APP_DEBUG) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
                }
            } else {
                if (APP_DEBUG) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (APP_DEBUG) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}


//
// nsComposeAppCore
//
nsComposeAppCore::nsComposeAppCore()
{
	nsMsgCompPrefs prefs;

	NS_INIT_REFCNT();
	mScriptObject		= nsnull;
	mWebShell			= nsnull;
	mWebShellWindow		= nsnull;
	mScriptContext		= nsnull;
	mWindow				= nsnull;
	mEditor				= nsnull;
	mMsgCompFields		= nsnull;
	mMsgSend			= nsnull;
	mUseHtml			= prefs.GetUseHtml();
	mWrapColumn			= prefs.GetWrapColumn();
}

nsComposeAppCore::~nsComposeAppCore()
{
/*
	NS_IF_RELEASE(mWebShell);
	NS_IF_RELEASE(mWebShellWindow);
	NS_IF_RELEASE(mScriptContext);
	NS_IF_RELEASE(mWindow);
	NS_IF_RELEASE(mEditor);
*/
	NS_IF_RELEASE(mMsgSend);
	NS_IF_RELEASE(mMsgCompFields);
}

NS_IMETHODIMP
nsComposeAppCore::ConstructBeforeJavaScript(nsIWebShell *aWebShell)
{
    setAttribute( aWebShell, "args", "value", mArgs );
    // Trigger pseudo-onload handler, now that args are ready.
    setAttribute( aWebShell, "dialog.start", "ready", "true" );
	return NS_OK;
}

	// like ConstructBeforeJavaScript, but called after the onConstruction callback
NS_IMETHODIMP 
nsComposeAppCore::ConstructAfterJavaScript(nsIWebShell *aWebShell)
{
    mWebShell = aWebShell;
	  // --**--**-- This should the way it should work. However, we don't know
      // how to query for the nsIDOMEditorAppCore interface from nsIWebShell.
 	nsIDOMDocument* domDoc = nsnull;
 	if (nsnull != aWebShell) {
 		nsIContentViewer* mCViewer;
 		aWebShell->GetContentViewer(&mCViewer);
 		if (nsnull != mCViewer) {
 		  nsIDocumentViewer* mDViewer;
 		  if (NS_OK == mCViewer->QueryInterface(nsIDocumentViewer::GetIID(), (void**) &mDViewer)) {
 			  nsIDocument* mDoc;
 			  mDViewer->GetDocument(mDoc);
 			  if (nsnull != mDoc) {
 				  if (NS_OK == mDoc->QueryInterface(nsIDOMDocument::GetIID(), (void**) &domDoc)) {
 				}
 				NS_RELEASE(mDoc);
 			  }
 			  NS_RELEASE(mDViewer);
 		  }
 		  NS_RELEASE(mCViewer);
 		}
 	}
 
 	if (mMsgCompFields && domDoc)
 	{
 		nsString aCharset(msgCompHeaderInternalCharset());
    char *aString;
    nsString to, cc, bcc, newsgroup, subject, body;
    nsresult res;
    
    mMsgCompFields->GetTo(&aString);
    if (NS_FAILED(res = ConvertToUnicode(aCharset, aString, to))) {
      to = aString;
    }
    mMsgCompFields->GetCc(&aString);
    if (NS_FAILED(res = ConvertToUnicode(aCharset, aString, cc))) {
      cc = aString;
    }
    mMsgCompFields->GetBcc(&aString);
    if (NS_FAILED(res = ConvertToUnicode(aCharset, aString, bcc))) {
      bcc = aString;
    }
    mMsgCompFields->GetNewsgroups(&aString);
    if (NS_FAILED(res = ConvertToUnicode(aCharset, aString, newsgroup))) {
      newsgroup = aString;
    }
    mMsgCompFields->GetSubject(&aString);
    if (NS_FAILED(res = ConvertToUnicode(aCharset, aString, subject))) {
      subject = aString;
    }

    // mail charset is used for the body instead of utf-8
    char *mail_charset;
    mMsgCompFields->GetCharacterSet(&mail_charset);
    aCharset.SetString(mail_charset);
    mMsgCompFields->GetBody(&aString);
    if (NS_FAILED(res = ConvertToUnicode(aCharset, aString, body))) {
      body = subject;
    }
 		
 		SetWindowFields(domDoc, to, cc, bcc, newsgroup, subject, body);
 	}
	return NS_OK;
}

nsresult nsComposeAppCore::Dispose()
{
	// remove ourselves to the app cores manager...
	// if we were able to inherit directly from nsBaseAppCore then it would do this for
	// us automatically

	nsresult res;

	NS_WITH_SERVICE(nsIDOMAppCoresManager, appCoreManager, kAppCoresManagerCID, &res); 
	if (NS_SUCCEEDED(res) && appCoreManager)
	{
//		if (mEditor)
//			appCoreManager->Remove((nsIDOMBaseAppCore *) mEditor);
		appCoreManager->Remove((nsIDOMBaseAppCore *) this);
	}

	Release();
	return NS_OK;
}

nsresult nsComposeAppCore::CloseWindow()
{
	if (mWebShellWindow)
		mWebShellWindow->Close();
	
	return NS_OK;
}

nsresult nsComposeAppCore::SetDocumentCharset(class nsString const & aCharset) 
{
  // Set charset, this will be used for the MIME charset labeling.
  mMsgCompFields->SetCharacterSet(nsAutoCString(aCharset), NULL);
	
	return NS_OK;
}

nsIScriptContext *    
nsComposeAppCore::GetScriptContext(nsIDOMWindow * aWin)
{
  nsIScriptContext * scriptContext = nsnull;
  if (nsnull != aWin) {
    nsIDOMDocument * domDoc;
    aWin->GetDocument(&domDoc);
    if (nsnull != domDoc) {
      nsIDocument * doc;
      if (NS_OK == domDoc->QueryInterface(kIDocumentIID,(void**)&doc)) {
        nsIScriptContextOwner * owner = doc->GetScriptContextOwner();
        if (nsnull != owner) {
          owner->GetScriptContext(&scriptContext);
          NS_RELEASE(owner);
        }
        NS_RELEASE(doc);
      }
      NS_RELEASE(domDoc);
    }
  }
  return scriptContext;
}

void 
nsComposeAppCore::HackToGetBody(PRInt32 what)
{
    char *buffer = (char *) PR_CALLOC(1024);
    if (buffer)
    {
    	nsString fileName(TEMP_PATH_DIR);
    	fileName += TEMP_MESSAGE_IN;
   
        nsFileSpec fileSpec(fileName);
        nsInputFileStream fileStream(fileSpec);

        nsString msgBody = (what == 2 && !mUseHtml) ? "--------Original Message--------\r\n" 
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
            if (what == 1 && ! mUseHtml)
                msgBody += "> ";
            msgBody += buffer;
            msgBody += MSG_LINEBREAK;
        }
        
        if (mUseHtml)
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

		// mMsgCompFields->SetBody(msgBody.ToNewCString(), NULL);
		// SetBody() strdup()'s cmsgBody.
		mMsgCompFields->SetBody(nsAutoCString(msgBody), NULL);
        PR_Free(buffer);
    }
}

void 
nsComposeAppCore::SetWindowFields(nsIDOMDocument *domDoc, nsString& msgTo, nsString& msgCc, nsString& msgBcc, nsString& msgNewsgroup, nsString& msgSubject, nsString& msgBody)
{
	nsresult res = NS_OK;

	nsCOMPtr<nsIDOMNode> node;
	nsCOMPtr<nsIDOMNodeList> nodeList;
	nsCOMPtr<nsIDOMHTMLInputElement> inputElement;

	if (domDoc) 
	{
		res = domDoc->GetElementsByTagName("INPUT", getter_AddRefs(nodeList));
		if ((NS_SUCCEEDED(res)) && nodeList)
		{
			PRUint32 count;
			PRUint32 i;
			nodeList->GetLength(&count);
			for (i = 0; i < count; i ++)
			{
				res = nodeList->Item(i, getter_AddRefs(node));
				if ((NS_SUCCEEDED(res)) && node)
				{
					nsString value;
					res = node->QueryInterface(nsIDOMHTMLInputElement::GetIID(), getter_AddRefs(inputElement));
					if ((NS_SUCCEEDED(res)) && inputElement)
					{
						nsString id;
						inputElement->GetId(id);
						if (id == "msgTo") inputElement->SetValue(msgTo);
						if (id == "msgCc") inputElement->SetValue(msgCc);
						if (id == "msgBcc") inputElement->SetValue(msgBcc);
                        if (id == "msgNewsgroup") inputElement->SetValue(msgNewsgroup);
						if (id == "msgSubject") inputElement->SetValue(msgSubject);
					}
                    
				}
			}
        }
        if (mEditor)
        {
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
	    			
	        		mEditor->LoadUrl(nsFileURL(aPath).GetURLString());
	    		}
	    	}
	    	else
	    	{
	    		if (mUseHtml)
	        		mEditor->LoadUrl("chrome://messengercompose/content/defaultHtmlBody.html");
	        	else
	        		mEditor->LoadUrl("chrome://messengercompose/content/defaultTextBody.html");
	    	}
        }
    }
}


//
// nsISupports
//
NS_IMPL_ADDREF(nsComposeAppCore);
NS_IMPL_RELEASE(nsComposeAppCore);

NS_IMETHODIMP
nsComposeAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
      return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIScriptObjectOwnerIID)) {
      *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
      AddRef();
      return NS_OK;
  }
  if ( aIID.Equals(nsIDOMBaseAppCore::GetIID())) {
      *aInstancePtr = (void*) ((nsIDOMBaseAppCore*)this);
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(nsIDOMComposeAppCore::GetIID()) ) {
      *aInstancePtr = (void*) (nsIDOMComposeAppCore*)this;
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(nsIXULWindowCallbacks::GetIID()) ) {
      *aInstancePtr = (void*) (nsIXULWindowCallbacks*)this;
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
      *aInstancePtr = (void*)(nsISupports*) (nsIScriptObjectOwner *) this;
      AddRef();
      return NS_OK;
  }


  return NS_NOINTERFACE;
}

//
// nsIScriptObjectOwner
//
nsresult
nsComposeAppCore::GetScriptObject(nsIScriptContext *aContext, void **aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptComposeAppCore(aContext, 
                                   (nsISupports *)(nsIDOMComposeAppCore*)this, 
                                   nsnull, 
                                   &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsComposeAppCore::SetScriptObject(void* aScriptObject)
{
  
  return NS_OK;
} 

//
// nsIDOMBaseAppCore
//
nsresult
nsComposeAppCore::Init(const nsString& aId)
{
	nsresult res;
	mId = aId;

	// add ourselves to the app cores manager...
	// if we were able to inherit directly from nsBaseAppCore then it would do this for
	// us automatically

	NS_WITH_SERVICE(nsIDOMAppCoresManager, appCoreManager, kAppCoresManagerCID, &res); 
	if (NS_SUCCEEDED(res) && appCoreManager)
		appCoreManager->Add((nsIDOMBaseAppCore *) this);

	if (!mMsgSend)
	{
		res = nsComponentManager::CreateInstance(kMsgSendCID, 
		                                         NULL, 
			                                     kIMsgSendIID, 
                                                 (void **) &mMsgSend); 
		if (NS_FAILED(res)) return NS_ERROR_FAILURE;
		printf("We succesfully obtained a nsIMsgSend interface....\n");
	}
	if (!mMsgCompFields)
	{
		res = nsComponentManager::CreateInstance(kMsgCompFieldsCID, 
												 NULL, 
												 kIMsgCompFieldsIID, 
												 (void **) &mMsgCompFields); 
		if (NS_FAILED(NS_OK)) return NS_ERROR_FAILURE;
		printf("We succesfully obtained a nsIMsgCompFields interface....\n");
	}
	return NS_OK;
}


nsresult
nsComposeAppCore::GetId(nsString& aId)
{
	aId = mId;
	return NS_OK;
}

//
// nsIComposeAppCore
//
NS_IMETHODIMP    
nsComposeAppCore::SetWindow(nsIDOMWindow* aWin)
{
	mWindow = aWin;
	// NS_ADDREF(mWindow);
	mScriptContext = GetScriptContext(aWin);
	return NS_OK;
}


NS_IMETHODIMP    
nsComposeAppCore::SetEditor(nsIDOMEditorAppCore* editor)
{
	mEditor = editor;
	// NS_ADDREF(mEditor);
	return NS_OK;
}


NS_IMETHODIMP    
nsComposeAppCore::CompleteCallback(nsAutoString& aScript)
{
	mScript = aScript;
	return NS_OK;
}

NS_IMETHODIMP    
nsComposeAppCore::NewMessage(nsAutoString& aUrl,
							               nsAutoString& args,
                             nsIDOMXULTreeElement *tree,
                             nsIDOMNodeList *nodeList,
                             PRInt32 messageType)
{
	nsresult rv;
  char *default_mail_charset = nsnull;

	mArgs = args;
	NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;

	nsIURI* url = nsnull;
	NS_WITH_SERVICE(nsINetService, pNetService, kNetServiceCID, &rv); 
	if (NS_SUCCEEDED(rv) && pNetService) 
	{
		rv = pNetService->CreateURL(&url, aUrl);
		if (NS_FAILED(rv))
			goto done;		// goto in C++ is evil!
	}
	else
		goto done;		// goto in C++ is evil!

  nsCOMPtr<nsIWebShellWindow>  newWindow;
	appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   PR_TRUE,
                                   *getter_AddRefs(newWindow),   // result widget
                                   nsnull,      // observer
                                   this,      // callbacks
                                   615,         // width
                                   650);        // height
	
	mWebShellWindow = newWindow;
  // Get the default charset from pref, use this as a mail charset.
  default_mail_charset = nsMsgI18NGetDefaultMailCharset();
  if (NULL != default_mail_charset) {
    mMsgCompFields->SetCharacterSet(default_mail_charset, NULL);
    PR_Free(default_mail_charset);
  }

	if (tree && nodeList) {
        nsCOMPtr<nsIMessenger> messenger;
        rv = nsComponentManager::CreateInstance("component://netscape/messenger",
                                                nsnull,
                                                nsIMessenger::GetIID(),
                                                (void **)getter_AddRefs(messenger));
        if (NS_FAILED(rv)) goto done;
		nsCOMPtr<nsISupports> object;
		rv = messenger->GetRDFResourceForMessage(tree, nodeList,
                                                   getter_AddRefs(object));
		if ((NS_SUCCEEDED(rv)) && object) {
			nsCOMPtr<nsIMessage> message;
			rv = object->QueryInterface(nsIMessage::GetIID(),
                                         getter_AddRefs(message));
			if ((NS_SUCCEEDED(rv)) && message) {
				nsString aString = "";
				nsString bString = "";
        nsString aCharset = "";
        nsString decodedString;
        nsString encodedCharset;  // we don't use this
        char *aCString;

        message->GetCharSet(aCharset);
				message->GetSubject(&aString);
                switch (messageType)
                {
                default:        
                case 0:         // reply to sender
                case 1:         // reply to all
                {
                    // get an original charset, used for a label, UTF-8 is used for the internal processing
                    if (!aCharset.Equals("")) {
                      mMsgCompFields->SetCharacterSet(nsAutoCString(aCharset), NULL);
                    }
                    bString += "Re: ";
                    bString += aString;
					mMsgCompFields->SetSubject(nsAutoCString(bString), NULL);
                    if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(bString, encodedCharset, decodedString))) {
                      if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString))) {
                        mMsgCompFields->SetSubject(aCString, NULL);
                        PR_Free(aCString);
                      }
                    }
                    
					message->GetAuthor(&aString);		
					mMsgCompFields->SetTo(nsAutoCString(aString), NULL);
                    if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(aString, encodedCharset, decodedString))) {
                      if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString))) {
                        mMsgCompFields->SetTo(aCString, NULL);
                        PR_Free(aCString);
                      }
                    }

                    if (messageType == 1)
                    {
                        nsString cString, dString;
                        message->GetRecipients(&cString);
                        message->GetCCList(&dString);
                        if (cString.Length() > 0 && dString.Length() > 0)
                            cString = cString + ", ";
                        cString = cString + dString;
						mMsgCompFields->SetCc(nsAutoCString(cString), NULL);
                        if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(cString, encodedCharset, decodedString))) {
                          if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString))) {
                            mMsgCompFields->SetTo(aCString, NULL);
                            PR_Free(aCString);
                          }
                        }
                    }
                    HackToGetBody(1);
                    break;
                }
                case 2:         // forward as attachment
                case 3:         // forward as inline
                case 4:         // forward as quoted
                {
                    if (!aCharset.Equals("")) {
                      mMsgCompFields->SetCharacterSet(nsAutoCString(aCharset), NULL);
                    }
                    bString += "[Fwd: ";
                    bString += aString;
                    bString += "]";

					mMsgCompFields->SetSubject(nsAutoCString(bString), NULL);
                    if (NS_SUCCEEDED(rv = nsMsgI18NDecodeMimePartIIStr(bString, encodedCharset, decodedString))) {
                      if (NS_SUCCEEDED(rv = ConvertFromUnicode(msgCompHeaderInternalCharset(), decodedString, &aCString))) {
                        mMsgCompFields->SetSubject(aCString, NULL);
                        PR_Free(aCString);
                      }
                    }

                    /* We need to get more information out from the message. */
                    nsCOMPtr<nsIRDFResource> rdfResource;
                    rv = object->QueryInterface(kIRDFResourceIID,
                                                getter_AddRefs(rdfResource));
                    if (rdfResource)
                    {	
                        nsXPIDLCString uri;
                        rdfResource->GetValue( getter_Copies(uri) );
                        nsString messageUri(uri);
                    }
                    if (messageType == 2)
                        HackToGetBody(0);
                    else if (messageType == 3)
                        HackToGetBody(2);
                    else
                        HackToGetBody(1);
                    break;
                }
                }
                    
            }
        }
	}

done:
	NS_IF_RELEASE(url);
	return NS_OK;
}


NS_IMETHODIMP nsComposeAppCore::SendMsg(nsAutoString& aAddrTo,
                                        nsAutoString& aAddrCc,
                                        nsAutoString& aAddrBcc,
                                        nsAutoString& aAddrNewsgroup,
                                        nsAutoString& aSubject,
                                        nsAutoString& aMsg)
{
	nsMsgCompPrefs pCompPrefs;
	char* pUserEmail = nsnull;

	if (nsnull == mScriptContext)
		return NS_ERROR_FAILURE;

#ifdef DEBUG
	  printf("----------------------------\n");
	  printf("-- Sending Mail Message\n");
	  printf("----------------------------\n");
	  printf("To: %s  Cc: %s  Bcc: %s\n", aAddrTo.ToNewCString(), aAddrCc.ToNewCString(), aAddrBcc.ToNewCString());
	  printf("Subject: %s  \nMsg: %s\n", aSubject.ToNewCString(), aMsg.ToNewCString());
	  printf("----------------------------\n");
#endif //DEBUG

//	nsIMsgCompose *pMsgCompose; 
	if (mMsgCompFields) 
	{ 
		nsString aString;
		nsString aCharset(msgCompHeaderInternalCharset());
		char *outCString;

		// Pref values are supposed to be stored as UTF-8, so no conversion
		mMsgCompFields->SetFrom((char *)pCompPrefs.GetUserEmail(), NULL);
		mMsgCompFields->SetReplyTo((char *)pCompPrefs.GetReplyTo(), NULL);
		mMsgCompFields->SetOrganization((char *)pCompPrefs.GetOrganization(), NULL);

		// Convert fields to UTF-8
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, aAddrTo, &outCString))) 
		{
			mMsgCompFields->SetTo(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			mMsgCompFields->SetTo(nsAutoCString(aAddrTo), NULL);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, aAddrCc, &outCString))) 
		{
			mMsgCompFields->SetCc(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			mMsgCompFields->SetCc(nsAutoCString(aAddrCc), NULL);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, aAddrBcc, &outCString))) 
		{
			mMsgCompFields->SetBcc(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			mMsgCompFields->SetBcc(nsAutoCString(aAddrBcc), NULL);

		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, aAddrNewsgroup, &outCString))) 
		{
			mMsgCompFields->SetNewsgroups(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			mMsgCompFields->SetNewsgroups(nsAutoCString(aAddrNewsgroup), NULL);
        
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, aSubject, &outCString))) 
		{
			mMsgCompFields->SetSubject(outCString, NULL);
			PR_Free(outCString);
		}
		else 
			mMsgCompFields->SetSubject(nsAutoCString(aSubject), NULL);

		// Convert body to mail charset not to utf-8 (because we don't manipulate body text)
		char *mail_charset;
		mMsgCompFields->GetCharacterSet(&mail_charset);
		aCharset.SetString(mail_charset);
		if (NS_SUCCEEDED(ConvertFromUnicode(aCharset, aMsg, &outCString))) 
		{
			mMsgCompFields->SetBody(outCString, NULL);
			PR_Free(outCString);
		}
		else
			mMsgCompFields->SetBody(nsAutoCString(aMsg), NULL);

		if (mMsgSend)
    {
        char    *bodyString = NULL;
        PRInt32 bodyLength;

        mMsgCompFields->GetBody(&bodyString);
        bodyLength = PL_strlen(bodyString);

        mMsgSend->CreateAndSendMessage(mMsgCompFields, 
					PR_FALSE,         				// PRBool                            digest_p,
					PR_FALSE,         				// PRBool                            dont_deliver_p,
					nsMsgDeliverNow,   				// nsMsgDeliverMode                  mode,
					mUseHtml?TEXT_HTML:TEXT_PLAIN,  // const char                        *attachment1_type,
					bodyString,               // const char                        *attachment1_body,
        	bodyLength,               // PRUint32                          attachment1_body_length,
					NULL,             				// const struct nsMsgAttachmentData   *attachments,
					NULL,             				// const struct nsMsgAttachedFile     *preloaded_attachments,
					NULL,             				// nsMsgSendPart                     *relatedPart,
          nsnull,                   // callback function defined in nsMsgComposeBE.h
          nsnull);                  // tagged FE data that will be passed to the FE
    }
	}
	if (nsnull != mScriptContext) {
		const char* url = "";
		PRBool isUndefined = PR_FALSE;
		nsString rVal;
		
		mScriptContext->EvaluateString(mScript, url, 0, rVal, &isUndefined);
		CloseWindow();
	}
	else // If we don't have a JS callback, then close the window by default!
		CloseWindow();

	PR_FREEIF(pUserEmail);
	return NS_OK;
}

NS_IMETHODIMP nsComposeAppCore::SendMessage2(PRInt32 * _retval)
{
	nsresult res = NS_OK;

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

	if (nsnull != mWindow) 
	{
		res = mWindow->GetDocument(getter_AddRefs(domDoc));
		if (NS_SUCCEEDED(res) && domDoc) 
		{
			res = domDoc->GetElementsByTagName("INPUT", getter_AddRefs(nodeList));
			if ((NS_SUCCEEDED(res)) && nodeList)
			{
				PRUint32 count;
				PRUint32 i;
				nodeList->GetLength(&count);
				for (i = 0; i < count; i ++)
				{
					res = nodeList->Item(i, getter_AddRefs(node));
					if ((NS_SUCCEEDED(res)) && node)
					{
						nsString value;
						res = node->QueryInterface(nsIDOMHTMLInputElement::GetIID(), getter_AddRefs(inputElement));
						if ((NS_SUCCEEDED(res)) && inputElement)
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

				if (mEditor)
				{
					if (mUseHtml)
						mEditor->GetContentsAsHTML(msgBody);
					else
						mEditor->GetContentsAsText(msgBody);
					*_retval = SendMsg(msgTo, msgCc, msgBcc, msgNewsgroup, msgSubject, msgBody);          
				}
			}
		}
	}
	
	return res;
}

NS_IMETHODIMP nsComposeAppCore::GetUseHtml(PRBool *_retVal)
{
	*_retVal = mUseHtml;
	return NS_OK;
}

NS_IMETHODIMP nsComposeAppCore::SetUseHtml(PRBool aUseHtml)
{
	mUseHtml = aUseHtml;
	return NS_OK;
}

NS_IMETHODIMP nsComposeAppCore::GetWrapColumn(PRInt32 *_retval)
{
	*_retval = mWrapColumn;
	return NS_OK;
}

 
extern "C"
nsresult
NS_NewComposeAppCore(const nsIID &aIID, void **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsComposeAppCore *appcore = new nsComposeAppCore();
  if (appcore) 
     return appcore->QueryInterface(aIID, aResult);
  else
	  return NS_ERROR_NOT_INITIALIZED;
}

