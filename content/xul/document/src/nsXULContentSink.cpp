/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *   Mark Hammond <mhammond@skippinet.com.au>
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

/*
 * An implementation for a Gecko-style content sink that knows how
 * to build a content model (the "prototype" document) from XUL.
 *
 * For more information on XUL,
 * see http://developer.mozilla.org/en/docs/XUL
 */

#include "nsCOMPtr.h"
#include "nsForwardReference.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIContentSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIFormControl.h"
#include "nsHTMLStyleSheet.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIViewManager.h"
#include "nsIXULContentSink.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsIScriptSecurityManager.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsParserUtils.h"
#include "nsIMIMEHeaderParam.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "prmem.h"
#include "jscntxt.h"  // for JSVERSION_HAS_XML
#include "nsCRT.h"

#include "nsIFastLoadService.h"         // XXXbe temporary
#include "nsIObjectInputStream.h"       // XXXbe temporary
#include "nsXULDocument.h"              // XXXbe temporary

#include "nsIExpatSink.h"
#include "nsUnicharUtils.h"
#include "nsGkAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsContentUtils.h"
#include "nsAttrName.h"
#include "nsXMLContentSink.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

static NS_DEFINE_CID(kXULPrototypeCacheCID, NS_XULPROTOTYPECACHE_CID);

//----------------------------------------------------------------------

class XULContentSinkImpl : public nsIXULContentSink,
                           public nsIExpatSink
{
public:
    XULContentSinkImpl(nsresult& aRV);
    virtual ~XULContentSinkImpl();

    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXPATSINK

    // nsIContentSink
    NS_IMETHOD WillTokenize(void) { return NS_OK; }
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(void);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsIParser* aParser);  
    virtual void FlushPendingNotifications(mozFlushType aType) { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
    virtual nsISupports *GetTarget();

    // nsIXULContentSink
    NS_IMETHOD Init(nsIDocument* aDocument, nsXULPrototypeDocument* aPrototype);

protected:
    // pseudo-constants
    static nsrefcnt               gRefCnt;
    static nsIXULPrototypeCache*  gXULCache;

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    nsresult AddAttributes(const PRUnichar** aAttributes, 
                           const PRUint32 aAttrLen, 
                           nsXULPrototypeElement* aElement);
   
    nsresult OpenRoot(const PRUnichar** aAttributes, 
                      const PRUint32 aAttrLen, 
                      nsINodeInfo *aNodeInfo);

    nsresult OpenTag(const PRUnichar** aAttributes, 
                     const PRUint32 aAttrLen, 
                     const PRUint32 aLineNumber, 
                     nsINodeInfo *aNodeInfo);
    
    nsresult OpenScript(const PRUnichar** aAttributes,
                        const PRUint32 aLineNumber);

    static PRBool IsDataInBuffer(PRUnichar* aBuffer, PRInt32 aLength);

    nsresult SetElementScriptType(nsXULPrototypeElement* element,
                                  const PRUnichar** aAttributes, 
                                  const PRUint32 aAttrLen);

    // Text management
    nsresult FlushText(PRBool aCreateTextNode = PR_TRUE);
    nsresult AddText(const PRUnichar* aText, PRInt32 aLength);


    nsRefPtr<nsNodeInfoManager> mNodeInfoManager;
    
    
    nsresult NormalizeAttributeString(const PRUnichar *aExpatName,
                                      nsAttrName &aName);
    nsresult CreateElement(nsINodeInfo *aNodeInfo, nsXULPrototypeElement** aResult);


    public:
    enum State { eInProlog, eInDocumentElement, eInScript, eInEpilog };
    protected:

    State mState;

    // content stack management
    class ContextStack {
    protected:
        struct Entry {
            nsXULPrototypeNode* mNode;
            // a LOT of nodes have children; preallocate for 8
            nsAutoVoidArray     mChildren;
            State               mState;
            Entry*              mNext;
        };

        Entry* mTop;
        PRInt32 mDepth;

    public:
        ContextStack();
        ~ContextStack();

        PRInt32 Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeNode* aNode, State aState);
        nsresult Pop(State* aState);

        nsresult GetTopNode(nsXULPrototypeNode** aNode);
        nsresult GetTopChildren(nsVoidArray** aChildren);
        nsresult GetTopNodeScriptType(PRUint32 *aScriptType);
    };

    friend class ContextStack;
    ContextStack mContextStack;

    nsWeakPtr              mDocument;             // [OWNER]
    nsCOMPtr<nsIURI>       mDocumentURL;          // [OWNER]

    nsRefPtr<nsXULPrototypeDocument> mPrototype;  // [OWNER]
    nsIParser*             mParser;               // [OWNER] We use regular pointer b/c of funky exports on nsIParser
    
    nsCOMPtr<nsICSSLoader> mCSSLoader;            // [OWNER]
    nsCOMPtr<nsICSSParser> mCSSParser;            // [OWNER]
    nsCOMPtr<nsIScriptSecurityManager> mSecMan;
};

nsrefcnt XULContentSinkImpl::gRefCnt;
nsIXULPrototypeCache* XULContentSinkImpl::gXULCache;

//----------------------------------------------------------------------

XULContentSinkImpl::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
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
XULContentSinkImpl::ContextStack::GetTopNode(nsXULPrototypeNode** aNode)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aNode = mTop->mNode;
    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopChildren(nsVoidArray** aChildren)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aChildren = &(mTop->mChildren);
    return NS_OK;
}

nsresult
XULContentSinkImpl::ContextStack::GetTopNodeScriptType(PRUint32 *aScriptType)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    // This would be much simpler if nsXULPrototypeNode itself
    // stored the language ID - but text elements don't need it!
    nsresult rv = NS_OK;
    nsXULPrototypeNode* node;
    rv = GetTopNode(&node);
    if (NS_FAILED(rv)) return rv;
    switch (node->mType) {
        case nsXULPrototypeNode::eType_Element: {
            nsXULPrototypeElement *parent = \
                NS_REINTERPRET_CAST(nsXULPrototypeElement*, node);
            *aScriptType = parent->mScriptTypeID;
            break;
        }
        case nsXULPrototypeNode::eType_Script: {
            nsXULPrototypeScript *parent = \
                NS_REINTERPRET_CAST(nsXULPrototypeScript*, node);
            *aScriptType = parent->mScriptObject.getScriptTypeID();
            break;
        }
        default: {
            NS_WARNING("Unexpected parent node type");
            rv = NS_ERROR_UNEXPECTED;
        }
    }
    return rv;
}

//----------------------------------------------------------------------


XULContentSinkImpl::XULContentSinkImpl(nsresult& rv)
    : mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mState(eInProlog),
      mParser(nsnull)
{

    if (gRefCnt++ == 0) {
        rv = CallGetService(kXULPrototypeCacheCID, &gXULCache);
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsXULContentSink");
#endif

    rv = NS_OK;
}


XULContentSinkImpl::~XULContentSinkImpl()
{
    NS_IF_RELEASE(mParser); // XXX should've been released by now, unless error.

    // Pop all of the elements off of the context stack, and delete
    // any remaining content elements. The context stack _should_ be
    // empty, unless something has gone wrong.
    while (mContextStack.Depth()) {
        nsresult rv;

        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_SUCCEEDED(rv)) {
            for (PRInt32 i = children->Count() - 1; i >= 0; --i) {
                nsXULPrototypeNode* child =
                    NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

                delete child;
            }
        }

        nsXULPrototypeNode* node;
        rv = mContextStack.GetTopNode(&node);
        if (NS_SUCCEEDED(rv)) delete node;

        State state;
        mContextStack.Pop(&state);
    }

    PR_FREEIF(mText);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gXULCache);
    }
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_ISUPPORTS4(XULContentSinkImpl,
                   nsIXULContentSink,
                   nsIXMLContentSink,
                   nsIContentSink,
                   nsIExpatSink)

//----------------------------------------------------------------------
// nsIContentSink interface

NS_IMETHODIMP 
XULContentSinkImpl::WillBuildModel(void)
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
XULContentSinkImpl::DidBuildModel(void)
{
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    if (doc) {
        doc->EndLoad();
        mDocument = nsnull;
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
XULContentSinkImpl::SetParser(nsIParser* aParser)
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
//
// nsIXULContentSink interface
//

NS_IMETHODIMP
XULContentSinkImpl::Init(nsIDocument* aDocument, nsXULPrototypeDocument* aPrototype)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
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

    // Get the CSS loader from the document so we can load
    // stylesheets
    mCSSLoader = aDocument->CSSLoader();
    mCSSLoader->SetPreferredSheet(preferredStyle);

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

PRBool
XULContentSinkImpl::IsDataInBuffer(PRUnichar* buffer, PRInt32 length)
{
    for (PRInt32 i = 0; i < length; ++i) {
        if (buffer[i] == ' ' ||
            buffer[i] == '\t' ||
            buffer[i] == '\n' ||
            buffer[i] == '\r')
            continue;

        return PR_TRUE;
    }
    return PR_FALSE;
}


nsresult
XULContentSinkImpl::FlushText(PRBool aCreateTextNode)
{
    nsresult rv;

    do {
        // Don't do anything if there's no text to create a node from, or
        // if they've told us not to create a text node
        if (! mTextLength)
            break;

        if (! aCreateTextNode)
            break;

        nsXULPrototypeNode* node;
        rv = mContextStack.GetTopNode(&node);
        if (NS_FAILED(rv)) return rv;

        PRBool stripWhitespace = PR_FALSE;
        if (node->mType == nsXULPrototypeNode::eType_Element) {
            nsINodeInfo *nodeInfo =
                NS_STATIC_CAST(nsXULPrototypeElement*, node)->mNodeInfo;

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
        nsVoidArray* children;
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
XULContentSinkImpl::NormalizeAttributeString(const PRUnichar *aExpatName,
                                             nsAttrName &aName)
{
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> prefix, localName;
    nsContentUtils::SplitExpatName(aExpatName, getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID == kNameSpaceID_None) {
        aName.SetTo(localName);

        return NS_OK;
    }

    nsCOMPtr<nsINodeInfo> ni;
    nsresult rv = mNodeInfoManager->GetNodeInfo(localName, prefix,
                                                nameSpaceID,
                                                getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

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

nsresult
NS_NewXULContentSink(nsIXULContentSink** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    XULContentSinkImpl* sink = new XULContentSinkImpl(rv);
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv)) {
        delete sink;
        return rv;
    }

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}

/**** BEGIN NEW APIs ****/


NS_IMETHODIMP 
XULContentSinkImpl::HandleStartElement(const PRUnichar *aName, 
                                       const PRUnichar **aAtts,
                                       PRUint32 aAttsCount, 
                                       PRInt32 aIndex, 
                                       PRUint32 aLineNumber)
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

  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                 getter_AddRefs(localName), &nameSpaceID);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID,
                                              getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

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
      PR_LOG(gLog, PR_LOG_WARNING,
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
XULContentSinkImpl::HandleEndElement(const PRUnichar *aName)
{
    // Never EVER return anything but NS_OK or
    // NS_ERROR_HTMLPARSER_BLOCK from this method. Doing so will blow
    // the parser's little mind all over the planet.
    nsresult rv;

    nsXULPrototypeNode* node;
    rv = mContextStack.GetTopNode(&node);

    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    switch (node->mType) {
    case nsXULPrototypeNode::eType_Element: {
        // Flush any text _now_, so that we'll get text nodes created
        // before popping the stack.
        FlushText();

        // Pop the context stack and do prototype hookup.
        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        nsXULPrototypeElement* element =
            NS_REINTERPRET_CAST(nsXULPrototypeElement*, node);

        PRInt32 count = children->Count();
        if (count) {
            element->mChildren = new nsXULPrototypeNode*[count];
            if (! element->mChildren)
                return NS_ERROR_OUT_OF_MEMORY;

            for (PRInt32 i = count - 1; i >= 0; --i)
                element->mChildren[i] =
                    NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

            element->mNumChildren = count;
        }
    }
    break;

    case nsXULPrototypeNode::eType_Script: {
        nsXULPrototypeScript* script =
            NS_STATIC_CAST(nsXULPrototypeScript*, node);

        // If given a src= attribute, we must ignore script tag content.
        if (! script->mSrcURI && ! script->mScriptObject) {
            nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);

            script->mOutOfLine = PR_FALSE;
            if (doc)
                script->Compile(mText, mTextLength, mDocumentURL,
                                script->mLineNo, doc, mPrototype);
        }

        FlushText(PR_FALSE);
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
            NS_STATIC_CAST(nsXULPrototypeElement*, node);

        mPrototype->SetRootElement(element);
        mState = eInEpilog;
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleComment(const PRUnichar *aName)
{
   FlushText();
   return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleCDataSection(const PRUnichar *aData, PRUint32 aLength)
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
XULContentSinkImpl::HandleCharacterData(const PRUnichar *aData, 
                                        PRUint32 aLength)
{
  if (aData && mState != eInProlog && mState != eInEpilog) {
    return AddText(aData, aLength);
  }
  return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                                const PRUnichar *aData)
{
    FlushText();

    const nsDependentString target(aTarget);
    const nsDependentString data(aData);

    // Note: the created nsXULPrototypePI has mRefCnt == 1
    nsXULPrototypePI* pi = new nsXULPrototypePI();
    if (!pi)
        return NS_ERROR_OUT_OF_MEMORY;

    pi->mTarget = target;
    pi->mData = data;

    if (mState == eInProlog) {
        // Note: passing in already addrefed pi
        return mPrototype->AddProcessingInstruction(pi);
    }

    nsresult rv;
    nsVoidArray* children;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_FAILED(rv)) {
        pi->Release();
        return rv;
    }

    if (!children->AppendElement(pi)) {
        pi->Release();
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}


NS_IMETHODIMP
XULContentSinkImpl::HandleXMLDeclaration(const PRUnichar *aVersion,
                                         const PRUnichar *aEncoding,
                                         PRInt32 aStandalone)
{
  return NS_OK;
}


NS_IMETHODIMP
XULContentSinkImpl::ReportError(const PRUnichar* aErrorText, 
                                const PRUnichar* aSourceText,
                                nsIScriptError *aError,
                                PRBool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

  // The expat driver should report the error.
  *_retval = PR_TRUE;

  nsresult rv = NS_OK;

  // make sure to empty the context stack so that
  // <parsererror> could become the root element.
  while (mContextStack.Depth()) {
    nsVoidArray* children;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_SUCCEEDED(rv)) {
      for (PRInt32 i = children->Count() - 1; i >= 0; --i) {
        nsXULPrototypeNode* child =
            NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

        delete child;
      }
    }

    State state;
    mContextStack.Pop(&state);
  }

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

  const PRUnichar* noAtts[] = { 0, 0 };

  NS_NAMED_LITERAL_STRING(errorNs,
                          "http://www.mozilla.org/newlayout/xml/parsererror.xml");

  nsAutoString parsererror(errorNs);
  parsererror.Append((PRUnichar)0xFFFF);
  parsererror.AppendLiteral("parsererror");
  
  rv = HandleStartElement(parsererror.get(), noAtts, 0, -1, 0);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = HandleCharacterData(aErrorText, nsCRT::strlen(aErrorText));
  NS_ENSURE_SUCCESS(rv,rv);  
  
  nsAutoString sourcetext(errorNs);
  sourcetext.Append((PRUnichar)0xFFFF);
  sourcetext.AppendLiteral("sourcetext");

  rv = HandleStartElement(sourcetext.get(), noAtts, 0, -1, 0);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleCharacterData(aSourceText, nsCRT::strlen(aSourceText));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleEndElement(sourcetext.get());
  NS_ENSURE_SUCCESS(rv,rv); 
  
  rv = HandleEndElement(parsererror.get());
  NS_ENSURE_SUCCESS(rv,rv);

  return rv;
}

nsresult
XULContentSinkImpl::SetElementScriptType(nsXULPrototypeElement* element,
                                         const PRUnichar** aAttributes, 
                                         const PRUint32 aAttrLen)
{
    // First check if the attributes specify an explicit script type.
    nsresult rv = NS_OK;
    PRUint32 i;
    PRBool found = PR_FALSE;
    for (i=0;i<aAttrLen;i++) {
        const nsDependentString key(aAttributes[i*2]);
        if (key.EqualsLiteral("script-type")) {
            const nsDependentString value(aAttributes[i*2+1]);
            if (!value.IsEmpty()) {
                nsCOMPtr<nsIScriptRuntime> runtime;
                rv = NS_GetScriptRuntime(value, getter_AddRefs(runtime));
                if (NS_SUCCEEDED(rv))
                    element->mScriptTypeID = runtime->GetScriptTypeID();
                else {
                    // probably just a bad language name (typo, etc)
                    NS_WARNING("Failed to load the node's script language!");
                    // Leave the default language as unknown - we don't want js
                    // trying to execute this stuff.
                    NS_ASSERTION(element->mScriptTypeID == nsIProgrammingLanguage::UNKNOWN,
                                 "Default script type should be unknown");
                }
                found = PR_TRUE;
                break;
            }
        }
    }
    // If not specified, look at the context stack and use the element
    // there.
    if (!found) {
        if (mContextStack.Depth() == 0) {
            // This is the root element - default to JS
            element->mScriptTypeID = nsIProgrammingLanguage::JAVASCRIPT;
        } else {
            // Ask the top-node for its script type (which has already
            // had this function called for it - so no need to recurse
            // until we find it)
            rv = mContextStack.GetTopNodeScriptType(&element->mScriptTypeID);
        }
    }
    return rv;
}

nsresult
XULContentSinkImpl::OpenRoot(const PRUnichar** aAttributes, 
                             const PRUint32 aAttrLen, 
                             nsINodeInfo *aNodeInfo)
{
    NS_ASSERTION(mState == eInProlog, "how'd we get here?");
    if (mState != eInProlog)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    if (aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML) || 
        aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XUL)) {
        PR_LOG(gLog, PR_LOG_ERROR,
               ("xul: script tag not allowed as root content element"));

        return NS_ERROR_UNEXPECTED;
    }

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ERROR)) {
            nsAutoString anodeC;
            aNodeInfo->GetName(anodeC);
            PR_LOG(gLog, PR_LOG_ERROR,
                   ("xul: unable to create element '%s' at line %d",
                    NS_ConvertUTF16toUTF8(anodeC).get(),
                    -1)); // XXX pass in line number
        }
#endif

        return rv;
    }

    // Set the correct script-type for the element.
    rv = SetElementScriptType(element, aAttributes, aAttrLen);
    if (NS_FAILED(rv)) return rv;

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) {
        delete element;
        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aAttributes, aAttrLen, element);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenTag(const PRUnichar** aAttributes, 
                            const PRUint32 aAttrLen,
                            const PRUint32 aLineNumber,
                            nsINodeInfo *aNodeInfo)
{
    nsresult rv;

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ERROR)) {
            nsAutoString anodeC;
            aNodeInfo->GetName(anodeC);
            PR_LOG(gLog, PR_LOG_ERROR,
                   ("xul: unable to create element '%s' at line %d",
                    NS_ConvertUTF16toUTF8(anodeC).get(),
                    aLineNumber));
        }
#endif

        return rv;
    }

    // Link this element to its parent.
    nsVoidArray* children;
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
        // Do scripty things now.  Set a script language for the element,
        // even though it is ignored (the nsPrototypeScriptElement
        // has its own script-type).
        element->mScriptTypeID = nsIProgrammingLanguage::JAVASCRIPT;
        // OpenScript will push the nsPrototypeScriptElement onto the 
        // stack, so we're done after this.
        return OpenScript(aAttributes, aLineNumber);
    }

    // Set the correct script-type for the element.
    rv = SetElementScriptType(element, aAttributes, aAttrLen);
    if (NS_FAILED(rv)) return rv;

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenScript(const PRUnichar** aAttributes,
                               const PRUint32 aLineNumber)
{
  PRUint32 langID;
  nsresult rv = mContextStack.GetTopNodeScriptType(&langID);
  if (NS_FAILED(rv)) return rv;
  PRUint32 version = 0;

  // Look for SRC attribute and look for a LANGUAGE attribute
  nsAutoString src;
  while (*aAttributes) {
      const nsDependentString key(aAttributes[0]);
      if (key.EqualsLiteral("src")) {
          src.Assign(aAttributes[1]);
      }
      else if (key.EqualsLiteral("type")) {
          nsCOMPtr<nsIMIMEHeaderParam> mimeHdrParser =
              do_GetService("@mozilla.org/network/mime-hdrparam;1");
          NS_ENSURE_TRUE(mimeHdrParser, NS_ERROR_FAILURE);

          NS_ConvertUTF16toUTF8 typeAndParams(aAttributes[1]);

          nsAutoString mimeType;
          rv = mimeHdrParser->GetParameter(typeAndParams, nsnull,
                                           EmptyCString(), PR_FALSE, nsnull,
                                           mimeType);
          NS_ENSURE_SUCCESS(rv, rv);

          // Javascript keeps the fast path, optimized for most-likely type
          // Table ordered from most to least likely JS MIME types. For .xul
          // files that we host, the likeliest type is application/x-javascript.
          // See bug 62485, feel free to add <script type="..."> survey data to it,
          // or to a new bug once 62485 is closed.
          static const char *jsTypes[] = {
              "application/x-javascript",
              "text/javascript",
              "text/ecmascript",
              "application/javascript",
              "application/ecmascript",
              nsnull
          };

          PRBool isJavaScript = PR_FALSE;
          for (PRInt32 i = 0; jsTypes[i]; i++) {
              if (mimeType.LowerCaseEqualsASCII(jsTypes[i])) {
                  isJavaScript = PR_TRUE;
                  break;
              }
          }

          if (isJavaScript) {
              langID = nsIProgrammingLanguage::JAVASCRIPT;
          } else {
              // Use the script object factory to locate the language.
              nsCOMPtr<nsIScriptRuntime> runtime;
              rv = NS_GetScriptRuntime(mimeType, getter_AddRefs(runtime));
              if (NS_FAILED(rv) || runtime == nsnull) {
                  // Failed to get the explicitly specified language
                  NS_WARNING("Failed to find a scripting language");
                  langID = nsIProgrammingLanguage::UNKNOWN;
              } else
                  langID = runtime->GetScriptTypeID();
          }

          if (langID != nsIProgrammingLanguage::UNKNOWN) {
            // Get the version string, and ensure the language supports it.
            nsAutoString versionName;
            rv = mimeHdrParser->GetParameter(typeAndParams, "version",
                                             EmptyCString(), PR_FALSE, nsnull,
                                             versionName);
            if (NS_FAILED(rv)) {
              // no version specified - version remains 0.
                  if (rv != NS_ERROR_INVALID_ARG)
                      return rv;
              } else {
                nsCOMPtr<nsIScriptRuntime> runtime;
                rv = NS_GetScriptRuntimeByID(langID, getter_AddRefs(runtime));
                if (NS_FAILED(rv))
                    return rv;
                rv = runtime->ParseVersion(versionName, &version);
                if (NS_FAILED(rv)) {
                    NS_WARNING("This script language version is not supported - ignored");
                    langID = nsIProgrammingLanguage::UNKNOWN;
                  }
              }
          }
          // Some js specifics yet to be abstracted.
          if (langID == nsIProgrammingLanguage::JAVASCRIPT) {
              // By default scripts in XUL documents have E4X turned on. We use
              // our implementation knowledge to reuse JSVERSION_HAS_XML as a
              // safe version flag. This is still OK if version is
              // JSVERSION_UNKNOWN (-1),
              version |= JSVERSION_HAS_XML;

              nsAutoString value;
              rv = mimeHdrParser->GetParameter(typeAndParams, "e4x",
                                               EmptyCString(), PR_FALSE, nsnull,
                                               value);
              if (NS_FAILED(rv)) {
                  if (rv != NS_ERROR_INVALID_ARG)
                      return rv;
              } else {
                  if (value.Length() == 1 && value[0] == '0')
                    version &= ~JSVERSION_HAS_XML;
              }
          }
      }
      else if (key.EqualsLiteral("language")) {
          // Language is deprecated, and the impl in nsScriptLoader ignores the
          // various version strings anyway.  So we make no attempt to support
          // languages other than JS for language=
          nsAutoString lang(aAttributes[1]);
          if (nsParserUtils::IsJavaScriptLanguage(lang, &version)) {
              langID = nsIProgrammingLanguage::JAVASCRIPT;

              // Even when JS version < 1.6 is specified, E4X is
              // turned on in XUL.
              version |= JSVERSION_HAS_XML;
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
      nsIScriptGlobalObject* globalObject = nsnull; // borrowed reference
      if (doc)
          globalObject = doc->GetScriptGlobalObject();
      nsXULPrototypeScript* script =
          new nsXULPrototypeScript(langID, aLineNumber, version);
      if (! script)
          return NS_ERROR_OUT_OF_MEMORY;

      // If there is a SRC attribute...
      if (! src.IsEmpty()) {
          // Use the SRC attribute value to load the URL
          rv = NS_NewURI(getter_AddRefs(script->mSrcURI), src, nsnull, mDocumentURL);

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
              delete script;
              return rv;
          }

          // Attempt to deserialize an out-of-line script from the FastLoad
          // file right away.  Otherwise we'll end up reloading the script and
          // corrupting the FastLoad file trying to serialize it, in the case
          // where it's already there.
          if (globalObject)
                script->DeserializeOutOfLine(nsnull, globalObject);
      }

      nsVoidArray* children;
      rv = mContextStack.GetTopChildren(&children);
      if (NS_FAILED(rv)) {
          delete script;
          return rv;
      }

      children->AppendElement(script);

      mConstrainSize = PR_FALSE;

      mContextStack.Push(script, mState);
      mState = eInScript;
  }

  return NS_OK;
}

nsresult
XULContentSinkImpl::AddAttributes(const PRUnichar** aAttributes, 
                                  const PRUint32 aAttrLen, 
                                  nsXULPrototypeElement* aElement)
{
  // Add tag attributes to the element
  nsresult rv;

  // Create storage for the attributes
  nsXULPrototypeAttribute* attrs = nsnull;
  if (aAttrLen > 0) {
    attrs = new nsXULPrototypeAttribute[aAttrLen];
    if (! attrs)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  aElement->mAttributes    = attrs;
  aElement->mNumAttributes = aAttrLen;

  // Copy the attributes into the prototype
  PRUint32 i;
  for (i = 0; i < aAttrLen; ++i) {
      rv = NormalizeAttributeString(aAttributes[i * 2], attrs[i].mName);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aElement->SetAttrAt(i, nsDependentString(aAttributes[i * 2 + 1]),
                               mDocumentURL);
      NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
      if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
          nsAutoString extraWhiteSpace;
          PRInt32 cnt = mContextStack.Depth();
          while (--cnt >= 0)
              extraWhiteSpace.AppendLiteral("  ");
          nsAutoString qnameC,valueC;
          qnameC.Assign(aAttributes[0]);
          valueC.Assign(aAttributes[1]);
          PR_LOG(gLog, PR_LOG_DEBUG,
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
XULContentSinkImpl::AddText(const PRUnichar* aText, 
                            PRInt32 aLength)
{
  // Create buffer when we first need it
  if (0 == mTextSize) {
      mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * 4096);
      if (nsnull == mText) {
          return NS_ERROR_OUT_OF_MEMORY;
      }
      mTextSize = 4096;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
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
    memcpy(&mText[mTextLength],aText + offset, sizeof(PRUnichar) * amount);
    
    mTextLength += amount;
    offset += amount;
    aLength -= amount;
  }

  return NS_OK;
}
