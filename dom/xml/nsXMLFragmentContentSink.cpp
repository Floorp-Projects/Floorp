/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsContentCreatorFunctions.h"
#include "nsError.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/ScriptLoader.h"

using namespace mozilla::dom;

class nsXMLFragmentContentSink : public nsXMLContentSink,
                                 public nsIFragmentContentSink
{
public:
  nsXMLFragmentContentSink();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsXMLFragmentContentSink,
                                                     nsXMLContentSink)

  // nsIExpatSink
  NS_IMETHOD HandleDoctypeDecl(const nsAString& aSubset,
                               const nsAString& aName,
                               const nsAString& aSystemId,
                               const nsAString& aPublicId,
                               nsISupports* aCatalogData) override;
  NS_IMETHOD HandleProcessingInstruction(const char16_t* aTarget,
                                         const char16_t* aData) override;
  NS_IMETHOD HandleXMLDeclaration(const char16_t* aVersion,
                                  const char16_t* aEncoding,
                                  int32_t aStandalone) override;
  NS_IMETHOD ReportError(const char16_t* aErrorText,
                         const char16_t* aSourceText,
                         nsIScriptError* aError,
                         bool* aRetval) override;

  // nsIContentSink
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override;
  NS_IMETHOD DidBuildModel(bool aTerminated) override;
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding) override;
  virtual nsISupports* GetTarget() override;
  NS_IMETHOD DidProcessATokenImpl();

  // nsIXMLContentSink

  // nsIFragmentContentSink
  NS_IMETHOD FinishFragmentParsing(DocumentFragment** aFragment) override;
  NS_IMETHOD SetTargetDocument(nsIDocument* aDocument) override;
  NS_IMETHOD WillBuildContent() override;
  NS_IMETHOD DidBuildContent() override;
  NS_IMETHOD IgnoreFirstContainer() override;
  NS_IMETHOD SetPreventScriptExecution(bool aPreventScriptExecution) override;

protected:
  virtual ~nsXMLFragmentContentSink();

  virtual bool SetDocElement(int32_t aNameSpaceID,
                               nsAtom* aTagName,
                               nsIContent* aContent) override;
  virtual nsresult CreateElement(const char16_t** aAtts, uint32_t aAttsCount,
                                 mozilla::dom::NodeInfo* aNodeInfo, uint32_t aLineNumber,
                                 nsIContent** aResult, bool* aAppendContent,
                                 mozilla::dom::FromParser aFromParser) override;
  virtual nsresult CloseElement(nsIContent* aContent) override;

  virtual void MaybeStartLayout(bool aIgnorePendingSheets) override;

  // nsContentSink overrides
  virtual nsresult ProcessStyleLinkFromHeader(
    const nsAString& aHref,
    bool aAlternate,
    const nsAString& aTitle,
    const nsAString& aType,
    const nsAString& aMedia,
    const nsAString& aReferrerPolicy) override;

  // nsXMLContentSink overrides
  virtual nsresult MaybeProcessXSLTLink(
    ProcessingInstruction* aProcessingInstruction,
    const nsAString& aHref,
    bool aAlternate,
    const nsAString& aTitle,
    const nsAString& aType,
    const nsAString& aMedia,
    const nsAString& aReferrerPolicy,
    bool* aWasXSLT = nullptr) override;

  nsCOMPtr<nsIDocument> mTargetDocument;
  // the fragment
  RefPtr<DocumentFragment> mRoot;
  bool                  mParseError;
};

static nsresult
NewXMLFragmentContentSinkHelper(nsIFragmentContentSink** aResult)
{
  nsXMLFragmentContentSink* it = new nsXMLFragmentContentSink();

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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXMLFragmentContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIFragmentContentSink)
NS_INTERFACE_MAP_END_INHERITING(nsXMLContentSink)

NS_IMPL_ADDREF_INHERITED(nsXMLFragmentContentSink, nsXMLContentSink)
NS_IMPL_RELEASE_INHERITED(nsXMLFragmentContentSink, nsXMLContentSink)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLFragmentContentSink)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLFragmentContentSink,
                                                  nsXMLContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTargetDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsXMLFragmentContentSink::WillBuildModel(nsDTDMode aDTDMode)
{
  if (mRoot) {
    return NS_OK;
  }

  mState = eXMLContentSinkState_InDocumentElement;

  NS_ASSERTION(mTargetDocument, "Need a document!");

  mRoot = new DocumentFragment(mNodeInfoManager);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::DidBuildModel(bool aTerminated)
{
  // Drop our reference to the parser to get rid of a circular
  // reference.
  mParser = nullptr;

  return NS_OK;
}

void
nsXMLFragmentContentSink::SetDocumentCharset(
  NotNull<const Encoding*> aEncoding)
{
  MOZ_ASSERT_UNREACHABLE("fragments shouldn't set charset");
}

nsISupports *
nsXMLFragmentContentSink::GetTarget()
{
  return mTargetDocument;
}

////////////////////////////////////////////////////////////////////////

bool
nsXMLFragmentContentSink::SetDocElement(int32_t aNameSpaceID,
                                        nsAtom* aTagName,
                                        nsIContent *aContent)
{
  // this is a fragment, not a document
  return false;
}

nsresult
nsXMLFragmentContentSink::CreateElement(const char16_t** aAtts, uint32_t aAttsCount,
                                        mozilla::dom::NodeInfo* aNodeInfo, uint32_t aLineNumber,
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
  if (mPreventScriptExecution &&
      (aContent->IsHTMLElement(nsGkAtoms::script) ||
       aContent->IsSVGElement(nsGkAtoms::script))) {
    nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(aContent);
    if (sele) {
      sele->PreventExecution();
    } else {
      NS_ASSERTION(nsNameSpaceManager::GetInstance()->mSVGDisabled, "Script did QI correctly, but wasn't a disabled SVG!");
    }
  }
  return NS_OK;
}

void
nsXMLFragmentContentSink::MaybeStartLayout(bool aIgnorePendingSheets)
{
}

////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXMLFragmentContentSink::HandleDoctypeDecl(const nsAString & aSubset,
                                            const nsAString & aName,
                                            const nsAString & aSystemId,
                                            const nsAString & aPublicId,
                                            nsISupports* aCatalogData)
{
  MOZ_ASSERT_UNREACHABLE("fragments shouldn't have doctype declarations");

  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::HandleProcessingInstruction(const char16_t *aTarget,
                                                      const char16_t *aData)
{
  FlushText();

  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  RefPtr<ProcessingInstruction> node =
    NS_NewXMLProcessingInstruction(mNodeInfoManager, target, data);

  // no special processing here.  that should happen when the fragment moves into the document
  return AddContentAsLeaf(node);
}

NS_IMETHODIMP
nsXMLFragmentContentSink::HandleXMLDeclaration(const char16_t *aVersion,
                                               const char16_t *aEncoding,
                                               int32_t aStandalone)
{
  MOZ_ASSERT_UNREACHABLE("fragments shouldn't have XML declarations");
  return NS_OK;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::ReportError(const char16_t* aErrorText,
                                      const char16_t* aSourceText,
                                      nsIScriptError *aError,
                                      bool *_retval)
{
  MOZ_ASSERT(aError && aSourceText && aErrorText, "Check arguments!!!");

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
  while (mRoot->GetLastChild()) {
    mRoot->GetLastChild()->Remove();
  }

  // Clear any buffered-up text we have.  It's enough to set the length to 0.
  // The buffer itself is allocated when we're created and deleted in our
  // destructor, so don't mess with it.
  mTextLength = 0;

  return NS_OK;
}

nsresult
nsXMLFragmentContentSink::ProcessStyleLinkFromHeader(
  const nsAString& aHref,
  bool aAlternate,
  const nsAString& aTitle,
  const nsAString& aType,
  const nsAString& aMedia,
  const nsAString& aReferrerPolicy)

{
  MOZ_ASSERT_UNREACHABLE("Shouldn't have headers for a fragment sink");
  return NS_OK;
}

nsresult
nsXMLFragmentContentSink::MaybeProcessXSLTLink(
  ProcessingInstruction* aProcessingInstruction,
  const nsAString& aHref,
  bool aAlternate,
  const nsAString& aTitle,
  const nsAString& aType,
  const nsAString& aMedia,
  const nsAString& aReferrerPolicy,
  bool* aWasXSLT)
{
  MOZ_ASSERT(!aWasXSLT, "Our one caller doesn't care about whether we're XSLT");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXMLFragmentContentSink::FinishFragmentParsing(DocumentFragment** aFragment)
{
  mTargetDocument = nullptr;
  mNodeInfoManager = nullptr;
  mScriptLoader = nullptr;
  mCSSLoader = nullptr;
  mContentStack.Clear();
  mDocumentURI = nullptr;
  mDocShell = nullptr;
  mDocElement = nullptr;
  mCurrentHead = nullptr;
  if (mParseError) {
    //XXX PARSE_ERR from DOM3 Load and Save would be more appropriate
    mRoot = nullptr;
    mParseError = false;
    *aFragment = nullptr;
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  mRoot.forget(aFragment);
  return NS_OK;
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
  MOZ_ASSERT_UNREACHABLE("XML isn't as broken as HTML");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXMLFragmentContentSink::SetPreventScriptExecution(bool aPrevent)
{
  mPreventScriptExecution = aPrevent;
  return NS_OK;
}
