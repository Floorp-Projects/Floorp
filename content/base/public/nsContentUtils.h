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
#include "jsnum.h"
#include "nsAString.h"
#include "nsIStatefulFrame.h"
#include "nsIPref.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsContentList.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIDOM3Node.h"
#include "nsDataHashtable.h"
#include "nsIScriptRuntime.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEvent.h"
#include "nsTArray.h"
#include "nsTextFragment.h"

struct nsNativeKeyEvent; // Don't include nsINativeKeyBindings.h here: it will force strange compilation error!

class nsIDOMScriptObjectFactory;
class nsIXPConnect;
class nsINode;
class nsIContent;
class nsIDOMNode;
class nsIDOMKeyEvent;
class nsIDocument;
class nsIDocShell;
class nsINameSpaceManager;
class nsIScriptSecurityManager;
class nsIJSContextStack;
class nsIThreadJSContextStack;
class nsIParserService;
class nsIIOService;
class nsIURI;
class imgIDecoderObserver;
class imgIRequest;
class imgILoader;
class nsIPrefBranch;
class nsIImage;
class nsIImageLoadingContent;
class nsIDOMHTMLFormElement;
class nsIDOMDocument;
class nsIConsoleService;
class nsIStringBundleService;
class nsIStringBundle;
class nsIContentPolicy;
class nsILineBreaker;
class nsIWordBreaker;
class nsIJSRuntimeService;
class nsIEventListenerManager;
class nsIScriptContext;
class nsIRunnable;
class nsIInterfaceRequestor;
template<class E> class nsCOMArray;
class nsIPref;
class nsVoidArray;
struct JSRuntime;
class nsICaseConversion;
class nsIUGenCategory;
class nsIWidget;
class nsIDragSession;
class nsPIDOMWindow;
class nsPIDOMEventTarget;
#ifdef MOZ_XTF
class nsIXTFService;
#endif
#ifdef IBMBIDI
class nsIBidiKeyboard;
#endif

extern const char kLoadAsData[];

enum EventNameType {
  EventNameType_None = 0x0000,
  EventNameType_HTML = 0x0001,
  EventNameType_XUL = 0x0002,
  EventNameType_SVGGraphic = 0x0004, // svg graphic elements
  EventNameType_SVGSVG = 0x0008, // the svg element

  EventNameType_HTMLXUL = 0x0003,
  EventNameType_All = 0xFFFF
};

struct EventNameMapping {
  PRUint32  mId;
  PRInt32 mType;
};

struct nsShortcutCandidate {
  nsShortcutCandidate(PRUint32 aCharCode, PRBool aIgnoreShift) :
    mCharCode(aCharCode), mIgnoreShift(aIgnoreShift)
  {
  }
  PRUint32 mCharCode;
  PRBool   mIgnoreShift;
};

class nsContentUtils
{
public:
  static nsresult Init();

  // You MUST pass the old ownerDocument of aContent in as aOldDocument and the
  // new one as aNewDocument.  aNewParent is allowed to be null; in that case
  // aNewDocument will be assumed to be the parent.  Note that at this point
  // the actual ownerDocument of aContent may not yet be aNewDocument.
  // XXXbz but then if it gets wrapped after we do this call but before its
  // ownerDocument actually changes, things will break...
  static nsresult ReparentContentWrapper(nsIContent *aNode,
                                         nsIContent *aNewParent,
                                         nsIDocument *aNewDocument,
                                         nsIDocument *aOldDocument);

  /**
   * Get a scope from aOldDocument and one from aNewDocument. Also get a
   * context through one of the scopes, from the stack or the safe context.
   *
   * @param aOldDocument The document to get aOldScope from.
   * @param aNewDocument The document to get aNewScope from.
   * @param aCx [out] Context gotten through one of the scopes, from the stack
   *                  or the safe context.
   * @param aOldScope [out] Scope gotten from aOldDocument.
   * @param aNewScope [out] Scope gotten from aNewDocument.
   */
  static nsresult GetContextAndScopes(nsIDocument *aOldDocument,
                                      nsIDocument *aNewDocument,
                                      JSContext **aCx, JSObject **aOldScope,
                                      JSObject **aNewScope);

  /**
   * When a document's scope changes (e.g., from document.open(), call this
   * function to move all content wrappers from the old scope to the new one.
   */
  static nsresult ReparentContentWrappersInScope(nsIScriptGlobalObject *aOldScope,
                                                 nsIScriptGlobalObject *aNewScope);

  static PRBool   IsCallerChrome();

  static PRBool   IsCallerTrustedForRead();

  static PRBool   IsCallerTrustedForWrite();

  /**
   * Check whether a caller is trusted to have aCapability.  This also
   * checks for UniversalXPConnect in addition to aCapability.
   */
  static PRBool   IsCallerTrustedForCapability(const char* aCapability);

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
  static PRBool ContentIsDescendantOf(nsINode* aPossibleDescendant,
                                      nsINode* aPossibleAncestor);

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
   * Returns an error if the two nodes are disconnected and don't have
   * a common ancestor.
   */
  static nsresult GetCommonAncestor(nsIDOMNode *aNode,
                                    nsIDOMNode *aOther,
                                    nsIDOMNode** aCommonAncestor);

  /**
   * Returns the common ancestor, if any, for two nodes. Returns null if the
   * nodes are disconnected.
   */
  static nsINode* GetCommonAncestor(nsINode* aNode1,
                                    nsINode* aNode2);

  /**
   * Compares the document position of nodes.
   *
   * @param aNode1 The node whose position is being compared to the reference
   *               node
   * @param aNode2 The reference node
   *
   * @return  The document position flags of the nodes. aNode1 is compared to
   *          aNode2, i.e. if aNode1 is before aNode2 then
   *          DOCUMENT_POSITION_PRECEDING will be set.
   *
   * @see nsIDOMNode
   * @see nsIDOM3Node
   */
  static PRUint16 ComparePosition(nsINode* aNode1,
                                  nsINode* aNode2);

  /**
   * Returns true if aNode1 is before aNode2 in the same connected
   * tree.
   */
  static PRBool PositionIsBefore(nsINode* aNode1,
                                 nsINode* aNode2)
  {
    return (ComparePosition(aNode1, aNode2) &
      (nsIDOM3Node::DOCUMENT_POSITION_PRECEDING |
       nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED)) ==
      nsIDOM3Node::DOCUMENT_POSITION_PRECEDING;
  }

  /**
   *  Utility routine to compare two "points", where a point is a
   *  node/offset pair
   *  Returns -1 if point1 < point2, 1, if point1 > point2,
   *  0 if error or if point1 == point2.
   *  NOTE! If the two nodes aren't in the same connected subtree,
   *  the result is undefined, but the optional aDisconnected parameter
   *  is set to PR_TRUE.
   */
  static PRInt32 ComparePoints(nsINode* aParent1, PRInt32 aOffset1,
                               nsINode* aParent2, PRInt32 aOffset2,
                               PRBool* aDisconnected = nsnull);

  /**
   * Find the first child of aParent with a resolved tag matching
   * aNamespace and aTag. Both the explicit and anonymous children of
   * aParent are examined. The return value is not addrefed.
   *
   * XXXndeakin this should return the first child whether in anonymous or
   * explicit children, but currently XBL doesn't tell us the relative
   * ordering of anonymous vs explicit children, so instead it searches
   * the explicit children first then the anonymous children.
   */
  static nsIContent* FindFirstChildWithResolvedTag(nsIContent* aParent,
                                                   PRInt32 aNamespace,
                                                   nsIAtom* aTag);

  /**
   * Brute-force search of the element subtree rooted at aContent for
   * an element with the given id.  aId must be nonempty, otherwise
   * this method may return nodes even if they have no id!
   */
  static nsIContent* MatchElementId(nsIContent *aContent,
                                    const nsAString& aId);

  /**
   * Similar to above, but to be used if one already has an atom for the ID
   */
  static nsIContent* MatchElementId(nsIContent *aContent,
                                    nsIAtom* aId);

  /**
   * Given a URI containing an element reference (#whatever),
   * resolve it to the target content element with the given ID.
   *
   * If aFromContent is anonymous XBL content then the URI
   * must refer to its binding document and we will return
   * a node in the same anonymous content subtree as aFromContent,
   * if one exists with the correct ID.
   *
   * @param aFromContent the context of the reference;
   *   currently we only support references to elements in the
   *   same document as the context, so this must be non-null
   *
   * @return the element, or nsnull on failure
   */
  static nsIContent* GetReferencedElement(nsIURI* aURI,
                                          nsIContent *aFromContent);

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

  /**
   * Returns true if aChar is of class Ps, Pi, Po, Pf, or Pe.
   */
  static PRBool IsPunctuationMark(PRUint32 aChar);
  static PRBool IsPunctuationMarkAt(const nsTextFragment* aFrag, PRUint32 aOffset);
 
  /**
   * Returns true if aChar is of class Lu, Ll, Lt, Lm, Lo, Nd, Nl or No
   */
  static PRBool IsAlphanumeric(PRUint32 aChar);
  static PRBool IsAlphanumericAt(const nsTextFragment* aFrag, PRUint32 aOffset);

  /*
   * Is the character an HTML whitespace character?
   *
   * We define whitespace using the list in HTML5 and css3-selectors:
   * U+0009, U+000A, U+000C, U+000D, U+0020
   *
   * HTML 4.01 also lists U+200B (zero-width space).
   */
  static PRBool IsHTMLWhitespace(PRUnichar aChar);

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

  // Check if the (JS) caller can access aWindow.
  // aWindow can be either outer or inner window.
  static PRBool CanCallerAccess(nsPIDOMWindow* aWindow);

  /**
   * Get the docshell through the JS context that's currently on the stack.
   * If there's no JS context currently on the stack aDocShell will be null.
   *
   * @param aDocShell The docshell or null if no JS context
   */
  static nsIDocShell *GetDocShellFromCaller();

  /**
   * The two GetDocumentFrom* functions below allow a caller to get at a
   * document that is relevant to the currently executing script.
   *
   * GetDocumentFromCaller gets its document by looking at the last called
   * function and finding the document that the function itself relates to.
   * For example, consider two windows A and B in the same origin. B has a
   * function which does something that ends up needing the current document.
   * If a script in window A were to call B's function, GetDocumentFromCaller
   * would find that function (in B) and return B's document.
   *
   * GetDocumentFromContext gets its document by looking at the currently
   * executing context's global object and returning its document. Thus,
   * given the example above, GetDocumentFromCaller would see that the
   * currently executing script was in window A, and return A's document.
   */
  /**
   * Get the document from the currently executing function. This will return
   * the document that the currently executing function is in/from.
   *
   * @return The document or null if no JS Context.
   */
  static nsIDOMDocument *GetDocumentFromCaller();

  /**
   * Get the document through the JS context that's currently on the stack.
   * If there's no JS context currently on the stack it will return null.
   * This will return the document of the calling script.
   *
   * @return The document or null if no JS context
   */
  static nsIDOMDocument *GetDocumentFromContext();

  // Check if a node is in the document prolog, i.e. before the document
  // element.
  static PRBool InProlog(nsINode *aNode);

  static nsIParserService* GetParserService();

  static nsINameSpaceManager* NameSpaceManager()
  {
    return sNameSpaceManager;
  }

  static nsIIOService* GetIOService()
  {
    return sIOService;
  }

  static imgILoader* GetImgLoader()
  {
    return sImgLoader;
  }

#ifdef MOZ_XTF
  static nsIXTFService* GetXTFService();
#endif

#ifdef IBMBIDI
  static nsIBidiKeyboard* GetBidiKeyboard();
#endif
  
  /**
   * Get the cache security manager service. Can return null if the layout
   * module has been shut down.
   */
  static nsIScriptSecurityManager* GetSecurityManager()
  {
    return sSecurityManager;
  }

  static nsresult GenerateStateKey(nsIContent* aContent,
                                   nsIDocument* aDocument,
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
   * Convert aInput (in charset aCharset) to UTF16 in aOutput.
   *
   * @param aCharset the name of the charset; if empty, we assume UTF8
   */
  static nsresult ConvertStringFromCharset(const nsACString& aCharset,
                                           const nsACString& aInput,
                                           nsAString& aOutput);

  /**
   * Determine whether a buffer begins with a BOM for UTF-8, UTF-16LE,
   * UTF-16BE, UTF-32LE, UTF-32BE.
   *
   * @param aBuffer the buffer to check
   * @param aLength the length of the buffer
   * @param aCharset empty if not found
   * @return boolean indicating whether a BOM was detected.
   */
  static PRBool CheckForBOM(const unsigned char* aBuffer, PRUint32 aLength,
                            nsACString& aCharset);


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

  static nsresult SplitQName(nsIContent* aNamespaceResolver,
                             const nsAFlatString& aQName,
                             PRInt32 *aNamespace, nsIAtom **aLocalName);

  static nsresult LookupNamespaceURI(nsIContent* aNamespaceResolver,
                                     const nsAString& aNamespacePrefix,
                                     nsAString& aNamespaceURI);

  static nsresult GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                       const nsAString& aQualifiedName,
                                       nsNodeInfoManager* aNodeInfoManager,
                                       nsINodeInfo** aNodeInfo);

  static void SplitExpatName(const PRUnichar *aExpatName, nsIAtom **aPrefix,
                             nsIAtom **aTagName, PRInt32 *aNameSpaceID);

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
  static void AddBoolPrefVarCache(const char* aPref, PRBool* aVariable);
  static nsIPrefBranch *GetPrefBranch()
  {
    return sPrefBranch;
  }

  static nsILineBreaker* LineBreaker()
  {
    return sLineBreaker;
  }

  static nsIWordBreaker* WordBreaker()
  {
    return sWordBreaker;
  }
  
  static nsICaseConversion* GetCaseConv()
  {
    return sCaseConv;
  }

  static nsIUGenCategory* GetGenCat()
  {
    return sGenCat;
  }

  /**
   * @return PR_TRUE if aContent has an attribute aName in namespace aNameSpaceID,
   * and the attribute value is non-empty.
   */
  static PRBool HasNonEmptyAttr(nsIContent* aContent, PRInt32 aNameSpaceID,
                                nsIAtom* aName);

  /**
   * Method that gets the primary presContext for the node.
   * 
   * @param aContent The content node.
   * @return the presContext, or nsnull if the content is not in a document
   *         (if GetCurrentDoc returns nsnull)
   */
  static nsPresContext* GetContextForContent(nsIContent* aContent);

  /**
   * Method to do security and content policy checks on the image URI
   *
   * @param aURI uri of the image to be loaded
   * @param aContext the context the image is loaded in (eg an element)
   * @param aLoadingDocument the document we belong to
   * @param aLoadingPrincipal the principal doing the load
   * @param aImageBlockingStatus the nsIContentPolicy blocking status for this
   *        image.  This will be set even if a security check fails for the
   *        image, to some reasonable REJECT_* value.  This out param will only
   *        be set if it's non-null.
   * @return PR_TRUE if the load can proceed, or PR_FALSE if it is blocked.
   *         Note that aImageBlockingStatus, if set will always be an ACCEPT
   *         status if PR_TRUE is returned and always be a REJECT_* status if
   *         PR_FALSE is returned.
   */
  static PRBool CanLoadImage(nsIURI* aURI,
                             nsISupports* aContext,
                             nsIDocument* aLoadingDocument,
                             nsIPrincipal* aLoadingPrincipal,
                             PRInt16* aImageBlockingStatus = nsnull);
  /**
   * Method to start an image load.  This does not do any security checks.
   * This method will attempt to make aURI immutable; a caller that wants to
   * keep a mutable version around should pass in a clone.
   *
   * @param aURI uri of the image to be loaded
   * @param aLoadingDocument the document we belong to
   * @param aLoadingPrincipal the principal doing the load
   * @param aReferrer the referrer URI
   * @param aObserver the observer for the image load
   * @param aLoadFlags the load flags to use.  See nsIRequest
   * @return the imgIRequest for the image load
   */
  static nsresult LoadImage(nsIURI* aURI,
                            nsIDocument* aLoadingDocument,
                            nsIPrincipal* aLoadingPrincipal,
                            nsIURI* aReferrer,
                            imgIDecoderObserver* aObserver,
                            PRInt32 aLoadFlags,
                            imgIRequest** aRequest);

  /**
   * Method to get an nsIImage from an image loading content
   *
   * @param aContent The image loading content.  Must not be null.
   * @param aRequest The image request [out]
   * @return the nsIImage corresponding to the first frame of the image
   */
  static already_AddRefed<nsIImage> GetImageFromContent(nsIImageLoadingContent* aContent, imgIRequest **aRequest = nsnull);

  /**
   * Method that decides whether a content node is draggable
   *
   * @param aContent The content node to test.
   * @return whether it's draggable
   */
  static PRBool ContentIsDraggable(nsIContent* aContent);

  /**
   * Method that decides whether a content node is a draggable image
   *
   * @param aContent The content node to test.
   * @return whether it's a draggable image
   */
  static PRBool IsDraggableImage(nsIContent* aContent);

  /**
   * Method that decides whether a content node is a draggable link
   *
   * @param aContent The content node to test.
   * @return whether it's a draggable link
   */
  static PRBool IsDraggableLink(nsIContent* aContent);

  /**
   * Convenience method to create a new nodeinfo that differs only by name
   * from aNodeInfo.
   */
  static nsresult NameChanged(nsINodeInfo *aNodeInfo, nsIAtom *aName,
                              nsINodeInfo** aResult)
  {
    nsNodeInfoManager *niMgr = aNodeInfo->NodeInfoManager();

    *aResult = niMgr->GetNodeInfo(aName, aNodeInfo->GetPrefixAtom(),
                                  aNodeInfo->NamespaceID()).get();
    return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Convenience method to create a new nodeinfo that differs only by prefix
   * from aNodeInfo.
   */
  static nsresult PrefixChanged(nsINodeInfo *aNodeInfo, nsIAtom *aPrefix,
                                nsINodeInfo** aResult)
  {
    nsNodeInfoManager *niMgr = aNodeInfo->NodeInfoManager();

    *aResult = niMgr->GetNodeInfo(aNodeInfo->NameAtom(), aPrefix,
                                  aNodeInfo->NamespaceID()).get();
    return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Returns the appropriate event argument names for the specified
   * namespace and event name.  Added because we need to switch between
   * SVG's "evt" and the rest of the world's "event", and because onerror
   * takes 3 args.
   */
  static void GetEventArgNames(PRInt32 aNameSpaceID, nsIAtom *aEventName,
                               PRUint32 *aArgCount, const char*** aArgNames);

  /**
   * If aNode is not an element, return true exactly when aContent's binding
   * parent is null.
   *
   * If aNode is an element, return true exactly when aContent's binding parent
   * is the same as aNode's.
   *
   * This method is particularly useful for callers who are trying to ensure
   * that they are working with a non-anonymous descendant of a given node.  If
   * aContent is a descendant of aNode, a return value of PR_FALSE from this
   * method means that it's an anonymous descendant from aNode's point of view.
   *
   * Both arguments to this method must be non-null.
   */
  static PRBool IsInSameAnonymousTree(nsINode* aNode, nsIContent* aContent);

  /**
   * Return the nsIXPConnect service.
   */
  static nsIXPConnect *XPConnect()
  {
    return sXPConnect;
  }

  /**
   * Report a localized error message to the error console.
   *   @param aFile Properties file containing localized message.
   *   @param aMessageName Name of localized message.
   *   @param aParams Parameters to be substituted into localized message.
   *   @param aParamsLength Length of aParams.
   *   @param aURI URI of resource containing error (may be null).
   *   @param aSourceLine The text of the line that contains the error (may be
              empty).
   *   @param aLineNumber Line number within resource containing error.
   *   @param aColumnNumber Column number within resource containing error.
   *   @param aErrorFlags See nsIScriptError.
   *   @param aCategory Name of module reporting error.
   */
  enum PropertiesFile {
    eCSS_PROPERTIES,
    eXBL_PROPERTIES,
    eXUL_PROPERTIES,
    eLAYOUT_PROPERTIES,
    eFORMS_PROPERTIES,
    ePRINTING_PROPERTIES,
    eDOM_PROPERTIES,
#ifdef MOZ_SVG
    eSVG_PROPERTIES,
#endif
    eBRAND_PROPERTIES,
    eCOMMON_DIALOG_PROPERTIES,
    PropertiesFile_COUNT
  };
  static nsresult ReportToConsole(PropertiesFile aFile,
                                  const char *aMessageName,
                                  const PRUnichar **aParams,
                                  PRUint32 aParamsLength,
                                  nsIURI* aURI,
                                  const nsAFlatString& aSourceLine,
                                  PRUint32 aLineNumber,
                                  PRUint32 aColumnNumber,
                                  PRUint32 aErrorFlags,
                                  const char *aCategory);

  /**
   * Get the localized string named |aKey| in properties file |aFile|.
   */
  static nsresult GetLocalizedString(PropertiesFile aFile,
                                     const char* aKey,
                                     nsXPIDLString& aResult);

  /**
   * Fill (with the parameters given) the localized string named |aKey| in
   * properties file |aFile|.
   */
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const PRUnichar **aParams,
                                        PRUint32 aParamsLength,
                                        nsXPIDLString& aResult);

  /**
   * Returns true if aDocument is a chrome document
   */
  static PRBool IsChromeDoc(nsIDocument *aDocument);

  /**
   * Returns true if aDocument belongs to a chrome docshell for
   * display purposes.  Returns false for null documents or documents
   * which do not belong to a docshell.
   */
  static PRBool IsInChromeDocshell(nsIDocument *aDocument);

  /**
   * Release *aSupportsPtr when the shutdown notification is received
   */
  static nsresult ReleasePtrOnShutdown(nsISupports** aSupportsPtr) {
    NS_ASSERTION(aSupportsPtr, "Expect to crash!");
    NS_ASSERTION(*aSupportsPtr, "Expect to crash!");
    return sPtrsToPtrsToRelease->AppendElement(aSupportsPtr) ? NS_OK :
      NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Return the content policy service
   */
  static nsIContentPolicy *GetContentPolicy();

  /**
   * Quick helper to determine whether there are any mutation listeners
   * of a given type that apply to this content or any of its ancestors.
   * The method has the side effect to call document's MayDispatchMutationEvent
   * using aTargetForSubtreeModified as the parameter.
   *
   * @param aNode  The node to search for listeners
   * @param aType  The type of listener (NS_EVENT_BITS_MUTATION_*)
   * @param aTargetForSubtreeModified The node which is the target of the
   *                                  possible DOMSubtreeModified event.
   *
   * @return true if there are mutation listeners of the specified type
   */
  static PRBool HasMutationListeners(nsINode* aNode,
                                     PRUint32 aType,
                                     nsINode* aTargetForSubtreeModified);

  /**
   * This method creates and dispatches a trusted event.
   * Works only with events which can be created by calling
   * nsIDOMDocumentEvent::CreateEvent() with parameter "Events".
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       nsIDOMEventTarget.
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see nsIDOMEventTarget::DispatchEvent.
   */
  static nsresult DispatchTrustedEvent(nsIDocument* aDoc,
                                       nsISupports* aTarget,
                                       const nsAString& aEventName,
                                       PRBool aCanBubble,
                                       PRBool aCancelable,
                                       PRBool *aDefaultAction = nsnull);

  /**
   * Determines if an event attribute name (such as onclick) is valid for
   * a given element type. Types are from the EventNameType enumeration
   * defined above.
   *
   * @param aName the event name to look up
   * @param aType the type of content
   */
  static PRBool IsEventAttributeName(nsIAtom* aName, PRInt32 aType);

  /**
   * Return the event id for the event with the given name. The name is the
   * event name with the 'on' prefix. Returns NS_USER_DEFINED_EVENT if the
   * event doesn't match a known event name.
   *
   * @param aName the event name to look up
   */
  static PRUint32 GetEventId(nsIAtom* aName);

  /**
   * Used only during traversal of the XPCOM graph by the cycle
   * collector: push a pointer to the listener manager onto the
   * children deque, if it exists. Do nothing if there is no listener
   * manager.
   *
   * Crucially: does not perform any refcounting operations.
   *
   * @param aNode The node to traverse.
   * @param children The buffer to push a listener manager pointer into.
   */
  static void TraverseListenerManager(nsINode *aNode,
                                      nsCycleCollectionTraversalCallback &cb);

  /**
   * Get the eventlistener manager for aNode. If a new eventlistener manager
   * was created, aCreated is set to PR_TRUE.
   *
   * @param aNode The node for which to get the eventlistener manager.
   * @param aCreateIfNotFound If PR_FALSE, returns a listener manager only if
   *                          one already exists.
   * @param aResult [out] Set to the eventlistener manager for aNode.
   */
  static nsresult GetListenerManager(nsINode *aNode,
                                     PRBool aCreateIfNotFound,
                                     nsIEventListenerManager **aResult);

  /**
   * Remove the eventlistener manager for aNode.
   *
   * @param aNode The node for which to remove the eventlistener manager.
   */
  static void RemoveListenerManager(nsINode *aNode);

  static PRBool IsInitialized()
  {
    return sInitialized;
  }

  /**
   * Checks if the localname/prefix/namespace triple is valid wrt prefix
   * and namespace according to the Namespaces in XML and DOM Code
   * specfications.
   *
   * @param aLocalname localname of the node
   * @param aPrefix prefix of the node
   * @param aNamespaceID namespace of the node
   */
  static PRBool IsValidNodeName(nsIAtom *aLocalName, nsIAtom *aPrefix,
                                PRInt32 aNamespaceID);

  /**
   * Creates a DocumentFragment from text using a context node to resolve
   * namespaces.
   *
   * @param aContextNode the node which is used to resolve namespaces
   * @param aFragment the string which is parsed to a DocumentFragment
   * @param aWillOwnFragment is PR_TRUE if ownership of the fragment should be
   *                         transferred to the caller.
   * @param aReturn [out] the created DocumentFragment
   */
  static nsresult CreateContextualFragment(nsIDOMNode* aContextNode,
                                           const nsAString& aFragment,
                                           PRBool aWillOwnFragment,
                                           nsIDOMDocumentFragment** aReturn);

  /**
   * Creates a new XML document, which is marked to be loaded as data.
   *
   * @param aNamespaceURI Namespace for the root element to create and insert in
   *                      the document. Only used if aQualifiedName is not
   *                      empty.
   * @param aQualifiedName Qualified name for the root element to create and
   *                       insert in the document. If empty no root element will
   *                       be created.
   * @param aDoctype Doctype node to insert in the document.
   * @param aDocumentURI URI of the document. Must not be null.
   * @param aBaseURI Base URI of the document. Must not be null.
   * @param aPrincipal Prinicpal of the document. Must not be null.
   * @param aScriptObject The object from which the context for event handling
   *                      can be got.
   * @param aResult [out] The document that was created.
   */
  static nsresult CreateDocument(const nsAString& aNamespaceURI, 
                                 const nsAString& aQualifiedName, 
                                 nsIDOMDocumentType* aDoctype,
                                 nsIURI* aDocumentURI,
                                 nsIURI* aBaseURI,
                                 nsIPrincipal* aPrincipal,
                                 nsIScriptGlobalObject* aScriptObject,
                                 nsIDOMDocument** aResult);

  /**
   * Sets the text contents of a node by replacing all existing children
   * with a single text child.
   *
   * The function always notifies.
   *
   * Will reuse the first text child if one is available. Will not reuse
   * existing cdata children.
   *
   * @param aContent Node to set contents of.
   * @param aValue   Value to set contents to.
   * @param aTryReuse When true, the function will try to reuse an existing
   *                  textnodes rather than always creating a new one.
   */
  static nsresult SetNodeTextContent(nsIContent* aContent,
                                     const nsAString& aValue,
                                     PRBool aTryReuse);

  /**
   * Get the textual contents of a node. This is a concatenation of all
   * textnodes that are direct or (depending on aDeep) indirect children
   * of the node.
   *
   * NOTE! No serialization takes place and <br> elements
   * are not converted into newlines. Only textnodes and cdata nodes are
   * added to the result.
   *
   * @param aNode Node to get textual contents of.
   * @param aDeep If true child elements of aNode are recursivly descended
   *              into to find text children.
   * @param aResult the result. Out param.
   */
  static void GetNodeTextContent(nsINode* aNode, PRBool aDeep,
                                 nsAString& aResult)
  {
    aResult.Truncate();
    AppendNodeTextContent(aNode, aDeep, aResult);
  }

  /**
   * Same as GetNodeTextContents but appends the result rather than sets it.
   */
  static void AppendNodeTextContent(nsINode* aNode, PRBool aDeep,
                                    nsAString& aResult);

  /**
   * Utility method that checks if a given node has any non-empty
   * children.
   * NOTE! This method does not descend recursivly into elements.
   * Though it would be easy to make it so if needed
   */
  static PRBool HasNonEmptyTextContent(nsINode* aNode);

  /**
   * Delete strings allocated for nsContentList matches
   */
  static void DestroyMatchString(void* aData)
  {
    if (aData) {
      nsString* matchString = static_cast<nsString*>(aData);
      delete matchString;
    }
  }

  /**
   * Unbinds the content from the tree and nulls it out if it's not null.
   */
  static void DestroyAnonymousContent(nsCOMPtr<nsIContent>* aContent);

  /**
   * Keep script object aNewObject, held by aScriptObjectHolder, alive.
   *
   * NOTE: This currently only supports objects that hold script objects of one
   *       scripting language.
   *
   * @param aLangID script language ID of aNewObject
   * @param aScriptObjectHolder the object that holds aNewObject
   * @param aTracer the tracer for aScriptObject
   * @param aNewObject the script object to hold
   * @param aWasHoldingObjects whether aScriptObjectHolder was already holding
   *                           script objects (ie. HoldScriptObject was called
   *                           on it before, without a corresponding call to
   *                           DropScriptObjects)
   */
  static nsresult HoldScriptObject(PRUint32 aLangID, void* aScriptObjectHolder,
                                   nsScriptObjectTracer* aTracer,
                                   void* aNewObject, PRBool aWasHoldingObjects)
  {
    if (aLangID == nsIProgrammingLanguage::JAVASCRIPT) {
      return aWasHoldingObjects ? NS_OK :
                                  HoldJSObjects(aScriptObjectHolder, aTracer);
    }

    return HoldScriptObject(aLangID, aNewObject);
  }

  /**
   * Drop any script objects that aScriptObjectHolder is holding.
   *
   * NOTE: This currently only supports objects that hold script objects of one
   *       scripting language.
   *
   * @param aLangID script language ID of the objects that 
   * @param aScriptObjectHolder the object that holds script object that we want
   *                            to drop
   * @param aTracer the tracer for aScriptObject
   */
  static nsresult DropScriptObjects(PRUint32 aLangID, void* aScriptObjectHolder,
                                    nsScriptObjectTracer* aTracer)
  {
    if (aLangID == nsIProgrammingLanguage::JAVASCRIPT) {
      return DropJSObjects(aScriptObjectHolder);
    }

    aTracer->Trace(aScriptObjectHolder, DropScriptObject, nsnull);

    return NS_OK;
  }

  /**
   * Keep the JS objects held by aScriptObjectHolder alive.
   *
   * @param aScriptObjectHolder the object that holds JS objects that we want to
   *                            keep alive
   * @param aTracer the tracer for aScriptObject
   */
  static nsresult HoldJSObjects(void* aScriptObjectHolder,
                                nsScriptObjectTracer* aTracer);

  /**
   * Drop the JS objects held by aScriptObjectHolder.
   *
   * @param aScriptObjectHolder the object that holds JS objects that we want to
   *                            drop
   */
  static nsresult DropJSObjects(void* aScriptObjectHolder);

  /**
   * Convert nsIContent::IME_STATUS_* to nsIWidget::IME_STATUS_*
   */
  static PRUint32 GetWidgetStatusFromIMEStatus(PRUint32 aState);

  /*
   * Notify when the first XUL menu is opened and when the all XUL menus are
   * closed. At opening, aInstalling should be TRUE, otherwise, it should be
   * FALSE.
   */
  static void NotifyInstalledMenuKeyboardListener(PRBool aInstalling);

  /**
   * Do security checks before loading a resource. Does the following checks:
   *   nsIScriptSecurityManager::CheckLoadURIWithPrincipal
   *   NS_CheckContentLoadPolicy
   *   nsIScriptSecurityManager::CheckSameOriginURI
   *
   * You will still need to do at least SameOrigin checks before on redirects.
   *
   * @param aURIToLoad         URI that is getting loaded.
   * @param aLoadingPrincipal  Principal of the resource that is initiating
   *                           the load
   * @param aCheckLoadFlags    Flags to be passed to
   *                           nsIScriptSecurityManager::CheckLoadURIWithPrincipal
   *                           NOTE: If this contains ALLOW_CHROME the
   *                                 CheckSameOriginURI check will be skipped if
   *                                 aURIToLoad is a chrome uri.
   * @param aAllowData         Set to true to skip CheckSameOriginURI check when
                               aURIToLoad is a data uri.
   * @param aContentPolicyType Type     \
   * @param aContext           Context   |- to be passed to
   * @param aMimeGuess         Mimetype  |      NS_CheckContentLoadPolicy
   * @param aExtra             Extra    /
   */
  static nsresult CheckSecurityBeforeLoad(nsIURI* aURIToLoad,
                                          nsIPrincipal* aLoadingPrincipal,
                                          PRUint32 aCheckLoadFlags,
                                          PRBool aAllowData,
                                          PRUint32 aContentPolicyType,
                                          nsISupports* aContext,
                                          const nsACString& aMimeGuess = EmptyCString(),
                                          nsISupports* aExtra = nsnull);

  /**
   * Trigger a link with uri aLinkURI. If aClick is false, this triggers a
   * mouseover on the link, otherwise it triggers a load after doing a
   * security check using aContent's principal.
   *
   * @param aContent the node on which a link was triggered.
   * @param aPresContext the pres context, must be non-null.
   * @param aLinkURI the URI of the link, must be non-null.
   * @param aTargetSpec the target (like target=, may be empty).
   * @param aClick whether this was a click or not (if false, this method
   *               assumes you just hovered over the link).
   * @param aIsUserTriggered whether the user triggered the link. This would be
   *                         false for loads from auto XLinks or from the
   *                         click() method if we ever implement it.
   */
  static void TriggerLink(nsIContent *aContent, nsPresContext *aPresContext,
                          nsIURI *aLinkURI, const nsString& aTargetSpec,
                          PRBool aClick, PRBool aIsUserTriggered);

  /**
   * Return top-level widget in the parent chain.
   */
  static nsIWidget* GetTopLevelWidget(nsIWidget* aWidget);

  /**
   * Return the localized ellipsis for UI.
   */
  static const nsDependentString GetLocalizedEllipsis();

  /**
   * The routine GetNativeEvent is used to fill nsNativeKeyEvent.
   * It's also used in DOMEventToNativeKeyEvent.
   * See bug 406407 for details.
   */
  static nsEvent* GetNativeEvent(nsIDOMEvent* aDOMEvent);
  static PRBool DOMEventToNativeKeyEvent(nsIDOMKeyEvent* aKeyEvent,
                                         nsNativeKeyEvent* aNativeEvent,
                                         PRBool aGetCharCode);

  /**
   * Get the candidates for accelkeys for aDOMKeyEvent.
   *
   * @param aDOMKeyEvent [in] the key event for accelkey handling.
   * @param aCandidates [out] the candidate shortcut key combination list.
   *                          the first item is most preferred.
   */
  static void GetAccelKeyCandidates(nsIDOMKeyEvent* aDOMKeyEvent,
                                    nsTArray<nsShortcutCandidate>& aCandidates);

  /**
   * Get the candidates for accesskeys for aNativeKeyEvent.
   *
   * @param aNativeKeyEvent [in] the key event for accesskey handling.
   * @param aCandidates [out] the candidate access key list.
   *                          the first item is most preferred.
   */
  static void GetAccessKeyCandidates(nsKeyEvent* aNativeKeyEvent,
                                     nsTArray<PRUint32>& aCandidates);

  /**
   * Hide any XUL popups associated with aDocument, including any documents
   * displayed in child frames.
   */
  static void HidePopupsInDocument(nsIDocument* aDocument);

  /**
   * Retrieve the current drag session, or null if no drag is currently occuring
   */
  static already_AddRefed<nsIDragSession> GetDragSession();

  /**
   * Return true if aURI is a local file URI (i.e. file://).
   */
  static PRBool URIIsLocalFile(nsIURI *aURI);

  /**
   * If aContent is an HTML element with a DOM level 0 'name', then
   * return the name. Otherwise return null.
   */
  static nsIAtom* IsNamedItem(nsIContent* aContent);

  /**
   * Get the application manifest URI for this document.  The manifest URI
   * is specified in the manifest= attribute of the root element of the
   * document.
   *
   * @param aDocument The document that lists the manifest.
   * @param aURI The manifest URI.
   */
  static void GetOfflineAppManifest(nsIDocument *aDocument, nsIURI **aURI);

  /**
   * Check whether an application should be allowed to use offline APIs.
   */
  static PRBool OfflineAppAllowed(nsIURI *aURI);

  /**
   * Check whether an application should be allowed to use offline APIs.
   */
  static PRBool OfflineAppAllowed(nsIPrincipal *aPrincipal);

  /**
   * Increases the count of blockers preventing scripts from running.
   * NOTE: You might want to use nsAutoScriptBlocker rather than calling
   * this directly
   */
  static void AddScriptBlocker();

  /**
   * Decreases the count of blockers preventing scripts from running.
   * NOTE: You might want to use nsAutoScriptBlocker rather than calling
   * this directly
   *
   * WARNING! Calling this function could synchronously execute scripts.
   */
  static void RemoveScriptBlocker();

  /**
   * Add a runnable that is to be executed as soon as it's safe to execute
   * scripts.
   * NOTE: If it's currently safe to execute scripts, aRunnable will be run
   *       synchronously before the function returns.
   *
   * @param aRunnable  The nsIRunnable to run as soon as it's safe to execute
   *                   scripts. Passing null is allowed and results in nothing
   *                   happening. It is also allowed to pass an object that
   *                   has not yet been AddRefed.
   */
  static PRBool AddScriptRunner(nsIRunnable* aRunnable);

  /**
   * Returns true if it's safe to execute content script and false otherwise.
   *
   * The only known case where this lies is mutation events. They run, and can
   * run anything else, when this function returns false, but this is ok.
   */
  static PRBool IsSafeToRunScript() {
    return sScriptBlockerCount == 0;
  }

  /**
   * Get/Set the current number of removable updates. Currently only
   * UPDATE_CONTENT_MODEL updates are removable, and only when firing mutation
   * events. These functions should only be called by mozAutoDocUpdateRemover.
   * The count is also adjusted by the normal calls to BeginUpdate/EndUpdate.
   */
  static void AddRemovableScriptBlocker()
  {
    AddScriptBlocker();
    ++sRemovableScriptBlockerCount;
  }
  static void RemoveRemovableScriptBlocker()
  {
    NS_ASSERTION(sRemovableScriptBlockerCount != 0,
                "Number of removable blockers should never go below zero");
    --sRemovableScriptBlockerCount;
    RemoveScriptBlocker();
  }
  static PRUint32 GetRemovableScriptBlockerLevel()
  {
    return sRemovableScriptBlockerCount;
  }

  /* Process viewport META data. This gives us information for the scale
   * and zoom of a page on mobile devices. We stick the information in
   * the document header and use it later on after rendering.
   *
   * See Bug #436083
   */
  static nsresult ProcessViewportInfo(nsIDocument *aDocument,
                                      const nsAString &viewportInfo);

  static nsresult GetContextForEventHandlers(nsINode* aNode,
                                             nsIScriptContext** aContext);

  static JSContext *GetCurrentJSContext();

                                             
  static nsIInterfaceRequestor* GetSameOriginChecker();

  static nsIThreadJSContextStack* ThreadJSContextStack()
  {
    return sThreadJSContextStack;
  }
private:

  static PRBool InitializeEventTable();

  static nsresult doReparentContentWrapper(nsIContent *aChild,
                                           JSContext *cx,
                                           JSObject *aOldGlobal,
                                           JSObject *aNewGlobal,
                                           nsIDocument *aOldDocument,
                                           nsIDocument *aNewDocument);

  static nsresult EnsureStringBundle(PropertiesFile aFile);

  static nsIDOMScriptObjectFactory *GetDOMScriptObjectFactory();

  static nsresult HoldScriptObject(PRUint32 aLangID, void* aObject);
  static void DropScriptObject(PRUint32 aLangID, void *aObject, void *aClosure);

  static PRBool CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                nsIPrincipal* aPrincipal);

  static nsIDOMScriptObjectFactory *sDOMScriptObjectFactory;

  static nsIXPConnect *sXPConnect;

  static nsIScriptSecurityManager *sSecurityManager;

  static nsIThreadJSContextStack *sThreadJSContextStack;

  static nsIParserService *sParserService;

  static nsINameSpaceManager *sNameSpaceManager;

  static nsIIOService *sIOService;

#ifdef MOZ_XTF
  static nsIXTFService *sXTFService;
#endif

  static nsIPrefBranch *sPrefBranch;

  static nsIPref *sPref;

  static imgILoader* sImgLoader;

  static nsIConsoleService* sConsoleService;

  static nsDataHashtable<nsISupportsHashKey, EventNameMapping>* sEventTable;

  static nsIStringBundleService* sStringBundleService;
  static nsIStringBundle* sStringBundles[PropertiesFile_COUNT];

  static nsIContentPolicy* sContentPolicyService;
  static PRBool sTriedToGetContentPolicy;

  static nsILineBreaker* sLineBreaker;
  static nsIWordBreaker* sWordBreaker;
  static nsICaseConversion* sCaseConv;
  static nsIUGenCategory* sGenCat;

  // Holds pointers to nsISupports* that should be released at shutdown
  static nsVoidArray* sPtrsToPtrsToRelease;

  static nsIScriptRuntime* sScriptRuntimes[NS_STID_ARRAY_UBOUND];
  static PRInt32 sScriptRootCount[NS_STID_ARRAY_UBOUND];
  static PRUint32 sJSGCThingRootCount;

#ifdef IBMBIDI
  static nsIBidiKeyboard* sBidiKeyboard;
#endif

  static PRBool sInitialized;
  static PRUint32 sScriptBlockerCount;
  static PRUint32 sRemovableScriptBlockerCount;
  static nsCOMArray<nsIRunnable>* sBlockedScriptRunners;
  static PRUint32 sRunnersCountAtFirstBlocker;

  static nsIInterfaceRequestor* sSameOriginChecker;
};

#define NS_HOLD_JS_OBJECTS(obj, clazz)                                         \
  nsContentUtils::HoldJSObjects(NS_CYCLE_COLLECTION_UPCAST(obj, clazz),        \
                                &NS_CYCLE_COLLECTION_NAME(clazz))

#define NS_DROP_JS_OBJECTS(obj, clazz)                                         \
  nsContentUtils::DropJSObjects(NS_CYCLE_COLLECTION_UPCAST(obj, clazz))


class NS_STACK_CLASS nsCxPusher
{
public:
  nsCxPusher();
  ~nsCxPusher(); // Calls Pop();

  // Returns PR_FALSE if something erroneous happened.
  PRBool Push(nsPIDOMEventTarget *aCurrentTarget);
  PRBool Push(JSContext *cx);
  void Pop();

private:
  nsCOMPtr<nsIScriptContext> mScx;
  PRBool mScriptIsRunning;
};

class nsAutoGCRoot {
public:
  // aPtr should be the pointer to the jsval we want to protect
  nsAutoGCRoot(jsval* aPtr, nsresult* aResult) :
    mPtr(aPtr)
  {
    mResult = *aResult = AddJSGCRoot(aPtr, "nsAutoGCRoot");
  }

  // aPtr should be the pointer to the JSObject* we want to protect
  nsAutoGCRoot(JSObject** aPtr, nsresult* aResult) :
    mPtr(aPtr)
  {
    mResult = *aResult = AddJSGCRoot(aPtr, "nsAutoGCRoot");
  }

  // aPtr should be the pointer to the thing we want to protect
  nsAutoGCRoot(void* aPtr, nsresult* aResult) :
    mPtr(aPtr)
  {
    mResult = *aResult = AddJSGCRoot(aPtr, "nsAutoGCRoot");
  }

  ~nsAutoGCRoot() {
    if (NS_SUCCEEDED(mResult)) {
      RemoveJSGCRoot(mPtr);
    }
  }

  static void Shutdown();

private:
  static nsresult AddJSGCRoot(void *aPtr, const char* aName);
  static nsresult RemoveJSGCRoot(void *aPtr);

  static nsIJSRuntimeService* sJSRuntimeService;
  static JSRuntime* sJSScriptRuntime;

  void* mPtr;
  nsresult mResult;
};

class nsAutoScriptBlocker {
public:
  nsAutoScriptBlocker() {
    nsContentUtils::AddScriptBlocker();
  }
  ~nsAutoScriptBlocker() {
    nsContentUtils::RemoveScriptBlocker();
  }
};

class mozAutoRemovableBlockerRemover
{
public:
  mozAutoRemovableBlockerRemover()
  {
    mNestingLevel = nsContentUtils::GetRemovableScriptBlockerLevel();
    for (PRUint32 i = 0; i < mNestingLevel; ++i) {
      nsContentUtils::RemoveRemovableScriptBlocker();
    }

    NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "killing mutation events");
  }

  ~mozAutoRemovableBlockerRemover()
  {
    NS_ASSERTION(nsContentUtils::GetRemovableScriptBlockerLevel() == 0,
                 "Should have had none");
    for (PRUint32 i = 0; i < mNestingLevel; ++i) {
      nsContentUtils::AddRemovableScriptBlocker();
    }
  }

private:
  PRUint32 mNestingLevel;
};

/**
 * Class used to detect unexpected mutations. To use the class create an
 * nsMutationGuard on the stack before unexpected mutations could occur.
 * You can then at any time call Mutated to check if any unexpected mutations
 * have occured.
 *
 * When a guard is instantiated sMutationCount is set to 300. It is then
 * decremented by every mutation (capped at 0). This means that we can only
 * detect 300 mutations during the lifetime of a single guard, however that
 * should be more then we ever care about as we usually only care if more then
 * one mutation has occured.
 *
 * When the guard goes out of scope it will adjust sMutationCount so that over
 * the lifetime of the guard the guard itself has not affected sMutationCount,
 * while mutations that happened while the guard was alive still will. This
 * allows a guard to be instantiated even if there is another guard higher up
 * on the callstack watching for mutations.
 *
 * The only thing that has to be avoided is for an outer guard to be used
 * while an inner guard is alive. This can be avoided by only ever
 * instantiating a single guard per scope and only using the guard in the
 * current scope.
 */
class nsMutationGuard {
public:
  nsMutationGuard()
  {
    mDelta = eMaxMutations - sMutationCount;
    sMutationCount = eMaxMutations;
  }
  ~nsMutationGuard()
  {
    sMutationCount =
      mDelta > sMutationCount ? 0 : sMutationCount - mDelta;
  }

  /**
   * Returns true if any unexpected mutations have occured. You can pass in
   * an 8-bit ignore count to ignore a number of expected mutations.
   */
  PRBool Mutated(PRUint8 aIgnoreCount)
  {
    return sMutationCount < static_cast<PRUint32>(eMaxMutations - aIgnoreCount);
  }

  // This function should be called whenever a mutation that we want to keep
  // track of happen. For now this is only done when children are added or
  // removed, but we might do it for attribute changes too in the future.
  static void DidMutate()
  {
    if (sMutationCount) {
      --sMutationCount;
    }
  }

private:
  // mDelta is the amount sMutationCount was adjusted when the guard was
  // initialized. It is needed so that we can undo that adjustment once
  // the guard dies.
  PRUint32 mDelta;

  // The value 300 is not important, as long as it is bigger then anything
  // ever passed to Mutated().
  enum { eMaxMutations = 300 };

  
  // sMutationCount is a global mutation counter which is decreased by one at
  // every mutation. It is capped at 0 to avoid wrapping.
  // It's value is always between 0 and 300, inclusive.
  static PRUint32 sMutationCount;
};

#define NS_AUTO_GCROOT_PASTE2(tok,line) tok##line
#define NS_AUTO_GCROOT_PASTE(tok,line) \
  NS_AUTO_GCROOT_PASTE2(tok,line)
#define NS_AUTO_GCROOT(ptr, result) \ \
  nsAutoGCRoot NS_AUTO_GCROOT_PASTE(_autoGCRoot_, __LINE__) \
  (ptr, result)

#define NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(_class)                      \
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)

#define NS_INTERFACE_MAP_ENTRY_TEAROFF(_interface, _allocator)                \
  if (aIID.Equals(NS_GET_IID(_interface))) {                                  \
    foundInterface = static_cast<_interface *>(_allocator);                   \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

/*
 * Check whether a floating point number is finite (not +/-infinity and not a
 * NaN value). We wrap JSDOUBLE_IS_FINITE in a function because it expects to
 * take the address of its argument, and because the argument must be of type
 * jsdouble to have the right size and layout of bits.
 *
 * Note: we could try to exploit the fact that |infinity - infinity == NaN|
 * instead of using JSDOUBLE_IS_FINITE. This would produce more compact code
 * and perform better by avoiding type conversions and bit twiddling.
 * Unfortunately, some architectures don't guarantee that |f == f| evaluates
 * to true (where f is any *finite* floating point number). See
 * https://bugzilla.mozilla.org/show_bug.cgi?id=369418#c63 . To play it safe
 * for gecko 1.9, we just reuse JSDOUBLE_IS_FINITE.
 */
inline NS_HIDDEN_(PRBool) NS_FloatIsFinite(jsdouble f) {
  return JSDOUBLE_IS_FINITE(f);
}

/*
 * In the following helper macros we exploit the fact that the result of a
 * series of additions will not be finite if any one of the operands in the
 * series is not finite.
 */
#define NS_ENSURE_FINITE(f, rv)                                               \
  if (!NS_FloatIsFinite(f)) {                                                 \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE2(f1, f2, rv)                                         \
  if (!NS_FloatIsFinite((f1)+(f2))) {                                         \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE3(f1, f2, f3, rv)                                     \
  if (!NS_FloatIsFinite((f1)+(f2)+(f3))) {                                    \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE4(f1, f2, f3, f4, rv)                                 \
  if (!NS_FloatIsFinite((f1)+(f2)+(f3)+(f4))) {                               \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE5(f1, f2, f3, f4, f5, rv)                             \
  if (!NS_FloatIsFinite((f1)+(f2)+(f3)+(f4)+(f5))) {                          \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE6(f1, f2, f3, f4, f5, f6, rv)                         \
  if (!NS_FloatIsFinite((f1)+(f2)+(f3)+(f4)+(f5)+(f6))) {                     \
    return (rv);                                                              \
  }

#endif /* nsContentUtils_h___ */
