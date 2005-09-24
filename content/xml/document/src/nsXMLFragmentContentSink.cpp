/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Steve Swanson  steve.swanson@mackichan.com.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Kaplan <mrbkap@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsCOMPtr.h"
#include "nsXMLContentSink.h"
#include "nsIFragmentContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsContentSink.h"
#include "nsIExpatSink.h"
#include "nsIParser.h"
#include "nsIDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIXMLContent.h"
#include "nsHTMLAtoms.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsDOMError.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"

class nsXMLFragmentContentSink : public nsXMLContentSink,
                                 public nsIFragmentContentSink
{
public:
  nsXMLFragmentContentSink(PRBool aAllContent = PR_FALSE);
  virtual ~nsXMLFragmentContentSink();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIExpatSink
  NS_IMETHOD HandleDoctypeDecl(const nsAString & aSubset, 
                               const nsAString & aName, 
                               const nsAString & aSystemId, 
                               const nsAString & aPublicId,
                               nsISupports* aCatalogData);
  NS_IMETHOD HandleProcessingInstruction(const PRUnichar *aTarget, 
                                         const PRUnichar *aData);
  NS_IMETHOD HandleXMLDeclaration(const PRUnichar *aVersion,
                                  const PRUnichar *aEncoding,
                                  PRInt32 aStandalone);
  NS_IMETHOD ReportError(const PRUnichar* aErrorText, 
                         const PRUnichar* aSourceText);

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel();
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();

  // nsIXMLContentSink

  // nsIFragmentContentSink
  NS_IMETHOD GetFragment(nsIDOMDocumentFragment** aFragment);
  NS_IMETHOD SetTargetDocument(nsIDocument* aDocument);
  NS_IMETHOD WillBuildContent();
  NS_IMETHOD DidBuildContent();
  NS_IMETHOD IgnoreFirstContainer();

protected:
  virtual PRBool SetDocElement(PRInt32 aNameSpaceID, 
                               nsIAtom *aTagName,
                               nsIContent *aContent);
  virtual nsresult CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount,
                                 nsINodeInfo* aNodeInfo, PRUint32 aLineNumber,
                                 nsIContent** aResult, PRBool* aAppendContent);
  virtual nsresult CloseElement(nsIContent* aContent, nsIContent* aParent,
                                PRBool* aAppendContent);

  // nsContentSink overrides
  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    PRBool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia);
  nsresult LoadXSLStyleSheet(nsIURI* aUrl);
  void StartLayout();

  nsCOMPtr<nsIDocument> mTargetDocument;
  // the fragment
  nsCOMPtr<nsIContent>  mRoot;
  PRPackedBool          mParseError;

  // if FALSE, take content inside endnote tag
  PRPackedBool          mAllContent;
};

static nsresult
NewXMLFragmentContentSinkHelper(PRBool aAllContent, nsIFragmentContentSink** aResult)
{
  nsXMLFragmentContentSink* it = new nsXMLFragmentContentSink(aAllContent);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(*aResult = it);
  
  return NS_OK;
}

nsresult
NS_NewXMLFragmentContentSink2(nsIFragmentContentSink** aResult)
{
  return NewXMLFragmentContentSinkHelper(PR_TRUE, aResult);
}

nsresult
NS_NewXMLFragmentContentSink(nsIFragmentContentSink** aResult)
{
  return NewXMLFragmentContentSinkHelper(PR_FALSE, aResult);
}

nsXMLFragmentContentSink::nsXMLFragmentContentSink(PRBool aAllContent)
 : mParseError(PR_FALSE), mAllContent(aAllContent)
{
}

nsXMLFragmentContentSink::~nsXMLFragmentContentSink()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXMLFragmentContentSink,
                             nsXMLContentSink,
                             nsIFragmentContentSink)

NS_IMETHODIMP 
nsXMLFragmentContentSink::WillBuildModel(void)
{
  if (mRoot) {
    return NS_OK;
  }

  mState = eXMLContentSinkState_InDocumentElement;

  NS_ASSERTION(mTargetDocument, "Need a document!");

  nsCOMPtr<nsIDOMDocumentFragment> frag;
  nsresult rv = NS_NewDocumentFragment(getter_AddRefs(frag), mNodeInfoManager);
  NS_ENSURE_SUCCESS(rv, rv);

  mRoot = do_QueryInterface(frag);
  
  if (mAllContent) {
    // Preload content stack because we know all content goes in the fragment
    PushContent(mRoot);
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLFragmentContentSink::DidBuildModel()
{
  if (mAllContent) {
    // Need the nsCOMPtr to properly release
    nsCOMPtr<nsIContent> root = PopContent();  // remove mRoot pushed above
  }

  nsCOMPtr<nsIParser> kungFuDeathGrip(mParser);

  // Drop our reference to the parser to get rid of a circular
  // reference.
  mParser = nsnull;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLFragmentContentSink::SetDocumentCharset(nsACString& aCharset)
{
  NS_NOTREACHED("fragments shouldn't set charset");
  return NS_OK;
}

nsISupports *
nsXMLFragmentContentSink::GetTarget()
{
  return mTargetDocument;
}

////////////////////////////////////////////////////////////////////////

PRBool
nsXMLFragmentContentSink::SetDocElement(PRInt32 aNameSpaceID,
                                        nsIAtom* aTagName,
                                        nsIContent *aContent)
{
  // this is a fragment, not a document
  return PR_FALSE;
}

nsresult
nsXMLFragmentContentSink::CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount,
                                        nsINodeInfo* aNodeInfo, PRUint32 aLineNumber,
                                        nsIContent** aResult, PRBool* aAppendContent)
{
  nsresult rv = nsXMLContentSink::CreateElement(aAtts, aAttsCount,
                                aNodeInfo, aLineNumber,
                                aResult, aAppendContent);

  // Make sure that scripts are added immediately, not on close.
  *aAppendContent = PR_TRUE;

  // However, when we aren't grabbing all of the content we, never open a doc
  // element, we run into trouble on the first element, so we don't append,
  // and simply push this onto the content stack.
  if (!mAllContent && mContentStack.Count() == 0) {
    *aAppendContent = PR_FALSE;
  }

  return rv;
}

nsresult
nsXMLFragmentContentSink::CloseElement(nsIContent* aContent,
                                       nsIContent* aParent,
                                       PRBool* aAppendContent)
{
  // don't do fancy stuff in nsXMLContentSink
  *aAppendContent = PR_FALSE;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXMLFragmentContentSink::HandleDoctypeDecl(const nsAString & aSubset, 
                                            const nsAString & aName, 
                                            const nsAString & aSystemId, 
                                            const nsAString & aPublicId,
                                            nsISupports* aCatalogData)
{
  NS_NOTREACHED("fragments shouldn't have doctype declarations");
  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                                      const PRUnichar *aData)
{
  FlushText();

  nsresult result = NS_OK;
  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  nsCOMPtr<nsIContent> node;

  result = NS_NewXMLProcessingInstruction(getter_AddRefs(node),
                                          mNodeInfoManager, target, data);
  if (NS_SUCCEEDED(result)) {
    // no special processing here.  that should happen when the fragment moves into the document
    result = AddContentAsLeaf(node);
  }
  return result;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::HandleXMLDeclaration(const PRUnichar *aVersion,
                                               const PRUnichar *aEncoding,
                                               PRInt32 aStandalone)
{
  NS_NOTREACHED("fragments shouldn't have XML declarations");
  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::ReportError(const PRUnichar* aErrorText, 
                                      const PRUnichar* aSourceText)
{
  mParseError = PR_TRUE;
  // The following error reporting is copied from nsXBLContentSink::ReportError()

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (consoleService) {
    consoleService->LogStringMessage(aErrorText);
  }

#ifdef DEBUG
  // Report the error to stderr.
  fprintf(stderr,
          "\n%s\n%s\n\n",
          NS_LossyConvertUCS2toASCII(aErrorText).get(),
          NS_LossyConvertUCS2toASCII(aSourceText).get());
#endif

  // The following code is similar to the cleanup in nsXMLContentSink::ReportError()
  mState = eXMLContentSinkState_InProlog;

  // Clear the current content
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mRoot));
  if (node) {
    for (;;) {
      nsCOMPtr<nsIDOMNode> child, dummy;
      node->GetLastChild(getter_AddRefs(child));
      if (!child)
        break;
      node->RemoveChild(child, getter_AddRefs(dummy));
    }
  }

  // Clear any buffered-up text we have.  It's enough to set the length to 0.
  // The buffer itself is allocated when we're created and deleted in our
  // destructor, so don't mess with it.
  mTextLength = 0;

  return NS_OK; 
}

nsresult
nsXMLFragmentContentSink::ProcessStyleLink(nsIContent* aElement,
                                           const nsSubstring& aHref,
                                           PRBool aAlternate,
                                           const nsSubstring& aTitle,
                                           const nsSubstring& aType,
                                           const nsSubstring& aMedia)
{
  // don't process until moved to document
  return NS_OK;
}

nsresult
nsXMLFragmentContentSink::LoadXSLStyleSheet(nsIURI* aUrl)
{
  NS_NOTREACHED("fragments shouldn't have XSL style sheets");
  return NS_ERROR_UNEXPECTED;
}

void
nsXMLFragmentContentSink::StartLayout()
{
  NS_NOTREACHED("fragments shouldn't layout");
}

////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsXMLFragmentContentSink::GetFragment(nsIDOMDocumentFragment** aFragment)
{
  *aFragment = nsnull;
  if (mParseError) {
    //XXX PARSE_ERR from DOM3 Load and Save would be more appropriate
    return NS_ERROR_DOM_SYNTAX_ERR;
  } else if (mRoot) {
    return CallQueryInterface(mRoot, aFragment);
  } else {
    return NS_OK;
  }
}

NS_IMETHODIMP
nsXMLFragmentContentSink::SetTargetDocument(nsIDocument* aTargetDocument)
{
  NS_ENSURE_ARG_POINTER(aTargetDocument);

  mTargetDocument = aTargetDocument;
  mNodeInfoManager = aTargetDocument->NodeInfoManager();

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::WillBuildContent()
{
  // If we're taking all of the content, then we've already pushed mRoot
  // onto the content stack, otherwise, start here.
  if (!mAllContent) {
    PushContent(mRoot);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::DidBuildContent()
{
  // If we're taking all of the content, then this is handled in DidBuildModel
  if (!mAllContent) {
    // Note: we need to FlushText() here because if we don't, we might not get
    // an end element to do it for us, so make sure.
    if (!mParseError) {
      FlushText();
    }
    // Need the nsCOMPtr to properly release
    nsCOMPtr<nsIContent> root = PopContent();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::IgnoreFirstContainer()
{
  NS_NOTREACHED("XML isn't as broken as HTML");
  return NS_ERROR_FAILURE;
}

