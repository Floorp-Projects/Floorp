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

/* A namespace class for static content utilities. */

#ifndef nsContentUtils_h___
#define nsContentUtils_h___

#include "jspubtd.h"
#include "nsAString.h"
#include "nsIDOMNode.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIJSContextStack.h"
#include "nsIScriptContext.h"
#include "nsCOMArray.h"

class nsIScriptGlobalObject;
class nsIXPConnect;
class nsIContent;
class nsIDocument;
class nsIDocShell;
class nsINameSpaceManager;
class nsIScriptSecurityManager;
class nsIThreadJSContextStack;
class nsIParserService;


class nsContentUtils
{
public:
  static nsresult Init();

  static nsresult ReparentContentWrapper(nsIContent *aContent,
                                         nsIContent *aNewParent,
                                         nsIDocument *aNewDocument,
                                         nsIDocument *aOldDocument);

  static PRBool   IsCallerChrome();

  /*
   * Returns true if the nodes are both in the same document or
   * if neither is in a document.
   * Returns false if the nodes are not in the same document.
   */
  static PRBool   InSameDoc(nsIDOMNode *aNode,
                            nsIDOMNode *aOther);

  /**
   * Do not ever pass null pointers to this method.  If one of your
   * nsIContents is null, you have to decide for yourself what
   * "IsDescendantOf" really means.
   *
   * @param  aPossibleDescendant node to test for being a descendant of
   *         aPossibleAncestor
   * @param  aPossibleAncestor node to test for being an ancestor of
   *         aPossibleDescendant
   * @return PR_TRUE if aPossibleDescendant is a descendant of
   *         aPossibleAncestor (or is aPossibleAncestor).  PR_FALSE
   *         otherwise.
   */   
  static PRBool ContentIsDescendantOf(nsIContent* aPossibleDescendant,
                                      nsIContent* aPossibleAncestor);
  
  /*
   * This method fills the |aArray| with all ancestor nodes of |aNode|
   * including |aNode| at the zero index.
   *
   * These elements were |nsIDOMNode*|s before casting to |void*| and must
   * be cast back to |nsIDOMNode*| on usage, or bad things will happen.
   */
  static nsresult GetAncestors(nsIDOMNode* aNode,
                               nsVoidArray* aArray);

  /*
   * This method fills |aAncestorNodes| with all ancestor nodes of |aNode|
   * including |aNode| (QI'd to nsIContent) at the zero index.
   * For each ancestor, there is a corresponding element in |aAncestorOffsets|
   * which is the IndexOf the child in relation to its parent.
   *
   * The elements of |aAncestorNodes| were |nsIContent*|s before casting to 
   * |void*| and must be cast back to |nsIContent*| on usage, or bad things
   * will happen.
   *
   * This method just sucks.
   */
  static nsresult GetAncestorsAndOffsets(nsIDOMNode* aNode,
                                         PRInt32 aOffset,
                                         nsVoidArray* aAncestorNodes,
                                         nsVoidArray* aAncestorOffsets);

  /*
   * The out parameter, |aCommonAncestor| will be the closest node, if any,
   * to both |aNode| and |aOther| which is also an ancestor of each.
   */
  static nsresult GetCommonAncestor(nsIDOMNode *aNode,
                                    nsIDOMNode *aOther,
                                    nsIDOMNode** aCommonAncestor);

  /*
   * |aDifferentNodes| will contain up to 3 elements.
   * The first, if present, is the common ancestor of |aNode| and |aOther|.
   * The second, if present, is the ancestor node of |aNode| which is
   * closest to the common ancestor, but not an ancestor of |aOther|.
   * The third, if present, is the ancestor node of |aOther| which is
   * closest to the common ancestor, but not an ancestor of |aNode|.
   */
  static nsresult GetFirstDifferentAncestors(nsIDOMNode *aNode,
                                             nsIDOMNode *aOther,
                                             nsCOMArray<nsIDOMNode>& aDifferentNodes);

  /**
   * Compares the document position of nodes which may have parents.
   * DO NOT pass in nodes that cannot have a parentNode. In other words:
   * DO NOT pass in Attr, Document, DocumentFragment, Entity, or Notation!
   * The results will be completely wrong!
   *
   * @param   aNode   The node to which you are comparing.
   * @param   aOther  The reference node to which aNode is compared.
   *
   * @return  The document position flags of the nodes.
   *
   * @see nsIDOMNode
   * @see nsIDOM3Node
   */
  static PRUint16 ComparePositionWithAncestors(nsIDOMNode *aNode,
                                               nsIDOMNode *aOther);

  /**
   * Reverses the document position flags passed in.
   *
   * @param   aDocumentPosition   The document position flags to be reversed.
   *
   * @return  The reversed document position flags.
   *
   * @see nsIDOMNode
   * @see nsIDOM3Node
   */
  static PRUint16 ReverseDocumentPosition(PRUint16 aDocumentPosition);

  // These are copied from nsJSUtils.h

  static nsresult GetStaticScriptGlobal(JSContext* aContext,
                                        JSObject* aObj,
                                        nsIScriptGlobalObject** aNativeGlobal);

  static nsresult GetStaticScriptContext(JSContext* aContext,
                                         JSObject* aObj,
                                         nsIScriptContext** aScriptContext);

  static nsresult GetDynamicScriptGlobal(JSContext *aContext,
                                         nsIScriptGlobalObject** aNativeGlobal);

  static nsresult GetDynamicScriptContext(JSContext *aContext,
                                          nsIScriptContext** aScriptContext);

  static PRUint32 CopyNewlineNormalizedUnicodeTo(const nsAString& aSource, 
                                                 PRUint32 aSrcOffset, 
                                                 PRUnichar* aDest, 
                                                 PRUint32 aLength,
                                                 PRBool& aLastCharCR);

  static PRUint32 CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAString& aDest);

  static nsISupports *
  GetClassInfoInstance(nsDOMClassInfoID aID);

  static const nsDependentSubstring TrimCharsInSet(const char* aSet,
                                                   const nsAString& aValue);

  static void Shutdown();
  
  /**
   * Checks whether two nodes come from the same origin. aTrustedNode is
   * considered 'safe' in that a user can operate on it and that it isn't
   * a js-object that implements nsIDOMNode.
   * Never call this function with the first node provided by script, it
   * must always be known to be a 'real' node!
   */
  static nsresult CheckSameOrigin(nsIDOMNode* aTrustedNode,
                                  nsIDOMNode* aUnTrustedNode);

  // Check if the (JS) caller can access aNode.
  static PRBool CanCallerAccess(nsIDOMNode *aNode);

  /**
   * Get the docshell through the JS context that's currently on the stack.
   * If there's no JS context currently on the stack aDocShell will be null.
   *
   * @param aDocShell The docshell or null if no JS context
   */
  static void GetDocShellFromCaller(nsIDocShell** aDocShell);

  /**
   * Get the document through the JS context that's currently on the stack.
   * If there's no JS context currently on the stack aDocument will be null.
   *
   * @param aDocument The document or null if no JS context
   */
  static void GetDocumentFromCaller(nsIDOMDocument** aDocument);

  // Check if a node is in the document prolog, i.e. before the document
  // element.
  static PRBool InProlog(nsIDOMNode *aNode);

  static nsIParserService* GetParserServiceWeakRef();
  
  static nsINameSpaceManager* GetNSManagerWeakRef()
  {
      return sNameSpaceManager;
  };

private:
  static nsresult GetDocumentAndPrincipal(nsIDOMNode* aNode,
                                          nsIDocument** aDocument,
                                          nsIPrincipal** aPrincipal);

  static nsresult doReparentContentWrapper(nsIContent *aChild,
                                           nsIDocument *aNewDocument,
                                           nsIDocument *aOldDocument,
                                           JSContext *cx,
                                           JSObject *parent_obj);


  static nsIDOMScriptObjectFactory *sDOMScriptObjectFactory;

  static nsIXPConnect *sXPConnect;

  static nsIScriptSecurityManager *sSecurityManager;

  static nsIThreadJSContextStack *sThreadJSContextStack;

  static nsIParserService *sParserService;

  static nsINameSpaceManager *sNameSpaceManager;
};


class nsCxPusher
{
public:
  nsCxPusher(nsISupports *aCurrentTarget)
    : mScriptIsRunning(PR_FALSE)
  {
    if (aCurrentTarget) {
      Push(aCurrentTarget);
    }
  }

  ~nsCxPusher()
  {
    Pop();
  }

  void Push(nsISupports *aCurrentTarget);
  void Pop();

private:
  nsCOMPtr<nsIJSContextStack> mStack;
  nsCOMPtr<nsIScriptContext> mScx;
  PRBool mScriptIsRunning;
};


#define NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(_class)                      \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    foundInterface =                                                          \
      nsContentUtils::GetClassInfoInstance(eDOMClassInfo_##_class##_id);      \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  } else

#endif /* nsContentUtils_h___ */
