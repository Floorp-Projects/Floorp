/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * An implementation for a Gecko-style content sink that knows how
 * to build a content model (the "prototype" document) from XUL.
 *
 * For more information on XUL,
 * see http://developer.mozilla.org/en/docs/XUL
 */

#include "nsXULContentSink.h"

#include "jsfriendapi.h"

#include "nsCOMPtr.h"
#include "nsForwardReference.h"
#include "nsHTMLStyleSheet.h"
#include "nsIContentSink.h"
#include "nsIDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIFormControl.h"
#include "nsINodeInfo.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsNameSpaceManager.h"
#include "nsParserBase.h"
#include "nsViewManager.h"
#include "nsIXULDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "prmem.h"
#include "nsCRT.h"

#include "nsXULPrototypeDocument.h"     // XXXbe temporary
#include "mozilla/css/Loader.h"

#include "nsUnicharUtils.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsAttrName.h"
#include "nsXMLContentSink.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsContentTypeParser.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gContentSinkLog;
#endif

//----------------------------------------------------------------------

XULContentSinkImpl::ContextStack::ContextStack()
    : mTop(nullptr), mDepth(0)
{
}

XULContentSinkImpl::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        delete doomed;
    }
}

nsresult
XULContentSinkImpl::ContextStack::Push(nsXULPrototypeNode* aNode, State aState)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mNode  = aNode;
    entry->mState = aState;
    entry->mNext  = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
XULContentSinkImpl::ContextStack::Pop(State* aState)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* entry = mTop;
    mTop = mTop->mNext;
    --mDepth;

    *aState = entry->mState;
    delete entry;

    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopNode(nsRefPtr<nsXULPrototypeNode>& aNode)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    aNode = mTop->mNode;
    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopChildren(nsPrototypeArray** aChildren)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aChildren = &(mTop->mChildren);
    return NS_OK;
}

void
XULContentSinkImpl::ContextStack::Clear()
{
  Entry *cur = mTop;
  while (cur) {
    // Release the root element (and its descendants).
    Entry *next = cur->mNext;
    delete cur;
    cur = next;
  }

  mTop = nullptr;
  mDepth = 0;
}

void
XULContentSinkImpl::ContextStack::Traverse(nsCycleCollectionTraversalCallback& aCb)
{
  nsCycleCollectionTraversalCallback& cb = aCb;
  for (ContextStack::Entry* tmp = mTop; tmp; tmp = tmp->mNext) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNode)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildren)
  }
}

//----------------------------------------------------------------------


XULContentSinkImpl::XULContentSinkImpl()
    : mText(nullptr),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(true),
      mState(eInProlog),
      mParser(nullptr)
{

#ifdef PR_LOGGING
    if (! gContentSinkLog)
        gContentSinkLog = PR_NewLogModule("nsXULContentSink");
#endif
}


XULContentSinkImpl::~XULContentSinkImpl()
{
    NS_IF_RELEASE(mParser); // XXX should've been released by now, unless error.

    // The context stack _should_ be empty, unless something has gone wrong.
    NS_ASSERTION(mContextStack.Depth() == 0, "Context stack not empty?");
    mContextStack.Clear();

    moz_free(mText);
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_CYCLE_COLLECTION_CLASS(XULContentSinkImpl)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XULContentSinkImpl)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNodeInfoManager)
  tmp->mContextStack.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrototype)
  NS_IF_RELEASE(tmp->mParser);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XULContentSinkImpl)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNodeInfoManager)
  tmp->mContextStack.Traverse(cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrototype)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mParser)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XULContentSinkImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIExpatSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(XULContentSinkImpl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XULContentSinkImpl)

//----------------------------------------------------------------------
// nsIContentSink interface

NS_IMETHODIMP 
XULContentSinkImpl::WillBuildModel(nsDTDMode aDTDMode)
{
#if FIXME
    if (! mParentContentSink) {
        // If we're _not_ an overlay, then notify the document that
        // the load is beginning.
        mDocument->BeginLoad();
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::DidBuildModel(bool aTerminated)
{
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    if (doc) {
        doc->EndLoad();
        mDocument = nullptr;
    }

    // Drop our reference to the parser to get rid of a circular
    // reference.
    NS_IF_RELEASE(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillInterrupt(void)
{
    // XXX Notify the docshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillResume(void)
{
    // XXX Notify the docshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetParser(nsParserBase* aParser)
{
    NS_IF_RELEASE(mParser);
    mParser = aParser;
    NS_IF_ADDREF(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetDocumentCharset(nsACString& aCharset)
{
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    if (doc) {
        doc->SetDocumentCharacterSet(aCharset);
    }
  
    return NS_OK;
}

nsISupports *
XULContentSinkImpl::GetTarget()
{
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    return doc;    
}

//----------------------------------------------------------------------

nsresult
XULContentSinkImpl::Init(nsIDocument* aDocument,
                         nsXULPrototypeDocument* aPrototype)
{
    NS_PRECONDITION(aDocument != nullptr, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;
    
    nsresult rv;

    mDocument    = do_GetWeakReference(aDocument);
    mPrototype   = aPrototype;

    mDocumentURL = mPrototype->GetURI();

    // XXX this presumes HTTP header info is already set in document
    // XXX if it isn't we need to set it here...
    // XXXbz not like GetHeaderData on the proto doc _does_ anything....
    nsAutoString preferredStyle;
    rv = mPrototype->GetHeaderData(nsGkAtoms::headerDefaultStyle,
                                   preferredStyle);
    if (NS_FAILED(rv)) return rv;

    if (!preferredStyle.IsEmpty()) {
        aDocument->SetHeaderData(nsGkAtoms::headerDefaultStyle,
                                 preferredStyle);
    }

    // Set the right preferred style on the document's CSSLoader.
    aDocument->CSSLoader()->SetPreferredSheet(preferredStyle);

    mNodeInfoManager = aPrototype->GetNodeInfoManager();
    if (! mNodeInfoManager)
        return NS_ERROR_UNEXPECTED;

    mState = eInProlog;
    return NS_OK;
}


//----------------------------------------------------------------------
//
// Text buffering
//

bool
XULContentSinkImpl::IsDataInBuffer(char16_t* buffer, int32_t length)
{
    for (int32_t i = 0; i < length; ++i) {
        if (buffer[i] == ' ' ||
            buffer[i] == '\t' ||
            buffer[i] == '\n' ||
            buffer[i] == '\r')
            continue;

        return true;
    }
    return false;
}


nsresult
XULContentSinkImpl::FlushText(bool aCreateTextNode)
{
    nsresult rv;

    do {
        // Don't do anything if there's no text to create a node from, or
        // if they've told us not to create a text node
        if (! mTextLength)
            break;

        if (! aCreateTextNode)
            break;

        nsRefPtr<nsXULPrototypeNode> node;
        rv = mContextStack.GetTopNode(node);
        if (NS_FAILED(rv)) return rv;

        bool stripWhitespace = false;
        if (node->mType == nsXULPrototypeNode::eType_Element) {
            nsINodeInfo *nodeInfo =
                static_cast<nsXULPrototypeElement*>(node.get())->mNodeInfo;

            if (nodeInfo->NamespaceEquals(kNameSpaceID_XUL))
                stripWhitespace = !nodeInfo->Equals(nsGkAtoms::label) &&
                                  !nodeInfo->Equals(nsGkAtoms::description);
        }

        // Don't bother if there's nothing but whitespace.
        if (stripWhitespace && ! IsDataInBuffer(mText, mTextLength))
            break;

        // Don't bother if we're not in XUL document body
        if (mState != eInDocumentElement || mContextStack.Depth() == 0)
            break;

        nsXULPrototypeText* text = new nsXULPrototypeText();
        if (! text)
            return NS_ERROR_OUT_OF_MEMORY;

        text->mValue.Assign(mText, mTextLength);
        if (stripWhitespace)
            text->mValue.Trim(" \t\n\r");

        // hook it up
        nsPrototypeArray* children = nullptr;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        // transfer ownership of 'text' to the children array
        children->AppendElement(text);
    } while (0);

    // Reset our text buffer
    mTextLength = 0;
    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
XULContentSinkImpl::NormalizeAttributeString(const char16_t *aExpatName,
                                             nsAttrName &aName)
{
    int32_t nameSpaceID;
    nsCOMPtr<nsIAtom> prefix, localName;
    nsContentUtils::SplitExpatName(aExpatName, getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID == kNameSpaceID_None) {
        aName.SetTo(localName);

        return NS_OK;
    }

    nsCOMPtr<nsINodeInfo> ni;
    ni = mNodeInfoManager->GetNodeInfo(localName, prefix,
                                       nameSpaceID,
                                       nsIDOMNode::ATTRIBUTE_NODE);
    aName.SetTo(ni);

    return NS_OK;
}

nsresult
XULContentSinkImpl::CreateElement(nsINodeInfo *aNodeInfo,
                                  nsXULPrototypeElement** aResult)
{
    nsXULPrototypeElement* element = new nsXULPrototypeElement();
    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    element->mNodeInfo    = aNodeInfo;
    
    *aResult = element;
    return NS_OK;
}

/**** BEGIN NEW APIs ****/


NS_IMETHODIMP 
XULContentSinkImpl::HandleStartElement(const char16_t *aName, 
                                       const char16_t **aAtts,
                                       uint32_t aAttsCount, 
                                       int32_t aIndex, 
                                       uint32_t aLineNumber)
{ 
  // XXX Hopefully the parser will flag this before we get here. If
  // we're in the epilog, there should be no new elements
  NS_PRECONDITION(mState != eInEpilog, "tag in XUL doc epilog");
  NS_PRECONDITION(aIndex >= -1, "Bogus aIndex");
  NS_PRECONDITION(aAttsCount % 2 == 0, "incorrect aAttsCount");
  // Adjust aAttsCount so it's the actual number of attributes
  aAttsCount /= 2;
  
  if (mState == eInEpilog)
      return NS_ERROR_UNEXPECTED;

  if (mState != eInScript) {
      FlushText();
  }

  int32_t nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                 getter_AddRefs(localName), &nameSpaceID);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID,
                                           nsIDOMNode::ELEMENT_NODE);
  
  nsresult rv = NS_OK;
  switch (mState) {
  case eInProlog:
      // We're the root document element
      rv = OpenRoot(aAtts, aAttsCount, nodeInfo);
      break;

  case eInDocumentElement:
      rv = OpenTag(aAtts, aAttsCount, aLineNumber, nodeInfo);
      break;

  case eInEpilog:
  case eInScript:
      PR_LOG(gContentSinkLog, PR_LOG_WARNING,
             ("xul: warning: unexpected tags in epilog at line %d",
             aLineNumber));
      rv = NS_ERROR_UNEXPECTED; // XXX
      break;
  }

  // Set the ID attribute atom on the node info object for this node
  if (aIndex != -1 && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIAtom> IDAttr = do_GetAtom(aAtts[aIndex]);

    if (IDAttr) {
      nodeInfo->SetIDAttributeAtom(IDAttr);
    }
  }

  return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleEndElement(const char16_t *aName)
{
    // Never EVER return anything but NS_OK or
    // NS_ERROR_HTMLPARSER_BLOCK from this method. Doing so will blow
    // the parser's little mind all over the planet.
    nsresult rv;

    nsRefPtr<nsXULPrototypeNode> node;
    rv = mContextStack.GetTopNode(node);

    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    switch (node->mType) {
    case nsXULPrototypeNode::eType_Element: {
        // Flush any text _now_, so that we'll get text nodes created
        // before popping the stack.
        FlushText();

        // Pop the context stack and do prototype hookup.
        nsPrototypeArray* children = nullptr;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        nsXULPrototypeElement* element =
          static_cast<nsXULPrototypeElement*>(node.get());

        int32_t count = children->Length();
        if (count) {
            element->mChildren.SetCapacity(count);

            for (int32_t i = 0; i < count; ++i)
                element->mChildren.AppendElement(children->ElementAt(i));

        }
    }
    break;

    case nsXULPrototypeNode::eType_Script: {
        nsXULPrototypeScript* script =
            static_cast<nsXULPrototypeScript*>(node.get());

        // If given a src= attribute, we must ignore script tag content.
        if (!script->mSrcURI && !script->GetScriptObject()) {
            nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);

            script->mOutOfLine = false;
            if (doc)
                script->Compile(mText, mTextLength, mDocumentURL,
                                script->mLineNo, doc, mPrototype);
        }

        FlushText(false);
    }
    break;

    default:
        NS_ERROR("didn't expect that");
        break;
    }

    rv = mContextStack.Pop(&mState);
    NS_ASSERTION(NS_SUCCEEDED(rv), "context stack corrupted");
    if (NS_FAILED(rv)) return rv;

    if (mContextStack.Depth() == 0) {
        // The root element should -always- be an element, because
        // it'll have been created via XULContentSinkImpl::OpenRoot().
        NS_ASSERTION(node->mType == nsXULPrototypeNode::eType_Element, "root is not an element");
        if (node->mType != nsXULPrototypeNode::eType_Element)
            return NS_ERROR_UNEXPECTED;

        // Now that we're done parsing, set the prototype document's
        // root element. This transfers ownership of the prototype
        // element tree to the prototype document.
        nsXULPrototypeElement* element =
            static_cast<nsXULPrototypeElement*>(node.get());

        mPrototype->SetRootElement(element);
        mState = eInEpilog;
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleComment(const char16_t *aName)
{
   FlushText();
   return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleCDataSection(const char16_t *aData, uint32_t aLength)
{
    FlushText();
    return AddText(aData, aLength);
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleDoctypeDecl(const nsAString & aSubset, 
                                      const nsAString & aName, 
                                      const nsAString & aSystemId, 
                                      const nsAString & aPublicId,
                                      nsISupports* aCatalogData)
{
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleCharacterData(const char16_t *aData, 
                                        uint32_t aLength)
{
  if (aData && mState != eInProlog && mState != eInEpilog) {
    return AddText(aData, aLength);
  }
  return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleProcessingInstruction(const char16_t *aTarget, 
                                                const char16_t *aData)
{
    FlushText();

    const nsDependentString target(aTarget);
    const nsDependentString data(aData);

    // Note: the created nsXULPrototypePI has mRefCnt == 1
    nsRefPtr<nsXULPrototypePI> pi = new nsXULPrototypePI();
    if (!pi)
        return NS_ERROR_OUT_OF_MEMORY;

    pi->mTarget = target;
    pi->mData = data;

    if (mState == eInProlog) {
        // Note: passing in already addrefed pi
        return mPrototype->AddProcessingInstruction(pi);
    }

    nsresult rv;
    nsPrototypeArray* children = nullptr;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (!children->AppendElement(pi)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}


NS_IMETHODIMP
XULContentSinkImpl::HandleXMLDeclaration(const char16_t *aVersion,
                                         const char16_t *aEncoding,
                                         int32_t aStandalone)
{
  return NS_OK;
}


NS_IMETHODIMP
XULContentSinkImpl::ReportError(const char16_t* aErrorText, 
                                const char16_t* aSourceText,
                                nsIScriptError *aError,
                                bool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

  // The expat driver should report the error.
  *_retval = true;

  nsresult rv = NS_OK;

  // make sure to empty the context stack so that
  // <parsererror> could become the root element.
  mContextStack.Clear();

  mState = eInProlog;

  // Clear any buffered-up text we have.  It's enough to set the length to 0.
  // The buffer itself is allocated when we're created and deleted in our
  // destructor, so don't mess with it.
  mTextLength = 0;

  nsCOMPtr<nsIXULDocument> doc = do_QueryReferent(mDocument);
  if (doc && !doc->OnDocumentParserError()) {
    // The overlay was broken.  Don't add a messy element to the master doc.
    return NS_OK;
  }

  const char16_t* noAtts[] = { 0, 0 };

  NS_NAMED_LITERAL_STRING(errorNs,
                          "http://www.mozilla.org/newlayout/xml/parsererror.xml");

  nsAutoString parsererror(errorNs);
  parsererror.Append((char16_t)0xFFFF);
  parsererror.AppendLiteral("parsererror");
  
  rv = HandleStartElement(parsererror.get(), noAtts, 0, -1, 0);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = HandleCharacterData(aErrorText, NS_strlen(aErrorText));
  NS_ENSURE_SUCCESS(rv,rv);  
  
  nsAutoString sourcetext(errorNs);
  sourcetext.Append((char16_t)0xFFFF);
  sourcetext.AppendLiteral("sourcetext");

  rv = HandleStartElement(sourcetext.get(), noAtts, 0, -1, 0);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleCharacterData(aSourceText, NS_strlen(aSourceText));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleEndElement(sourcetext.get());
  NS_ENSURE_SUCCESS(rv,rv); 
  
  rv = HandleEndElement(parsererror.get());
  NS_ENSURE_SUCCESS(rv,rv);

  return rv;
}

nsresult
XULContentSinkImpl::OpenRoot(const char16_t** aAttributes, 
                             const uint32_t aAttrLen, 
                             nsINodeInfo *aNodeInfo)
{
    NS_ASSERTION(mState == eInProlog, "how'd we get here?");
    if (mState != eInProlog)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    if (aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML) || 
        aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XUL)) {
        PR_LOG(gContentSinkLog, PR_LOG_ERROR,
               ("xul: script tag not allowed as root content element"));

        return NS_ERROR_UNEXPECTED;
    }

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gContentSinkLog, PR_LOG_ERROR)) {
            nsAutoString anodeC;
            aNodeInfo->GetName(anodeC);
            PR_LOG(gContentSinkLog, PR_LOG_ERROR,
                   ("xul: unable to create element '%s' at line %d",
                    NS_ConvertUTF16toUTF8(anodeC).get(),
                    -1)); // XXX pass in line number
        }
#endif

        return rv;
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) {
        element->Release();
        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aAttributes, aAttrLen, element);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenTag(const char16_t** aAttributes, 
                            const uint32_t aAttrLen,
                            const uint32_t aLineNumber,
                            nsINodeInfo *aNodeInfo)
{
    nsresult rv;

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gContentSinkLog, PR_LOG_ERROR)) {
            nsAutoString anodeC;
            aNodeInfo->GetName(anodeC);
            PR_LOG(gContentSinkLog, PR_LOG_ERROR,
                   ("xul: unable to create element '%s' at line %d",
                    NS_ConvertUTF16toUTF8(anodeC).get(),
                    aLineNumber));
        }
#endif

        return rv;
    }

    // Link this element to its parent.
    nsPrototypeArray* children = nullptr;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_FAILED(rv)) {
        delete element;
        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aAttributes, aAttrLen, element);
    if (NS_FAILED(rv)) return rv;

    children->AppendElement(element);

    if (aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML) || 
        aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XUL)) {
        // Do scripty things now
        rv = OpenScript(aAttributes, aLineNumber);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ASSERTION(mState == eInScript || mState == eInDocumentElement,
                     "Unexpected state");
        if (mState == eInScript) {
            // OpenScript has pushed the nsPrototypeScriptElement onto the 
            // stack, so we're done.
            return NS_OK;
        }
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenScript(const char16_t** aAttributes,
                               const uint32_t aLineNumber)
{
  uint32_t langID = nsIProgrammingLanguage::JAVASCRIPT;
  uint32_t version = JSVERSION_LATEST;
  nsresult rv;

  // Look for SRC attribute and look for a LANGUAGE attribute
  nsAutoString src;
  while (*aAttributes) {
      const nsDependentString key(aAttributes[0]);
      if (key.EqualsLiteral("src")) {
          src.Assign(aAttributes[1]);
      }
      else if (key.EqualsLiteral("type")) {
          nsDependentString str(aAttributes[1]);
          nsContentTypeParser parser(str);
          nsAutoString mimeType;
          rv = parser.GetType(mimeType);
          if (NS_FAILED(rv)) {
              if (rv == NS_ERROR_INVALID_ARG) {
                  // Might as well bail out now instead of setting langID to
                  // nsIProgrammingLanguage::UNKNOWN and bailing out later.
                  return NS_OK;
              }
              // We do want the warning here
              NS_ENSURE_SUCCESS(rv, rv);
          }

          if (nsContentUtils::IsJavascriptMIMEType(mimeType)) {
              langID = nsIProgrammingLanguage::JAVASCRIPT;
              version = JSVERSION_LATEST;
          } else {
              langID = nsIProgrammingLanguage::UNKNOWN;
          }

          if (langID != nsIProgrammingLanguage::UNKNOWN) {
              // Get the version string, and ensure the language supports it.
              nsAutoString versionName;
              rv = parser.GetParameter("version", versionName);

              if (NS_SUCCEEDED(rv)) {
                  version = nsContentUtils::ParseJavascriptVersion(versionName);
              } else if (rv != NS_ERROR_INVALID_ARG) {
                  return rv;
              }
          }
      }
      else if (key.EqualsLiteral("language")) {
          // Language is deprecated, and the impl in nsScriptLoader ignores the
          // various version strings anyway.  So we make no attempt to support
          // languages other than JS for language=
          nsAutoString lang(aAttributes[1]);
          if (nsContentUtils::IsJavaScriptLanguage(lang)) {
              version = JSVERSION_DEFAULT;
              langID = nsIProgrammingLanguage::JAVASCRIPT;
          }
      }
      aAttributes += 2;
  }

  // Not all script languages have a "sandbox" concept.  At time of
  // writing, Python is the only other language, and it does not.
  // For such languages, neither any inline script nor remote script are
  // safe to execute from untrusted sources.
  // So for such languages, we only allow script when the document
  // itself is from chrome.  We then don't bother to check the
  // "src=" tag - we trust chrome to do the right thing.
  // (See also similar code in nsScriptLoader.cpp)
  nsCOMPtr<nsIDocument> doc(do_QueryReferent(mDocument));
  if (langID != nsIProgrammingLanguage::UNKNOWN && 
      langID != nsIProgrammingLanguage::JAVASCRIPT &&
      doc && !nsContentUtils::IsChromeDoc(doc)) {
      langID = nsIProgrammingLanguage::UNKNOWN;
      NS_WARNING("Non JS language called from non chrome - ignored");
  }

  // Don't process scripts that aren't known
  if (langID != nsIProgrammingLanguage::UNKNOWN) {
      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      if (doc)
          globalObject = do_QueryInterface(doc->GetWindow());
      nsRefPtr<nsXULPrototypeScript> script =
          new nsXULPrototypeScript(aLineNumber, version);
      if (! script)
          return NS_ERROR_OUT_OF_MEMORY;

      // If there is a SRC attribute...
      if (! src.IsEmpty()) {
          // Use the SRC attribute value to load the URL
          rv = NS_NewURI(getter_AddRefs(script->mSrcURI), src, nullptr, mDocumentURL);

          // Check if this document is allowed to load a script from this source
          // NOTE: if we ever allow scripts added via the DOM to run, we need to
          // add a CheckLoadURI call for that as well.
          if (NS_SUCCEEDED(rv)) {
              if (!mSecMan)
                  mSecMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
              if (NS_SUCCEEDED(rv)) {
                  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument, &rv);

                  if (NS_SUCCEEDED(rv)) {
                      rv = mSecMan->
                          CheckLoadURIWithPrincipal(doc->NodePrincipal(),
                                                    script->mSrcURI,
                                                    nsIScriptSecurityManager::ALLOW_CHROME);
                  }
              }
          }

          if (NS_FAILED(rv)) {
              return rv;
          }

          // Attempt to deserialize an out-of-line script from the FastLoad
          // file right away.  Otherwise we'll end up reloading the script and
          // corrupting the FastLoad file trying to serialize it, in the case
          // where it's already there.
          script->DeserializeOutOfLine(nullptr, mPrototype);
      }

      nsPrototypeArray* children = nullptr;
      rv = mContextStack.GetTopChildren(&children);
      if (NS_FAILED(rv)) {
          return rv;
      }

      children->AppendElement(script);

      mConstrainSize = false;

      mContextStack.Push(script, mState);
      mState = eInScript;
  }

  return NS_OK;
}

nsresult
XULContentSinkImpl::AddAttributes(const char16_t** aAttributes, 
                                  const uint32_t aAttrLen, 
                                  nsXULPrototypeElement* aElement)
{
  // Add tag attributes to the element
  nsresult rv;

  // Create storage for the attributes
  nsXULPrototypeAttribute* attrs = nullptr;
  if (aAttrLen > 0) {
    attrs = new nsXULPrototypeAttribute[aAttrLen];
    if (! attrs)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  aElement->mAttributes    = attrs;
  aElement->mNumAttributes = aAttrLen;

  // Copy the attributes into the prototype
  uint32_t i;
  for (i = 0; i < aAttrLen; ++i) {
      rv = NormalizeAttributeString(aAttributes[i * 2], attrs[i].mName);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aElement->SetAttrAt(i, nsDependentString(aAttributes[i * 2 + 1]),
                               mDocumentURL);
      NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
      if (PR_LOG_TEST(gContentSinkLog, PR_LOG_DEBUG)) {
          nsAutoString extraWhiteSpace;
          int32_t cnt = mContextStack.Depth();
          while (--cnt >= 0)
              extraWhiteSpace.AppendLiteral("  ");
          nsAutoString qnameC,valueC;
          qnameC.Assign(aAttributes[0]);
          valueC.Assign(aAttributes[1]);
          PR_LOG(gContentSinkLog, PR_LOG_DEBUG,
                 ("xul: %.5d. %s    %s=%s",
                  -1, // XXX pass in line number
                  NS_ConvertUTF16toUTF8(extraWhiteSpace).get(),
                  NS_ConvertUTF16toUTF8(qnameC).get(),
                  NS_ConvertUTF16toUTF8(valueC).get()));
      }
#endif
  }

  return NS_OK;
}

nsresult
XULContentSinkImpl::AddText(const char16_t* aText, 
                            int32_t aLength)
{
  // Create buffer when we first need it
  if (0 == mTextSize) {
      mText = (char16_t *) moz_malloc(sizeof(char16_t) * 4096);
      if (nullptr == mText) {
          return NS_ERROR_OUT_OF_MEMORY;
      }
      mTextSize = 4096;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  int32_t offset = 0;
  while (0 != aLength) {
    int32_t amount = mTextSize - mTextLength;
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
        mText = (char16_t *) moz_realloc(mText, sizeof(char16_t) * mTextSize);
        if (nullptr == mText) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    memcpy(&mText[mTextLength],aText + offset, sizeof(char16_t) * amount);
    
    mTextLength += amount;
    offset += amount;
    aLength -= amount;
  }

  return NS_OK;
}
