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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// mozXMLTermShell.cpp: implementation of mozIXMLTermShell
// providing an XPCONNECT wrapper to the XMLTerminal interface,
// thus allowing easy (and controlled) access from scripts

#include <stdio.h>

#undef RAW_GTK_WINDOW

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#ifdef RAW_GTK_WINDOW
#include <gtk/gtk.h>
#include "gtkmozarea.h"
#include "gdksuperwin.h"
#include "mozISimpleContainer.h"
#else
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIBaseWindow.h"
#endif

#include "nsIDocumentViewer.h"
#include "nsIDocument.h"

#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIScriptGlobalObject.h"

#include "nsIServiceManager.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDOMDocument.h"
#include "nsIDOMSelection.h"
#include "nsIDOMWindow.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTermShell.h"

// Define Interface IDs
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);

// Define Class IDs
static NS_DEFINE_IID(kAppShellServiceCID,    NS_APPSHELL_SERVICE_CID);

#ifdef RAW_GTK_WINDOW
#else
static NS_DEFINE_IID(kWindowCID,             NS_WINDOW_CID);
static NS_DEFINE_IID(kWebShellCID,           NS_WEB_SHELL_CID);
#endif


/////////////////////////////////////////////////////////////////////////
// mozXMLTermShell factory
/////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXMLTermShell(mozIXMLTermShell** aXMLTermShell)
{
    NS_PRECONDITION(aXMLTermShell != nsnull, "null ptr");
    if (! aXMLTermShell)
        return NS_ERROR_NULL_POINTER;

    *aXMLTermShell = new mozXMLTermShell();
    if (! *aXMLTermShell)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aXMLTermShell);
    return NS_OK;
}

/////////////////////////////////////////////////////////////////////////
// mozXMLTermShell implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermShell::mozXMLTermShell() :
  mInitialized(PR_FALSE),
  mContentWindow(nsnull),
  mContentAreaDocShell(nsnull),
  mXMLTerminal(nsnull)
{
  NS_INIT_REFCNT();
}

mozXMLTermShell::~mozXMLTermShell()
{
  if (mInitialized) {
    Finalize();
  }
}


// Implement AddRef and Release
NS_IMPL_ADDREF(mozXMLTermShell)
NS_IMPL_RELEASE(mozXMLTermShell)


NS_IMETHODIMP 
mozXMLTermShell::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kISupportsIID)) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(mozIXMLTermShell*,this));

  } else if ( aIID.Equals(NS_GET_IID(mozIXMLTermShell)) ) {
    *aInstancePtr = NS_STATIC_CAST(mozIXMLTermShell*,this);

  } else if ( aIID.Equals(NS_GET_IID(nsIWebShellContainer)) ) {
    *aInstancePtr = NS_STATIC_CAST(nsIWebShellContainer*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}


NS_IMETHODIMP mozXMLTermShell::GetCurrentEntryNumber(PRInt32 *aNumber)
{
  if (mXMLTerminal) {
    return mXMLTerminal->GetCurrentEntryNumber(aNumber);
  } else {
    return NS_ERROR_NOT_INITIALIZED;
  }
}


/** Sets command history buffer count
 * @param aHistory history buffer count
 * @param aCookie document.cookie string for authentication
 */
NS_IMETHODIMP mozXMLTermShell::SetHistory(PRInt32 aHistory,
                                          const PRUnichar* aCookie)
{
  if (mXMLTerminal) {
    nsresult result;
    PRBool matchesCookie;
    result = mXMLTerminal->MatchesCookie(aCookie, &matchesCookie);
    if (NS_FAILED(result) || !matchesCookie)
      return NS_ERROR_FAILURE;

    return mXMLTerminal->SetHistory(aHistory);

  } else {
    return NS_ERROR_NOT_INITIALIZED;
  }
}


/** Sets command prompt
 * @param aPrompt command prompt string (HTML)
 * @param aCookie document.cookie string for authentication
 */
NS_IMETHODIMP mozXMLTermShell::SetPrompt(const PRUnichar* aPrompt,
                                         const PRUnichar* aCookie)
{
  if (mXMLTerminal) {
    nsresult result;
    PRBool matchesCookie;
    result = mXMLTerminal->MatchesCookie(aCookie, &matchesCookie);
    if (NS_FAILED(result) || !matchesCookie)
      return NS_ERROR_FAILURE;

    return mXMLTerminal->SetPrompt(aPrompt);

  } else {
    return NS_ERROR_NOT_INITIALIZED;
  }
}


/** Ignore key press events
 * (workaround for form input being transmitted to xmlterm)
 * @param aIgnore ignore flag (true/false)
 * @param aCookie document.cookie string for authentication
 */
NS_IMETHODIMP mozXMLTermShell::IgnoreKeyPress(const PRBool aIgnore,
                                              const PRUnichar* aCookie)
{
  if (mXMLTerminal) {
    nsresult result;
    PRBool matchesCookie;
    result = mXMLTerminal->MatchesCookie(aCookie, &matchesCookie);
    if (NS_FAILED(result) || !matchesCookie)
      return NS_ERROR_FAILURE;

    return mXMLTerminal->SetKeyIgnore(aIgnore);

  } else {
    return NS_ERROR_NOT_INITIALIZED;
  }
}


// Initialize XMLTermShell
NS_IMETHODIMP    
mozXMLTermShell::Init(nsIDOMWindow* aContentWin,
                      const PRUnichar* URL,
                      const PRUnichar* args)
{
  nsresult result;

  XMLT_LOG(mozXMLTermShell::Init,10,("\n"));

  if (mInitialized)
    return NS_ERROR_ALREADY_INITIALIZED;

  if (!aContentWin)
      return NS_ERROR_NULL_POINTER;

  mContentWindow = aContentWin;  // no addref

  nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(mContentWindow,
                                                                &result);
  if (NS_FAILED(result) || !globalObj)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_ERROR_FAILURE;
    
  mContentAreaDocShell = docShell;  // SVN: does this assignment addref?

  // Create XMLTerminal
  nsCOMPtr<mozIXMLTerminal> newXMLTerminal;
  result = NS_NewXMLTerminal(getter_AddRefs(newXMLTerminal));

  if(!newXMLTerminal)
    result = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(result)) {
    // Initialize XMLTerminal with non-owning reference to us
    result = newXMLTerminal->Init(mContentAreaDocShell, this, URL, args);

    if (NS_SUCCEEDED(result)) {
      mXMLTerminal = newXMLTerminal;
    }
  }

  return result;
}


/** Closes XMLterm, freeing resources
 * @param aCookie document.cookie string for authentication
 */
NS_IMETHODIMP
mozXMLTermShell::Close(const PRUnichar* aCookie)
{
  XMLT_LOG(mozXMLTermShell::Close,10,("\n"));

  if (mInitialized && mXMLTerminal) {
    nsresult result;
    PRBool matchesCookie;
    result = mXMLTerminal->MatchesCookie(aCookie, &matchesCookie);
    if (NS_FAILED(result) || !matchesCookie)
      return NS_ERROR_FAILURE;

    Finalize();
  }

  return NS_OK;
}


// De-initialize XMLTermShell and free resources
NS_IMETHODIMP
mozXMLTermShell::Finalize(void)
{
  XMLT_LOG(mozXMLTermShell::Finalize,10,("\n"));

  if (mXMLTerminal) {
    // Finalize and release reference to XMLTerm object owned by us
    mXMLTerminal->Finalize();
    mXMLTerminal = nsnull;
  }

  mContentAreaDocShell = nsnull;
  mContentWindow =       nsnull;

  mInitialized = PR_FALSE;

  return NS_OK;
}


// Poll for readable data from XMLTerminal
NS_IMETHODIMP mozXMLTermShell::Poll(void)
{
  if (!mXMLTerminal)
    return NS_ERROR_NOT_INITIALIZED;

  return mXMLTerminal->Poll();
}


/** Resizes XMLterm to match a resized window.
 */
NS_IMETHODIMP mozXMLTermShell::Resize(void)
{
  if (!mXMLTerminal)
    return NS_ERROR_NOT_INITIALIZED;

  return mXMLTerminal->Resize();
}


// Send string to LineTerm as if the user had typed it
NS_IMETHODIMP mozXMLTermShell::SendText(const PRUnichar* aString,
                                        const PRUnichar* aCookie)
{
  if (!mXMLTerminal)
    return NS_ERROR_FAILURE;

  nsAutoString sendStr (aString);

  XMLT_LOG(mozXMLTermShell::SendText,10,("length=%d\n", sendStr.Length()));

  return mXMLTerminal->SendText(sendStr, aCookie);
}


// Create new XMLTerm window with specified argument string
NS_IMETHODIMP
mozXMLTermShell::NewXMLTermWindow(const PRUnichar* args,
                                  nsIDOMWindow **_retval)
{
  nsresult result;
  PRInt32 width = 760;
  PRInt32 height = 400;

  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,10,("\n"));

  if (!_retval)
    return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;

  if (!mContentAreaDocShell)
    return NS_ERROR_FAILURE;

  // Get top window
  nsCOMPtr<nsIWebShell> contentAreaWebShell( do_QueryInterface(mContentAreaDocShell) );

  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check0, contWebShell=0x%x\n",
                                            (int) contentAreaWebShell.get()));

  nsCOMPtr<nsIWebShellContainer> topContainer = nsnull;
  result = contentAreaWebShell->GetTopLevelWindow(getter_AddRefs(topContainer));
  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check0, topContainer=0x%x\n",
                                                (int) topContainer.get()));

  nsCOMPtr<nsIWebShellWindow> topWin( do_QueryInterface(topContainer) );
  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check0, topWin=0x%x\n",
                                                (int) topWin.get()));

  // Determine preferences
  nsCOMPtr<nsIPref> prefs = nsnull;
  result = mContentAreaDocShell->GetPrefs(getter_AddRefs(prefs));

#ifdef RAW_GTK_WINDOW // Create window using raw GTK calls
  GtkWidget *mainWin = NULL;
  GtkWidget *mozArea = NULL;
  GdkSuperWin *superWin = NULL;

  mainWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size( GTK_WINDOW(mainWin), width, height);
  gtk_window_set_title(GTK_WINDOW(mainWin), "XMLterm2");
  
  mozArea = gtk_mozarea_new();
  gtk_container_add(GTK_CONTAINER(mainWin), mozArea);
  gtk_widget_realize(mozArea);
  gtk_widget_show(mozArea);
  superWin = GTK_MOZAREA(mozArea)->superwin;

  gdk_window_show(superWin->bin_window);                                       
  gdk_window_show(superWin->shell_window);

  gtk_widget_show(mainWin);


  // Create simple container
  nsCOMPtr<mozISimpleContainer> gSimpleContainer = nsnull;
  result = NS_NewSimpleContainer(getter_AddRefs(gSimpleContainer));

  if (NS_FAILED(result) || !gSimpleContainer) {
    return result; // Exit main program
  }

  // Determine window dimensions
  GtkAllocation *alloc = &GTK_WIDGET(mainWin)->allocation;
    
  // Initialize container it to hold a doc shell
  result = gSimpleContainer->Init((nsNativeWidget *) superWin,
                              alloc->width, alloc->height, prefs);

  if (NS_FAILED(result)) {
    return result; // Exit main program
  }

  // Get reference to doc shell embedded in a simple container
  nsCOMPtr<nsIDocShell> docShell;
  result = gSimpleContainer->GetDocShell(*getter_AddRefs(docShell));

  if (NS_FAILED(result) || !docShell) {
    return result; // Exit main program
  }

  // Load initial XMLterm document
  result = gSimpleContainer->LoadURL(
                                 "file:///home/svn/mysrc/web/home/links.html");
  if (NS_FAILED(result))
    return result;

  ///nsCOMPtr<nsIWebShell> webShell( do_QueryInterface(docShell) );
  ///nsString aStr("file:///home/svn/mysrc/web/home/links.html");
  ///result = webShell->LoadURL(aStr.GetUnicode());
  ///if (NS_FAILED(result))
  ///  return result;

  // Return new DOM window
  result = mozXMLTermUtils::ConvertDocShellToDOMWindow(docShell, _retval);
  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check8, *_retval=0x%x\n",
                                                *_retval));
  if (NS_FAILED(result) || !*_retval)
    return NS_ERROR_FAILURE;

#else            // Use nsIAppShellService

  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check4\n"));

  // Create the Application Shell instance...
  nsIAppShellService* appShellSvc = nsnull;
  result = nsServiceManager::GetService(kAppShellServiceCID,
                                        NS_GET_IID(nsIAppShellService),
                                        (nsISupports**)&appShellSvc);

  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check5\n"));
  if (NS_FAILED(result) || !appShellSvc)
      return NS_ERROR_FAILURE;

  // Create top level window
  nsCOMPtr<nsIWebShellWindow> webShellWin;
  nsCOMPtr<nsIURI> uri = nsnull;
  //nsCAutoString urlCString("chrome://xmlterm/content/xmlterm.html");
  //result = uri->SetSpec(urlCString.GetBuffer());

  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check6\n"));
  if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

  appShellSvc->CreateTopLevelWindow((nsIWebShellWindow*) nsnull,
                                    uri,
                                    PR_TRUE,
                                    PR_FALSE,
                                    (PRUint32) 0,
                                    (nsIXULWindowCallbacks*)nsnull,
                                    width, height,
                                    getter_AddRefs(webShellWin));

  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check7, webShellWin=0x%x\n",
                                                webShellWin.get()));
  if (NS_FAILED(result))
    return result;

  // Return new DOM window
  result = webShellWin->GetDOMWindow(_retval);
  XMLT_LOG(mozXMLTermShell::NewXMLTermWindow,0,("check8, *_retval=0x%x\n",
                                                *_retval));
  if (NS_FAILED(result) || !*_retval)
    return NS_ERROR_FAILURE;

#endif

  return NS_OK;
}


// Exit XMLTerm window
NS_IMETHODIMP    
mozXMLTermShell::Exit()
{  
  nsIAppShellService* appShell = nsnull;

  XMLT_LOG(mozXMLTermShell::Exit,10,("\n"));

  // Create the Application Shell instance...
  nsresult result = nsServiceManager::GetService(kAppShellServiceCID,
                                                 NS_GET_IID(nsIAppShellService),
                                                 (nsISupports**)&appShell);
  if (NS_SUCCEEDED(result)) {
    appShell->Shutdown();
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  } 
  return NS_OK;
}


NS_IMETHODIMP mozXMLTermShell::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
  XMLT_LOG(mozXMLTermShell::WillLoadURL,0,("\n"));
  return NS_OK;
}


NS_IMETHODIMP mozXMLTermShell::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  XMLT_LOG(mozXMLTermShell::BeginLoadURL,0,("\n"));
  return NS_OK;
}


NS_IMETHODIMP mozXMLTermShell::ProgressLoadURL(nsIWebShell* aShell,
          const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  XMLT_LOG(mozXMLTermShell::ProgressLoadURL,0,("\n"));
  return NS_OK;
}


NS_IMETHODIMP mozXMLTermShell::EndLoadURL(nsIWebShell* aShell,
                                             const PRUnichar* aURL,
                                             nsresult aStatus)
{
  XMLT_LOG(mozXMLTermShell::EndLoadURL,0,("\n"));
  return NS_OK;
}
