/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// mozXMLTermShell.cpp: implementation of mozIXMLTermShell
// providing an XPCONNECT wrapper to the XMLTerminal interface,
// thus allowing easy (and controlled) access from scripts

#include <stdio.h>

#undef NEW_XMLTERM_IMP

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDocumentViewer.h"
#include "nsIDocument.h"

#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIScriptGlobalObject.h"

#include "nsIServiceManager.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDOMDocument.h"
#include "nsISelection.h"
#include "nsIDOMWindowInternal.h"

#include "mozXMLT.h"
#include "mozLineTerm.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTermShell.h"

// Define Class IDs
static NS_DEFINE_IID(kAppShellServiceCID,    NS_APPSHELL_SERVICE_CID);


/////////////////////////////////////////////////////////////////////////
// mozXMLTermShell implementation
/////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR(mozXMLTermShell)

NS_IMPL_THREADSAFE_ISUPPORTS1(mozXMLTermShell, 
                              mozIXMLTermShell)

PRBool mozXMLTermShell::mLoggingInitialized = PR_FALSE;

NS_METHOD
mozXMLTermShell::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{

  if (!mLoggingInitialized) {
    // Set initial debugging message level for XMLterm
    int messageLevel = 0;
    char* debugStr = (char*) PR_GetEnv("XMLT_DEBUG");
                      
    if (debugStr && (strlen(debugStr) == 1)) {
      messageLevel = 98;
      debugStr = nsnull;
    }

    tlog_init(stderr);
    tlog_set_level(XMLT_TLOG_MODULE, messageLevel, debugStr);
    mLoggingInitialized = PR_TRUE;
  }

  return mozXMLTermShellConstructor( aOuter,
                                  aIID,
                                  aResult );
}

NS_METHOD
mozXMLTermShell::RegisterProc(nsIComponentManager *aCompMgr,
                              nsIFile *aPath,
                              const char *registryLocation,
                              const char *componentType,
                              const nsModuleComponentInfo *info)
{
  // Component specific actions at registration time
  PR_LogPrint("mozXMLTermShell::RegisterProc: registered mozXMLTermShell\n");
  return NS_OK;
}

NS_METHOD
mozXMLTermShell::UnregisterProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const nsModuleComponentInfo *info)
{
  // Component specific actions at unregistration time
  return NS_OK;  // Return value is not used
}


mozXMLTermShell::mozXMLTermShell() :
  mInitialized(PR_FALSE),
  mContentWindow(nsnull),
  mContentAreaDocShell(nsnull),
  mXMLTerminal(nsnull)
{
}

mozXMLTermShell::~mozXMLTermShell()
{
  Finalize();
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


/** Exports HTML to file, with META REFRESH, if refreshSeconds is non-zero.
 * Nothing is done if display has not changed since last export, unless
 * forceExport is true. Returns true if export actually takes place.
 * If filename is a null string, HTML is written to STDERR.
 */
NS_IMETHODIMP mozXMLTermShell::ExportHTML(const PRUnichar* aFilename,
                                          PRInt32 permissions,
                                          const PRUnichar* style,
                                          PRUint32 refreshSeconds,
                                          PRBool forceExport,
                                          const PRUnichar* aCookie,
                                          PRBool* exported)
{
  if (!mXMLTerminal)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult result;
  PRBool matchesCookie;
  result = mXMLTerminal->MatchesCookie(aCookie, &matchesCookie);
  if (NS_FAILED(result) || !matchesCookie)
    return NS_ERROR_FAILURE;

  return mXMLTerminal->ExportHTML( aFilename, permissions, style,
                                   refreshSeconds, forceExport,
                                   exported);
}

/** Ignore key press events
 * (workaround for form input being transmitted to xmlterm)
 * @param aIgnore ignore flag (PR_TRUE/PR_FALSE)
 * @param aCookie document.cookie string for authentication
 */
NS_IMETHODIMP mozXMLTermShell::IgnoreKeyPress(PRBool aIgnore,
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
mozXMLTermShell::Init(nsIDOMWindowInternal* aContentWin,
                      const PRUnichar* URL,
                      const PRUnichar* args)
{
  nsresult result;

  XMLT_LOG(mozXMLTermShell::Init,10,("\n"));

  if (mInitialized)
    return NS_ERROR_ALREADY_INITIALIZED;

  if (!aContentWin)
      return NS_ERROR_NULL_POINTER;

  mInitialized = PR_TRUE;

  mContentWindow = aContentWin;  // no addref

  nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(mContentWindow,
                                                                &result);
  if (NS_FAILED(result) || !globalObj)
    return NS_ERROR_FAILURE;

  nsIDocShell *docShell = globalObj->GetDocShell();
  if (!docShell)
    return NS_ERROR_FAILURE;
    
  mContentAreaDocShell = docShell;  // SVN: does this assignment addref?

  // Create XMLTerminal
  nsCOMPtr<mozIXMLTerminal> newXMLTerminal = do_CreateInstance(
                                                MOZXMLTERMINAL_CONTRACTID,
                                                &result);
  if(NS_FAILED(result))
    return result;

  // Initialize XMLTerminal with non-owning reference to us
  result = newXMLTerminal->Init(mContentAreaDocShell, this, URL, args);

  if (NS_FAILED(result))
    return result;

  mXMLTerminal = newXMLTerminal;
  return NS_OK;
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
  if (!mInitialized)
    return NS_OK;

  XMLT_LOG(mozXMLTermShell::Finalize,10,("\n"));

  mInitialized = PR_FALSE;

  if (mXMLTerminal) {
    // Finalize and release reference to XMLTerm object owned by us
    mXMLTerminal->Finalize();
    mXMLTerminal = nsnull;
  }

  mContentAreaDocShell = nsnull;
  mContentWindow =       nsnull;

  XMLT_LOG(mozXMLTermShell::Finalize,12,("END\n"));

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

  XMLT_LOG(mozXMLTermShell::SendText,10,("\n"));

  return mXMLTerminal->SendText(aString, aCookie);
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
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  } 
  return NS_OK;
}
