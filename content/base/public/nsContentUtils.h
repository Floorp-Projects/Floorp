/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsIStatefulFrame.h"
#include "nsIPref.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"

class nsIXPConnect;
class nsIContent;
class nsIDocument;
class nsIDocShell;
class nsINameSpaceManager;
class nsIScriptSecurityManager;
class nsIThreadJSContextStack;
class nsIParserService;
class nsIIOService;
class nsIURI;
class imgIDecoderObserver;
class imgIRequest;
class imgILoader;
class nsIPrefBranch;
class nsIPref;

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
   *
   * @throws NS_ERROR_FAILURE if aNode and aOther are disconnected.
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

  static const nsDependentSubstring TrimWhitespace(const nsAString& aStr,
                                                   PRBool aTrimTrailing = PR_TRUE);

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
  static nsIDocShell *GetDocShellFromCaller();

  /**
   * Get the document through the JS context that's currently on the stack.
   * If there's no JS context currently on the stack aDocument will be null.
   *
   * @param aDocument The document or null if no JS context
   */
  static nsIDOMDocument *GetDocumentFromCaller();

  // Check if a node is in the document prolog, i.e. before the document
  // element.
  static PRBool InProlog(nsIDOMNode *aNode);

  static nsIParserService* GetParserServiceWeakRef();
  
  static nsINameSpaceManager* GetNSManagerWeakRef()
  {
    return sNameSpaceManager;
  };

  static nsIIOService* GetIOServiceWeakRef()
  {
    return sIOService;
  };

  static nsIScriptSecurityManager* GetSecurityManager()
  {
    return sSecurityManager;
  }
  
  static nsresult GenerateStateKey(nsIContent* aContent,
                                   nsIStatefulFrame::SpecialStateID aID,
                                   nsACString& aKey);

  /**
   * Create a new nsIURI from aSpec, using aBaseURI as the base.  The
   * origin charset of the new nsIURI will be the document charset of
   * aDocument.
   */
  static nsresult NewURIWithDocumentCharset(nsIURI** aResult,
                                            const nsAString& aSpec,
                                            nsIDocument* aDocument,
                                            nsIURI* aBaseURI);

  /**
   * Determine whether aContent is in some way associated with aForm.  If the
   * form is a container the only elements that are considered to be associated
   * with a form are the elements that are contained within the form. If the
   * form is a leaf element then all elements will be accepted into this list,
   * since this can happen due to content fixup when a form spans table rows or
   * table cells.
   */
  static PRBool BelongsInForm(nsIDOMHTMLFormElement *aForm,
                              nsIContent *aContent);
  
  static nsresult CheckQName(const nsAString& aQualifiedName,
                             PRBool aNamespaceAware = PR_TRUE);

  static nsresult GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                       const nsAString& aQualifiedName,
                                       nsNodeInfoManager* aNodeInfoManager,
                                       nsINodeInfo** aNodeInfo);

  static nsAdoptingCString GetCharPref(const char *aPref);
  static PRPackedBool GetBoolPref(const char *aPref,
                                  PRBool aDefault = PR_FALSE);
  static PRInt32 GetIntPref(const char *aPref, PRInt32 aDefault = 0);
  static nsAdoptingString GetLocalizedStringPref(const char *aPref);
  static nsAdoptingString GetStringPref(const char *aPref);
  static void RegisterPrefCallback(const char *aPref,
                                   PrefChangedFunc aCallback,
                                   void * aClosure);
  static void UnregisterPrefCallback(const char *aPref,
                                     PrefChangedFunc aCallback,
                                     void * aClosure);
  static nsIPrefBranch *GetPrefBranch()
  {
    return sPrefBranch;
  }

  static nsresult GetDocumentAndPrincipal(nsIDOMNode* aNode,
                                          nsIDocument** aDocument,
                                          nsIPrincipal** aPrincipal);

  /**
   * Method to do security and content policy checks on the image URI
   *
   * @param aURI uri of the image to be loaded
   * @param aContext the context the image is loaded in (eg an element)
   * @param aLoadingDocument the document we belong to
   * @return PR_TRUE if the load can proceed, or PR_FALSE if it is blocked
   */
  static PRBool CanLoadImage(nsIURI* aURI,
                             nsISupports* aContext,
                             nsIDocument* aLoadingDocument);
  /**
   * Method to start an image load.  This does not do any security checks.
   *
   * @param aURI uri of the image to be loaded
   * @param aLoadingDocument the document we belong to
   * @param aObserver the observer for the image load
   * @param aLoadFlags the load flags to use.  See nsIRequest
   * @return the imgIRequest for the image load
   */
  static nsresult LoadImage(nsIURI* aURI,
                            nsIDocument* aLoadingDocument,
                            nsIURI* aReferrer,
                            imgIDecoderObserver* aObserver,
                            PRInt32 aLoadFlags,
                            imgIRequest** aRequest);

  /**
   * Convenience method to create a new nodeinfo that differs only by name
   * from aNodeInfo.
   */
  static nsresult NameChanged(nsINodeInfo *aNodeInfo, nsIAtom *aName,
                              nsINodeInfo** aResult)
  {
    nsNodeInfoManager *niMgr = aNodeInfo->NodeInfoManager();

    return niMgr->GetNodeInfo(aName, aNodeInfo->GetPrefixAtom(),
                              aNodeInfo->NamespaceID(), aResult);
  }

  /**
   * Convenience method to create a new nodeinfo that differs only by prefix
   * from aNodeInfo.
   */
  static nsresult PrefixChanged(nsINodeInfo *aNodeInfo, nsIAtom *aPrefix,
                                nsINodeInfo** aResult)
  {
    nsNodeInfoManager *niMgr = aNodeInfo->NodeInfoManager();

    return niMgr->GetNodeInfo(aNodeInfo->NameAtom(), aPrefix,
                              aNodeInfo->NamespaceID(), aResult);
  }

  /**
   * Retrieve a pointer to the document that owns aNodeInfo.
   */
  static nsIDocument *GetDocument(nsINodeInfo *aNodeInfo)
  {
    return aNodeInfo->NodeInfoManager()->GetDocument();
  }

  /**
   * Returns the appropriate event argument name for the specified
   * namespace.  Added because we need to switch between SVG's "evt"
   * and the rest of the world's "event".
   */
  static const char *GetEventArgName(PRInt32 aNameSpaceID);

private:
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

  static nsIIOService *sIOService;

  static nsIPrefBranch *sPrefBranch;

  static nsIPref *sPref;

  static imgILoader* sImgLoader;

  static PRBool sInitialized;
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
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#define NS_INTERFACE_MAP_ENTRY_TEAROFF(_interface, _allocator)                \
  if (aIID.Equals(NS_GET_IID(_interface))) {                                  \
    foundInterface = NS_STATIC_CAST(_interface *, _allocator);                \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#endif /* nsContentUtils_h___ */
