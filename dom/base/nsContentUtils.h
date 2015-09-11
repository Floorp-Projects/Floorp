/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content utilities. */

#ifndef nsContentUtils_h___
#define nsContentUtils_h___

#if defined(XP_WIN)
#include <float.h>
#endif

#if defined(SOLARIS)
#include <ieeefp.h>
#endif

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/RootingAPI.h"
#include "mozilla/EventForwards.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/TimeStamp.h"
#include "nsContentListDeclarations.h"
#include "nsMathUtils.h"
#include "nsTArrayForwardDeclare.h"
#include "Units.h"
#include "mozilla/dom/AutocompleteInfoBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "nsIContentPolicy.h"

#if defined(XP_WIN)
// Undefine LoadImage to prevent naming conflict with Windows.
#undef LoadImage
#endif

class imgICache;
class imgIContainer;
class imgINotificationObserver;
class imgIRequest;
class imgLoader;
class imgRequestProxy;
class nsAutoScriptBlockerSuppressNodeRemoved;
class nsHtml5StringParser;
class nsIChannel;
class nsIConsoleService;
class nsIContent;
class nsIContentPolicy;
class nsIContentSecurityPolicy;
class nsIDocShellTreeItem;
class nsIDocument;
class nsIDocumentLoaderFactory;
class nsIDOMDocument;
class nsIDOMDocumentFragment;
class nsIDOMEvent;
class nsIDOMHTMLInputElement;
class nsIDOMKeyEvent;
class nsIDOMNode;
class nsIDOMWindow;
class nsIDragSession;
class nsIEditor;
class nsIFragmentContentSink;
class nsIFrame;
class nsIImageLoadingContent;
class nsIInterfaceRequestor;
class nsIIOService;
class nsILineBreaker;
class nsILoadGroup;
class nsIMessageBroadcaster;
class nsNameSpaceManager;
class nsIObserver;
class nsIParser;
class nsIParserService;
class nsIPresShell;
class nsIPrincipal;
class nsIRequest;
class nsIRunnable;
class nsIScriptContext;
class nsIScriptSecurityManager;
class nsIStringBundle;
class nsIStringBundleService;
class nsISupportsArray;
class nsISupportsHashKey;
class nsIURI;
class nsIUUIDGenerator;
class nsIWidget;
class nsIWordBreaker;
class nsIXPConnect;
class nsNodeInfoManager;
class nsPIDOMWindow;
class nsPresContext;
class nsStringBuffer;
class nsStringHashKey;
class nsTextFragment;
class nsView;
class nsViewportInfo;
class nsWrapperCache;
class nsAttrValue;
class nsITransferable;
class nsPIWindowRoot;

struct JSPropertyDescriptor;
struct JSRuntime;
struct nsIntMargin;

template<class E> class nsCOMArray;
template<class K, class V> class nsDataHashtable;
template<class K, class V> class nsRefPtrHashtable;
template<class T> class nsReadingIterator;

namespace mozilla {
class ErrorResult;
class EventListenerManager;

namespace dom {
class DocumentFragment;
class Element;
class EventTarget;
class IPCDataTransfer;
class NodeInfo;
class nsIContentChild;
class nsIContentParent;
class Selection;
class TabParent;
} // namespace dom

namespace gfx {
class DataSourceSurface;
} // namespace gfx

namespace layers {
class LayerManager;
} // namespace layers

} // namespace mozilla

class nsIBidiKeyboard;

extern const char kLoadAsData[];

// Stolen from nsReadableUtils, but that's OK, since we can declare the same
// name multiple times.
const nsAFlatString& EmptyString();
const nsAFlatCString& EmptyCString();

enum EventNameType {
  EventNameType_None = 0x0000,
  EventNameType_HTML = 0x0001,
  EventNameType_XUL = 0x0002,
  EventNameType_SVGGraphic = 0x0004, // svg graphic elements
  EventNameType_SVGSVG = 0x0008, // the svg element
  EventNameType_SMIL = 0x0010, // smil elements
  EventNameType_HTMLBodyOrFramesetOnly = 0x0020,

  EventNameType_HTMLXUL = 0x0003,
  EventNameType_All = 0xFFFF
};

struct EventNameMapping
{
  // This holds pointers to nsGkAtoms members, and is therefore safe as a
  // non-owning reference.
  nsIAtom* MOZ_NON_OWNING_REF mAtom;
  int32_t  mType;
  mozilla::EventMessage mMessage;
  mozilla::EventClassID mEventClassID;
};

struct nsShortcutCandidate {
  nsShortcutCandidate(uint32_t aCharCode, bool aIgnoreShift) :
    mCharCode(aCharCode), mIgnoreShift(aIgnoreShift)
  {
  }
  uint32_t mCharCode;
  bool     mIgnoreShift;
};

typedef void (*CallOnRemoteChildFunction) (mozilla::dom::TabParent* aTabParent,
                                           void* aArg);

class nsContentUtils
{
  friend class nsAutoScriptBlockerSuppressNodeRemoved;
  typedef mozilla::dom::Element Element;
  typedef mozilla::TimeDuration TimeDuration;

public:
  static nsresult Init();

  static bool     IsCallerChrome();
  static bool     ThreadsafeIsCallerChrome();
  static bool     IsCallerContentXBL();

  static bool     IsImageSrcSetDisabled();

  static bool LookupBindingMember(JSContext* aCx, nsIContent *aContent,
                                  JS::Handle<jsid> aId,
                                  JS::MutableHandle<JSPropertyDescriptor> aDesc);

  // Check whether we should avoid leaking distinguishing information to JS/CSS.
  static bool ShouldResistFingerprinting(nsIDocShell* aDocShell);

  /**
   * Returns the parent node of aChild crossing document boundaries.
   * Uses the parent node in the composed document.
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
   * Similar to ContentIsDescendantOf, except will treat an HTMLTemplateElement
   * or ShadowRoot as an ancestor of things in the corresponding DocumentFragment.
   * See the concept of "host-including inclusive ancestor" in the DOM
   * specification.
   */
  static bool ContentIsHostIncludingDescendantOf(
    const nsINode* aPossibleDescendant, const nsINode* aPossibleAncestor);

  /**
   * Similar to ContentIsDescendantOf except it crosses document boundaries,
   * this function uses ancestor/descendant relations in the composed document
   * (see shadow DOM spec).
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
                                         int32_t aOffset,
                                         nsTArray<nsIContent*>* aAncestorNodes,
                                         nsTArray<int32_t>* aAncestorOffsets);

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
  static bool PositionIsBefore(nsINode* aNode1, nsINode* aNode2);

  /**
   *  Utility routine to compare two "points", where a point is a
   *  node/offset pair
   *  Returns -1 if point1 < point2, 1, if point1 > point2,
   *  0 if error or if point1 == point2.
   *  NOTE! If the two nodes aren't in the same connected subtree,
   *  the result is 1, and the optional aDisconnected parameter
   *  is set to true.
   */
  static int32_t ComparePoints(nsINode* aParent1, int32_t aOffset1,
                               nsINode* aParent2, int32_t aOffset2,
                               bool* aDisconnected = nullptr);
  static int32_t ComparePoints(nsIDOMNode* aParent1, int32_t aOffset1,
                               nsIDOMNode* aParent2, int32_t aOffset2,
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
  static uint16_t ReverseDocumentPosition(uint16_t aDocumentPosition);

  /**
   * Returns a subdocument for aDocument with a particular outer window ID.
   *
   * @param aDocument
   *        The document whose subdocuments will be searched.
   * @param aOuterWindowID
   *        The outer window ID for the subdocument to be found. This must
   *        be a value greater than 0.
   * @return nsIDocument*
   *        A pointer to the found nsIDocument. nullptr if the subdocument
   *        cannot be found, or if either aDocument or aOuterWindowId were
   *        invalid. If the outer window ID belongs to aDocument itself, this
   *        will return a pointer to aDocument.
   */
  static nsIDocument* GetSubdocumentWithOuterWindowId(nsIDocument *aDocument,
                                                      uint64_t aOuterWindowId);

  static uint32_t CopyNewlineNormalizedUnicodeTo(const nsAString& aSource,
                                                 uint32_t aSrcOffset,
                                                 char16_t* aDest,
                                                 uint32_t aLength,
                                                 bool& aLastCharCR);

  static uint32_t CopyNewlineNormalizedUnicodeTo(nsReadingIterator<char16_t>& aSrcStart, const nsReadingIterator<char16_t>& aSrcEnd, nsAString& aDest);

  static const nsDependentSubstring TrimCharsInSet(const char* aSet,
                                                   const nsAString& aValue);

  template<bool IsWhitespace(char16_t)>
  static const nsDependentSubstring TrimWhitespace(const nsAString& aStr,
                                                   bool aTrimTrailing = true);

  /**
   * Returns true if aChar is of class Ps, Pi, Po, Pf, or Pe.
   */
  static bool IsFirstLetterPunctuation(uint32_t aChar);
  static bool IsFirstLetterPunctuationAt(const nsTextFragment* aFrag, uint32_t aOffset);
 
  /**
   * Returns true if aChar is of class Lu, Ll, Lt, Lm, Lo, Nd, Nl or No
   */
  static bool IsAlphanumeric(uint32_t aChar);
  static bool IsAlphanumericAt(const nsTextFragment* aFrag, uint32_t aOffset);

  /*
   * Is the character an HTML whitespace character?
   *
   * We define whitespace using the list in HTML5 and css3-selectors:
   * U+0009, U+000A, U+000C, U+000D, U+0020
   *
   * HTML 4.01 also lists U+200B (zero-width space).
   */
  static bool IsHTMLWhitespace(char16_t aChar);

  /*
   * Returns whether the character is an HTML whitespace (see IsHTMLWhitespace)
   * or a nbsp character (U+00A0).
   */
  static bool IsHTMLWhitespaceOrNBSP(char16_t aChar);

  /**
   * Is the HTML local name a block element?
   */
  static bool IsHTMLBlock(nsIContent* aContent);

  enum ParseHTMLIntegerResultFlags {
    eParseHTMLInteger_NoFlags               = 0,
    eParseHTMLInteger_IsPercent             = 1 << 0,
    // eParseHTMLInteger_NonStandard is set if the string representation of the
    // integer was not the canonical one (e.g. had extra leading '+' or '0').
    eParseHTMLInteger_NonStandard           = 1 << 1,
    eParseHTMLInteger_DidNotConsumeAllInput = 1 << 2,
    // Set if one or more error flags were set.
    eParseHTMLInteger_Error                 = 1 << 3,
    eParseHTMLInteger_ErrorNoValue          = 1 << 4,
    eParseHTMLInteger_ErrorOverflow         = 1 << 5
  };
  static int32_t ParseHTMLInteger(const nsAString& aValue,
                                  ParseHTMLIntegerResultFlags *aResult);

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
  static int32_t ParseLegacyFontSize(const nsAString& aValue);

  static void Shutdown();

  /**
   * Checks whether two nodes come from the same origin.
   */
  static nsresult CheckSameOrigin(const nsINode* aTrustedNode,
                                  nsIDOMNode* aUnTrustedNode);
  static nsresult CheckSameOrigin(const nsINode* aTrustedNode,
                                  const nsINode* unTrustedNode);

  // Check if the (JS) caller can access aNode.
  static bool CanCallerAccess(nsIDOMNode *aNode);
  static bool CanCallerAccess(nsINode* aNode);

  // Check if the (JS) caller can access aWindow.
  // aWindow can be either outer or inner window.
  static bool CanCallerAccess(nsPIDOMWindow* aWindow);

  /**
   * GetDocumentFromCaller gets its document by looking at the last called
   * function and finding the document that the function itself relates to.
   * For example, consider two windows A and B in the same origin. B has a
   * function which does something that ends up needing the current document.
   * If a script in window A were to call B's function, GetDocumentFromCaller
   * would find that function (in B) and return B's document.
   *
   * @return The document or null if no JS Context.
   */
  static nsIDocument* GetDocumentFromCaller();

  // Check if a node is in the document prolog, i.e. before the document
  // element.
  static bool InProlog(nsINode *aNode);

  static nsIParserService* GetParserService();

  static nsNameSpaceManager* NameSpaceManager()
  {
    return sNameSpaceManager;
  }

  static nsIIOService* GetIOService()
  {
    return sIOService;
  }

  static nsIBidiKeyboard* GetBidiKeyboard();

  /**
   * Get the cache security manager service. Can return null if the layout
   * module has been shut down.
   */
  static nsIScriptSecurityManager* GetSecurityManager()
  {
    return sSecurityManager;
  }

  /**
   * Get the ContentSecurityPolicy for a JS context.
   **/
  static bool GetContentSecurityPolicy(nsIContentSecurityPolicy** aCSP);

  // Returns the subject principal. Guaranteed to return non-null. May only
  // be called when nsContentUtils is initialized.
  static nsIPrincipal* SubjectPrincipal();

  // Returns the prinipal of the given JS object. This may only be called on
  // the main thread for objects from the main thread's JSRuntime.
  static nsIPrincipal* ObjectPrincipal(JSObject* aObj);

  static nsresult GenerateStateKey(nsIContent* aContent,
                                   const nsIDocument* aDocument,
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
   * Convert aInput (in encoding aEncoding) to UTF16 in aOutput.
   *
   * @param aEncoding the Gecko-canonical name of the encoding or the empty
   *                  string (meaning UTF-8)
   */
  static nsresult ConvertStringFromEncoding(const nsACString& aEncoding,
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
  static bool CheckForBOM(const unsigned char* aBuffer, uint32_t aLength,
                          nsACString& aCharset);

  /**
   * Returns true if |aName| is a valid name to be registered via
   * document.registerElement.
   */
  static bool IsCustomElementName(nsIAtom* aName);

  static nsresult CheckQName(const nsAString& aQualifiedName,
                             bool aNamespaceAware = true,
                             const char16_t** aColon = nullptr);

  static nsresult SplitQName(const nsIContent* aNamespaceResolver,
                             const nsAFlatString& aQName,
                             int32_t *aNamespace, nsIAtom **aLocalName);

  static nsresult GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                       const nsAString& aQualifiedName,
                                       nsNodeInfoManager* aNodeInfoManager,
                                       uint16_t aNodeType,
                                       mozilla::dom::NodeInfo** aNodeInfo);

  static void SplitExpatName(const char16_t *aExpatName, nsIAtom **aPrefix,
                             nsIAtom **aTagName, int32_t *aNameSpaceID);

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

  // Get a permission-manager setting for the given principal and type.
  // If the pref doesn't exist or if it isn't ALLOW_ACTION, false is
  // returned, otherwise true is returned. Always returns true for the
  // system principal, and false for a null principal.
  // This version checks the permission for an exact host match on
  // the principal
  static bool IsExactSitePermAllow(nsIPrincipal* aPrincipal, const char* aType);

  // Get a permission-manager setting for the given principal and type.
  // If the pref doesn't exist or if it isn't DENY_ACTION, false is
  // returned, otherwise true is returned. Always returns false for the
  // system principal, and true for a null principal.
  // This version checks the permission for an exact host match on
  // the principal
  static bool IsExactSitePermDeny(nsIPrincipal* aPrincipal, const char* aType);

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
  static bool HasNonEmptyAttr(const nsIContent* aContent, int32_t aNameSpaceID,
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
   * @param [aContentPolicyType=nsIContentPolicy::TYPE_IMAGE] (Optional)
   *        The CP content type to use
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
                           int16_t* aImageBlockingStatus = nullptr,
                           uint32_t aContentPolicyType = nsIContentPolicy::TYPE_IMAGE);

  /**
   * Returns true if objects in aDocument shouldn't initiate image loads.
   */
  static bool DocumentInactiveForImageLoads(nsIDocument* aDocument);

  /**
   * Method to start an image load.  This does not do any security checks.
   * This method will attempt to make aURI immutable; a caller that wants to
   * keep a mutable version around should pass in a clone.
   *
   * @param aURI uri of the image to be loaded
   * @param aLoadingDocument the document we belong to
   * @param aLoadingPrincipal the principal doing the load
   * @param aReferrer the referrer URI
   * @param aReferrerPolicy the referrer-sending policy to use on channel
   *         creation
   * @param aObserver the observer for the image load
   * @param aLoadFlags the load flags to use.  See nsIRequest
   * @param [aContentPolicyType=nsIContentPolicy::TYPE_IMAGE] (Optional)
   *        The CP content type to use
   * @return the imgIRequest for the image load
   */
  static nsresult LoadImage(nsIURI* aURI,
                            nsIDocument* aLoadingDocument,
                            nsIPrincipal* aLoadingPrincipal,
                            nsIURI* aReferrer,
                            mozilla::net::ReferrerPolicy aReferrerPolicy,
                            imgINotificationObserver* aObserver,
                            int32_t aLoadFlags,
                            const nsAString& initiatorType,
                            imgRequestProxy** aRequest,
                            uint32_t aContentPolicyType = nsIContentPolicy::TYPE_IMAGE);

  /**
   * Obtain an image loader that respects the given document/channel's privacy status.
   * Null document/channel arguments return the public image loader.
   */
  static imgLoader* GetImgLoaderForDocument(nsIDocument* aDoc);
  static imgLoader* GetImgLoaderForChannel(nsIChannel* aChannel,
                                           nsIDocument* aContext);

  /**
   * Returns whether the given URI is in the image cache.
   */
  static bool IsImageInCache(nsIURI* aURI, nsIDocument* aDocument);

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
  static already_AddRefed<imgRequestProxy> GetStaticRequest(imgRequestProxy* aRequest);

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
  static nsresult NameChanged(mozilla::dom::NodeInfo* aNodeInfo, nsIAtom* aName,
                              mozilla::dom::NodeInfo** aResult);

  /**
   * Returns the appropriate event argument names for the specified
   * namespace and event name.  Added because we need to switch between
   * SVG's "evt" and the rest of the world's "event", and because onerror
   * on window takes 5 args.
   */
  static void GetEventArgNames(int32_t aNameSpaceID, nsIAtom *aEventName,
                               bool aIsForWindow,
                               uint32_t *aArgCount, const char*** aArgNames);

  /**
   * Returns true if this document is in a Private Browsing window.
   */
  static bool IsInPrivateBrowsing(nsIDocument* aDoc);

  /**
   * Returns true if this loadGroup uses Private Browsing.
   */
  static bool IsInPrivateBrowsing(nsILoadGroup* aLoadGroup);

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
   * Report simple error message to the browser console
   *   @param aErrorText the error message
   *   @param classification Name of the module reporting error
   */
  static void LogSimpleConsoleError(const nsAString& aErrorText,
                                    const char * classification);

  /**
   * Report a non-localized error message to the error console.
   *   @param aErrorText the error message
   *   @param aErrorFlags See nsIScriptError.
   *   @param aCategory Name of module reporting error.
   *   @param aDocument Reference to the document which triggered the message.
   *   @param [aURI=nullptr] (Optional) URI of resource containing error.
   *   @param [aSourceLine=EmptyString()] (Optional) The text of the line that
              contains the error (may be empty).
   *   @param [aLineNumber=0] (Optional) Line number within resource
              containing error.
   *   @param [aColumnNumber=0] (Optional) Column number within resource
              containing error.
              If aURI is null, then aDocument->GetDocumentURI() is used.
   */
  static nsresult ReportToConsoleNonLocalized(const nsAString& aErrorText,
                                              uint32_t aErrorFlags,
                                              const nsACString& aCategory,
                                              const nsIDocument* aDocument,
                                              nsIURI* aURI = nullptr,
                                              const nsAFlatString& aSourceLine
                                                = EmptyString(),
                                              uint32_t aLineNumber = 0,
                                              uint32_t aColumnNumber = 0);

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
    eMATHML_PROPERTIES,
    eSECURITY_PROPERTIES,
    eNECKO_PROPERTIES,
    PropertiesFile_COUNT
  };
  static nsresult ReportToConsole(uint32_t aErrorFlags,
                                  const nsACString& aCategory,
                                  const nsIDocument* aDocument,
                                  PropertiesFile aFile,
                                  const char *aMessageName,
                                  const char16_t **aParams = nullptr,
                                  uint32_t aParamsLength = 0,
                                  nsIURI* aURI = nullptr,
                                  const nsAFlatString& aSourceLine
                                    = EmptyString(),
                                  uint32_t aLineNumber = 0,
                                  uint32_t aColumnNumber = 0);

  static nsresult
  MaybeReportInterceptionErrorToConsole(nsIDocument* aDocument, nsresult aError);

  static void LogMessageToConsole(const char* aMsg, ...);
  
  /**
   * Get the localized string named |aKey| in properties file |aFile|.
   */
  static nsresult GetLocalizedString(PropertiesFile aFile,
                                     const char* aKey,
                                     nsXPIDLString& aResult);

  /**
   * A helper function that parses a sandbox attribute (of an <iframe> or
   * a CSP directive) and converts it to the set of flags used internally.
   *
   * @param sandboxAttr   the sandbox attribute
   * @return              the set of flags (0 if sandboxAttr is null)
   */
  static uint32_t ParseSandboxAttributeToFlags(const nsAttrValue* sandboxAttr);

  /**
   * Helper function that generates a UUID.
   */
  static nsresult GenerateUUIDInPlace(nsID& aUUID);


  /**
   * Fill (with the parameters given) the localized string named |aKey| in
   * properties file |aFile|.
   */
private:
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const char16_t** aParams,
                                        uint32_t aParamsLength,
                                        nsXPIDLString& aResult);
  
public:
  template<uint32_t N>
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const char16_t* (&aParams)[N],
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
  '* Returns true if the content-type will be rendered as plain-text.
   */
  static bool IsPlainTextType(const nsACString& aContentType);

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
   * Map internal content policy types to external ones.
   */
  static nsContentPolicyType InternalContentPolicyTypeToExternal(nsContentPolicyType aType);

  /**
   * Map internal content policy types to external ones or script types:
   *   * TYPE_INTERNAL_SCRIPT
   *   * TYPE_INTERNAL_WORKER
   *   * TYPE_INTERNAL_SHARED_WORKER
   *   * TYPE_INTERNAL_SERVICE_WORKER
   *
   *
   * Note: DO NOT call this function unless you know what you're doing!
   */
  static nsContentPolicyType InternalContentPolicyTypeToExternalOrScript(nsContentPolicyType aType);

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
                                     uint32_t aType,
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
                                     uint32_t aType);
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
   * event handler (the parent object of the DOM Window in the event target
   * chain). Note, chrome event handler is used even if aTarget is a chrome
   * object. Use DispatchEventOnlyToChrome if the normal event dispatching is
   * wanted in case aTarget is a chrome object.
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
   * This method creates and dispatches a trusted event.
   * If aTarget is not a chrome object, the nearest chrome object in the
   * propagation path will be used as the start of the event target chain.
   * This method is different than DispatchChromeEvent, which always dispatches
   * events to chrome event handler. DispatchEventOnlyToChrome works like
   * DispatchTrustedEvent in the case aTarget is a chrome object.
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
  static nsresult DispatchEventOnlyToChrome(nsIDocument* aDoc,
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
  static bool IsEventAttributeName(nsIAtom* aName, int32_t aType);

  /**
   * Return the event message for the event with the given name. The name is
   * the event name with the 'on' prefix. Returns eUnidentifiedEvent if the
   * event doesn't match a known event name.
   *
   * @param aName the event name to look up
   */
  static mozilla::EventMessage GetEventMessage(nsIAtom* aName);

  /**
   * Return the EventClassID for the event with the given name. The name is the
   * event name *without* the 'on' prefix. Returns eBasicEventClass if the event
   * is not known to be of any particular event class.
   *
   * @param aName the event name to look up
   */
  static mozilla::EventClassID GetEventClassID(const nsAString& aName);

  /**
   * Return the event message and atom for the event with the given name.
   * The name is the event name *without* the 'on' prefix.
   * Returns eUnidentifiedEvent on the aEventID if the
   * event doesn't match a known event name in the category.
   *
   * @param aName the event name to look up
   * @param aEventClassID only return event id for aEventClassID
   */
  static nsIAtom* GetEventMessageAndAtom(const nsAString& aName,
                                         mozilla::EventClassID aEventClassID,
                                         mozilla::EventMessage* aEventMessage);

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
   * Get the eventlistener manager for aNode, creating it if it does not
   * already exist.
   *
   * @param aNode The node for which to get the eventlistener manager.
   */
  static mozilla::EventListenerManager*
    GetListenerManagerForNode(nsINode* aNode);
  /**
   * Get the eventlistener manager for aNode, returning null if it does not
   * already exist.
   *
   * @param aNode The node for which to get the eventlistener manager.
   */
  static mozilla::EventListenerManager*
    GetExistingListenerManagerForNode(const nsINode* aNode);

  static void UnmarkGrayJSListenersInCCGenerationDocuments();

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
                                int32_t aNamespaceID);

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
  static already_AddRefed<mozilla::dom::DocumentFragment>
  CreateContextualFragment(nsINode* aContextNode, const nsAString& aFragment,
                           bool aPreventScriptExecution,
                           mozilla::ErrorResult& aRv);

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
                                    int32_t aContextNamespace,
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
                                     uint32_t aFlags,
                                     uint32_t aWrapCol);

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
   * @see nsLayoutUtils::GetFrameTextContent
   *
   * @param aNode Node to get textual contents of.
   * @param aDeep If true child elements of aNode are recursivly descended
   *              into to find text children.
   * @param aResult the result. Out param.
   * @return false on out of memory errors, true otherwise.
   */
  MOZ_WARN_UNUSED_RESULT
  static bool GetNodeTextContent(nsINode* aNode, bool aDeep,
                                 nsAString& aResult, const mozilla::fallible_t&);

  static void GetNodeTextContent(nsINode* aNode, bool aDeep,
                                 nsAString& aResult);

  /**
   * Same as GetNodeTextContents but appends the result rather than sets it.
   */
  static bool AppendNodeTextContent(nsINode* aNode, bool aDeep,
                                    nsAString& aResult, const mozilla::fallible_t&);

  /**
   * Utility method that checks if a given node has any non-empty children. This
   * method does not descend recursively into children by default.
   *
   * @param aDiscoverMode Set to eRecurseIntoChildren to descend recursively
   * into children.
   */
  enum TextContentDiscoverMode : uint8_t {
    eRecurseIntoChildren, eDontRecurseIntoChildren
  };

  static bool HasNonEmptyTextContent(
    nsINode* aNode,
    TextContentDiscoverMode aDiscoverMode = eDontRecurseIntoChildren);

  /**
   * Delete strings allocated for nsContentList matches
   */
  static void DestroyMatchString(void* aData);

  /**
   * Unbinds the content from the tree and nulls it out if it's not null.
   */
  static void DestroyAnonymousContent(nsCOMPtr<nsIContent>* aContent);
  static void DestroyAnonymousContent(nsCOMPtr<Element>* aElement);

  /*
   * Notify when the first XUL menu is opened and when the all XUL menus are
   * closed. At opening, aInstalling should be TRUE, otherwise, it should be
   * FALSE.
   */
  static void NotifyInstalledMenuKeyboardListener(bool aInstalling);

  /**
   * Returns true if aPrincipal is the system principal.
   */
  static bool IsSystemPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Returns true if aPrincipal is an nsExpandedPrincipal.
   */
  static bool IsExpandedPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Returns true if aPrincipal is the system or an nsExpandedPrincipal.
   */
  static bool IsSystemOrExpandedPrincipal(nsIPrincipal* aPrincipal)
  {
    return IsSystemPrincipal(aPrincipal) || IsExpandedPrincipal(aPrincipal);
  }

  /**
   * Gets the system principal from the security manager.
   */
  static nsIPrincipal* GetSystemPrincipal();

  /**
   * Gets the null subject principal singleton. This is only useful for
   * assertions.
   */
  static nsIPrincipal* GetNullSubjectPrincipal() { return sNullSubjectPrincipal; }

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
  static void GetAccessKeyCandidates(
                mozilla::WidgetKeyboardEvent* aNativeKeyEvent,
                nsTArray<uint32_t>& aCandidates);

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
   * Initialize and set the dataTransfer field of an WidgetDragEvent.
   */
  static nsresult SetDataTransferInEvent(mozilla::WidgetDragEvent* aDragEvent);

  // filters the drag and drop action to fit within the effects allowed and
  // returns it.
  static uint32_t FilterDropEffect(uint32_t aAction, uint32_t aEffectAllowed);

  /*
   * Return true if the target of a drop event is a content document that is
   * an ancestor of the document for the source of the drag.
   */
  static bool CheckForSubFrameDrop(nsIDragSession* aDragSession,
                                   mozilla::WidgetDragEvent* aDropEvent);

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
   * If offline-apps.allow_by_default is true, we set offline-app permission
   * for the principal and return true.  Otherwise false.
   */
  static bool MaybeAllowOfflineAppByDefault(nsIPrincipal *aPrincipal, nsIDOMWindow *aWindow);

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
   * Call this function if !IsSafeToRunScript() and we fail to run the script
   * (rather than using AddScriptRunner as we usually do). |aDocument| is
   * optional as it is only used for showing the URL in the console.
   */
  static void WarnScriptWasIgnored(nsIDocument* aDocument);

  /**
   * Add a "synchronous section", in the form of an nsIRunnable run once the
   * event loop has reached a "stable state". |aRunnable| must not cause any
   * queued events to be processed (i.e. must not spin the event loop).
   * We've reached a stable state when the currently executing task/event has
   * finished, see
   * http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#synchronous-section
   * In practice this runs aRunnable once the currently executing event
   * finishes. If called multiple times per task/event, all the runnables will
   * be executed, in the order in which RunInStableState() was called.
   */
  static void RunInStableState(already_AddRefed<nsIRunnable> aRunnable);

  /* Add a "synchronous section", in the form of an nsIRunnable run once the
   * event loop has reached a "metastable state". |aRunnable| must not cause any
   * queued events to be processed (i.e. must not spin the event loop).
   * We've reached a metastable state when the currently executing task or
   * microtask has finished.  This is not specced at this time.
   * In practice this runs aRunnable once the currently executing task or
   * microtask finishes.  If called multiple times per microtask, all the
   * runnables will be executed, in the order in which RunInMetastableState()
   * was called
   */
  static void RunInMetastableState(already_AddRefed<nsIRunnable> aRunnable);

  /**
   * Retrieve information about the viewport as a data structure.
   * This will return information in the viewport META data section
   * of the document. This can be used in lieu of ProcessViewportInfo(),
   * which places the viewport information in the document header instead
   * of returning it directly.
   *
   * @param aDisplayWidth width of the on-screen display area for this
   * document, in device pixels.
   * @param aDisplayHeight height of the on-screen display area for this
   * document, in device pixels.
   *
   * NOTE: If the site is optimized for mobile (via the doctype), this
   * will return viewport information that specifies default information.
   */
  static nsViewportInfo GetViewportInfo(nsIDocument* aDocument,
                                        const mozilla::ScreenIntSize& aDisplaySize);

  // Call EnterMicroTask when you're entering JS execution.
  // Usually the best way to do this is to use nsAutoMicroTask.
  static void EnterMicroTask();
  static void LeaveMicroTask();

  static bool IsInMicroTask();
  static uint32_t MicroTaskLevel();
  static void SetMicroTaskLevel(uint32_t aLevel);

  static void PerformMainThreadMicroTaskCheckpoint();

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
  static JSContext *GetCurrentJSContextForThread();
  static JSContext *GetDefaultJSContextForThread();
  inline static JSContext *RootingCx() { return GetSafeJSContext(); }
  inline static JSContext *RootingCxForThread() { return GetDefaultJSContextForThread(); }

  /**
   * Case insensitive comparison between two strings. However it only ignores
   * case for ASCII characters a-z.
   */
  static bool EqualsIgnoreASCIICase(const nsAString& aStr1,
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

  /**
   * Return whether aStr contains an ASCII uppercase character.
   */
  static bool StringContainsASCIIUpper(const nsAString& aStr);

  // Returns NS_OK for same origin, error (NS_ERROR_DOM_BAD_URI) if not.
  static nsresult CheckSameOrigin(nsIChannel *aOldChannel, nsIChannel *aNewChannel);
  static nsIInterfaceRequestor* SameOriginChecker();

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
                                 nsACString& aOrigin);
  static nsresult GetASCIIOrigin(nsIURI* aURI, nsACString& aOrigin);
  static nsresult GetUTFOrigin(nsIPrincipal* aPrincipal,
                               nsAString& aOrigin);
  static nsresult GetUTFOrigin(nsIURI* aURI, nsAString& aOrigin);

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
  static nsIDocument*
  GetDocumentFromScriptContext(nsIScriptContext* aScriptContext);

  static bool CheckMayLoad(nsIPrincipal* aPrincipal, nsIChannel* aChannel, bool aAllowIfInheritsPrincipal);

  /**
   * The method checks whether the caller can access native anonymous content.
   * If there is no JS in the stack or privileged JS is running, this
   * method returns true, otherwise false.
   */
  static bool CanAccessNativeAnon();

  MOZ_WARN_UNUSED_RESULT
  static nsresult WrapNative(JSContext *cx, nsISupports *native,
                             const nsIID* aIID, JS::MutableHandle<JS::Value> vp,
                             bool aAllowWrapping = true)
  {
    return WrapNative(cx, native, nullptr, aIID, vp, aAllowWrapping);
  }

  // Same as the WrapNative above, but use this one if aIID is nsISupports' IID.
  MOZ_WARN_UNUSED_RESULT
  static nsresult WrapNative(JSContext *cx, nsISupports *native,
                             JS::MutableHandle<JS::Value> vp,
                             bool aAllowWrapping = true)
  {
    return WrapNative(cx, native, nullptr, nullptr, vp, aAllowWrapping);
  }

  MOZ_WARN_UNUSED_RESULT
  static nsresult WrapNative(JSContext *cx, nsISupports *native,
                             nsWrapperCache *cache,
                             JS::MutableHandle<JS::Value> vp,
                             bool aAllowWrapping = true)
  {
    return WrapNative(cx, native, cache, nullptr, vp, aAllowWrapping);
  }

  /**
   * Creates an arraybuffer from a binary string.
   */
  static nsresult CreateArrayBuffer(JSContext *aCx, const nsACString& aData,
                                    JSObject** aResult);

  static nsresult CreateBlobBuffer(JSContext* aCx,
                                   nsISupports* aParent,
                                   const nsACString& aData,
                                   JS::MutableHandle<JS::Value> aBlob);

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
  MOZ_WARN_UNUSED_RESULT
  static bool PlatformToDOMLineBreaks(nsString &aString,
                                      const mozilla::fallible_t&);

  /**
   * Populates aResultString with the contents of the string-buffer aBuf, up
   * to aBuf's null-terminator.  aBuf must not be null. Ownership of the string
   * is not transferred.
   */
  static void PopulateStringFromStringBuffer(nsStringBuffer* aBuf,
                                             nsAString& aResultString);

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
  static already_AddRefed<nsContentList>
  GetElementsByClassName(nsINode* aRootNode, const nsAString& aClasses)
  {
    NS_PRECONDITION(aRootNode, "Must have root node");

    return NS_GetFuncStringHTMLCollection(aRootNode, MatchClassNames,
                                          DestroyClassNameArray,
                                          AllocClassMatchingInfo,
                                          aClasses);
  }

  /**
   * Returns a presshell for this document, if there is one. This will be
   * aDoc's direct presshell if there is one, otherwise we'll look at all
   * ancestor documents to try to find a presshell, so for example this can
   * still find a presshell for documents in display:none frames that have
   * no presentation. So you have to be careful how you use this presshell ---
   * getting generic data like a device context or widget from it is OK, but it
   * might not be this document's actual presentation.
   */
  static nsIPresShell* FindPresShellForDocument(const nsIDocument* aDoc);

  /**
   * Returns the widget for this document if there is one. Looks at all ancestor
   * documents to try to find a widget, so for example this can still find a
   * widget for documents in display:none frames that have no presentation.
   */
  static nsIWidget* WidgetForDocument(const nsIDocument* aDoc);

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
  LayerManagerForDocument(const nsIDocument *aDoc, bool *aAllowRetaining = nullptr);

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
   * Returns true if calling execCommand with 'cut' or 'copy' arguments
   * is restricted to chrome code.
   */
  static bool IsCutCopyRestricted()
  {
    return !sIsCutCopyAllowed;
  }

  /**
   * Returns true if calling execCommand with 'cut' or 'copy' arguments is
   * allowed in the current context. These are only allowed if the user initiated
   * them (like with a mouse-click or key press).
   */
  static bool IsCutCopyAllowed();

  /*
   * Returns true if the performance timing APIs are enabled.
   */
  static bool IsPerformanceTimingEnabled()
  {
    return sIsPerformanceTimingEnabled;
  }
  
  /*
   * Returns true if user timing API should print to console.
   */
  static bool IsUserTimingLoggingEnabled()
  {
    return sIsUserTimingLoggingEnabled;
  }

  /*
   * Returns true if the performance timing APIs are enabled.
   */
  static bool IsResourceTimingEnabled()
  {
    return sIsResourceTimingEnabled;
  }

  /*
   * Returns true if notification should be sent for peformance timing events.
   */
  static bool SendPerformanceTimingNotifications()
  {
    return sSendPerformanceTimingNotifications;
  }

  /*
   * Returns true if the frame timing APIs are enabled.
   */
  static bool IsFrameTimingEnabled();

  /*
   * Returns true if URL setters should percent encode the Hash/Ref segment
   * and getters should return the percent decoded value of the segment
   */
  static bool EncodeDecodeURLHash()
  {
    return sEncodeDecodeURLHash;
  }

  /*
   * Returns true if URL getters should percent decode the value of the segment
   */
  static bool GettersDecodeURLHash()
  {
    return sGettersDecodeURLHash && sEncodeDecodeURLHash;
  }

  /*
   * Returns true if the browser should attempt to prevent content scripts
   * from collecting distinctive information about the browser that could
   * be used to "fingerprint" and track the user across websites.
   */
  static bool ResistFingerprinting()
  {
    return sPrivacyResistFingerprinting;
  }

  /**
   * Returns true if the doc tree branch which contains aDoc contains any
   * plugins which we don't control event dispatch for, i.e. do any plugins
   * in the same tab as this document receive key events outside of our
   * control? This always returns false on MacOSX.
   */
  static bool HasPluginWithUncontrolledEventDispatch(nsIDocument* aDoc);

  /**
   * Fire mutation events for changes caused by parsing directly into a
   * context node.
   *
   * @param aDoc the document of the node
   * @param aDest the destination node that got stuff appended to it
   * @param aOldChildCount the number of children the node had before parsing
   */
  static void FireMutationEventsForDirectParsing(nsIDocument* aDoc,
                                                 nsIContent* aDest,
                                                 int32_t aOldChildCount);

  /**
   * Returns true if the content is in a document and contains a plugin
   * which we don't control event dispatch for, i.e. do any plugins in this
   * doc tree receive key events outside of our control? This always returns
   * false on MacOSX.
   */
  static bool HasPluginWithUncontrolledEventDispatch(nsIContent* aContent);

  /**
   * Returns the root document in a document hierarchy. Normally this
   * will be the chrome document.
   */
  static nsIDocument* GetRootDocument(nsIDocument* aDoc);

  /**
   * Returns true if aWin and the current pointer lock document
   * have common scriptable top window.
   */
  static bool IsInPointerLockContext(nsIDOMWindow* aWin);

  /**
   * Returns the time limit on handling user input before
   * EventStateManager::IsHandlingUserInput() stops returning true.
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
   * Returns if aNode ignores user focus.
   *
   * @param aNode node to test
   *
   * @return Whether the node ignores user focus.
   */
  static bool IsUserFocusIgnored(nsINode* aNode);

  /**
   * Returns if aContent has the 'scrollgrab' property.
   * aContent may be null (in this case false is returned).
   */
  static bool HasScrollgrab(nsIContent* aContent);

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

  /**
   * Checks if internal PDF viewer is enabled.
   */
  static bool IsPDFJSEnabled();

  /**
   * Checks if internal SWF player is enabled.
   */
  static bool IsSWFPlayerEnabled();

  enum ContentViewerType
  {
      TYPE_UNSUPPORTED,
      TYPE_CONTENT,
      TYPE_PLUGIN,
      TYPE_UNKNOWN
  };

  static already_AddRefed<nsIDocumentLoaderFactory>
  FindInternalContentViewer(const nsACString& aType,
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
    * Called before a channel is created to query whether the new
    * channel should inherit the principal.
    *
    * The argument aLoadingPrincipal must not be null. The argument
    * aURI must be the URI of the new channel. If aInheritForAboutBlank
    * is true, then about:blank will be told to inherit the principal.
    * If aForceInherit is true, the new channel will be told to inherit
    * the principal no matter what.
    *
    * The return value is whether the new channel should inherit
    * the principal.
    */
  static bool ChannelShouldInheritPrincipal(nsIPrincipal* aLoadingPrincipal,
                                            nsIURI* aURI,
                                            bool aInheritForAboutBlank,
                                            bool aForceInherit);

  static nsresult Btoa(const nsAString& aBinaryData,
                       nsAString& aAsciiBase64String);

  static nsresult Atob(const nsAString& aAsciiString,
                       nsAString& aBinaryData);

  /**
   * Returns whether the input element passed in parameter has the autocomplete
   * functionality enabled. It is taking into account the form owner.
   * NOTE: the caller has to make sure autocomplete makes sense for the
   * element's type.
   *
   * @param aInput the input element to check. NOTE: aInput can't be null.
   * @return whether the input element has autocomplete enabled.
   */
  static bool IsAutocompleteEnabled(nsIDOMHTMLInputElement* aInput);

  enum AutocompleteAttrState : uint8_t
  {
    eAutocompleteAttrState_Unknown = 1,
    eAutocompleteAttrState_Invalid,
    eAutocompleteAttrState_Valid,
  };
  /**
   * Parses the value of the autocomplete attribute into aResult, ensuring it's
   * composed of valid tokens, otherwise the value "" is used.
   * Note that this method is used for form fields, not on a <form> itself.
   *
   * @return whether aAttr was valid and can be cached.
   */
  static AutocompleteAttrState
  SerializeAutocompleteAttribute(const nsAttrValue* aAttr,
                                 nsAString& aResult,
                                 AutocompleteAttrState aCachedState =
                                   eAutocompleteAttrState_Unknown);

  /* Variation that is used to retrieve a dictionary of the parts of the
   * autocomplete attribute.
   *
   * @return whether aAttr was valid and can be cached.
   */
  static AutocompleteAttrState
  SerializeAutocompleteAttribute(const nsAttrValue* aAttr,
                                 mozilla::dom::AutocompleteInfo& aInfo,
                                 AutocompleteAttrState aCachedState =
                                   eAutocompleteAttrState_Unknown);

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
  static bool IsJavaScriptLanguage(const nsString& aName);

  /**
   * Returns the JSVersion for a string of the form '1.n', n = 0, ..., 8, and
   * JSVERSION_UNKNOWN for other strings.
   */
  static JSVersion ParseJavascriptVersion(const nsAString& aVersionStr);

  static bool IsJavascriptMIMEType(const nsAString& aMIMEType);

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
  static nsresult IsUserIdle(uint32_t aRequestedIdleTimeInMS, bool* aUserIsIdle);

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
  static void GetSelectionInTextControl(mozilla::dom::Selection* aSelection,
                                        Element* aRoot,
                                        int32_t& aOutStartOffset,
                                        int32_t& aOutEndOffset);

  /**
   * Takes a selection, and return selection's bounding rect which is relative
   * to root frame.
   *
   * @param aSel      Selection to check
   */
  static nsRect GetSelectionBoundingRect(mozilla::dom::Selection* aSel);

  /**
   * Takes a frame for anonymous content within a text control (<input> or
   * <textarea>), and returns an offset in the text content, adjusted for a
   * trailing <br> frame.
   *
   * @param aOffsetFrame      Frame for the text content in which the offset
   *                          lies
   * @param aOffset           Offset as calculated by GetContentOffsetsFromPoint
   * @param aOutOffset        Output adjusted offset
   *
   * @see GetSelectionInTextControl for the original basis of this function.
   */
  static int32_t GetAdjustedOffsetInTextControl(nsIFrame* aOffsetFrame,
                                                int32_t aOffset);

  static nsIEditor* GetHTMLEditor(nsPresContext* aPresContext);

  /**
   * Check whether a spec feature/version is supported.
   * @param aObject the object, which should support the feature,
   *        for example nsIDOMNode or nsIDOMDOMImplementation
   * @param aFeature the feature ("Views", "Core", "HTML", "Range" ...)
   * @param aVersion the version ("1.0", "2.0", ...)
   * @return whether the feature is supported or not
   */
  static bool InternalIsSupported(nsISupports* aObject,
                                  const nsAString& aFeature,
                                  const nsAString& aVersion);

  /**
   * Return true if the browser.dom.window.dump.enabled pref is set.
   */
  static bool DOMWindowDumpEnabled();

  /**
   * Returns whether a content is an insertion point for XBL
   * bindings or web components ShadowRoot. In web components,
   * this corresponds to a <content> element that participates
   * in node distribution. In XBL this corresponds to an
   * <xbl:children> element in anonymous content.
   *
   * @param aContent The content to test for being an insertion point.
   */
  static bool IsContentInsertionPoint(const nsIContent* aContent);


  /**
   * Returns whether the children of the provided content are
   * nodes that are distributed to Shadow DOM insertion points.
   */
  static bool HasDistributedChildren(nsIContent* aContent);

  /**
   * Returns whether a given header is forbidden for an XHR or fetch
   * request.
   */
  static bool IsForbiddenRequestHeader(const nsACString& aHeader);

  /**
   * Returns whether a given header is forbidden for a system XHR
   * request.
   */
  static bool IsForbiddenSystemRequestHeader(const nsACString& aHeader);

  /**
   * Returns whether a given Content-Type header value is allowed
   * for a non-CORS XHR or fetch request.
   */
  static bool IsAllowedNonCorsContentType(const nsACString& aHeaderValue);

  /**
   * Returns whether a given header is forbidden for an XHR or fetch
   * response.
   */
  static bool IsForbiddenResponseHeader(const nsACString& aHeader);

  /**
   * Returns the inner window ID for the window associated with a request,
   */
  static uint64_t GetInnerWindowID(nsIRequest* aRequest);

  /**
   * If the hostname for aURI is an IPv6 it encloses it in brackets,
   * otherwise it just outputs the hostname in aHost.
   */
  static void GetHostOrIPv6WithBrackets(nsIURI* aURI, nsAString& aHost);
  static void GetHostOrIPv6WithBrackets(nsIURI* aURI, nsCString& aHost);

  /*
   * Call the given callback on all remote children of the given top-level
   * window.
   */
  static void CallOnAllRemoteChildren(nsIDOMWindow* aWindow,
                                      CallOnRemoteChildFunction aCallback,
                                      void* aArg);

  static void TransferablesToIPCTransferables(nsISupportsArray* aTransferables,
                                              nsTArray<mozilla::dom::IPCDataTransfer>& aIPC,
                                              mozilla::dom::nsIContentChild* aChild,
                                              mozilla::dom::nsIContentParent* aParent);

  static void TransferableToIPCTransferable(nsITransferable* aTransferable,
                                            mozilla::dom::IPCDataTransfer* aIPCDataTransfer,
                                            mozilla::dom::nsIContentChild* aChild,
                                            mozilla::dom::nsIContentParent* aParent);

  /*
   * Get the pixel data from the given source surface and return it as a buffer.
   * The length and stride will be assigned from the surface.
   */
  static mozilla::UniquePtr<char[]> GetSurfaceData(mozilla::gfx::DataSourceSurface* aSurface,
                                                   size_t* aLength, int32_t* aStride);

  // Helpers shared by the implementations of nsContentUtils methods and
  // nsIDOMWindowUtils methods.
  static mozilla::Modifiers GetWidgetModifiers(int32_t aModifiers);
  static nsIWidget* GetWidget(nsIPresShell* aPresShell, nsPoint* aOffset);
  static int16_t GetButtonsFlagForButton(int32_t aButton);
  static mozilla::LayoutDeviceIntPoint ToWidgetPoint(const mozilla::CSSPoint& aPoint,
                                                     const nsPoint& aOffset,
                                                     nsPresContext* aPresContext);
  static nsView* GetViewToDispatchEvent(nsPresContext* aPresContext,
                                        nsIPresShell** aPresShell);

  /**
   * Synthesize a key event to the given widget
   * (see nsIDOMWindowUtils.sendKeyEvent).
   */
  static nsresult SendKeyEvent(nsIWidget* aWidget,
                               const nsAString& aType,
                               int32_t aKeyCode,
                               int32_t aCharCode,
                               int32_t aModifiers,
                               uint32_t aAdditionalFlags,
                               bool* aDefaultActionTaken);

  /**
   * Synthesize a mouse event to the given widget
   * (see nsIDOMWindowUtils.sendMouseEvent).
   */
  static nsresult SendMouseEvent(nsCOMPtr<nsIPresShell> aPresShell,
                                 const nsAString& aType,
                                 float aX,
                                 float aY,
                                 int32_t aButton,
                                 int32_t aClickCount,
                                 int32_t aModifiers,
                                 bool aIgnoreRootScrollFrame,
                                 float aPressure,
                                 unsigned short aInputSourceArg,
                                 bool aToWindow,
                                 bool *aPreventDefault,
                                 bool aIsSynthesized);

  static void FirePageShowEvent(nsIDocShellTreeItem* aItem,
                                mozilla::dom::EventTarget* aChromeEventHandler,
                                bool aFireIfShowing);

  static void FirePageHideEvent(nsIDocShellTreeItem* aItem,
                                mozilla::dom::EventTarget* aChromeEventHandler);

  static already_AddRefed<nsPIWindowRoot> GetWindowRoot(nsIDocument* aDoc);

  /*
   * Implements step 3.1 and 3.3 of the Determine request's Referrer algorithm
   * from the Referrer Policy specification.
   *
   * The referrer policy of the document is applied by Necko when using
   * channels.
   *
   * For documents representing an iframe srcdoc attribute, the document sets
   * its own URI correctly, so this method simply uses the document's original
   * or current URI as appropriate.
   *
   * aDoc may be null.
   *
   * https://w3c.github.io/webappsec/specs/referrer-policy/#determine-requests-referrer
   */
  static nsresult SetFetchReferrerURIWithPolicy(nsIPrincipal* aPrincipal,
                                                nsIDocument* aDoc,
                                                nsIHttpChannel* aChannel);

  static bool PushEnabled(JSContext* aCx, JSObject* aObj);

  // The order of these entries matters, as we use std::min for total ordering
  // of permissions. Private Browsing is considered to be more limiting
  // then session scoping
  enum class StorageAccess {
    // Don't allow access to the storage
    eDeny = 0,
    // Allow access to the storage, but only if it is secure to do so in a
    // private browsing context.
    ePrivateBrowsing = 1,
    // Allow access to the storage, but only persist it for the current session
    eSessionScoped = 2,
    // Allow access to the storage
    eAllow = 3,
  };

  /*
   * Checks if storage for the given window is permitted by a combination of
   * the user's preferences, and whether the window is a third-party iframe.
   *
   * This logic is intended to be shared between the different forms of
   * persistent storage which are available to web pages. Cookies don't use
   * this logic, and security logic related to them must be updated separately.
   */
  static StorageAccess StorageAllowedForWindow(nsPIDOMWindow* aWindow);

  /*
   * Checks if storage for the given principal is permitted by the user's
   * preferences. The caller is assumed to not be a third-party iframe.
   * (if that is possible, the caller should use StorageAllowedForWindow)
   */
  static StorageAccess StorageAllowedForPrincipal(nsIPrincipal* aPrincipal);

private:
  static bool InitializeEventTable();

  static nsresult EnsureStringBundle(PropertiesFile aFile);

  static bool CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                nsIPrincipal* aPrincipal);

  static nsresult WrapNative(JSContext *cx, nsISupports *native,
                             nsWrapperCache *cache, const nsIID* aIID,
                             JS::MutableHandle<JS::Value> vp,
                             bool aAllowWrapping);

  static nsresult DispatchEvent(nsIDocument* aDoc,
                                nsISupports* aTarget,
                                const nsAString& aEventName,
                                bool aCanBubble,
                                bool aCancelable,
                                bool aTrusted,
                                bool *aDefaultAction = nullptr,
                                bool aOnlyChromeDispatch = false);

  static void InitializeModifierStrings();

  static void DropFragmentParsers();

  static bool MatchClassNames(nsIContent* aContent, int32_t aNamespaceID,
                              nsIAtom* aAtom, void* aData);
  static void DestroyClassNameArray(void* aData);
  static void* AllocClassMatchingInfo(nsINode* aRootNode,
                                      const nsString* aClasses);

  // Fills in aInfo with the tokens from the supplied autocomplete attribute.
  static AutocompleteAttrState InternalSerializeAutocompleteAttribute(const nsAttrValue* aAttrVal,
                                                                      mozilla::dom::AutocompleteInfo& aInfo);

  static void CallOnAllRemoteChildren(nsIMessageBroadcaster* aManager,
                                      CallOnRemoteChildFunction aCallback,
                                      void* aArg);

  /*
   * Checks if storage for a given principal is permitted by the user's
   * preferences. If aWindow is non-null, its principal must be passed as
   * aPrincipal, and the third-party iframe and sandboxing status of the window
   * are also checked.
   *
   * Used in the implementation of StorageAllowedForWindow and
   * StorageAllowedForPrincipal.
   */
  static StorageAccess InternalStorageAllowedForPrincipal(nsIPrincipal* aPrincipal,
                                                          nsPIDOMWindow* aWindow);

  static nsIXPConnect *sXPConnect;

  static nsIScriptSecurityManager *sSecurityManager;
  static nsIPrincipal *sSystemPrincipal;
  static nsIPrincipal *sNullSubjectPrincipal;

  static nsIParserService *sParserService;

  static nsNameSpaceManager *sNameSpaceManager;

  static nsIIOService *sIOService;
  static nsIUUIDGenerator *sUUIDGenerator;

  static bool sImgLoaderInitialized;
  static void InitImgLoader();

  // The following four members are initialized lazily
  static imgLoader* sImgLoader;
  static imgLoader* sPrivateImgLoader;
  static imgICache* sImgCache;
  static imgICache* sPrivateImgCache;

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

  static nsIBidiKeyboard* sBidiKeyboard;

  static bool sInitialized;
  static uint32_t sScriptBlockerCount;
  static uint32_t sDOMNodeRemovedSuppressCount;
  static uint32_t sMicroTaskLevel;
  // Not an nsCOMArray because removing elements from those is slower
  static nsTArray< nsCOMPtr<nsIRunnable> >* sBlockedScriptRunners;
  static uint32_t sRunnersCountAtFirstBlocker;
  static uint32_t sScriptBlockerCountWhereRunnersPrevented;

  static nsIInterfaceRequestor* sSameOriginChecker;

  static bool sIsHandlingKeyBoardEvent;
  static bool sAllowXULXBL_for_file;
  static bool sIsFullScreenApiEnabled;
  static bool sTrustedFullScreenOnly;
  static bool sIsCutCopyAllowed;
  static uint32_t sHandlingInputTimeout;
  static bool sIsPerformanceTimingEnabled;
  static bool sIsResourceTimingEnabled;
  static bool sIsUserTimingLoggingEnabled;
  static bool sIsFrameTimingPrefEnabled;
  static bool sIsExperimentalAutocompleteEnabled;
  static bool sEncodeDecodeURLHash;
  static bool sGettersDecodeURLHash;
  static bool sPrivacyResistFingerprinting;
  static bool sSendPerformanceTimingNotifications;
  static uint32_t sCookiesLifetimePolicy;
  static uint32_t sCookiesBehavior;

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

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  static bool sDOMWindowDumpEnabled;
#endif
};

class MOZ_RAII nsAutoScriptBlocker {
public:
  explicit nsAutoScriptBlocker(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    nsContentUtils::AddScriptBlocker();
  }
  ~nsAutoScriptBlocker() {
    nsContentUtils::RemoveScriptBlocker();
  }
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_STACK_CLASS nsAutoScriptBlockerSuppressNodeRemoved :
                          public nsAutoScriptBlocker {
public:
  nsAutoScriptBlockerSuppressNodeRemoved() {
    ++nsContentUtils::sDOMNodeRemovedSuppressCount;
  }
  ~nsAutoScriptBlockerSuppressNodeRemoved() {
    --nsContentUtils::sDOMNodeRemovedSuppressCount;
  }
};

class MOZ_STACK_CLASS nsAutoMicroTask
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

namespace mozilla {
namespace dom {

class TreeOrderComparator {
public:
  bool Equals(nsINode* aElem1, nsINode* aElem2) const {
    return aElem1 == aElem2;
  }
  bool LessThan(nsINode* aElem1, nsINode* aElem2) const {
    return nsContentUtils::PositionIsBefore(aElem1, aElem2);
  }
};

} // namespace dom
} // namespace mozilla

#define NS_INTERFACE_MAP_ENTRY_TEAROFF(_interface, _allocator)                \
  if (aIID.Equals(NS_GET_IID(_interface))) {                                  \
    foundInterface = static_cast<_interface *>(_allocator);                   \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nullptr;                                                \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

/*
 * In the following helper macros we exploit the fact that the result of a
 * series of additions will not be finite if any one of the operands in the
 * series is not finite.
 */
#define NS_ENSURE_FINITE(f, rv)                                               \
  if (!mozilla::IsFinite(f)) {                                                \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE2(f1, f2, rv)                                         \
  if (!mozilla::IsFinite((f1)+(f2))) {                                        \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE4(f1, f2, f3, f4, rv)                                 \
  if (!mozilla::IsFinite((f1)+(f2)+(f3)+(f4))) {                              \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE5(f1, f2, f3, f4, f5, rv)                             \
  if (!mozilla::IsFinite((f1)+(f2)+(f3)+(f4)+(f5))) {                         \
    return (rv);                                                              \
  }

#define NS_ENSURE_FINITE6(f1, f2, f3, f4, f5, f6, rv)                         \
  if (!mozilla::IsFinite((f1)+(f2)+(f3)+(f4)+(f5)+(f6))) {                    \
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

#endif /* nsContentUtils_h___ */
