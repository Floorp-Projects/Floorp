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
#include "nsIServiceManager.h"
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

#include "nsIJSContextStack.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"

static const char *sJSStackContractID = "@mozilla.org/js/xpc/ContextStack;1";

nsIDOMScriptObjectFactory *nsContentUtils::sDOMScriptObjectFactory = nsnull;
nsIXPConnect *nsContentUtils::sXPConnect = nsnull;

// static
nsresult
nsContentUtils::Init()
{
  NS_ENSURE_TRUE(!sXPConnect, NS_ERROR_ALREADY_INITIALIZED);

  nsresult rv = nsServiceManager::GetService(nsIXPConnect::GetCID(),
                                             nsIXPConnect::GetIID(),
                                             (nsISupports **)&sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
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
    CopyNormalizeNewlines(OutputIterator* aDestination,PRBool aLastCharCR=PR_FALSE) : 
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
}

// static
nsISupports *
nsContentUtils::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  if (!sDOMScriptObjectFactory) {
    static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                         NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

    nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                 NS_GET_IID(nsIDOMScriptObjectFactory),
                                 (nsISupports **)&sDOMScriptObjectFactory);

    if (!sDOMScriptObjectFactory) {
      return nsnull;
    }
  }

  return sDOMScriptObjectFactory->GetClassInfoInstance(aID);
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

PRBool
nsContentUtils::IsCallerChrome()
{
  nsCOMPtr<nsIDocShell> docShell;
  nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));

  if (stack) {
    JSContext *cx = nsnull;
    stack->Peek(&cx);

    if (cx) {
      nsCOMPtr<nsIScriptGlobalObject> sgo;
      nsContentUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(sgo));

      if (sgo) {
        sgo->GetDocShell(getter_AddRefs(docShell));
      }
    }
  }

  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(docShell));
  if (item) {
    PRInt32 callerType = nsIDocShellTreeItem::typeChrome;
    item->GetItemType(&callerType);

    if (callerType != nsIDocShellTreeItem::typeChrome) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
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

  nsAutoVoidArray nodeArray;
  nsresult rv = GetFirstDifferentAncestors(aNode, aOther, &nodeArray);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDOMNode *common =
    NS_STATIC_CAST(nsIDOMNode*, nodeArray.ElementAt(0));

  NS_ASSERTION(common, "The common ancestor is null!  Very bad!");

  *aCommonAncestor = common;
  NS_IF_ADDREF(*aCommonAncestor);

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetFirstDifferentAncestors(nsIDOMNode *aNode,
                                           nsIDOMNode *aOther,
                                           nsVoidArray* aDifferentNodes)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aOther);

  if (aDifferentNodes->Count() != 0) {
    NS_WARNING("The aDifferentNodes array passed in is not empty!");
    aDifferentNodes->Clear();
  }

  // Test if both are the same node.
  if (aNode == aOther) {
    aDifferentNodes->AppendElement(NS_STATIC_CAST(void*, aNode));
    return NS_OK;
  }

  nsAutoVoidArray nodeAncestors;
  nsAutoVoidArray otherAncestors;

  // Insert all the ancestors of |aNode|
  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> ancestor(node);
  do {
    nodeAncestors.AppendElement(node.get());
    node->GetParentNode(getter_AddRefs(ancestor));
    if (ancestor == aOther) {
      aDifferentNodes->AppendElement(NS_STATIC_CAST(void*, aOther));
      return NS_OK;
    }
    node = ancestor;
  } while (ancestor);

  // Insert all the ancestors of |aOther|
  nsCOMPtr<nsIDOMNode> other(aOther);
  ancestor = other;
  do {
    otherAncestors.AppendElement(other.get());
    other->GetParentNode(getter_AddRefs(ancestor));
    if (ancestor == aNode) {
      aDifferentNodes->AppendElement(NS_STATIC_CAST(void*, aNode));
      return NS_OK;
    }
    other = ancestor;
  } while (ancestor);

  PRInt32 nodeIdx  = nodeAncestors.Count() - 1;
  PRInt32 otherIdx = otherAncestors.Count() - 1;

  if (nodeAncestors.ElementAt(nodeIdx) != otherAncestors.ElementAt(otherIdx)) {
    NS_ERROR("This function was called on two disconnected nodes!");
    return NS_ERROR_FAILURE;
  }

  // Go back through the ancestors, starting from the root,
  // until the first different ancestor found.
  do {
    --nodeIdx;
    --otherIdx;
  } while (nodeAncestors.ElementAt(nodeIdx) == otherAncestors.ElementAt(otherIdx));

  NS_ASSERTION(nodeIdx >= 0 && otherIdx >= 0,
               "Something's wrong: our indices should not be negative here!");

  aDifferentNodes->AppendElement(nodeAncestors.ElementAt(nodeIdx + 1));
  aDifferentNodes->AppendElement(nodeAncestors.ElementAt(nodeIdx));
  aDifferentNodes->AppendElement(otherAncestors.ElementAt(otherIdx));

  return NS_OK;
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
