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
 *   Steve Swanson <steve.swanson@mackichan.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Kaplan <mrbkap@gmail.com>
 *   Robert Sayre <sayrer@gmail.com>
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
#include "nsIDTD.h"
#include "nsIParser.h"
#include "nsIDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
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

using namespace mozilla::dom;

class nsXMLFragmentContentSink : public nsXMLContentSink,
                                 public nsIFragmentContentSink
{
public:
  nsXMLFragmentContentSink(PRBool aAllContent = PR_FALSE);
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
                         PRBool *_retval);

  // nsIContentSink
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);
  NS_IMETHOD DidBuildModel(PRBool aTerminated);
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();
  NS_IMETHOD DidProcessATokenImpl();

  // nsIXMLContentSink

  // nsIFragmentContentSink
  NS_IMETHOD GetFragment(PRBool aWillOwnFragment,
                         nsIDOMDocumentFragment** aFragment);
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
                                 nsIContent** aResult, PRBool* aAppendContent,
                                 mozilla::dom::FromParser aFromParser);
  virtual nsresult CloseElement(nsIContent* aContent);

  virtual void MaybeStartLayout(PRBool aIgnorePendingSheets);

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
  
  if (mAllContent) {
    // Preload content stack because we know all content goes in the fragment
    PushContent(mRoot);
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLFragmentContentSink::DidBuildModel(PRBool aTerminated)
{
  if (mAllContent) {
    PopContent();  // remove mRoot pushed above
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
                                        nsIContent** aResult, PRBool* aAppendContent,
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
  if (!mAllContent && mContentStack.Length() == 0) {
    *aAppendContent = PR_FALSE;
  }

  return rv;
}

nsresult
nsXMLFragmentContentSink::CloseElement(nsIContent* aContent)
{
  // don't do fancy stuff in nsXMLContentSink
  return NS_OK;
}

void
nsXMLFragmentContentSink::MaybeStartLayout(PRBool aIgnorePendingSheets)
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
                                      PRBool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

  // The expat driver should report the error.
  *_retval = PR_TRUE;

  mParseError = PR_TRUE;

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
nsXMLFragmentContentSink::GetFragment(PRBool aWillOwnFragment,
                                      nsIDOMDocumentFragment** aFragment)
{
  *aFragment = nsnull;
  if (mParseError) {
    //XXX PARSE_ERR from DOM3 Load and Save would be more appropriate
    return NS_ERROR_DOM_SYNTAX_ERR;
  } else if (mRoot) {
    nsresult rv = CallQueryInterface(mRoot, aFragment);
    if (NS_SUCCEEDED(rv) && aWillOwnFragment) {
      mRoot = nsnull;
    }
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
    PopContent();
  }

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


// nsXHTMLParanoidFragmentSink

// Find the whitelist of allowed elements and attributes in
// nsContentSink.h We share it with nsHTMLParanoidFragmentSink

class nsXHTMLParanoidFragmentSink : public nsXMLFragmentContentSink
{
public:
  nsXHTMLParanoidFragmentSink(PRBool aAllContent = PR_FALSE);

  static nsresult Init();
  static void Cleanup();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  
  // nsXMLContentSink
  nsresult AddAttributes(const PRUnichar** aNode, nsIContent* aContent);

  // nsIExpatSink
  NS_IMETHOD HandleStartElement(const PRUnichar *aName,
                                const PRUnichar **aAtts,
                                PRUint32 aAttsCount, PRInt32 aIndex,
                                PRUint32 aLineNumber);
    
  NS_IMETHOD HandleEndElement(const PRUnichar *aName);

  NS_IMETHOD HandleComment(const PRUnichar *aName);

  NS_IMETHOD HandleProcessingInstruction(const PRUnichar *aTarget, 
                                         const PRUnichar *aData);

  NS_IMETHOD HandleCDataSection(const PRUnichar *aData, 
                                PRUint32 aLength);

  NS_IMETHOD HandleCharacterData(const PRUnichar *aData,
                                 PRUint32 aLength);
protected:
  PRUint32 mSkipLevel; // used when we descend into <style> or <script>
  // Use nsTHashTable as a hash set for our whitelists
  static nsTHashtable<nsISupportsHashKey>* sAllowedTags;
  static nsTHashtable<nsISupportsHashKey>* sAllowedAttributes;
};

nsTHashtable<nsISupportsHashKey>* nsXHTMLParanoidFragmentSink::sAllowedTags;
nsTHashtable<nsISupportsHashKey>* nsXHTMLParanoidFragmentSink::sAllowedAttributes;

nsXHTMLParanoidFragmentSink::nsXHTMLParanoidFragmentSink(PRBool aAllContent):
  nsXMLFragmentContentSink(aAllContent), mSkipLevel(0)
{
}

nsresult
nsXHTMLParanoidFragmentSink::Init()
{
  nsresult rv = NS_ERROR_FAILURE;
  
  if (sAllowedTags) {
    return NS_OK;
  }

  sAllowedTags = new nsTHashtable<nsISupportsHashKey>();
  if (sAllowedTags) {
    rv = sAllowedTags->Init(80);
    for (PRUint32 i = 0; kDefaultAllowedTags[i] && NS_SUCCEEDED(rv); i++) {
      if (!sAllowedTags->PutEntry(*kDefaultAllowedTags[i])) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  sAllowedAttributes = new nsTHashtable<nsISupportsHashKey>();
  if (sAllowedAttributes && NS_SUCCEEDED(rv)) {
    rv = sAllowedAttributes->Init(80);
    for (PRUint32 i = 0;
         kDefaultAllowedAttributes[i] && NS_SUCCEEDED(rv); i++) {
      if (!sAllowedAttributes->PutEntry(*kDefaultAllowedAttributes[i])) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to populate whitelist hash sets");
    Cleanup();
  }

  return rv;
}

void
nsXHTMLParanoidFragmentSink::Cleanup()
{
  if (sAllowedTags) {
    delete sAllowedTags;
    sAllowedTags = nsnull;
  }
  
  if (sAllowedAttributes) {
    delete sAllowedAttributes;
    sAllowedAttributes = nsnull;
  }
}

nsresult
NS_NewXHTMLParanoidFragmentSink(nsIFragmentContentSink** aResult)
{
  nsXHTMLParanoidFragmentSink* it = new nsXHTMLParanoidFragmentSink();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = nsXHTMLParanoidFragmentSink::Init();
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ADDREF(*aResult = it);
  
  return NS_OK;
}

nsresult
NS_NewXHTMLParanoidFragmentSink2(nsIFragmentContentSink** aResult)
{
  nsXHTMLParanoidFragmentSink* it = new nsXHTMLParanoidFragmentSink(PR_TRUE);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = nsXHTMLParanoidFragmentSink::Init();
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ADDREF(*aResult = it);
  
  return NS_OK;
}

void
NS_XHTMLParanoidFragmentSinkShutdown()
{
  nsXHTMLParanoidFragmentSink::Cleanup();
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXHTMLParanoidFragmentSink,
                             nsXMLFragmentContentSink)

nsresult
nsXHTMLParanoidFragmentSink::AddAttributes(const PRUnichar** aAtts,
                                           nsIContent* aContent)
{
  nsresult rv = NS_OK;

  // use this to check for safe URIs in the few attributes that allow them
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIURI> baseURI;
  PRUint32 flags = nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL;

  // scrub URI attributes that point at dangerous content
  // We have to do this here, because this is where we have a base URI,
  // but we can't do all the scrubbing here, because other parts of the
  // code get the attributes before this method is called.
  nsTArray<const PRUnichar *> allowedAttrs;
  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsCOMPtr<nsINodeInfo> nodeInfo;
  while (*aAtts) {
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);
    nodeInfo = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    // check the attributes we allow that contain URIs
    if (IsAttrURI(nodeInfo->NameAtom())) {
      if (!aAtts[1])
        rv = NS_ERROR_FAILURE;
      if (!baseURI)
        baseURI = aContent->GetBaseURI();
      nsCOMPtr<nsIURI> attrURI;
      rv = NS_NewURI(getter_AddRefs(attrURI), nsDependentString(aAtts[1]),
                     nsnull, baseURI);
      if (NS_SUCCEEDED(rv)) {
        rv = secMan->CheckLoadURIWithPrincipal(mTargetDocument->NodePrincipal(),
                                               attrURI, flags);
      }
    }

    if (NS_SUCCEEDED(rv)) {
      allowedAttrs.AppendElement(aAtts[0]);
      allowedAttrs.AppendElement(aAtts[1]);
    }

    aAtts += 2;
  }
  allowedAttrs.AppendElement((const PRUnichar*) nsnull);

  return nsXMLFragmentContentSink::AddAttributes(allowedAttrs.Elements(),
                                                 aContent);
}

NS_IMETHODIMP
nsXHTMLParanoidFragmentSink::HandleStartElement(const PRUnichar *aName,
                                                const PRUnichar **aAtts,
                                                PRUint32 aAttsCount,
                                                PRInt32 aIndex,
                                                PRUint32 aLineNumber)
{
  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                 getter_AddRefs(localName), &nameSpaceID);
  
  // If the element is not in the XHTML namespace, bounce it
  if (nameSpaceID != kNameSpaceID_XHTML)
    return NS_OK;
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  
  // bounce it if it's not on the whitelist or we're inside
  // <script> or <style>
  nsCOMPtr<nsIAtom> name = nodeInfo->NameAtom();
  if (mSkipLevel != 0 ||
      name == nsGkAtoms::script ||
      name == nsGkAtoms::style) {
    ++mSkipLevel; // track this so we don't spew script text
    return NS_OK;
  }  
  
  if (!sAllowedTags || !sAllowedTags->GetEntry(name))
    return NS_OK;
  
  // It's an allowed element, so let's scrub the attributes
  nsTArray<const PRUnichar *> allowedAttrs;
  for (PRUint32 i = 0; i < aAttsCount; i += 2) {
    nsContentUtils::SplitExpatName(aAtts[i], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);
    nodeInfo = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
    
    name = nodeInfo->NameAtom();
    // Add if it's xmlns, xml: or on the HTML whitelist
    if (nameSpaceID == kNameSpaceID_XMLNS ||
        nameSpaceID == kNameSpaceID_XML ||
        (sAllowedAttributes && sAllowedAttributes->GetEntry(name))) {
      allowedAttrs.AppendElement(aAtts[i]);
      allowedAttrs.AppendElement(aAtts[i + 1]);
    }
  }
  allowedAttrs.AppendElement((const PRUnichar*) nsnull);
  return
    nsXMLFragmentContentSink::HandleStartElement(aName,
                                                 allowedAttrs.Elements(),
                                                 allowedAttrs.Length() - 1,
                                                 aIndex,
                                                 aLineNumber);
}

NS_IMETHODIMP 
nsXHTMLParanoidFragmentSink::HandleEndElement(const PRUnichar *aName)
{
  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                 getter_AddRefs(localName), &nameSpaceID);
  
  // If the element is not in the XHTML namespace, bounce it
  if (nameSpaceID != kNameSpaceID_XHTML) {
    return NS_OK;
  }
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  
  nsCOMPtr<nsIAtom> name = nodeInfo->NameAtom();
  if (mSkipLevel != 0) {
    --mSkipLevel;
    return NS_OK;
  }

  if (!sAllowedTags || !sAllowedTags->GetEntry(name)) {
    return NS_OK;
  }

  return nsXMLFragmentContentSink::HandleEndElement(aName);
}

NS_IMETHODIMP
nsXHTMLParanoidFragmentSink::
HandleProcessingInstruction(const PRUnichar *aTarget, 
                            const PRUnichar *aData)
{
  // We don't do PIs
  return NS_OK;
}

NS_IMETHODIMP
nsXHTMLParanoidFragmentSink::HandleComment(const PRUnichar *aName)
{
  // We don't do comments
  return NS_OK;
}

// We pass all character data through, unless we're inside <script>
NS_IMETHODIMP 
nsXHTMLParanoidFragmentSink::HandleCDataSection(const PRUnichar *aData, 
                                                PRUint32 aLength)
{
  if (mSkipLevel != 0) {
    return NS_OK;
  }

  return nsXMLFragmentContentSink::HandleCDataSection(aData, aLength);
}

NS_IMETHODIMP 
nsXHTMLParanoidFragmentSink::HandleCharacterData(const PRUnichar *aData, 
                                                 PRUint32 aLength)
{
  if (mSkipLevel != 0) {
    return NS_OK;
  }

  return nsXMLFragmentContentSink::HandleCharacterData(aData, aLength);
}
