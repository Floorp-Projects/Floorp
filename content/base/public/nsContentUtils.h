/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

//A trick to handle IEEE floating point exceptions on FreeBSD - E.D.
#ifdef __FreeBSD__
#include <ieeefp.h>
#ifdef __alpha__
static fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP;
#else
static fp_except_t allmask = FP_X_INV|FP_X_OFL|FP_X_UFL|FP_X_DZ|FP_X_IMP|FP_X_DNML;
#endif
static fp_except_t oldmask = fpsetmask(~allmask);
#endif

#include "nsAString.h"
#include "nsIStatefulFrame.h"
#include "nsNodeInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsDataHashtable.h"
#include "nsIDOMEvent.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "nsINode.h"
#include "nsIDOMNode.h"
#include "nsHtml5StringParser.h"
#include "nsIDocument.h"
#include "nsContentSink.h"
#include "nsMathUtils.h"
#include "nsThreadUtils.h"
#include "nsIContent.h"
#include "nsCharSeparatedTokenizer.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/TimeStamp.h"

struct nsNativeKeyEvent; // Don't include nsINativeKeyBindings.h here: it will force strange compilation error!

class nsIDOMScriptObjectFactory;
class nsIXPConnect;
class nsIContent;
class nsIDOMKeyEvent;
class nsIDocument;
class nsIDocumentObserver;
class nsIDocShell;
class nsINameSpaceManager;
class nsIFragmentContentSink;
class nsIScriptGlobalObject;
class nsIScriptSecurityManager;
class nsTextFragment;
class nsIJSContextStack;
class nsIThreadJSContextStack;
class nsIParser;
class nsIParserService;
class nsIIOService;
class nsIURI;
class imgIContainer;
class imgIDecoderObserver;
class imgIRequest;
class imgILoader;
class imgICache;
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
class nsEventListenerManager;
class nsIScriptContext;
class nsIRunnable;
class nsIInterfaceRequestor;
class nsINodeInfo;
template<class E> class nsCOMArray;
template<class K, class V> class nsRefPtrHashtable;
struct JSRuntime;
class nsIWidget;
class nsIDragSession;
class nsIPresShell;
class nsIXPConnectJSObjectHolder;
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
class nsAutoScriptBlockerSuppressNodeRemoved;
struct nsIntMargin;
class nsPIDOMWindow;
class nsIDocumentLoaderFactory;
class nsIDOMHTMLInputElement;

namespace mozilla {

class Selection;

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

/**
 * Information retrieved from the <meta name="viewport"> tag. See
 * GetViewportInfo for more information on this functionality.
 */
struct ViewportInfo
{
    // Default zoom indicates the level at which the display is 'zoomed in'
    // initially for the user, upon loading of the page.
    double defaultZoom;

    // The minimum zoom level permitted by the page.
    double minZoom;

    // The maximum zoom level permitted by the page.
    double maxZoom;

    // The width of the viewport, specified by the <meta name="viewport"> tag,
    // in CSS pixels.
    PRUint32 width;

    // The height of the viewport, specified by the <meta name="viewport"> tag,
    // in CSS pixels.
    PRUint32 height;

    // Whether or not we should automatically size the viewport to the device's
    // width. This is true if the document has been optimized for mobile, and
    // the width property of a specified <meta name="viewport"> tag is either
    // not specified, or is set to the special value 'device-width'.
    bool autoSize;

    // Whether or not the user can zoom in and out on the page. Default is true.
    bool allowZoom;

    // This is a holdover from e10s fennec, and might be removed in the future.
    // It's a hack to work around bugs that didn't allow zooming of documents
    // from within the parent process. It is still used in native Fennec for XUL
    // documents, but it should probably be removed.
    // Currently, from, within GetViewportInfo(), This is only set to false
    // if the document is a XUL document.
    bool autoScale;
};

struct EventNameMapping
{
  nsIAtom* mAtom;
  PRUint32 mId;
  PRInt32  mType;
  PRUint32 mStructType;
};

struct nsShortcutCandidate {
  nsShortcutCandidate(PRUint32 aCharCode, bool aIgnoreShift) :
    mCharCode(aCharCode), mIgnoreShift(aIgnoreShift)
  {
  }
  PRUint32 mCharCode;
  bool     mIgnoreShift;
};

class nsContentUtils
{
  friend class nsAutoScriptBlockerSuppressNodeRemoved;
  typedef mozilla::dom::Element Element;
  typedef mozilla::TimeDuration TimeDuration;

public:
  static nsresult Init();

  /**
   * Get a JSContext from the document's scope object.
   */
  static JSContext* GetContextFromDocument(nsIDocument *aDocument);

  /**
   * When a document's scope changes (e.g., from document.open(), call this
   * function to move all content wrappers from the old scope to the new one.
   */
  static nsresult ReparentContentWrappersInScope(JSContext *cx,
                                                 nsIScriptGlobalObject *aOldScope,
                                                 nsIScriptGlobalObject *aNewScope);

  static bool     IsCallerChrome();

  static bool     IsCallerTrustedForRead();

  static bool     IsCallerTrustedForWrite();

  /**
   * Check whether a caller has UniversalXPConnect.
   */
  static bool     CallerHasUniversalXPConnect();

  static bool     IsImageSrcSetDisabled();

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
   * @return true if aPossibleDescendant is a descendant of
   *         aPossibleAncestor (or is aPossibleAncestor).  false
   *         otherwise.
   */
  static bool ContentIsDescendantOf(const nsINode* aPossibleDescendant,
                                      const nsINode* aPossibleAncestor);

  /**
   * Similar to ContentIsDescendantOf except it crosses document boundaries.
   */
  static bool ContentIsCrossDocDescendantOf(nsINode* aPossibleDescendant,
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
  static bool PositionIsBefore(nsINode* aNode1,
                                 nsINode* aNode2)
  {
    return (aNode2->CompareDocPosition(aNode1) &
      (nsIDOMNode::DOCUMENT_POSITION_PRECEDING |
       nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED)) ==
      nsIDOMNode::DOCUMENT_POSITION_PRECEDING;
  }

  /**
   *  Utility routine to compare two "points", where a point is a
   *  node/offset pair
   *  Returns -1 if point1 < point2, 1, if point1 > point2,
   *  0 if error or if point1 == point2.
   *  NOTE! If the two nodes aren't in the same connected subtree,
   *  the result is 1, and the optional aDisconnected parameter
   *  is set to true.
   */
  static PRInt32 ComparePoints(nsINode* aParent1, PRInt32 aOffset1,
                               nsINode* aParent2, PRInt32 aOffset2,
                               bool* aDisconnected = nullptr);
  static PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                               nsIDOMNode* aParent2, PRInt32 aOffset2,
                               bool* aDisconnected = nullptr);

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
   * Reverses the document position flags passed in.
   *
   * @param   aDocumentPosition   The document position flags to be reversed.
   *
   * @return  The reversed document position flags.
   *
   * @see nsIDOMNode
   */
  static PRUint16 ReverseDocumentPosition(PRUint16 aDocumentPosition);

  static PRUint32 CopyNewlineNormalizedUnicodeTo(const nsAString& aSource,
                                                 PRUint32 aSrcOffset,
                                                 PRUnichar* aDest,
                                                 PRUint32 aLength,
                                                 bool& aLastCharCR);

  static PRUint32 CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAString& aDest);

  static const nsDependentSubstring TrimCharsInSet(const char* aSet,
                                                   const nsAString& aValue);

  template<bool IsWhitespace(PRUnichar)>
  static const nsDependentSubstring TrimWhitespace(const nsAString& aStr,
                                                   bool aTrimTrailing = true);

  /**
   * Returns true if aChar is of class Ps, Pi, Po, Pf, or Pe.
   */
  static bool IsFirstLetterPunctuation(PRUint32 aChar);
  static bool IsFirstLetterPunctuationAt(const nsTextFragment* aFrag, PRUint32 aOffset);
 
  /**
   * Returns true if aChar is of class Lu, Ll, Lt, Lm, Lo, Nd, Nl or No
   */
  static bool IsAlphanumeric(PRUint32 aChar);
  static bool IsAlphanumericAt(const nsTextFragment* aFrag, PRUint32 aOffset);

  /*
   * Is the character an HTML whitespace character?
   *
   * We define whitespace using the list in HTML5 and css3-selectors:
   * U+0009, U+000A, U+000C, U+000D, U+0020
   *
   * HTML 4.01 also lists U+200B (zero-width space).
   */
  static bool IsHTMLWhitespace(PRUnichar aChar);

  /**
   * Is the HTML local name a block element?
   */
  static bool IsHTMLBlock(nsIAtom* aLocalName);

  /**
   * Is the HTML local name a void element?
   */
  static bool IsHTMLVoid(nsIAtom* aLocalName);

  /**
   * Parse a margin string of format 'top, right, bottom, left' into
   * an nsIntMargin.
   *
   * @param aString the string to parse
   * @param aResult the resulting integer
   * @return whether the value could be parsed
   */
  static bool ParseIntMarginValue(const nsAString& aString, nsIntMargin& aResult);

  /**
   * Parse the value of the <font size=""> attribute according to the HTML5
   * spec as of April 16, 2012.
   *
   * @param aValue the value to parse
   * @return 1 to 7, or 0 if the value couldn't be parsed
   */
  static PRInt32 ParseLegacyFontSize(const nsAString& aValue);

  static void Shutdown();

  /**
   * Checks whether two nodes come from the same origin.
   */
  static nsresult CheckSameOrigin(nsINode* aTrustedNode,
                                  nsIDOMNode* aUnTrustedNode);
  static nsresult CheckSameOrigin(nsINode* aTrustedNode,
                                  nsINode* unTrustedNode);

  // Check if the (JS) caller can access aNode.
  static bool CanCallerAccess(nsIDOMNode *aNode);

  // Check if the (JS) caller can access aWindow.
  // aWindow can be either outer or inner window.
  static bool CanCallerAccess(nsPIDOMWindow* aWindow);

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
  static bool InProlog(nsINode *aNode);

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
   * UTF-16BE
   *
   * @param aBuffer the buffer to check
   * @param aLength the length of the buffer
   * @param aCharset empty if not found
   * @return boolean indicating whether a BOM was detected.
   */
  static bool CheckForBOM(const unsigned char* aBuffer, PRUint32 aLength,
                          nsACString& aCharset, bool *bigEndian = nullptr);

  static nsresult GuessCharset(const char *aData, PRUint32 aDataLen,
                               nsACString &aCharset);

  /**
   * Determine whether aContent is in some way associated with aForm.  If the
   * form is a container the only elements that are considered to be associated
   * with a form are the elements that are contained within the form. If the
   * form is a leaf element then all elements will be accepted into this list,
   * since this can happen due to content fixup when a form spans table rows or
   * table cells.
   */
  static bool BelongsInForm(nsIContent *aForm,
                              nsIContent *aContent);

  static nsresult CheckQName(const nsAString& aQualifiedName,
                             bool aNamespaceAware = true,
                             const PRUnichar** aColon = nullptr);

  static nsresult SplitQName(const nsIContent* aNamespaceResolver,
                             const nsAFlatString& aQName,
                             PRInt32 *aNamespace, nsIAtom **aLocalName);

  static nsresult GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                       const nsAString& aQualifiedName,
                                       nsNodeInfoManager* aNodeInfoManager,
                                       PRUint16 aNodeType,
                                       nsINodeInfo** aNodeInfo);

  static void SplitExpatName(const PRUnichar *aExpatName, nsIAtom **aPrefix,
                             nsIAtom **aTagName, PRInt32 *aNameSpaceID);

  // Get a permission-manager setting for the given principal and type.
  // If the pref doesn't exist or if it isn't ALLOW_ACTION, false is
  // returned, otherwise true is returned. Always returns true for the
  // system principal, and false for a null principal.
  static bool IsSitePermAllow(nsIPrincipal* aPrincipal, const char* aType);

  // Get a permission-manager setting for the given principal and type.
  // If the pref doesn't exist or if it isn't DENY_ACTION, false is
  // returned, otherwise true is returned. Always returns false for the
  // system principal, and true for a null principal.
  static bool IsSitePermDeny(nsIPrincipal* aPrincipal, const char* aType);

  // Returns true if aDoc1 and aDoc2 have equal NodePrincipal()s.
  static bool HaveEqualPrincipals(nsIDocument* aDoc1, nsIDocument* aDoc2);

  static nsILineBreaker* LineBreaker()
  {
    return sLineBreaker;
  }

  static nsIWordBreaker* WordBreaker()
  {
    return sWordBreaker;
  }

  /**
   * Regster aObserver as a shutdown observer. A strong reference is held
   * to aObserver until UnregisterShutdownObserver is called.
   */
  static void RegisterShutdownObserver(nsIObserver* aObserver);
  static void UnregisterShutdownObserver(nsIObserver* aObserver);

  /**
   * @return true if aContent has an attribute aName in namespace aNameSpaceID,
   * and the attribute value is non-empty.
   */
  static bool HasNonEmptyAttr(const nsIContent* aContent, PRInt32 aNameSpaceID,
                                nsIAtom* aName);

  /**
   * Method that gets the primary presContext for the node.
   * 
   * @param aContent The content node.
   * @return the presContext, or nullptr if the content is not in a document
   *         (if GetCurrentDoc returns nullptr)
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
   * @return true if the load can proceed, or false if it is blocked.
   *         Note that aImageBlockingStatus, if set will always be an ACCEPT
   *         status if true is returned and always be a REJECT_* status if
   *         false is returned.
   */
  static bool CanLoadImage(nsIURI* aURI,
                             nsISupports* aContext,
                             nsIDocument* aLoadingDocument,
                             nsIPrincipal* aLoadingPrincipal,
                             PRInt16* aImageBlockingStatus = nullptr);
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
  static bool IsImageInCache(nsIURI* aURI);

  /**
   * Method to get an imgIContainer from an image loading content
   *
   * @param aContent The image loading content.  Must not be null.
   * @param aRequest The image request [out]
   * @return the imgIContainer corresponding to the first frame of the image
   */
  static already_AddRefed<imgIContainer> GetImageFromContent(nsIImageLoadingContent* aContent, imgIRequest **aRequest = nullptr);

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
  static bool ContentIsDraggable(nsIContent* aContent);

  /**
   * Method that decides whether a content node is a draggable image
   *
   * @param aContent The content node to test.
   * @return whether it's a draggable image
   */
  static bool IsDraggableImage(nsIContent* aContent);

  /**
   * Method that decides whether a content node is a draggable link
   *
   * @param aContent The content node to test.
   * @return whether it's a draggable link
   */
  static bool IsDraggableLink(const nsIContent* aContent);

  /**
   * Convenience method to create a new nodeinfo that differs only by name
   * from aNodeInfo.
   */
  static nsresult NameChanged(nsINodeInfo *aNodeInfo, nsIAtom *aName,
                              nsINodeInfo** aResult)
  {
    nsNodeInfoManager *niMgr = aNodeInfo->NodeInfoManager();

    *aResult = niMgr->GetNodeInfo(aName, aNodeInfo->GetPrefixAtom(),
                                  aNodeInfo->NamespaceID(),
                                  aNodeInfo->NodeType(),
                                  aNodeInfo->GetExtraName()).get();
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
   * aContent is a descendant of aNode, a return value of false from this
   * method means that it's an anonymous descendant from aNode's point of view.
   *
   * Both arguments to this method must be non-null.
   */
  static bool IsInSameAnonymousTree(const nsINode* aNode, const nsIContent* aContent);

  /**
   * Return the nsIXPConnect service.
   */
  static nsIXPConnect *XPConnect()
  {
    return sXPConnect;
  }

  /**
   * Report a localized error message to the error console.
   *   @param aErrorFlags See nsIScriptError.
   *   @param aCategory Name of module reporting error.
   *   @param aDocument Reference to the document which triggered the message.
   *   @param aFile Properties file containing localized message.
   *   @param aMessageName Name of localized message.
   *   @param [aParams=nullptr] (Optional) Parameters to be substituted into
              localized message.
   *   @param [aParamsLength=0] (Optional) Length of aParams.
   *   @param [aURI=nullptr] (Optional) URI of resource containing error.
   *   @param [aSourceLine=EmptyString()] (Optional) The text of the line that
              contains the error (may be empty).
   *   @param [aLineNumber=0] (Optional) Line number within resource
              containing error.
   *   @param [aColumnNumber=0] (Optional) Column number within resource
              containing error.
              If aURI is null, then aDocument->GetDocumentURI() is used.
   */
  enum PropertiesFile {
    eCSS_PROPERTIES,
    eXBL_PROPERTIES,
    eXUL_PROPERTIES,
    eLAYOUT_PROPERTIES,
    eFORMS_PROPERTIES,
    ePRINTING_PROPERTIES,
    eDOM_PROPERTIES,
    eHTMLPARSER_PROPERTIES,
    eSVG_PROPERTIES,
    eBRAND_PROPERTIES,
    eCOMMON_DIALOG_PROPERTIES,
    PropertiesFile_COUNT
  };
  static nsresult ReportToConsole(PRUint32 aErrorFlags,
                                  const char *aCategory,
                                  nsIDocument* aDocument,
                                  PropertiesFile aFile,
                                  const char *aMessageName,
                                  const PRUnichar **aParams = nullptr,
                                  PRUint32 aParamsLength = 0,
                                  nsIURI* aURI = nullptr,
                                  const nsAFlatString& aSourceLine
                                    = EmptyString(),
                                  PRUint32 aLineNumber = 0,
                                  PRUint32 aColumnNumber = 0);

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
private:
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const PRUnichar** aParams,
                                        PRUint32 aParamsLength,
                                        nsXPIDLString& aResult);
  
public:
  template<PRUint32 N>
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const PRUnichar* (&aParams)[N],
                                        nsXPIDLString& aResult)
  {
    return FormatLocalizedString(aFile, aKey, aParams, N, aResult);
  }

  /**
   * Returns true if aDocument is a chrome document
   */
  static bool IsChromeDoc(nsIDocument *aDocument);

  /**
   * Returns true if aDocument is in a docshell whose parent is the same type
   */
  static bool IsChildOfSameType(nsIDocument* aDoc);

  /**
   * Get the script file name to use when compiling the script
   * referenced by aURI. In cases where there's no need for any extra
   * security wrapper automation the script file name that's returned
   * will be the spec in aURI, else it will be the spec in aDocument's
   * URI followed by aURI's spec, separated by " -> ". Returns true
   * if the script file name was modified, false if it's aURI's
   * spec.
   */
  static bool GetWrapperSafeScriptFilename(nsIDocument *aDocument,
                                             nsIURI *aURI,
                                             nsACString& aScriptURI);


  /**
   * Returns true if aDocument belongs to a chrome docshell for
   * display purposes.  Returns false for null documents or documents
   * which do not belong to a docshell.
   */
  static bool IsInChromeDocshell(nsIDocument *aDocument);

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
  static bool HasMutationListeners(nsINode* aNode,
                                     PRUint32 aType,
                                     nsINode* aTargetForSubtreeModified);

  /**
   * Quick helper to determine whether there are any mutation listeners
   * of a given type that apply to any content in this document. It is valid
   * to pass null for aDocument here, in which case this function always
   * returns true.
   *
   * @param aDocument The document to search for listeners
   * @param aType     The type of listener (NS_EVENT_BITS_MUTATION_*)
   *
   * @return true if there are mutation listeners of the specified type
   */
  static bool HasMutationListeners(nsIDocument* aDocument,
                                     PRUint32 aType);
  /**
   * Synchronously fire DOMNodeRemoved on aChild. Only fires the event if
   * there really are listeners by checking using the HasMutationListeners
   * function above. The function makes sure to hold the relevant objects alive
   * for the duration of the event firing. However there are no guarantees
   * that any of the objects are alive by the time the function returns.
   * If you depend on that you need to hold references yourself.
   *
   * @param aChild    The node to fire DOMNodeRemoved at.
   * @param aParent   The parent of aChild.
   * @param aOwnerDoc The ownerDocument of aChild.
   */
  static void MaybeFireNodeRemoved(nsINode* aChild, nsINode* aParent,
                                   nsIDocument* aOwnerDoc);

  /**
   * This method creates and dispatches a trusted event.
   * Works only with events which can be created by calling
   * nsIDOMDocument::CreateEvent() with parameter "Events".
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
                                       bool aCanBubble,
                                       bool aCancelable,
                                       bool *aDefaultAction = nullptr);
                                       
  /**
   * This method creates and dispatches a untrusted event.
   * Works only with events which can be created by calling
   * nsIDOMDocument::CreateEvent() with parameter "Events".
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       nsIDOMEventTarget.
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see nsIDOMEventTarget::DispatchEvent.
   */
  static nsresult DispatchUntrustedEvent(nsIDocument* aDoc,
                                         nsISupports* aTarget,
                                         const nsAString& aEventName,
                                         bool aCanBubble,
                                         bool aCancelable,
                                         bool *aDefaultAction = nullptr);

  /**
   * This method creates and dispatches a trusted event to the chrome
   * event handler.
   * Works only with events which can be created by calling
   * nsIDOMDocument::CreateEvent() with parameter "Events".
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
                                      bool aCanBubble,
                                      bool aCancelable,
                                      bool *aDefaultAction = nullptr);

  /**
   * Determines if an event attribute name (such as onclick) is valid for
   * a given element type. Types are from the EventNameType enumeration
   * defined above.
   *
   * @param aName the event name to look up
   * @param aType the type of content
   */
  static bool IsEventAttributeName(nsIAtom* aName, PRInt32 aType);

  /**
   * Return the event id for the event with the given name. The name is the
   * event name with the 'on' prefix. Returns NS_USER_DEFINED_EVENT if the
   * event doesn't match a known event name.
   *
   * @param aName the event name to look up
   */
  static PRUint32 GetEventId(nsIAtom* aName);

  /**
   * Return the category for the event with the given name. The name is the
   * event name *without* the 'on' prefix. Returns NS_EVENT if the event
   * is not known to be in any particular category.
   *
   * @param aName the event name to look up
   */
  static PRUint32 GetEventCategory(const nsAString& aName);

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
   * was created, aCreated is set to true.
   *
   * @param aNode The node for which to get the eventlistener manager.
   * @param aCreateIfNotFound If false, returns a listener manager only if
   *                          one already exists.
   */
  static nsEventListenerManager* GetListenerManager(nsINode* aNode,
                                                    bool aCreateIfNotFound);

  static void UnmarkGrayJSListenersInCCGenerationDocuments(PRUint32 aGeneration);

  /**
   * Remove the eventlistener manager for aNode.
   *
   * @param aNode The node for which to remove the eventlistener manager.
   */
  static void RemoveListenerManager(nsINode *aNode);

  static bool IsInitialized()
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
  static bool IsValidNodeName(nsIAtom *aLocalName, nsIAtom *aPrefix,
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
   * @param aReturn the resulting fragment
   * @param aPreventScriptExecution whether to mark scripts as already started
   */
  static nsresult CreateContextualFragment(nsINode* aContextNode,
                                           const nsAString& aFragment,
                                           bool aPreventScriptExecution,
                                           nsIDOMDocumentFragment** aReturn);

  /**
   * Invoke the fragment parsing algorithm (innerHTML) using the HTML parser.
   *
   * @param aSourceBuffer the string being set as innerHTML
   * @param aTargetNode the target container
   * @param aContextLocalName local name of context node
   * @param aContextNamespace namespace of context node
   * @param aQuirks true to make <table> not close <p>
   * @param aPreventScriptExecution true to prevent scripts from executing;
   *        don't set to false when parsing into a target node that has been
   *        bound to tree.
   * @return NS_ERROR_DOM_INVALID_STATE_ERR if a re-entrant attempt to parse
   *         fragments is made, NS_ERROR_OUT_OF_MEMORY if aSourceBuffer is too
   *         long and NS_OK otherwise.
   */
  static nsresult ParseFragmentHTML(const nsAString& aSourceBuffer,
                                    nsIContent* aTargetNode,
                                    nsIAtom* aContextLocalName,
                                    PRInt32 aContextNamespace,
                                    bool aQuirks,
                                    bool aPreventScriptExecution);

  /**
   * Invoke the fragment parsing algorithm (innerHTML) using the XML parser.
   *
   * @param aSourceBuffer the string being set as innerHTML
   * @param aTargetNode the target container
   * @param aTagStack the namespace mapping context
   * @param aPreventExecution whether to mark scripts as already started
   * @param aReturn the result fragment
   * @return NS_ERROR_DOM_INVALID_STATE_ERR if a re-entrant attempt to parse
   *         fragments is made, a return code from the XML parser.
   */
  static nsresult ParseFragmentXML(const nsAString& aSourceBuffer,
                                   nsIDocument* aDocument,
                                   nsTArray<nsString>& aTagStack,
                                   bool aPreventScriptExecution,
                                   nsIDOMDocumentFragment** aReturn);

  /**
   * Parse a string into a document using the HTML parser.
   * Script elements are marked unexecutable.
   *
   * @param aSourceBuffer the string to parse as an HTML document
   * @param aTargetDocument the document object to parse into. Must not have
   *                        child nodes.
   * @param aScriptingEnabledForNoscriptParsing whether <noscript> is parsed
   *                                            as if scripting was enabled
   * @return NS_ERROR_DOM_INVALID_STATE_ERR if a re-entrant attempt to parse
   *         fragments is made, NS_ERROR_OUT_OF_MEMORY if aSourceBuffer is too
   *         long and NS_OK otherwise.
   */
  static nsresult ParseDocumentHTML(const nsAString& aSourceBuffer,
                                    nsIDocument* aTargetDocument,
                                    bool aScriptingEnabledForNoscriptParsing);

  /**
   * Converts HTML source to plain text by parsing the source and using the
   * plain text serializer on the resulting tree.
   *
   * @param aSourceBuffer the string to parse as an HTML document
   * @param aResultBuffer the string where the plain text result appears;
   *                      may be the same string as aSourceBuffer
   * @param aFlags Flags from nsIDocumentEncoder.
   * @param aWrapCol Number of columns after which to line wrap; 0 for no
   *                 auto-wrapping
   * @return NS_ERROR_DOM_INVALID_STATE_ERR if a re-entrant attempt to parse
   *         fragments is made, NS_ERROR_OUT_OF_MEMORY if aSourceBuffer is too
   *         long and NS_OK otherwise.
   */
  static nsresult ConvertToPlainText(const nsAString& aSourceBuffer,
                                     nsAString& aResultBuffer,
                                     PRUint32 aFlags,
                                     PRUint32 aWrapCol);

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
   * @param aFlavor Select the kind of document to create.
   * @param aResult [out] The document that was created.
   */
  static nsresult CreateDocument(const nsAString& aNamespaceURI, 
                                 const nsAString& aQualifiedName, 
                                 nsIDOMDocumentType* aDoctype,
                                 nsIURI* aDocumentURI,
                                 nsIURI* aBaseURI,
                                 nsIPrincipal* aPrincipal,
                                 nsIScriptGlobalObject* aScriptObject,
                                 DocumentFlavor aFlavor,
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
                                     bool aTryReuse);

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
  static void GetNodeTextContent(nsINode* aNode, bool aDeep,
                                 nsAString& aResult)
  {
    aResult.Truncate();
    AppendNodeTextContent(aNode, aDeep, aResult);
  }

  /**
   * Same as GetNodeTextContents but appends the result rather than sets it.
   */
  static void AppendNodeTextContent(nsINode* aNode, bool aDeep,
                                    nsAString& aResult);

  /**
   * Utility method that checks if a given node has any non-empty
   * children.
   * NOTE! This method does not descend recursivly into elements.
   * Though it would be easy to make it so if needed
   */
  static bool HasNonEmptyTextContent(nsINode* aNode);

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
  static bool AreJSObjectsHeld(void* aScriptObjectHolder); 

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
      aCache->SetPreservingWrapper(true);
#ifdef DEBUG
      // Make sure the cycle collector will be able to traverse to the wrapper.
      CheckCCWrapperTraversal(aScriptObjectHolder, aCache);
#endif
    }
  }
  static void ReleaseWrapper(nsISupports* aScriptObjectHolder,
                             nsWrapperCache* aCache);
  static void TraceWrapper(nsWrapperCache* aCache, TraceCallback aCallback,
                           void *aClosure);

  /*
   * Notify when the first XUL menu is opened and when the all XUL menus are
   * closed. At opening, aInstalling should be TRUE, otherwise, it should be
   * FALSE.
   */
  static void NotifyInstalledMenuKeyboardListener(bool aInstalling);

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
                                          bool aAllowData,
                                          PRUint32 aContentPolicyType,
                                          nsISupports* aContext,
                                          const nsACString& aMimeGuess = EmptyCString(),
                                          nsISupports* aExtra = nullptr);

  /**
   * Returns true if aPrincipal is the system principal.
   */
  static bool IsSystemPrincipal(nsIPrincipal* aPrincipal);

  /**
   * *aResourcePrincipal is a principal describing who may access the contents
   * of a resource. The resource can only be consumed by a principal that
   * subsumes *aResourcePrincipal. MAKE SURE THAT NOTHING EVER ACTS WITH THE
   * AUTHORITY OF *aResourcePrincipal.
   * It may be null to indicate that the resource has no data from any origin
   * in it yet and anything may access the resource.
   * Additional data is being mixed into the resource from aExtraPrincipal
   * (which may be null; if null, no data is being mixed in and this function
   * will do nothing). Update *aResourcePrincipal to reflect the new data.
   * If *aResourcePrincipal subsumes aExtraPrincipal, nothing needs to change,
   * otherwise *aResourcePrincipal is replaced with the system principal.
   * Returns true if *aResourcePrincipal changed.
   */
  static bool CombineResourcePrincipals(nsCOMPtr<nsIPrincipal>* aResourcePrincipal,
                                        nsIPrincipal* aExtraPrincipal);

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
   * @param aIsTrusted If false, JS Context will be pushed to stack
   *                   when the link is triggered.
   */
  static void TriggerLink(nsIContent *aContent, nsPresContext *aPresContext,
                          nsIURI *aLinkURI, const nsString& aTargetSpec,
                          bool aClick, bool aIsUserTriggered,
                          bool aIsTrusted);

  /**
   * Get the link location.
   */
  static void GetLinkLocation(mozilla::dom::Element* aElement,
                              nsString& aLocationString);

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
  static bool DOMEventToNativeKeyEvent(nsIDOMKeyEvent* aKeyEvent,
                                         nsNativeKeyEvent* aNativeEvent,
                                         bool aGetCharCode);

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

  /*
   * Return true if the target of a drop event is a content document that is
   * an ancestor of the document for the source of the drag.
   */
  static bool CheckForSubFrameDrop(nsIDragSession* aDragSession,
                                   nsDragEvent* aDropEvent);

  /**
   * Return true if aURI is a local file URI (i.e. file://).
   */
  static bool URIIsLocalFile(nsIURI *aURI);

  /**
   * Given a URI, return set beforeHash to the part before the '#', and
   * afterHash to the remainder of the URI, including the '#'.
   */
  static nsresult SplitURIAtHash(nsIURI *aURI,
                                 nsACString &aBeforeHash,
                                 nsACString &aAfterHash);

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
  static bool OfflineAppAllowed(nsIURI *aURI);

  /**
   * Check whether an application should be allowed to use offline APIs.
   */
  static bool OfflineAppAllowed(nsIPrincipal *aPrincipal);

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
   * @return false on out of memory, true otherwise.
   */
  static bool AddScriptRunner(nsIRunnable* aRunnable);

  /**
   * Returns true if it's safe to execute content script and false otherwise.
   *
   * The only known case where this lies is mutation events. They run, and can
   * run anything else, when this function returns false, but this is ok.
   */
  static bool IsSafeToRunScript() {
    return sScriptBlockerCount == 0;
  }

  /**
   * Retrieve information about the viewport as a data structure.
   * This will return information in the viewport META data section
   * of the document. This can be used in lieu of ProcessViewportInfo(),
   * which places the viewport information in the document header instead
   * of returning it directly.
   *
   * NOTE: If the site is optimized for mobile (via the doctype), this
   * will return viewport information that specifies default information.
   */
  static ViewportInfo GetViewportInfo(nsIDocument* aDocument);

  // Call EnterMicroTask when you're entering JS execution.
  // Usually the best way to do this is to use nsAutoMicroTask.
  static void EnterMicroTask() { ++sMicroTaskLevel; }
  static void LeaveMicroTask();

  static bool IsInMicroTask() { return sMicroTaskLevel != 0; }
  static PRUint32 MicroTaskLevel() { return sMicroTaskLevel; }
  static void SetMicroTaskLevel(PRUint32 aLevel) { sMicroTaskLevel = aLevel; }

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
  static JSContext *GetSafeJSContext();

  /**
   * Case insensitive comparison between two strings. However it only ignores
   * case for ASCII characters a-z.
   */
  static bool EqualsIgnoreASCIICase(const nsAString& aStr1,
                                    const nsAString& aStr2);

  /**
   * Case insensitive comparison between a string and an ASCII literal.
   * This must ONLY be applied to an actual literal string. Do not attempt
   * to use it with a regular char* pointer, or with a char array variable.
   * The template trick to acquire the array length at compile time without
   * using a macro is due to Corey Kosak, which much thanks.
   */
  static bool EqualsLiteralIgnoreASCIICase(const nsAString& aStr1,
                                           const char* aStr2,
                                           const PRUint32 len);
#ifdef NS_DISABLE_LITERAL_TEMPLATE
  static inline bool
  EqualsLiteralIgnoreASCIICase(const nsAString& aStr1,
                               const char* aStr2)
  {
    PRUint32 len = strlen(aStr2);
    return EqualsLiteralIgnoreASCIICase(aStr1, aStr2, len);
  }
#else
  template<int N>
  static inline bool
  EqualsLiteralIgnoreASCIICase(const nsAString& aStr1,
                               const char (&aStr2)[N])
  {
    return EqualsLiteralIgnoreASCIICase(aStr1, aStr2, N-1);
  }
  template<int N>
  static inline bool
  EqualsLiteralIgnoreASCIICase(const nsAString& aStr1,
                               char (&aStr2)[N])
  {
    const char* s = aStr2;
    return EqualsLiteralIgnoreASCIICase(aStr1, s, N-1);
  }
#endif

  /**
   * Convert ASCII A-Z to a-z.
   * @return NS_OK on success, or NS_ERROR_OUT_OF_MEMORY if making the string
   * writable needs to allocate memory and that allocation fails.
   */
  static nsresult ASCIIToLower(nsAString& aStr);
  static nsresult ASCIIToLower(const nsAString& aSource, nsAString& aDest);

  /**
   * Convert ASCII a-z to A-Z.
   * @return NS_OK on success, or NS_ERROR_OUT_OF_MEMORY if making the string
   * writable needs to allocate memory and that allocation fails.
   */
  static nsresult ASCIIToUpper(nsAString& aStr);
  static nsresult ASCIIToUpper(const nsAString& aSource, nsAString& aDest);

  /**
   * Return whether aStr contains an ASCII uppercase character.
   */
  static bool StringContainsASCIIUpper(const nsAString& aStr);

  // Returns NS_OK for same origin, error (NS_ERROR_DOM_BAD_URI) if not.
  static nsresult CheckSameOrigin(nsIChannel *aOldChannel, nsIChannel *aNewChannel);
  static nsIInterfaceRequestor* GetSameOriginChecker();

  static nsIThreadJSContextStack* ThreadJSContextStack()
  {
    return sThreadJSContextStack;
  }

  // Trace the safe JS context of the ThreadJSContextStack.
  static void TraceSafeJSContext(JSTracer* aTrc);


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
                                     bool aTrusted,
                                     nsIDOMEvent* aSourceEvent = nullptr,
                                     nsIPresShell* aShell = nullptr,
                                     bool aCtrl = false,
                                     bool aAlt = false,
                                     bool aShift = false,
                                     bool aMeta = false);

  /**
   * Gets the nsIDocument given the script context. Will return nullptr on failure.
   *
   * @param aScriptContext the script context to get the document for; can be null
   *
   * @return the document associated with the script context
   */
  static already_AddRefed<nsIDocument>
  GetDocumentFromScriptContext(nsIScriptContext *aScriptContext);

  static bool CheckMayLoad(nsIPrincipal* aPrincipal, nsIChannel* aChannel);

  /**
   * The method checks whether the caller can access native anonymous content.
   * If there is no JS in the stack or privileged JS is running, this
   * method returns true, otherwise false.
   */
  static bool CanAccessNativeAnon();

  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, const nsIID* aIID, jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nullptr,
                             bool aAllowWrapping = false)
  {
    return WrapNative(cx, scope, native, nullptr, aIID, vp, aHolder,
                      aAllowWrapping);
  }

  // Same as the WrapNative above, but use this one if aIID is nsISupports' IID.
  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nullptr,
                             bool aAllowWrapping = false)
  {
    return WrapNative(cx, scope, native, nullptr, nullptr, vp, aHolder,
                      aAllowWrapping);
  }
  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, nsWrapperCache *cache,
                             jsval *vp,
                             // If non-null aHolder will keep the jsval alive
                             // while there's a ref to it
                             nsIXPConnectJSObjectHolder** aHolder = nullptr,
                             bool aAllowWrapping = false)
  {
    return WrapNative(cx, scope, native, cache, nullptr, vp, aHolder,
                      aAllowWrapping);
  }

  /**
   * Creates an arraybuffer from a binary string.
   */
  static nsresult CreateArrayBuffer(JSContext *aCx, const nsACString& aData,
                                    JSObject** aResult);

  static void StripNullChars(const nsAString& aInStr, nsAString& aOutStr);

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

  static bool IsHandlingKeyBoardEvent()
  {
    return sIsHandlingKeyBoardEvent;
  }

  static void SetIsHandlingKeyBoardEvent(bool aHandling)
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
   * Returns the widget for this document if there is one. Looks at all ancestor
   * documents to try to find a widget, so for example this can still find a
   * widget for documents in display:none frames that have no presentation.
   */
  static nsIWidget *WidgetForDocument(nsIDocument *aDoc);

  /**
   * Returns a layer manager to use for the given document. Basically we
   * look up the document hierarchy for the first document which has
   * a presentation with an associated widget, and use that widget's
   * layer manager.
   *
   * @param aDoc the document for which to return a layer manager.
   * @param aAllowRetaining an outparam that states whether the returned
   * layer manager should be used for retained layers
   */
  static already_AddRefed<mozilla::layers::LayerManager>
  LayerManagerForDocument(nsIDocument *aDoc, bool *aAllowRetaining = nullptr);

  /**
   * Returns a layer manager to use for the given document. Basically we
   * look up the document hierarchy for the first document which has
   * a presentation with an associated widget, and use that widget's
   * layer manager. In addition to the normal layer manager lookup this will
   * specifically request a persistent layer manager. This means that the layer
   * manager is expected to remain the layer manager for the document in the
   * forseeable future. This function should be used carefully as it may change
   * the document's layer manager.
   *
   * @param aDoc the document for which to return a layer manager.
   * @param aAllowRetaining an outparam that states whether the returned
   * layer manager should be used for retained layers
   */
  static already_AddRefed<mozilla::layers::LayerManager>
  PersistentLayerManagerForDocument(nsIDocument *aDoc, bool *aAllowRetaining = nullptr);

  /**
   * Determine whether a content node is focused or not,
   *
   * @param aContent the content node to check
   * @return true if the content node is focused, false otherwise.
   */
  static bool IsFocusedContent(const nsIContent *aContent);

  /**
   * Returns true if the DOM full-screen API is enabled.
   */
  static bool IsFullScreenApiEnabled();

  /**
   * Returns true if requests for full-screen are allowed in the current
   * context. Requests are only allowed if the user initiated them (like with
   * a mouse-click or key press), unless this check has been disabled by
   * setting the pref "full-screen-api.allow-trusted-requests-only" to false.
   */
  static bool IsRequestFullScreenAllowed();

  /**
   * Returns true if the idle observers API is enabled.
   */
  static bool IsIdleObserverAPIEnabled() { return sIsIdleObserverAPIEnabled; }
  
  /**
   * Returns true if the doc tree branch which contains aDoc contains any
   * plugins which we don't control event dispatch for, i.e. do any plugins
   * in the same tab as this document receive key events outside of our
   * control? This always returns false on MacOSX.
   */
  static bool HasPluginWithUncontrolledEventDispatch(nsIDocument* aDoc);

  /**
   * Returns true if the content is in a document and contains a plugin
   * which we don't control event dispatch for, i.e. do any plugins in this
   * doc tree receive key events outside of our control? This always returns
   * false on MacOSX.
   */
  static bool HasPluginWithUncontrolledEventDispatch(nsIContent* aContent);

  /**
   * Returns the root document in a document hierarchy. Normally this will
   * be the chrome document.
   */
  static nsIDocument* GetRootDocument(nsIDocument* aDoc);

  /**
   * Returns the time limit on handling user input before
   * nsEventStateManager::IsHandlingUserInput() stops returning true.
   * This enables us to detect long running user-generated event handlers.
   */
  static TimeDuration HandlingUserInputTimeout();

  static void GetShiftText(nsAString& text);
  static void GetControlText(nsAString& text);
  static void GetMetaText(nsAString& text);
  static void GetOSText(nsAString& text);
  static void GetAltText(nsAString& text);
  static void GetModifierSeparatorText(nsAString& text);

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

  /**
   * Returns true if content with the given principal is allowed to use XUL
   * and XBL and false otherwise.
   */
  static bool AllowXULXBLForPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Perform cleanup that's appropriate for XPCOM shutdown.
   */
  static void XPCOMShutdown();

  enum ContentViewerType
  {
      TYPE_UNSUPPORTED,
      TYPE_CONTENT,
      TYPE_PLUGIN,
      TYPE_UNKNOWN
  };

  static already_AddRefed<nsIDocumentLoaderFactory>
  FindInternalContentViewer(const char* aType,
                            ContentViewerType* aLoaderType = nullptr);

  /**
   * This helper method returns true if the aPattern pattern matches aValue.
   * aPattern should not contain leading and trailing slashes (/).
   * The pattern has to match the entire value not just a subset.
   * aDocument must be a valid pointer (not null).
   *
   * This is following the HTML5 specification:
   * http://dev.w3.org/html5/spec/forms.html#attr-input-pattern
   *
   * WARNING: This method mutates aPattern and aValue!
   *
   * @param aValue    the string to check.
   * @param aPattern  the string defining the pattern.
   * @param aDocument the owner document of the element.
   * @result          whether the given string is matches the pattern.
   */
  static bool IsPatternMatching(nsAString& aValue, nsAString& aPattern,
                                  nsIDocument* aDocument);

  /**
   * Calling this adds support for
   * ontouch* event handler DOM attributes.
   */
  static void InitializeTouchEventTable();

  /**
   * Test whether the given URI always inherits a security context
   * from the document it comes from.
   */
  static nsresult URIInheritsSecurityContext(nsIURI *aURI, bool *aResult);

  /**
   * Set the given principal as the owner of the given channel, if
   * needed.  aURI must be the URI of aChannel.  aPrincipal may be
   * null.  If aSetUpForAboutBlank is true, then about:blank will get
   * the principal set up on it.
   *
   * The return value is whether the principal was set up as the owner
   * of the channel.
   */
  static bool SetUpChannelOwner(nsIPrincipal* aLoadingPrincipal,
                                nsIChannel* aChannel,
                                nsIURI* aURI,
                                bool aSetUpForAboutBlank);

  static nsresult Btoa(const nsAString& aBinaryData,
                       nsAString& aAsciiBase64String);

  static nsresult Atob(const nsAString& aAsciiString,
                       nsAString& aBinaryData);

  /** If aJSArray is a Javascript array, this method iterates over its
   *  elements and appends values to aRetVal as nsIAtoms.
   *  @throw NS_ERROR_ILLEGAL_VALUE if aJSArray isn't a JS array.
   */ 
  static nsresult JSArrayToAtomArray(JSContext* aCx, const JS::Value& aJSArray,
                                     nsCOMArray<nsIAtom>& aRetVal);

  /**
   * Returns whether the input element passed in parameter has the autocomplete
   * functionnality enabled. It is taking into account the form owner.
   * NOTE: the caller has to make sure autocomplete makes sense for the
   * element's type.
   *
   * @param aInput the input element to check. NOTE: aInput can't be null.
   * @return whether the input element has autocomplete enabled.
   */
  static bool IsAutocompleteEnabled(nsIDOMHTMLInputElement* aInput);

  /**
   * If the URI is chrome, return true unconditionarlly.
   *
   * Otherwise, get the contents of the given pref, and treat it as a
   * comma-separated list of URIs.  Return true if the given URI's prepath is
   * in the list, and false otherwise.
   *
   * Comparisons are case-insensitive, and whitespace between elements of the
   * comma-separated list is ignored.
   */
  static bool URIIsChromeOrInPref(nsIURI *aURI, const char *aPref);

  /**
   * This will parse aSource, to extract the value of the pseudo attribute
   * with the name specified in aName. See
   * http://www.w3.org/TR/xml-stylesheet/#NT-StyleSheetPI for the specification
   * which is used to parse aSource.
   *
   * @param aSource the string to parse
   * @param aName the name of the attribute to get the value for
   * @param aValue [out] the value for the attribute with name specified in
   *                     aAttribute. Empty if the attribute isn't present.
   * @return true     if the attribute exists and was successfully parsed.
   *         false if the attribute doesn't exist, or has a malformed
   *                  value, such as an unknown or unterminated entity.
   */
  static bool GetPseudoAttributeValue(const nsString& aSource, nsIAtom *aName,
                                      nsAString& aValue);

  /**
   * Returns true if the language name is a version of JavaScript and
   * false otherwise
   */
  static bool IsJavaScriptLanguage(const nsString& aName, PRUint32 *aVerFlags);

  /**
   * Returns the JSVersion for a string of the form '1.n', n = 0, ..., 8, and
   * JSVERSION_UNKNOWN for other strings.
   */
  static JSVersion ParseJavascriptVersion(const nsAString& aVersionStr);

  static bool IsJavascriptMIMEType(const nsAString& aMIMEType)
  {
    // Table ordered from most to least likely JS MIME types.
    static const char* jsTypes[] = {
      "text/javascript",
      "text/ecmascript",
      "application/javascript",
      "application/ecmascript",
      "application/x-javascript",
      "application/x-ecmascript",
      "text/javascript1.0",
      "text/javascript1.1",
      "text/javascript1.2",
      "text/javascript1.3",
      "text/javascript1.4",
      "text/javascript1.5",
      "text/jscript",
      "text/livescript",
      "text/x-ecmascript",
      "text/x-javascript",
      nullptr
    };

    for (PRUint32 i = 0; jsTypes[i]; ++i) {
      if (aMIMEType.LowerCaseEqualsASCII(jsTypes[i])) {
        return true;
      }
    }

    return false;
  }

  static void SplitMimeType(const nsAString& aValue, nsString& aType,
                            nsString& aParams);

  /**
   * Function checks if the user is idle.
   * 
   * @param aRequestedIdleTimeInMS    The idle observer's requested idle time.
   * @param aUserIsIdle               boolean indicating if the user 
   *                                  is currently idle or not.   *
   * @return NS_OK                    NS_OK returned if the requested idle service and 
   *                                  the current idle time were successfully obtained.
   *                                  NS_ERROR_FAILURE returned if the the requested
   *                                  idle service or the current idle were not obtained.
   */
  static nsresult IsUserIdle(PRUint32 aRequestedIdleTimeInMS, bool* aUserIsIdle);

  /** 
   * Takes a window and a string to check prefs against. Assumes that
   * the window is an app window, and that the pref is a comma
   * seperated list of app urls that have permission to use whatever
   * the preference refers to (for example, does the current window
   * have access to mozTelephony). Chrome is always given permissions
   * for the requested preference. Sets aAllowed based on preference.
   *
   * @param aWindow Current window asking for preference permission
   * @param aPrefURL Preference name
   * @param aAllowed [out] outparam on whether or not window is allowed
   *                       to access pref
   *
   * @return NS_OK on successful preference lookup, error code otherwise
   */
  static nsresult IsOnPrefWhitelist(nsPIDOMWindow* aWindow,
                                    const char* aPrefURL, bool *aAllowed);

  /**
   * Takes a selection, and a text control element (<input> or <textarea>), and
   * returns the offsets in the text content corresponding to the selection.
   * The selection's anchor and focus must both be in the root node passed or a
   * descendant.
   *
   * @param aSelection      Selection to check
   * @param aRoot           Root <input> or <textarea> element
   * @param aOutStartOffset Output start offset
   * @param aOutEndOffset   Output end offset
   */
  static void GetSelectionInTextControl(mozilla::Selection* aSelection,
                                        Element* aRoot,
                                        PRInt32& aOutStartOffset,
                                        PRInt32& aOutEndOffset);

  static nsIEditor* GetHTMLEditor(nsPresContext* aPresContext);

private:
  static bool InitializeEventTable();

  static nsresult EnsureStringBundle(PropertiesFile aFile);

  static bool CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                nsIPrincipal* aPrincipal);

  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, nsWrapperCache *cache,
                             const nsIID* aIID, jsval *vp,
                             nsIXPConnectJSObjectHolder** aHolder,
                             bool aAllowWrapping);
                            
  static nsresult DispatchEvent(nsIDocument* aDoc,
                                nsISupports* aTarget,
                                const nsAString& aEventName,
                                bool aCanBubble,
                                bool aCancelable,
                                bool aTrusted,
                                bool *aDefaultAction = nullptr);

  static void InitializeModifierStrings();

  static void DropFragmentParsers();

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

  static bool sImgLoaderInitialized;
  static void InitImgLoader();

  // The following two members are initialized lazily
  static imgILoader* sImgLoader;
  static imgICache* sImgCache;

  static nsIConsoleService* sConsoleService;

  static nsDataHashtable<nsISupportsHashKey, EventNameMapping>* sAtomEventTable;
  static nsDataHashtable<nsStringHashKey, EventNameMapping>* sStringEventTable;
  static nsCOMArray<nsIAtom>* sUserDefinedEvents;

  static nsIStringBundleService* sStringBundleService;
  static nsIStringBundle* sStringBundles[PropertiesFile_COUNT];

  static nsIContentPolicy* sContentPolicyService;
  static bool sTriedToGetContentPolicy;

  static nsILineBreaker* sLineBreaker;
  static nsIWordBreaker* sWordBreaker;

  static PRUint32 sJSGCThingRootCount;

#ifdef IBMBIDI
  static nsIBidiKeyboard* sBidiKeyboard;
#endif

  static bool sInitialized;
  static PRUint32 sScriptBlockerCount;
#ifdef DEBUG
  static PRUint32 sDOMNodeRemovedSuppressCount;
#endif
  static PRUint32 sMicroTaskLevel;
  // Not an nsCOMArray because removing elements from those is slower
  static nsTArray< nsCOMPtr<nsIRunnable> >* sBlockedScriptRunners;
  static PRUint32 sRunnersCountAtFirstBlocker;
  static PRUint32 sScriptBlockerCountWhereRunnersPrevented;

  static nsIInterfaceRequestor* sSameOriginChecker;

  static bool sIsHandlingKeyBoardEvent;
  static bool sAllowXULXBL_for_file;
  static bool sIsFullScreenApiEnabled;
  static bool sTrustedFullScreenOnly;
  static PRUint32 sHandlingInputTimeout;
  static bool sIsIdleObserverAPIEnabled;

  static nsHtml5StringParser* sHTMLFragmentParser;
  static nsIParser* sXMLFragmentParser;
  static nsIFragmentContentSink* sXMLFragmentSink;

  /**
   * True if there's a fragment parser activation on the stack.
   */
  static bool sFragmentParsingActive;

  static nsString* sShiftText;
  static nsString* sControlText;
  static nsString* sMetaText;
  static nsString* sOSText;
  static nsString* sAltText;
  static nsString* sModifierSeparator;
};

typedef nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace>
                                                    HTMLSplitOnSpacesTokenizer;

#define NS_HOLD_JS_OBJECTS(obj, clazz)                                         \
  nsContentUtils::HoldJSObjects(NS_CYCLE_COLLECTION_UPCAST(obj, clazz),        \
                                NS_CYCLE_COLLECTION_PARTICIPANT(clazz))

#define NS_DROP_JS_OBJECTS(obj, clazz)                                         \
  nsContentUtils::DropJSObjects(NS_CYCLE_COLLECTION_UPCAST(obj, clazz))


class NS_STACK_CLASS nsCxPusher
{
public:
  nsCxPusher();
  ~nsCxPusher(); // Calls Pop();

  // Returns false if something erroneous happened.
  bool Push(nsIDOMEventTarget *aCurrentTarget);
  // If nothing has been pushed to stack, this works like Push.
  // Otherwise if context will change, Pop and Push will be called.
  bool RePush(nsIDOMEventTarget *aCurrentTarget);
  // If a null JSContext is passed to Push(), that will cause no
  // push to happen and false to be returned.
  bool Push(JSContext *cx, bool aRequiresScriptContext = true);
  // Explicitly push a null JSContext on the the stack
  bool PushNull();

  // Pop() will be a no-op if Push() or PushNull() fail
  void Pop();

  nsIScriptContext* GetCurrentScriptContext() { return mScx; }
private:
  // Combined code for PushNull() and Push(JSContext*)
  bool DoPush(JSContext* cx);

  nsCOMPtr<nsIScriptContext> mScx;
  bool mScriptIsRunning;
  bool mPushedSomething;
#ifdef DEBUG
  JSContext* mPushedContext;
#endif
};

class NS_STACK_CLASS nsAutoScriptBlocker {
public:
  nsAutoScriptBlocker(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    nsContentUtils::AddScriptBlocker();
  }
  ~nsAutoScriptBlocker() {
    nsContentUtils::RemoveScriptBlocker();
  }
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class NS_STACK_CLASS nsAutoScriptBlockerSuppressNodeRemoved :
                          public nsAutoScriptBlocker {
public:
  nsAutoScriptBlockerSuppressNodeRemoved() {
#ifdef DEBUG
    ++nsContentUtils::sDOMNodeRemovedSuppressCount;
#endif
  }
  ~nsAutoScriptBlockerSuppressNodeRemoved() {
#ifdef DEBUG
    --nsContentUtils::sDOMNodeRemovedSuppressCount;
#endif
  }
};

class NS_STACK_CLASS nsAutoMicroTask
{
public:
  nsAutoMicroTask()
  {
    nsContentUtils::EnterMicroTask();
  }
  ~nsAutoMicroTask()
  {
    nsContentUtils::LeaveMicroTask();
  }
};

#define NS_INTERFACE_MAP_ENTRY_TEAROFF(_interface, _allocator)                \
  if (aIID.Equals(NS_GET_IID(_interface))) {                                  \
    foundInterface = static_cast<_interface *>(_allocator);                   \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nullptr;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

/*
 * In the following helper macros we exploit the fact that the result of a
 * series of additions will not be finite if any one of the operands in the
 * series is not finite.
 */
#define NS_ENSURE_FINITE(f, rv)                                               \
  if (!NS_finite(f)) {                                                        \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE2(f1, f2, rv)                                         \
  if (!NS_finite((f1)+(f2))) {                                                \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE3(f1, f2, f3, rv)                                     \
  if (!NS_finite((f1)+(f2)+(f3))) {                                           \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE4(f1, f2, f3, f4, rv)                                 \
  if (!NS_finite((f1)+(f2)+(f3)+(f4))) {                                      \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE5(f1, f2, f3, f4, f5, rv)                             \
  if (!NS_finite((f1)+(f2)+(f3)+(f4)+(f5))) {                                 \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE6(f1, f2, f3, f4, f5, f6, rv)                         \
  if (!NS_finite((f1)+(f2)+(f3)+(f4)+(f5)+(f6))) {                            \
    return (rv);                                                              \
  }

// Deletes a linked list iteratively to avoid blowing up the stack (bug 460444).
#define NS_CONTENT_DELETE_LIST_MEMBER(type_, ptr_, member_)                   \
  {                                                                           \
    type_ *cur = (ptr_)->member_;                                             \
    (ptr_)->member_ = nullptr;                                                 \
    while (cur) {                                                             \
      type_ *next = cur->member_;                                             \
      cur->member_ = nullptr;                                                  \
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
    return GetParameter(nullptr, aResult);
  }

private:
  NS_ConvertUTF16toUTF8 mString;
  nsIMIMEHeaderParam*   mService;
};

class nsDocElementCreatedNotificationRunner : public nsRunnable
{
public:
    nsDocElementCreatedNotificationRunner(nsIDocument* aDoc)
        : mDoc(aDoc)
    {
    }

    NS_IMETHOD Run()
    {
        nsContentSink::NotifyDocElementCreated(mDoc);
        return NS_OK;
    }

    nsCOMPtr<nsIDocument> mDoc;
};

#endif /* nsContentUtils_h___ */
