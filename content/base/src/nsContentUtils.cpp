/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com>
 *   Christopher A. Aillon <christopher@aillon.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* A namespace class for static layout utilities. */

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsReadableUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIDOMWindowInternal.h"
#include "nsIJSContextStack.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsParserCIID.h"
#include "nsIParserService.h"
#include "nsIServiceManager.h"

static const char *kJSStackContractID = "@mozilla.org/js/xpc/ContextStack;1";
static NS_DEFINE_IID(kParserServiceCID, NS_PARSERSERVICE_CID);

nsIDOMScriptObjectFactory *nsContentUtils::sDOMScriptObjectFactory = nsnull;
nsIXPConnect *nsContentUtils::sXPConnect = nsnull;
nsIScriptSecurityManager *nsContentUtils::sSecurityManager = nsnull;
nsIThreadJSContextStack *nsContentUtils::sThreadJSContextStack = nsnull;
nsIParserService *nsContentUtils::sParserService = nsnull;
nsINameSpaceManager *nsContentUtils::sNameSpaceManager = nsnull;

// static
nsresult
nsContentUtils::Init()
{
  NS_ENSURE_TRUE(!sXPConnect, NS_ERROR_ALREADY_INITIALIZED);

  nsresult rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                      &sSecurityManager);
  if (NS_FAILED(rv)) {
    // We can run without a security manager, so don't return early.
    sSecurityManager = nsnull;
  }

  rv = CallGetService(kJSStackContractID, &sThreadJSContextStack);
  if (NS_FAILED(rv)) {
    sThreadJSContextStack = nsnull;

    return rv;
  }

  return NS_GetNameSpaceManager(&sNameSpaceManager);
}

/**
 * Access a cached parser service. Don't addref. We need only one
 * reference to it and this class has that one.
 */
/* static */
nsIParserService*
nsContentUtils::GetParserServiceWeakRef()
{
  // XXX: This isn't accessed from several threads, is it?
  if (sParserService == nsnull) {
    // Lock, recheck sCachedParserService and aquire if this should be
    // safe for multiple threads.
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    
    if (NS_FAILED(rv))
      return nsnull;

    // This addrefs the service for us and it will be released in
    // |Shutdown|.
    mgr->GetService(kParserServiceCID,
                    NS_GET_IID(nsIParserService),
                    NS_REINTERPRET_CAST(void**, &sParserService));
  }

  return sParserService;
}

// static
nsresult
nsContentUtils::GetStaticScriptGlobal(JSContext* aContext, JSObject* aObj,
                                      nsIScriptGlobalObject** aNativeGlobal)
{
  if (!sXPConnect) {
    *aNativeGlobal = nsnull;

    return NS_OK;
  }

  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return NS_ERROR_FAILURE;

  while (nsnull != (parent = JS_GetParent(aContext, glob))) {
    glob = parent;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native;

  nsresult rv =
    sXPConnect->GetWrappedNativeOfJSObject(aContext, glob,
                                           getter_AddRefs(wrapped_native));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> native;
  rv = wrapped_native->GetNative(getter_AddRefs(native));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(native, aNativeGlobal);
}

//static
nsresult
nsContentUtils::GetStaticScriptContext(JSContext* aContext,
                                       JSObject* aObj,
                                       nsIScriptContext** aScriptContext)
{
  nsCOMPtr<nsIScriptGlobalObject> nativeGlobal;
  GetStaticScriptGlobal(aContext, aObj, getter_AddRefs(nativeGlobal));
  if (!nativeGlobal)
    return NS_ERROR_FAILURE;
  nsIScriptContext* scriptContext = nsnull;
  nativeGlobal->GetContext(&scriptContext);
  *aScriptContext = scriptContext;
  return scriptContext ? NS_OK : NS_ERROR_FAILURE;
}

//static
nsresult
nsContentUtils::GetDynamicScriptGlobal(JSContext* aContext,
                                       nsIScriptGlobalObject** aNativeGlobal)
{
  nsCOMPtr<nsIScriptContext> scriptCX;
  GetDynamicScriptContext(aContext, getter_AddRefs(scriptCX));
  if (!scriptCX) {
    *aNativeGlobal = nsnull;
    return NS_ERROR_FAILURE;
  }
  return scriptCX->GetGlobalObject(aNativeGlobal);
}

//static
nsresult
nsContentUtils::GetDynamicScriptContext(JSContext *aContext,
                                        nsIScriptContext** aScriptContext)
{
  *aScriptContext = nsnull;

  // XXX We rely on the rule that if any JSContext in our JSRuntime has a
  // private set then that private *must* be a pointer to an nsISupports.
  nsISupports *supports = (nsIScriptContext*)JS_GetContextPrivate(aContext);
  if (!supports) {
      return NS_OK;
  }

  return CallQueryInterface(supports, aScriptContext);
}

template <class OutputIterator>
struct NormalizeNewlinesCharTraits {
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    NormalizeNewlinesCharTraits(OutputIterator& aIterator) : mIterator(aIterator) { }
    void writechar(typename OutputIterator::value_type aChar) {
      *mIterator++ = aChar;
    }

  private:
    OutputIterator mIterator;
};

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct NormalizeNewlinesCharTraits<CharT*> {
  public:
    typedef CharT value_type;

  public:
    NormalizeNewlinesCharTraits(CharT* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(CharT aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    CharT* mCharPtr;
};

#else

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<char*> {
  public:
    typedef char value_type;

  public:
    NormalizeNewlinesCharTraits(char* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(char aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    char* mCharPtr;
};

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<PRUnichar*> {
  public:
    typedef PRUnichar value_type;

  public:
    NormalizeNewlinesCharTraits(PRUnichar* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(PRUnichar aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    PRUnichar* mCharPtr;
};

#endif

template <class OutputIterator>
class CopyNormalizeNewlines
{
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    CopyNormalizeNewlines(OutputIterator* aDestination,
                          PRBool aLastCharCR=PR_FALSE) :
      mLastCharCR(aLastCharCR),
      mDestination(aDestination),
      mWritten(0)
    { }

    PRUint32 GetCharsWritten() {
      return mWritten;
    }

    PRBool IsLastCharCR() {
      return mLastCharCR;
    }

    PRUint32 write(const typename OutputIterator::value_type* aSource, PRUint32 aSourceLength) {

      const typename OutputIterator::value_type* done_writing = aSource + aSourceLength;

      // If the last source buffer ended with a CR...
      if (mLastCharCR) {
        // ..and if the next one is a LF, then skip it since
        // we've already written out a newline
        if (aSourceLength && (*aSource == value_type('\n'))) {
          ++aSource;
        }
        mLastCharCR = PR_FALSE;
      }

      PRUint32 num_written = 0;
      while ( aSource < done_writing ) {
        if (*aSource == value_type('\r')) {
          mDestination->writechar('\n');
          ++aSource;
          // If we've reached the end of the buffer, record
          // that we wrote out a CR
          if (aSource == done_writing) {
            mLastCharCR = PR_TRUE;
          }
          // If the next character is a LF, skip it
          else if (*aSource == value_type('\n')) {
            ++aSource;
          }
        }
        else {
          mDestination->writechar(*aSource++);
        }
        ++num_written;
      }

      mWritten += num_written;
      return aSourceLength;
    }

  private:
    PRBool mLastCharCR;
    OutputIterator* mDestination;
    PRUint32 mWritten;
};

// static
PRUint32
nsContentUtils::CopyNewlineNormalizedUnicodeTo(const nsAString& aSource,
                                               PRUint32 aSrcOffset,
                                               PRUnichar* aDest,
                                               PRUint32 aLength,
                                               PRBool& aLastCharCR)
{
  typedef NormalizeNewlinesCharTraits<PRUnichar*> sink_traits;

  sink_traits dest_traits(aDest);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits,aLastCharCR);
  nsReadingIterator<PRUnichar> fromBegin, fromEnd;
  copy_string(aSource.BeginReading(fromBegin).advance( PRInt32(aSrcOffset) ),
              aSource.BeginReading(fromEnd).advance( PRInt32(aSrcOffset+aLength) ),
              normalizer);
  aLastCharCR = normalizer.IsLastCharCR();
  return normalizer.GetCharsWritten();
}

// static
PRUint32
nsContentUtils::CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAString& aDest)
{
  typedef nsWritingIterator<PRUnichar> WritingIterator;
  typedef NormalizeNewlinesCharTraits<WritingIterator> sink_traits;

  WritingIterator iter;
  aDest.BeginWriting(iter);
  sink_traits dest_traits(iter);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits);
  copy_string(aSrcStart, aSrcEnd, normalizer);
  return normalizer.GetCharsWritten();
}

// static
void
nsContentUtils::Shutdown()
{
  NS_IF_RELEASE(sDOMScriptObjectFactory);
  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecurityManager);
  NS_IF_RELEASE(sThreadJSContextStack);
  NS_IF_RELEASE(sNameSpaceManager);
  NS_IF_RELEASE(sParserService);
}

// static
nsISupports *
nsContentUtils::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  if (!sDOMScriptObjectFactory) {
    static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                         NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

    CallGetService(kDOMScriptObjectFactoryCID, &sDOMScriptObjectFactory);

    if (!sDOMScriptObjectFactory) {
      return nsnull;
    }
  }

  return sDOMScriptObjectFactory->GetClassInfoInstance(aID);
}

/**
 * Checks whether two nodes come from the same origin. aTrustedNode is
 * considered 'safe' in that a user can operate on it and that it isn't
 * a js-object that implements nsIDOMNode.
 * Never call this function with the first node provided by script, it
 * must always be known to be a 'real' node!
 */
// static
nsresult
nsContentUtils::CheckSameOrigin(nsIDOMNode *aTrustedNode,
                                nsIDOMNode *aUnTrustedNode)
{
  NS_PRECONDITION(aTrustedNode, "There must be a trusted node");

  /*
   * Get hold of each node's document or principal
   */

  // In most cases this is a document, so lets try that first
  nsCOMPtr<nsIDocument> trustedDoc = do_QueryInterface(aTrustedNode);
  nsCOMPtr<nsIPrincipal> trustedPrincipal;

  if (!trustedDoc) {
#ifdef DEBUG
    nsCOMPtr<nsIContent> trustCont = do_QueryInterface(aTrustedNode);
    NS_ASSERTION(trustCont,
                 "aTrustedNode is neither nsIContent nor nsIDocument!");
#endif
    nsCOMPtr<nsIDOMDocument> domDoc;
    aTrustedNode->GetOwnerDocument(getter_AddRefs(domDoc));
    if (!domDoc) {
      // In theory this should never happen. But since theory and reality are
      // different for XUL elements we'll try to get the principal from the
      // nsINodeInfoManager.

      nsCOMPtr<nsIContent> cont = do_QueryInterface(aTrustedNode);
      NS_ENSURE_TRUE(cont, NS_ERROR_UNEXPECTED);
      
      nsCOMPtr<nsINodeInfo> ni;
      cont->GetNodeInfo(*getter_AddRefs(ni));
      NS_ENSURE_TRUE(ni, NS_ERROR_UNEXPECTED);
      
      ni->GetDocumentPrincipal(getter_AddRefs(trustedPrincipal));
      
      if (!trustedPrincipal) {
        // Can't get principal of aTrustedNode so we can't check security
        // against it

        return NS_ERROR_UNEXPECTED;
      }
    }
    else {
      trustedDoc = do_QueryInterface(domDoc);
      NS_ASSERTION(trustedDoc, "QI to nsIDocument failed");
    }
  }


  // For performance reasons it's important to try to QI the node to
  // nsIContent before trying to QI to nsIDocument since a QI miss on
  // a node is potentially expensive.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aUnTrustedNode);

  nsCOMPtr<nsIDocument> unTrustedDoc;
  nsCOMPtr<nsIPrincipal> unTrustedPrincipal;

  if (!content) {
    unTrustedDoc = do_QueryInterface(aUnTrustedNode);

    if (!unTrustedDoc) {
      // aUnTrustedNode is neither a nsIContent nor an nsIDocument, something
      // weird is going on...

      NS_ERROR("aUnTrustedNode is neither an nsIContent nor an nsIDocument!");

      return NS_ERROR_UNEXPECTED;
    }
  }
  else {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aUnTrustedNode->GetOwnerDocument(getter_AddRefs(domDoc));
    if (!domDoc) {
      // if we can't get a doc then lets try to get principal through nodeinfo
      // manager
      nsCOMPtr<nsINodeInfo> ni;
      content->GetNodeInfo(*getter_AddRefs(ni));
      if (!ni) {
        // we can't get to the principal so we'll give up and give the caller
        // access

        return NS_OK;
      }
      
      ni->GetDocumentPrincipal(getter_AddRefs(unTrustedPrincipal));

      if (!unTrustedPrincipal) {
        // we can't get to the principal so we'll give up and give the caller access

        return NS_OK;
      }
    }
    else {
      unTrustedDoc = do_QueryInterface(domDoc);
      NS_ASSERTION(unTrustedDoc, "QI to nsIDocument failed");
    }
  }

  /*
   * Compare the principals
   */

  // If they are in the same document then everything is just fine
  if (trustedDoc == unTrustedDoc && trustedDoc)
    return NS_OK;

  if (!trustedPrincipal) {
    trustedDoc->GetPrincipal(getter_AddRefs(trustedPrincipal));
    if (!trustedPrincipal) {
      // If the trusted node doesn't have a principal we can't check security against it

      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  if (!unTrustedPrincipal) {
    unTrustedDoc->GetPrincipal(getter_AddRefs(unTrustedPrincipal));
    if (!unTrustedDoc) {
      // We can't get hold of the principal for this node. This should happen
      // very rarely, like for textnodes out of the tree and <option>s created
      // using 'new Option'.
      // If we didn't allow access to nodes like this you wouldn't be able to
      // insert these nodes into a document.

      return NS_OK;
    }
  }

  // If there isn't a security manager it is probably because it is not
  // installed so we don't care about security anyway
  if (!sSecurityManager) {
    return NS_OK;
  }

  return sSecurityManager->CheckSameOriginPrincipal(trustedPrincipal,
                                                    unTrustedPrincipal);
}

// static
PRBool
nsContentUtils::CanCallerAccess(nsIDOMNode *aNode)
{
  if (!sSecurityManager) {
    // No security manager available, let any calls go through...

    return PR_TRUE;
  }

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  sSecurityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  if (!subjectPrincipal) {
    // we're running as system, grant access to the node.

    return PR_TRUE;
  }

  // Make sure that this is a real node. We do this by first QI'ing to
  // nsIContent (which is important performance wise) and if that QI
  // fails we QI to nsIDocument. If both those QI's fail we won't let
  // the caller access this unknown node.
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aNode);

    if (!doc) {
      // aNode is neither a nsIContent nor an nsIDocument, something
      // weird is going on...

      NS_ERROR("aNode is neither an nsIContent nor an nsIDocument!");

      return PR_FALSE;
    }
    doc->GetPrincipal(getter_AddRefs(principal));
  }
  else {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aNode->GetOwnerDocument(getter_AddRefs(domDoc));
    if (!domDoc) {
      nsCOMPtr<nsINodeInfo> ni;
      content->GetNodeInfo(*getter_AddRefs(ni));
      if (!ni) {
        // aNode is not part of a document, let any caller access it.

        return PR_TRUE;
      }
      
      ni->GetDocumentPrincipal(getter_AddRefs(principal));
    }
    else {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
      NS_ASSERTION(doc, "QI to nsIDocument failed");
      doc->GetPrincipal(getter_AddRefs(principal));
    }
  }

  if (!principal) {
    // We can't get hold of the principal for this node. This should happen
    // very rarely, like for textnodes out of the tree and <option>s created
    // using 'new Option'.
    // If we didn't allow access to nodes like this you wouldn't be able to
    // insert these nodes into a document.

    return PR_TRUE;
  }

  nsresult rv = sSecurityManager->CheckSameOriginPrincipal(subjectPrincipal,
                                                           principal);

  return NS_SUCCEEDED(rv);
}

//static
PRBool
nsContentUtils::InProlog(nsIDOMNode *aNode)
{
  NS_PRECONDITION(aNode, "missing node to nsContentUtils::InProlog");

  // Check that there is an ancestor and that it is a document
  nsCOMPtr<nsIDOMNode> parent;
  aNode->GetParentNode(getter_AddRefs(parent));
  if (!parent) {
    return PR_FALSE;
  }

  PRUint16 type;
  parent->GetNodeType(&type);
  if (type != nsIDOMNode::DOCUMENT_NODE) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(parent);
  NS_ASSERTION(doc, "document doesn't implement nsIDocument");
  nsCOMPtr<nsIContent> cont = do_QueryInterface(aNode);
  NS_ASSERTION(cont, "node doesn't implement nsIContent");

  // Check that there are no elements before aNode to make sure we are not
  // in the epilog
  PRInt32 pos;
  doc->IndexOf(cont, pos);
  while (pos > 0) {
    --pos;
    nsCOMPtr<nsIContent> sibl;
    doc->ChildAt(pos, *getter_AddRefs(sibl));
    if (sibl->IsContentOfType(nsIContent::eELEMENT)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

// static
nsresult
nsContentUtils::doReparentContentWrapper(nsIContent *aChild,
                                         nsIDocument *aNewDocument,
                                         nsIDocument *aOldDocument,
                                         JSContext *cx,
                                         JSObject *parent_obj)
{
  nsCOMPtr<nsIXPConnectJSObjectHolder> old_wrapper;

  nsresult rv;

  rv = sXPConnect->ReparentWrappedNativeIfFound(cx, ::JS_GetGlobalObject(cx),
                                                parent_obj, aChild,
                                                getter_AddRefs(old_wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!old_wrapper) {
    // If aChild isn't wrapped none of it's children are wrapped so
    // there's no need to walk into aChild's children.

    return NS_OK;
  }

  if (aOldDocument) {
    nsCOMPtr<nsISupports> old_ref;

    aOldDocument->RemoveReference(aChild, getter_AddRefs(old_ref));

    if (old_ref) {
      // Transfer the reference from aOldDocument to aNewDocument

      aNewDocument->AddReference(aChild, old_ref);
    }
  }

  JSObject *old;

  rv = old_wrapper->GetJSObject(&old);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> child;
  PRInt32 count = 0, i;

  aChild->ChildCount(count);

  for (i = 0; i < count; i++) {
    aChild->ChildAt(i, *getter_AddRefs(child));
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);

    rv = doReparentContentWrapper(child, aNewDocument, aOldDocument, cx, old);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

static
nsresult GetContextFromDocument(nsIDocument *aDocument, JSContext **cx)
{
  *cx = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  aDocument->GetScriptGlobalObject(getter_AddRefs(sgo));

  if (!sgo) {
    // No script global, no context.

    return NS_OK;
  }

  nsCOMPtr<nsIScriptContext> scx;
  sgo->GetContext(getter_AddRefs(scx));

  if (!scx) {
    // No context left in the old scope...

    return NS_OK;
  }

  *cx = (JSContext *)scx->GetNativeContext();

  return NS_OK;
}

// static
nsresult
nsContentUtils::ReparentContentWrapper(nsIContent *aContent,
                                       nsIContent *aNewParent,
                                       nsIDocument *aNewDocument,
                                       nsIDocument *aOldDocument)
{
  if (!aNewDocument || aNewDocument == aOldDocument) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> old_doc(aOldDocument);

  if (!old_doc) {
    nsCOMPtr<nsINodeInfo> ni;

    aContent->GetNodeInfo(*getter_AddRefs(ni));

    if (ni) {
      ni->GetDocument(*getter_AddRefs(old_doc));
    }

    if (!aOldDocument) {
      // If we can't find our old document we don't know what our old
      // scope was so there's no way to find the old wrapper

      return NS_OK;
    }
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISupports> new_parent;

  if (!aNewParent) {
    nsCOMPtr<nsIContent> root;
    old_doc->GetRootContent(getter_AddRefs(root));

    if (root.get() == aContent) {
      new_parent = old_doc;
    }
  } else {
    new_parent = aNewParent;
  }

  JSContext *cx = nsnull;

  GetContextFromDocument(old_doc, &cx);

  if (!cx) {
    // No JSContext left in the old scope, can't find the old wrapper
    // w/o the old context.

    return NS_OK;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;

  nsresult rv;

  rv = sXPConnect->GetWrappedNativeOfNativeObject(cx, ::JS_GetGlobalObject(cx),
                                                  aContent,
                                                  NS_GET_IID(nsISupports),
                                                  getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!wrapper) {
    // aContent is not wrapped (and thus none of it's children are
    // wrapped) so there's no need to reparent anything.

    return NS_OK;
  }

  // Wrap the new parent and reparent aContent

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  rv = sXPConnect->WrapNative(cx, ::JS_GetGlobalObject(cx), new_parent,
                              NS_GET_IID(nsISupports),
                              getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *obj;
  rv = holder->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  return doReparentContentWrapper(aContent, aNewDocument, aOldDocument, cx,
                                  obj);
}

void
nsContentUtils::GetDocShellFromCaller(nsIDocShell** aDocShell)
{
  *aDocShell = nsnull;
  if (!sThreadJSContextStack) {
    return;
  }

  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  if (cx) {
    nsCOMPtr<nsIScriptGlobalObject> sgo;
    GetDynamicScriptGlobal(cx, getter_AddRefs(sgo));

    if (sgo) {
      sgo->GetDocShell(aDocShell);
    }
  }
}

void
nsContentUtils::GetDocumentFromCaller(nsIDOMDocument** aDocument)
{
  *aDocument = nsnull;
  if (!sThreadJSContextStack) {
    return;
  }

  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  if (cx) {
    nsCOMPtr<nsIScriptGlobalObject> sgo;
    GetDynamicScriptGlobal(cx, getter_AddRefs(sgo));

    nsCOMPtr<nsIDOMWindowInternal> win(do_QueryInterface(sgo));
    if (!win) {
      return;
    }

    win->GetDocument(aDocument);
  }
}

PRBool
nsContentUtils::IsCallerChrome()
{
  if (!sSecurityManager) {
    return PR_FALSE;
  }

  PRBool is_caller_chrome = PR_FALSE;
  nsresult rv = sSecurityManager->SubjectPrincipalIsSystem(&is_caller_chrome);
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  return is_caller_chrome;
}

// static
PRBool
nsContentUtils::InSameDoc(nsIDOMNode* aNode, nsIDOMNode* aOther)
{
  if (!aNode || !aOther) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsCOMPtr<nsIContent> other(do_QueryInterface(aOther));

  if (content && other) {
    nsCOMPtr<nsIDocument> contentDoc;
    nsCOMPtr<nsIDocument> otherDoc;
    content->GetDocument(*getter_AddRefs(contentDoc));
    other->GetDocument(*getter_AddRefs(otherDoc));
    // XXXcaa Don't bother to check that either node is in a
    // document.  Editor relies on us returning true if neither
    // node is in a document.  See bug 154401.
    if (contentDoc == otherDoc) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// static
PRBool
nsContentUtils::ContentIsDescendantOf(nsIContent* aPossibleDescendant,
                                      nsIContent* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  nsCOMPtr<nsIContent> parent;
  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return PR_TRUE;
    aPossibleDescendant->GetParent(*getter_AddRefs(parent));
    aPossibleDescendant = parent;
  } while (aPossibleDescendant);

  return PR_FALSE;
}


// static
nsresult
nsContentUtils::GetAncestors(nsIDOMNode* aNode,
                             nsVoidArray* aArray)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> ancestor;

  do {
    aArray->AppendElement(node.get());
    node->GetParentNode(getter_AddRefs(ancestor));
    node = ancestor;
  } while (node);

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetAncestorsAndOffsets(nsIDOMNode* aNode,
                                       PRInt32 aOffset,
                                       nsVoidArray* aAncestorNodes,
                                       nsVoidArray* aAncestorOffsets)
{
  NS_ENSURE_ARG_POINTER(aNode);

  PRInt32 offset = 0;
  nsCOMPtr<nsIContent> ancestor;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content) {
    return NS_ERROR_FAILURE;
  }

  if (aAncestorNodes->Count() != 0) {
    NS_WARNING("aAncestorNodes is not empty");
    aAncestorNodes->Clear();
  }

  if (aAncestorOffsets->Count() != 0) {
    NS_WARNING("aAncestorOffsets is not empty");
    aAncestorOffsets->Clear();
  }

  // insert the node itself
  aAncestorNodes->AppendElement(content.get());
  aAncestorOffsets->AppendElement(NS_INT32_TO_PTR(aOffset));

  // insert all the ancestors
  content->GetParent(*getter_AddRefs(ancestor));
  while (ancestor) {
    ancestor->IndexOf(content, offset);
    aAncestorNodes->AppendElement(ancestor.get());
    aAncestorOffsets->AppendElement(NS_INT32_TO_PTR(offset));
    content = ancestor;
    content->GetParent(*getter_AddRefs(ancestor));
  }

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetCommonAncestor(nsIDOMNode *aNode,
                                  nsIDOMNode *aOther,
                                  nsIDOMNode** aCommonAncestor)
{
  *aCommonAncestor = nsnull;

  nsCOMArray<nsIDOMNode> nodeArray;
  nsresult rv = GetFirstDifferentAncestors(aNode, aOther, nodeArray);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDOMNode *common = nodeArray[0];

  NS_ASSERTION(common, "The common ancestor is null!  Very bad!");

  *aCommonAncestor = common;
  NS_IF_ADDREF(*aCommonAncestor);

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetFirstDifferentAncestors(nsIDOMNode *aNode,
                                           nsIDOMNode *aOther,
                                           nsCOMArray<nsIDOMNode>& aDifferentNodes)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aOther);

  if (aDifferentNodes.Count() != 0) {
    NS_WARNING("The aDifferentNodes array passed in is not empty!");
    aDifferentNodes.Clear();
  }

  // Test if both are the same node.
  if (aNode == aOther) {
    aDifferentNodes.AppendObject(aNode);
    return NS_OK;
  }

  nsCOMArray<nsIDOMNode> nodeAncestors;
  nsCOMArray<nsIDOMNode> otherAncestors;

  // Insert all the ancestors of |aNode|
  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> ancestor(node);
  do {
    nodeAncestors.AppendObject(node);
    node->GetParentNode(getter_AddRefs(ancestor));
    if (ancestor == aOther) {
      aDifferentNodes.AppendObject(aOther);
      return NS_OK;
    }
    node = ancestor;
  } while (ancestor);

  // Insert all the ancestors of |aOther|
  nsCOMPtr<nsIDOMNode> other(aOther);
  ancestor = other;
  do {
    otherAncestors.AppendObject(other);
    other->GetParentNode(getter_AddRefs(ancestor));
    if (ancestor == aNode) {
      aDifferentNodes.AppendObject(aNode);
      return NS_OK;
    }
    other = ancestor;
  } while (ancestor);

  PRInt32 nodeIdx  = nodeAncestors.Count() - 1;
  PRInt32 otherIdx = otherAncestors.Count() - 1;

  if (nodeAncestors[nodeIdx] != otherAncestors[otherIdx]) {
    NS_ERROR("This function was called on two disconnected nodes!");
    return NS_ERROR_FAILURE;
  }

  // Go back through the ancestors, starting from the root,
  // until the first different ancestor found.
  do {
    --nodeIdx;
    --otherIdx;
  } while (nodeAncestors[nodeIdx] == otherAncestors[otherIdx]);

  NS_ASSERTION(nodeIdx >= 0 && otherIdx >= 0,
               "Something's wrong: our indices should not be negative here!");

  aDifferentNodes.AppendObject(nodeAncestors[nodeIdx + 1]);
  aDifferentNodes.AppendObject(nodeAncestors[nodeIdx]);
  aDifferentNodes.AppendObject(otherAncestors[otherIdx]);

  return NS_OK;
}

PRUint16
nsContentUtils::ComparePositionWithAncestors(nsIDOMNode *aNode,
                                             nsIDOMNode *aOther)
{
#ifdef DEBUG
  PRUint16 nodeType = 0;
  PRUint16 otherType = 0;
  aNode->GetNodeType(&nodeType);
  aOther->GetNodeType(&otherType);

  NS_PRECONDITION(nodeType != nsIDOMNode::ATTRIBUTE_NODE &&
                  nodeType != nsIDOMNode::DOCUMENT_NODE &&
                  nodeType != nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
                  nodeType != nsIDOMNode::ENTITY_NODE &&
                  nodeType != nsIDOMNode::NOTATION_NODE &&
                  otherType != nsIDOMNode::ATTRIBUTE_NODE &&
                  otherType != nsIDOMNode::DOCUMENT_NODE &&
                  otherType != nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
                  otherType != nsIDOMNode::ENTITY_NODE &&
                  otherType != nsIDOMNode::NOTATION_NODE,
                  "Bad.  Go read the documentation in the header!");
#endif // DEBUG

  PRUint16 mask = 0;

  nsCOMArray<nsIDOMNode> nodeAncestors;

  nsresult rv =
    nsContentUtils::GetFirstDifferentAncestors(aNode, aOther, nodeAncestors);

  if (NS_FAILED(rv)) {
    // If there is no common container node, then the order is based upon
    // order between the root container of each node that is in no container.
    // In this case, the result is disconnected and implementation-dependent.
    mask = (nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED |
            nsIDOMNode::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

    return mask;
  }

  nsIDOMNode* commonAncestor = nodeAncestors[0];

  if (commonAncestor == aNode) {
    mask = (nsIDOMNode::DOCUMENT_POSITION_IS_CONTAINED |
            nsIDOMNode::DOCUMENT_POSITION_FOLLOWING);

    return mask;
  }

  if (commonAncestor == aOther) {
    mask |= (nsIDOMNode::DOCUMENT_POSITION_CONTAINS |
             nsIDOMNode::DOCUMENT_POSITION_PRECEDING);

    return mask;
  }

  // GetFirstDifferentAncestors should only succeed if one of the
  // two nodes is the common ancestor, or if we have a common ancestor
  // and the two first different ancestors.  We checked the case above
  // where one of the two nodes were the common ancestor, so we must
  // have three items in our array now.
  NS_ASSERTION(commonAncestor && nodeAncestors.Count() == 3,
               "Something's wrong");

  nsIDOMNode* nodeAncestor = nodeAncestors[1];
  nsIDOMNode* otherAncestor = nodeAncestors[2];

  if (nodeAncestor && otherAncestor) {
    // Find out which of the two nodes comes first in the document order.
    // First get the children of the common ancestor.
    nsCOMPtr<nsIDOMNodeList> children;
    commonAncestor->GetChildNodes(getter_AddRefs(children));
    PRUint32 numKids;
    children->GetLength(&numKids);
    for (PRUint32 i = 0; i < numKids; ++i) {
      // Then go through the children one at a time to see which we hit first.
      nsCOMPtr<nsIDOMNode> childNode;
      children->Item(i, getter_AddRefs(childNode));
      if (childNode == nodeAncestor) {
        mask |= nsIDOMNode::DOCUMENT_POSITION_FOLLOWING;
        break;
      }

      if (childNode == otherAncestor) {
        mask |= nsIDOMNode::DOCUMENT_POSITION_PRECEDING;
        break;
      }
    }
  }

  return mask;
}

PRUint16
nsContentUtils::ReverseDocumentPosition(PRUint16 aDocumentPosition)
{
  // Disconnected and implementation-specific flags cannot be reversed.
  // Keep them.
  PRUint16 reversedPosition =
    aDocumentPosition & (nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED |
                         nsIDOMNode::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

  // Following/preceding
  if (aDocumentPosition & nsIDOMNode::DOCUMENT_POSITION_FOLLOWING) {
    reversedPosition |= nsIDOMNode::DOCUMENT_POSITION_PRECEDING;
  }
  else if (aDocumentPosition & nsIDOMNode::DOCUMENT_POSITION_PRECEDING) {
    reversedPosition |= nsIDOMNode::DOCUMENT_POSITION_FOLLOWING;
  }

  // Is contained/contains.
  if (aDocumentPosition & nsIDOMNode::DOCUMENT_POSITION_CONTAINS) {
    reversedPosition |= nsIDOMNode::DOCUMENT_POSITION_IS_CONTAINED;
  }
  else if (aDocumentPosition & nsIDOMNode::DOCUMENT_POSITION_IS_CONTAINED) {
    reversedPosition |= nsIDOMNode::DOCUMENT_POSITION_CONTAINS;
  }

  return reversedPosition;
}

inline PRBool
IsCharInSet(const char* aSet,
            const PRUnichar aChar)
{
  PRUnichar ch;
  while ((ch = *aSet)) {
    if (aChar == PRUnichar(ch)) {
      return PR_TRUE;
    }
    ++aSet;
  }
  return PR_FALSE;
}

/**
 * This method strips leading/trailing chars, in given set, from string.
 */
const nsDependentSubstring
nsContentUtils::TrimCharsInSet(const char* aSet,
                               const nsAString& aValue)
{
  nsAString::const_iterator valueCurrent, valueEnd;

  aValue.BeginReading(valueCurrent);
  aValue.EndReading(valueEnd);

  // Skip charaters in the beginning
  while (valueCurrent != valueEnd) {
    if (!IsCharInSet(aSet, *valueCurrent)) {
      break;
    }
    ++valueCurrent;
  }

  if (valueCurrent != valueEnd) {
    for (;;) {
      --valueEnd;
      if (!IsCharInSet(aSet, *valueEnd)) {
        break;
      }
    }
    ++valueEnd; // Step beyond the last character we want in the value.
  }

  // valueEnd should point to the char after the last to copy
  return Substring(valueCurrent, valueEnd);
}

void
nsCxPusher::Push(nsISupports *aCurrentTarget)
{
  if (mScx) {
    NS_ERROR("Whaaa! No double pushing with nsCxPusher::Push()!");

    return;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentTarget));
  nsCOMPtr<nsIDocument> document;

  if (content) {
    content->GetDocument(*getter_AddRefs(document));
  }

  if (!document) {
    document = do_QueryInterface(aCurrentTarget);
  }

  if (document) {
    document->GetScriptGlobalObject(getter_AddRefs(sgo));
  }

  if (!document && !sgo) {
    sgo = do_QueryInterface(aCurrentTarget);
  }

  JSContext *cx = nsnull;

  if (sgo) {
    sgo->GetContext(getter_AddRefs(mScx));

    if (mScx) {
      cx = (JSContext *)mScx->GetNativeContext();
    }
  }

  if (cx) {
    if (!mStack) {
      mStack = do_GetService(kJSStackContractID);
    }

    if (mStack) {
      JSContext *current = nsnull;
      mStack->Peek(&current);

      if (current) {
        // If there's a context on the stack, that means that a script
        // is running at the moment.

        mScriptIsRunning = PR_TRUE;
      }

      mStack->Push(cx);
    }
  } else {
    // If there's no native context in the script context it must be
    // in the process or being torn down. We don't want to notify the
    // script context about scripts having been evaluated in such a
    // case, so null out mScx.

    mScx = nsnull;
  }
}

void
nsCxPusher::Pop()
{
  if (!mScx || !mStack) {
    mScx = nsnull;

    NS_ASSERTION(!mScriptIsRunning, "Huh, this can't be happening, "
                 "mScriptIsRunning can't be set here!");

    return;
  }

  JSContext *unused;
  mStack->Pop(&unused);

  if (!mScriptIsRunning) {
    // No JS is running, but executing the event handler might have
    // caused some JS to run. Tell the script context that it's done.

    mScx->ScriptEvaluated(PR_TRUE);
  }

  mScx = nsnull;
  mScriptIsRunning = PR_FALSE;
}
