/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsIDOMWindow.h"
#include "nsMsgStatusFeedback.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULDocument.h"

nsMsgStatusFeedback::nsMsgStatusFeedback()
{
	NS_INIT_REFCNT();
	m_meteorsSpinning = PR_FALSE;
}

nsMsgStatusFeedback::~nsMsgStatusFeedback()
{
}

//
// nsISupports
//
NS_IMPL_ISUPPORTS2(nsMsgStatusFeedback, nsIMsgStatusFeedback, nsIDocumentLoaderObserver)

// nsIDocumentLoaderObserver


// nsIDocumentLoaderObserver methods

NS_IMETHODIMP
nsMsgStatusFeedback::OnStartDocumentLoad(nsIDocumentLoader* aLoader, nsIURI* aURL, const char* aCommand)
{
	NS_PRECONDITION(aLoader != nsnull, "null ptr");
	if (! aLoader)
	    return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aURL != nsnull, "null ptr");
	if (! aURL)
	    return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;

	if (mWindow)
	{
		nsIDOMWindow *aWindow = mWindow;
		nsCOMPtr<nsIScriptGlobalObject>
			globalScript(do_QueryInterface(aWindow));
		nsCOMPtr<nsIWebShell> webshell, rootWebshell;
		if (globalScript)
			globalScript->GetWebShell(getter_AddRefs(webshell));
		if (webshell)
			webshell->GetRootWebShell(*getter_AddRefs(rootWebshell));
		if (rootWebshell) 
		{
		  // Kick start the throbber
		  if (!m_meteorsSpinning)
			  setAttribute( rootWebshell, "Messenger:Throbber", "busy", "true" );
		  else	// because of a bug, we're not stopping the meteors, so lets just stop them here.
			  setAttribute( rootWebshell, "Messenger:Throbber", "busy", "false" );
		  setAttribute( rootWebshell, "Messenger:Status", "value", "Loading Document..." );
		  m_meteorsSpinning = PR_TRUE;

		  // Enable the Stop buton
//		  setAttribute( rootWebshell, "canStop", "disabled", "" );
		}
	}
	return rv;
}


NS_IMETHODIMP
nsMsgStatusFeedback::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* channel, nsresult aStatus,
									nsIDocumentLoaderObserver * aObserver)
{
  NS_PRECONDITION(aLoader != nsnull, "null ptr");
  if (! aLoader)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(channel != nsnull, "null ptr");
  if (! channel)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;

	if (mWindow)
	{
		nsIDOMWindow *aWindow = mWindow;
		nsCOMPtr<nsIScriptGlobalObject>
			globalScript(do_QueryInterface(aWindow));
		nsCOMPtr<nsIWebShell> webshell, rootWebshell;
		if (globalScript)
			globalScript->GetWebShell(getter_AddRefs(webshell));
		if (webshell)
			webshell->GetRootWebShell(*getter_AddRefs(rootWebshell));
		if (rootWebshell) 
		{
		  // stop the throbber
			setAttribute( rootWebshell, "Messenger:Throbber", "busy", "false" );
			setAttribute( rootWebshell, "Messenger:Status", "value", "Document: Done" );
			m_meteorsSpinning = PR_FALSE;
		  // Disable the Stop buton
//		  setAttribute( rootWebshell, "canStop", "disabled", "true" );
		}
	}
  return rv;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgStatusFeedback::HandleUnknownContentType(nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType,const char *aCommand )
{
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::ShowStatusString(const PRUnichar *status)
{
	nsString statusMsg = status;
	setAttribute( mWebShell, "Messenger:Status", "value", statusMsg );
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::ShowProgress(PRInt32 percentage)
{
	nsString strPercentage;
	
	if (percentage >= 0)
		setAttribute(mWebShell, "Messenger:LoadingProgress", "mode","normal");

	strPercentage.Append(percentage, 10);
	setAttribute( mWebShell, "Messenger:LoadingProgress", "value", strPercentage);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgStatusFeedback::StartMeteors()
{
	if (!m_meteorsSpinning)
	{
		setAttribute( mWebShell, "Messenger:Throbber", "busy", "true" );
		m_meteorsSpinning = PR_TRUE;
	}
	return NS_OK;
}


NS_IMETHODIMP
nsMsgStatusFeedback::StopMeteors()
{
	if (m_meteorsSpinning)
	{
		setAttribute( mWebShell, "Messenger:Throbber", "busy", "false" );
		m_meteorsSpinning = PR_FALSE;
	}
	return NS_OK;
}


void nsMsgStatusFeedback::SetWebShell(nsIWebShell *shell, nsIDOMWindow *aWindow)
{
	if (aWindow)
	{
		nsCOMPtr<nsIScriptGlobalObject>
			globalScript(do_QueryInterface(aWindow));
		nsCOMPtr<nsIWebShell> webshell, rootWebshell;
		if (globalScript)
			globalScript->GetWebShell(getter_AddRefs(webshell));
		if (webshell)
			webshell->GetRootWebShell(*getter_AddRefs(mWebShell));
	}
	mWindow = aWindow;
}

static int debugSetAttr = 0;
nsresult nsMsgStatusFeedback::setAttribute( nsIWebShell *shell,
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
                if ( xulDoc ) 
				{
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( debugSetAttr ) {
                            char *p = value.ToNewCString();
							printf("setting busy to %s\n", p);
                            delete [] p;
                        }
                        if ( rv != NS_OK ) {
                             if (debugSetAttr) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (debugSetAttr) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (debugSetAttr)   printf("Upcast to nsIDOMHTMLDocument failed\n");
                }
            } else {
                if (debugSetAttr) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (debugSetAttr) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (debugSetAttr) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}


