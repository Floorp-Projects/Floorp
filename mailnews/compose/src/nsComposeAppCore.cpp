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
#include "nsIDOMBaseAppCore.h"
#include "nsJSComposeAppCore.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIScriptContextOwner.h"

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
#include "nsIDOMMsgAppCore.h"
#include "nsIMessage.h"
#include "nsXPIDLString.h"

#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"

// jefft
#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentViewer.h"
#include "nsIRDFResource.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"


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

NS_BEGIN_EXTERN_C

//nsresult NS_MailNewsLoadUrl(const nsString& urlString, nsISupports * aConsumer);

NS_END_EXTERN_C

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsComposeAppCore : public nsIDOMComposeAppCore,
                         public nsIScriptObjectOwner,
						 public nsIXULWindowCallbacks
{
  
public:
	nsComposeAppCore();
	virtual ~nsComposeAppCore();

	NS_DECL_ISUPPORTS
	NS_DECL_IDOMBASEAPPCORE

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
	NS_IMETHOD CompleteCallback(nsAutoString& aScript);
	NS_IMETHOD SetWindow(nsIDOMWindow* aWin);
	NS_IMETHOD SetEditor(nsIDOMEditorAppCore *editor);
	NS_IMETHOD NewMessage(nsAutoString& aUrl, nsIDOMXULTreeElement *tree,
                          nsIDOMNodeList *nodeList, nsIDOMMsgAppCore *
                          msgAppCore, const PRInt32 messageType);
	NS_IMETHOD SendMessage(nsAutoString& aAddrTo, nsAutoString& aAddrCc,
                           nsAutoString& aAddrBcc, nsAutoString& aSubject,
                           nsAutoString& aMsg);
	NS_IMETHOD SendMessage2(PRInt32 * _retval);

protected:
  
	nsIScriptContext *	GetScriptContext(nsIDOMWindow * aWin);
	void SetWindowFields(nsIDOMDocument* domDoc, nsString& to, nsString& cc, nsString& bcc,
		nsString& subject, nsString& body);

	nsString mId;
	nsString mScript;
	void *mScriptObject;
	nsIScriptContext		*mScriptContext;

	/* rhp - need this to drive message display */
	nsIDOMWindow			*mWindow;
	nsIWebShell				*mWebShell;
	nsIDOMEditorAppCore     *mEditor;

	/* jefft */
	nsIMsgCompFields *mMsgCompFields;
	nsIMsgSend *mMsgSend;
    // ****** Hack Alert ***** Hack Alert ***** Hack Alert *****
    void HackToGetBody(PRInt32 what);
};

//
// nsComposeAppCore
//
nsComposeAppCore::nsComposeAppCore()
{
	mScriptObject		= nsnull;
	mWebShell			= nsnull; 
	mScriptContext	= nsnull;
	mWindow			= nsnull;
	mEditor			= nsnull;
	mMsgCompFields	= nsnull;
	mMsgSend		= nsnull;

	NS_INIT_REFCNT();
}

nsComposeAppCore::~nsComposeAppCore()
{
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mScriptContext);
  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mEditor);
  NS_IF_RELEASE(mMsgSend);
  NS_IF_RELEASE(mMsgCompFields);
}

NS_IMETHODIMP
nsComposeAppCore::ConstructBeforeJavaScript(nsIWebShell *aWebShell)
{
	return NS_OK;
}

	// like ConstructBeforeJavaScript, but called after the onConstruction callback
NS_IMETHODIMP 
nsComposeAppCore::ConstructAfterJavaScript(nsIWebShell *aWebShell)
{
    mWebShell = aWebShell;
#if 1 // --**--**-- This should the way it should work. However, we don't know
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
         char *aString;
         mMsgCompFields->GetTo(&aString);
         nsString to = aString;
         mMsgCompFields->GetCc(&aString);
         nsString cc = aString;
         mMsgCompFields->GetBcc(&aString);
         nsString bcc = aString;
         mMsgCompFields->GetSubject(&aString);
         nsString subject = aString;
         mMsgCompFields->GetBody(&aString);
         nsString body = aString;
 		
 		SetWindowFields(domDoc, to, cc, bcc, subject, body);
 	}
#endif    
	return NS_OK;
}

nsresult nsComposeAppCore::SetDocumentCharset(class nsString const & aCharset) 
{
	nsresult res = NS_OK;
	if (nsnull != mWindow) 
	{
		nsIDOMDocument* domDoc;
		res = mWindow->GetDocument(&domDoc);
		if (NS_SUCCEEDED(res) && nsnull != domDoc) 
		{
			nsIDocument * doc;
			res = domDoc->QueryInterface(kIDocumentIID,(void**)&doc);
			if (NS_SUCCEEDED(res) && nsnull != doc) 
			{
				nsString *aNewCharset = new nsString(aCharset);
				if (nsnull != aNewCharset) 
				{
					doc->SetDocumentCharacterSet(aNewCharset);
				}
				
				NS_RELEASE(doc);
			}
			
			NS_RELEASE(domDoc);
		}
	}
	
	return res;
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
        nsFileSpec fileSpec("c:\\temp\\tempMessage.eml");
        nsInputFileStream fileStream(fileSpec);
        nsString msgBody = what == 2 ? "--------Original Message--------\r\n" 
            : ""; 

        while (!fileStream.eof() && !fileStream.failed() &&
               fileStream.is_open())
        {
            fileStream.readline(buffer, 1024);
            if (*buffer == 0)
                break;
        }
        while (!fileStream.eof() && !fileStream.failed() &&
               fileStream.is_open())
        {
            fileStream.readline(buffer, 1024);
            if (what == 1)
                msgBody += "> ";
            msgBody += buffer;
        }
        mMsgCompFields->SetBody(msgBody.ToNewCString(), NULL);
        PR_Free(buffer);
    }
}

void 
nsComposeAppCore::SetWindowFields(nsIDOMDocument *domDoc, nsString& msgTo, nsString& msgCc, nsString& msgBcc,
		nsString& msgSubject, nsString& msgBody)
{
	nsresult res = NS_OK;

	nsCOMPtr<nsIDOMNode> node;
	nsCOMPtr<nsIDOMNodeList> nodeList;
	nsCOMPtr<nsIDOMHTMLInputElement> inputElement;

	if (domDoc) 
	{
		res = domDoc->GetElementsByTagName("input", getter_AddRefs(nodeList));
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
						if (id == "msgSubject") inputElement->SetValue(msgSubject);
					}
                    
				}
			}
        }
        // ****** We need to find a way to query for nsIDOMEditorAppCore
        // interface from either a nsIWebShell or nsIDOMDocument instead of
        // relying on setting the mEditor pointer from the JavaScript. We tend
        // to set to the wrong instance of nsComposeAppCore. In theory we
        // should have only one nsComposeAppCore running at a given time. We
        // seems not being able to achieve this at the moment.
        if (mEditor)
        {
			mEditor->InsertText(msgBody);
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
	printf("Init\n");
	mId = aId;
	nsresult res;

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
	printf("GetID\n");
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
#if 0 // --*--*--*-- This doesn't work. There are more than one instance of
      // nsComposeAppCore. Which one are we setting?
	nsCOMPtr<nsIDOMDocument> domDoc;
    nsresult res;

 	if (nsnull != mWindow) {
        res = mWindow->GetDocument(getter_AddRefs(domDoc));
 	}
 
 	if (NS_SUCCEEDED(res) && mMsgCompFields && domDoc)
 	{
         char *aString;
         mMsgCompFields->GetTo(&aString);
         nsString to = aString;
         mMsgCompFields->GetCc(&aString);
         nsString cc = aString;
         mMsgCompFields->GetBcc(&aString);
         nsString bcc = aString;
         mMsgCompFields->GetSubject(&aString);
         nsString subject = aString;
         mMsgCompFields->GetBody(&aString);
         nsString body = aString;
 		
 		SetWindowFields(domDoc, to, cc, bcc, subject, body);
 	}
#endif 
	return NS_OK;
}

NS_IMETHODIMP    
nsComposeAppCore::NewMessage(nsAutoString& aUrl,
                             nsIDOMXULTreeElement *tree,
                             nsIDOMNodeList *nodeList,
                             nsIDOMMsgAppCore * msgAppCore,
                             const PRInt32 messageType)
{

	char *  urlstr=nsnull;
	nsresult rv;
	nsString controllerCID;

	nsIAppShellService* appShell;
    rv = nsServiceManager::GetService(kAppShellServiceCID,
                                      nsIAppShellService::GetIID(),
                                      (nsISupports**)&appShell);
    if (NS_FAILED(rv)) return rv;

	nsIURL* url;
	nsIWebShellWindow* newWindow;
  
	rv = NS_NewURL(&url, aUrl);
	if (NS_FAILED(rv)) {
	  goto done;
	}

	controllerCID = "6B75BB61-BD41-11d2-9D31-00805F8ADDDF";
	appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   controllerCID,
                                   newWindow,   // result widget
                                   nsnull,      // observer
                                   this,      // callbacks
                                   615,         // width
                                   650);        // height

	if (tree && nodeList && msgAppCore) {
		nsCOMPtr<nsISupports> object;
		rv = msgAppCore->GetRDFResourceForMessage(tree, nodeList,
                                                   getter_AddRefs(object));
		if ((NS_SUCCEEDED(rv)) && object) {
			nsCOMPtr<nsIMessage> message;
			rv = object->QueryInterface(nsIMessage::GetIID(),
                                         getter_AddRefs(message));
			if ((NS_SUCCEEDED(rv)) && message) {
				nsString aString = "";
				nsString bString = "";

				message->GetSubject(aString);
                switch (messageType)
                {
                default:        
                case 0:         // reply to sender
                case 1:         // reply to all
                {
                    bString += "Re: ";
                    bString += aString;
                    mMsgCompFields->SetSubject(bString.ToNewCString(), NULL);
                    message->GetAuthor(aString);
                    mMsgCompFields->SetTo(aString.ToNewCString(), NULL);
                    if (messageType == 1)
                    {
                        nsString cString, dString;
                        message->GetRecipients(cString);
                        message->GetCCList(dString);
                        if (cString.Length() > 0 && dString.Length() > 0)
                            cString = cString + ", ";
                        cString = cString + dString;
                        mMsgCompFields->SetCc(cString.ToNewCString(), NULL);
                    }
                    HackToGetBody(1);
                    break;
                }
                case 2:         // forward as attachment
                case 3:         // forward as inline
                case 4:         // forward as quoted
                {
                    bString += "[Fwd: ";
                    bString += aString;
                    bString += "]";
                    mMsgCompFields->SetSubject(bString.ToNewCString(), NULL);
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
	NS_RELEASE(url);
    (void)nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
	return NS_OK;
}


NS_IMETHODIMP nsComposeAppCore::SendMessage(nsAutoString& aAddrTo,
                                            nsAutoString& aAddrCc,
                                            nsAutoString& aAddrBcc,
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
	if (mMsgCompFields) { 
		mMsgCompFields->SetFrom((char *)pCompPrefs.GetUserEmail(), NULL);
		mMsgCompFields->SetReplyTo((char *)pCompPrefs.GetReplyTo(), NULL);
		mMsgCompFields->SetOrganization((char *)pCompPrefs.GetOrganization(), NULL);
		mMsgCompFields->SetTo(aAddrTo.ToNewCString(), NULL);
		mMsgCompFields->SetCc(aAddrCc.ToNewCString(), NULL);
		mMsgCompFields->SetBcc(aAddrBcc.ToNewCString(), NULL);
		mMsgCompFields->SetSubject(aSubject.ToNewCString(), NULL);
		mMsgCompFields->SetBody(aMsg.ToNewCString(), NULL);

		if (mMsgSend)
			mMsgSend->SendMessage(mMsgCompFields, NULL);
	}

	if (nsnull != mScriptContext) {
		const char* url = "";
		PRBool isUndefined = PR_FALSE;
		nsString rVal;
		mScriptContext->EvaluateString(mScript, url, 0, rVal, &isUndefined);
	}

	if (mWindow) {
		mWindow->Close();	//JFD Doesn't work yet because mopener wasn't set!
		NS_RELEASE(mWindow);
	}

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
	nsAutoString msgSubject;
	nsAutoString msgBody;

	if (nsnull != mWindow) 
	{
		res = mWindow->GetDocument(getter_AddRefs(domDoc));
		if (NS_SUCCEEDED(res) && domDoc) 
		{
			res = domDoc->GetElementsByTagName("input", getter_AddRefs(nodeList));
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
						}

					}
				}

				if (mEditor)
				{
					mEditor->GetContentsAsText(msgBody);
					SendMessage(msgTo, msgCc, msgBcc, msgSubject, msgBody);
				}
			}
		}
	}
	
	return res;
}
  
extern "C"
nsresult
NS_NewComposeAppCore(nsIDOMComposeAppCore **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsComposeAppCore *appcore = new nsComposeAppCore();
  if (appcore) {
    return appcore->QueryInterface(nsIDOMComposeAppCore::GetIID(),
                                   (void **)aResult);

  }
  return NS_ERROR_NOT_INITIALIZED;
}

