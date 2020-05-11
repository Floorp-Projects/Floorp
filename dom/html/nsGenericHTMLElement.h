/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsGenericHTMLElement_h___
#define nsGenericHTMLElement_h___

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsMappedAttributeElement.h"
#include "nsNameSpaceManager.h"  // for kNameSpaceID_None
#include "nsIFormControl.h"
#include "nsGkAtoms.h"
#include "nsContentCreatorFunctions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ValidityState.h"
#include "mozilla/dom/Element.h"

class nsDOMTokenList;
class nsIFormControlFrame;
class nsIFrame;
class nsILayoutHistoryState;
class nsIURI;
struct nsSize;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
class EventChainVisitor;
class EventListenerManager;
class EventStates;
class TextEditor;
class PresState;
namespace dom {
class ElementInternals;
class HTMLFormElement;
class HTMLMenuElement;
}  // namespace dom
}  // namespace mozilla

typedef nsMappedAttributeElement nsGenericHTMLElementBase;

/**
 * A common superclass for HTML elements
 */
class nsGenericHTMLElement : public nsGenericHTMLElementBase {
 public:
  using Element::Focus;
  using Element::SetTabIndex;
  explicit nsGenericHTMLElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElementBase(std::move(aNodeInfo)) {
    NS_ASSERTION(mNodeInfo->NamespaceID() == kNameSpaceID_XHTML,
                 "Unexpected namespace");
    AddStatesSilently(NS_EVENT_STATE_LTR);
    SetFlags(NODE_HAS_DIRECTION_LTR);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsGenericHTMLElement,
                                       nsGenericHTMLElementBase)

  NS_IMPL_FROMNODE(nsGenericHTMLElement, kNameSpaceID_XHTML)

  // From Element
  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  void GetTitle(mozilla::dom::DOMString& aTitle) {
    GetHTMLAttr(nsGkAtoms::title, aTitle);
  }
  void SetTitle(const nsAString& aTitle) {
    SetHTMLAttr(nsGkAtoms::title, aTitle);
  }
  void GetLang(mozilla::dom::DOMString& aLang) {
    GetHTMLAttr(nsGkAtoms::lang, aLang);
  }
  void SetLang(const nsAString& aLang) { SetHTMLAttr(nsGkAtoms::lang, aLang); }
  void GetDir(nsAString& aDir) { GetHTMLEnumAttr(nsGkAtoms::dir, aDir); }
  void SetDir(const nsAString& aDir, mozilla::ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::dir, aDir, aError);
  }
  bool Hidden() const { return GetBoolAttr(nsGkAtoms::hidden); }
  void SetHidden(bool aHidden, mozilla::ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::hidden, aHidden, aError);
  }
  void Click(mozilla::dom::CallerType aCallerType);
  void GetAccessKey(nsString& aAccessKey) {
    GetHTMLAttr(nsGkAtoms::accesskey, aAccessKey);
  }
  void SetAccessKey(const nsAString& aAccessKey, mozilla::ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::accesskey, aAccessKey, aError);
  }
  void GetAccessKeyLabel(nsString& aAccessKeyLabel);
  virtual bool Draggable() const {
    return AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                       nsGkAtoms::_true, eIgnoreCase);
  }
  void SetDraggable(bool aDraggable, mozilla::ErrorResult& aError) {
    SetHTMLAttr(
        nsGkAtoms::draggable,
        aDraggable ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false"),
        aError);
  }
  void GetContentEditable(nsString& aContentEditable) {
    ContentEditableTristate value = GetContentEditableValue();
    if (value == eTrue) {
      aContentEditable.AssignLiteral("true");
    } else if (value == eFalse) {
      aContentEditable.AssignLiteral("false");
    } else {
      aContentEditable.AssignLiteral("inherit");
    }
  }
  void SetContentEditable(const nsAString& aContentEditable,
                          mozilla::ErrorResult& aError) {
    if (aContentEditable.LowerCaseEqualsLiteral("inherit")) {
      UnsetHTMLAttr(nsGkAtoms::contenteditable, aError);
    } else if (aContentEditable.LowerCaseEqualsLiteral("true")) {
      SetHTMLAttr(nsGkAtoms::contenteditable, NS_LITERAL_STRING("true"),
                  aError);
    } else if (aContentEditable.LowerCaseEqualsLiteral("false")) {
      SetHTMLAttr(nsGkAtoms::contenteditable, NS_LITERAL_STRING("false"),
                  aError);
    } else {
      aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    }
  }
  bool IsContentEditable() {
    for (nsIContent* node = this; node; node = node->GetParent()) {
      nsGenericHTMLElement* element = FromNode(node);
      if (element) {
        ContentEditableTristate value = element->GetContentEditableValue();
        if (value != eInherit) {
          return value == eTrue;
        }
      }
    }
    return false;
  }

  void SetNonce(const nsAString& aNonce) {
    SetProperty(nsGkAtoms::nonce, new nsString(aNonce),
                nsINode::DeleteProperty<nsString>);
  }
  void RemoveNonce() { RemoveProperty(nsGkAtoms::nonce); }
  void GetNonce(nsAString& aNonce) const {
    nsString* cspNonce = static_cast<nsString*>(GetProperty(nsGkAtoms::nonce));
    if (cspNonce) {
      aNonce = *cspNonce;
    }
  }

  /**
   * Returns the count of descendants (inclusive of this node) in
   * the uncomposed document that are explicitly set as editable.
   */
  uint32_t EditableInclusiveDescendantCount();

  mozilla::dom::HTMLMenuElement* GetContextMenu() const;
  bool Spellcheck();
  void SetSpellcheck(bool aSpellcheck, mozilla::ErrorResult& aError) {
    SetHTMLAttr(
        nsGkAtoms::spellcheck,
        aSpellcheck ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false"),
        aError);
  }

  MOZ_CAN_RUN_SCRIPT
  void GetInnerText(mozilla::dom::DOMString& aValue,
                    mozilla::ErrorResult& aError);
  void SetInnerText(const nsAString& aValue);

  void GetInputMode(nsAString& aValue) {
    GetEnumAttr(nsGkAtoms::inputmode, nullptr, aValue);
  }
  void SetInputMode(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::inputmode, aValue, aRv);
  }

  /**
   * Determine whether an attribute is an event (onclick, etc.)
   * @param aName the attribute
   * @return whether the name is an event handler name
   */
  virtual bool IsEventAttributeNameInternal(nsAtom* aName) override;

#define EVENT(name_, id_, type_, struct_) /* nothing; handled by nsINode */
// The using nsINode::Get/SetOn* are to avoid warnings about shadowing the XPCOM
// getter and setter on nsINode.
#define FORWARDED_EVENT(name_, id_, type_, struct_)  \
  using nsINode::GetOn##name_;                       \
  using nsINode::SetOn##name_;                       \
  mozilla::dom::EventHandlerNonNull* GetOn##name_(); \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler);
#define ERROR_EVENT(name_, id_, type_, struct_)                       \
  using nsINode::GetOn##name_;                                        \
  using nsINode::SetOn##name_;                                        \
  already_AddRefed<mozilla::dom::EventHandlerNonNull> GetOn##name_(); \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler);
#include "mozilla/EventNameList.h"  // IWYU pragma: keep
#undef ERROR_EVENT
#undef FORWARDED_EVENT
#undef EVENT
  mozilla::dom::Element* GetOffsetParent() {
    mozilla::CSSIntRect rcFrame;
    return GetOffsetRect(rcFrame);
  }
  int32_t OffsetTop() {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.y;
  }
  int32_t OffsetLeft() {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.x;
  }
  int32_t OffsetWidth() {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.Width();
  }
  int32_t OffsetHeight() {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.Height();
  }

  // These methods are already implemented in nsIContent but we want something
  // faster for HTMLElements ignoring the namespace checking.
  // This is safe because we already know that we are in the HTML namespace.
  inline bool IsHTMLElement() const { return true; }

  inline bool IsHTMLElement(nsAtom* aTag) const {
    return mNodeInfo->Equals(aTag);
  }

  template <typename First, typename... Args>
  inline bool IsAnyOfHTMLElements(First aFirst, Args... aArgs) const {
    return IsNodeInternal(aFirst, aArgs...);
  }

  // https://html.spec.whatwg.org/multipage/custom-elements.html#dom-attachinternals
  already_AddRefed<mozilla::dom::ElementInternals> AttachInternals(
      ErrorResult& aRv);

 protected:
  virtual ~nsGenericHTMLElement() = default;

 public:
  /**
   * Get width and height, using given image request if attributes are unset.
   * Pass a reference to the image request, since the method may change the
   * value and we want to use the updated value.
   */
  MOZ_CAN_RUN_SCRIPT
  nsSize GetWidthHeightForImage(RefPtr<imgRequestProxy>& aImageRequest);

 public:
  // Implementation for nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;

  virtual bool IsFocusableInternal(int32_t* aTabIndex,
                                   bool aWithMouse) override {
    bool isFocusable = false;
    IsHTMLFocusable(aWithMouse, &isFocusable, aTabIndex);
    return isFocusable;
  }
  /**
   * Returns true if a subclass is not allowed to override the value returned
   * in aIsFocusable.
   */
  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex);
  virtual bool PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent) override;

  /**
   * Check if an event for an anchor can be handled
   * @return true if the event can be handled, false otherwise
   */
  bool CheckHandleEventForAnchorsPreconditions(
      mozilla::EventChainVisitor& aVisitor);
  void GetEventTargetParentForAnchors(mozilla::EventChainPreVisitor& aVisitor);
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEventForAnchors(mozilla::EventChainPostVisitor& aVisitor);
  bool IsHTMLLink(nsIURI** aURI) const;

  // HTML element methods
  void Compact() { mAttrs.Compact(); }

  virtual void UpdateEditableState(bool aNotify) override;

  virtual mozilla::EventStates IntrinsicState() const override;

  // Helper for setting our editable flag and notifying
  void DoSetEditableFlag(bool aEditable, bool aNotify) {
    SetEditableFlag(aEditable);
    UpdateState(aNotify);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  bool ParseBackgroundAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                const nsAString& aValue, nsAttrValue& aResult);

  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;

  /**
   * Get the base target for any links within this piece
   * of content. Generally, this is the document's base target,
   * but certain content carries a local base for backward
   * compatibility.
   *
   * @param aBaseTarget the base target [OUT]
   */
  void GetBaseTarget(nsAString& aBaseTarget) const;

  /**
   * Get the primary form control frame for this element.  Same as
   * GetPrimaryFrame(), except it QI's to nsIFormControlFrame.
   *
   * @param aFlush whether to flush out frames so that they're up to date.
   * @return the primary frame as nsIFormControlFrame
   */
  nsIFormControlFrame* GetFormControlFrame(bool aFlushFrames);

  //----------------------------------------

  /**
   * Parse an alignment attribute (top/middle/bottom/baseline)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseAlignValue(const nsAString& aString, nsAttrValue& aResult);

  /**
   * Parse a div align string to value (left/right/center/middle/justify)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseDivAlignValue(const nsAString& aString,
                                 nsAttrValue& aResult);

  /**
   * Convert a table halign string to value (left/right/center/char/justify)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseTableHAlignValue(const nsAString& aString,
                                    nsAttrValue& aResult);

  /**
   * Convert a table cell halign string to value
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseTableCellHAlignValue(const nsAString& aString,
                                        nsAttrValue& aResult);

  /**
   * Convert a table valign string to value (left/right/center/char/justify/
   * abscenter/absmiddle/middle)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseTableVAlignValue(const nsAString& aString,
                                    nsAttrValue& aResult);

  /**
   * Convert an image attribute to value (width, height, hspace, vspace, border)
   *
   * @param aAttribute the attribute to parse
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseImageAttribute(nsAtom* aAttribute, const nsAString& aString,
                                  nsAttrValue& aResult);

  static bool ParseReferrerAttribute(const nsAString& aString,
                                     nsAttrValue& aResult);

  /**
   * Convert a frameborder string to value (yes/no/1/0)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseFrameborderValue(const nsAString& aString,
                                    nsAttrValue& aResult);

  /**
   * Convert a scrolling string to value (yes/no/on/off/scroll/noscroll/auto)
   *
   * @param aString the string to parse
   * @param aResult the resulting HTMLValue
   * @return whether the value was parsed
   */
  static bool ParseScrollingValue(const nsAString& aString,
                                  nsAttrValue& aResult);

  /*
   * Attribute Mapping Helpers
   */

  /**
   * A style attribute mapping function for the most common attributes, to be
   * called by subclasses' attribute mapping functions.  Currently handles
   * dir, lang and hidden, could handle others.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapCommonAttributesInto(const nsMappedAttributes* aAttributes,
                                      mozilla::MappedDeclarations&);
  /**
   * Same as MapCommonAttributesInto except that it does not handle hidden.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapCommonAttributesIntoExceptHidden(
      const nsMappedAttributes* aAttributes, mozilla::MappedDeclarations&);

  static const MappedAttributeEntry sCommonAttributeMap[];
  static const MappedAttributeEntry sImageMarginSizeAttributeMap[];
  static const MappedAttributeEntry sImageBorderAttributeMap[];
  static const MappedAttributeEntry sImageAlignAttributeMap[];
  static const MappedAttributeEntry sDivAlignAttributeMap[];
  static const MappedAttributeEntry sBackgroundAttributeMap[];
  static const MappedAttributeEntry sBackgroundColorAttributeMap[];

  /**
   * Helper to map the align attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                         mozilla::MappedDeclarations&);

  /**
   * Helper to map the align attribute into a style struct for things
   * like <div>, <h1>, etc.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapDivAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                       mozilla::MappedDeclarations&);

  /**
   * Helper to map the valign attribute into a style struct for things
   * like <col>, <tr>, <section>, etc.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapVAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                     mozilla::MappedDeclarations&);

  /**
   * Helper to map the image border attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageBorderAttributeInto(const nsMappedAttributes* aAttributes,
                                          mozilla::MappedDeclarations&);
  /**
   * Helper to map the image margin attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageMarginAttributeInto(const nsMappedAttributes* aAttributes,
                                          mozilla::MappedDeclarations&);

  // Whether to map the width and height attributes to aspect-ratio.
  enum class MapAspectRatio { No, Yes };

  /**
   * Helper to map the image position attribute into a style struct.
   */
  static void MapImageSizeAttributesInto(const nsMappedAttributes*,
                                         mozilla::MappedDeclarations&,
                                         MapAspectRatio = MapAspectRatio::No);

  /**
   * Helper to map `width` attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapWidthAttributeInto(const nsMappedAttributes* aAttributes,
                                    mozilla::MappedDeclarations&);
  /**
   * Helper to map `height` attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapHeightAttributeInto(const nsMappedAttributes* aAttributes,
                                     mozilla::MappedDeclarations&);
  /**
   * Helper to map the background attribute
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundInto(const nsMappedAttributes* aAttributes,
                                mozilla::MappedDeclarations&);
  /**
   * Helper to map the bgcolor attribute
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBGColorInto(const nsMappedAttributes* aAttributes,
                             mozilla::MappedDeclarations&);
  /**
   * Helper to map the background attributes (currently background and bgcolor)
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundAttributesInto(const nsMappedAttributes* aAttributes,
                                          mozilla::MappedDeclarations&);
  /**
   * Helper to map the scrolling attribute on FRAME and IFRAME
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapScrollingAttributeInto(const nsMappedAttributes* aAttributes,
                                        mozilla::MappedDeclarations&);
  /**
   * Get the presentation context for this content node.
   * @return the presentation context
   */
  enum PresContextFor { eForComposedDoc, eForUncomposedDoc };
  nsPresContext* GetPresContext(PresContextFor aFor);

  // Form Helper Routines
  /**
   * Find an ancestor of this content node which is a form (could be null)
   * @param aCurrentForm the current form for this node.  If this is
   *        non-null, and no ancestor form is found, and the current form is in
   *        a connected subtree with the node, the current form will be
   *        returned.  This is needed to handle cases when HTML elements have a
   *        current form that they're not descendants of.
   * @note This method should not be called if the element has a form attribute.
   */
  mozilla::dom::HTMLFormElement* FindAncestorForm(
      mozilla::dom::HTMLFormElement* aCurrentForm = nullptr);

  virtual void RecompileScriptEventListeners() override;

  /**
   * See if the document being tested has nav-quirks mode enabled.
   * @param doc the document
   */
  static bool InNavQuirksMode(Document*);

  /**
   * Gets the absolute URI value of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If the attribute
   * isn't a relative URI the value of the attribute is returned as is. Only
   * works for attributes in null namespace.
   *
   * @param aAttr      name of attribute.
   * @param aBaseAttr  name of base attribute.
   * @param aResult    result value [out]
   */
  void GetURIAttr(nsAtom* aAttr, nsAtom* aBaseAttr, nsAString& aResult) const;

  /**
   * Gets the absolute URI values of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If a substring
   * isn't a relative URI, the substring is returned as is. Only works for
   * attributes in null namespace.
   */
  bool GetURIAttr(nsAtom* aAttr, nsAtom* aBaseAttr, nsIURI** aURI) const;

  /**
   * Returns the current disabled state of the element.
   *
   * TODO(emilio): Consider moving to Element?
   */
  bool IsDisabled() const { return State().HasState(NS_EVENT_STATE_DISABLED); }

  bool IsHidden() const {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::hidden);
  }

  virtual bool IsLabelable() const override;

  static bool MatchLabelsElement(Element* aElement, int32_t aNamespaceID,
                                 nsAtom* aAtom, void* aData);

  already_AddRefed<nsINodeList> Labels();

  static bool LegacyTouchAPIEnabled(JSContext* aCx, JSObject* aObj);

  static inline bool CanHaveName(nsAtom* aTag) {
    return aTag == nsGkAtoms::img || aTag == nsGkAtoms::form ||
           aTag == nsGkAtoms::embed || aTag == nsGkAtoms::object;
  }
  static inline bool ShouldExposeNameAsHTMLDocumentProperty(Element* aElement) {
    return aElement->IsHTMLElement() &&
           CanHaveName(aElement->NodeInfo()->NameAtom());
  }
  static inline bool ShouldExposeIdAsHTMLDocumentProperty(Element* aElement) {
    if (aElement->IsHTMLElement(nsGkAtoms::object)) {
      return true;
    }

    // Per spec, <img> is exposed by id only if it also has a nonempty
    // name (which doesn't have to match the id or anything).
    // HasName() is true precisely when name is nonempty.
    return aElement->IsHTMLElement(nsGkAtoms::img) && aElement->HasName();
  }

  virtual inline void ResultForDialogSubmit(nsAString& aResult) {
    GetAttr(kNameSpaceID_None, nsGkAtoms::value, aResult);
  }

 protected:
  /**
   * Add/remove this element to the documents name cache
   */
  void AddToNameTable(nsAtom* aName);
  void RemoveFromNameTable();

  /**
   * Register or unregister an access key to this element based on the
   * accesskey attribute.
   */
  void RegAccessKey() {
    if (HasFlag(NODE_HAS_ACCESSKEY)) {
      RegUnRegAccessKey(true);
    }
  }

  void UnregAccessKey() {
    if (HasFlag(NODE_HAS_ACCESSKEY)) {
      RegUnRegAccessKey(false);
    }
  }

 private:
  void RegUnRegAccessKey(bool aDoReg);

 protected:
  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;

  virtual mozilla::EventListenerManager* GetEventListenerManagerForAttr(
      nsAtom* aAttrName, bool* aDefer) override;

  /**
   * Dispatch a simulated mouse click by keyboard to the given element.
   */
  nsresult DispatchSimulatedClick(nsGenericHTMLElement* aElement,
                                  bool aIsTrusted, nsPresContext* aPresContext);

  /**
   * Create a URI for the given aURISpec string.
   * Returns INVALID_STATE_ERR and nulls *aURI if aURISpec is empty
   * and the document's URI matches the element's base URI.
   */
  nsresult NewURIFromString(const nsAString& aURISpec, nsIURI** aURI);

  void GetHTMLAttr(nsAtom* aName, nsAString& aResult) const {
    GetAttr(aName, aResult);
  }
  void GetHTMLAttr(nsAtom* aName, mozilla::dom::DOMString& aResult) const {
    GetAttr(kNameSpaceID_None, aName, aResult);
  }
  void GetHTMLEnumAttr(nsAtom* aName, nsAString& aResult) const {
    GetEnumAttr(aName, nullptr, aResult);
  }
  void GetHTMLURIAttr(nsAtom* aName, nsAString& aResult) const {
    GetURIAttr(aName, nullptr, aResult);
  }

  void SetHTMLAttr(nsAtom* aName, const nsAString& aValue) {
    SetAttr(kNameSpaceID_None, aName, aValue, true);
  }
  void SetHTMLAttr(nsAtom* aName, const nsAString& aValue,
                   mozilla::ErrorResult& aError) {
    SetAttr(aName, aValue, aError);
  }
  void SetHTMLAttr(nsAtom* aName, const nsAString& aValue,
                   nsIPrincipal* aTriggeringPrincipal,
                   mozilla::ErrorResult& aError) {
    SetAttr(aName, aValue, aTriggeringPrincipal, aError);
  }
  void UnsetHTMLAttr(nsAtom* aName, mozilla::ErrorResult& aError) {
    UnsetAttr(aName, aError);
  }
  void SetHTMLBoolAttr(nsAtom* aName, bool aValue,
                       mozilla::ErrorResult& aError) {
    if (aValue) {
      SetHTMLAttr(aName, EmptyString(), aError);
    } else {
      UnsetHTMLAttr(aName, aError);
    }
  }
  template <typename T>
  void SetHTMLIntAttr(nsAtom* aName, T aValue, mozilla::ErrorResult& aError) {
    nsAutoString value;
    value.AppendInt(aValue);

    SetHTMLAttr(aName, value, aError);
  }

  /**
   * Gets the integer-value of an attribute, returns specified default value
   * if the attribute isn't set or isn't set to an integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   */
  int32_t GetIntAttr(nsAtom* aAttr, int32_t aDefault) const;

  /**
   * Sets value of attribute to specified integer. Only works for attributes
   * in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Integer value of attribute.
   */
  nsresult SetIntAttr(nsAtom* aAttr, int32_t aValue);

  /**
   * Gets the unsigned integer-value of an attribute, returns specified default
   * value if the attribute isn't set or isn't set to an integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   */
  uint32_t GetUnsignedIntAttr(nsAtom* aAttr, uint32_t aDefault) const;

  /**
   * Sets value of attribute to specified unsigned integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Integer value of attribute.
   * @param aDefault Default value (in case value is out of range).  If the spec
   *                 doesn't provide one, should be 1 if the value is limited to
   *                 nonzero values, and 0 otherwise.
   */
  void SetUnsignedIntAttr(nsAtom* aName, uint32_t aValue, uint32_t aDefault,
                          mozilla::ErrorResult& aError) {
    nsAutoString value;
    if (aValue > INT32_MAX) {
      value.AppendInt(aDefault);
    } else {
      value.AppendInt(aValue);
    }

    SetHTMLAttr(aName, value, aError);
  }

  /**
   * Gets the unsigned integer-value of an attribute that is stored as a
   * dimension (i.e. could be an integer or a percentage), returns specified
   * default value if the attribute isn't set or isn't set to a dimension. Only
   * works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   */
  uint32_t GetDimensionAttrAsUnsignedInt(nsAtom* aAttr,
                                         uint32_t aDefault) const;

  /**
   * Sets value of attribute to specified double. Only works for attributes
   * in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Double value of attribute.
   */
  void SetDoubleAttr(nsAtom* aAttr, double aValue, mozilla::ErrorResult& aRv) {
    nsAutoString value;
    value.AppendFloat(aValue);

    SetHTMLAttr(aAttr, value, aRv);
  }

  /**
   * Locates the TextEditor associated with this node.  In general this is
   * equivalent to GetEditorInternal(), but for designmode or contenteditable,
   * this may need to get an editor that's not actually on this element's
   * associated TextControlFrame.  This is used by the spellchecking routines
   * to get the editor affected by changing the spellcheck attribute on this
   * node.
   */
  virtual already_AddRefed<mozilla::TextEditor> GetAssociatedEditor();

  /**
   * Get the frame's offset information for offsetTop/Left/Width/Height.
   * Returns the parent the offset is relative to.
   * @note This method flushes pending notifications (FlushType::Layout).
   * @param aRect the offset information [OUT]
   */
  mozilla::dom::Element* GetOffsetRect(mozilla::CSSIntRect& aRect);

  /**
   * Ensures all editors associated with a subtree are synced, for purposes of
   * spellchecking.
   */
  static void SyncEditorsOnSubtree(nsIContent* content);

  enum ContentEditableTristate { eInherit = -1, eFalse = 0, eTrue = 1 };

  /**
   * Returns eTrue if the element has a contentEditable attribute and its value
   * is "true" or an empty string. Returns eFalse if the element has a
   * contentEditable attribute and its value is "false". Otherwise returns
   * eInherit.
   */
  ContentEditableTristate GetContentEditableValue() const {
    static const Element::AttrValuesArray values[] = {
        nsGkAtoms::_false, nsGkAtoms::_true, nsGkAtoms::_empty, nullptr};

    if (!MayHaveContentEditableAttr()) return eInherit;

    int32_t value = FindAttrValueIn(
        kNameSpaceID_None, nsGkAtoms::contenteditable, values, eIgnoreCase);

    return value > 0 ? eTrue : (value == 0 ? eFalse : eInherit);
  }

  // Used by A, AREA, LINK, and STYLE.
  already_AddRefed<nsIURI> GetHrefURIForAnchors() const;

 public:
  /**
   * Returns whether this element is an editable root. There are two types of
   * editable roots:
   *   1) the documentElement if the whole document is editable (for example for
   *      desginMode=on)
   *   2) an element that is marked editable with contentEditable=true and that
   *      doesn't have a parent or whose parent is not editable.
   * Note that this doesn't return input and textarea elements that haven't been
   * made editable through contentEditable or designMode.
   */
  bool IsEditableRoot() const;

 private:
  void ChangeEditableState(int32_t aChange);
};

namespace mozilla {
namespace dom {
class HTMLFieldSetElement;
}  // namespace dom
}  // namespace mozilla

#define FORM_ELEMENT_FLAG_BIT(n_) \
  NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Form element specific bits
enum {
  // If this flag is set on an nsGenericHTMLFormElement or an HTMLImageElement,
  // that means that we have added ourselves to our mForm.  It's possible to
  // have a non-null mForm, but not have this flag set.  That happens when the
  // form is set via the content sink.
  ADDED_TO_FORM = FORM_ELEMENT_FLAG_BIT(0),

  // If this flag is set on an nsGenericHTMLFormElement or an HTMLImageElement,
  // that means that its form is in the process of being unbound from the tree,
  // and this form element hasn't re-found its form in
  // nsGenericHTMLFormElement::UnbindFromTree yet.
  MAYBE_ORPHAN_FORM_ELEMENT = FORM_ELEMENT_FLAG_BIT(1),

  // If this flag is set on an nsGenericHTMLElement or an HTMLImageElement, then
  // the element might be in the past names map of its form.
  MAY_BE_IN_PAST_NAMES_MAP = FORM_ELEMENT_FLAG_BIT(2)
};

// NOTE: I don't think it's possible to have both ADDED_TO_FORM and
// MAYBE_ORPHAN_FORM_ELEMENT set at the same time, so if it becomes an issue we
// can probably merge them into the same bit.  --bz

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 3);

#undef FORM_ELEMENT_FLAG_BIT

/**
 * A helper class for form elements that can contain children
 */
class nsGenericHTMLFormElement : public nsGenericHTMLElement,
                                 public nsIFormControl {
 public:
  nsGenericHTMLFormElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                           uint8_t aType);

  NS_DECL_ISUPPORTS_INHERITED

  nsINode* GetScopeChainParent() const override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual void SaveSubtreeState() override;

  // nsIFormControl
  virtual mozilla::dom::HTMLFieldSetElement* GetFieldSet() override;
  virtual mozilla::dom::HTMLFormElement* GetFormElement() override;
  mozilla::dom::HTMLFormElement* GetForm() const { return mForm; }
  virtual void SetForm(mozilla::dom::HTMLFormElement* aForm) override;
  virtual void ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) override;

  NS_IMETHOD SaveState() override { return NS_OK; }

  virtual bool RestoreState(mozilla::PresState* aState) override {
    return false;
  }
  virtual bool AllowDrop() override { return true; }

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;
  virtual IMEState GetDesiredIMEState() override;
  virtual mozilla::EventStates IntrinsicState() const override;

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;
  virtual nsresult PreHandleEvent(
      mozilla::EventChainVisitor& aVisitor) override;

  /**
   * This callback is called by a fieldest on all its elements whenever its
   * disabled attribute is changed so the element knows its disabled state
   * might have changed.
   *
   * @note Classes redefining this method should not do any content
   * state updates themselves but should just make sure to call into
   * nsGenericHTMLFormElement::FieldSetDisabledChanged.
   */
  virtual void FieldSetDisabledChanged(bool aNotify);

  /**
   * Check our disabled content attribute and fieldset's (if it exists) disabled
   * state to decide whether our disabled flag should be toggled.
   */
  void UpdateDisabledState(bool aNotify);

  /**
   * Update our required/optional flags to match the given aIsRequired boolean.
   */
  void UpdateRequiredState(bool aIsRequired, bool aNotify);

  void FieldSetFirstLegendChanged(bool aNotify) { UpdateFieldSet(aNotify); }

  /**
   * This callback is called by a fieldset on all it's elements when it's being
   * destroyed. When called, the elements should check that aFieldset is there
   * first parent fieldset and null mFieldset in that case only.
   *
   * @param aFieldSet The fieldset being removed.
   */
  void ForgetFieldSet(nsIContent* aFieldset);

  /**
   * Returns if the control can be disabled.
   */
  bool CanBeDisabled() const;

  /**
   * Returns if the readonly attribute applies.
   */
  bool DoesReadOnlyApply() const;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex) override;

  virtual bool IsLabelable() const override;

  void GetFormAction(nsString& aValue);

 protected:
  virtual ~nsGenericHTMLFormElement();

  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;

  virtual void BeforeSetForm(bool aBindToTree) {}

  virtual void AfterClearForm(bool aUnbindOrDelete) {}

  void SetForm(mozilla::dom::HTMLFormElement* aForm, bool aBindToTree);

  /**
   * This method will update the form owner, using @form or looking to a parent.
   *
   * @param aBindToTree Whether the element is being attached to the tree.
   * @param aFormIdElement The element associated with the id in @form. If
   * aBindToTree is false, aFormIdElement *must* contain the element associated
   * with the id in @form. Otherwise, it *must* be null.
   *
   * @note Callers of UpdateFormOwner have to be sure the element is in a
   * document (GetUncomposedDoc() != nullptr).
   */
  void UpdateFormOwner(bool aBindToTree, Element* aFormIdElement);

  /**
   * This method will update mFieldset and set it to the first fieldset parent.
   */
  void UpdateFieldSet(bool aNotify);

  /**
   * Add a form id observer which will observe when the element with the id in
   * @form will change.
   *
   * @return The element associated with the current id in @form (may be null).
   */
  Element* AddFormIdObserver();

  /**
   * Remove the form id observer.
   */
  void RemoveFormIdObserver();

  /**
   * This method is a a callback for IDTargetObserver (from Document).
   * It will be called each time the element associated with the id in @form
   * changes.
   */
  static bool FormIdUpdated(Element* aOldElement, Element* aNewElement,
                            void* aData);

  // Returns true if the event should not be handled from GetEventTargetParent
  bool IsElementDisabledForEvents(mozilla::WidgetEvent* aEvent,
                                  nsIFrame* aFrame);

  // The focusability state of this form control.  eUnfocusable means that it
  // shouldn't be focused at all, eInactiveWindow means it's in an inactive
  // window, eActiveWindow means it's in an active window.
  enum FocusTristate { eUnfocusable, eInactiveWindow, eActiveWindow };

  // Get our focus state.  If this returns eInactiveWindow, it will set this
  // element as the focused element for that window.
  FocusTristate FocusState();

  /** The form that contains this control */
  mozilla::dom::HTMLFormElement* mForm;

  /* This is a pointer to our closest fieldset parent if any */
  mozilla::dom::HTMLFieldSetElement* mFieldSet;
};

class nsGenericHTMLFormElementWithState : public nsGenericHTMLFormElement {
 public:
  nsGenericHTMLFormElementWithState(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      mozilla::dom::FromParser aFromParser, uint8_t aType);

  /**
   * Get the presentation state for a piece of content, or create it if it does
   * not exist.  Generally used by SaveState().
   */
  mozilla::PresState* GetPrimaryPresState();

  /**
   * Get the layout history object for a particular piece of content.
   *
   * @param aRead if true, won't return a layout history state if the
   *              layout history state is empty.
   * @return the history state object
   */
  already_AddRefed<nsILayoutHistoryState> GetLayoutHistory(bool aRead);

  /**
   * Called when we have been cloned and adopted, and the information of the
   * node has been changed.
   */
  virtual void NodeInfoChanged(Document* aOldDoc) override;

 protected:
  /**
   * Restore the state for a form control in response to the element being
   * inserted into the document by the parser.  Ends up calling
   * nsIFormControl::RestoreState().
   *
   * GenerateStateKey() must already have been called.
   *
   * @return false if RestoreState() was not called, the return
   *         value of RestoreState() otherwise.
   */
  bool RestoreFormControlState();

  /* Generates the state key for saving the form state in the session if not
     computed already. The result is stored in mStateKey. */
  void GenerateStateKey();

  int32_t GetParserInsertedControlNumberForStateKey() const override {
    return mControlNumber;
  }

  /* Used to store the key to that element in the session. Is void until
     GenerateStateKey has been used */
  nsCString mStateKey;

  // A number for this form control that is unique within its owner document.
  // This is only set to a number for elements inserted into the document by
  // the parser from the network.  Otherwise, it is -1.
  int32_t mControlNumber;
};

#define NS_INTERFACE_MAP_ENTRY_IF_TAG(_interface, _tag) \
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(_interface,        \
                                     mNodeInfo->Equals(nsGkAtoms::_tag))

namespace mozilla {
namespace dom {

typedef nsGenericHTMLElement* (*HTMLContentCreatorFunction)(
    already_AddRefed<mozilla::dom::NodeInfo>&&,
    mozilla::dom::FromParser aFromParser);

}  // namespace dom
}  // namespace mozilla

/**
 * A macro to declare the NS_NewHTMLXXXElement() functions.
 */
#define NS_DECLARE_NS_NEW_HTML_ELEMENT(_elementName)        \
  namespace mozilla {                                       \
  namespace dom {                                           \
  class HTML##_elementName##Element;                        \
  }                                                         \
  }                                                         \
  nsGenericHTMLElement* NS_NewHTML##_elementName##Element(  \
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo, \
      mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

#define NS_DECLARE_NS_NEW_HTML_ELEMENT_AS_SHARED(_elementName)                \
  inline nsGenericHTMLElement* NS_NewHTML##_elementName##Element(             \
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,                   \
      mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER) { \
    return NS_NewHTMLSharedElement(std::move(aNodeInfo), aFromParser);        \
  }

/**
 * A macro to implement the NS_NewHTMLXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_HTML_ELEMENT(_elementName)                     \
  nsGenericHTMLElement* NS_NewHTML##_elementName##Element(            \
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,           \
      mozilla::dom::FromParser aFromParser) {                         \
    RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);               \
    auto* nim = nodeInfo->NodeInfoManager();                          \
    MOZ_ASSERT(nim);                                                  \
    return new (nim)                                                  \
        mozilla::dom::HTML##_elementName##Element(nodeInfo.forget()); \
  }

#define NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(_elementName)  \
  nsGenericHTMLElement* NS_NewHTML##_elementName##Element(      \
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,     \
      mozilla::dom::FromParser aFromParser) {                   \
    RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);         \
    auto* nim = nodeInfo->NodeInfoManager();                    \
    MOZ_ASSERT(nim);                                            \
    return new (nim) mozilla::dom::HTML##_elementName##Element( \
        nodeInfo.forget(), aFromParser);                        \
  }

// Here, we expand 'NS_DECLARE_NS_NEW_HTML_ELEMENT()' by hand.
// (Calling the macro directly (with no args) produces compiler warnings.)
nsGenericHTMLElement* NS_NewHTMLElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

// Distinct from the above in order to have function pointer that compared
// unequal to a function pointer to the above.
nsGenericHTMLElement* NS_NewCustomElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

NS_DECLARE_NS_NEW_HTML_ELEMENT(Shared)
NS_DECLARE_NS_NEW_HTML_ELEMENT(SharedList)

NS_DECLARE_NS_NEW_HTML_ELEMENT(Anchor)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Area)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Audio)
NS_DECLARE_NS_NEW_HTML_ELEMENT(BR)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Body)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Button)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Canvas)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Content)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Mod)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Data)
NS_DECLARE_NS_NEW_HTML_ELEMENT(DataList)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Details)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Dialog)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Div)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Embed)
NS_DECLARE_NS_NEW_HTML_ELEMENT(FieldSet)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Font)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Form)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Frame)
NS_DECLARE_NS_NEW_HTML_ELEMENT(FrameSet)
NS_DECLARE_NS_NEW_HTML_ELEMENT(HR)
NS_DECLARE_NS_NEW_HTML_ELEMENT_AS_SHARED(Head)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Heading)
NS_DECLARE_NS_NEW_HTML_ELEMENT_AS_SHARED(Html)
NS_DECLARE_NS_NEW_HTML_ELEMENT(IFrame)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Image)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Input)
NS_DECLARE_NS_NEW_HTML_ELEMENT(LI)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Label)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Legend)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Link)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Marquee)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Map)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Menu)
NS_DECLARE_NS_NEW_HTML_ELEMENT(MenuItem)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Meta)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Meter)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Object)
NS_DECLARE_NS_NEW_HTML_ELEMENT(OptGroup)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Option)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Output)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Paragraph)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Picture)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Pre)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Progress)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Script)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Select)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Slot)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Source)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Span)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Style)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Summary)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableCaption)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableCell)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableCol)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Table)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableRow)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TableSection)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Tbody)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Template)
NS_DECLARE_NS_NEW_HTML_ELEMENT(TextArea)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Tfoot)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Thead)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Time)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Title)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Track)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Unknown)
NS_DECLARE_NS_NEW_HTML_ELEMENT(Video)

#endif /* nsGenericHTMLElement_h___ */
