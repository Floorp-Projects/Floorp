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

// mozSimpleContainer.cpp: Implements mozISimpleContainer
// which provides a DocShell container for use in simple programs
// using the layout engine

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsISupports.h"
#include "nsRepository.h"

#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"

#include "nsIWebShell.h"
#include "nsIBaseWindow.h"

#include "mozSimpleContainer.h"


// Define Class IDs
static NS_DEFINE_IID(kWebShellCID,           NS_WEB_SHELL_CID);

// Define Interface IDs
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);

/////////////////////////////////////////////////////////////////////////
// mozSimpleContainer factory
/////////////////////////////////////////////////////////////////////////

nsresult
NS_NewSimpleContainer(mozISimpleContainer** aSimpleContainer)
{
    NS_PRECONDITION(aSimpleContainer != nsnull, "null ptr");
    if (!aSimpleContainer)
        return NS_ERROR_NULL_POINTER;

    *aSimpleContainer = new mozSimpleContainer();
    if (! *aSimpleContainer)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aSimpleContainer);
    return NS_OK;
}

/////////////////////////////////////////////////////////////////////////
// mozSimpleContainer implementation
/////////////////////////////////////////////////////////////////////////

mozSimpleContainer::mozSimpleContainer() :
  mDocShell(nsnull)
{
  NS_INIT_REFCNT();
}


mozSimpleContainer::~mozSimpleContainer()
{
  mDocShell = nsnull;
}

#define NS_IMPL_ADDREF_TRACE(_class)                         \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                \
{                                                            \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  \
  ++mRefCnt;                                                 \
  fprintf(stderr, "mozSimpleContainer:AddRef, mRefCnt=%d\n", mRefCnt);   \
  return mRefCnt;                                            \
}

#define NS_IMPL_RELEASE_TRACE(_class)                  \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)         \
{                                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  --mRefCnt;                                           \
  fprintf(stderr, "mozSimpleContainer:Release, mRefCnt=%d\n", mRefCnt); \
  if (mRefCnt == 0) {                                  \
    NS_DELETEXPCOM(this);                              \
    return 0;                                          \
  }                                                    \
  return mRefCnt;                                      \
}

// Implement AddRef and Release
NS_IMPL_ADDREF(mozSimpleContainer)
NS_IMPL_RELEASE(mozSimpleContainer)


NS_IMETHODIMP 
mozSimpleContainer::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kISupportsIID)) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(mozISimpleContainer*,this));

  } else if ( aIID.Equals(NS_GET_IID(mozISimpleContainer)) ) {
    *aInstancePtr = NS_STATIC_CAST(mozISimpleContainer*,this);

  } else if ( aIID.Equals(NS_GET_IID(nsIDocumentLoaderObserver)) ) {
    *aInstancePtr = NS_STATIC_CAST(nsIDocumentLoaderObserver*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  fprintf(stderr, "mozSimpleContainer::QueryInterface, mRefCnt = %d\n", mRefCnt);

  return NS_OK;
}


/** Initializes simple container for native window widget
 * @param aNativeWidget native window widget (e.g., GtkWidget)
 * @param width window width (pixels)
 * @param height window height (pixels)
 * @param aPref preferences object
 */
NS_IMETHODIMP mozSimpleContainer::Init(nsNativeWidget aNativeWidget,
                                       PRInt32 width, PRInt32 height,
                                       nsIPref* aPref)
{
  nsresult result;

  // Create doc shell and show it
  result = nsComponentManager::CreateInstance(kWebShellCID, nsnull,
                                              NS_GET_IID(nsIDocShell),
                                              getter_AddRefs(mDocShell));

  if (NS_FAILED(result) || !mDocShell) {
    fprintf(stderr, "Failed to create create doc shell\n");
    return NS_ERROR_FAILURE;
  }

  // We are the document loader observer for the doc shell
  mDocShell->SetDocLoaderObserver(this);

  // Initialize web shell and show it
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
  webShell->Init(aNativeWidget, 0, 0, width, height);

  if (aPref) {
    //mDocShell->SetPrefs(aPref);
  }

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  docShellWin->SetVisibility(PR_TRUE);

  return NS_OK;
}


// nsIDocumentLoaderObserver interface

NS_IMETHODIMP mozSimpleContainer::OnStartDocumentLoad(
                      nsIDocumentLoader *aLoader,
                      nsIURI *aURL,
                      const char *aCommand)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::OnEndDocumentLoad(nsIDocumentLoader *loader,
                                                    nsIChannel *aChannel,
                                                    PRUint32 aStatus)
{
  fprintf(stderr, "mozSimpleContainer::OnEndDocumentLoad\n");

  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::OnStartURLLoad(nsIDocumentLoader *aLoader,
                                                 nsIChannel *channel)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::OnProgressURLLoad(nsIDocumentLoader *aLoader,
                                                    nsIChannel *aChannel,
                                                    PRUint32 aProgress,
                                                    PRUint32 aProgressMax)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::OnStatusURLLoad(nsIDocumentLoader *loader,
                                                  nsIChannel *channel,
                                                  nsString & aMsg)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::OnEndURLLoad(nsIDocumentLoader *aLoader,
                                               nsIChannel *aChannel,
                                               PRUint32 aStatus)
{
  return NS_OK;
}


/** Resizes container to new dimensions
 * @param width new window width (pixels)
 * @param height new window height (pixels)
 */
NS_IMETHODIMP mozSimpleContainer::Resize(PRInt32 aWidth, PRInt32 aHeight)
{
  fprintf(stderr, "mozSimpleContainer::Resize\n");

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  docShellWin->SetPositionAndSize(0, 0, aWidth, aHeight, PR_FALSE);

  return NS_OK;
}


/** Loads specified URL into container
 * @param aURL URL string
 */
NS_IMETHODIMP mozSimpleContainer::LoadURL(const char* aURL)
{
  nsresult result;

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));

  nsString aStr(aURL);
  result = webShell->LoadURL(aStr.GetUnicode());
  return result;
}


/** Gets doc shell in container
 * @param aDocShell (output) doc shell object
 */
NS_IMETHODIMP mozSimpleContainer::GetDocShell(nsIDocShell*& aDocShell)
{
  aDocShell = mDocShell.get();
  NS_IF_ADDREF(aDocShell);      // Add ref'ed; needs to be released
  return NS_OK;
}
