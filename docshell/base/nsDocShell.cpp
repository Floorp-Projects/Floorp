/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsDocShell.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIDeviceContext.h"
#include "nsCURILoader.h"
#include "nsLayoutCID.h"
#include "nsNeckoUtil.h"
#include "nsRect.h"
#include "prprf.h"
#include "nsIFrame.h"
#include "nsIContent.h"

#ifdef XXX_NS_DEBUG       // XXX: we'll need a logging facility for debugging
#define WEB_TRACE(_bit,_args)            \
  PR_BEGIN_MACRO                         \
    if (WEB_LOG_TEST(gLogModule,_bit)) { \
      PR_LogPrint _args;                 \
    }                                    \
  PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif




//*****************************************************************************
//***    nsDocShell: Object Management
//*****************************************************************************

nsDocShell::nsDocShell() : mCreated(PR_FALSE), mContentListener(nsnull),
   //XXX Remove HTML Specific ones
   mAllowPlugins(PR_TRUE), mMarginWidth(0), mMarginHeight(0), 
   mIsFrame(PR_FALSE)

{
	NS_INIT_REFCNT();
   mBaseInitInfo = new nsDocShellInitInfo();
}

nsDocShell::~nsDocShell()
{
   if(mBaseInitInfo)
      {
      delete mBaseInitInfo;
      mBaseInitInfo = nsnull;
      }

   if(mContentListener)
      {
      mContentListener->DocShell(nsnull);
      mContentListener->Release();
      mContentListener = nsnull;
      }
}

NS_IMETHODIMP nsDocShell::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsDocShell* docShell = new  nsDocShell();
	NS_ENSURE_TRUE(docShell, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(docShell);
	nsresult rv = docShell->QueryInterface(aIID, ppv);
	NS_RELEASE(docShell);  
	return rv;
}

//*****************************************************************************
// nsDocShell::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS7(nsDocShell, nsIDocShell, nsIDocShellEdit, 
   nsIDocShellFile, nsIGenericWindow, nsIScrollable, nsITextScroll, 
   nsIHTMLDocShell)

//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::LoadURI(nsIURI* aUri, 
   nsIPresContext* presContext)
{
   //NS_ENSURE_ARG(aUri);  // Done in LoadURIVia for us.

   return LoadURIVia(aUri, presContext, 0);
}

NS_IMETHODIMP nsDocShell::LoadURIVia(nsIURI* aUri, 
   nsIPresContext* aPresContext, PRUint32 aAdapterBinding)
{
   NS_ENSURE_ARG(aUri);

   nsCOMPtr<nsIURILoader> uriLoader = do_CreateInstance(NS_URI_LOADER_PROGID);
   NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
   mContentListener->SetPresContext(aPresContext);

   NS_ENSURE_SUCCESS(uriLoader->OpenURI(aUri, nsnull, nsnull, mContentListener,
      nsnull, nsnull, getter_AddRefs(mLoadCookie)), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetDocument(nsIDOMDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_STATE(mContentViewer);

  nsCOMPtr<nsIPresShell> presShell;
  NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument>doc;
  NS_ENSURE_SUCCESS(presShell->GetDocument(getter_AddRefs(doc)), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

  // the result's addref comes from this QueryInterface call
  NS_ENSURE_SUCCESS(CallQueryInterface(doc, aDocument), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetCurrentURI(nsIURI** aURI)
{
   NS_ENSURE_ARG_POINTER(aURI);
   
   *aURI = mCurrentURI;
   NS_IF_ADDREF(*aURI);

   return NS_OK;
}

// SetDocument is only meaningful for doc shells that support DOM documents.  Not all do.
NS_IMETHODIMP
nsDocShell::SetDocument(nsIDOMDocument *aDOMDoc, nsIDOMElement *aRootNode)
{

  // The tricky part is bypassing the normal load process and just putting a document into
  // the webshell.  This is particularly nasty, since webshells don't normally even know
  // about their documents

  // (1) Create a document viewer 
  nsCOMPtr<nsIContentViewer> documentViewer;
  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;
  static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
  NS_ENSURE_SUCCESS(nsComponentManager::CreateInstance(kLayoutDocumentLoaderFactoryCID, nsnull, 
                                                       nsIDocumentLoaderFactory::GetIID(),
                                                       (void**)getter_AddRefs(docFactory)),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDOMDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(docFactory->CreateInstanceForDocument(this,
                                                          doc,
                                                          "view",
                                                          getter_AddRefs(documentViewer)),
                    NS_ERROR_FAILURE); 

  // (2) Feed the docshell to the content viewer
  NS_ENSURE_SUCCESS(documentViewer->SetContainer(this), NS_ERROR_FAILURE);

  // (3) Tell the content viewer container to embed the content viewer.
  //     (This step causes everything to be set up for an initial flow.)
  NS_ENSURE_SUCCESS(Embed(documentViewer, "view", nsnull), NS_ERROR_FAILURE);

  // XXX: It would be great to get rid of this dummy channel!
  const nsAutoString uriString = "about:blank";
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), uriString), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(uri, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIChannel> dummyChannel;
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(dummyChannel), uri, nsnull), NS_ERROR_FAILURE);

  // (4) fire start document load notification
  nsIStreamListener* outStreamListener=nsnull;  // a valid pointer is required for the returned stream listener
  NS_ENSURE_SUCCESS(doc->StartDocumentLoad("view", dummyChannel, nsnull, this, &outStreamListener), 
                    NS_ERROR_FAILURE);
  NS_IF_RELEASE(outStreamListener);
  NS_ENSURE_SUCCESS(FireStartDocumentLoad(mDocLoader, uri, "load"), NS_ERROR_FAILURE);

  // (5) hook up the document and its content
  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(aRootNode);
  NS_ENSURE_TRUE(doc, NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_SUCCESS(rootContent->SetDocument(doc, PR_FALSE), NS_ERROR_FAILURE);
  doc->SetRootContent(rootContent);

  // (6) reflow the document
  //XXX: SetScrolling doesn't make any sense
  //SetScrolling(-1, PR_FALSE);
  PRInt32 i;
  PRInt32 ns = doc->GetNumberOfShells();
  for (i = 0; i < ns; i++) 
  {
    nsCOMPtr<nsIPresShell> shell(dont_AddRef(doc->GetShellAt(i)));
    if (shell) 
    {
      // Make shell an observer for next time
      NS_ENSURE_SUCCESS(shell->BeginObservingDocument(), NS_ERROR_FAILURE);

      // Resize-reflow this time
      nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(documentViewer);
      NS_ENSURE_TRUE(docViewer, NS_ERROR_OUT_OF_MEMORY);
      nsCOMPtr<nsIPresContext> presContext;
      NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(presContext, NS_ERROR_OUT_OF_MEMORY);
      float p2t;
      presContext->GetScaledPixelsToTwips(&p2t);

      nsRect r;
      NS_ENSURE_SUCCESS(GetPosition(&r.x, &r.y), NS_ERROR_FAILURE);;
      NS_ENSURE_SUCCESS(GetSize(&r.width, &r.height), NS_ERROR_FAILURE);;
      NS_ENSURE_SUCCESS(shell->InitialReflow(NSToCoordRound(r.width * p2t), NSToCoordRound(r.height * p2t)), NS_ERROR_FAILURE);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
      if (vm) 
      {
        PRBool enabled;
        documentViewer->GetEnableRendering(&enabled);
        if (enabled) {
          vm->EnableRefresh();
        }
        NS_ENSURE_SUCCESS(vm->SetWindowDimensions(NSToCoordRound(r.width * p2t), 
                                                  NSToCoordRound(r.height * p2t)), 
                          NS_ERROR_FAILURE);
      }
    }
  }

  // (7) fire end document load notification
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocumentLoaderObserver> dlObserver; 
  // XXX: this was just "this", and webshell container relied on getting a webshell
  // through this interface.  No one else uses it anywhere afaict  
  //if (!dlObserver) { return NS_ERROR_NO_INTERFACE; }
  NS_ENSURE_SUCCESS(FireEndDocumentLoad(mDocLoader, dummyChannel, rv, dlObserver), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);  // test the resulting out-param separately

  return NS_OK;
}



// caller is responsible for calling nsString::Recycle(*aName);
NS_IMETHODIMP nsDocShell::GetName(PRUnichar** aName)
{
   NS_ENSURE_ARG_POINTER(aName);
   *aName = nsnull;
   if(0 != mName.Length())
      *aName = mName.ToNewUnicode();
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetName(const PRUnichar* aName)
{
  if (aName) {
    mName = aName;  // this does a copy of aName
  }
  else {
    mName = "";
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPresContext(nsIPresContext** aPresContext)
{
   NS_ENSURE_ARG_POINTER(aPresContext);

   if(!mContentViewer)
      {
      *aPresContext = nsnull;
      return NS_OK;
      }

   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
   NS_ENSURE_TRUE(docv, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(docv->GetPresContext(*aPresContext), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParent(nsIDocShell** parent)
{
   NS_ENSURE_ARG_POINTER(parent);

   *parent = mParent;
   NS_IF_ADDREF(*parent);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParent(nsIDocShell* aParent)
{
  // null aParent is ok
   /*
   Note this doesn't do an addref on purpose.  This is because the parent
   is an implied lifetime.  We don't want to create a cycle by refcounting
   the parent.
   */
   mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentURIContentListener(nsIURIContentListener**
   aParent)
{
   NS_ENSURE_ARG_POINTER(aParent);
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   mContentListener->GetParentContentListener(aParent);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentURIContentListener(nsIURIContentListener*
   aParent)
{
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   mContentListener->SetParentContentListener(aParent);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPrefs(nsIPref** aPrefs)
{
   NS_ENSURE_ARG_POINTER(aPrefs);

   *aPrefs = mPrefs;
   NS_IF_ADDREF(*aPrefs);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetPrefs(nsIPref* aPrefs)
{
  // null aPrefs is ok
  mPrefs = aPrefs;    // this assignment does an addref
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetRootDocShell(nsIDocShell** aRootDocShell)
{
  NS_ENSURE_ARG_POINTER(aRootDocShell);
  *aRootDocShell = this;

  nsCOMPtr<nsIDocShell> parent;
  NS_ENSURE_TRUE(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
  while (parent)
  {
    *aRootDocShell = parent;
    NS_ENSURE_TRUE(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
  }
  NS_IF_ADDREF(*aRootDocShell);
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetZoom(float* zoom)
{
   NS_ENSURE_ARG_POINTER(zoom);
   NS_ENSURE_STATE(mContentViewer);

   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);

   nsCOMPtr<nsIDeviceContext> deviceContext;
   NS_ENSURE_SUCCESS(presContext->GetDeviceContext(getter_AddRefs(deviceContext)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(deviceContext->GetZoom(*zoom), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetZoom(float zoom)
{
   NS_ENSURE_STATE(mContentViewer);

   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);

   nsCOMPtr<nsIDeviceContext> deviceContext;
   NS_ENSURE_SUCCESS(presContext->GetDeviceContext(getter_AddRefs(deviceContext)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(deviceContext->SetZoom(zoom), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP 
nsDocShell::GetDocLoaderObserver(nsIDocumentLoaderObserver * *aDocLoaderObserver)
{
  NS_ENSURE_ARG_POINTER(aDocLoaderObserver);

  *aDocLoaderObserver = mDocLoaderObserver;
  NS_IF_ADDREF(*aDocLoaderObserver);
  return NS_OK;
}

NS_IMETHODIMP 
nsDocShell::SetDocLoaderObserver(nsIDocumentLoaderObserver * aDocLoaderObserver)
{
  // it's legal for aDocLoaderObserver to be null.  
  mDocLoaderObserver = aDocLoaderObserver;
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellEdit
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::Search()
{
  NS_WARN_IF_FALSE(PR_FALSE, "Subclasses should override this method!!!!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShell::GetSearchable(PRBool* aSearchable)
{
   NS_ENSURE_ARG_POINTER(aSearchable);
   *aSearchable = PR_FALSE;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ClearSelection()
{
   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(presShell->GetSelection(SELECTION_NORMAL, 
      getter_AddRefs(selection)), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(selection->ClearSelection(), NS_ERROR_FAILURE);

   return NS_OK;
}

/* the basic idea here is to grab the topmost content object
 * (for HTML documents, that's the BODY)
 * and select all it's children
 */
NS_IMETHODIMP nsDocShell::SelectAll()
{
  NS_ENSURE_STATE(mContentViewer);
  nsCOMPtr<nsIPresShell> presShell;
  NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  // get the selection object
  nsCOMPtr<nsIDOMSelection> selection;
  NS_ENSURE_SUCCESS(presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection)), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  // get the document
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsAutoString bodyTag = "body";
  nsCOMPtr<nsIDOMDocument> document;
  NS_ENSURE_SUCCESS(GetDocument(getter_AddRefs(document)), NS_ERROR_FAILURE);

  // get the body tag(s) in the document
  NS_ENSURE_SUCCESS(document->GetElementsByTagName(bodyTag, 
                                                   getter_AddRefs(nodeList)), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(nodeList, NS_OK); // this means the document has no body, so nothing to select

  // verify this document has exactly one body node
  PRUint32 count; 
  NS_ENSURE_SUCCESS(nodeList->GetLength(&count), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(count != 1, NS_OK); // could be true for a frameset doc

  // select all children of the body
  nsCOMPtr<nsIDOMNode> bodyNode;
  NS_ENSURE_SUCCESS(nodeList->Item(0, getter_AddRefs(bodyNode)), NS_ERROR_FAILURE); 
  NS_ENSURE_TRUE(bodyNode, NS_ERROR_FAILURE);
    // start the selection in front of the first child
  NS_ENSURE_SUCCESS(selection->Collapse(bodyNode, 0), NS_ERROR_FAILURE);
    // end the selection after the last child
  PRInt32 numBodyChildren=0;
  nsCOMPtr<nsIDOMNode>lastChild;
  NS_ENSURE_SUCCESS(bodyNode->GetLastChild(getter_AddRefs(lastChild)), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(lastChild, NS_OK); // body isn't required to have any children, so it's ok if lastChild is null.
                                    // in this case, we just have a collapsed selection at (body, 0)
  NS_ENSURE_SUCCESS(GetChildOffset(lastChild, bodyNode, &numBodyChildren), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(selection->Extend(bodyNode, numBodyChildren+1), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::CopySelection()
{
   NS_ENSURE_STATE(mContentViewer);
   PRBool copyable;
   NS_ENSURE_SUCCESS(GetCopyable(&copyable), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(copyable, NS_ERROR_UNEXPECTED);

   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   // the pres shell knows how to copy, so let it do the work
   NS_ENSURE_SUCCESS(presShell->DoCopy(), NS_ERROR_FAILURE);
   return NS_OK;
}

/* the docShell is "copyable" if it has a selection and the selection is not
 * collapsed (that is, at least one token is selected, a character or a node
 */
NS_IMETHODIMP nsDocShell::GetCopyable(PRBool *aCopyable)
{
   NS_ENSURE_ARG_POINTER(aCopyable);
   NS_ENSURE_STATE(mContentViewer);

   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMSelection> selection;
   NS_ENSURE_SUCCESS(presShell->GetSelection(SELECTION_NORMAL, 
      getter_AddRefs(selection)), NS_ERROR_FAILURE);

   if(!selection)
      {
      *aCopyable = PR_FALSE;
      return NS_OK;
      }

   PRBool isCollapsed;
   NS_ENSURE_SUCCESS(selection->GetIsCollapsed(&isCollapsed), NS_ERROR_FAILURE);
   *aCopyable = !isCollapsed;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::CutSelection()
{
   NS_ENSURE_STATE(mContentViewer);
   PRBool cutable;
   NS_ENSURE_SUCCESS(GetCutable(&cutable), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(cutable, NS_ERROR_UNEXPECTED);



   //XXX Implement
   //Should check to find the current focused object.  Then cut the contents.
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetCutable(PRBool* aCutable)
{
   NS_ENSURE_ARG_POINTER(aCutable);
   //XXX Implement
   //Should check to find the current focused object.  Then see if it can
   //be cut out of.  For now the answer is always no since CutSelection()
   // has not been implemented.
   *aCutable = PR_FALSE;
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::Paste()
{
   NS_ENSURE_STATE(mContentViewer);
   PRBool pasteable;
   NS_ENSURE_SUCCESS(GetPasteable(&pasteable), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(pasteable, NS_ERROR_UNEXPECTED);
   
   //XXX Implement
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetPasteable(PRBool* aPasteable)
{
   NS_ENSURE_ARG_POINTER(aPasteable);

   //XXX Implement
   //Should check to find the current focused object.  Then see if it can
   //be pasted into.  For now the answer is always no since Paste()
   // has not been implemented.
   *aPasteable = PR_FALSE;
   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellFile
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::Save()
{
   NS_ENSURE_STATE(mContentViewer);
   PRBool saveable;
   NS_ENSURE_SUCCESS(GetSaveable(&saveable), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(saveable, NS_ERROR_UNEXPECTED);

   //XXX First Check
   return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP nsDocShell::GetSaveable(PRBool* saveable)
{
   NS_ENSURE_ARG_POINTER(saveable);
   //XXX First Check
   // XXX Implement
   // Should check if a doc is loaded and if it is in a state that that is
   // ready to save.  For now the answer is always no since saving isn't
   // implemented.

   *saveable = PR_FALSE;
   return NS_OK;
} 

NS_IMETHODIMP nsDocShell::Print()
{
   NS_ENSURE_STATE(mContentViewer);
   PRBool printable;
   NS_ENSURE_SUCCESS(GetPrintable(&printable), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(printable, NS_ERROR_UNEXPECTED);

   //XXX First Check
   return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP nsDocShell::GetPrintable(PRBool* printable)
{
   NS_ENSURE_ARG_POINTER(printable);
   // XXX Implement
   // Should check if a doc is loaded and if it is in a state that that is
   // ready to print.  For now the answer is always no since printing isn't
   // implemented.

   *printable = PR_FALSE;
   return NS_OK;
} 



//*****************************************************************************
// nsDocShell::nsIDocShellContainer
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetChildCount(PRInt32 *aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);
  *aChildCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::AddChild(nsIDocShell *aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);

  NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);
  mChildren.AppendElement(aChild);
  NS_ADDREF(aChild);

  //XXX HTML Specifics need to be moved out.
  nsCOMPtr<nsIHTMLDocShell> childAsHTMLDocShell = do_QueryInterface(aChild);
  if (childAsHTMLDocShell)
  {
    PRUnichar *defaultCharset=nsnull;
    NS_ENSURE_SUCCESS(GetDefaultCharacterSet(&defaultCharset), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(childAsHTMLDocShell->SetDefaultCharacterSet(defaultCharset), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(childAsHTMLDocShell->SetForceCharacterSet(mForceCharacterSet.GetUnicode()), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

// XXX: tiny semantic change from webshell.  aChild is only effected if it was actually a child of this docshell
NS_IMETHODIMP nsDocShell::RemoveChild(nsIDocShell *aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);

  PRBool childRemoved = mChildren.RemoveElement(aChild);
  if (PR_TRUE==childRemoved)
  {
    aChild->SetParent(nsnull);
    NS_RELEASE(aChild);
  }
  return NS_OK;
}

/* readonly attribute nsIEnumerator childEnumerator; */
NS_IMETHODIMP nsDocShell::GetChildEnumerator(nsIEnumerator * *aChildEnumerator)
{
  NS_ENSURE_ARG_POINTER(aChildEnumerator);

  return NS_OK;
}

/* depth-first search for a child shell with aName */
NS_IMETHODIMP nsDocShell::FindChildWithName(const PRUnichar *aName, nsIDocShell **_retval)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ENSURE_ARG_POINTER(_retval);
  
  *_retval = nsnull;  // if we don't find one, we return NS_OK and a null result 
  nsAutoString name(aName);
  PRUnichar *childName;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) 
  {
    nsIDocShell* child = (nsIDocShell*) mChildren.ElementAt(i); // doesn't addref the result
    if (nsnull != child) {
      child->GetName(&childName);
      if (name.Equals(childName)) {
        *_retval = child;
        NS_ADDREF(child);
        break;
      }

      // See if child contains the shell with the given name
      nsCOMPtr<nsIDocShellContainer> childAsContainer = do_QueryInterface(child);
      if (child)
      {
        NS_ENSURE_SUCCESS(childAsContainer->FindChildWithName(name.GetUnicode(), _retval), NS_ERROR_FAILURE);
      }
      if (_retval) {  // found it
        break;
      }
    }
  }
  return NS_OK;
}


//*****************************************************************************
// nsDocShell::nsIGenericWindow
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::InitWindow(nativeWindow parentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   NS_ENSURE_ARG(parentWidget);  // DocShells must get a widget for a parent
   NS_ENSURE_STATE(!mCreated && mBaseInitInfo);

   mParentWidget = parentWidget;
   mBaseInitInfo->x = x;
   mBaseInitInfo->y = y;
   mBaseInitInfo->cx = cx;
   mBaseInitInfo->cy = cy;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Create()
{
   NS_ENSURE_STATE(!mCreated);

   // Use mBaseInitInfo to do create
   // Then delete mBaseInitInfo
   //XXX First Check
	/*
	Tells the window that intialization and setup is complete.  When this is
	called the window can actually create itself based on the setup
	information handed to it.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::Destroy()
{
   // We don't support the dynamic destroy and recreate on the object.  Just
   // create a new object!
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShell::SetPosition(PRInt32 x, PRInt32 y)
{
   if(!mCreated)
      {
      mBaseInitInfo->x = x;
      mBaseInitInfo->y = y;
      }
   else
      {
      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

        PRInt32 w, h;
        nsresult rv = GetSize(w, h);
        if (NS_FAILED(rv)) { return rv; }

        PRInt32 borderWidth  = 0;
        PRInt32 borderHeight = 0;
        if (mWindow) 
        {
          mWindow->GetBorderSize(borderWidth, borderHeight);
          // Don't have the widget repaint. Layout will generate repaint requests
          // during reflow
          mWindow->Resize(aX, aY, w, h, PR_FALSE);
        }

        mBounds.SetRect(aX,aY,w,h);   // set the webshells bounds 

        return rv;
      */
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   if(!mCreated)
      {
      *x = mBaseInitInfo->x;
      *y = mBaseInitInfo->y;
      }
   else
      {
      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

        nsRect result;
        if (nsnull != mWindow) {
          mWindow->GetClientBounds(result);
        } else {
          result = mBounds;
        }

        *aX = result.x;
        *aY = result.y;

        return NS_OK;
      */
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   if(!mCreated)
      {
      mBaseInitInfo->cx = cx;
      mBaseInitInfo->cy = cy;
      }
   else
      {
      /*  XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.


        PRInt32 x, y;
     nsresult rv = GetPosition(x, y);
     if (NS_FAILED(rv)) { return rv; }

     PRInt32 borderWidth  = 0;
     PRInt32 borderHeight = 0;
     if (mWindow) 
     {
       mWindow->GetBorderSize(borderWidth, borderHeight);
       // Don't have the widget repaint. Layout will generate repaint requests
       // during reflow
       mWindow->Resize(x, y, aCX, aCY, PR_FALSE);
     }

     mBounds.SetRect(x, y, aCX, aCY);   // set the webshells bounds --dwc0001

     // Set the size of the content area, which is the size of the window
     // minus the borders
     if (nsnull != mContentViewer) {
       nsRect rr(0, 0, aCX-(borderWidth*2), aCY-(borderHeight*2));
       mContentViewer->SetBounds(rr);
     }
     return rv;
      */
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   if(!mCreated)
      {
      *cx = mBaseInitInfo->cx;
      *cy = mBaseInitInfo->cy;
      }
   else
      {
      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

        nsRect result;
     if (nsnull != mWindow) {
       mWindow->GetClientBounds(result);
     } else {
       result = mBounds;
     }

     *aCX = result.width;
     *aCY = result.height;

     return NS_OK;
      */
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   if(!mCreated)
      {
      mBaseInitInfo->x = x;
      mBaseInitInfo->y = y;
      mBaseInitInfo->cx = cx;
      mBaseInitInfo->cy = cy;
      }
   else
      {
      // XXX Do normal size and position stuff.  Could just call 
      // Size and then Position, but underlying control probably supports 
      // some optimized setting of both like this.

      /* XXX Implement below is code from old webShell
      We don't have a heavy-weight window, we want to talk to our view I think.
      We also don't want to duplicate the bounds locally.  No need, let the
      view keep up with that.

        PRInt32 borderWidth  = 0;
     PRInt32 borderHeight = 0;
     if (mWindow) 
     {
       mWindow->GetBorderSize(borderWidth, borderHeight);
       // Don't have the widget repaint. Layout will generate repaint requests
       // during reflow
       mWindow->Resize(aX, aY, aCX, aCY, PR_FALSE);
     }

     mBounds.SetRect(aX, aY, aCX, aCY);   // set the webshells bounds --dwc0001

     // Set the size of the content area, which is the size of the window
     // minus the borders
     if (nsnull != mContentViewer) {
       nsRect rr(0, 0, aCX-(borderWidth*2), aCY-(borderHeight*2));
       mContentViewer->SetBounds(rr);
     }
     return rv;
      */
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Repaint(PRBool fForce)
{
   //XXX First Check
	/** 
	 * Tell the window to repaint itself
	 * @param aForce - if true, repaint immediately
	 *                 if false, the window may defer repainting as it sees fit.
	 */

   /* XXX Implement Tell our view to repaint

     if (mWindow) {
    mWindow->Invalidate(aForce);
  }

	nsresult rv;
	nsCOMPtr<nsIViewManager> viewManager;
	rv = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!viewManager) { return NS_ERROR_NULL_POINTER; }

  //XXX: what about aForce?
	rv = viewManager->UpdateAllViews(0);
	return rv;

   */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetParentWidget(nsIWidget** parentWidget)
{
   NS_ENSURE_ARG_POINTER(parentWidget);

   *parentWidget = mParentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentWidget(nsIWidget* parentWidget)
{
   NS_ENSURE_STATE(!mCreated);

   mParentWidget = parentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);

   if(mParentWidget)
      *parentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
   else
      *parentNativeWindow = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShell::GetVisibility(PRBool* aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

  if(!mCreated) {
    *aVisibility = mBaseInitInfo->visible;
  }
  else
  {
    // get the pres shell
    nsCOMPtr<nsIPresShell> presShell;
    NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // get the view manager
    nsCOMPtr<nsIViewManager> vm;
    NS_ENSURE_SUCCESS(presShell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    // get the root view
    nsIView *rootView=nsnull; // views are not ref counted
    NS_ENSURE_SUCCESS(vm->GetRootView(rootView), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

    // convert the view's visibility attribute to a bool
    nsViewVisibility vis;
    NS_ENSURE_TRUE(rootView->GetVisibility(vis), NS_ERROR_FAILURE); 
    if (nsViewVisibility_kHide==vis) {
      *aVisibility = PR_FALSE;
    }
    else {
      *aVisibility = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetVisibility(PRBool visibility)
{
   if(!mCreated)
      mBaseInitInfo->visible = visibility;
   else
      {
      // XXX Set underlying control visibility
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetMainWidget(nsIWidget** mainWidget)
{
   NS_ENSURE_ARG_POINTER(mainWidget);

   // For now we don't create our own widget, so simply return the parent one. 
   *mainWidget = mParentWidget;
   NS_IF_ADDREF(*mainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetFocus()
{
   //XXX First Check
	/**
	* Give the window focus.
	*/

   /* XXX implement

     if (mWindow) {
    mWindow->SetFocus();
  }

  return NS_OK;

   */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetTitle(PRUnichar** title)
{
   NS_ENSURE_ARG_POINTER(title);

   //XXX First Check
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetTitle(const PRUnichar* title)
{
   //XXX First Check
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsDocShell::nsIScrollable
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32* curPos)
{
   NS_ENSURE_ARG_POINTER(curPos);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   nscoord x, y;
   NS_ENSURE_SUCCESS(scrollView->GetScrollPosition(x, y), NS_ERROR_FAILURE);

   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         *curPos = x;
         return NS_OK;

      case ScrollOrientation_Y:
         *curPos = y;
         return NS_OK;

      default:
         NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32 curPos)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   PRInt32 other;
   PRInt32 x;
   PRInt32 y;

   GetCurScrollPos(scrollOrientation, &other);

   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         x = curPos;
         y = other;
         break;

      case ScrollOrientation_Y:
         x = other;
         y = curPos;
         break;

      default:
         NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }

   NS_ENSURE_SUCCESS(scrollView->ScrollTo(x, y, NS_VMREFRESH_IMMEDIATE),
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetCurScrollPosEx(PRInt32 curHorizontalPos, 
   PRInt32 curVerticalPos)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(scrollView->ScrollTo(curHorizontalPos, curVerticalPos, 
      NS_VMREFRESH_IMMEDIATE), NS_ERROR_FAILURE);
   return NS_OK;
}

// XXX This is wrong
NS_IMETHODIMP nsDocShell::GetScrollRange(PRInt32 scrollOrientation,
   PRInt32* minPos, PRInt32* maxPos)
{
   NS_ENSURE_ARG_POINTER(minPos && maxPos);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   PRInt32 cx;
   PRInt32 cy;
   
   NS_ENSURE_SUCCESS(scrollView->GetContainerSize(&cx, &cy), NS_ERROR_FAILURE);
   *minPos = 0;
   
   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         *maxPos = cx;
         return NS_OK;

      case ScrollOrientation_Y:
         *maxPos = cy;
         return NS_OK;

      default:
         NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetScrollRange(PRInt32 scrollOrientation,
   PRInt32 minPos, PRInt32 maxPos)
{
   //XXX First Check
	/*
	Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
	something less than the current thumb position, curPos is set = to maxPos.

	@return	NS_OK - Setting or Getting completed successfully.
				NS_ERROR_INVALID_ARG - returned when curPos is not within the
					minPos and maxPos.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetScrollRangeEx(PRInt32 minHorizontalPos,
   PRInt32 maxHorizontalPos, PRInt32 minVerticalPos, PRInt32 maxVerticalPos)
{
   //XXX First Check
	/*
	Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
	something less than the current thumb position, curPos is set = to maxPos.

	@return	NS_OK - Setting or Getting completed successfully.
				NS_ERROR_INVALID_ARG - returned when curPos is not within the
					minPos and maxPos.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   // XXX This is all evil, we need to convert.  We don't know our prefs
   // are the same as this interfaces.
 /*  nsScrollPreference scrollPref;

   NS_ENSURE_SUCCESS(scrollView->GetScrollPreference(scrollPref), 
      NS_ERROR_FAILURE);

   *scrollbarPref = scrollPref; */

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   // XXX This is evil.  We should do a mapping, we don't know our prefs
   // are the same as this interface.  In fact it doesn't compile
  /* nsScrollPreference scrollPref = scrollbarPref;
   NS_ENSURE_SUCCESS(scrollView->SetScrollPreference(scrollPref), 
      NS_ERROR_FAILURE);  */

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetScrollbarVisibility(PRBool* verticalVisible,
   PRBool* horizontalVisible)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   PRBool vertVisible;
   PRBool horizVisible;

   NS_ENSURE_SUCCESS(scrollView->GetScrollbarVisibility(&vertVisible,
      &horizVisible), NS_ERROR_FAILURE);

   if(verticalVisible)
      *verticalVisible = vertVisible;
   if(horizontalVisible)
      *horizontalVisible = horizVisible;

   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::ScrollByLines(PRInt32 numLines)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(scrollView->ScrollByLines(numLines), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ScrollByPages(PRInt32 numPages)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(scrollView->ScrollByPages(numPages), NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIHTMLDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::ScrollToNode(nsIDOMNode* aNode)
{
   NS_ENSURE_ARG(aNode);
   NS_ENSURE_STATE(mContentViewer);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

   // Get the nsIContent interface, because that's what we need to 
   // get the primary frame
   
   nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
   NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

   // Get the primary frame
   nsIFrame* frame;  // Remember Frames aren't ref-counted.  They are in their 
                     // own special little world.

   NS_ENSURE_SUCCESS(GetPrimaryFrameFor(content, &frame),
      NS_ERROR_FAILURE);

   // tell the pres shell to scroll to the frame
   NS_ENSURE_SUCCESS(presShell->ScrollFrameIntoView(frame, 
      NS_PRESSHELL_SCROLL_TOP, NS_PRESSHELL_SCROLL_ANYWHERE), NS_ERROR_FAILURE); 
   return NS_OK; 
}

NS_IMETHODIMP nsDocShell::GetAllowPlugins(PRBool* aAllowPlugins)
{
   NS_ENSURE_ARG_POINTER(aAllowPlugins);

   *aAllowPlugins = mAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowPlugins(PRBool aAllowPlugins)
{
   mAllowPlugins = aAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetMarginWidth(PRInt32* aMarginWidth)
{
   NS_ENSURE_ARG_POINTER(aMarginWidth);

   *aMarginWidth = mMarginWidth;
   return NS_OK;  
}

NS_IMETHODIMP nsDocShell::SetMarginWidth(PRInt32 aMarginWidth)
{
   mMarginWidth = aMarginWidth;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetMarginHeight(PRInt32* aMarginHeight)
{
   NS_ENSURE_ARG_POINTER(aMarginHeight);

   *aMarginHeight = mMarginHeight;
   return NS_OK; 
}

NS_IMETHODIMP nsDocShell::SetMarginHeight(PRInt32 aMarginHeight)
{
   mMarginHeight = aMarginHeight;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetIsFrame(PRBool* aIsFrame)
{
  NS_ENSURE_ARG_POINTER(aIsFrame);

  *aIsFrame = mIsFrame;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetIsFrame(PRBool aIsFrame)
{
  mIsFrame = aIsFrame;
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aDefaultCharacterSet)
NS_IMETHODIMP nsDocShell::GetDefaultCharacterSet(PRUnichar** aDefaultCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aDefaultCharacterSet);

  static char *gDefCharset = nsnull;    // XXX: memory leak!

  if (0 == mDefaultCharacterSet.Length()) 
  {
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset)) 
    {
      if(mPrefs)
        mPrefs->CopyCharPref("intl.charset.default", &gDefCharset);
    }
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset))
      mDefaultCharacterSet = "ISO-8859-1";
    else
      mDefaultCharacterSet = gDefCharset;
  }
  *aDefaultCharacterSet = mDefaultCharacterSet.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetDefaultCharacterSet(const PRUnichar* aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;  // this does a copy of aDefaultCharacterSet
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) 
  {
    nsIDocShell* child = (nsIDocShell*) mChildren.ElementAt(i);
    NS_WARN_IF_FALSE(child, "null child in docshell");
    if (child) 
    {
      nsCOMPtr<nsIHTMLDocShell> childAsHTMLDocShell = do_QueryInterface(child);
      if (childAsHTMLDocShell) {
        childAsHTMLDocShell->SetDefaultCharacterSet(aDefaultCharacterSet);
      }
    }
  }
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aForceCharacterSet)
NS_IMETHODIMP nsDocShell::GetForceCharacterSet(PRUnichar** aForceCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aForceCharacterSet);

  nsAutoString emptyStr;
  if (mForceCharacterSet.Equals(emptyStr)) {
    *aForceCharacterSet = nsnull;
  }
  else {
    *aForceCharacterSet = mForceCharacterSet.ToNewUnicode();
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetForceCharacterSet(const PRUnichar* aForceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShell* child = (nsIDocShell*) mChildren.ElementAt(i);
    NS_WARN_IF_FALSE(child, "null child in docshell");
    if (child) 
    {
      nsCOMPtr<nsIHTMLDocShell> childAsHTMLDocShell = do_QueryInterface(child);
      if (childAsHTMLDocShell) {
        childAsHTMLDocShell->SetForceCharacterSet(aForceCharacterSet);
      }
    }
  }
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aHintCharacterSet)
NS_IMETHODIMP nsDocShell::GetHintCharacterSet(PRUnichar * *aHintCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSet);

  if(kCharsetUninitialized == mHintCharsetSource) {
    *aHintCharacterSet = nsnull;
  } else {
    *aHintCharacterSet = mHintCharset.ToNewUnicode();
     mHintCharsetSource = kCharsetUninitialized;
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetHintCharacterSetSource(PRInt32 *aHintCharacterSetSource)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSetSource);

  *aHintCharacterSetSource = mHintCharsetSource;
  return NS_OK;
}

// XXX: poor error checking
NS_IMETHODIMP nsDocShell::SizeToContent()
{

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsRect  shellArea;
  PRInt32 width, height;
  float   pixelScale;
  NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE), 
                    NS_ERROR_FAILURE);

  // so how big is it?
  nsCOMPtr<nsIPresContext> presContext;
  NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);
  presContext->GetVisibleArea(shellArea);
  presContext->GetTwipsToPixels(&pixelScale);
  width = PRInt32((float)shellArea.width*pixelScale);
  height = PRInt32((float)shellArea.height*pixelScale);

  // if we're the outermost webshell for this window, size the window
  /* XXX: how do we do this now?
  if (mContainer) 
  {
    nsCOMPtr<nsIBrowserWindow> browser = do_QueryInterface(mContainer);
    if (browser) 
    {
      nsCOMPtr<nsIDocShell> browserWebShell;
      PRInt32 oldX, oldY, oldWidth, oldHeight,
              widthDelta, heightDelta;
      nsRect  windowBounds;

      GetBounds(oldX, oldY, oldWidth, oldHeight);
      widthDelta = width - oldWidth;
      heightDelta = height - oldHeight;
      browser->GetWindowBounds(windowBounds);
      browser->SizeWindowTo(windowBounds.width + widthDelta,
                            windowBounds.height + heightDelta);
    }
  }
  */
  NS_ASSERTION(PR_FALSE, "NOT YET IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
  //return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIContentViewerContainer
//*****************************************************************************   
NS_IMETHODIMP nsDocShell::QueryCapability(const nsIID &aIID, void** aResult)
{
  NS_ENSURE_SUCCESS(PR_FALSE, NS_ERROR_NOT_IMPLEMENTED);
  return NS_OK;
};

NS_IMETHODIMP nsDocShell::Embed(nsIContentViewer* aContentViewer, 
                                    const char      * aCommand,
                                    nsISupports     * aExtraInfo)
{
  NS_ENSURE_ARG_POINTER(aContentViewer);
  // null aCommand is ok
  // null aExtraInfo is ok

  WEB_TRACE(WEB_TRACE_CALLS,
      ("nsWebShell::Embed: this=%p aDocViewer=%p aCommand=%s aExtraInfo=%p",
       this, aContentViewer, aCommand ? aCommand : "", aExtraInfo));

  nsRect bounds;
  // (1) reset state, clean up any left-over data from previous embedding
  mContentViewer = nsnull;
  if (nsnull != mScriptContext) {
    mScriptContext->GC();
  }
  
  // (2) set the new content viewer
  mContentViewer = aContentViewer;

  // XXX: comment from webshell code --
  // check to see if we have a window to embed into --dwc0001
  /* Note we also need to check for the presence of a native widget. If the
     webshell is hidden before it's embedded, which can happen in an onload
     handler, the native widget is destroyed before this code is run.  This
     appears to be mostly harmless except on Windows, where the subsequent
     attempt to create a child window without a parent is met with disdain
     by the OS. It's handy, then, that GetNativeData on Windows returns
     null in this case. */
  /* XXX native window
  if(mWindow && mWindow->GetNativeData(NS_NATIVE_WIDGET)) 
  {
    mWindow->GetClientBounds(bounds);
    bounds.x = bounds.y = 0;
    rv = mContentViewer->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET),
                              mDeviceContext,
                              mPrefs,
                              bounds,
                              mScrollPref);

    // If the history state has been set by session history,
    // set it on the pres shell now that we have a content
    // viewer.

    //XXX: history, should be removed
    if (mContentViewer && mHistoryState) {
      nsCOMPtr<nsIDocumentViewer> docv = do_QueryInterface(mContentViewer);
      if (nsnull != docv) {
        nsCOMPtr<nsIPresShell> shell;
        rv = docv->GetPresShell(*getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv)) {
          rv = shell->SetHistoryState((nsILayoutHistoryState*) mHistoryState);
        }
      }
    }

    if (NS_SUCCEEDED(rv)) {
      mContentViewer->Show();
    }
  } else {
    mContentViewer = nsnull;
  }
  */

  // Now that we have switched documents, forget all of our children
  DestroyChildren();

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetContentViewer(nsIContentViewer** aContentViewer)
{
  NS_ENSURE_ARG_POINTER(aContentViewer);

  *aContentViewer = mContentViewer;
  NS_IF_ADDREF(*aContentViewer);
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::HandleUnknownContentType(nsIDocumentLoader* aLoader,
                                      nsIChannel* channel,
                                      const char *aContentType,
                                      const char *aCommand)
{
  NS_ENSURE_SUCCESS(PR_FALSE, NS_ERROR_NOT_IMPLEMENTED);
  return NS_OK;
}
  
//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************   

nsresult nsDocShell::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode* aParent,
   PRInt32* aOffset)
{
   NS_ENSURE_ARG_POINTER(aChild || aParent);
   
   nsCOMPtr<nsIDOMNodeList> childNodes;
   NS_ENSURE_SUCCESS(aParent->GetChildNodes(getter_AddRefs(childNodes)),
      NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(childNodes, NS_ERROR_FAILURE);

   PRInt32 i=0;

   for( ; PR_TRUE; i++)
      {
      nsCOMPtr<nsIDOMNode> childNode;
      NS_ENSURE_SUCCESS(childNodes->Item(i, getter_AddRefs(childNode)), 
         NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(childNode, NS_ERROR_FAILURE);

      if(childNode.get() == aChild)
         {
         *aOffset = i;
         return NS_OK;
         }
      }

   return NS_ERROR_FAILURE;
}

nsresult nsDocShell::GetRootScrollableView(nsIScrollableView** aOutScrollView)
{
   NS_ENSURE_ARG_POINTER(aOutScrollView);
   
   nsCOMPtr<nsIPresShell> shell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(shell)), NS_ERROR_FAILURE);

   nsCOMPtr<nsIViewManager> viewManager;
   NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(viewManager)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(viewManager->GetRootScrollableView(aOutScrollView),
      NS_ERROR_FAILURE);

   return NS_OK;
} 

nsresult nsDocShell::GetPresShell(nsIPresShell** aPresShell)
{
   NS_ENSURE_ARG_POINTER(aPresShell);
   
   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presContext->GetShell(aPresShell), NS_ERROR_FAILURE);

   return NS_OK;
}

nsresult nsDocShell::EnsureContentListener()
{
   if(mContentListener)
      return NS_OK;
   
   mContentListener = new nsDSURIContentListener();
   NS_ENSURE_TRUE(mContentListener, NS_ERROR_OUT_OF_MEMORY);

   mContentListener->AddRef();
   mContentListener->DocShell(this);

   return NS_OK;
}

void nsDocShell::SetCurrentURI(nsIURI* aUri)
{
   mCurrentURI = aUri; //This assignment addrefs
}

nsresult nsDocShell::CreateContentViewer(const char* aContentType, 
   const char* aCommand, nsIChannel* aOpenedChannel, 
   nsIStreamListener** aContentHandler)
{
   NS_ENSURE_STATE(mCreated);

   //XXXQ Can we check the content type of the current content viewer
   // and reuse it without destroying it and re-creating it?

   // XXXIMPL Do cleanup....

   // Instantiate the content viewer object
   NS_ENSURE_SUCCESS(NewContentViewerObj(aContentType, aCommand, aOpenedChannel,
      aContentHandler), NS_ERROR_FAILURE);

   //XXXIMPL Do stuff found in embed here.  Don't call embed it is going away.

   return NS_ERROR_FAILURE;
}

nsresult nsDocShell::NewContentViewerObj(const char* aContentType,
   const char* aCommand, nsIChannel* aOpenedChannel, 
   nsIStreamListener** aContentHandler)
{
   //XXX This should probably be some category thing....
   char id[256];
   PR_snprintf(id, sizeof(id), NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "%s/%s",
      aCommand , aContentType);

   // Create an instance of the document-loader-factory
   nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory(do_CreateInstance(id));
   NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_FAILURE);

   nsCOMPtr<nsILoadGroup> loadGroup(do_QueryInterface(mLoadCookie));
   // Now create an instance of the content viewer
   NS_ENSURE_SUCCESS(docLoaderFactory->CreateInstance(aCommand, aOpenedChannel,
      loadGroup, aContentType, this /* this should become nsIDocShell*/,
      nsnull /*XXXQ Need ExtraInfo???*/,
      aContentHandler, getter_AddRefs(mContentViewer)), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FireStartDocumentLoad(nsIDocumentLoader* aLoader,
                                      nsIURI           * aURL,  //XXX: should be the channel?
                                      const char       * aCommand)
{
  NS_ENSURE_ARG_POINTER(aLoader);
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_ARG_POINTER(aCommand);

  nsCOMPtr<nsIDocumentViewer> docViewer;
  if (mScriptGlobal && (aLoader == mDocLoader)) 
  {
    docViewer = do_QueryInterface(mContentViewer);
    if (docViewer)
    {
      nsCOMPtr<nsIPresContext> presContext;
      NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
      if (presContext)
      {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_PAGE_UNLOAD;
        NS_ENSURE_SUCCESS(mScriptGlobal->HandleDOMEvent(*presContext, 
                                                        &event, 
                                                        nsnull, 
                                                        NS_EVENT_FLAG_INIT, 
                                                        status),
                          NS_ERROR_FAILURE);
      }
    }
  }

  if (aLoader == mDocLoader) 
  {
    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

    if (!mDocLoaderObserver && mParent) 
    {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIDocShell> root;
      NS_ENSURE_SUCCESS(GetRootDocShell(getter_AddRefs(root)), NS_ERROR_FAILURE);

      if (root)
        NS_ENSURE_SUCCESS(root->GetDocLoaderObserver(getter_AddRefs(dlObserver)), NS_ERROR_FAILURE);
    }
    else
    {
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }
    /*
     * Fire the OnStartDocumentLoad of the webshell observer
     */
    /* XXX This code means "notify dlObserver only if we're the top level webshell.
           I don't know why that would be, can't subdocument have doc loader observers?
     */
    if (/*(nsnull != mContainer) && */(nsnull != dlObserver))
    {
       NS_ENSURE_SUCCESS(dlObserver->OnStartDocumentLoad(mDocLoader, aURL, aCommand), 
                         NS_ERROR_FAILURE);
    }
  }

  return NS_OK;
}



NS_IMETHODIMP
nsDocShell::FireEndDocumentLoad(nsIDocumentLoader* aLoader,
                                    nsIChannel       * aChannel,
                                    nsresult           aStatus,
                                    nsIDocumentLoaderObserver * aDocLoadObserver)
{
#ifdef MOZ_PERF_METRICS
  RAPTOR_STOPWATCH_DEBUGTRACE(("Stop: nsWebShell::OnEndDocumentLoad(), this=%p\n", this));
  NS_STOP_STOPWATCH(mTotalTime)
  RAPTOR_STOPWATCH_TRACE(("Total (Layout + Page Load) Time (webshell=%p): ", this));
  mTotalTime.Print();
  RAPTOR_STOPWATCH_TRACE(("\n"));
#endif

  NS_ENSURE_ARG_POINTER(aLoader);
  NS_ENSURE_ARG_POINTER(aChannel);
  // null aDocLoadObserver is legal

  nsCOMPtr<nsIURI> aURL;
  NS_ENSURE_SUCCESS(aChannel->GetURI(getter_AddRefs(aURL)), NS_ERROR_FAILURE);

  if (aLoader == mDocLoader)
  {
    if (mScriptGlobal && mContentViewer)
    {
      nsCOMPtr<nsIDocumentViewer> docViewer;
      docViewer = do_QueryInterface(mContentViewer);
      if (docViewer)
      {
        nsCOMPtr<nsIPresContext> presContext;
        NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
        if (presContext)
        {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_PAGE_LOAD;
          NS_ENSURE_SUCCESS(mScriptGlobal->HandleDOMEvent(*presContext, 
                                                          &event, 
                                                          nsnull, 
                                                          NS_EVENT_FLAG_INIT, 
                                                          status),
                            NS_ERROR_FAILURE);
        }
      }
    }

    // Fire the EndLoadURL of the web shell container
    /* XXX: what replaces mContainer?
    if (nsnull != aURL) 
    {
      nsAutoString urlString;
      char* spec;
      rv = aURL->GetSpec(&spec);
      if (NS_SUCCEEDED(rv)) 
      {
        urlString = spec;
        if (nsnull != mContainer) {
          rv = mContainer->EndLoadURL(this, urlString.GetUnicode(), 0);
        }
        nsCRT::free(spec);
      }
    }
    */

    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;
    if (!mDocLoaderObserver && mParent) 
    {
      // If this is a frame (in which case it would have a parent && doesn't
      // have a documentloaderObserver, get it from the rootWebShell
      nsCOMPtr<nsIDocShell> root;
      NS_ENSURE_SUCCESS(GetRootDocShell(getter_AddRefs(root)), NS_ERROR_FAILURE);

      if (root)
        NS_ENSURE_SUCCESS(root->GetDocLoaderObserver(getter_AddRefs(dlObserver)), NS_ERROR_FAILURE);
    }
    else
    {
      /* Take care of the Trailing slash situation */
/* XXX: session history stuff, should be taken care of external to the docshell
      if (mSHist)
        CheckForTrailingSlash(aURL);
*/
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }

    /*
     * Fire the OnEndDocumentLoad of the DocLoaderobserver
     */
    if (dlObserver && aURL) {
       NS_ENSURE_SUCCESS(dlObserver->OnEndDocumentLoad(mDocLoader, aChannel, aStatus, aDocLoadObserver), 
                         NS_ERROR_FAILURE);
    }

    /* put the new document in the doc tree */
    NS_ENSURE_SUCCESS(InsertDocumentInDocTree(), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::InsertDocumentInDocTree()
{
  nsCOMPtr<nsIDocShell> parent;
  NS_ENSURE_SUCCESS(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
  // null parent is legal.  If we have a parent, hook up our doc to the parent's doc
  if (parent)
  {
    // Get the document object for the parent
    nsCOMPtr<nsIContentViewerContainer> parentAsContentViewerContainer;
    parentAsContentViewerContainer = do_QueryInterface(parent);
    NS_ENSURE_TRUE(parentAsContentViewerContainer, NS_ERROR_FAILURE);
    nsCOMPtr<nsIContentViewer> parentContentViewer;
    NS_ENSURE_SUCCESS(parentAsContentViewerContainer->GetContentViewer(getter_AddRefs(parentContentViewer)), 
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(parentContentViewer, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDocumentViewer> parentDocViewer;
    parentDocViewer = do_QueryInterface(parentContentViewer);
    NS_ENSURE_TRUE(parentDocViewer, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocument> parentDoc;
    NS_ENSURE_SUCCESS(parentDocViewer->GetDocument(*getter_AddRefs(parentDoc)), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(parentDoc, NS_ERROR_FAILURE);

    // Get the document object for this
    nsCOMPtr<nsIDocumentViewer> docViewer;
    docViewer = do_QueryInterface(mContentViewer);
    NS_ENSURE_TRUE(docViewer, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocument> doc;
    NS_ENSURE_SUCCESS(docViewer->GetDocument(*getter_AddRefs(doc)), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    doc->SetParentDocument(parentDoc);
    parentDoc->AddSubDocument(doc);
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::DestroyChildren()
{
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) 
  {
    nsIDocShell* shell = (nsIDocShell*) mChildren.ElementAt(i);
    NS_WARN_IF_FALSE(shell, "docshell has null child");
    if (shell)
    {
      shell->SetParent(nsnull);
      // XXX: will shells have a separate Destroy?  See webshell::Destroy for what it does
      //shell->Destroy();
      NS_RELEASE(shell);
    }
  }
  mChildren.Clear();
  return NS_OK;
}

nsresult nsDocShell::GetPrimaryFrameFor(nsIContent* content, nsIFrame** frame)
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}


