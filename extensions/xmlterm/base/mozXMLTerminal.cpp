/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozXMLTerminal.cpp: implementation of mozIXMLTerminal interface
// to manage all XMLTerm operations.
// Creates a new mozXMLTermSession object to manage session input/output.
// Creates a mozLineTermAux object to access LineTerm operations.
// Creates key/text/mouse/drag listener objects to handle user events.

#include "nscore.h"
#include "nspr.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIObserver.h"

#include "nsIPresContext.h"

#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"

#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTerminal.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIXMLTerminalIID,    MOZIXMLTERMINAL_IID);
static NS_DEFINE_IID(kXMLTerminalCID,     MOZXMLTERMINAL_CID);

static NS_DEFINE_CID(kCClipboardCID,      NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,   NS_TRANSFERABLE_CID);

/////////////////////////////////////////////////////////////////////////
// mozXMLTerminal factory
/////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXMLTerminal(mozIXMLTerminal** aXMLTerminal)
{
    NS_PRECONDITION(aXMLTerminal != nsnull, "null ptr");
    if (!aXMLTerminal)
        return NS_ERROR_NULL_POINTER;

    *aXMLTerminal = new mozXMLTerminal();
    if (! *aXMLTerminal)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aXMLTerminal);
    return NS_OK;
}


/////////////////////////////////////////////////////////////////////////
// mozXMLTerminal implementation
/////////////////////////////////////////////////////////////////////////
mozXMLTerminal::mozXMLTerminal() :
  mInitialized(PR_FALSE),

  mCookie(""),

  mCommand(""),
  mPromptExpr(""),
  mFirstInput(""),

  mXMLTermShell(nsnull),
  mWebShell(nsnull),
  mPresShell(nsnull),
  mDOMDocument(nsnull),

  mXMLTermSession(nsnull),

  mLineTermAux(nsnull),

  mKeyListener(nsnull),
  mTextListener(nsnull),
  mMouseListener(nsnull),
  mDragListener(nsnull)
{
  NS_INIT_REFCNT();
}


mozXMLTerminal::~mozXMLTerminal()
{
  if (mInitialized) {
    Finalize();
  }
}


// Implement AddRef and Release
NS_IMPL_ADDREF(mozXMLTerminal)
NS_IMPL_RELEASE(mozXMLTerminal)


NS_INTERFACE_MAP_BEGIN(mozXMLTerminal)
		/*
			I maintained the order from the original, however,
			the original called |XMLT_LOG| and in the interface-map form
			it no longer does.  Is this an issue?
		*/
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozIXMLTerminal)
	NS_INTERFACE_MAP_ENTRY(mozIXMLTerminal)
	NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


NS_IMETHODIMP mozXMLTerminal::GetCurrentEntryNumber(PRInt32 *aNumber)
{
  if (mXMLTermSession) {
    return mXMLTermSession->GetCurrentEntryNumber(aNumber);
  } else {
    return NS_ERROR_FAILURE;
  }
}


NS_IMETHODIMP mozXMLTerminal::GetHistory(PRInt32 *aHistory)
{
  if (mXMLTermSession) {
    return mXMLTermSession->GetHistory(aHistory);
  } else {
    return NS_ERROR_FAILURE;
  }
}


NS_IMETHODIMP mozXMLTerminal::SetHistory(PRInt32 aHistory)
{
  if (mXMLTermSession) {
    return mXMLTermSession->SetHistory(aHistory);
  } else {
    return NS_ERROR_FAILURE;
  }
}


NS_IMETHODIMP mozXMLTerminal::GetPrompt(PRUnichar **aPrompt)
{
  if (mXMLTermSession) {
    return mXMLTermSession->GetPrompt(aPrompt);
  } else {
    return NS_ERROR_FAILURE;
  }
}


NS_IMETHODIMP mozXMLTerminal::SetPrompt(const PRUnichar* aPrompt)
{
  if (mXMLTermSession) {
    return mXMLTermSession->SetPrompt(aPrompt);
  } else {
    return NS_ERROR_FAILURE;
  }
}


// Initialize by starting load of Init page for XMLTerm
NS_IMETHODIMP mozXMLTerminal::Init(nsIWebShell* aWebShell,
                                   mozIXMLTermShell* aXMLTermShell,
                                   const PRUnichar* aURL,
                                   const PRUnichar* args)
{
  XMLT_LOG(mozXMLTerminal::Init,20,("\n"));

  if (!aWebShell)
      return NS_ERROR_NULL_POINTER;

  if (mWebShell)
    return NS_ERROR_ALREADY_INITIALIZED;

  mWebShell = aWebShell;          // containing webshell; no addref

  mXMLTermShell = aXMLTermShell;  // containing xmlterm shell; no addref

  nsresult result = NS_OK;

  // NOTE: Need to parse args string!!!
  mCommand = "";
  mPromptExpr = "";
  mFirstInput = args;

  // Initialization completed
  mInitialized = PR_TRUE;

  if ((aURL != nsnull) && (*aURL != 0)) {
    // Load URL and activate XMLTerm after loading
    XMLT_LOG(mozXMLTerminal::Init,22,("setting DocLoaderObs\n"));

    // About to create owning reference to this
    result = mWebShell->SetDocLoaderObserver((nsIDocumentLoaderObserver*)this);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

    XMLT_LOG(mozXMLTerminal::Init,22,("done setting DocLoaderObs\n"));

    // Load initial XMLterm background document
    nsAutoString urlString(aURL);

    result = mWebShell->LoadURL(urlString.GetUnicode());
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

  } else {
    // Document already loaded; activate XMLTerm
    result = Activate();
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;
  }

  XMLT_LOG(mozXMLTerminal::Init,21,("exiting\n"));
  return result;
}


// De-initialize XMLTerminal
NS_IMETHODIMP mozXMLTerminal::Finalize(void)
{
  if (mXMLTermSession) {
    // Finalize XMLTermSession object and delete it (it is not ref. counted)
    mXMLTermSession->Finalize();
    delete mXMLTermSession;
    mXMLTermSession = nsnull;
  }

  if (mDOMDocument) {
    // Release any event listeners for the document
    nsCOMPtr<nsIDOMEventReceiver> eventReceiver;
    nsresult result = mDOMDocument->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(eventReceiver));

    if (NS_SUCCEEDED(result) && eventReceiver) {
      if (mKeyListener) {
        eventReceiver->RemoveEventListenerByIID(mKeyListener,
                                                nsIDOMKeyListener::GetIID());
        mKeyListener = nsnull;
      }

      if (mTextListener) {
        eventReceiver->RemoveEventListenerByIID(mTextListener,
                                                nsIDOMTextListener::GetIID());
        mTextListener = nsnull;
      }

      if (mMouseListener) {
        eventReceiver->RemoveEventListenerByIID(mMouseListener,
                                                nsIDOMMouseListener::GetIID());
        mMouseListener = nsnull;
      }

      if (mDragListener) {
        eventReceiver->RemoveEventListenerByIID(mDragListener,
                                                nsIDOMDragListener::GetIID());
        mDragListener = nsnull;
      }
    }
    mDOMDocument = nsnull;
  }

  if (mLineTermAux) {
    // Finalize and release reference to LineTerm object owned by us
    mLineTermAux->CloseAux();
    mLineTermAux = nsnull;
  }

  if (mWebShell) {
    // Stop observing document loading
    mWebShell->SetDocLoaderObserver((nsIDocumentLoaderObserver *)nsnull);
    mWebShell = nsnull;
  }

  mPresShell = nsnull;
  mXMLTermShell = nsnull;

  mInitialized = PR_FALSE;

  return NS_OK;
}


/** Activates XMLterm and instantiates LineTerm;
 * called at the the end of Init page loading.
 */
NS_IMETHODIMP mozXMLTerminal::Activate(void)
{
  nsresult result = NS_OK;

#if 0
  // TEMPORARY: Testing mozIXMLTermStream
  nsAutoString streamData = "<HTML><HEAD><TITLE>Stream Title</TITLE>"
                            "<SCRIPT language='JavaScript'>"
                            "function clik(){ dump('click\\n');return(false);}"
                            "</SCRIPT></HEAD>"
                            "<BODY><B>Stream Body "
                            "<SPAN STYLE='color: blue' onClick='return clik();'>Clik</SPAN></B><BR>"
"<TABLE WIDTH=720><TR><TD WIDTH=700 BGCOLOR=maroon>&nbsp;</TABLE>"
                            "<BR>ABCD<BR>EFGH<BR>JKLM<BR>"
                            "</BODY></HTML>";

  nsCOMPtr<mozIXMLTermStream> stream;
  result = NS_NewXMLTermStream(getter_AddRefs(stream));
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozXMLTerminal::Activate: Failed to create stream\n");
    return result;
  }

  nsCOMPtr<nsIDOMWindow> outerDOMWindow;
  result = mozXMLTermUtils::ConvertWebShellToDOMWindow(mWebShell,
                                              getter_AddRefs(outerDOMWindow));

  if (NS_FAILED(result) || !outerDOMWindow)
    return NS_ERROR_FAILURE;

  result = stream->Open(outerDOMWindow, "iframe1", "chrome://dummy",
                        "text/html", 800);
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozXMLTerminal::Activate: Failed to open stream\n");
    return result;
  }

  result = stream->Write(streamData.GetUnicode());
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozXMLTerminal::Activate: Failed to write to stream\n");
    return result;
  }

  result = stream->Close();
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozXMLTerminal::Activate: Failed to close stream\n");
    return result;
  }

#endif

  XMLT_LOG(mozXMLTerminal::Activate,20,("\n"));

  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  PR_ASSERT(mWebShell != nsnull);

  if ((mDOMDocument != nsnull) || (mPresShell != nsnull))
    return NS_ERROR_FAILURE;

  // Get reference to DOMDocument
  nsCOMPtr<nsIDOMDocument> domDocument;
  result = mozXMLTermUtils::GetWebShellDOMDocument(mWebShell,
                                                 getter_AddRefs(domDocument));

  if (NS_FAILED(result) || !domDocument)
    return NS_ERROR_FAILURE;

  // Get reference to presentation shell
  if (mPresShell != nsnull)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> presContext;
  result = mozXMLTermUtils::GetWebShellPresContext(mWebShell,
                                                  getter_AddRefs(presContext));
  if (NS_FAILED(result) || !presContext)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  result = presContext->GetShell(getter_AddRefs(presShell));

  if (NS_FAILED(result) || !presShell)
    return NS_ERROR_FAILURE;

  // Save references to DOMDocument and presentation shell
  // (SVN: Should these be addref'ed and released in the destructor?)
  mDOMDocument = domDocument;  // no addref
  mPresShell = presShell;      // no addref

  // Instantiate and initialize XMLTermSession object
  mXMLTermSession = new mozXMLTermSession();

  if (!mXMLTermSession) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result = mXMLTermSession->Init(this, mPresShell, mDOMDocument);
  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to initialize XMLTermSession\n");
    return NS_ERROR_FAILURE;
  }


  // Instantiate LineTerm  
  XMLT_LOG(mozXMLTerminal::Activate,22,("instantiating lineterm\n"));
  result = NS_NewLineTermAux(getter_AddRefs(mLineTermAux));
  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to instantiate LineTermAux\n");
    return result;
  }

  // Open LineTerm to execute command
  // Non-owning reference to this; delete LineTerm before deleting self
  PRInt32 options = LTERM_NOSTDERR_FLAG;

  XMLT_LOG(mozXMLTerminal::Activate,22,("Opening LineTerm\n"));
  nsCOMPtr<nsIObserver> anObserver = this;
#ifdef NO_CALLBACK
  anObserver = nsnull;
#endif
  nsAutoString cookie;
  result = mLineTermAux->OpenAux(mCommand.GetUnicode(),
                                 mPromptExpr.GetUnicode(),
                                 options, LTERM_DETERMINE_PROCESS,
                                 mDOMDocument, anObserver, cookie);

  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to open LineTermAux\n");
    return result;
  }
  XMLT_LOG(mozXMLTerminal::Activate,22,("Opened LineTerm\n"));

  // Save cookie
  mCookie = cookie;

  if (mFirstInput.Length() > 0) {
    // Send first input command line
    result = SendTextAux(mFirstInput);
    if (NS_FAILED(result))
      return result;
  }

  // Get the DOM event receiver for document
  nsCOMPtr<nsIDOMEventReceiver> eventReceiver;
  result = mDOMDocument->QueryInterface(nsIDOMEventReceiver::GetIID(),
                                        getter_AddRefs(eventReceiver));
  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to get DOM receiver\n");
    return result;
  }

  // Create a key listener
  result = NS_NewXMLTermKeyListener(getter_AddRefs(mKeyListener), this);
  if (NS_OK != result) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to get key listener\n");
    return result;
  }

  // Register the key listener with the DOM event receiver
  result = eventReceiver->AddEventListenerByIID(mKeyListener,
                                                nsIDOMKeyListener::GetIID());

  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to register key listener\n");
    return result;
  }

  // Create a text listener
  result = NS_NewXMLTermTextListener(getter_AddRefs(mTextListener), this);
  if (NS_OK != result) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to get text listener\n");
    return result;
  }

  // Register the text listener with the DOM event receiver
  result = eventReceiver->AddEventListenerByIID(mTextListener,
                                                nsIDOMTextListener::GetIID());
  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to register text listener\n");
    return result;
  }

  // Create a mouse listener
  result = NS_NewXMLTermMouseListener(getter_AddRefs(mMouseListener), this);
  if (NS_OK != result) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to get mouse listener\n");
    return result;
  }

  // Register the mouse listener with the DOM event receiver
  result = eventReceiver->AddEventListenerByIID(mMouseListener,
                                                nsIDOMMouseListener::GetIID());
  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to register mouse listener\n");
    return result;
  }

  // Create a drag listener
  result = NS_NewXMLTermDragListener(getter_AddRefs(mDragListener), this);
  if (NS_OK != result) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to get drag listener\n");
    return result;
  }

  // Register the drag listener with the DOM event receiver
  result = eventReceiver->AddEventListenerByIID(mDragListener,
                                                nsIDOMDragListener::GetIID());
  if (NS_FAILED(result)) {
    XMLT_WARNING("mozXMLTerminal::Activate: Warning - Failed to register drag listener\n");
    return result;
  }

  return NS_OK;
}


// Transmit string to LineTerm (use saved cookie)
NS_IMETHODIMP mozXMLTerminal::SendTextAux(const nsString& aString)
{
  return SendText(aString, mCookie.GetUnicode());
}


// Transmit string to LineTerm
NS_IMETHODIMP mozXMLTerminal::SendText(const nsString& aString,
                                       const PRUnichar* aCookie)
{
  nsresult result;

  if (!mLineTermAux)
    return NS_ERROR_FAILURE;

  // Preprocess string and check if it is to be consumed
  PRBool consumed = false;
  result = mXMLTermSession->Preprocess(aString, consumed);

  if (!consumed) {
    result = mLineTermAux->Write(aString.GetUnicode(), aCookie);
    if (NS_FAILED(result)) {
      // Close LineTerm
      mLineTermAux->Close(aCookie);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}


// Paste data from clipboard to terminal
NS_IMETHODIMP mozXMLTerminal::Paste()
{
  nsresult result;
  nsAutoString pasteString;

  XMLT_LOG(mozXMLTerminal::Paste,20,("\n"));

  // Get Clipboard service
  NS_WITH_SERVICE (nsIClipboard, clipboard, kCClipboardCID, &result);
  if ( NS_FAILED(result) )
    return result;
    
  // Generic transferable for getting clipboard data
  nsCOMPtr<nsITransferable> trans;
  result = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), 
                                              (void**) getter_AddRefs(trans));

  if (NS_FAILED(result) || !trans)
    return NS_ERROR_FAILURE;

  // DataFlavors to get out of transferable
  trans->AddDataFlavor(kHTMLMime);
  trans->AddDataFlavor(kTextMime);

  // Get data from clipboard
  result = clipboard->GetData(trans);
  if (NS_FAILED(result))
    return result;

  char* bestFlavor = nsnull;
  nsCOMPtr<nsISupports> genericDataObj;
  PRUint32 objLen = 0;
  result = trans->GetAnyTransferData(&bestFlavor,
                                     getter_AddRefs(genericDataObj), &objLen);
  if (NS_FAILED(result))
    return result;

  nsAutoString flavor ( bestFlavor );

  char* temCStr = flavor.ToNewCString();
  XMLT_LOG(mozXMLTerminal::Paste,20,("flavour=%s\n", temCStr));
  nsAllocator::Free(temCStr);

  if (flavor.Equals(kHTMLMime)) {
    nsCOMPtr<nsISupportsWString> textDataObj ( do_QueryInterface(genericDataObj) );
    if (textDataObj && objLen > 0) {
      PRUnichar* text = nsnull;
      textDataObj->ToString ( &text );
      pasteString.SetString ( text, objLen / 2 );
      result = SendTextAux(pasteString);
    }

  } else if (flavor.Equals(kTextMime)) {
    nsCOMPtr<nsISupportsString> textDataObj ( do_QueryInterface(genericDataObj) );
    if (textDataObj && objLen > 0) {
      char* text = nsnull;
      textDataObj->ToString ( &text );
      pasteString.SetString ( text, objLen );
      result = SendTextAux(pasteString);
    }
  }

  nsAllocator::Free(bestFlavor);

  return NS_OK;
}


// Poll for readable data from LineTerm
NS_IMETHODIMP mozXMLTerminal::Poll(void)
{
  if (!mLineTermAux)
    return NS_ERROR_NOT_INITIALIZED;

  XMLT_LOG(mozXMLTerminal::Poll,20,("\n"));

  return mXMLTermSession->ReadAll(mLineTermAux);
}


// Handle callback from LineTerm when new input/output needs to be displayed
NS_IMETHODIMP mozXMLTerminal::Observe(nsISupports *aSubject,
                                  const PRUnichar *aTopic,
                                  const PRUnichar *someData)
{
  nsCOMPtr<mozILineTermAux> lineTermAux = do_QueryInterface(aSubject);
  PR_ASSERT(lineTermAux != nsnull);

  return mXMLTermSession->ReadAll(lineTermAux);
}


// Returns document associated with XMLTerminal
NS_IMETHODIMP mozXMLTerminal::GetDocument(nsIDOMDocument** aDoc)
{
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;
  *aDoc = nsnull;

  NS_PRECONDITION(mDOMDocument, "bad state, null mDOMDocument");
  if (!mDOMDocument)
    return NS_ERROR_NOT_INITIALIZED;
  return mDOMDocument->QueryInterface(nsIDOMDocument::GetIID(),
                                      (void **)aDoc);
}


// Returns presentation shell associated with XMLTerm
NS_IMETHODIMP mozXMLTerminal::GetPresShell(nsIPresShell** aPresShell)
{
  if (!aPresShell)
    return NS_ERROR_NULL_POINTER;
  *aPresShell = nsnull;

  NS_PRECONDITION(mPresShell, "bad state, null mPresShell");
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;
  return mPresShell->QueryInterface(nsIPresShell::GetIID(),
                                    (void **)aPresShell);
}

// nsIDocumentLoaderObserver methods
NS_IMETHODIMP
mozXMLTerminal::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL,
                                const char* aCommand)
{
   return NS_OK;
}

NS_IMETHODIMP
mozXMLTerminal::OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel,
                              nsresult aStatus,
                              nsIDocumentLoaderObserver * aObserver)
{

   return NS_OK;
}

NS_IMETHODIMP
mozXMLTerminal::OnStartURLLoad(nsIDocumentLoader* loader,
                                 nsIChannel* channel,
                                 nsIContentViewer* aViewer)
{

   return NS_OK;
}

NS_IMETHODIMP
mozXMLTerminal::OnProgressURLLoad(nsIDocumentLoader* loader,
                                    nsIChannel* channel, PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
mozXMLTerminal::OnStatusURLLoad(nsIDocumentLoader* loader,
                                  nsIChannel* channel, nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
mozXMLTerminal::OnEndURLLoad(nsIDocumentLoader* loader,
                               nsIChannel* channel, nsresult aStatus)
{
  XMLT_LOG(mozXMLTerminal::OnEndURLLoad,20,("\n"));

  // Activate XMLTerm
  Activate();
  return NS_OK;
}

NS_IMETHODIMP
mozXMLTerminal::HandleUnknownContentType(nsIDocumentLoader* loader, 
                                           nsIChannel* channel,
                                           const char *aContentType,
                                           const char *aCommand )
{
   return NS_OK;
}
