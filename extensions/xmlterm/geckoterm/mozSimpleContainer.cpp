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

// mozSimpleContainer.cpp: Implements mozISimpleContainer
// which provides a WebShell container for use in simple programs
// using the layout engine

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsRepository.h"

#include "nsISupports.h"

#include "nsIWebShell.h"
#include "nsIBaseWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"

#include "mozSimpleContainer.h"


// Define Class IDs
static NS_DEFINE_IID(kWebShellCID,           NS_WEB_SHELL_CID);

// Define Interface IDs
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID,          NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,       NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,    NS_IDOCUMENT_VIEWER_IID);

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
  mWebShell(nsnull)
{
  NS_INIT_REFCNT();
}


mozSimpleContainer::~mozSimpleContainer()
{
  mWebShell = nsnull;
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

  } else if ( aIID.Equals(mozISimpleContainer::GetIID()) ) {
    *aInstancePtr = NS_STATIC_CAST(mozISimpleContainer*,this);

  } else if ( aIID.Equals(nsIWebShellContainer::GetIID()) ) {
    *aInstancePtr = NS_STATIC_CAST(nsIWebShellContainer*,this);

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
  // Create web shell and show it
  nsresult result = nsRepository::CreateInstance(kWebShellCID, nsnull,
                                                 kIWebShellIID,
                                                 getter_AddRefs(mWebShell));

  if (NS_FAILED(result) || !mWebShell) {
    fprintf(stderr, "Failed to create create web shell\n");
    return NS_ERROR_FAILURE;
  }
  
  mWebShell->Init(aNativeWidget, 0, 0, width, height);

  mWebShell->SetContainer(this);
  if (aPref) {
    mWebShell->SetPrefs(aPref);
  }

  nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(mWebShell);
  if (window) {
    window->SetVisibility(PR_TRUE);
  }

  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::ProgressLoadURL(nsIWebShell* aShell,
          const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::EndLoadURL(nsIWebShell* aShell,
                                             const PRUnichar* aURL,
                                             nsresult aStatus)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult result;

  if (aShell == mWebShell.get()) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    nsCOMPtr<nsIPresShell> presShell;

    result = GetDocument(*getter_AddRefs(domDoc));
    if (NS_FAILED(result) || !domDoc) return result;

    result = GetPresShell(*getter_AddRefs(presShell));
    if (NS_FAILED(result) || !presShell) return result;
  }

  return NS_OK;
}


NS_IMETHODIMP mozSimpleContainer::NewWebShell(PRUint32 aChromeMask,
                                              PRBool aVisible,
                                              nsIWebShell*& aNewWebShell)
{
  aNewWebShell = nsnull;
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP mozSimpleContainer::FindWebShellWithName(const PRUnichar* aName,
                                                       nsIWebShell*& aResult)
{
  aResult = nsnull;
  nsString aNameStr(aName);

  nsIWebShell *aWebShell;
    
  if (NS_OK == GetWebShell(aWebShell)) {
    const PRUnichar *name;
    if (NS_OK == aWebShell->GetName(&name)) {
      if (aNameStr.Equals(name)) {
        aResult = aWebShell;
        NS_ADDREF(aResult);
        return NS_OK;
      }
    }      
  }

  if (NS_OK == aWebShell->FindChildWithName(aName, aResult)) {
    if (nsnull != aResult) {
      return NS_OK;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
mozSimpleContainer::ContentShellAdded(nsIWebShell* aChildShell,
                                  nsIContent* frameNode)
{
  return NS_OK;
}


NS_IMETHODIMP
mozSimpleContainer::CreatePopup(nsIDOMElement* aElement, 
                            nsIDOMElement* aPopupContent, 
                            PRInt32 aXPos, PRInt32 aYPos, 
                            const nsString& aPopupType,
                            const nsString& anAnchorAlignment,
                            const nsString& aPopupAlignment,
                            nsIDOMWindow* aWindow, nsIDOMWindow** outPopup)
{
  return NS_OK;
}


NS_IMETHODIMP
mozSimpleContainer::FocusAvailable(nsIWebShell* aFocusedWebShell,
                                   PRBool& aFocusTaken)
{
  return NS_OK;
}


/** Resizes container to new dimensions
 * @param width new window width (pixels)
 * @param height new window height (pixels)
 */
NS_IMETHODIMP mozSimpleContainer::Resize(PRInt32 aWidth, PRInt32 aHeight)
{
  if (!mWebShell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(mWebShell);
  if (window) {
    window->SetPositionAndSize(0, 0, aWidth, aHeight, PR_FALSE);
  }
  return NS_OK;
}


/** Loads specified URL into container
 * @param aURL URL string
 */
NS_IMETHODIMP mozSimpleContainer::LoadURL(const char* aURL)
{
  if (!mWebShell) return NS_ERROR_FAILURE;

  nsString aStr(aURL);
  mWebShell->LoadURL(aStr.GetUnicode());
  return NS_OK;
}


/** Gets web shell in container
 * @param aWebShell (output) web shell object
 */
NS_IMETHODIMP mozSimpleContainer::GetWebShell(nsIWebShell*& aWebShell)
{
  aWebShell = mWebShell.get();
  NS_IF_ADDREF(aWebShell);      // Add ref'ed; needs to be released
  return NS_OK;
}


/** Gets DOM document in container
 * @param aDocument (output) DOM document
 */
NS_IMETHODIMP mozSimpleContainer::GetDocument(nsIDOMDocument*& aDocument)
{

  aDocument = nsnull;

  if (mWebShell) {
    nsIContentViewer* contViewer;
    mWebShell->GetContentViewer(&contViewer);

    if (nsnull != contViewer) {
      nsIDocumentViewer* docViewer;
      if (NS_OK == contViewer->QueryInterface(kIDocumentViewerIID,
                                            (void**) &docViewer)) 
      {
        nsIDocument* vDoc;
        docViewer->GetDocument(vDoc);

        if (nsnull != vDoc) {
          nsIDOMDocument* vDOMDoc;
          if (NS_OK == vDoc->QueryInterface(kIDOMDocumentIID,
                                            (void**) &vDOMDoc)) 
            {
              aDocument = vDOMDoc; // Add ref'ed; needs to be released
            }
          NS_RELEASE(vDoc);
        }

        NS_RELEASE(docViewer);
      }

      NS_RELEASE(contViewer);
    }
  }
  return NS_OK;
}


/** Gets presentation shell associated with container
 * @param aPresShell (output) presentation shell
 */
NS_IMETHODIMP mozSimpleContainer::GetPresShell(nsIPresShell*& aPresShell)
{
  aPresShell = nsnull;

  nsIPresShell* presShell = nsnull;

  if (mWebShell) {
    nsIContentViewer* contViewer = nsnull;
    mWebShell->GetContentViewer(&contViewer);

    if (nsnull != contViewer) {
      nsIDocumentViewer* docViewer = nsnull;
      contViewer->QueryInterface(kIDocumentViewerIID, (void**) &docViewer);

      if (nsnull != docViewer) {
        nsIPresContext* presContext;
        docViewer->GetPresContext(presContext);

        if (nsnull != presContext) {
          presContext->GetShell(&presShell);  // Add ref'ed
          aPresShell = presShell;
          NS_RELEASE(presContext);
        }

        NS_RELEASE(docViewer);
      }

      NS_RELEASE(contViewer);
    }
  }
  return NS_OK;
}
