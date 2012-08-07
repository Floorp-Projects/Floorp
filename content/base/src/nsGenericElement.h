/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#ifndef nsGenericElement_h___
#define nsGenericElement_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentFragment.h"
#include "nsILinkHandler.h"
#include "nsNodeUtils.h"
#include "nsAttrAndChildArray.h"
#include "mozFlushType.h"
#include "nsDOMAttributeMap.h"
#include "nsIWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIDOMNodeSelector.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsPresContext.h"
#include "nsIDOMDOMStringMap.h"
#include "nsContentList.h"
#include "nsDOMClassInfoID.h" // DOMCI_DATA
#include "nsIDOMTouchEvent.h"
#include "nsIInlineEventHandlers.h"
#include "mozilla/CORSMode.h"
#include "mozilla/Attributes.h"

#include "nsISMILAttr.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsIDOMNamedNodeMap;
class nsICSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsIURI;
class nsINodeInfo;
class nsIControllers;
class nsEventChainVisitor;
class nsEventListenerManager;
class nsIScrollableFrame;
class nsAttrValueOrString;
class nsContentList;
class nsDOMTokenList;
class ContentUnbinder;
struct nsRect;

typedef PRUptrdiff PtrBits;

/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
class nsGenericElement : public mozilla::dom::Element
{
public:
  nsGenericElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual void UpdateEditableState(bool aNotify);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual nsIAtom *GetClassAttributeName() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  /**
   * Helper for SetAttr/SetParsedAttr. This method will return true if aNotify
   * is true or there are mutation listeners that must be triggered, the
   * attribute is currently set, and the new value that is about to be set is
   * different to the current value. As a perf optimization the new and old
   * values will not actually be compared if we aren't notifying and we don't
   * have mutation listeners (in which case it's cheap to just return false
   * and let the caller go ahead and set the value).
   * @param aOldValue Set to the old value of the attribute, but only if there
   *   are event listeners. If set, the type of aOldValue will be either
   *   nsAttrValue::eString or nsAttrValue::eAtom.
   * @param aModType Set to nsIDOMMutationEvent::MODIFICATION or to
   *   nsIDOMMutationEvent::ADDITION, but only if this helper returns true
   * @param aHasListeners Set to true if there are mutation event listeners
   *   listening for NS_EVENT_BITS_MUTATION_ATTRMODIFIED
   */
  bool MaybeCheckSameAttrVal(PRInt32 aNamespaceID, nsIAtom* aName,
                             nsIAtom* aPrefix,
                             const nsAttrValueOrString& aValue,
                             bool aNotify, nsAttrValue& aOldValue,
                             PRUint8* aModType, bool* aHasListeners);
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                           const nsAString& aValue, bool aNotify);
  virtual nsresult SetParsedAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 nsIAtom* aPrefix, nsAttrValue& aParsedValue,
                                 bool aNotify);
  virtual bool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                         nsAString& aResult) const;
  virtual bool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  virtual bool AttrValueIs(PRInt32 aNameSpaceID, nsIAtom* aName,
                             const nsAString& aValue,
                             nsCaseTreatment aCaseSensitive) const;
  virtual bool AttrValueIs(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aValue,
                             nsCaseTreatment aCaseSensitive) const;
  virtual PRInt32 FindAttrValueIn(PRInt32 aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const;
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const;
  virtual PRUint32 GetAttrCount() const;
  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 /*aNamespaceID*/, nsIAtom* /*aName*/)
  {
    return nullptr;
  }
  virtual nsICSSDeclaration* GetSMILOverrideStyle();
  virtual mozilla::css::StyleRule* GetSMILOverrideStyleRule();
  virtual nsresult SetSMILOverrideStyleRule(mozilla::css::StyleRule* aStyleRule,
                                            bool aNotify);
  virtual bool IsLabelable() const;

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const
  {
    List(out, aIndent, EmptyCString());
  }
  virtual void DumpContent(FILE* out, PRInt32 aIndent, bool aDumpAll) const;
  void List(FILE* out, PRInt32 aIndent, const nsCString& aPrefix) const;
  void ListAttributes(FILE* out) const;
#endif

  virtual mozilla::css::StyleRule* GetInlineStyleRule();
  virtual nsresult SetInlineStyleRule(mozilla::css::StyleRule* aStyleRule,
                                      const nsAString* aSerialized,
                                      bool aNotify);
  NS_IMETHOD_(bool)
    IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  /*
   * Attribute Mapping Helpers
   */
  struct MappedAttributeEntry {
    nsIAtom** attribute;
  };

  /**
   * A common method where you can just pass in a list of maps to check
   * for attribute dependence. Most implementations of
   * IsAttributeMapped should use this function as a default
   * handler.
   */
  template<size_t N>
  static bool
  FindAttributeDependence(const nsIAtom* aAttribute,
                          const MappedAttributeEntry* const (&aMaps)[N])
  {
    return FindAttributeDependence(aAttribute, aMaps, N);
  }

private:
  static bool
  FindAttributeDependence(const nsIAtom* aAttribute,
                          const MappedAttributeEntry* const aMaps[],
                          PRUint32 aMapCount);

public:
  // nsIDOMElement method implementation
  NS_DECL_NSIDOMELEMENT

  //----------------------------------------

  /**
   * Add a script event listener with the given event handler name
   * (like onclick) and with the value as JS
   * @param aEventName the event listener name
   * @param aValue the JS to attach
   * @param aDefer indicates if deferred execution is allowed
   */
  nsresult AddScriptEventListener(nsIAtom* aEventName,
                                  const nsAString& aValue,
                                  bool aDefer = true);

  /**
   * Do whatever needs to be done when the mouse leaves a link
   */
  nsresult LeaveLink(nsPresContext* aPresContext);

  static bool ShouldBlur(nsIContent *aContent);

  /**
   * Method to create and dispatch a left-click event loosely based on
   * aSourceEvent. If aFullDispatch is true, the event will be dispatched
   * through the full dispatching of the presshell of the aPresContext; if it's
   * false the event will be dispatched only as a DOM event.
   * If aPresContext is nullptr, this does nothing.
   */
  static nsresult DispatchClickEvent(nsPresContext* aPresContext,
                                     nsInputEvent* aSourceEvent,
                                     nsIContent* aTarget,
                                     bool aFullDispatch,
                                     PRUint32 aFlags,
                                     nsEventStatus* aStatus);

  /**
   * Method to dispatch aEvent to aTarget. If aFullDispatch is true, the event
   * will be dispatched through the full dispatching of the presshell of the
   * aPresContext; if it's false the event will be dispatched only as a DOM
   * event.
   * If aPresContext is nullptr, this does nothing.
   */
  using nsIContent::DispatchEvent;
  static nsresult DispatchEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent* aTarget,
                                bool aFullDispatch,
                                nsEventStatus* aStatus);

  /**
   * Get the primary frame for this content with flushing
   *
   * @param aType the kind of flush to do, typically Flush_Frames or
   *              Flush_Layout
   * @return the primary frame
   */
  nsIFrame* GetPrimaryFrame(mozFlushType aType);
  // Work around silly C++ name hiding stuff
  nsIFrame* GetPrimaryFrame() const { return nsIContent::GetPrimaryFrame(); }

  /**
   * Struct that stores info on an attribute.  The name and value must
   * either both be null or both be non-null.
   */
  struct nsAttrInfo {
    nsAttrInfo(const nsAttrName* aName, const nsAttrValue* aValue) :
      mName(aName), mValue(aValue) {}
    nsAttrInfo(const nsAttrInfo& aOther) :
      mName(aOther.mName), mValue(aOther.mValue) {}

    const nsAttrName* mName;
    const nsAttrValue* mValue;
  };

  // Be careful when using this method. This does *NOT* handle
  // XUL prototypes. You may want to use GetAttrInfo.
  const nsAttrValue* GetParsedAttr(nsIAtom* aAttr) const
  {
    return mAttrsAndChildren.GetAttr(aAttr);
  }

  /**
   * Returns the attribute map, if there is one.
   *
   * @return existing attribute map or nullptr.
   */
  nsDOMAttributeMap *GetAttributeMap()
  {
    nsDOMSlots *slots = GetExistingDOMSlots();

    return slots ? slots->mAttributeMap.get() : nullptr;
  }

  virtual void RecompileScriptEventListeners()
  {
  }

  PRInt32 GetScrollTop();
  PRInt32 GetScrollLeft();
  PRInt32 GetScrollHeight();
  PRInt32 GetScrollWidth();
  PRInt32 GetScrollLeftMax();
  PRInt32 GetScrollTopMax();
  PRInt32 GetClientTop()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().y);
  }
  PRInt32 GetClientLeft()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().x);
  }
  PRInt32 GetClientHeight()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().height);
  }
  PRInt32 GetClientWidth()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().width);
  }
  nsIContent* GetFirstElementChild();
  nsIContent* GetLastElementChild();
  nsIContent* GetPreviousElementSibling();
  nsIContent* GetNextElementSibling();
  nsDOMTokenList* GetClassList(nsresult *aResult);
  bool MozMatchesSelector(const nsAString& aSelector, nsresult* aResult);

  /**
   * Get the attr info for the given namespace ID and attribute name.  The
   * namespace ID must not be kNameSpaceID_Unknown and the name must not be
   * null.  Note that this can only return info on attributes that actually
   * live on this element (and is only virtual to handle XUL prototypes).  That
   * is, this should only be called from methods that only care about attrs
   * that effectively live in mAttrsAndChildren.
   */
  virtual nsAttrInfo GetAttrInfo(PRInt32 aNamespaceID, nsIAtom* aName) const;

  virtual void NodeInfoChanged(nsINodeInfo* aOldNodeInfo)
  {
  }

  /**
   * Parse a string into an nsAttrValue for a CORS attribute.  This
   * never fails.  The resulting value is an enumerated value whose
   * GetEnumValue() returns one of the above constants.
   */
  static void ParseCORSValue(const nsAString& aValue, nsAttrValue& aResult);

  /**
   * Return the CORS mode for a given string
   */
  static mozilla::CORSMode StringToCORSMode(const nsAString& aValue);
  
  /**
   * Return the CORS mode for a given nsAttrValue (which may be null,
   * but if not should have been parsed via ParseCORSValue).
   */
  static mozilla::CORSMode AttrValueToCORSMode(const nsAttrValue* aValue);

protected:
  /*
   * Named-bools for use with SetAttrAndNotify to make call sites easier to
   * read.
   */
  static const bool kFireMutationEvent           = true;
  static const bool kDontFireMutationEvent       = false;
  static const bool kNotifyDocumentObservers     = true;
  static const bool kDontNotifyDocumentObservers = false;
  static const bool kCallAfterSetAttr            = true;
  static const bool kDontCallAfterSetAttr        = false;

  /**
   * Set attribute and (if needed) notify documentobservers and fire off
   * mutation events.  This will send the AttributeChanged notification.
   * Callers of this method are responsible for calling AttributeWillChange,
   * since that needs to happen before the new attr value has been set, and
   * in particular before it has been parsed.
   *
   * For the boolean parameters, consider using the named bools above to aid
   * code readability.
   *
   * @param aNamespaceID  namespace of attribute
   * @param aAttribute    local-name of attribute
   * @param aPrefix       aPrefix of attribute
   * @param aOldValue     previous value of attribute. Only needed if
   *                      aFireMutation is true.
   * @param aParsedValue  parsed new value of attribute
   * @param aModType      nsIDOMMutationEvent::MODIFICATION or ADDITION.  Only
   *                      needed if aFireMutation or aNotify is true.
   * @param aFireMutation should mutation-events be fired?
   * @param aNotify       should we notify document-observers?
   * @param aCallAfterSetAttr should we call AfterSetAttr?
   */
  nsresult SetAttrAndNotify(PRInt32 aNamespaceID,
                            nsIAtom* aName,
                            nsIAtom* aPrefix,
                            const nsAttrValue& aOldValue,
                            nsAttrValue& aParsedValue,
                            PRUint8 aModType,
                            bool aFireMutation,
                            bool aNotify,
                            bool aCallAfterSetAttr);

  /**
   * Convert an attribute string value to attribute type based on the type of
   * attribute.  Called by SetAttr().  Note that at the moment we only do this
   * for attributes in the null namespace (kNameSpaceID_None).
   *
   * @param aNamespaceID the namespace of the attribute to convert
   * @param aAttribute the attribute to convert
   * @param aValue the string value to convert
   * @param aResult the nsAttrValue [OUT]
   * @return true if the parsing was successful, false otherwise
   */
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  /**
   * Try to set the attribute as a mapped attribute, if applicable.  This will
   * only be called for attributes that are in the null namespace and only on
   * attributes that returned true when passed to IsAttributeMapped.  The
   * caller will not try to set the attr in any other way if this method
   * returns true (the value of aRetval does not matter for that purpose).
   *
   * @param aDocument the current document of this node (an optimization)
   * @param aName the name of the attribute
   * @param aValue the nsAttrValue to set
   * @param [out] aRetval the nsresult status of the operation, if any.
   * @return true if the setting was attempted, false otherwise.
   */
  virtual bool SetMappedAttribute(nsIDocument* aDocument,
                                    nsIAtom* aName,
                                    nsAttrValue& aValue,
                                    nsresult* aRetval);

  /**
   * Hook that is called by nsGenericElement::SetAttr to allow subclasses to
   * deal with attribute sets.  This will only be called after we verify that
   * we're actually doing an attr set and will be called before
   * AttributeWillChange and before ParseAttribute and hence before we've set
   * the new value.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to represented as either a string or
   *        a parsed nsAttrValue. Alternatively, if the attr is being removed it
   *        will be null.
   * @param aNotify Whether we plan to notify document observers.
   */
  // Note that this is inlined so that when subclasses call it it gets
  // inlined.  Those calls don't go through a vtable.
  virtual nsresult BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify)
  {
    return NS_OK;
  }

  /**
   * Hook that is called by nsGenericElement::SetAttr to allow subclasses to
   * deal with attribute sets.  This will only be called after we have called
   * SetAndTakeAttr and AttributeChanged (that is, after we have actually set
   * the attr).  It will always be called under a scriptblocker.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to.  If null, the attr is being
   *        removed.
   * @param aNotify Whether we plan to notify document observers.
   */
  // Note that this is inlined so that when subclasses call it it gets
  // inlined.  Those calls don't go through a vtable.
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
  {
    return NS_OK;
  }

  /**
   * Hook to allow subclasses to produce a different nsEventListenerManager if
   * needed for attachment of attribute-defined handlers
   */
  virtual nsEventListenerManager*
    GetEventListenerManagerForAttr(nsIAtom* aAttrName, bool* aDefer);

  /**
   * Internal hook for converting an attribute name-string to an atomized name
   */
  virtual const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

  /**
   * Retrieve the rectangle for the offsetX properties, which
   * are coordinates relative to the returned aOffsetParent.
   *
   * @param aRect offset rectangle
   * @param aOffsetParent offset parent
   */
  virtual void GetOffsetRect(nsRect& aRect, nsIContent** aOffsetParent);

  /**
   * Retrieve the size of the padding rect of this element.
   *
   * @param aSize the size of the padding rect
   */
  nsIntSize GetPaddingRectSize();

  nsIFrame* GetStyledFrame();

  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    return this;
  }

  nsresult GetAttributeNodeNSInternal(const nsAString& aNamespaceURI,
                                      const nsAString& aLocalName,
                                      nsIDOMAttr** aReturn);

  void RegisterFreezableElement() {
    OwnerDoc()->RegisterFreezableElement(this);
  }
  void UnregisterFreezableElement() {
    OwnerDoc()->UnregisterFreezableElement(this);
  }

  /**
   * Add/remove this element to the documents id cache
   */
  void AddToIdTable(nsIAtom* aId) {
    NS_ASSERTION(HasID(), "Node doesn't have an ID?");
    nsIDocument* doc = GetCurrentDoc();
    if (doc && (!IsInAnonymousSubtree() || doc->IsXUL())) {
      doc->AddToIdTable(this, aId);
    }
  }
  void RemoveFromIdTable() {
    if (HasID()) {
      nsIDocument* doc = GetCurrentDoc();
      if (doc) {
        nsIAtom* id = DoGetID();
        // id can be null during mutation events evilness. Also, XUL elements
        // loose their proto attributes during cc-unlink, so this can happen
        // during cc-unlink too.
        if (id) {
          doc->RemoveFromIdTable(this, DoGetID());
        }
      }
    }
  }

  /**
   * Functions to carry out event default actions for links of all types
   * (HTML links, XLinks, SVG "XLinks", etc.)
   */

  /**
   * Check that we meet the conditions to handle a link event
   * and that we are actually on a link.
   *
   * @param aVisitor event visitor
   * @param aURI the uri of the link, set only if the return value is true [OUT]
   * @return true if we can handle the link event, false otherwise
   */
  bool CheckHandleEventForLinksPrecondition(nsEventChainVisitor& aVisitor,
                                              nsIURI** aURI) const;

  /**
   * Handle status bar updates before they can be cancelled.
   */
  nsresult PreHandleEventForLinks(nsEventChainPreVisitor& aVisitor);

  /**
   * Handle default actions for link event if the event isn't consumed yet.
   */
  nsresult PostHandleEventForLinks(nsEventChainPostVisitor& aVisitor);

  /**
   * Get the target of this link element. Consumers should established that
   * this element is a link (probably using IsLink) before calling this
   * function (or else why call it?)
   *
   * Note: for HTML this gets the value of the 'target' attribute; for XLink
   * this gets the value of the xlink:_moz_target attribute, or failing that,
   * the value of xlink:show, converted to a suitably equivalent named target
   * (e.g. _blank).
   */
  virtual void GetLinkTarget(nsAString& aTarget);

private:
  /**
   * Get this element's client area rect in app units.
   * @return the frame's client area
   */
  nsRect GetClientAreaRect();

  nsIScrollableFrame* GetScrollFrame(nsIFrame **aStyledFrame = nullptr);
};

/**
 * Macros to implement Clone(). _elementName is the class for which to implement
 * Clone.
 */
#define NS_IMPL_ELEMENT_CLONE(_elementName)                                 \
nsresult                                                                    \
_elementName::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const        \
{                                                                           \
  *aResult = nullptr;                                                        \
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;                                     \
  _elementName *it = new _elementName(ni.forget());                         \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsINode> kungFuDeathGrip = it;                                   \
  nsresult rv = const_cast<_elementName*>(this)->CopyInnerTo(it);           \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#define NS_IMPL_ELEMENT_CLONE_WITH_INIT(_elementName)                       \
nsresult                                                                    \
_elementName::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const        \
{                                                                           \
  *aResult = nullptr;                                                        \
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;                                     \
  _elementName *it = new _elementName(ni.forget());                         \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsINode> kungFuDeathGrip = it;                                   \
  nsresult rv = it->Init();                                                 \
  nsresult rv2 = const_cast<_elementName*>(this)->CopyInnerTo(it);          \
  if (NS_FAILED(rv2)) {                                                     \
    rv = rv2;                                                               \
  }                                                                         \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#define DOMCI_NODE_DATA(_interface, _class)                             \
  DOMCI_DATA(_interface, _class)                                        \
  nsXPCClassInfo* _class::GetClassInfo()                                \
  {                                                                     \
    return static_cast<nsXPCClassInfo*>(                                \
      NS_GetDOMClassInfoInstance(eDOMClassInfo_##_interface##_id));     \
  }

/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.  We use the 5-argument form of SetAttr, because
 * some consumers only implement that one, hiding superclass
 * 4-argument forms.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom)                     \
  NS_IMETHODIMP                                                         \
  _class::Get##_method(nsAString& aValue)                               \
  {                                                                     \
    GetAttr(kNameSpaceID_None, nsGkAtoms::_atom, aValue);               \
    return NS_OK;                                                       \
  }                                                                     \
  NS_IMETHODIMP                                                         \
  _class::Set##_method(const nsAString& aValue)                         \
  {                                                                     \
    return SetAttr(kNameSpaceID_None, nsGkAtoms::_atom, nullptr, aValue, true); \
  }

#endif /* nsGenericElement_h___ */
