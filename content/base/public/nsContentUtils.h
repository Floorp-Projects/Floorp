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

#include <math.h>
#if defined(XP_WIN) || defined(XP_OS2)
#include <float.h>
#endif

#if defined(SOLARIS)
#include <ieeefp.h>
#endif

#include "nsAString.h"
#include "nsIStatefulFrame.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsContentList.h"
#include "nsDOMClassInfoID.h"
#include "nsIXPCScriptable.h"
#include "nsIDOM3Node.h"
#include "nsDataHashtable.h"
#include "nsIScriptRuntime.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMEvent.h"
#include "nsTArray.h"
#include "nsTextFragment.h"
#include "nsReadableUtils.h"
#include "nsIPrefBranch2.h"
#include "mozilla/AutoRestore.h"
#include "nsINode.h"
#include "nsHashtable.h"

#include "jsapi.h"

struct nsNativeKeyEvent; // Don't include nsINativeKeyBindings.h here: it will force strange compilation error!

class nsIDOMScriptObjectFactory;
class nsIXPConnect;
class nsIContent;
class nsIDOMNode;
class nsIDOMKeyEvent;
class nsIDocument;
class nsIDocumentObserver;
class nsIDocShell;
class nsINameSpaceManager;
class nsIScriptSecurityManager;
class nsIJSContextStack;
class nsIThreadJSContextStack;
class nsIParserService;
class nsIIOService;
class nsIURI;
class imgIContainer;
class imgIDecoderObserver;
class imgIRequest;
class imgILoader;
class imgICache;
class nsIPrefBranch2;
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
template<class K, class V> class nsRefPtrHashtable;
struct JSRuntime;
class nsIUGenCategory;
class nsIWidget;
class nsIDragSession;
class nsPIDOMWindow;
class nsPIDOMEventTarget;
class nsIPresShell;
class nsIXPConnectJSObjectHolder;
class nsPrefOldCallback;
class nsPrefObserverHashKey;
#ifdef MOZ_XTF
class nsIXTFService;
#endif
#ifdef IBMBIDI
class nsIBidiKeyboard;
#endif
class nsIMIMEHeaderParam;
class nsIObserver;
class nsPresContext;
class nsIChannel;
struct nsIntMargin;
class nsPIDOMWindow;

#ifndef have_PrefChangedFunc_typedef
typedef int (*PR_CALLBACK PrefChangedFunc)(const char *, void *);
#define have_PrefChangedFunc_typedef
#endif

namespace mozilla {
  class IHistory;

namespace layers {
  class LayerManager;
} // namespace layers

namespace dom {
class Element;
} // namespace dom

} // namespace mozilla

extern const char kLoadAsData[];

enum EventNameType {
  EventNameType_None = 0x0000,
  EventNameType_HTML = 0x0001,
  EventNameType_XUL = 0x0002,
  EventNameType_SVGGraphic = 0x0004, // svg graphic elements
  EventNameType_SVGSVG = 0x0008, // the svg element
  EventNameType_SMIL = 0x0016, // smil elements

  EventNameType_HTMLXUL = 0x0003,
  EventNameType_All = 0xFFFF
};

struct EventNameMapping
{
  nsIAtom* mAtom;
  PRUint32 mId;
  PRInt32  mType;
  PRUint32 mStructType;
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
  typedef mozilla::dom::Element Element;

public:
  static nsresult Init();

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
   * Returns the parent node of aChild crossing document boundaries.
   */
  static nsINode* GetCrossDocParentNode(nsINode* aChild);

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
  static PRBool ContentIsDescendantOf(const nsINode* aPossibleDescendant,
                                      const nsINode* aPossibleAncestor);

  /**
   * Similar to ContentIsDescendantOf except it crosses document boundaries.
   */
  static PRBool ContentIsCrossDocDescendantOf(nsINode* aPossibleDescendant,
                                              nsINode* aPossibleAncestor);

  /*
   * This method fills the |aArray| with all ancestor nodes of |aNode|
   * including |aNode| at the zero index.
   */
  static nsresult GetAncestors(nsINode* aNode,
                               nsTArray<nsINode*>& aArray);

  /*
   * This method fills |aAncestorNodes| with all ancestor nodes of |aNode|
   * including |aNode| (QI'd to nsIContent) at the zero index.
   * For each ancestor, there is a corresponding element in |aAncestorOffsets|
   * which is the IndexOf the child in relation to its parent.
   *
   * This method just sucks.
   */
  static nsresult GetAncestorsAndOffsets(nsIDOMNode* aNode,
                                         PRInt32 aOffset,
                                         nsTArray<nsIContent*>* aAncestorNodes,
                                         nsTArray<PRInt32>* aAncestorOffsets);

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
   * Returns true if aNode1 is before aNode2 in the same connected
   * tree.
   */
  static PRBool PositionIsBefore(nsINode* aNode1,
                                 nsINode* aNode2)
  {
    return (aNode2->CompareDocumentPosition(aNode1) &
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
   *  the result is 1, and the optional aDisconnected parameter
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
  static Element* MatchElementId(nsIContent *aContent, const nsAString& aId);

  /**
   * Similar to above, but to be used if one already has an atom for the ID
   */
  static Element* MatchElementId(nsIContent *aContent, const nsIAtom* aId);

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

  template<PRBool IsWhitespace(PRUnichar)>
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

  /**
   * Parse a margin string of format 'top, right, bottom, left' into
   * an nsIntMargin.
   *
   * @param aString the string to parse
   * @param aResult the resulting integer
   * @return whether the value could be parsed
   */
  static PRBool ParseIntMarginValue(const nsAString& aString, nsIntMargin& aResult);

  static void Shutdown();

  /**
   * Checks whether two nodes come from the same origin.
   */
  static nsresult CheckSameOrigin(nsINode* aTrustedNode,
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
   * Get the window through the JS context that's currently on the stack.
   * If there's no JS context currently on the stack, returns null.
   */
  static nsPIDOMWindow *GetWindowFromCaller();

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
    if (!sImgLoaderInitialized)
      InitImgLoader();
    return sImgLoader;
  }

  static mozilla::IHistory* GetHistory()
  {
    return sHistory;
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
                                   const nsIDocument* aDocument,
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
                            nsACString& aCharset, PRBool *bigEndian = nsnull);


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

  static nsresult SplitQName(const nsIContent* aNamespaceResolver,
                             const nsAFlatString& aQName,
                             PRInt32 *aNamespace, nsIAtom **aLocalName);

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
  static void AddBoolPrefVarCache(const char* aPref, PRBool* aVariable,
                                  PRBool aDefault = PR_FALSE);
  static void AddIntPrefVarCache(const char* aPref, PRInt32* aVariable,
                                 PRInt32 aDefault = 0);
  static nsIPrefBranch2 *GetPrefBranch()
  {
    return sPrefBranch;
  }

  // Get a permission-manager setting for the given uri and type.
  // If the pref doesn't exist or if it isn't ALLOW_ACTION, PR_FALSE is
  // returned, otherwise PR_TRUE is returned.
  static PRBool IsSitePermAllow(nsIURI* aURI, const char* aType);

  static nsILineBreaker* LineBreaker()
  {
    return sLineBreaker;
  }

  static nsIWordBreaker* WordBreaker()
  {
    return sWordBreaker;
  }

  static nsIUGenCategory* GetGenCat()
  {
    return sGenCat;
  }

  /**
   * Regster aObserver as a shutdown observer. A strong reference is held
   * to aObserver until UnregisterShutdownObserver is called.
   */
  static void RegisterShutdownObserver(nsIObserver* aObserver);
  static void UnregisterShutdownObserver(nsIObserver* aObserver);

  /**
   * @return PR_TRUE if aContent has an attribute aName in namespace aNameSpaceID,
   * and the attribute value is non-empty.
   */
  static PRBool HasNonEmptyAttr(const nsIContent* aContent, PRInt32 aNameSpaceID,
                                nsIAtom* aName);

  /**
   * Method that gets the primary presContext for the node.
   * 
   * @param aContent The content node.
   * @return the presContext, or nsnull if the content is not in a document
   *         (if GetCurrentDoc returns nsnull)
   */
  static nsPresContext* GetContextForContent(const nsIContent* aContent);

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
   * Returns whether the given URI is in the image cache.
   */
  static PRBool IsImageInCache(nsIURI* aURI);

  /**
   * Method to get an imgIContainer from an image loading content
   *
   * @param aContent The image loading content.  Must not be null.
   * @param aRequest The image request [out]
   * @return the imgIContainer corresponding to the first frame of the image
   */
  static already_AddRefed<imgIContainer> GetImageFromContent(nsIImageLoadingContent* aContent, imgIRequest **aRequest = nsnull);

  /**
   * Helper method to call imgIRequest::GetStaticRequest.
   */
  static already_AddRefed<imgIRequest> GetStaticRequest(imgIRequest* aRequest);

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
  static PRBool IsDraggableLink(const nsIContent* aContent);

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
  static PRBool IsInSameAnonymousTree(const nsINode* aNode, const nsIContent* aContent);

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
   * Returns true if aDocument is in a docshell whose parent is the same type
   */
  static PRBool IsChildOfSameType(nsIDocument* aDoc);

  /**
   * Get the script file name to use when compiling the script
   * referenced by aURI. In cases where there's no need for any extra
   * security wrapper automation the script file name that's returned
   * will be the spec in aURI, else it will be the spec in aDocument's
   * URI followed by aURI's spec, separated by " -> ". Returns PR_TRUE
   * if the script file name was modified, PR_FALSE if it's aURI's
   * spec.
   */
  static PRBool GetWrapperSafeScriptFilename(nsIDocument *aDocument,
                                             nsIURI *aURI,
                                             nsACString& aScriptURI);


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
    return sPtrsToPtrsToRelease->AppendElement(aSupportsPtr) != nsnull ? NS_OK :
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
   * This method creates and dispatches a trusted event to the chrome
   * event handler.
   * Works only with events which can be created by calling
   * nsIDOMDocumentEvent::CreateEvent() with parameter "Events".
   * @param aDocument      The document which will be used to create the event,
   *                       and whose window's chrome handler will be used to
   *                       dispatch the event.
   * @param aTarget        The target of the event, used for event->SetTarget()
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see nsIDOMEventTarget::DispatchEvent.
   */
  static nsresult DispatchChromeEvent(nsIDocument* aDoc,
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
   * Return the event id and atom for the event with the given name.
   * The name is the event name *without* the 'on' prefix.
   * Returns NS_USER_DEFINED_EVENT on the aEventID if the
   * event doesn't match a known event name in the category.
   *
   * @param aName the event name to look up
   * @param aEventStruct only return event id in aEventStruct category
   */
  static nsIAtom* GetEventIdAndAtom(const nsAString& aName,
                                    PRUint32 aEventStruct,
                                    PRUint32* aEventID);

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
   */
  static nsIEventListenerManager* GetListenerManager(nsINode* aNode,
                                                     PRBool aCreateIfNotFound);

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
   * Note! In the HTML case with the HTML5 parser enabled, this is only called
   * from Range.createContextualFragment() and the implementation here is
   * quirky accordingly (html context node behaves like a body context node).
   * If you don't want that quirky behavior, don't use this method as-is!
   *
   * @param aContextNode the node which is used to resolve namespaces
   * @param aFragment the string which is parsed to a DocumentFragment
   * @param aWillOwnFragment is PR_TRUE if ownership of the fragment should be
   *                         transferred to the caller.
   * @param aReturn [out] the created DocumentFragment
   */
  static nsresult CreateContextualFragment(nsINode* aContextNode,
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

#ifdef DEBUG
  static void CheckCCWrapperTraversal(nsISupports* aScriptObjectHolder,
                                      nsWrapperCache* aCache);
#endif

  static void PreserveWrapper(nsISupports* aScriptObjectHolder,
                              nsWrapperCache* aCache)
  {
    if (!aCache->PreservingWrapper()) {
      nsXPCOMCycleCollectionParticipant* participant;
      CallQueryInterface(aScriptObjectHolder, &participant);
      HoldJSObjects(aScriptObjectHolder, participant);
      aCache->SetPreservingWrapper(PR_TRUE);
#ifdef DEBUG
      // Make sure the cycle collector will be able to traverse to the wrapper.
      CheckCCWrapperTraversal(aScriptObjectHolder, aCache);
#endif
    }
  }
  static void ReleaseWrapper(nsISupports* aScriptObjectHolder,
                             nsWrapperCache* aCache)
  {
    if (aCache->PreservingWrapper()) {
      DropJSObjects(aScriptObjectHolder);
      aCache->SetPreservingWrapper(PR_FALSE);
    }
  }
  static void TraceWrapper(nsWrapperCache* aCache, TraceCallback aCallback,
                           void *aClosure)
  {
    if (aCache->PreservingWrapper()) {
      aCallback(nsIProgrammingLanguage::JAVASCRIPT, aCache->GetWrapper(),
                aClosure);
    }
  }

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
   * Returns true if aPrincipal is the system principal.
   */
  static PRBool IsSystemPrincipal(nsIPrincipal* aPrincipal);

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
   * displayed in child frames. Does nothing if aDocument is null.
   */
  static void HidePopupsInDocument(nsIDocument* aDocument);

  /**
   * Retrieve the current drag session, or null if no drag is currently occuring
   */
  static already_AddRefed<nsIDragSession> GetDragSession();

  /*
   * Initialize and set the dataTransfer field of an nsDragEvent.
   */
  static nsresult SetDataTransferInEvent(nsDragEvent* aDragEvent);

  // filters the drag and drop action to fit within the effects allowed and
  // returns it.
  static PRUint32 FilterDropEffect(PRUint32 aAction, PRUint32 aEffectAllowed);

  /**
   * Return true if aURI is a local file URI (i.e. file://).
   */
  static PRBool URIIsLocalFile(nsIURI *aURI);

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
   * Increases the count of blockers preventing scripts from running.
   * Also, while this script blocker is active, script runners must not be
   * added --- we'll assert if one is, and ignore it.
   */
  static void AddScriptBlockerAndPreventAddingRunners();

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
   * @return false on out of memory, true otherwise.
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

  static nsIScriptContext* GetContextForEventHandlers(nsINode* aNode,
                                                      nsresult* aRv);

  static JSContext *GetCurrentJSContext();

  /**
   * Case insensitive comparison between two strings. However it only ignores
   * case for ASCII characters a-z.
   */
  static PRBool EqualsIgnoreASCIICase(const nsAString& aStr1,
                                      const nsAString& aStr2);

  /**
   * Convert ASCII A-Z to a-z.
   */
  static void ASCIIToLower(nsAString& aStr);
  static void ASCIIToLower(const nsAString& aSource, nsAString& aDest);

  /**
   * Convert ASCII a-z to A-Z.
   */
  static void ASCIIToUpper(nsAString& aStr);
  static void ASCIIToUpper(const nsAString& aSource, nsAString& aDest);

  static nsIInterfaceRequestor* GetSameOriginChecker();

  static nsIThreadJSContextStack* ThreadJSContextStack()
  {
    return sThreadJSContextStack;
  }
  

  /**
   * Get the Origin of the passed in nsIPrincipal or nsIURI. If the passed in
   * nsIURI or the URI of the passed in nsIPrincipal does not have a host, the
   * origin is set to 'null'.
   *
   * The ASCII versions return a ASCII strings that are puny-code encoded,
   * suitable for, for example, header values. The UTF versions return strings
   * containing international characters.
   *
   * @pre aPrincipal/aOrigin must not be null.
   *
   * @note this should be used for HTML5 origin determination.
   */
  static nsresult GetASCIIOrigin(nsIPrincipal* aPrincipal,
                                 nsCString& aOrigin);
  static nsresult GetASCIIOrigin(nsIURI* aURI, nsCString& aOrigin);
  static nsresult GetUTFOrigin(nsIPrincipal* aPrincipal,
                               nsString& aOrigin);
  static nsresult GetUTFOrigin(nsIURI* aURI, nsString& aOrigin);

  /**
   * This method creates and dispatches "command" event, which implements
   * nsIDOMXULCommandEvent.
   * If aShell is not null, dispatching goes via
   * nsIPresShell::HandleDOMEventWithTarget.
   */
  static nsresult DispatchXULCommand(nsIContent* aTarget,
                                     PRBool aTrusted,
                                     nsIDOMEvent* aSourceEvent = nsnull,
                                     nsIPresShell* aShell = nsnull,
                                     PRBool aCtrl = PR_FALSE,
                                     PRBool aAlt = PR_FALSE,
                                     PRBool aShift = PR_FALSE,
                                     PRBool aMeta = PR_FALSE);

  /**
   * Gets the nsIDocument given the script context. Will return nsnull on failure.
   *
   * @param aScriptContext the script context to get the document for; can be null
   *
   * @return the document associated with the script context
   */
  static already_AddRefed<nsIDocument>
  GetDocumentFromScriptContext(nsIScriptContext *aScriptContext);

  static PRBool CheckMayLoad(nsIPrincipal* aPrincipal, nsIChannel* aChannel);

  /**
   * The method checks whether the caller can access native anonymous content.
   * If there is no JS in the stack or privileged JS is running, this
   * method returns PR_TRUE, otherwise PR_FALSE.
   */
  static PRBool CanAccessNativeAnon();

  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, const nsIID* aIID, jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nsnull,
                             PRBool aAllowWrapping = PR_FALSE)
  {
    return WrapNative(cx, scope, native, nsnull, aIID, vp, aHolder,
                      aAllowWrapping);
  }

  // Same as the WrapNative above, but use this one if aIID is nsISupports' IID.
  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nsnull,
                             PRBool aAllowWrapping = PR_FALSE)
  {
    return WrapNative(cx, scope, native, nsnull, nsnull, vp, aHolder,
                      aAllowWrapping);
  }
  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, nsWrapperCache *cache,
                             jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nsnull,
                             PRBool aAllowWrapping = PR_FALSE)
  {
    return WrapNative(cx, scope, native, cache, nsnull, vp, aHolder,
                      aAllowWrapping);
  }

  static void StripNullChars(const nsAString& aInStr, nsAString& aOutStr);

  /**
   * Creates a structured clone of the given jsval according to the algorithm
   * at:
   *     http://www.whatwg.org/specs/web-apps/current-work/multipage/
   *                                   urls.html#safe-passing-of-structured-data
   *
   * If the function returns a success code then rval is set to point at the
   * cloned jsval. rval is not set if the function returns a failure code.
   */
  static nsresult CreateStructuredClone(JSContext* cx, jsval val, jsval* rval);

  /**
   * Reparents the given object and all subobjects to the given scope. Also
   * fixes all the prototypes. Assumes obj is properly rooted, that obj has no
   * getter functions that can cause side effects, and that the only types of
   * objects nested within obj are the types that are cloneable via the
   * CreateStructuredClone function above.
   */
  static nsresult ReparentClonedObjectToScope(JSContext* cx, JSObject* obj,
                                              JSObject* scope);

  /**
   * Strip all \n, \r and nulls from the given string
   * @param aString the string to remove newlines from [in/out]
   */
  static void RemoveNewlines(nsString &aString);

  /**
   * Convert Windows and Mac platform linebreaks to \n.
   * @param aString the string to convert the newlines inside [in/out]
   */
  static void PlatformToDOMLineBreaks(nsString &aString);

  static PRBool IsHandlingKeyBoardEvent()
  {
    return sIsHandlingKeyBoardEvent;
  }

  static void SetIsHandlingKeyBoardEvent(PRBool aHandling)
  {
    sIsHandlingKeyBoardEvent = aHandling;
  }

  /**
   * Utility method for getElementsByClassName.  aRootNode is the node (either
   * document or element), which getElementsByClassName was called on.
   */
  static nsresult GetElementsByClassName(nsINode* aRootNode,
                                         const nsAString& aClasses,
                                         nsIDOMNodeList** aReturn);

  /**
   * Returns a layer manager to use for the given document. Basically we
   * look up the document hierarchy for the first document which has
   * a presentation with an associated widget, and use that widget's
   * layer manager.
   *
   * If one can't be found, a BasicLayerManager is created and returned.
   *
   * @param aDoc the document for which to return a layer manager.
   */
  static already_AddRefed<mozilla::layers::LayerManager>
  LayerManagerForDocument(nsIDocument *aDoc);

  /**
   * Determine whether a content node is focused or not,
   *
   * @param aContent the content node to check
   * @return true if the content node is focused, false otherwise.
   */
  static PRBool IsFocusedContent(const nsIContent *aContent);

  /**
   * Returns if aContent has a tabbable subdocument.
   * A sub document isn't tabbable when it's a zombie document.
   *
   * @param aElement element to test.
   *
   * @return Whether the subdocument is tabbable.
   */
  static bool IsSubDocumentTabbable(nsIContent* aContent);

  /**
   * Flushes the layout tree (recursively)
   *
   * @param aWindow the window the flush should start at
   *
   */
  static void FlushLayoutForTree(nsIDOMWindow* aWindow);

private:

  static PRBool InitializeEventTable();

  static nsresult EnsureStringBundle(PropertiesFile aFile);

  static nsIDOMScriptObjectFactory *GetDOMScriptObjectFactory();

  static nsresult HoldScriptObject(PRUint32 aLangID, void* aObject);
  static void DropScriptObject(PRUint32 aLangID, void *aObject, void *aClosure);

  static PRBool CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                nsIPrincipal* aPrincipal);

  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, nsWrapperCache *cache,
                             const nsIID* aIID, jsval *vp,
                             nsIXPConnectJSObjectHolder** aHolder,
                             PRBool aAllowWrapping);

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

  static nsIPrefBranch2 *sPrefBranch;
  // For old compatibility of RegisterPrefCallback
  static nsRefPtrHashtable<nsPrefObserverHashKey, nsPrefOldCallback>
    *sPrefCallbackTable;

  static bool sImgLoaderInitialized;
  static void InitImgLoader();

  // The following two members are initialized lazily
  static imgILoader* sImgLoader;
  static imgICache* sImgCache;

  static mozilla::IHistory* sHistory;

  static nsIConsoleService* sConsoleService;

  static nsDataHashtable<nsISupportsHashKey, EventNameMapping>* sAtomEventTable;
  static nsDataHashtable<nsStringHashKey, EventNameMapping>* sStringEventTable;
  static nsCOMArray<nsIAtom>* sUserDefinedEvents;

  static nsIStringBundleService* sStringBundleService;
  static nsIStringBundle* sStringBundles[PropertiesFile_COUNT];

  static nsIContentPolicy* sContentPolicyService;
  static PRBool sTriedToGetContentPolicy;

  static nsILineBreaker* sLineBreaker;
  static nsIWordBreaker* sWordBreaker;
  static nsIUGenCategory* sGenCat;

  // Holds pointers to nsISupports* that should be released at shutdown
  static nsTArray<nsISupports**>* sPtrsToPtrsToRelease;

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
  static PRUint32 sScriptBlockerCountWhereRunnersPrevented;

  static nsIInterfaceRequestor* sSameOriginChecker;

  static PRBool sIsHandlingKeyBoardEvent;
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
  // If nothing has been pushed to stack, this works like Push.
  // Otherwise if context will change, Pop and Push will be called.
  PRBool RePush(nsPIDOMEventTarget *aCurrentTarget);
  // If a null JSContext is passed to Push(), that will cause no
  // push to happen and false to be returned.
  PRBool Push(JSContext *cx, PRBool aRequiresScriptContext = PR_TRUE);
  // Explicitly push a null JSContext on the the stack
  PRBool PushNull();

  // Pop() will be a no-op if Push() or PushNull() fail
  void Pop();

  nsIScriptContext* GetCurrentScriptContext() { return mScx; }
private:
  // Combined code for PushNull() and Push(JSContext*)
  PRBool DoPush(JSContext* cx);

  nsCOMPtr<nsIScriptContext> mScx;
  PRBool mScriptIsRunning;
  PRBool mPushedSomething;
#ifdef DEBUG
  JSContext* mPushedContext;
#endif
};

class NS_STACK_CLASS nsAutoGCRoot {
public:
  // aPtr should be the pointer to the jsval we want to protect
  nsAutoGCRoot(jsval* aPtr, nsresult* aResult
               MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM) :
    mPtr(aPtr), mRootType(RootType_JSVal)
  {
    MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
    mResult = *aResult = AddJSGCRoot(aPtr, RootType_JSVal, "nsAutoGCRoot");
  }

  // aPtr should be the pointer to the JSObject* we want to protect
  nsAutoGCRoot(JSObject** aPtr, nsresult* aResult
               MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM) :
    mPtr(aPtr), mRootType(RootType_Object)
  {
    MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
    mResult = *aResult = AddJSGCRoot(aPtr, RootType_Object, "nsAutoGCRoot");
  }

  ~nsAutoGCRoot() {
    if (NS_SUCCEEDED(mResult)) {
      RemoveJSGCRoot((jsval *)mPtr, mRootType);
    }
  }

  static void Shutdown();

private:
  enum RootType { RootType_JSVal, RootType_Object };
  static nsresult AddJSGCRoot(void *aPtr, RootType aRootType, const char* aName);
  static nsresult RemoveJSGCRoot(void *aPtr, RootType aRootType);

  static nsIJSRuntimeService* sJSRuntimeService;
  static JSRuntime* sJSScriptRuntime;

  void* mPtr;
  RootType mRootType;
  nsresult mResult;
  MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class NS_STACK_CLASS nsAutoScriptBlocker {
public:
  nsAutoScriptBlocker(MOZILLA_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
    MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
    nsContentUtils::AddScriptBlocker();
  }
  ~nsAutoScriptBlocker() {
    nsContentUtils::RemoveScriptBlocker();
  }
private:
  MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class NS_STACK_CLASS mozAutoRemovableBlockerRemover
{
public:
  mozAutoRemovableBlockerRemover(nsIDocument* aDocument
                                 MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM);
  ~mozAutoRemovableBlockerRemover();

private:
  PRUint32 mNestingLevel;
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIDocumentObserver> mObserver;
  MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#define NS_AUTO_GCROOT_PASTE2(tok,line) tok##line
#define NS_AUTO_GCROOT_PASTE(tok,line) \
  NS_AUTO_GCROOT_PASTE2(tok,line)
#define NS_AUTO_GCROOT(ptr, result) \ \
  nsAutoGCRoot NS_AUTO_GCROOT_PASTE(_autoGCRoot_, __LINE__) \
  (ptr, result)

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
 * NaN value).
 */
inline NS_HIDDEN_(PRBool) NS_FloatIsFinite(jsdouble f) {
#ifdef WIN32
  return _finite(f);
#else
  return finite(f);
#endif
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

// Deletes a linked list iteratively to avoid blowing up the stack (bug 460444).
#define NS_CONTENT_DELETE_LIST_MEMBER(type_, ptr_, member_)                   \
  {                                                                           \
    type_ *cur = (ptr_)->member_;                                             \
    (ptr_)->member_ = nsnull;                                                 \
    while (cur) {                                                             \
      type_ *next = cur->member_;                                             \
      cur->member_ = nsnull;                                                  \
      delete cur;                                                             \
      cur = next;                                                             \
    }                                                                         \
  }

class nsContentTypeParser {
public:
  nsContentTypeParser(const nsAString& aString);
  ~nsContentTypeParser();

  nsresult GetParameter(const char* aParameterName, nsAString& aResult);
  nsresult GetType(nsAString& aResult)
  {
    return GetParameter(nsnull, aResult);
  }

private:
  NS_ConvertUTF16toUTF8 mString;
  nsIMIMEHeaderParam*   mService;
};

#endif /* nsContentUtils_h___ */
