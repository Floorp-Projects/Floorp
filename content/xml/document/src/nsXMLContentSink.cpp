/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsXMLContentSink.h"
#include "nsIElementFactory.h"
#include "nsIParser.h"
#include "nsIUnicharInputStream.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMNSDocument.h"
#include "nsIXMLDocument.h"
#include "nsIXMLContent.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsIRefreshURI.h"
#include "nsNetUtil.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "nsDOMDocumentType.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsHTMLAtoms.h"
#include "nsContentUtils.h"
#include "nsLayoutAtoms.h"
#include "nsContentCID.h"
#include "nsIScriptContext.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContentViewer.h"
#include "jsapi.h" // for JSVERSION_* and JS_VersionToString
#include "prtime.h"
#include "prlog.h"
#include "prmem.h"
#include "nsXSLContentSink.h"
#include "nsParserCIID.h"
#include "nsParserUtils.h"
#include "nsIScrollable.h"
#include "nsRect.h"
#include "nsGenericElement.h"
#include "nsIWebNavigation.h"
#include "nsIScriptElement.h"
#include "nsStyleLinkElement.h"
#include "nsEscape.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIPrincipal.h"
#include "nsIAggregatePrincipal.h"
#include "nsICodebasePrincipal.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsXBLAtoms.h"
#include "nsIPref.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIBindingManager.h"
#include "nsIObserver.h"
#include "nsIDocumentTransformer.h"
#include "nsISyncLoadDOMService.h"

// XXX misnamed header file, but oh well
#include "nsHTMLTokens.h"

static char kNameSpaceSeparator = ':';
#define kXSLType "text/xsl"

static NS_DEFINE_CID(kNameSpaceManagerCID, NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

nsINameSpaceManager* nsXMLContentSink::gNameSpaceManager = nsnull;
PRUint32 nsXMLContentSink::gRefCnt = 0;

// XXX Open Issues:
// 1) what's not allowed - We need to figure out which HTML tags
//    (prefixed with a HTML namespace qualifier) are explicitly not
//    allowed (if any).
// 2) factoring code with nsHTMLContentSink - There's some amount of
//    common code between this and the HTML content sink. This will
//    increase as we support more and more HTML elements. How can code
//    from the code be factored?

nsresult
NS_NewXMLContentSink(nsIXMLContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURI* aURL,
                     nsIWebShell* aWebShell,
                     nsIChannel* aChannel)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXMLContentSink* it;
  NS_NEWXPCOM(it, nsXMLContentSink);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDoc, aURL, aWebShell, aChannel);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(NS_GET_IID(nsIXMLContentSink), (void **)aResult);
}

nsXMLContentSink::nsXMLContentSink()
{
  NS_INIT_ISUPPORTS();
  gRefCnt++;
  if (gRefCnt == 1) {
#ifdef DEBUG
    nsresult rv =
#endif
    nsServiceManager::GetService(kNameSpaceManagerCID,
                                 NS_GET_IID(nsINameSpaceManager),
                                 (nsISupports**) &gNameSpaceManager);

     NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get namespace manager");
  }

  mDocument = nsnull;
  mDocumentURL = nsnull;
  mDocumentBaseURL = nsnull;
  mWebShell = nsnull;
  mParser = nsnull;
  mDocElement = nsnull;
  mContentStack = nsnull;
  mNameSpaceStack = nsnull;
  mText = nsnull;
  mTextLength = 0;
  mTextSize = 0;
  mConstrainSize = PR_TRUE;
  mInTitle = PR_FALSE;
  mStyleSheetCount = 0;
  mCSSLoader       = nsnull;
  mNeedToBlockParser = PR_FALSE;
  mPrettyPrintXML = PR_TRUE;
  mPrettyPrintHasSpecialRoot = PR_FALSE;
  mPrettyPrintHasFactoredElements = PR_FALSE;
}

nsXMLContentSink::~nsXMLContentSink()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_IF_RELEASE(gNameSpaceManager);
  }

  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mDocumentBaseURL);
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mDocElement);
  if (nsnull != mNameSpaceStack) {
    // There shouldn't be any here except in an error condition
    PRInt32 index = mNameSpaceStack->Count();
    while (0 < index--) {
      nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
      NS_RELEASE(nameSpace);
    }
    delete mNameSpaceStack;
  }
  if (nsnull != mText) {
    PR_FREEIF(mText);
  }
  NS_IF_RELEASE(mCSSLoader);
}

nsresult
nsXMLContentSink::Init(nsIDocument* aDoc,
                       nsIURI* aURL,
                       nsIWebShell* aContainer,
                       nsIChannel* aChannel)
{
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_OUT_OF_MEMORY);

  NS_PRECONDITION(nsnull != aDoc, "null ptr");
  NS_PRECONDITION(nsnull != aURL, "null ptr");
  if ((nsnull == aDoc) || (nsnull == aURL)) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument = aDoc;
  NS_ADDREF(aDoc);
  mDocumentURL = aURL;
  NS_ADDREF(aURL);
  mDocumentBaseURL = aURL;
  NS_ADDREF(aURL);
  mWebShell = aContainer;
  NS_IF_ADDREF(mWebShell);
  if (!mWebShell) {
    mPrettyPrintXML = PR_FALSE;
  }

  nsCOMPtr<nsIScriptLoader> loader;
  nsresult rv = mDocument->GetScriptLoader(getter_AddRefs(loader));
  NS_ENSURE_SUCCESS(rv, rv);
  loader->AddObserver(this);

  mState = eXMLContentSinkState_InProlog;
  mDocElement = nsnull;

  nsIHTMLContentContainer* htmlContainer = nsnull;
  if (NS_SUCCEEDED(aDoc->QueryInterface(NS_GET_IID(nsIHTMLContentContainer), (void**)&htmlContainer))) {
    htmlContainer->GetCSSLoader(mCSSLoader);
    NS_RELEASE(htmlContainer);
  }

  ProcessHTTPHeaders(aChannel);

  return aDoc->GetNodeInfoManager(*getter_AddRefs(mNodeInfoManager));
}

NS_IMPL_THREADSAFE_ADDREF(nsXMLContentSink)
NS_IMPL_THREADSAFE_RELEASE(nsXMLContentSink)

NS_INTERFACE_MAP_BEGIN(nsXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIExpatSink)
  NS_INTERFACE_MAP_ENTRY(nsITransformObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIScriptLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLContentSink)
NS_INTERFACE_MAP_END

  // nsIContentSink
NS_IMETHODIMP
nsXMLContentSink::WillBuildModel(void)
{
  // Notify document that the load is beginning
  mDocument->BeginLoad();
  return NS_OK;
}

// This function's implementation is in nsHTMLContentSink.cpp
nsresult CharsetConvRef(const nsString& aDocCharset, const nsCString& aRefInDocCharset, nsString& aRefInUnicode);

void
nsXMLContentSink::ScrollToRef()
{
  // XXX Duplicate code in nsHTMLContentSink.
  // XXX Be sure to change both places if you make changes here.
  if (!mRef.IsEmpty()) {
    char* tmpstr = ToNewCString(mRef);
    if(! tmpstr)
      return;
    nsUnescape(tmpstr);
    nsCAutoString unescapedRef;
    unescapedRef.Assign(tmpstr);
    nsMemory::Free(tmpstr);

    nsresult rv = NS_ERROR_FAILURE;
    // We assume that the bytes are in UTF-8, as it says in the spec:
    // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
    nsAutoString ref = NS_ConvertUTF8toUCS2(unescapedRef);

    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsCOMPtr<nsIPresShell> shell;
      mDocument->GetShellAt(i, getter_AddRefs(shell));
      if (shell) {
        // Scroll to the anchor
        shell->FlushPendingNotifications(PR_FALSE);

        // Check an empty string which might be caused by the UTF-8 conversion
        if (!ref.IsEmpty())
          rv = shell->GoToAnchor(ref);
        else
          rv = NS_ERROR_FAILURE;

        // If UTF-8 URL failed then try to assume the string as a
        // document's charset.

        if (NS_FAILED(rv)) {
          nsAutoString docCharset;
          rv = mDocument->GetDocumentCharacterSet(docCharset);

          if (NS_SUCCEEDED(rv)) {
            rv = CharsetConvRef(docCharset, unescapedRef, ref);

            if (NS_SUCCEEDED(rv) && !ref.IsEmpty())
              rv = shell->GoToAnchor(ref);
          }
        }
      }
    }
  }
}

nsresult
nsXMLContentSink::MaybePrettyPrint()
{
  if (!mPrettyPrintXML || (mPrettyPrintHasFactoredElements &&
                           !mPrettyPrintHasSpecialRoot)) {
    mPrettyPrintXML = PR_FALSE;

    return NS_OK;
  }

  // Check for correct load-command or if we're in a display:none iframe
  nsAutoString command;
  mParser->GetCommand(command);
  if (!command.Equals(NS_LITERAL_STRING("view")) ||
      !mDocument->GetNumberOfShells()) {
    mPrettyPrintXML = PR_FALSE;

    return NS_OK;
  }

  // check if we're in an invisible iframe
  nsCOMPtr<nsIScriptGlobalObject> sgo;
  mDocument->GetScriptGlobalObject(getter_AddRefs(sgo));
  nsCOMPtr<nsIDOMWindowInternal> internalWin = do_QueryInterface(sgo);
  nsCOMPtr<nsIDOMElement> frameElem;
  if (internalWin) {
    internalWin->GetFrameElement(getter_AddRefs(frameElem));
  }

  if (frameElem) {
    nsCOMPtr<nsIDOMCSSStyleDeclaration> computedStyle;
    nsCOMPtr<nsIDOMDocument> frameOwnerDoc;
    frameElem->GetOwnerDocument(getter_AddRefs(frameOwnerDoc));
    nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface(frameOwnerDoc);
    if (docView) {
      nsCOMPtr<nsIDOMAbstractView> defaultView;
      docView->GetDefaultView(getter_AddRefs(defaultView));
      nsCOMPtr<nsIDOMViewCSS> defaultCSSView = do_QueryInterface(defaultView);
      if (defaultCSSView) {
        defaultCSSView->GetComputedStyle(frameElem, NS_LITERAL_STRING(""),
                                         getter_AddRefs(computedStyle));
      }
    }

    if (computedStyle) {
      nsAutoString visibility;
      computedStyle->GetPropertyValue(NS_LITERAL_STRING("visibility"), visibility);
      if (!visibility.Equals(NS_LITERAL_STRING("visible"))) {
        mPrettyPrintXML = PR_FALSE;

        return NS_OK;
      }
    }
  }
  
  // check the pref
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (prefs) {
    PRBool pref = PR_FALSE;
    prefs->GetBoolPref("layout.xml.prettyprint", &pref);
    if (!pref) {
      mPrettyPrintXML = PR_FALSE;

      return NS_OK;
    }
  }


  // Ok, we should prettyprint. Let's do it!
  nsresult rv = NS_OK;

  // Load the XSLT
  nsCOMPtr<nsIURI> xslUri;
  rv = NS_NewURI(getter_AddRefs(xslUri),
                 NS_LITERAL_CSTRING("chrome://communicator/content/xml/XMLPrettyPrint.xsl"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), xslUri, nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> xslDocument;
  nsCOMPtr<nsISyncLoadDOMService> loader =
    do_GetService("@mozilla.org/content/syncload-dom-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = loader->LoadLocalDocument(channel, nsnull, getter_AddRefs(xslDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Transform the document
  nsCOMPtr<nsIDocumentTransformer> transformer =
      do_CreateInstance("@mozilla.org/document-transformer;1?type=text/xsl", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMDocument> resultDocument;
  nsAutoString emptyStr;
  rv = NS_NewDOMDocument(getter_AddRefs(resultDocument), emptyStr, emptyStr,
                         nsnull, mDocumentURL);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMDocument> sourceDocument = do_QueryInterface(mDocument);
  rv = transformer->TransformDocument(sourceDocument, xslDocument,
                                      resultDocument, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the binding
  nsCOMPtr<nsIDOMDocumentXBL> xblDoc = do_QueryInterface(mDocument);
  NS_ASSERTION(xblDoc, "xml document doesn't implement nsIDOMDocumentXBL");
  NS_ENSURE_TRUE(xblDoc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> dummy;
  xblDoc->LoadBindingDocument(NS_LITERAL_STRING("chrome://communicator/content/xml/XMLPrettyPrint.xml"), getter_AddRefs(dummy));

  nsCOMPtr<nsIDOMElement> rootElem = do_QueryInterface(mDocElement);
  NS_ASSERTION(rootElem, "No root element");

  rv = xblDoc->AddBinding(rootElem, NS_LITERAL_STRING("chrome://communicator/content/xml/XMLPrettyPrint.xml#prettyprint"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Hand the result document to the binding
  nsCOMPtr<nsIBindingManager> manager;
  mDocument->GetBindingManager(getter_AddRefs(manager));
  nsCOMPtr<nsIObserver> binding;
  manager->GetBindingImplementation(mDocElement, NS_GET_IID(nsIObserver), (void**)getter_AddRefs(binding));
  NS_ASSERTION(binding, "Prettyprint binding doesn't implement nsIObserver");
  NS_ENSURE_TRUE(binding, NS_ERROR_UNEXPECTED);

  return binding->Observe(resultDocument, "prettyprint-dom-created", NS_LITERAL_STRING("").get());
}


NS_IMETHODIMP
nsXMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  nsresult rv = NS_OK;

  // XXX this is silly; who cares?
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(i, getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if(vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
    }
  }

  if (mTitleText.IsEmpty()) {
    nsCOMPtr<nsIDOMNSDocument> dom_doc(do_QueryInterface(mDocument));
    if (dom_doc) {
      dom_doc->SetTitle(NS_LITERAL_STRING(""));
    }
  }

  mDocument->SetRootContent(mDocElement);

  // Check if we want to prettyprint
  MaybePrettyPrint();

  if (mXSLTransformMediator) {
    rv = SetupTransformMediator();
  }

  // Kick off layout for non-XSLT transformed documents.
  if (!mXSLTransformMediator || NS_FAILED(rv)) {
    nsCOMPtr<nsIScriptLoader> loader;
    mDocument->GetScriptLoader(getter_AddRefs(loader));
    if (loader) {
      loader->RemoveObserver(this);
    }

    StartLayout();

#if 0 /* Disable until this works for XML */
    //  Scroll to Anchor only if the document was *not* loaded through history means. 
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
    if (docShell) {
      PRUint32 documentLoadType = 0;
      docShell->GetLoadType(&documentLoadType);
      if (!(documentLoadType & nsIDocShell::LOAD_CMD_HISTORY)) {
        ScrollToRef();
      }
    }
#else
    ScrollToRef();
#endif

    mDocument->EndLoad();
  }

  // Ref. Bug 49115
  // Do this hack to make sure that the parser
  // doesn't get destroyed, accidently, before 
  // the circularity, between sink & parser, is
  // actually broken. 
  nsCOMPtr<nsIParser> kungFuDeathGrip(mParser);

  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::OnTransformDone(nsresult aResult,
                                  nsIDOMDocument* aResultDocument)
{
  NS_ASSERTION(NS_FAILED(aResult) || aResultDocument,
               "Don't notify about transform success without a document.");

  // Reset the observer on the transform mediator
  mXSLTransformMediator->SetTransformObserver(nsnull);

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));

  if (NS_FAILED(aResult) && contentViewer) {
    // Transform failed.
    if (aResultDocument) {
      // We have an error document.
      contentViewer->SetDOMDocument(aResultDocument);
    }
    else {
      // We don't have an error document, display the
      // untransformed source document.
      nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(mDocument);
      contentViewer->SetDOMDocument(document);
    }
  }

  nsCOMPtr<nsIDocument> originalDocument = mDocument;
  if (NS_SUCCEEDED(aResult) || aResultDocument) {
    // Transform succeeded or it failed and we have an error
    // document to display.
    NS_RELEASE(mDocument);
    CallQueryInterface(aResultDocument, &mDocument); // addrefs
  }
  else
  {
    // Transform failed and we don't have an error document, display the
    // untransformed source document.
    mDocument->SetRootContent(mDocElement);
  }

  nsCOMPtr<nsIScriptLoader> loader;
  originalDocument->GetScriptLoader(getter_AddRefs(loader));
  if (loader) {
    loader->RemoveObserver(this);
  }

  // Start the layout process
  StartLayout();

#if 0 /* Disable until this works for XML */
  //  Scroll to Anchor only if the document was *not* loaded through history means. 
  PRUint32 documentLoadType = 0;
  docShell->GetLoadType(&documentLoadType);
  if (!(documentLoadType & nsIDocShell::LOAD_CMD_HISTORY)) {
    ScrollToRef();
  }
#else
  ScrollToRef();
#endif

  originalDocument->EndLoad();

  return NS_OK;
}


// Provide the transform mediator with the source document's content
// model and the output document, and register the XML content sink
// as the transform observer.  The transform mediator will call
// the nsIObserver::Observe() method on the transform observer once
// the transform is completed.  The nsISupports pointer to the Observe
// method will be an nsIDOMElement pointer to the root node of the output
// content model.
nsresult
nsXMLContentSink::SetupTransformMediator()
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMDocument> currentDOMDoc(do_QueryInterface(mDocument));
  mXSLTransformMediator->SetSourceContentModel(currentDOMDoc);

  // Create the result document
  nsCOMPtr<nsIDOMDocument> resultDOMDoc;

  nsCOMPtr<nsIURI> url;
  mDocument->GetBaseURL(*getter_AddRefs(url));

  nsAutoString emptyStr;
  rv = NS_NewDOMDocument(getter_AddRefs(resultDOMDoc), emptyStr, emptyStr, nsnull, url);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    contentViewer->SetDOMDocument(resultDOMDoc);
  }

  mXSLTransformMediator->SetResultDocument(resultDOMDoc);
  mXSLTransformMediator->SetTransformObserver(this);

  return rv;
}

NS_IMETHODIMP
nsXMLContentSink::WillInterrupt(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::WillResume(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::SetParser(nsIParser* aParser)
{
  NS_IF_RELEASE(mParser);
  mParser = aParser;
  NS_IF_ADDREF(mParser);

  return NS_OK;
}

// static
void
nsXMLContentSink::SplitXMLName(const nsAString& aString, nsIAtom **aPrefix,
                               nsIAtom **aLocalName)
{
  nsReadingIterator<PRUnichar> iter, end;

  aString.BeginReading(iter);
  aString.EndReading(end);

  FindCharInReadable(kNameSpaceSeparator, iter, end);

  if (iter != end) {
    nsReadingIterator<PRUnichar> start;

    aString.BeginReading(start);

    *aPrefix = NS_NewAtom(nsDependentSubstring(start, iter));

    ++iter;

    *aLocalName = NS_NewAtom(nsDependentSubstring(iter, end));

    return;
  }

  *aPrefix = nsnull;
  *aLocalName = NS_NewAtom(aString);
}

nsresult
nsXMLContentSink::CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount, PRInt32 aNameSpaceID, 
                                nsINodeInfo* aNodeInfo, nsIContent** aResult)
{
  nsresult rv = NS_OK;
  // The first step here is to see if someone has provided their
  // own content element implementation (e.g., XUL or MathML).  
  // This is done based off a contractid/namespace scheme. 
  nsCOMPtr<nsIElementFactory> elementFactory;
  gNameSpaceManager->GetElementFactory(aNameSpaceID,
                                       getter_AddRefs(elementFactory));
  if (elementFactory) {
    // Create the content element using the element factory.
    elementFactory->CreateInstanceByTag(aNodeInfo, aResult);
  }
  else {
    NS_NewXMLElement(aResult, aNodeInfo);
  }

  // If we care, find out if we just used a special factory.
  if (!mPrettyPrintHasFactoredElements && !mPrettyPrintHasSpecialRoot &&
      mPrettyPrintXML) {
    PRBool hasFactory = PR_FALSE;
    rv = gNameSpaceManager->HasRegisteredFactory(aNameSpaceID, &hasFactory);
    NS_ENSURE_SUCCESS(rv, rv);
    mPrettyPrintHasFactoredElements = hasFactory;
  }

  return NS_OK;
}

nsresult
nsXMLContentSink::AddContentAsLeaf(nsIContent *aContent)
{
  nsresult result = NS_OK;

  if ((eXMLContentSinkState_InProlog == mState) ||
      (eXMLContentSinkState_InEpilog == mState)) {
    nsCOMPtr<nsIDOMDocument> domDoc( do_QueryInterface(mDocument) );
    nsCOMPtr<nsIDOMNode> trash;
    nsCOMPtr<nsIDOMNode> child( do_QueryInterface(aContent) );
    NS_ASSERTION(child, "not a dom node");
    domDoc->AppendChild(child, getter_AddRefs(trash));
  }
  else {
    nsCOMPtr<nsIContent> parent = getter_AddRefs(GetCurrentContent());

    if (parent) {
      result = parent->AppendChildTo(aContent, PR_FALSE, PR_FALSE);
    }
  }

  return result;
}

// Create an XML parser and an XSL content sink and start parsing
// the XSL stylesheet located at the given URL.
nsresult
nsXMLContentSink::LoadXSLStyleSheet(nsIURI* aUrl)
{
  nsresult rv = NS_OK;

  // Create a transform mediator
  rv = NS_NewTransformMediator(getter_AddRefs(mXSLTransformMediator),
                               NS_LITERAL_CSTRING(kXSLType));
  if (NS_FAILED(rv)) {
    // No XSLT processor available, continue normal document loading
    return NS_OK;
  }

  static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

  // Create the XML parser
  nsCOMPtr<nsIParser> parser(do_CreateInstance(kCParserCID, &rv));
  if (NS_FAILED(rv)) return rv;

  // Enable the transform mediator. It will start the transform
  // as soon as it has enough state to do so.  The state needed is
  // the source content model, the style content model, the current
  // document, and an observer.  The XML and XSL content sinks provide
  // this state by calling the various setters on nsITransformMediator.
  mXSLTransformMediator->SetEnabled(PR_TRUE);

  // Create the XSL stylesheet document
  nsCOMPtr<nsIDOMDocument> styleDOMDoc;
  nsAutoString emptyStr;
  emptyStr.Truncate();
  rv = NS_NewDOMDocument(getter_AddRefs(styleDOMDoc), emptyStr, emptyStr, nsnull, aUrl);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIDocument> styleDoc(do_QueryInterface(styleDOMDoc));

  // Create the XSL content sink
  nsCOMPtr<nsIXMLContentSink> sink;
  rv = NS_NewXSLContentSink(getter_AddRefs(sink), mXSLTransformMediator, styleDoc, aUrl, mWebShell);
  if (NS_FAILED(rv)) return rv;

  // Hook up the content sink to the parser's output and ask the parser
  // to start parsing the URL specified by aURL.
  parser->SetContentSink(sink);
  NS_NAMED_LITERAL_STRING(utf8, "UTF-8");
  styleDoc->SetDocumentCharacterSet(utf8);
  parser->SetDocumentCharset(utf8, kCharsetFromDocTypeDefault);
  parser->Parse(aUrl);

  // Set the parser as the stream listener and start the URL load
  nsCOMPtr<nsIStreamListener> sl;
  rv = parser->QueryInterface(NS_GET_IID(nsIStreamListener), (void**)getter_AddRefs(sl));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aUrl);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING(""));
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"));

    // XXX need to set the referrer equal to the document URI!
    // httpChannel->SetReferrer(documentURI);
  }

  // XXX need to set a load group on this channel!

  return channel->AsyncOpen(sl, nsnull);
}

NS_IMETHODIMP
nsXMLContentSink::ProcessStyleLink(nsIContent* aElement,
                                   const nsString& aHref,
                                   PRBool aAlternate,
                                   const nsString& aTitle,
                                   const nsString& aType,
                                   const nsString& aMedia)
{
  nsresult rv = NS_OK;
  mPrettyPrintXML = PR_FALSE;

  if (aType.EqualsIgnoreCase(kXSLType) ||
      aType.EqualsIgnoreCase(kXMLTextContentType) ||
      aType.EqualsIgnoreCase(kXMLApplicationContentType)) {
    if (aAlternate) {
      // don't load alternate XSLT
      return NS_OK;
    }
    // LoadXSLStyleSheet needs a mWebShell.
    if (!mWebShell)
      return NS_OK;

    nsCOMPtr<nsIURI> url;
    rv = NS_NewURI(getter_AddRefs(url), aHref, nsnull, mDocumentBaseURL);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptSecurityManager> secMan = 
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = secMan->CheckLoadURI(mDocumentURL, url,
                              nsIScriptSecurityManager::ALLOW_CHROME);
    if (NS_FAILED(rv))
      return NS_OK;
    rv = LoadXSLStyleSheet(url);
  } else if (aType.Equals(NS_LITERAL_STRING("text/css"))) {
    nsCOMPtr<nsIURI> url;
    rv = NS_NewURI(getter_AddRefs(url), aHref, nsnull, mDocumentBaseURL);
    if (NS_FAILED(rv)) {
      return NS_OK; // The URL is bad, move along, don't propagate the error (for now)
    }
    PRBool doneLoading;
    rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_Unknown,
                                   mStyleSheetCount++, 
                                   ((!aAlternate) ? mParser : nsnull),
                                   doneLoading, 
                                   this);
    if (NS_SUCCEEDED(rv) || (rv == NS_ERROR_HTMLPARSER_BLOCK)) {
      if (rv == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
        mParser->BlockParser();
      }
      mStyleSheetCount++;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsXMLContentSink::StyleSheetLoaded(nsICSSStyleSheet* aSheet, 
                                  PRBool aDidNotify)
{
  return NS_OK;
}

nsresult
nsXMLContentSink::ProcessBASETag()
{
  nsresult rv = NS_OK;

  if (mDocument) {
    nsAutoString value;
  
    if (NS_CONTENT_ATTR_HAS_VALUE == mBaseElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, value)) {
      mDocument->SetBaseTarget(value);
    }

    if (NS_CONTENT_ATTR_HAS_VALUE == mBaseElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, value)) {
      nsCOMPtr<nsIURI> baseURI;
      rv = NS_NewURI(getter_AddRefs(baseURI), value);
      if (NS_SUCCEEDED(rv)) {
        rv = mDocument->SetBaseURL(baseURI); // The document checks if it is legal to set this base
        if (NS_SUCCEEDED(rv)) {
          NS_IF_RELEASE(mDocumentBaseURL);
          mDocument->GetBaseURL(mDocumentBaseURL);
        }
      }
    }
  }

  return rv;
}

static PRBool IsAlternateHTTPStyleSheetHeader(const nsAString& aRel)
{
  nsStringArray linkTypes;
  nsStyleLinkElement::ParseLinkTypes(aRel, linkTypes);
  if (-1 != linkTypes.IndexOf(NS_LITERAL_STRING("stylesheet"))) {  // is it a stylesheet link?

    if (-1 != linkTypes.IndexOf(NS_LITERAL_STRING("alternate"))) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// XXX Copied from HTML, should be shared
static const PRUnichar kSemiCh = PRUnichar(';');
static const PRUnichar kCommaCh = PRUnichar(',');
static const PRUnichar kEqualsCh = PRUnichar('=');
static const PRUnichar kLessThanCh = PRUnichar('<');
static const PRUnichar kGreaterThanCh = PRUnichar('>');

nsresult 
nsXMLContentSink::ProcessLink(nsIHTMLContent* aElement,
                              const nsAString& aLinkData)
{
  nsresult result = NS_OK;
  
  // parse link content and call process style link
  nsAutoString href;
  nsAutoString rel;
  nsAutoString title;
  nsAutoString type;
  nsAutoString media;
  PRBool didBlock = PR_FALSE;

  nsAutoString  stringList(aLinkData); // copy to work buffer
  
  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList.get();
  PRUnichar* end   = start;
  PRUnichar* last  = start;
  PRUnichar  endCh;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }

    end = start;
    last = end - 1;

    while ((kNullCh != *end) && (kSemiCh != *end) && (kCommaCh != *end)) { // look for semicolon or comma
      if ((kApostrophe == *end) || (kQuote == *end) || 
          (kLessThanCh == *end)) { // quoted string
        PRUnichar quote = *end;
        if (kLessThanCh == quote) {
          quote = kGreaterThanCh;
        }
        PRUnichar* closeQuote = (end + 1);
        while ((kNullCh != *closeQuote) && (quote != *closeQuote)) {
          closeQuote++; // seek closing quote
        }
        if (quote == *closeQuote) { // found closer
          end = closeQuote; // skip to close quote
          last = end - 1;
          if ((kSemiCh != *(end + 1)) && (kNullCh != *(end + 1)) && (kCommaCh != *(end + 1))) {
            *(++end) = kNullCh;     // end string here
            while ((kNullCh != *(end + 1)) && (kSemiCh != *(end + 1)) &&
                   (kCommaCh != *(end + 1))) { // keep going until semi or comma
              end++;
            }
          }
        }
      }
      end++;
      last++;
    }

    endCh = *end;
    *end = kNullCh; // end string here

    if (start < end) {
      if ((kLessThanCh == *start) && (kGreaterThanCh == *last)) {
        *last = kNullCh;
        if (href.IsEmpty()) { // first one wins
          href = (start + 1);
          href.StripWhitespace();
        }
      }
      else {
        PRUnichar* equals = start;
        while ((kNullCh != *equals) && (kEqualsCh != *equals)) {
          equals++;
        }
        if (kNullCh != *equals) {
          *equals = kNullCh;
          nsAutoString  attr(start);
          attr.StripWhitespace();

          PRUnichar* value = ++equals;
          while (nsCRT::IsAsciiSpace(*value)) {
            value++;
          }
          if (((kApostrophe == *value) || (kQuote == *value)) &&
              (*value == *last)) {
            *last = kNullCh;
            value++;
          }

          if (attr.EqualsIgnoreCase("rel")) {
            if (rel.IsEmpty()) {
              rel = value;
              rel.CompressWhitespace();
            }
          }
          else if (attr.EqualsIgnoreCase("title")) {
            if (title.IsEmpty()) {
              title = value;
              title.CompressWhitespace();
            }
          }
          else if (attr.EqualsIgnoreCase("type")) {
            if (type.IsEmpty()) {
              type = value;
              type.StripWhitespace();
            }
          }
          else if (attr.EqualsIgnoreCase("media")) {
            if (media.IsEmpty()) {
              media = value;
              ToLowerCase(media); // HTML4.0 spec is inconsistent, make it case INSENSITIVE
            }
          }
        }
      }
    }
    if (kCommaCh == endCh) {  // hit a comma, process what we've got so far
      if (!href.IsEmpty()) {
        result = ProcessStyleLink(
          aElement, 
          href, 
          !title.IsEmpty() && IsAlternateHTTPStyleSheetHeader(rel), 
          title, 
          type, 
          media);
        if (NS_ERROR_HTMLPARSER_BLOCK == result) {
          didBlock = PR_TRUE;
        }
      }
      href.Truncate();
      rel.Truncate();
      title.Truncate();
      type.Truncate();
      media.Truncate();
    }

    start = ++end;
  }

  if (!href.IsEmpty()) {
    result = ProcessStyleLink(
      aElement, 
      href, 
      !title.IsEmpty() && IsAlternateHTTPStyleSheetHeader(rel), 
      title, 
      type, 
      media);
    if (NS_SUCCEEDED(result) && didBlock) {
      result = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }
  return result;
}

// XXX Copied from HTML, should be shared
nsresult
nsXMLContentSink::ProcessHeaderData(nsIAtom* aHeader,const nsAString& aValue,nsIHTMLContent* aContent)
{
  nsresult rv=NS_OK;
  // XXX necko isn't going to process headers coming in from the parser          
  //NS_WARNING("need to fix how necko adds mime headers (in HTMLContentSink::ProcessMETATag)");

  // If we add support for linking scripts here then we have to remember to
  // turn off prettyprinting

  mDocument->SetHeaderData(aHeader, aValue);
  
  // see if we have a refresh "header".
  if (aHeader == nsHTMLAtoms::refresh) {
    // first get our baseURI
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mWebShell, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(docShell);
    rv = webNav->GetCurrentURI(getter_AddRefs(baseURI));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRefreshURI> reefer = do_QueryInterface(mWebShell);
    if (reefer) {
      rv = reefer->SetupRefreshURIFromHeader(baseURI, NS_ConvertUCS2toUTF8(aValue));
      if (NS_FAILED(rv)) return rv;
    }
  } // END refresh
  else if (aHeader == nsHTMLAtoms::setcookie) {
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mWebShell, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsICookieService> cookieServ = do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    // Get a URI from the document principal
    // We use the original codebase in case the codebase was changed by SetDomain
    nsCOMPtr<nsIPrincipal> docPrincipal;
    rv = mDocument->GetPrincipal(getter_AddRefs(docPrincipal));
    if (NS_FAILED(rv)) return rv;
    if (!docPrincipal) return NS_OK;

    nsCOMPtr<nsIAggregatePrincipal> agg(do_QueryInterface(docPrincipal, &rv));
    // Document principal should always be an aggregate
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> originalPrincipal;
    rv = agg->GetOriginalCodebase(getter_AddRefs(originalPrincipal));
    nsCOMPtr<nsICodebasePrincipal> originalCodebase(
        do_QueryInterface(originalPrincipal, &rv));
    if (NS_FAILED(rv)) {
      // Document's principal is not a codebase (may be system), so can't set cookies
      return NS_OK; 
    }

    nsCOMPtr<nsIURI> codebaseURI;
    rv = originalCodebase->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(rv, rv);

    char *cookie = ToNewUTF8String(aValue);
    nsCOMPtr<nsIScriptGlobalObject> globalObj;
    nsCOMPtr<nsIPrompt> prompt;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
    if (globalObj) {
      nsCOMPtr<nsIDOMWindowInternal> window (do_QueryInterface(globalObj));
      if (window) {
        window->GetPrompter(getter_AddRefs(prompt));
      }
    }
    
    nsCOMPtr<nsIHttpChannel> httpChannel;
    if (mParser) {
      nsCOMPtr<nsIChannel> channel;
      if (NS_SUCCEEDED(mParser->GetChannel(getter_AddRefs(channel)))) {
        httpChannel = do_QueryInterface(channel);
      }
    }

    rv = cookieServ->SetCookieString(codebaseURI, prompt, cookie, httpChannel);
    nsCRT::free(cookie);
    if (NS_FAILED(rv)) return rv;
  } // END set-cookie
  else if (aHeader == nsHTMLAtoms::link) {
    rv = ProcessLink(aContent, aValue);
  }
  else if (aHeader == nsHTMLAtoms::msthemecompatible) {
    // Disable theming for the presshell if the value is no.
    nsAutoString value(aValue);
    if (value.EqualsIgnoreCase("no")) {
      nsCOMPtr<nsIPresShell> shell;
      mDocument->GetShellAt(0, getter_AddRefs(shell));
      if (shell)
        shell->DisableThemeSupport();
    }
  }
  else if (mParser) {
    // we also need to report back HTTP-EQUIV headers to the channel
    // so that it can process things like pragma: no-cache or other
    // cache-control headers. Ideally this should also be the way for
    // cookies to be set! But we'll worry about that in the next
    // iteration
    nsCOMPtr<nsIChannel> channel;
    if (NS_SUCCEEDED(mParser->GetChannel(getter_AddRefs(channel)))) {
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      if (httpChannel) {
        const PRUnichar *header = 0;
        (void)aHeader->GetUnicode(&header);
        (void)httpChannel->SetResponseHeader(
                       NS_ConvertUCS2toUTF8(header),
                       NS_ConvertUCS2toUTF8(aValue));
      }
    }
  }
  return rv;
}

// XXX Copied from HTML content sink, should be shared
nsresult 
nsXMLContentSink::ProcessHTTPHeaders(nsIChannel* aChannel) {
  nsresult rv=NS_OK;

  if(aChannel) {
    nsCOMPtr<nsIHttpChannel> httpchannel(do_QueryInterface(aChannel));
    if (httpchannel) {
      const char *const headers[] = {
        "link",
        "default-style",
        "content-style-type",
        // add more http headers if you need
        0};
      const char *const *name=headers;
      nsCAutoString tmp;

      while(*name) {
        rv = httpchannel->GetResponseHeader(nsDependentCString(*name), tmp);
        if (NS_SUCCEEDED(rv) && !tmp.IsEmpty()) {
          nsCOMPtr<nsIAtom> key(dont_AddRef(NS_NewAtom(*name)));
          ProcessHeaderData(key,NS_ConvertASCIItoUCS2(tmp),nsnull);
        }
        name++;
      }//while
    }//if - httpchannel 
  }//if - channel

  return rv;
}

nsresult
nsXMLContentSink::ProcessMETATag()
{
  nsresult rv = NS_OK;

  // set any HTTP-EQUIV data into document's header data as well as url
  nsAutoString header;
  mMetaElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::httpEquiv, header);
  if (!header.IsEmpty()) {
    nsAutoString result;
    mMetaElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::content, result);
    if (!result.IsEmpty()) {
      ToLowerCase(header);
      nsCOMPtr<nsIAtom> fieldAtom(dont_AddRef(NS_NewAtom(header)));
      rv=ProcessHeaderData(fieldAtom,result,mMetaElement); 
    }//if (!result.IsEmpty()) 
  }//if (!header.IsEmpty()) 

  return rv;
}

NS_IMETHODIMP 
nsXMLContentSink::SetDocumentCharset(nsAString& aCharset)
{
  if (mDocument) {
    return mDocument->SetDocumentCharacterSet(aCharset);
  }
  
  return NS_OK;
}

nsresult
nsXMLContentSink::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;
  if (0 != mTextLength) {
    if (aCreateTextNode) {
      nsIContent* content;
      rv = NS_NewTextNode(&content);
      if (NS_OK == rv) {
        // Set the content's document
        content->SetDocument(mDocument, PR_FALSE, PR_TRUE);

        // Set the text in the text node
        nsITextContent* text = nsnull;
        content->QueryInterface(NS_GET_IID(nsITextContent), (void**) &text);
        text->SetText(mText, mTextLength, PR_FALSE);
        NS_RELEASE(text);

        // Add text to its parent
        AddContentAsLeaf(content);
        NS_RELEASE(content);
      }
    }
    mTextLength = 0;
    didFlush = PR_TRUE;
  }
  if (nsnull != aDidFlush) {
    *aDidFlush = didFlush;
  }
  return rv;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

PRInt32
nsXMLContentSink::GetNameSpaceId(nsIAtom* aPrefix)
{
  PRInt32 id = aPrefix ? kNameSpaceID_Unknown : kNameSpaceID_None;

  if (mNameSpaceStack && mNameSpaceStack->Count() > 0) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    nameSpace->FindNameSpaceID(aPrefix, id);
  }

  return id;
}

nsINameSpace*
nsXMLContentSink::PopNameSpaces()
{
  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    mNameSpaceStack->RemoveElementAt(index);
    return nameSpace;
  }

  return nsnull;
}

nsIContent*
nsXMLContentSink::GetCurrentContent()
{
  if (nsnull != mContentStack) {
    PRUint32 count;
    mContentStack->Count(&count);
    PR_ASSERT(count);
    if (count) {
      return (nsIContent *)mContentStack->ElementAt(count-1);
    }
  }
  return nsnull;
}

PRInt32
nsXMLContentSink::PushContent(nsIContent *aContent)
{
  PRUint32 count;
  if (nsnull == mContentStack) {
    NS_NewISupportsArray(getter_AddRefs(mContentStack));
  }

  mContentStack->AppendElement(aContent);
  mContentStack->Count(&count);

  return count;
}

nsIContent*
nsXMLContentSink::PopContent()
{  
  nsIContent* content = nsnull;
  if (nsnull != mContentStack) {
    PRUint32 index, count;
    mContentStack->Count(&count);
    index =  count - 1;
    content = (nsIContent *)mContentStack->ElementAt(index);
    mContentStack->RemoveElementAt(index);
  }

  // The caller should NS_RELEASE the returned content object.
  return content;
}

void
nsXMLContentSink::StartLayout()
{
  // Reset scrolling to default settings for this shell.
  // This must happen before the initial reflow, when we create the root frame
  nsCOMPtr<nsIScrollable> scrollableContainer(do_QueryInterface(mWebShell));
  if (scrollableContainer) {
    scrollableContainer->ResetScrollbarPreferences();
  }

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(i, getter_AddRefs(shell));
    if (nsnull != shell) {
      // Make shell an observer for next time
      shell->BeginObservingDocument();

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsRect r;
      cx->GetVisibleArea(r);
      shell->InitialReflow(r.width, r.height);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        RefreshIfEnabled(vm);
      }

    }
  }

  // If the document we are loading has a reference or it is a top level
  // frameset document, disable the scroll bars on the views.
  nsCAutoString ref;
  nsIURL* url;
  nsresult rv = mDocumentURL->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
  if (NS_SUCCEEDED(rv)) {
    rv = url->GetRef(ref);
    NS_RELEASE(url);
  }
  if (rv == NS_OK) {
    NS_UnescapeURL(ref); // XXX this may result in random non-ASCII bytes!
    mRef = NS_ConvertASCIItoUCS2(ref);
  }

  PRBool topLevelFrameset = PR_FALSE;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mWebShell));
  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
    if(docShellAsItem.get() == root.get()) {
      topLevelFrameset = PR_TRUE;
    }
  }

  if (!ref.IsEmpty() || topLevelFrameset) {
    // XXX support more than one presentation-shell here

    // Get initial scroll preference and save it away; disable the
    // scroll bars.
    ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsCOMPtr<nsIPresShell> shell;
      mDocument->GetShellAt(i, getter_AddRefs(shell));
      if (shell) {
        nsCOMPtr<nsIViewManager> vm;
        shell->GetViewManager(getter_AddRefs(vm));
        if (vm) {
          nsIView* rootView = nsnull;
          vm->GetRootView(rootView);
          if (nsnull != rootView) {
            nsIScrollableView* sview = nsnull;
            rootView->QueryInterface(NS_GET_IID(nsIScrollableView), (void**) &sview);
            if (nsnull != sview) {
              sview->SetScrollPreference(nsScrollPreference_kNeverScroll);
            }
          }
        }
      }
    }
  }
}

NS_IMETHODIMP
nsXMLContentSink::ScriptAvailable(nsresult aResult, 
                                  nsIDOMHTMLScriptElement *aElement, 
                                  PRBool aIsInline,
                                  PRBool aWasPending,
                                  nsIURI *aURI, 
                                  PRInt32 aLineNo,
                                  const nsAString& aScript)
{
  // Check if this is the element we were waiting for
  PRUint32 count;
  mScriptElements.Count(&count);
  nsCOMPtr<nsISupports> sup(dont_AddRef(mScriptElements.ElementAt(count-1)));
  nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement(do_QueryInterface(sup));
  if (aElement != scriptElement.get()) {
    return NS_OK;
  }

  if (mParser && !mParser->IsParserEnabled()) {
    // make sure to unblock the parser before evaluating the script,
    // we must unblock the parser even if loading the script failed or
    // if the script was empty, if we don't, the parser will never be
    // unblocked.
    mParser->UnblockParser();
  }

  // Mark the current script as loaded
  mNeedToBlockParser = PR_FALSE;

  if (NS_FAILED(aResult)) {
    mScriptElements.RemoveElementAt(count-1);
     
    if(mParser && aWasPending){
      // Loading external script failed!. So, resume
      // parsing since the parser got blocked when loading
      // external script. - Ref. Bug: 94903
      mParser->ContinueParsing();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSink::ScriptEvaluated(nsresult aResult, 
                                  nsIDOMHTMLScriptElement *aElement,
                                  PRBool aIsInline,
                                  PRBool aWasPending)
{
  // Check if this is the element we were waiting for
  PRUint32 count;
  mScriptElements.Count(&count);
  nsCOMPtr<nsISupports> sup(dont_AddRef(mScriptElements.ElementAt(count-1)));
  nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement(do_QueryInterface(sup));
  if (aElement != scriptElement.get()) {
    return NS_OK;
  }

  // Pop the script element stack
  mScriptElements.RemoveElementAt(count-1); 

  if(mParser && mParser->IsParserEnabled() && aWasPending){
    mParser->ContinueParsing();
  }

  return NS_OK;
}

nsresult
nsXMLContentSink::RefreshIfEnabled(nsIViewManager* vm)
{
  if (vm) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
    nsCOMPtr<nsIContentViewer> contentViewer;
    nsresult rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
    if (NS_SUCCEEDED(rv) && contentViewer) {
      PRBool enabled;
      contentViewer->GetEnableRendering(&enabled);
      if (enabled) {
        vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
      }
    }
  }
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
//
//   XML Element Factory
//

class XMLElementFactoryImpl : public nsIElementFactory
{
protected:
  XMLElementFactoryImpl();
  virtual ~XMLElementFactoryImpl();

public:
  friend
  nsresult
  NS_NewXMLElementFactory(nsIElementFactory** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIElementFactory interface
  NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo, nsIContent** aResult);

};


XMLElementFactoryImpl::XMLElementFactoryImpl()
{
  NS_INIT_ISUPPORTS();
}

XMLElementFactoryImpl::~XMLElementFactoryImpl()
{
}


NS_IMPL_ISUPPORTS1(XMLElementFactoryImpl, nsIElementFactory)


nsresult
NS_NewXMLElementFactory(nsIElementFactory** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  XMLElementFactoryImpl* result = new XMLElementFactoryImpl();
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  *aResult = result;
  NS_ADDREF(*aResult);
 
  return NS_OK;
}



NS_IMETHODIMP
XMLElementFactoryImpl::CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                           nsIContent** aResult)
{
  return NS_NewXMLElement(aResult, aNodeInfo);
}


#ifdef MOZ_MATHML
////////////////////////////////////////////////////////////////////////
// MathML Element Factory - temporary location for bug 132844
// Will be factored out post 1.0

class MathMLElementFactoryImpl : public nsIElementFactory
{
protected:
  MathMLElementFactoryImpl();
  virtual ~MathMLElementFactoryImpl();

public:
  friend
  nsresult
  NS_NewMathMLElementFactory(nsIElementFactory** aResult);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIElementFactory interface
  NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo, nsIContent** aResult);

};

MathMLElementFactoryImpl::MathMLElementFactoryImpl()
{
  NS_INIT_ISUPPORTS();
}

MathMLElementFactoryImpl::~MathMLElementFactoryImpl()
{
}

NS_IMPL_ISUPPORTS1(MathMLElementFactoryImpl, nsIElementFactory)

nsresult
NS_NewMathMLElementFactory(nsIElementFactory** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  MathMLElementFactoryImpl* result = new MathMLElementFactoryImpl();
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  *aResult = result;
  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
MathMLElementFactoryImpl::CreateInstanceByTag(nsINodeInfo* aNodeInfo,
                                              nsIContent** aResult)
{
  static const char kMathMLStyleSheetURI[] = "resource:///res/mathml.css";

  // this bit of code is to load mathml.css on demand
  nsCOMPtr<nsIDocument> doc;
  aNodeInfo->GetDocument(*getter_AddRefs(doc));
  if (doc) {
    PRBool alreadyLoaded = PR_FALSE;
    PRInt32 i = 0, sheetCount = 0;
    doc->GetNumberOfStyleSheets(&sheetCount);
    for (; i < sheetCount; i++) {
      nsCOMPtr<nsIStyleSheet> sheet;
      doc->GetStyleSheetAt(i, getter_AddRefs(sheet));
      NS_ASSERTION(sheet, "unexpected null stylesheet in the document");
      if (sheet) {
        nsCOMPtr<nsIURI> uri;
        sheet->GetURL(*getter_AddRefs(uri));
        nsCAutoString uriStr;
        uri->GetSpec(uriStr);
        if (uriStr.Equals(kMathMLStyleSheetURI)) {
          alreadyLoaded = PR_TRUE;
          break;
        }
      }
    }
    if (!alreadyLoaded) {
      nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));
      if (htmlContainer) {
        nsCOMPtr<nsICSSLoader> cssLoader;
        htmlContainer->GetCSSLoader(*getter_AddRefs(cssLoader));
        if (cssLoader) {
          nsCOMPtr<nsIURI> uri;
          NS_NewURI(getter_AddRefs(uri), kMathMLStyleSheetURI);
          if (uri) {
            PRBool complete;
            nsCOMPtr<nsICSSStyleSheet> sheet;
            cssLoader->LoadAgentSheet(uri, *getter_AddRefs(sheet), complete, nsnull);
#ifdef NS_DEBUG
            nsCAutoString uriStr;
            uri->GetSpec(uriStr);
            printf("MathML Factory: loading catalog stylesheet: %s ... %s\n", uriStr.get(), sheet.get() ? "Done" : "Failed");
            NS_ASSERTION(uriStr.Equals(kMathMLStyleSheetURI), "resolved URI unexpected");
#endif
            if (sheet) {
              doc->AddStyleSheet(sheet, NS_STYLESHEET_FROM_CATALOG);
            }
          }
        }
      }
    }
  }

  return NS_NewXMLElement(aResult, aNodeInfo);
}
#endif // MOZ_MATHML


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsXMLContentSink::HandleStartElement(const PRUnichar *aName, 
                                     const PRUnichar **aAtts, 
                                     PRUint32 aAttsCount, 
                                     PRUint32 aIndex, 
                                     PRUint32 aLineNumber)
{
  nsresult result = NS_OK;
  PRBool appendContent = PR_TRUE;
  nsCOMPtr<nsIContent> content;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the epilog, there should be no
  // new elements
  PR_ASSERT(eXMLContentSinkState_InEpilog != mState);

  FlushText();

  mState = eXMLContentSinkState_InDocumentElement;

  nsCOMPtr<nsIAtom> nameSpacePrefix, tagAtom;

  SplitXMLName(nsDependentString(aName), getter_AddRefs(nameSpacePrefix),
               getter_AddRefs(tagAtom));

  // We must register namespace declarations found in the attribute list
  // of an element before creating the element. This is because the
  // namespace prefix for an element might be declared within the attribute
  // list.
  result = PushNameSpacesFrom(aAtts);
  NS_ENSURE_SUCCESS(result, result);

  PRInt32 nameSpaceID = GetNameSpaceId(nameSpacePrefix);

  if (!OnOpenContainer(aAtts, aAttsCount, nameSpaceID, tagAtom))
    return NS_OK;
  
  nsCOMPtr<nsINodeInfo> nodeInfo;

  mNodeInfoManager->GetNodeInfo(tagAtom, nameSpacePrefix, nameSpaceID,
                                *getter_AddRefs(nodeInfo));

  const PRBool isXHTML = nameSpaceID == kNameSpaceID_XHTML;

  if (isXHTML) {
    mPrettyPrintHasFactoredElements = PR_TRUE;
    if (tagAtom.get() == nsHTMLAtoms::script) {
      result = ProcessStartSCRIPTTag(aLineNumber);
      // Don't append the content to the tree until we're all
      // done collecting its contents
      appendContent = PR_FALSE;
    } else if (tagAtom.get() == nsHTMLAtoms::title) {
      if (mTitleText.IsEmpty())
        mInTitle = PR_TRUE; // The first title wins
    }

    nsCOMPtr<nsIHTMLContent> htmlContent;
    result = NS_CreateHTMLElement(getter_AddRefs(htmlContent), nodeInfo, PR_TRUE);
    content = do_QueryInterface(htmlContent);

    if (tagAtom == nsHTMLAtoms::base) {
      if (!mBaseElement) {
        mBaseElement = htmlContent; // The first base wins
      }
    } else if (tagAtom.get() == nsHTMLAtoms::meta) {
      if (!mMetaElement) {
        mMetaElement = htmlContent;
      }
    }
  }
  else
    CreateElement(aAtts, aAttsCount, nameSpaceID, nodeInfo, getter_AddRefs(content));

  if (NS_OK == result) {
    PRInt32 id;
    mDocument->GetAndIncrementContentID(&id);
    content->SetContentID(id);

    if (isXHTML &&
        ((tagAtom == nsHTMLAtoms::link) ||
         (tagAtom == nsHTMLAtoms::style))) {
      nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(content));

      if (ssle) {
        ssle->InitStyleLinkElement(mParser, PR_FALSE);
        ssle->SetEnableUpdates(PR_FALSE);
      }
    }

    content->SetDocument(mDocument, PR_FALSE, PR_TRUE);

    // Set the attributes on the new content element
    result = AddAttributes(aAtts, content, isXHTML);

    if (NS_OK == result) {
      // If this is the document element
      if (!mDocElement) {
        
        // check for root elements that needs special handling for
        // prettyprinting
        if ((nameSpaceID == kNameSpaceID_XBL &&
             tagAtom == nsXBLAtoms::bindings) ||
            (nameSpaceID == kNameSpaceID_XSLT &&
             (tagAtom == nsLayoutAtoms::stylesheet ||
              tagAtom == nsLayoutAtoms::transform))) {
          mPrettyPrintHasSpecialRoot = PR_TRUE;
        }

        mDocElement = content;
        NS_ADDREF(mDocElement);

        // For XSL, we need to wait till after the transform
        // to set the root content object.
        if (!mXSLTransformMediator)
            mDocument->SetRootContent(mDocElement);
      }
      else if (appendContent) {
        nsCOMPtr<nsIContent> parent = getter_AddRefs(GetCurrentContent());

        parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
      }

      PushContent(content);
    }

    // Set the ID attribute atom on the node info object for this node
    if ((aIndex != (PRUint32)-1) && NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIAtom> IDAttr =
        dont_AddRef(NS_NewAtom((const PRUnichar *)aAtts[aIndex]));

      if (IDAttr) {
        result = nodeInfo->SetIDAttributeAtom(IDAttr);
      }
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleEndElement(const PRUnichar *aName)
{
  nsresult result = NS_OK;
  PRBool popContent = PR_TRUE;
  PRBool appendContent = PR_FALSE;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the prolog or epilog, there should be
  // no close tags for elements.
  PR_ASSERT(eXMLContentSinkState_InDocumentElement == mState);

  FlushText();

  nsCOMPtr<nsIContent> currentContent(dont_AddRef(GetCurrentContent()));
  nsCOMPtr<nsIAtom> tagAtom;

  if (currentContent && currentContent->IsContentOfType(nsIContent::eHTML)) {
    currentContent->GetTag(*getter_AddRefs(tagAtom));

    if (tagAtom.get() == nsHTMLAtoms::script) {
      result = ProcessEndSCRIPTTag();
      appendContent = PR_TRUE;
    } else if (tagAtom.get() == nsHTMLAtoms::title) {
      if (mInTitle) { // The first title wins
        nsCOMPtr<nsIDOMNSDocument> dom_doc(do_QueryInterface(mDocument));
        if (dom_doc) {
          mTitleText.CompressWhitespace();
          dom_doc->SetTitle(mTitleText);
        }
        mInTitle = PR_FALSE;
      }
    } else if (tagAtom == nsHTMLAtoms::base) {
      if (mBaseElement) {
        result = ProcessBASETag();
      }
    } else if (tagAtom.get() == nsHTMLAtoms::meta) {
      if (mMetaElement) {
        result = ProcessMETATag();
        mMetaElement = nsnull;  // HTML can have more than one meta so clear this now
      }
    }
  }

  nsCOMPtr<nsIContent> content;
  if (popContent) {
    content = getter_AddRefs(PopContent());
    if (content) {
      if (mDocElement == content.get()) {
        mState = eXMLContentSinkState_InEpilog;
      }
      else if (appendContent) {
        nsCOMPtr<nsIContent> parent = getter_AddRefs(GetCurrentContent());

        parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
      }
    }
    else {
      // XXX Again, the parser should catch unmatched tags and
      // we should never get here.
      PR_ASSERT(0);
    }
  }
  nsINameSpace* nameSpace = PopNameSpaces();
  NS_IF_RELEASE(nameSpace);

  if (currentContent &&
      currentContent->IsContentOfType(nsIContent::eHTML) &&
      ((tagAtom == nsHTMLAtoms::link) ||
       (tagAtom == nsHTMLAtoms::style))) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(content));

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      result = ssle->UpdateStyleSheet(nsnull, mStyleSheetCount);
      if (NS_SUCCEEDED(result) || (result == NS_ERROR_HTMLPARSER_BLOCK)) {
        if (result == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
          mParser->BlockParser();
         }
        mStyleSheetCount++;
      }
    }
  }

  if (mNeedToBlockParser || (mParser && !mParser->IsParserEnabled())) {
    if (mParser) mParser->BlockParser();
    result = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleComment(const PRUnichar *aName)
{
  FlushText();

  nsIContent *comment;
  nsIDOMComment *domComment;
  nsresult result = NS_OK;

  result = NS_NewCommentNode(&comment);
  if (NS_OK == result) {
    result = comment->QueryInterface(NS_GET_IID(nsIDOMComment), (void **)&domComment);
    if (NS_OK == result) {
      domComment->AppendData(nsDependentString(aName));
      NS_RELEASE(domComment);

      comment->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      result = AddContentAsLeaf(comment);
    }
    NS_RELEASE(comment);
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleCDataSection(const PRUnichar *aData, 
                                     PRUint32 aLength)
{
  FlushText();

  nsIContent *cdata;
  nsIDOMCDATASection *domCDATA;
  nsresult result = NS_OK;
  
  if (mInTitle) {
    mTitleText.Append(aData, aLength);
  }
  
  result = NS_NewXMLCDATASection(&cdata);
  if (NS_OK == result) {
    result = cdata->QueryInterface(NS_GET_IID(nsIDOMCDATASection), (void **)&domCDATA);
    if (NS_OK == result) {
      domCDATA->SetData(nsDependentString(aData, aLength));
      NS_RELEASE(domCDATA);

      cdata->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      result = AddContentAsLeaf(cdata);
    }
    NS_RELEASE(cdata);
  }

  return result;
}

NS_IMETHODIMP
nsXMLContentSink::HandleDoctypeDecl(const nsAString & aSubset, 
                                    const nsAString & aName, 
                                    const nsAString & aSystemId, 
                                    const nsAString & aPublicId,
                                    nsISupports* aCatalogData)
{
  FlushText();

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mDocument));
  if (!doc)
    return NS_OK;

  nsCOMPtr<nsIDOMDocumentType> docType;
  
  // Create a new doctype node
  rv = NS_NewDOMDocumentType(getter_AddRefs(docType),
                             aName, nsnull, nsnull,
                             aPublicId, aSystemId, aSubset);
  if (NS_FAILED(rv) || !docType) {
    return rv;
  }

  if (aCatalogData && mCSSLoader && mDocument) {
    // bug 124570 - we only expect additional agent sheets for now -- ignore
    // exit codes, error are not fatal here, just that the stylesheet won't apply
    nsCOMPtr<nsIURI> uri(do_QueryInterface(aCatalogData));
    if (uri) {
      PRBool complete;
      nsCOMPtr<nsICSSStyleSheet> sheet;
      mCSSLoader->LoadAgentSheet(uri, *getter_AddRefs(sheet), complete, nsnull);
#ifdef NS_DEBUG
      nsCAutoString uriStr;
      uri->GetSpec(uriStr);
      printf("Loading catalog stylesheet: %s ... %s\n", uriStr.get(), sheet.get() ? "Done" : "Failed");
#endif
      if (sheet) {
        mDocument->AddStyleSheet(sheet, NS_STYLESHEET_FROM_CATALOG);
      }
    }
  }

  nsCOMPtr<nsIDOMNode> tmpNode;
  
  return doc->AppendChild(docType, getter_AddRefs(tmpNode));
}

NS_IMETHODIMP 
nsXMLContentSink::HandleCharacterData(const PRUnichar *aData, 
                                      PRUint32 aLength)
{
  nsresult result = NS_OK;
  if (aData) {
    result = AddText(aData,aLength);
  }
  return result;
}

NS_IMETHODIMP
nsXMLContentSink::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                              const PRUnichar *aData)
{
  FlushText();

  nsresult result = NS_OK;
  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  nsCOMPtr<nsIContent> node;

  result = NS_NewXMLProcessingInstruction(getter_AddRefs(node), target, data);
  if (NS_OK == result) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(node));

    if (ssle) {
      ssle->InitStyleLinkElement(mParser, PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
      mPrettyPrintXML = PR_FALSE;
    }

    result = AddContentAsLeaf(node);

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      result = ssle->UpdateStyleSheet(nsnull, mStyleSheetCount);
      if (NS_SUCCEEDED(result) || (result == NS_ERROR_HTMLPARSER_BLOCK))
        mStyleSheetCount++; // This count may not reflect the real stylesheet count
    }

    if (NS_FAILED(result)) {
      if (result == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
        mParser->BlockParser();
      }
      return result;
    }

    // If it's not a CSS stylesheet PI...
    nsAutoString type;
    nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("type"), type);
    if (mState == eXMLContentSinkState_InProlog && 
        target.Equals(NS_LITERAL_STRING("xml-stylesheet")) && 
        !type.EqualsIgnoreCase("text/css")) {
      nsAutoString href, title, media, alternate;

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("href"), href);
      // If there was no href, we can't do anything with this PI
      if (href.IsEmpty()) {
        return NS_OK;
      }

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("title"), title);
      title.CompressWhitespace();

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("media"), media);
      ToLowerCase(media);

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("alternate"), alternate);

      result = ProcessStyleLink(node, href, alternate.Equals(NS_LITERAL_STRING("yes")),
                                title, type, media);
    }
  }
  return result;
}

NS_IMETHODIMP
nsXMLContentSink::HandleXMLDeclaration(const PRUnichar *aData, 
                                       PRUint32 aLength)
{
  NS_ENSURE_ARG_POINTER(aData);
  // strlen("<?xml version='a'?>") == 19, shortest decl
  NS_ENSURE_TRUE(aLength >= 19, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIXMLDocument> xml(do_QueryInterface(mDocument));
  NS_WARN_IF_FALSE(xml, "why is XML sink building non-XML document?");
  if (!xml)
    return NS_OK;

  // <?xml version="a" encoding="a" standalone="yes|no"?>
  const nsAString& data = Substring(aData + 6, aData + aLength - 2); // strip out "<?xml " and "?>"

  nsAutoString version, encoding, standalone;

  // XXX If this is too slow we need to parse this here
  nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("version"), version);
  nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("encoding"), encoding);
  nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("standalone"), standalone);

  return xml->SetXMLDeclaration(version, encoding, standalone);
}

NS_IMETHODIMP
nsXMLContentSink::ReportError(const PRUnichar* aErrorText, 
                              const PRUnichar* aSourceText)
{
  nsresult rv = NS_OK;

  mState = eXMLContentSinkState_InProlog;

  // Clear the current content and
  // prepare to set <parsererror> as the document root
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mDocument));
  if (node) {
    for (;;) {
      nsCOMPtr<nsIDOMNode> child, dummy;
      node->GetLastChild(getter_AddRefs(child));
      if (!child)
        break;
      node->RemoveChild(child, getter_AddRefs(dummy));
    }
  }
  NS_IF_RELEASE(mDocElement); 

  if (mXSLTransformMediator) {
    // Get rid of the transform mediator.
    mXSLTransformMediator->SetEnabled(PR_FALSE);
    mXSLTransformMediator = nsnull;
  }

  NS_NAMED_LITERAL_STRING(name, "xmlns");
  NS_NAMED_LITERAL_STRING(value, "http://www.mozilla.org/newlayout/xml/parsererror.xml");

  const PRUnichar* atts[] = {name.get(), value.get(), nsnull};
    
  rv = HandleStartElement(NS_LITERAL_STRING("parsererror").get(), atts, 1,
                          (PRUint32)-1, (PRUint32)-1);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = HandleCharacterData(aErrorText, nsCRT::strlen(aErrorText));
  NS_ENSURE_SUCCESS(rv,rv);  
  
  const PRUnichar* noAtts[] = {0, 0};
  rv = HandleStartElement(NS_LITERAL_STRING("sourcetext").get(), noAtts, 0,
                          (PRUint32)-1, (PRUint32)-1);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleCharacterData(aSourceText, nsCRT::strlen(aSourceText));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleEndElement(NS_LITERAL_STRING("sourcetext").get());
  NS_ENSURE_SUCCESS(rv,rv); 
  
  rv = HandleEndElement(NS_LITERAL_STRING("parsererror").get());
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsXMLContentSink::PushNameSpacesFrom(const PRUnichar** aAtts)
{
  nsCOMPtr<nsINameSpace> nameSpace;
  nsresult rv = NS_OK;

  if (mNameSpaceStack && (0 < mNameSpaceStack->Count())) {
    nameSpace =
      (nsINameSpace*)mNameSpaceStack->ElementAt(mNameSpaceStack->Count() - 1);
  } else {
    nsCOMPtr<nsINameSpaceManager> manager;
    mDocument->GetNameSpaceManager(*getter_AddRefs(manager));

    NS_ASSERTION(manager, "no name space manager in document");

    if (manager) {
      rv = manager->CreateRootNameSpace(*getter_AddRefs(nameSpace));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  NS_ENSURE_TRUE(nameSpace, NS_ERROR_UNEXPECTED);

  static const NS_NAMED_LITERAL_STRING(kNameSpaceDef, "xmlns");
  static const PRUint32 xmlns_len = kNameSpaceDef.Length();

  
  while (*aAtts) {
    const nsDependentString key(aAtts[0]);

    // Look for "xmlns" at the start of the attribute name

    PRUint32 key_len = key.Length();

    if (key_len >= xmlns_len &&
        nsDependentSubstring(key, 0, xmlns_len).Equals(kNameSpaceDef)) {
      nsCOMPtr<nsIAtom> prefixAtom;

      // If key_len > xmlns_len we have a xmlns:foo type attribute,
      // extract the prefix. If not, we have a xmlns attribute in
      // which case there is no prefix.

      if (key_len > xmlns_len) {
        nsReadingIterator<PRUnichar> start, end;

        key.BeginReading(start);
        key.EndReading(end);

        start.advance(xmlns_len);

        if (*start == ':') {
          ++start;

          prefixAtom =
            dont_AddRef(NS_NewAtom(nsDependentSubstring(start, end)));
        }
      }

      nsCOMPtr<nsINameSpace> child;
      rv = nameSpace->CreateChildNameSpace(prefixAtom, nsDependentString(aAtts[1]),
                                           *getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);

      nameSpace = child;
    }
    aAtts += 2;
  }

  if (!mNameSpaceStack) {
    mNameSpaceStack = new nsAutoVoidArray();

    if (!mNameSpaceStack) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  nsINameSpace *tmp = nameSpace;
  mNameSpaceStack->AppendElement(tmp);
  NS_ADDREF(tmp);

  return NS_OK;
}

nsresult
nsXMLContentSink::AddAttributes(const PRUnichar** aAtts,
                                nsIContent* aContent,
                                PRBool aIsHTML)
{
  // Add tag attributes to the content attributes
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;

  while (*aAtts) {
    // Get upper-cased key
    const nsDependentString key(aAtts[0]);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    PRInt32 nameSpaceID;

    if (nameSpacePrefix) {
        nameSpaceID = GetNameSpaceId(nameSpacePrefix);
    } else {
      if (nameAtom.get() == nsLayoutAtoms::xmlnsNameSpace)
        nameSpaceID = kNameSpaceID_XMLNS;
      else
        nameSpaceID = kNameSpaceID_None;
    }

    if (kNameSpaceID_Unknown == nameSpaceID) {
      nameSpaceID = kNameSpaceID_None;
      nameAtom = dont_AddRef(NS_NewAtom(key));
      nameSpacePrefix = nsnull;
    }

    nsCOMPtr<nsINodeInfo> ni;
    mNodeInfoManager->GetNodeInfo(nameAtom, nameSpacePrefix, nameSpaceID,
                                  *getter_AddRefs(ni));
    NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

    // Add attribute to content
    aContent->SetAttr(ni, nsDependentString(aAtts[1]), PR_FALSE);
    aAtts += 2;
  }

  // Give autoloading links a chance to fire
  if (mWebShell) {
    nsCOMPtr<nsIXMLContent> xmlcontent(do_QueryInterface(aContent));
    if (xmlcontent) {
      nsresult rv = xmlcontent->MaybeTriggerAutoLink(mWebShell);
      if (rv == NS_XML_AUTOLINK_REPLACE ||
          rv == NS_XML_AUTOLINK_UNDEFINED) {
        // If we do not terminate the parse, we just keep generating link trigger
        // events. We want to parse only up to the first replace link, and stop.
        mParser->Terminate();
      }
    }
  }

  return NS_OK;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

nsresult
nsXMLContentSink::AddText(const PRUnichar* aText, 
                          PRInt32 aLength)
{

  if (mInTitle) {
    mTitleText.Append(aText,aLength);
  }

  // Create buffer when we first need it
  if (0 == mTextSize) {
    mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * NS_ACCUMULATION_BUFFER_SIZE);
    if (nsnull == mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = NS_ACCUMULATION_BUFFER_SIZE;
  }

  const nsAString& str = Substring(aText, aText+aLength);

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  PRBool  isLastCharCR = PR_FALSE;
  while (0 != aLength) {
    PRInt32 amount = mTextSize - mTextLength;
    if (amount > aLength) {
      amount = aLength;
    }
    if (0 == amount) {
      if (mConstrainSize) {
        nsresult rv = FlushText();
        if (NS_OK != rv) {
          return rv;
        }
      }
      else {
        mTextSize += aLength;
        mText = (PRUnichar *) PR_REALLOC(mText, sizeof(PRUnichar) * mTextSize);
        if (nsnull == mText) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    mTextLength +=
      nsContentUtils::CopyNewlineNormalizedUnicodeTo(str, 
                                                     offset, 
                                                     &mText[mTextLength], 
                                                     amount,
                                                     isLastCharCR);
    offset  += amount;
    aLength -= amount;
  }

  return NS_OK;
}

nsresult
nsXMLContentSink::ProcessEndSCRIPTTag()
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIContent> element(dont_AddRef(GetCurrentContent()));
  nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement(do_QueryInterface(element));
  NS_ASSERTION(scriptElement, "null script element in XML content sink");
  mScriptElements.AppendElement(scriptElement);

  nsCOMPtr<nsIScriptElement> sele(do_QueryInterface(element));
  if (sele) {
    sele->SetLineNumber(mScriptLineNo);
  }

  mConstrainSize = PR_TRUE; 
  // Assume that we're going to block the parser with a script load.
  // If it's an inline script, we'll be told otherwise in the call
  // to our ScriptAvailable method.
  mNeedToBlockParser = PR_TRUE;

  return result;
}

nsresult
nsXMLContentSink::ProcessStartSCRIPTTag(PRUint32 aLineNo)
{
  // Wait until we get the script content
  mConstrainSize = PR_FALSE;
  mScriptLineNo = aLineNo;

  return NS_OK;
}
