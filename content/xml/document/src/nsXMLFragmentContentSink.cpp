/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsXMLContentSink.h"
#include "nsIFragmentContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsContentSink.h"
#include "nsIExpatSink.h"
#include "nsIDTD.h"
#include "nsIDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsNullPrincipal.h"
#include "nsContentCreatorFunctions.h"
#include "nsDOMError.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"
#include "nsScriptLoader.h"
#include "mozilla/css/Loader.h"

using namespace mozilla::dom;

class nsXMLFragmentContentSink : public nsXMLContentSink,
                                 public nsIFragmentContentSink
{
public:
  nsXMLFragmentContentSink();
  virtual ~nsXMLFragmentContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsXMLFragmentContentSink,
                                                     nsXMLContentSink)

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
                         const PRUnichar* aSourceText,
                         nsIScriptError *aError,
                         bool *_retval);

  // nsIContentSink
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);
  NS_IMETHOD DidBuildModel(bool aTerminated);
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();
  NS_IMETHOD DidProcessATokenImpl();

  // nsIXMLContentSink

  // nsIFragmentContentSink
  NS_IMETHOD FinishFragmentParsing(nsIDOMDocumentFragment** aFragment);
  NS_IMETHOD SetTargetDocument(nsIDocument* aDocument);
  NS_IMETHOD WillBuildContent();
  NS_IMETHOD DidBuildContent();
  NS_IMETHOD IgnoreFirstContainer();
  NS_IMETHOD SetPreventScriptExecution(bool aPreventScriptExecution);

protected:
  virtual bool SetDocElement(PRInt32 aNameSpaceID, 
                               nsIAtom *aTagName,
                               nsIContent *aContent);
  virtual nsresult CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount,
                                 nsINodeInfo* aNodeInfo, PRUint32 aLineNumber,
                                 nsIContent** aResult, bool* aAppendContent,
                                 mozilla::dom::FromParser aFromParser);
  virtual nsresult CloseElement(nsIContent* aContent);

  virtual void MaybeStartLayout(bool aIgnorePendingSheets);

  // nsContentSink overrides
  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    bool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia);
  nsresult LoadXSLStyleSheet(nsIURI* aUrl);
  void StartLayout();

  nsCOMPtr<nsIDocument> mTargetDocument;
  // the fragment
  nsCOMPtr<nsIContent>  mRoot;
  bool                  mParseError;
};

static nsresult
NewXMLFragmentContentSinkHelper(nsIFragmentContentSink** aResult)
{
  nsXMLFragmentContentSink* it = new nsXMLFragmentContentSink();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(*aResult = it);
  
  return NS_OK;
}

nsresult
NS_NewXMLFragmentContentSink(nsIFragmentContentSink** aResult)
{
  return NewXMLFragmentContentSinkHelper(aResult);
}

nsXMLFragmentContentSink::nsXMLFragmentContentSink()
 : mParseError(false)
{
  mRunsToCompletion = true;
}

nsXMLFragmentContentSink::~nsXMLFragmentContentSink()
{
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXMLFragmentContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIFragmentContentSink)
NS_INTERFACE_MAP_END_INHERITING(nsXMLContentSink)

NS_IMPL_ADDREF_INHERITED(nsXMLFragmentContentSink, nsXMLContentSink)
NS_IMPL_RELEASE_INHERITED(nsXMLFragmentContentSink, nsXMLContentSink)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLFragmentContentSink)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLFragmentContentSink,
                                                  nsXMLContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTargetDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRoot)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP 
nsXMLFragmentContentSink::WillBuildModel(nsDTDMode aDTDMode)
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
  
  return rv;
}

NS_IMETHODIMP 
nsXMLFragmentContentSink::DidBuildModel(bool aTerminated)
{
  nsRefPtr<nsParserBase> kungFuDeathGrip(mParser);

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

bool
nsXMLFragmentContentSink::SetDocElement(PRInt32 aNameSpaceID,
                                        nsIAtom* aTagName,
                                        nsIContent *aContent)
{
  // this is a fragment, not a document
  return false;
}

nsresult
nsXMLFragmentContentSink::CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount,
                                        nsINodeInfo* aNodeInfo, PRUint32 aLineNumber,
                                        nsIContent** aResult, bool* aAppendContent,
                                        FromParser /*aFromParser*/)
{
  // Claim to not be coming from parser, since we don't do any of the
  // fancy CloseElement stuff.
  nsresult rv = nsXMLContentSink::CreateElement(aAtts, aAttsCount,
                                                aNodeInfo, aLineNumber,
                                                aResult, aAppendContent,
                                                NOT_FROM_PARSER);

  // When we aren't grabbing all of the content we, never open a doc
  // element, we run into trouble on the first element, so we don't append,
  // and simply push this onto the content stack.
  if (mContentStack.Length() == 0) {
    *aAppendContent = false;
  }

  return rv;
}

nsresult
nsXMLFragmentContentSink::CloseElement(nsIContent* aContent)
{
  // don't do fancy stuff in nsXMLContentSink
  if (mPreventScriptExecution && aContent->Tag() == nsGkAtoms::script &&
      (aContent->IsHTML() || aContent->IsSVG())) {
    nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(aContent);
    NS_ASSERTION(sele, "script did QI correctly!");
    sele->PreventExecution();
  }
  return NS_OK;
}

void
nsXMLFragmentContentSink::MaybeStartLayout(bool aIgnorePendingSheets)
{
  return;
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
                                      const PRUnichar* aSourceText,
                                      nsIScriptError *aError,
                                      bool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

  // The expat driver should report the error.
  *_retval = true;

  mParseError = true;

#ifdef DEBUG
  // Report the error to stderr.
  fprintf(stderr,
          "\n%s\n%s\n\n",
          NS_LossyConvertUTF16toASCII(aErrorText).get(),
          NS_LossyConvertUTF16toASCII(aSourceText).get());
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
                                           bool aAlternate,
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
nsXMLFragmentContentSink::FinishFragmentParsing(nsIDOMDocumentFragment** aFragment)
{
  *aFragment = nsnull;
  mTargetDocument = nsnull;
  mNodeInfoManager = nsnull;
  mScriptLoader = nsnull;
  mCSSLoader = nsnull;
  mContentStack.Clear();
  mDocumentURI = nsnull;
  mDocShell = nsnull;
  if (mParseError) {
    //XXX PARSE_ERR from DOM3 Load and Save would be more appropriate
    mRoot = nsnull;
    mParseError = false;
    return NS_ERROR_DOM_SYNTAX_ERR;
  } else if (mRoot) {
    nsresult rv = CallQueryInterface(mRoot, aFragment);
    mRoot = nsnull;
    return rv;
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
  PushContent(mRoot);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::DidBuildContent()
{
  // Note: we need to FlushText() here because if we don't, we might not get
  // an end element to do it for us, so make sure.
  if (!mParseError) {
    FlushText();
  }
  PopContent();

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::DidProcessATokenImpl()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::IgnoreFirstContainer()
{
  NS_NOTREACHED("XML isn't as broken as HTML");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::SetPreventScriptExecution(bool aPrevent)
{
  mPreventScriptExecution = aPrevent;
  return NS_OK;
}
