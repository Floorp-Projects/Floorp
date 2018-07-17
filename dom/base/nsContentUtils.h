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
#include "mozilla/dom/FromParser.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CORSMode.h"
#include "mozilla/EventForwards.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/TimeStamp.h"
#include "nsContentListDeclarations.h"
#include "nsMathUtils.h"
#include "nsTArrayForwardDeclare.h"
#include "Units.h"
#include "mozilla/dom/AutocompleteInfoBinding.h"
#include "mozilla/dom/BindingDeclarations.h" // For CallerType
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/LineBreaker.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Logging.h"
#include "mozilla/NotNull.h"
#include "mozilla/Maybe.h"
#include "mozilla/RangeBoundary.h"
#include "nsIContentPolicy.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "nsRFPService.h"

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
class nsCacheableFuncStringHTMLCollection;
class nsHtml5StringParser;
class nsAtom;
class nsIChannel;
class nsIConsoleService;
class nsIContent;
class nsIContentPolicy;
class nsIContentSecurityPolicy;
class nsIDocShellTreeItem;
class nsIDocumentLoaderFactory;
class nsIDragSession;
class nsIEventTarget;
class nsIFragmentContentSink;
class nsIFrame;
class nsIImageLoadingContent;
class nsIInterfaceRequestor;
class nsIIOService;
class nsILoadInfo;
class nsILoadGroup;
class nsNameSpaceManager;
class nsIObserver;
class nsIParser;
class nsIPluginTag;
class nsIPresShell;
class nsIPrincipal;
class nsIRequest;
class nsIRunnable;
class nsIScriptContext;
class nsIScriptSecurityManager;
class nsIStringBundle;
class nsIStringBundleService;
class nsISupportsHashKey;
class nsIURI;
class nsIUUIDGenerator;
class nsIWidget;
class nsIXPConnect;
class nsNodeInfoManager;
class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;
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
class nsIWindowProvider;

struct JSRuntime;

template<class E> class nsCOMArray;
template<class K, class V> class nsDataHashtable;
template<class K, class V> class nsRefPtrHashtable;
template<class T> class nsReadingIterator;

namespace mozilla {
class Dispatcher;
class ErrorResult;
class EventListenerManager;
class HTMLEditor;

namespace dom {
struct CustomElementDefinition;
class DocumentFragment;
class Element;
class Event;
class EventTarget;
class HTMLInputElement;
class IPCDataTransfer;
class IPCDataTransferItem;
struct LifecycleCallbackArgs;
struct LifecycleAdoptedCallbackArgs;
class MessageBroadcaster;
class NodeInfo;
class nsIContentChild;
class nsIContentParent;
class TabChild;
class Selection;
class TabParent;
} // namespace dom

namespace ipc {
class Shmem;
class IShmemAllocator;
}

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
const nsString& EmptyString();
const nsCString& EmptyCString();

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
  nsAtom* MOZ_NON_OWNING_REF mAtom;
  int32_t  mType;
  mozilla::EventMessage mMessage;
  mozilla::EventClassID mEventClassID;
  // True if mAtom is possibly used by special SVG/SMIL events, but
  // mMessage is eUnidentifiedEvent. See EventNameList.h
  bool mMaybeSpecialSVGorSMILEvent;
};

typedef bool (*CallOnRemoteChildFunction) (mozilla::dom::TabParent* aTabParent,
                                           void* aArg);

class nsContentUtils
{
  friend class nsAutoScriptBlockerSuppressNodeRemoved;
  typedef mozilla::dom::Element Element;
  typedef mozilla::Cancelable Cancelable;
  typedef mozilla::CanBubble CanBubble;
  typedef mozilla::ChromeOnlyDispatch ChromeOnlyDispatch;
  typedef mozilla::EventMessage EventMessage;
  typedef mozilla::TimeDuration TimeDuration;
  typedef mozilla::Trusted Trusted;

public:
  static nsresult Init();

  // Strip off "wyciwyg://n/" part of a URL. aURI must have "wyciwyg" scheme.
  static nsresult RemoveWyciwygScheme(nsIURI* aURI, nsIURI** aReturn);

  static bool     IsCallerChrome();
  static bool     ThreadsafeIsCallerChrome();
  static bool     IsCallerContentXBL();

  // The APIs for checking whether the caller is system (in the sense of system
  // principal) should only be used when the JSContext is known to accurately
  // represent the caller.  In practice, that means you should only use them in
  // two situations at the moment:
  //
  // 1) Functions used in WebIDL Func annotations.
  // 2) Bindings code or other code called directly from the JS engine.
  //
  // Use pretty much anywhere else is almost certainly wrong and should be
  // replaced with [NeedsCallerType] annotations in bindings.

  // Check whether the caller is system if you know you're on the main thread.
  static bool IsSystemCaller(JSContext* aCx);

  // Check whether the caller is system if you might be on a worker or worklet
  // thread.
  static bool ThreadsafeIsSystemCaller(JSContext* aCx);

  // In the traditional Gecko architecture, both C++ code and untrusted JS code
  // needed to rely on the same XPCOM method/getter/setter to get work done.
  // This required lots of security checks in the various exposed methods, which
  // in turn created difficulty in determining whether the caller was script
  // (whose access needed to be checked) and internal C++ platform code (whose
  // access did not need to be checked). To address this problem, Gecko had a
  // convention whereby the absence of script on the stack was interpretted as
  // "System Caller" and always granted unfettered access.
  //
  // Unfortunately, this created a bunch of footguns. For example, when the
  // implementation of a DOM method wanted to perform a privileged
  // sub-operation, it needed to "hide" the presence of script on the stack in
  // order for that sub-operation to be allowed. Additionally, if script could
  // trigger an API entry point to be invoked in some asynchronous way without
  // script on the stack, it could potentially perform privilege escalation.
  //
  // In the modern world, untrusted script should interact with the platform
  // exclusively over WebIDL APIs, and platform code has a lot more flexibility
  // in deciding whether or not to use XPCOM. This gives us the flexibility to
  // do something better.
  //
  // Going forward, APIs should be designed such that any security checks that
  // ask the question "is my caller allowed to do this?" should live in WebIDL
  // API entry points, with a separate method provided for internal callers
  // that just want to get the job done.
  //
  // To enforce this and catch bugs, nsContentUtils::SubjectPrincipal will crash
  // if it is invoked without script on the stack. To land that transition, it
  // was necessary to go through and whitelist a bunch of callers that were
  // depending on the old behavior. Those callers should be fixed up, and these
  // methods should not be used by new code without review from bholley or bz.
  static bool     LegacyIsCallerNativeCode() { return !GetCurrentJSContext(); }
  static bool     LegacyIsCallerChromeOrNativeCode() { return LegacyIsCallerNativeCode() || IsCallerChrome(); }
  static nsIPrincipal* SubjectPrincipalOrSystemIfNativeCaller()
  {
    if (!GetCurrentJSContext()) {
      return GetSystemPrincipal();
    }
    return SubjectPrincipal();
  }

  static bool LookupBindingMember(JSContext* aCx, nsIContent *aContent,
                                  JS::Handle<jsid> aId,
                                  JS::MutableHandle<JS::PropertyDescriptor> aDesc);

  // Check whether we should avoid leaking distinguishing information to JS/CSS.
  // This function can be called both in the main thread and worker threads.
  static bool ShouldResistFingerprinting();
  static bool ShouldResistFingerprinting(nsIDocShell* aDocShell);
  static bool ShouldResistFingerprinting(nsIDocument* aDoc);

  // A helper function to calculate the rounded window size for fingerprinting
  // resistance. The rounded size is based on the chrome UI size and available
  // screen size. If the inputWidth/Height is greater than the available content
  // size, this will report the available content size. Otherwise, it will
  // round the size to the nearest upper 200x100.
  static void CalcRoundedWindowSizeForResistingFingerprinting(int32_t  aChromeWidth,
                                                              int32_t  aChromeHeight,
                                                              int32_t  aScreenWidth,
                                                              int32_t  aScreenHeight,
                                                              int32_t  aInputWidth,
                                                              int32_t  aInputHeight,
                                                              bool     aSetOuterWidth,
                                                              bool     aSetOuterHeight,
                                                              int32_t* aOutputWidth,
                                                              int32_t* aOutputHeight);

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
   * Similar to above, but does special case only ShadowRoot,
   * not HTMLTemplateElement.
   */
  static bool ContentIsShadowIncludingDescendantOf(
    const nsINode* aPossibleDescendant, const nsINode* aPossibleAncestor);

  /**
   * Similar to ContentIsDescendantOf except it crosses document boundaries,
   * this function uses ancestor/descendant relations in the composed document
   * (see shadow DOM spec).
   */
  static bool ContentIsCrossDocDescendantOf(nsINode* aPossibleDescendant,
                                              nsINode* aPossibleAncestor);

  /**
   * As with ContentIsCrossDocDescendantOf but crosses shadow boundaries but not
   * cross document boundaries.
   *
   * @see nsINode::GetFlattenedTreeParentNode()
   */
  static bool
  ContentIsFlattenedTreeDescendantOf(const nsINode* aPossibleDescendant,
                                     const nsINode* aPossibleAncestor);

  /**
   * Same as `ContentIsFlattenedTreeDescendantOf`, but from the flattened tree
   * point of view of the style system
   *
   * @see nsINode::GetFlattenedTreeParentNodeForStyle()
   */
  static bool
  ContentIsFlattenedTreeDescendantOfForStyle(const nsINode* aPossibleDescendant,
                                             const nsINode* aPossibleAncestor);

  /**
   * Retarget an object A against an object B
   * @see https://dom.spec.whatwg.org/#retarget
   */
  static nsINode*
  Retarget(nsINode* aTargetA, nsINode* aTargetB);

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
  static nsresult GetAncestorsAndOffsets(nsINode* aNode,
                                         int32_t aOffset,
                                         nsTArray<nsIContent*>* aAncestorNodes,
                                         nsTArray<int32_t>* aAncestorOffsets);

  /**
   * Returns the common ancestor, if any, for two nodes.
   *
   * Returns null if the nodes are disconnected.
   */
  static nsINode* GetCommonAncestor(nsINode* aNode1, nsINode* aNode2)
  {
    if (aNode1 == aNode2) {
      return aNode1;
    }

    return GetCommonAncestorHelper(aNode1, aNode2);
  }

  /**
   * Returns the common flattened tree ancestor, if any, for two given content
   * nodes.
   */
  static nsIContent* GetCommonFlattenedTreeAncestor(nsIContent* aContent1,
                                                    nsIContent* aContent2)
  {
    if (aContent1 == aContent2) {
      return aContent1;
    }

    return GetCommonFlattenedTreeAncestorHelper(aContent1, aContent2);
  }

  /**
   * Returns the common flattened tree ancestor from the point of view of the
   * style system, if any, for two given content nodes.
   */
  static Element* GetCommonFlattenedTreeAncestorForStyle(
      Element* aElement1, Element* aElement2);

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
   *
   *  XXX aOffset1 and aOffset2 should be uint32_t since valid offset value is
   *      between 0 - UINT32_MAX.  However, these methods work even with
   *      negative offset values!  E.g., when aOffset1 is -1 and aOffset is 0,
   *      these methods return -1.  Some root callers depend on this behavior.
   *      On the other hand, nsINode can have ATTRCHILD_ARRAY_MAX_CHILD_COUN
   *      (0x3FFFFF) at most.  Therefore, they can be int32_t for now.
   */
  static int32_t ComparePoints(nsINode* aParent1, int32_t aOffset1,
                               nsINode* aParent2, int32_t aOffset2,
                               bool* aDisconnected = nullptr);
  static int32_t ComparePoints(const mozilla::RawRangeBoundary& aFirst,
                               const mozilla::RawRangeBoundary& aSecond,
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
  static Element* MatchElementId(nsIContent *aContent, const nsAtom* aId);

  /**
   * Reverses the document position flags passed in.
   *
   * @param   aDocumentPosition   The document position flags to be reversed.
   *
   * @return  The reversed document position flags.
   *
   * @see Node
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
    eParseHTMLInteger_ErrorOverflow         = 1 << 5,
    // Use this flag to detect the difference between overflow and underflow
    eParseHTMLInteger_Negative              = 1 << 6,
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
                                  const nsINode* unTrustedNode);

  // Check if the (JS) caller can access aNode.
  static bool CanCallerAccess(nsINode* aNode);

  // Check if the (JS) caller can access aWindow.
  // aWindow can be either outer or inner window.
  static bool CanCallerAccess(nsPIDOMWindowInner* aWindow);

  // Check if the principal is chrome or an addon with the permission.
  static bool PrincipalHasPermission(nsIPrincipal* aPrincipal, const nsAtom* aPerm);

  // Check if the JS caller is chrome or an addon with the permission.
  static bool CallerHasPermission(JSContext* aCx, const nsAtom* aPerm);

  /**
   * Returns the triggering principal which should be used for the given URL
   * attribute value with the given subject principal.
   *
   * If the attribute value is not an absolute URL, the subject principal will
   * be ignored, and the node principal of aContent will be used instead.
   * If aContent is non-null, this function will always return a principal.
   * Otherewise, it may return null if aSubjectPrincipal is null or is rejected
   * based on the attribute value.
   *
   * @param aContent The content on which the attribute is being set.
   * @param aAttrValue The URL value of the attribute. For parsed attribute
   *        values, such as `srcset`, this function should be called separately
   *        for each URL value it contains.
   * @param aSubjectPrincipal The subject principal of the scripted caller
   *        responsible for setting the attribute, or null if no scripted caller
   *        can be determined.
   */
  static nsIPrincipal* GetAttrTriggeringPrincipal(nsIContent* aContent,
                                                  const nsAString& aAttrValue,
                                                  nsIPrincipal* aSubjectPrincipal);

  /**
   * Returns true if the given string is guaranteed to be treated as an absolute
   * URL, rather than a relative URL. In practice, this means any complete URL
   * as supported by nsStandardURL, or any string beginning with a valid scheme
   * which is known to the IO service, and has the URI_NORELATIVE flag.
   *
   * If the URL may be treated as absolute in some cases, but relative in others
   * (for instance, "http:foo", which can be either an absolute or relative URL,
   * depending on the context), this function returns false.
   */
  static bool IsAbsoluteURL(const nsACString& aURL);

  // Check if a node is in the document prolog, i.e. before the document
  // element.
  static bool InProlog(nsINode *aNode);

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

  // Returns the subject principal from the JSContext. May only be called
  // from the main thread and assumes an existing compartment.
  static nsIPrincipal* SubjectPrincipal(JSContext* aCx);

  // Returns the subject principal. Guaranteed to return non-null. May only
  // be called when nsContentUtils is initialized.
  static nsIPrincipal* SubjectPrincipal();

  // Returns the prinipal of the given JS object. This may only be called on
  // the main thread for objects from the main thread's JSRuntime.
  static nsIPrincipal* ObjectPrincipal(JSObject* aObj);

  static nsresult GenerateStateKey(nsIContent* aContent,
                                   nsIDocument* aDocument,
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
   * Returns true if |aName| is a name with dashes.
   */
  static bool IsNameWithDash(nsAtom* aName);

  /**
   * Returns true if |aName| is a valid name to be registered via
   * customElements.define.
   */
  static bool IsCustomElementName(nsAtom* aName, uint32_t aNameSpaceID);

  static nsresult CheckQName(const nsAString& aQualifiedName,
                             bool aNamespaceAware = true,
                             const char16_t** aColon = nullptr);

  static nsresult SplitQName(const nsIContent* aNamespaceResolver,
                             const nsString& aQName,
                             int32_t *aNamespace, nsAtom **aLocalName);

  static nsresult GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                       const nsAString& aQualifiedName,
                                       nsNodeInfoManager* aNodeInfoManager,
                                       uint16_t aNodeType,
                                       mozilla::dom::NodeInfo** aNodeInfo);

  static void SplitExpatName(const char16_t *aExpatName, nsAtom **aPrefix,
                             nsAtom **aTagName, int32_t *aNameSpaceID);

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

  static mozilla::intl::LineBreaker* LineBreaker()
  {
    return sLineBreaker.get();
  }

  static mozilla::intl::WordBreaker* WordBreaker()
  {
    return sWordBreaker.get();
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
                                nsAtom* aName);

  /**
   * Method that gets the primary presContext for the node.
   *
   * @param aContent The content node.
   * @return the presContext, or nullptr if the content is not in a document
   *         (if GetComposedDoc returns nullptr)
   */
  static nsPresContext* GetContextForContent(const nsIContent* aContent);


  /**
   * Method that gets the pres shell for the node.
   *
   * @param aContent The content node.
   * @return the pres shell, or nullptr if the content is not in a document
   *         (if GetComposedDoc returns nullptr)
   */
  static nsIPresShell* GetPresShellForContent(const nsIContent* aContent);

  /**
   * Method to do security and content policy checks on the image URI
   *
   * @param aURI uri of the image to be loaded
   * @param aNode, the context the image is loaded in (eg an element)
   * @param aLoadingDocument the document we belong to
   * @param aLoadingPrincipal the principal doing the load
   * @param [aContentPolicyType=nsIContentPolicy::TYPE_INTERNAL_IMAGE] (Optional)
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
                           nsINode* aNode,
                           nsIDocument* aLoadingDocument,
                           nsIPrincipal* aLoadingPrincipal);

  /**
   * Returns true if objects in aDocument shouldn't initiate image loads.
   */
  static bool DocumentInactiveForImageLoads(nsIDocument* aDocument);

  /**
   * Convert a CORSMode into the corresponding imgILoader flags for
   * passing to LoadImage.
   * @param aMode CORS mode to convert
   * @return a bitfield suitable to bitwise OR with other nsIRequest flags
   */
  static int32_t CORSModeToLoadImageFlags(mozilla::CORSMode aMode);

  /**
   * Method to start an image load.  This does not do any security checks.
   * This method will attempt to make aURI immutable; a caller that wants to
   * keep a mutable version around should pass in a clone.
   *
   * @param aURI uri of the image to be loaded
   * @param aContext element of document where the result of this request
   *                 will be used.
   * @param aLoadingDocument the document we belong to
   * @param aLoadingPrincipal the principal doing the load
   * @param aReferrer the referrer URI
   * @param aReferrerPolicy the referrer-sending policy to use on channel
   *         creation
   * @param aObserver the observer for the image load
   * @param aLoadFlags the load flags to use.  See nsIRequest
   * @param [aContentPolicyType=nsIContentPolicy::TYPE_INTERNAL_IMAGE] (Optional)
   *        The CP content type to use
   * @param aUseUrgentStartForChannel,(Optional) a flag to mark on channel if it
   *        is triggered by user input events.
   * @return the imgIRequest for the image load
   */
  static nsresult LoadImage(nsIURI* aURI,
                            nsINode* aContext,
                            nsIDocument* aLoadingDocument,
                            nsIPrincipal* aLoadingPrincipal,
                            uint64_t aRequestContextID,
                            nsIURI* aReferrer,
                            mozilla::net::ReferrerPolicy aReferrerPolicy,
                            imgINotificationObserver* aObserver,
                            int32_t aLoadFlags,
                            const nsAString& initiatorType,
                            imgRequestProxy** aRequest,
                            uint32_t aContentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE,
                            bool aUseUrgentStartForChannel = false);

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
  static already_AddRefed<imgRequestProxy> GetStaticRequest(nsIDocument* aLoadingDocument,
                                                            imgRequestProxy* aRequest);

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
   * Convenience method to create a new nodeinfo that differs only by prefix and
   * name from aNodeInfo. The new nodeinfo's name is set to aName, and prefix is
   * set to null.
   */
  static nsresult QNameChanged(mozilla::dom::NodeInfo* aNodeInfo,
                               nsAtom* aName,
                               mozilla::dom::NodeInfo** aResult);

  /**
   * Returns the appropriate event argument names for the specified
   * namespace and event name.  Added because we need to switch between
   * SVG's "evt" and the rest of the world's "event", and because onerror
   * on window takes 5 args.
   */
  static void GetEventArgNames(int32_t aNameSpaceID, nsAtom *aEventName,
                               bool aIsForWindow,
                               uint32_t *aArgCount, const char*** aArgNames);

  /**
   * Returns origin attributes of the document.
   **/
  static mozilla::OriginAttributes
  GetOriginAttributes(nsIDocument* aDoc);

  /**
   * Returns origin attributes of the load group.
   **/
  static mozilla::OriginAttributes
  GetOriginAttributes(nsILoadGroup* aLoadGroup);

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
                                    const char * classification,
                                    bool aFromPrivateWindow);

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
   *   @param [aLocationMode] (Optional) Specifies the behavior if
              error location information is omitted.
   */
  enum MissingErrorLocationMode {
    // Don't show location information in the error console.
    eOMIT_LOCATION,
    // Get location information from the currently executing script.
    eUSE_CALLING_LOCATION
  };
  static nsresult ReportToConsoleNonLocalized(const nsAString& aErrorText,
                                              uint32_t aErrorFlags,
                                              const nsACString& aCategory,
                                              const nsIDocument* aDocument,
                                              nsIURI* aURI = nullptr,
                                              const nsString& aSourceLine
                                                = EmptyString(),
                                              uint32_t aLineNumber = 0,
                                              uint32_t aColumnNumber = 0,
                                              MissingErrorLocationMode aLocationMode
                                                = eUSE_CALLING_LOCATION);

  /**
   * Report a non-localized error message to the error console base on the
   * innerWindowID.
   *   @param aErrorText the error message
   *   @param aErrorFlags See nsIScriptError.
   *   @param aCategory Name of module reporting error.
   *   @param [aInnerWindowID] Inner window ID for document which triggered the
   *          message.
   *   @param [aURI=nullptr] (Optional) URI of resource containing error.
   *   @param [aSourceLine=EmptyString()] (Optional) The text of the line that
              contains the error (may be empty).
   *   @param [aLineNumber=0] (Optional) Line number within resource
              containing error.
   *   @param [aColumnNumber=0] (Optional) Column number within resource
              containing error.
              If aURI is null, then aDocument->GetDocumentURI() is used.
   *   @param [aLocationMode] (Optional) Specifies the behavior if
              error location information is omitted.
   */
  static nsresult ReportToConsoleByWindowID(const nsAString& aErrorText,
                                            uint32_t aErrorFlags,
                                            const nsACString& aCategory,
                                            uint64_t aInnerWindowID,
                                            nsIURI* aURI = nullptr,
                                            const nsString& aSourceLine
                                              = EmptyString(),
                                            uint32_t aLineNumber = 0,
                                            uint32_t aColumnNumber = 0,
                                            MissingErrorLocationMode aLocationMode
                                              = eUSE_CALLING_LOCATION);

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
                                  const nsString& aSourceLine
                                    = EmptyString(),
                                  uint32_t aLineNumber = 0,
                                  uint32_t aColumnNumber = 0);

  static void ReportEmptyGetElementByIdArg(const nsIDocument* aDoc);

  static void LogMessageToConsole(const char* aMsg);

  /**
   * Get the localized string named |aKey| in properties file |aFile|.
   */
  static nsresult GetLocalizedString(PropertiesFile aFile,
                                     const char* aKey,
                                     nsAString& aResult);

  /**
   * A helper function that parses a sandbox attribute (of an <iframe> or a CSP
   * directive) and converts it to the set of flags used internally.
   *
   * @param aSandboxAttr  the sandbox attribute
   * @return              the set of flags (SANDBOXED_NONE if aSandboxAttr is
   *                      null)
   */
  static uint32_t ParseSandboxAttributeToFlags(const nsAttrValue* aSandboxAttr);

  /**
   * A helper function that checks if a string matches a valid sandbox flag.
   *
   * @param aFlag   the potential sandbox flag.
   * @return        true if the flag is a sandbox flag.
   */
  static bool IsValidSandboxFlag(const nsAString& aFlag);

  /**
   * A helper function that returns a string attribute corresponding to the
   * sandbox flags.
   *
   * @param aFlags    the sandbox flags
   * @param aString   the attribute corresponding to the flags (null if aFlags
   *                  is zero)
   */
  static void SandboxFlagsToString(uint32_t aFlags, nsAString& aString);

  /**
   * Helper function that generates a UUID.
   */
  static nsresult GenerateUUIDInPlace(nsID& aUUID);

  static bool PrefetchPreloadEnabled(nsIDocShell* aDocShell);

  static void
  ExtractErrorValues(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     nsAString& aSourceSpecOut, uint32_t *aLineOut,
                     uint32_t *aColumnOut, nsString& aMessageOut);

  // Variant on `ExtractErrorValues` with a `nsACString`. This
  // method is provided for backwards compatibility. Prefer the
  // faster method above for your code.
  static void
  ExtractErrorValues(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     nsACString& aSourceSpecOut, uint32_t *aLineOut,
                     uint32_t *aColumnOut, nsString& aMessageOut);

  /**
   * Helper function to tell if user ever enabled DevTools explicitely.
   * Allows making DevTools related API no-op until user do so.
   */
  static bool DevToolsEnabled(JSContext* aCx);

  static nsresult CalculateBufferSizeForImage(const uint32_t& aStride,
                                            const mozilla::gfx::IntSize& aImageSize,
                                            const mozilla::gfx::SurfaceFormat& aFormat,
                                            size_t* aMaxBufferSize,
                                            size_t* aUsedBufferSize);

private:
  /**
   * Fill (with the parameters given) the localized string named |aKey| in
   * properties file |aFile|.
   */
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const char16_t** aParams,
                                        uint32_t aParamsLength,
                                        nsAString& aResult);

public:
  template<uint32_t N>
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const char16_t* (&aParams)[N],
                                        nsAString& aResult)
  {
    return FormatLocalizedString(aFile, aKey, aParams, N, aResult);
  }

  /**
   * Fill (with the parameters given) the localized string named |aKey| in
   * properties file |aFile| consuming an nsTArray of nsString parameters rather
   * than a char16_t** for the sake of avoiding use-after-free errors involving
   * temporaries.
   */
  static nsresult FormatLocalizedString(PropertiesFile aFile,
                                        const char* aKey,
                                        const nsTArray<nsString>& aParamArray,
                                        nsAString& aResult);

  /**
   * Returns true if aDocument is a chrome document
   */
  static bool IsChromeDoc(nsIDocument *aDocument);

  /**
   * Returns true if aDocument is in a docshell whose parent is the same type
   */
  static bool IsChildOfSameType(nsIDocument* aDoc);

  /**
   * Returns true if the content-type will be rendered as plain-text.
   */
  static bool IsPlainTextType(const nsACString& aContentType);

  /**
   * Returns true iff the type is rendered as plain text and doesn't support
   * non-UTF-8 encodings.
   */
  static bool IsUtf8OnlyPlainTextType(const nsACString& aContentType);

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
                                             nsACString& aScriptURI,
                                             nsresult* aRv);


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
  static inline nsContentPolicyType
    InternalContentPolicyTypeToExternal(nsContentPolicyType aType);

  /**
   * Map internal content policy types to external ones or preload types:
   *   * TYPE_INTERNAL_SCRIPT_PRELOAD
   *   * TYPE_INTERNAL_IMAGE_PRELOAD
   *   * TYPE_INTERNAL_STYLESHEET_PRELOAD
   *
   * Note: DO NOT call this function unless you know what you're doing!
   */
  static inline nsContentPolicyType
    InternalContentPolicyTypeToExternalOrPreload(nsContentPolicyType aType);

  /**
   * Map internal content policy types to external ones, worker, or preload types:
   *   * TYPE_INTERNAL_WORKER
   *   * TYPE_INTERNAL_SHARED_WORKER
   *   * TYPE_INTERNAL_SERVICE_WORKER
   *
   * Note: DO NOT call this function unless you know what you're doing!
   */
  static nsContentPolicyType InternalContentPolicyTypeToExternalOrWorker(nsContentPolicyType aType);

  /**
   * Returns true if the content policy type is any of:
   *   * TYPE_INTERNAL_SCRIPT_PRELOAD
   *   * TYPE_INTERNAL_IMAGE_PRELOAD
   *   * TYPE_INTERNAL_STYLESHEET_PRELOAD
   */
  static bool IsPreloadType(nsContentPolicyType aType);

  /**
   * Returns true if the pref "security.mixed_content.upgrade_display_content" is true
   * and the content policy type is any of:
   *   * TYPE_IMAGE
   *   * TYPE_MEDIA
   */
  static bool IsUpgradableDisplayType(nsContentPolicyType aType);

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
   */
  static void MaybeFireNodeRemoved(nsINode* aChild, nsINode* aParent);

  /**
   * This method creates and dispatches a trusted event.
   * Works only with events which can be created by calling
   * nsIDocument::CreateEvent() with parameter "Events".
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       EventTarget.
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see EventTarget::DispatchEvent.
   */
  static nsresult DispatchTrustedEvent(nsIDocument* aDoc,
                                       nsISupports* aTarget,
                                       const nsAString& aEventName,
                                       CanBubble,
                                       Cancelable,
                                       bool* aDefaultAction = nullptr);

  /**
   * This method creates and dispatches a trusted event using an event message.
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       EventTarget.
   * @param aEventMessage  The event message.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see EventTarget::DispatchEvent.
   */
  template <class WidgetEventType>
  static nsresult DispatchTrustedEvent(
    nsIDocument* aDoc,
    nsISupports* aTarget,
    EventMessage aEventMessage,
    CanBubble aCanBubble,
    Cancelable aCancelable,
    bool* aDefaultAction = nullptr,
    ChromeOnlyDispatch aOnlyChromeDispatch = ChromeOnlyDispatch::eNo)
  {
    WidgetEventType event(true, aEventMessage);
    MOZ_ASSERT(GetEventClassIDFromMessage(aEventMessage) == event.mClass);
    return DispatchEvent(aDoc, aTarget, event, aEventMessage,
                         aCanBubble, aCancelable, Trusted::eYes,
                         aDefaultAction, aOnlyChromeDispatch);
  }

  /**
   * This method creates and dispatches a untrusted event.
   * Works only with events which can be created by calling
   * nsIDocument::CreateEvent() with parameter "Events".
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       EventTarget.
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see EventTarget::DispatchEvent.
   */
  static nsresult DispatchUntrustedEvent(nsIDocument* aDoc,
                                         nsISupports* aTarget,
                                         const nsAString& aEventName,
                                         CanBubble,
                                         Cancelable,
                                         bool* aDefaultAction = nullptr);


  /**
   * This method creates and dispatches a untrusted event using an event message.
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       EventTarget.
   * @param aEventMessage  The event message.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see EventTarget::DispatchEvent.
   */
  template <class WidgetEventType>
  static nsresult DispatchUntrustedEvent(
    nsIDocument* aDoc,
    nsISupports* aTarget,
    EventMessage aEventMessage,
    CanBubble aCanBubble,
    Cancelable aCancelable,
    bool* aDefaultAction = nullptr,
    ChromeOnlyDispatch aOnlyChromeDispatch = ChromeOnlyDispatch::eNo)
  {
    WidgetEventType event(false, aEventMessage);
    MOZ_ASSERT(GetEventClassIDFromMessage(aEventMessage) == event.mClass);
    return DispatchEvent(aDoc, aTarget, event, aEventMessage,
                         aCanBubble, aCancelable, Trusted::eNo,
                         aDefaultAction, aOnlyChromeDispatch);
  }

  /**
   * This method creates and dispatches a trusted event to the chrome
   * event handler (the parent object of the DOM Window in the event target
   * chain). Note, chrome event handler is used even if aTarget is a chrome
   * object. Use DispatchEventOnlyToChrome if the normal event dispatching is
   * wanted in case aTarget is a chrome object.
   * Works only with events which can be created by calling
   * nsIDocument::CreateEvent() with parameter "Events".
   * @param aDocument      The document which will be used to create the event,
   *                       and whose window's chrome handler will be used to
   *                       dispatch the event.
   * @param aTarget        The target of the event, used for event->SetTarget()
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see EventTarget::DispatchEvent.
   */
  static nsresult DispatchChromeEvent(nsIDocument* aDoc,
                                      nsISupports* aTarget,
                                      const nsAString& aEventName,
                                      CanBubble,
                                      Cancelable,
                                      bool* aDefaultAction = nullptr);

  /**
   * Helper function for dispatching a "DOMWindowFocus" event to
   * the chrome event handler of the given DOM Window. This has the effect
   * of focusing the corresponding tab and bringing the browser window
   * to the foreground.
   */
  static nsresult DispatchFocusChromeEvent(nsPIDOMWindowOuter* aWindow);

  /**
   * This method creates and dispatches a trusted event.
   * If aTarget is not a chrome object, the nearest chrome object in the
   * propagation path will be used as the start of the event target chain.
   * This method is different than DispatchChromeEvent, which always dispatches
   * events to chrome event handler. DispatchEventOnlyToChrome works like
   * DispatchTrustedEvent in the case aTarget is a chrome object.
   * Works only with events which can be created by calling
   * nsIDocument::CreateEvent() with parameter "Events".
   * @param aDoc           The document which will be used to create the event.
   * @param aTarget        The target of the event, should be QIable to
   *                       EventTarget.
   * @param aEventName     The name of the event.
   * @param aCanBubble     Whether the event can bubble.
   * @param aCancelable    Is the event cancelable.
   * @param aDefaultAction Set to true if default action should be taken,
   *                       see EventTarget::DispatchEvent.
   */
  static nsresult DispatchEventOnlyToChrome(nsIDocument* aDoc,
                                            nsISupports* aTarget,
                                            const nsAString& aEventName,
                                            CanBubble,
                                            Cancelable,
                                            bool* aDefaultAction = nullptr);

  /**
   * Determines if an event attribute name (such as onclick) is valid for
   * a given element type. Types are from the EventNameType enumeration
   * defined above.
   *
   * @param aName the event name to look up
   * @param aType the type of content
   */
  static bool IsEventAttributeName(nsAtom* aName, int32_t aType);

  /**
   * Return the event message for the event with the given name. The name is
   * the event name with the 'on' prefix. Returns eUnidentifiedEvent if the
   * event doesn't match a known event name.
   *
   * @param aName the event name to look up
   */
  static EventMessage GetEventMessage(nsAtom* aName);

  /**
   * Returns the EventMessage and nsAtom to be used for event listener
   * registration.
   */
  static EventMessage
  GetEventMessageAndAtomForListener(const nsAString& aName, nsAtom** aOnName);

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
  static nsAtom* GetEventMessageAndAtom(const nsAString& aName,
                                        mozilla::EventClassID aEventClassID,
                                        EventMessage* aEventMessage);

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
  static bool IsValidNodeName(nsAtom *aLocalName, nsAtom *aPrefix,
                                int32_t aNamespaceID);

  /**
   * Creates a DocumentFragment from text using a context node to resolve
   * namespaces.
   *
   * Please note that for safety reasons, if the node principal of
   * aContextNode is the system principal, this function will automatically
   * sanitize its input using nsTreeSanitizer.
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
  static already_AddRefed<mozilla::dom::DocumentFragment>
  CreateContextualFragment(nsINode* aContextNode, const nsAString& aFragment,
                           bool aPreventScriptExecution,
                           mozilla::ErrorResult& aRv);

  /**
   * Invoke the fragment parsing algorithm (innerHTML) using the HTML parser.
   *
   * Please note that for safety reasons, if the node principal of aTargetNode
   * is the system principal, this function will automatically sanitize its
   * input using nsTreeSanitizer.
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
                                    nsAtom* aContextLocalName,
                                    int32_t aContextNamespace,
                                    bool aQuirks,
                                    bool aPreventScriptExecution);

  /**
   * Invoke the fragment parsing algorithm (innerHTML) using the XML parser.
   *
   * Please note that for safety reasons, if the node principal of aDocument
   * is the system principal, this function will automatically sanitize its
   * input using nsTreeSanitizer.
   *
   * @param aSourceBuffer the string being set as innerHTML
   * @param aDocument the target document
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
                                   mozilla::dom::DocumentFragment** aReturn);

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
  MOZ_MUST_USE
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

  /*
   * Notify when the first XUL menu is opened and when the all XUL menus are
   * closed. At opening, aInstalling should be TRUE, otherwise, it should be
   * FALSE.
   */
  static void NotifyInstalledMenuKeyboardListener(bool aInstalling);

  /**
   * Check whether the nsIURI uses the given scheme.
   *
   * Note that this will check the innermost URI rather than that of
   * the nsIURI itself.
   */
  static bool SchemeIs(nsIURI* aURI, const char* aScheme);

  /**
   * Returns true if aPrincipal is the system principal.
   */
  static bool IsSystemPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Returns true if aPrincipal is an ExpandedPrincipal.
   */
  static bool IsExpandedPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Returns true if aPrincipal is the system or an ExpandedPrincipal.
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
   * @param aIsTrusted If false, JS Context will be pushed to stack
   *                   when the link is triggered.
   */
  static void TriggerLink(nsIContent *aContent, nsPresContext *aPresContext,
                          nsIURI *aLinkURI, const nsString& aTargetSpec,
                          bool aClick, bool aIsTrusted);

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
  static bool MaybeAllowOfflineAppByDefault(nsIPrincipal *aPrincipal);

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
  static void AddScriptRunner(already_AddRefed<nsIRunnable> aRunnable);
  static void AddScriptRunner(nsIRunnable* aRunnable);

  /**
   * Returns true if it's safe to execute content script and false otherwise.
   *
   * The only known case where this lies is mutation events. They run, and can
   * run anything else, when this function returns false, but this is ok.
   */
  static bool IsSafeToRunScript() {
    MOZ_ASSERT(NS_IsMainThread(),
               "This static variable only makes sense on the main thread!");
    return sScriptBlockerCount == 0;
  }

  // XXXcatalinb: workaround for weird include error when trying to reference
  // ipdl types in WindowWatcher.
  static nsIWindowProvider*
  GetWindowProviderForContentProcess();

  // Returns the browser window with the most recent time stamp that is
  // not in private browsing mode.
  static already_AddRefed<nsPIDOMWindowOuter>
  GetMostRecentNonPBWindow();

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

  /* Add a pending IDBTransaction to be cleaned up at the end of performing a
   * microtask checkpoint.
   * See the step of "Cleanup Indexed Database Transactions" in
   * https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint
   */
  static void AddPendingIDBTransaction(already_AddRefed<nsIRunnable> aTransaction);

  /**
   * Returns true if we are doing StableState/MetastableState.
   */
  static bool IsInStableOrMetaStableState();

  /* Process viewport META data. This gives us information for the scale
   * and zoom of a page on mobile devices. We stick the information in
   * the document header and use it later on after rendering.
   *
   * See Bug #436083
   */
  static nsresult ProcessViewportInfo(nsIDocument *aDocument,
                                      const nsAString &viewportInfo);

  static JSContext *GetCurrentJSContext();

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
  static void ASCIIToLower(nsACString& aStr);
  static void ASCIIToLower(const nsAString& aSource, nsAString& aDest);
  static void ASCIIToLower(const nsACString& aSource, nsACString& aDest);

  /**
   * Convert ASCII a-z to A-Z.
   */
  static void ASCIIToUpper(nsAString& aStr);
  static void ASCIIToUpper(nsACString& aStr);
  static void ASCIIToUpper(const nsAString& aSource, nsAString& aDest);
  static void ASCIIToUpper(const nsACString& aSource, nsACString& aDest);

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
   * XULCommandEvent.
   * If aShell is not null, dispatching goes via
   * nsIPresShell::HandleDOMEventWithTarget.
   */
  static nsresult DispatchXULCommand(nsIContent* aTarget,
                                     bool aTrusted,
                                     mozilla::dom::Event* aSourceEvent = nullptr,
                                     nsIPresShell* aShell = nullptr,
                                     bool aCtrl = false,
                                     bool aAlt = false,
                                     bool aShift = false,
                                     bool aMeta = false,
                                     // Including MouseEventBinding here leads
                                     // to incude loops, unfortunately.
                                     uint16_t inputSource = 0 /* MouseEvent_Binding::MOZ_SOURCE_UNKNOWN */);

  static bool CheckMayLoad(nsIPrincipal* aPrincipal, nsIChannel* aChannel, bool aAllowIfInheritsPrincipal);

  /**
   * The method checks whether the caller can access native anonymous content.
   * If there is no JS in the stack or privileged JS is running, this
   * method returns true, otherwise false.
   */
  static bool CanAccessNativeAnon();

  MOZ_MUST_USE
  static nsresult WrapNative(JSContext *cx, nsISupports *native,
                             const nsIID* aIID, JS::MutableHandle<JS::Value> vp,
                             bool aAllowWrapping = true)
  {
    return WrapNative(cx, native, nullptr, aIID, vp, aAllowWrapping);
  }

  // Same as the WrapNative above, but use this one if aIID is nsISupports' IID.
  MOZ_MUST_USE
  static nsresult WrapNative(JSContext *cx, nsISupports *native,
                             JS::MutableHandle<JS::Value> vp,
                             bool aAllowWrapping = true)
  {
    return WrapNative(cx, native, nullptr, nullptr, vp, aAllowWrapping);
  }

  MOZ_MUST_USE
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
  MOZ_MUST_USE
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
    MOZ_ASSERT(aRootNode, "Must have root node");

    return GetFuncStringContentList<nsCacheableFuncStringHTMLCollection>(aRootNode,
                                                                         MatchClassNames,
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
   *
   * You should probably use WidgetForContent() instead of this, unless you have
   * a good reason to do otherwise.
   */
  static nsIWidget* WidgetForDocument(const nsIDocument* aDoc);

  /**
   * Returns the appropriate widget for this element, if there is one. Unlike
   * WidgetForDocument(), this returns the correct widget for content in popups.
   *
   * You should probably use this instead of WidgetForDocument().
   */
  static nsIWidget* WidgetForContent(const nsIContent* aContent);

  /**
   * Returns a layer manager to use for the given document. Basically we
   * look up the document hierarchy for the first document which has
   * a presentation with an associated widget, and use that widget's
   * layer manager.
   *
   * You should probably use LayerManagerForContent() instead of this, unless
   * you have a good reason to do otherwise.
   *
   * @param aDoc the document for which to return a layer manager.
   * @param aAllowRetaining an outparam that states whether the returned
   * layer manager should be used for retained layers
   */
  static already_AddRefed<mozilla::layers::LayerManager>
  LayerManagerForDocument(const nsIDocument *aDoc);

  /**
   * Returns a layer manager to use for the given content. Unlike
   * LayerManagerForDocument(), this returns the correct layer manager for
   * content in popups.
   *
   * You should probably use this instead of LayerManagerForDocument().
   */
  static already_AddRefed<mozilla::layers::LayerManager>
  LayerManagerForContent(const nsIContent *aContent);

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
  PersistentLayerManagerForDocument(nsIDocument *aDoc);

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
   * Returns true if the unprefixed fullscreen API is enabled.
   */
  static bool IsUnprefixedFullscreenApiEnabled()
    { return sIsUnprefixedFullscreenApiEnabled; }

  /**
   * Returns true if requests for full-screen are allowed in the current
   * context. Requests are only allowed if the user initiated them (like with
   * a mouse-click or key press), unless this check has been disabled by
   * setting the pref "full-screen-api.allow-trusted-requests-only" to false.
   */
  static bool IsRequestFullScreenAllowed(mozilla::dom::CallerType aCallerType);

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
   * allowed for the given subject principal. These are only allowed if the user
   * initiated them (like with a mouse-click or key press).
   */
  static bool IsCutCopyAllowed(nsIPrincipal* aSubjectPrincipal);

  /*
   * Returns true if the performance timing APIs are enabled.
   */
  static bool IsPerformanceTimingEnabled()
  {
    return sIsPerformanceTimingEnabled;
  }

  /*
   * Returns true if the performance timing APIs are enabled.
   */
  static bool IsResourceTimingEnabled()
  {
    return sIsResourceTimingEnabled;
  }

  /*
   * Returns true if the performance timing APIs are enabled.
   */
  static bool IsPerformanceNavigationTimingEnabled()
  {
    return sIsPerformanceNavigationTimingEnabled;
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
   * Returns true if the browser should attempt to prevent the given caller type
   * from collecting distinctive information about the browser that could
   * be used to "fingerprint" and track the user across websites.
   */
  static bool ResistFingerprinting(mozilla::dom::CallerType aCallerType)
  {
    return aCallerType != mozilla::dom::CallerType::System &&
           ShouldResistFingerprinting();
  }

  /**
   * Returns true if the browser should show busy cursor when loading page.
   */
  static bool UseActivityCursor()
  {
    return sUseActivityCursor;
  }

  /**
   * Returns true if the DOM Animations API should be enabled.
   */
  static bool AnimationsAPICoreEnabled()
  {
    return sAnimationsAPICoreEnabled;
  }

  /**
   * Returns true if the getBoxQuads API should be enabled.
   */
  static bool GetBoxQuadsEnabled()
  {
    return sGetBoxQuadsEnabled;
  }

  /**
   * Returns true if the requestIdleCallback API should be enabled.
   */
  static bool RequestIdleCallbackEnabled()
  {
    return sRequestIdleCallbackEnabled;
  }

  /**
   * Returns true if CSSOM origin check should be skipped for WebDriver
   * based crawl to be able to collect data from cross-origin CSS style
   * sheets. This can be enabled by setting environment variable
   * MOZ_BYPASS_CSSOM_ORIGIN_CHECK.
   */
  static bool BypassCSSOMOriginCheck()
  {
#ifdef RELEASE_OR_BETA
    return false;
#else
    return sBypassCSSOMOriginCheck;
#endif
  }

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
  static bool IsInPointerLockContext(nsPIDOMWindowOuter* aWin);

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
  static void FlushLayoutForTree(nsPIDOMWindowOuter* aWindow);

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
  static bool IsAutocompleteEnabled(mozilla::dom::HTMLInputElement* aInput);

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
                                   eAutocompleteAttrState_Unknown,
                                 bool aGrantAllValidValue = false);

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
  static bool GetPseudoAttributeValue(const nsString& aSource, nsAtom *aName,
                                      nsAString& aValue);

  /**
   * Returns true if the language name is a version of JavaScript and
   * false otherwise
   */
  static bool IsJavaScriptLanguage(const nsString& aName);

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
                                        uint32_t& aOutStartOffset,
                                        uint32_t& aOutEndOffset);

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

  /**
   * Returns pointer to HTML editor instance for the aPresContext when there is.
   * The HTML editor is shared by contenteditable elements or used in
   * designMode.  When there are no contenteditable elements and the document
   * is not in designMode, this returns nullptr.
   */
  static mozilla::HTMLEditor* GetHTMLEditor(nsPresContext* aPresContext);

  /**
   * Returns true if the privacy.donottrackheader.enabled pref is set.
   */
  static bool DoNotTrackEnabled();

  /**
   * Returns a LogModule that dump calls from content script are logged to.
   * This can be enabled with the 'Dump' module, and is useful for synchronizing
   * content JS to other logging modules.
   */
  static mozilla::LogModule* DOMDumpLog();

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
   * Returns the inner window ID for the window associated with a request.
   */
  static uint64_t GetInnerWindowID(nsIRequest* aRequest);

  /**
   * Returns the inner window ID for the window associated with a load group.
   */
  static uint64_t GetInnerWindowID(nsILoadGroup* aLoadGroup);

  /**
   * If the hostname for aURI is an IPv6 it encloses it in brackets,
   * otherwise it just outputs the hostname in aHost.
   */
  static nsresult GetHostOrIPv6WithBrackets(nsIURI* aURI, nsAString& aHost);
  static nsresult GetHostOrIPv6WithBrackets(nsIURI* aURI, nsCString& aHost);

  /*
   * Call the given callback on all remote children of the given top-level
   * window. Return true from the callback to stop calling further children.
   */
  static void CallOnAllRemoteChildren(nsPIDOMWindowOuter* aWindow,
                                      CallOnRemoteChildFunction aCallback,
                                      void* aArg);

  /*
   * Call nsPIDOMWindow::SetKeyboardIndicators all all remote children. This is
   * in here rather than nsGlobalWindow because TabParent indirectly includes
   * Windows headers which aren't allowed there.
   */
  static void SetKeyboardIndicatorsOnRemoteChildren(nsPIDOMWindowOuter* aWindow,
                                                    UIStateChangeType aShowAccelerators,
                                                    UIStateChangeType aShowFocusRings);

  /**
   * Given an nsIFile, attempts to read it into aString.
   *
   * Note: Use sparingly! This causes main-thread I/O, which causes jank and all
   * other bad things.
   */
  static nsresult SlurpFileToString(nsIFile* aFile, nsACString& aString);

  /**
   * Returns true if the mime service thinks this file contains an image.
   *
   * The content type is returned in aType.
   */
  static bool IsFileImage(nsIFile* aFile, nsACString& aType);

  /**
   * Given an IPCDataTransferItem that has a flavor for which IsFlavorImage
   * returns true and whose IPCDataTransferData is of type nsCString (raw image
   * data), construct an imgIContainer for the image encoded by the transfer
   * item.
   */
  static nsresult DataTransferItemToImage(const mozilla::dom::IPCDataTransferItem& aItem,
                                          imgIContainer** aContainer);

  /**
   * Given a flavor obtained from an IPCDataTransferItem or nsITransferable,
   * returns true if we should treat the data as an image.
   */
  static bool IsFlavorImage(const nsACString& aFlavor);

  static nsresult IPCTransferableToTransferable(const mozilla::dom::IPCDataTransfer& aDataTransfer,
                                                const bool& aIsPrivateData,
                                                nsIPrincipal* aRequestingPrincipal,
                                                const nsContentPolicyType& aContentPolicyType,
                                                nsITransferable* aTransferable,
                                                mozilla::dom::nsIContentParent* aContentParent,
                                                mozilla::dom::TabChild* aTabChild);

  static void TransferablesToIPCTransferables(nsIArray* aTransferables,
                                              nsTArray<mozilla::dom::IPCDataTransfer>& aIPC,
                                              bool aInSyncMessage,
                                              mozilla::dom::nsIContentChild* aChild,
                                              mozilla::dom::nsIContentParent* aParent);

  static void TransferableToIPCTransferable(nsITransferable* aTransferable,
                                            mozilla::dom::IPCDataTransfer* aIPCDataTransfer,
                                            bool aInSyncMessage,
                                            mozilla::dom::nsIContentChild* aChild,
                                            mozilla::dom::nsIContentParent* aParent);

  /*
   * Get the pixel data from the given source surface and return it as a buffer.
   * The length and stride will be assigned from the surface.
   */
  static mozilla::UniquePtr<char[]> GetSurfaceData(
    mozilla::NotNull<mozilla::gfx::DataSourceSurface*> aSurface,
    size_t* aLength, int32_t* aStride);

  /*
   * Get the pixel data from the given source surface and fill it in Shmem.
   * The length and stride will be assigned from the surface.
   */
  static mozilla::Maybe<mozilla::ipc::Shmem>
  GetSurfaceData(mozilla::gfx::DataSourceSurface* aSurface,
                 size_t* aLength, int32_t* aStride,
                 mozilla::ipc::IShmemAllocator* aAlloc);

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
   * Synthesize a mouse event to the given widget
   * (see nsIDOMWindowUtils.sendMouseEvent).
   */
  MOZ_CAN_RUN_SCRIPT
  static nsresult SendMouseEvent(const nsCOMPtr<nsIPresShell>& aPresShell,
                                 const nsAString& aType,
                                 float aX,
                                 float aY,
                                 int32_t aButton,
                                 int32_t aButtons,
                                 int32_t aClickCount,
                                 int32_t aModifiers,
                                 bool aIgnoreRootScrollFrame,
                                 float aPressure,
                                 unsigned short aInputSourceArg,
                                 uint32_t aIdentifier,
                                 bool aToWindow,
                                 bool *aPreventDefault,
                                 bool aIsDOMEventSynthesized,
                                 bool aIsWidgetEventSynthesized);

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
                                                nsIHttpChannel* aChannel,
                                                mozilla::net::ReferrerPolicy aReferrerPolicy);

  /*
   * Parse a referrer policy from a Referrer-Policy header
   * https://www.w3.org/TR/referrer-policy/#parse-referrer-policy-from-header
   *
   * @param aHeader the response's Referrer-Policy header to parse
   * @return referrer policy from the response header.
   */
  static mozilla::net::ReferrerPolicy GetReferrerPolicyFromHeader(const nsAString& aHeader);

  static bool IsNonSubresourceRequest(nsIChannel* aChannel);

  static bool IsNonSubresourceInternalPolicyType(nsContentPolicyType aType);

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
    // Keep this at the end.  Used for serialization, but not a valid value.
    eNumValues = 4,
  };

  /*
   * Checks if storage for the given window is permitted by a combination of
   * the user's preferences, and whether the window is a third-party iframe.
   *
   * This logic is intended to be shared between the different forms of
   * persistent storage which are available to web pages. Cookies don't use
   * this logic, and security logic related to them must be updated separately.
   */
  static StorageAccess StorageAllowedForWindow(nsPIDOMWindowInner* aWindow);

  /*
   * Checks if storage for the given document is permitted by a combination of
   * the user's preferences, and whether the document's window is a third-party
   * iframe.
   *
   * Note, this may be used on documents during the loading process where
   * the window's extant document has not been set yet.  The code in
   * StorageAllowedForWindow(), however, will not work in these cases.
   */
  static StorageAccess StorageAllowedForDocument(nsIDocument* aDoc);

  /*
   * Checks if storage should be allowed for a new window with the given
   * principal, load URI, and parent.
   */
  static StorageAccess StorageAllowedForNewWindow(nsIPrincipal* aPrincipal,
                                                  nsIURI* aURI,
                                                  nsPIDOMWindowInner* aParent);

  /*
   * Checks if storage should be allowed for the given channel.  The check will
   * be based on the channel result principal and, depending on preferences and
   * permissions, mozIThirdPartyUtil.isThirdPartyChannel().
   */
  static StorageAccess StorageAllowedForChannel(nsIChannel* aChannel);

  /*
   * Checks if storage for the given principal is permitted by the user's
   * preferences. The caller is assumed to not be a third-party iframe.
   * (if that is possible, the caller should use StorageAllowedForWindow)
   */
  static StorageAccess StorageAllowedForPrincipal(nsIPrincipal* aPrincipal);

  /*
   * Returns true if this window/channel should disable storages because of the
   * anti-tracking feature.
   */
  static bool StorageDisabledByAntiTracking(nsPIDOMWindowInner* aWindow,
                                            nsIChannel* aChannel,
                                            nsIURI* aURI);

  /*
   * Returns true if this window/channel is a 3rd party context.
   */
  static bool IsThirdPartyWindowOrChannel(nsPIDOMWindowInner* aWindow,
                                          nsIChannel* aChannel,
                                          nsIURI* aURI);

  /*
   * Returns true if this window's channel has been marked as a tracking
   * resource.
   */
  static bool IsTrackingResourceWindow(nsPIDOMWindowInner* aWindow);

  /*
   * Serializes a HTML nsINode into its markup representation.
   */
  static bool SerializeNodeToMarkup(nsINode* aRoot,
                                    bool aDescendentsOnly,
                                    nsAString& aOut);

  /*
   * Returns true iff the provided JSObject is a global, and its URI matches
   * the provided about: URI.
   * @param aGlobal the JSObject whose URI to check, if it is a global.
   * @param aUri the URI to match, e.g. "about:feeds"
   */
  static bool IsSpecificAboutPage(JSObject* aGlobal, const char* aUri);

  static void SetScrollbarsVisibility(nsIDocShell* aDocShell, bool aVisible);

  /*
   * Return the associated presentation URL of the presented content.
   * Will return empty string if the docshell is not in a presented content.
   */
  static void GetPresentationURL(nsIDocShell* aDocShell, nsAString& aPresentationUrl);

  /*
   * Try to find the docshell corresponding to the given event target.
   */
  static nsIDocShell* GetDocShellForEventTarget(mozilla::dom::EventTarget* aTarget);

  /**
   * Returns true if the "HTTPS state" of the document should be "modern". See:
   *
   * https://html.spec.whatwg.org/#concept-document-https-state
   * https://fetch.spec.whatwg.org/#concept-response-https-state
   */
  static bool HttpsStateIsModern(nsIDocument* aDocument);

  /**
   * Try to upgrade an element.
   * https://html.spec.whatwg.org/multipage/custom-elements.html#concept-try-upgrade
   */
  static void TryToUpgradeElement(Element* aElement);

  /**
   * Creates a new XUL or XHTML element applying any appropriate custom element
   * definition.
   */
  static nsresult NewXULOrHTMLElement(Element** aResult, mozilla::dom::NodeInfo* aNodeInfo,
                                      mozilla::dom::FromParser aFromParser, nsAtom* aIsAtom,
                                      mozilla::dom::CustomElementDefinition* aDefinition);

  /**
   * Looking up a custom element definition.
   * https://html.spec.whatwg.org/#look-up-a-custom-element-definition
   */
  static mozilla::dom::CustomElementDefinition*
    LookupCustomElementDefinition(nsIDocument* aDoc,
                                  nsAtom* aNameAtom,
                                  uint32_t aNameSpaceID,
                                  nsAtom* aTypeAtom);

  static void RegisterCallbackUpgradeElement(Element* aElement,
                                             nsAtom* aTypeName);

  static void RegisterUnresolvedElement(Element* aElement, nsAtom* aTypeName);
  static void UnregisterUnresolvedElement(Element* aElement);

  static void EnqueueUpgradeReaction(Element* aElement,
                                     mozilla::dom::CustomElementDefinition* aDefinition);

  static void EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                       Element* aCustomElement,
                                       mozilla::dom::LifecycleCallbackArgs* aArgs = nullptr,
                                       mozilla::dom::LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs = nullptr,
                                       mozilla::dom::CustomElementDefinition* aDefinition = nullptr);

  static bool AttemptLargeAllocationLoad(nsIHttpChannel* aChannel);

  /**
   * Appends all "document level" native anonymous content subtree roots for
   * aDocument to aElements.  Document level NAC subtrees are those created
   * by ancestor frames of the document element's primary frame, such as
   * the scrollbar elements created by the root scroll frame.
   */
  static void AppendDocumentLevelNativeAnonymousContentTo(
      nsIDocument* aDocument,
      nsTArray<nsIContent*>& aElements);

  /**
   * Appends all native anonymous content subtree roots generated by `aContent`
   * to `aKids`.
   *
   * See `AllChildrenIterator` for the description of the `aFlags` parameter.
   */
  static void AppendNativeAnonymousChildren(const nsIContent* aContent,
                                            nsTArray<nsIContent*>& aKids,
                                            uint32_t aFlags);

  /**
   * Query triggeringPrincipal if there's a 'triggeringprincipal' attribute on
   * aLoadingNode, if no such attribute is specified, aDefaultPrincipal is
   * returned if it is provided, otherwise the NodePrincipal of aLoadingNode is
   * returned.
   *
   * Return true if aLoadingNode has a 'triggeringprincipal' attribute, and
   * the value 'triggeringprincipal' is also successfully deserialized, otherwise
   * return false.
   */
  static bool
  QueryTriggeringPrincipal(nsIContent* aLoadingNode,
                           nsIPrincipal* aDefaultPrincipal,
                           nsIPrincipal** aTriggeringPrincipal);

  static bool
  QueryTriggeringPrincipal(nsIContent* aLoadingNode,
                           nsIPrincipal** aTriggeringPrincipal)
  {
    return QueryTriggeringPrincipal(aLoadingNode, nullptr, aTriggeringPrincipal);
  }

  /**
   * Returns the content policy type that should be used for loading images
   * for displaying in the UI.  The sources of such images can be <xul:image>,
   * <xul:menuitem> on OSX where we load the image through nsMenuItemIconX, etc.
   */
  static void
  GetContentPolicyTypeForUIImageLoading(nsIContent* aLoadingNode,
                                        nsIPrincipal** aTriggeringPrincipal,
                                        nsContentPolicyType& aContentPolicyType,
                                        uint64_t* aRequestContextID);

  static nsresult
  CreateJSValueFromSequenceOfObject(JSContext* aCx,
                                    const mozilla::dom::Sequence<JSObject*>& aTransfer,
                                    JS::MutableHandle<JS::Value> aValue);

  static bool
  IsShadowDOMEnabled() { return sIsShadowDOMEnabled; }

  /**
   * Returns true if reserved key events should be prevented from being sent
   * to their target. Instead, the key event should be handled by chrome only.
   */
  static bool ShouldBlockReservedKeys(mozilla::WidgetKeyboardEvent* aKeyEvent);

  /**
   * Returns the nsIPluginTag for the plugin we should try to use for a given
   * MIME type.
   *
   * @param aMIMEType  The MIME type of the document being loaded.
   * @param aNoFakePlugin  If false then this method should consider JS plugins.
   */
  static already_AddRefed<nsIPluginTag>
    PluginTagForType(const nsCString& aMIMEType, bool aNoFakePlugin);

  /**
   * Returns one of the nsIObjectLoadingContent::TYPE_ values describing the
   * content type which will be used for the given MIME type when loaded within
   * an nsObjectLoadingContent.
   *
   * NOTE: This method doesn't take capabilities into account. The caller must
   * take that into account.
   *
   * @param aMIMEType  The MIME type of the document being loaded.
   * @param aNoFakePlugin  If false then this method should consider JS plugins.
   * @param aContent The nsIContent object which is performing the load. May be
   *                 nullptr in which case the docshell's plugin permissions
   *                 will not be checked.
   */
  static uint32_t
  HtmlObjectContentTypeForMIMEType(const nsCString& aMIMEType,
                                   bool aNoFakePlugin,
                                   nsIContent* aContent);

  static already_AddRefed<nsISerialEventTarget>
  GetEventTargetByLoadInfo(nsILoadInfo* aLoadInfo, mozilla::TaskCategory aCategory);

  /**
   * Detect whether a string is a local-url.
   * https://drafts.csswg.org/css-values/#local-urls
   */
  static bool
  IsLocalRefURL(const nsString& aString);

  /**
   * Detect whether a string is a local-url.
   * https://drafts.csswg.org/css-values/#local-urls
   */
  static bool
  IsLocalRefURL(const nsACString& aString);

  static bool
  IsCustomElementsEnabled() { return sIsCustomElementsEnabled; }

  /**
   * Compose a tab id with process id and a serial number.
   */
  static uint64_t GenerateTabId();

  /**
   * Check whether we should skip moving the cursor for a same-value .value set
   * on a text input or textarea.
   */
  static bool
  SkipCursorMoveForSameValueSet() { return sSkipCursorMoveForSameValueSet; }

  /**
   * Determine whether or not the user is currently interacting with the web
   * browser. This method is safe to call from off of the main thread.
   */
  static bool
  GetUserIsInteracting();

  // Check pref "privacy.trackingprotection.lower_network_priority" to see
  // if we want to lower the priority of the channel.
  static bool IsLowerNetworkPriority() { return sLowerNetworkPriority; }

  // Whether tracker tailing is turned on - "network.http.tailing.enabled".
  static bool IsTailingEnabled() { return sTailingEnabled; }

  // Check pref "dom.placeholder.show_on_focus" to see
  // if we want to show the placeholder inside input elements
  // when they have focus.
  static bool ShowInputPlaceholderOnFocus() { return sShowInputPlaceholderOnFocus; }

  // Check pref "browser.autofocus" to see if we want to enable autofocusing elements
  // when the page requests it.
  static bool AutoFocusEnabled() { return sAutoFocusEnabled; }

  // Check pref "dom.script_loader.bytecode_cache.enabled" to see
  // if we want to cache JS bytecode on the cache entry.
  static bool IsBytecodeCacheEnabled() { return sIsBytecodeCacheEnabled; }

  // Check pref "dom.script_loader.bytecode_cache.strategy" to see which
  // heuristic strategy should be used to trigger the caching of the bytecode.
  static int32_t BytecodeCacheStrategy() { return sBytecodeCacheStrategy; }

  // Alternate data MIME type used by the ScriptLoader to register and read
  // bytecode out of the nsCacheInfoChannel.
  static nsCString& JSBytecodeMimeType() { return *sJSBytecodeMimeType; }

  /**
   * Checks if the passed-in name should override an existing name on the
   * window. Values which should not override include: "", "_blank", "_top",
   * "_parent" and "_self".
   */
  static bool IsOverridingWindowName(const nsAString& aName);

  /**
   * If there is a SourceMap (higher precedence) or X-SourceMap (lower
   * precedence) response header in |aChannel|, set |aResult| to the
   * header's value and return true.  Otherwise, return false.
   *
   * @param aChannel The HTTP channel
   * @param aResult The string result.
   */
  static bool GetSourceMapURL(nsIHttpChannel* aChannel, nsACString& aResult);

  /**
   * Returns true if the passed-in mesasge is a pending InputEvent.
   *
   * @param aMsg  The message to check
   */
  static bool IsMessageInputEvent(const IPC::Message& aMsg);

  static void AsyncPrecreateStringBundles();

  static bool ContentIsLink(nsIContent* aContent);

  static already_AddRefed<mozilla::dom::EventTarget>
  TryGetTabChildGlobalAsEventTarget(nsISupports* aFrom);

  static PopupControlState
  PushPopupControlState(PopupControlState aState, bool aForce)
  {
    MOZ_ASSERT(NS_IsMainThread());
    PopupControlState old = sPopupControlState;
    if (aState < old || aForce) {
      sPopupControlState = aState;
    }
    return old;
  }

  static void
  PopPopupControlState(PopupControlState aState)
  {
    MOZ_ASSERT(NS_IsMainThread());
    sPopupControlState = aState;
  }

  static PopupControlState GetPopupControlState() { return sPopupControlState; }

  // Get a serial number for a newly created inner or outer window.
  static uint32_t InnerOrOuterWindowCreated();
  // Record that an inner or outer window has been destroyed.
  static void InnerOrOuterWindowDestroyed();
  // Get the current number of inner or outer windows.
  static int32_t GetCurrentInnerOrOuterWindowCount() { return sInnerOrOuterWindowCount; }

  static bool CanShowPopup(nsIPrincipal* aPrincipal);

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
                                CanBubble,
                                Cancelable,
                                Trusted,
                                bool* aDefaultAction = nullptr,
                                ChromeOnlyDispatch = ChromeOnlyDispatch::eNo);

  static nsresult DispatchEvent(nsIDocument* aDoc,
                                nsISupports* aTarget,
                                mozilla::WidgetEvent& aWidgetEvent,
                                EventMessage aEventMessage,
                                CanBubble,
                                Cancelable,
                                Trusted,
                                bool* aDefaultAction = nullptr,
                                ChromeOnlyDispatch = ChromeOnlyDispatch::eNo);

  static void InitializeModifierStrings();

  static void DropFragmentParsers();

  static bool MatchClassNames(mozilla::dom::Element* aElement,
                              int32_t aNamespaceID,
                              nsAtom* aAtom, void* aData);
  static void DestroyClassNameArray(void* aData);
  static void* AllocClassMatchingInfo(nsINode* aRootNode,
                                      const nsString* aClasses);

  static mozilla::EventClassID
  GetEventClassIDFromMessage(EventMessage aEventMessage);

  // Fills in aInfo with the tokens from the supplied autocomplete attribute.
  static AutocompleteAttrState InternalSerializeAutocompleteAttribute(const nsAttrValue* aAttrVal,
                                                                      mozilla::dom::AutocompleteInfo& aInfo,
                                                                      bool aGrantAllValidValue = false);

  static bool CallOnAllRemoteChildren(mozilla::dom::MessageBroadcaster* aManager,
                                      CallOnRemoteChildFunction aCallback,
                                      void* aArg);

  /**
   * Gets the current cookie lifetime policy and cookie behavior for a given
   * principal by checking with preferences and the permission manager.
   *
   * Used in the implementation of InternalStorageAllowedForPrincipal.
   */
  static void GetCookieBehaviorForPrincipal(nsIPrincipal* aPrincipal,
                                            uint32_t* aLifetimePolicy,
                                            uint32_t* aBehavior);

  /*
   * Checks if storage for a given principal is permitted by the user's
   * preferences. If aWindow is non-null, its principal must be passed as
   * aPrincipal, and the third-party iframe and sandboxing status of the window
   * are also checked.  If aURI is non-null, then it is used as the comparison
   * against aWindow to determine if this is a third-party load.  We also
   * allow a channel instead of the window reference when determining 3rd party
   * status.
   *
   * Used in the implementation of StorageAllowedForWindow and
   * StorageAllowedForPrincipal.
   */
  static StorageAccess InternalStorageAllowedForPrincipal(nsIPrincipal* aPrincipal,
                                                          nsPIDOMWindowInner* aWindow,
                                                          nsIURI* aURI,
                                                          nsIChannel* aChannel);

  static nsINode* GetCommonAncestorHelper(nsINode* aNode1, nsINode* aNode2);
  static nsIContent* GetCommonFlattenedTreeAncestorHelper(nsIContent* aContent1,
                                                          nsIContent* aContent2);

  static nsIXPConnect *sXPConnect;

  static nsIScriptSecurityManager *sSecurityManager;
  static nsIPrincipal *sSystemPrincipal;
  static nsIPrincipal *sNullSubjectPrincipal;

  static nsNameSpaceManager *sNameSpaceManager;

  static nsIIOService *sIOService;
  static nsIUUIDGenerator *sUUIDGenerator;

  static nsIConsoleService* sConsoleService;

  static nsDataHashtable<nsRefPtrHashKey<nsAtom>, EventNameMapping>* sAtomEventTable;
  static nsDataHashtable<nsStringHashKey, EventNameMapping>* sStringEventTable;
  static nsTArray<RefPtr<nsAtom>>* sUserDefinedEvents;

  static nsIStringBundleService* sStringBundleService;
  static nsIStringBundle* sStringBundles[PropertiesFile_COUNT];
  class nsContentUtilsReporter;

  static nsIContentPolicy* sContentPolicyService;
  static bool sTriedToGetContentPolicy;

  static RefPtr<mozilla::intl::LineBreaker> sLineBreaker;
  static RefPtr<mozilla::intl::WordBreaker> sWordBreaker;

  static nsIBidiKeyboard* sBidiKeyboard;

  static bool sInitialized;
  static uint32_t sScriptBlockerCount;
  static uint32_t sDOMNodeRemovedSuppressCount;

  // Not an nsCOMArray because removing elements from those is slower
  static AutoTArray<nsCOMPtr<nsIRunnable>, 8>* sBlockedScriptRunners;
  static uint32_t sRunnersCountAtFirstBlocker;
  static uint32_t sScriptBlockerCountWhereRunnersPrevented;

  static nsIInterfaceRequestor* sSameOriginChecker;

  static bool sIsHandlingKeyBoardEvent;
  static bool sAllowXULXBL_for_file;
  static bool sDisablePopups;
  static bool sIsFullScreenApiEnabled;
  static bool sIsUnprefixedFullscreenApiEnabled;
  static bool sTrustedFullScreenOnly;
  static bool sIsCutCopyAllowed;
  static uint32_t sHandlingInputTimeout;
  static bool sIsPerformanceTimingEnabled;
  static bool sIsResourceTimingEnabled;
  static bool sIsPerformanceNavigationTimingEnabled;
  static bool sIsUpgradableDisplayContentPrefEnabled;
  static bool sIsFrameTimingPrefEnabled;
  static bool sIsFormAutofillAutocompleteEnabled;
  static bool sIsShadowDOMEnabled;
  static bool sIsCustomElementsEnabled;
  static bool sSendPerformanceTimingNotifications;
  static bool sUseActivityCursor;
  static bool sAnimationsAPICoreEnabled;
  static bool sGetBoxQuadsEnabled;
  static bool sSkipCursorMoveForSameValueSet;
  static bool sRequestIdleCallbackEnabled;
  static bool sLowerNetworkPriority;
  static bool sTailingEnabled;
  static bool sShowInputPlaceholderOnFocus;
  static bool sAutoFocusEnabled;
#ifndef RELEASE_OR_BETA
  static bool sBypassCSSOMOriginCheck;
#endif
  static bool sIsBytecodeCacheEnabled;
  static int32_t sBytecodeCacheStrategy;
  static uint32_t sCookiesLifetimePolicy;
  static uint32_t sCookiesBehavior;
  static bool sShortcutsCustomized;

  static int32_t sPrivacyMaxInnerWidth;
  static int32_t sPrivacyMaxInnerHeight;

  class UserInteractionObserver;
  static UserInteractionObserver* sUserInteractionObserver;

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

  // Alternate data mime type, used by the ScriptLoader to register and read the
  // bytecode out of the nsCacheInfoChannel.
  static nsCString* sJSBytecodeMimeType;

  static bool sDoNotTrackEnabled;
  static mozilla::LazyLogModule sDOMDumpLog;

  static PopupControlState sPopupControlState;

  static int32_t sInnerOrOuterWindowCount;
  static uint32_t sInnerOrOuterWindowSerialCounter;
};

/* static */ inline
nsContentPolicyType
nsContentUtils::InternalContentPolicyTypeToExternal(nsContentPolicyType aType)
{
  switch (aType) {
  case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
  case nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD:
  case nsIContentPolicy::TYPE_INTERNAL_WORKER:
  case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
  case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
  case nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS:
    return nsIContentPolicy::TYPE_SCRIPT;

  case nsIContentPolicy::TYPE_INTERNAL_EMBED:
  case nsIContentPolicy::TYPE_INTERNAL_OBJECT:
    return nsIContentPolicy::TYPE_OBJECT;

  case nsIContentPolicy::TYPE_INTERNAL_FRAME:
  case nsIContentPolicy::TYPE_INTERNAL_IFRAME:
    return nsIContentPolicy::TYPE_SUBDOCUMENT;

  case nsIContentPolicy::TYPE_INTERNAL_AUDIO:
  case nsIContentPolicy::TYPE_INTERNAL_VIDEO:
  case nsIContentPolicy::TYPE_INTERNAL_TRACK:
    return nsIContentPolicy::TYPE_MEDIA;

  case nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST:
  case nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE:
    return nsIContentPolicy::TYPE_XMLHTTPREQUEST;

  case nsIContentPolicy::TYPE_INTERNAL_IMAGE:
  case nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD:
  case nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON:
    return nsIContentPolicy::TYPE_IMAGE;

  case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET:
  case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD:
    return nsIContentPolicy::TYPE_STYLESHEET;

  default:
    return aType;
  }
}

/* static */ inline
nsContentPolicyType
nsContentUtils::InternalContentPolicyTypeToExternalOrWorker(nsContentPolicyType aType)
{
  switch (aType) {
  case nsIContentPolicy::TYPE_INTERNAL_WORKER:
  case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
  case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
    return aType;

  default:
    return InternalContentPolicyTypeToExternal(aType);
  }
}

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
