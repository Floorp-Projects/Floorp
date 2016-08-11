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
#include "nsIDOMHTMLElement.h"
#include "nsNameSpaceManager.h"  // for kNameSpaceID_None
#include "nsIFormControl.h"
#include "nsGkAtoms.h"
#include "nsContentCreatorFunctions.h"
#include "mozilla/ErrorResult.h"
#include "nsIDOMHTMLMenuElement.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ValidityState.h"
#include "mozilla/dom/ElementInlines.h"

class nsDOMTokenList;
class nsIDOMHTMLMenuElement;
class nsIEditor;
class nsIFormControlFrame;
class nsIFrame;
class nsILayoutHistoryState;
class nsIURI;
class nsPresState;
struct nsSize;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
class EventChainVisitor;
class EventListenerManager;
class EventStates;
namespace dom {
class HTMLFormElement;
class HTMLMenuElement;
} // namespace dom
} // namespace mozilla

typedef nsMappedAttributeElement nsGenericHTMLElementBase;

/**
 * A common superclass for HTML elements
 */
class nsGenericHTMLElement : public nsGenericHTMLElementBase,
                             public nsIDOMHTMLElement
{
public:
  using Element::SetTabIndex;
  using Element::Focus;
  explicit nsGenericHTMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElementBase(aNodeInfo)
  {
    NS_ASSERTION(mNodeInfo->NamespaceID() == kNameSpaceID_XHTML,
                 "Unexpected namespace");
    AddStatesSilently(NS_EVENT_STATE_LTR);
    SetFlags(NODE_HAS_DIRECTION_LTR);
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMCONTENT(nsGenericHTMLElement, kNameSpaceID_XHTML)

  // From Element
  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  void GetTitle(mozilla::dom::DOMString& aTitle)
  {
    GetHTMLAttr(nsGkAtoms::title, aTitle);
  }
  NS_IMETHOD SetTitle(const nsAString& aTitle) override
  {
    SetHTMLAttr(nsGkAtoms::title, aTitle);
    return NS_OK;
  }
  void GetLang(mozilla::dom::DOMString& aLang)
  {
    GetHTMLAttr(nsGkAtoms::lang, aLang);
  }
  NS_IMETHOD SetLang(const nsAString& aLang) override
  {
    SetHTMLAttr(nsGkAtoms::lang, aLang);
    return NS_OK;
  }
  void GetDir(mozilla::dom::DOMString& aDir)
  {
    GetHTMLEnumAttr(nsGkAtoms::dir, aDir);
  }
  void SetDir(const nsAString& aDir, mozilla::ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::dir, aDir, aError);
  }
  bool Hidden() const
  {
    return GetBoolAttr(nsGkAtoms::hidden);
  }
  void SetHidden(bool aHidden, mozilla::ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::hidden, aHidden, aError);
  }
  virtual void Click();
  void GetAccessKey(nsString& aAccessKey)
  {
    GetHTMLAttr(nsGkAtoms::accesskey, aAccessKey);
  }
  void SetAccessKey(const nsAString& aAccessKey, mozilla::ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::accesskey, aAccessKey, aError);
  }
  void GetAccessKeyLabel(nsString& aAccessKeyLabel);
  virtual bool Draggable() const
  {
    return AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                       nsGkAtoms::_true, eIgnoreCase);
  }
  void SetDraggable(bool aDraggable, mozilla::ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::draggable,
                aDraggable ? NS_LITERAL_STRING("true")
                           : NS_LITERAL_STRING("false"),
                aError);
  }
  void GetContentEditable(nsString& aContentEditable)
  {
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
                          mozilla::ErrorResult& aError)
  {
    if (aContentEditable.LowerCaseEqualsLiteral("inherit")) {
      UnsetHTMLAttr(nsGkAtoms::contenteditable, aError);
    } else if (aContentEditable.LowerCaseEqualsLiteral("true")) {
      SetHTMLAttr(nsGkAtoms::contenteditable, NS_LITERAL_STRING("true"), aError);
    } else if (aContentEditable.LowerCaseEqualsLiteral("false")) {
      SetHTMLAttr(nsGkAtoms::contenteditable, NS_LITERAL_STRING("false"), aError);
    } else {
      aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    }
  }
  bool IsContentEditable()
  {
    for (nsIContent* node = this; node; node = node->GetParent()) {
      nsGenericHTMLElement* element = FromContent(node);
      if (element) {
        ContentEditableTristate value = element->GetContentEditableValue();
        if (value != eInherit) {
          return value == eTrue;
        }
      }
    }
    return false;
  }

  /**
   * Returns the count of descendants (inclusive of this node) in
   * the uncomposed document that are explicitly set as editable.
   */
  uint32_t EditableInclusiveDescendantCount();

  mozilla::dom::HTMLMenuElement* GetContextMenu() const;
  bool Spellcheck();
  void SetSpellcheck(bool aSpellcheck, mozilla::ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::spellcheck,
                aSpellcheck ? NS_LITERAL_STRING("true")
                            : NS_LITERAL_STRING("false"),
                aError);
  }
  bool Scrollgrab() const
  {
    return HasFlag(ELEMENT_HAS_SCROLLGRAB);
  }
  void SetScrollgrab(bool aValue)
  {
    if (aValue) {
      SetFlags(ELEMENT_HAS_SCROLLGRAB);
    } else {
      UnsetFlags(ELEMENT_HAS_SCROLLGRAB);
    }
  }

  void GetInnerText(mozilla::dom::DOMString& aValue, mozilla::ErrorResult& aError);
  void SetInnerText(const nsAString& aValue);

  /**
   * Determine whether an attribute is an event (onclick, etc.)
   * @param aName the attribute
   * @return whether the name is an event handler name
   */
  virtual bool IsEventAttributeName(nsIAtom* aName) override;

#define EVENT(name_, id_, type_, struct_) /* nothing; handled by nsINode */
// The using nsINode::Get/SetOn* are to avoid warnings about shadowing the XPCOM
// getter and setter on nsINode.
#define FORWARDED_EVENT(name_, id_, type_, struct_)                           \
  using nsINode::GetOn##name_;                                                \
  using nsINode::SetOn##name_;                                                \
  mozilla::dom::EventHandlerNonNull* GetOn##name_();                          \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler);
#define ERROR_EVENT(name_, id_, type_, struct_)                               \
  using nsINode::GetOn##name_;                                                \
  using nsINode::SetOn##name_;                                                \
  already_AddRefed<mozilla::dom::EventHandlerNonNull> GetOn##name_();         \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler);
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef ERROR_EVENT
#undef FORWARDED_EVENT
#undef EVENT
  mozilla::dom::Element* GetOffsetParent()
  {
    mozilla::CSSIntRect rcFrame;
    return GetOffsetRect(rcFrame);
  }
  int32_t OffsetTop()
  {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.y;
  }
  int32_t OffsetLeft()
  {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.x;
  }
  int32_t OffsetWidth()
  {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.width;
  }
  int32_t OffsetHeight()
  {
    mozilla::CSSIntRect rcFrame;
    GetOffsetRect(rcFrame);

    return rcFrame.height;
  }

  // These methods are already implemented in nsIContent but we want something
  // faster for HTMLElements ignoring the namespace checking.
  // This is safe because we already know that we are in the HTML namespace.
  inline bool IsHTMLElement() const
  {
    return true;
  }

  inline bool IsHTMLElement(nsIAtom* aTag) const
  {
    return mNodeInfo->Equals(aTag);
  }

  template<typename First, typename... Args>
  inline bool IsAnyOfHTMLElements(First aFirst, Args... aArgs) const
  {
    return IsNodeInternal(aFirst, aArgs...);
  }

protected:
  virtual ~nsGenericHTMLElement() {}

public:
  virtual already_AddRefed<mozilla::dom::UndoManager> GetUndoManager() override;
  virtual bool UndoScope() override;
  virtual void SetUndoScope(bool aUndoScope, mozilla::ErrorResult& aError) override;

  /**
   * Get width and height, using given image request if attributes are unset.
   * Pass a reference to the image request, since the method may change the
   * value and we want to use the updated value.
   */
  nsSize GetWidthHeightForImage(RefPtr<imgRequestProxy>& aImageRequest);

  // XPIDL methods
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  NS_IMETHOD GetTitle(nsAString& aTitle) final override {
    mozilla::dom::DOMString title;
    GetTitle(title);
    title.ToString(aTitle);
    return NS_OK;
  }
  NS_IMETHOD GetLang(nsAString& aLang) final override {
    mozilla::dom::DOMString lang;
    GetLang(lang);
    lang.ToString(aLang);
    return NS_OK;
  }
  NS_IMETHOD GetDir(nsAString& aDir) final override {
    mozilla::dom::DOMString dir;
    GetDir(dir);
    dir.ToString(aDir);
    return NS_OK;
  }
  NS_IMETHOD SetDir(const nsAString& aDir) final override {
    mozilla::ErrorResult rv;
    SetDir(aDir, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetDOMClassName(nsAString& aClassName) final {
    GetHTMLAttr(nsGkAtoms::_class, aClassName);
    return NS_OK;
  }
  NS_IMETHOD SetDOMClassName(const nsAString& aClassName) final {
    SetClassName(aClassName);
    return NS_OK;
  }
  NS_IMETHOD GetDataset(nsISupports** aDataset) final override;
  NS_IMETHOD GetHidden(bool* aHidden) final override {
    *aHidden = Hidden();
    return NS_OK;
  }
  NS_IMETHOD SetHidden(bool aHidden) final override {
    mozilla::ErrorResult rv;
    SetHidden(aHidden, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD DOMBlur() final override {
    mozilla::ErrorResult rv;
    Blur(rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetAccessKey(nsAString& aAccessKey) final override {
    nsString accessKey;
    GetAccessKey(accessKey);
    aAccessKey.Assign(accessKey);
    return NS_OK;
  }
  NS_IMETHOD SetAccessKey(const nsAString& aAccessKey) final override {
    mozilla::ErrorResult rv;
    SetAccessKey(aAccessKey, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetAccessKeyLabel(nsAString& aAccessKeyLabel)
    final override {
    nsString accessKeyLabel;
    GetAccessKeyLabel(accessKeyLabel);
    aAccessKeyLabel.Assign(accessKeyLabel);
    return NS_OK;
  }
  NS_IMETHOD SetDraggable(bool aDraggable) final override {
    mozilla::ErrorResult rv;
    SetDraggable(aDraggable, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetContentEditable(nsAString& aContentEditable)
    final override {
    nsString contentEditable;
    GetContentEditable(contentEditable);
    aContentEditable.Assign(contentEditable);
    return NS_OK;
  }
  NS_IMETHOD SetContentEditable(const nsAString& aContentEditable)
    final override {
    mozilla::ErrorResult rv;
    SetContentEditable(aContentEditable, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetIsContentEditable(bool* aIsContentEditable)
    final override {
    *aIsContentEditable = IsContentEditable();
    return NS_OK;
  }
  NS_IMETHOD GetContextMenu(nsIDOMHTMLMenuElement** aContextMenu)
    final override;
  NS_IMETHOD GetSpellcheck(bool* aSpellcheck) final override {
    *aSpellcheck = Spellcheck();
    return NS_OK;
  }
  NS_IMETHOD SetSpellcheck(bool aSpellcheck) final override {
    mozilla::ErrorResult rv;
    SetSpellcheck(aSpellcheck, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetOuterHTML(nsAString& aOuterHTML) final override {
    mozilla::dom::Element::GetOuterHTML(aOuterHTML);
    return NS_OK;
  }
  NS_IMETHOD SetOuterHTML(const nsAString& aOuterHTML) final override {
    mozilla::ErrorResult rv;
    mozilla::dom::Element::SetOuterHTML(aOuterHTML, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD InsertAdjacentHTML(const nsAString& position,
                                const nsAString& text) final override;
  NS_IMETHOD ScrollIntoView(bool top, uint8_t _argc) final override {
    if (!_argc) {
      top = true;
    }
    mozilla::dom::Element::ScrollIntoView(top);
    return NS_OK;
  }
  NS_IMETHOD GetOffsetParent(nsIDOMElement** aOffsetParent)
    final override {
    mozilla::dom::Element* offsetParent = GetOffsetParent();
    if (!offsetParent) {
      *aOffsetParent = nullptr;
      return NS_OK;
    }
    return CallQueryInterface(offsetParent, aOffsetParent);
  }
  NS_IMETHOD GetOffsetTop(int32_t* aOffsetTop) final override {
    *aOffsetTop = OffsetTop();
    return NS_OK;
  }
  NS_IMETHOD GetOffsetLeft(int32_t* aOffsetLeft) final override {
    *aOffsetLeft = OffsetLeft();
    return NS_OK;
  }
  NS_IMETHOD GetOffsetWidth(int32_t* aOffsetWidth) final override {
    *aOffsetWidth = OffsetWidth();
    return NS_OK;
  }
  NS_IMETHOD GetOffsetHeight(int32_t* aOffsetHeight) final override {
    *aOffsetHeight = OffsetHeight();
    return NS_OK;
  }
  NS_IMETHOD DOMClick() final override {
    Click();
    return NS_OK;
  }
  NS_IMETHOD GetTabIndex(int32_t* aTabIndex) final override {
    *aTabIndex = TabIndex();
    return NS_OK;
  }
  NS_IMETHOD SetTabIndex(int32_t aTabIndex) final override {
    mozilla::ErrorResult rv;
    SetTabIndex(aTabIndex, rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD Focus() final override {
    mozilla::ErrorResult rv;
    Focus(rv);
    return rv.StealNSResult();
  }
  NS_IMETHOD GetDraggable(bool* aDraggable) final override {
    *aDraggable = Draggable();
    return NS_OK;
  }
  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) override {
    return mozilla::dom::Element::GetInnerHTML(aInnerHTML);
  }
  using mozilla::dom::Element::SetInnerHTML;
  NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) final override {
    mozilla::ErrorResult rv;
    SetInnerHTML(aInnerHTML, rv);
    return rv.StealNSResult();
  }

  using nsGenericHTMLElementBase::GetOwnerDocument;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

public:
  // Implementation for nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  MOZ_ALWAYS_INLINE // Avoid a crashy hook from Avast 10 Beta
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                             bool aNotify) override;
  virtual bool IsFocusableInternal(int32_t *aTabIndex, bool aWithMouse) override
  {
    bool isFocusable = false;
    IsHTMLFocusable(aWithMouse, &isFocusable, aTabIndex);
    return isFocusable;
  }
  /**
   * Returns true if a subclass is not allowed to override the value returned
   * in aIsFocusable.
   */
  virtual bool IsHTMLFocusable(bool aWithMouse,
                               bool *aIsFocusable,
                               int32_t *aTabIndex);
  virtual bool PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent) override;

  /**
   * Check if an event for an anchor can be handled
   * @return true if the event can be handled, false otherwise
   */
  bool CheckHandleEventForAnchorsPreconditions(
         mozilla::EventChainVisitor& aVisitor);
  nsresult PreHandleEventForAnchors(mozilla::EventChainPreVisitor& aVisitor);
  nsresult PostHandleEventForAnchors(mozilla::EventChainPostVisitor& aVisitor);
  bool IsHTMLLink(nsIURI** aURI) const;

  // HTML element methods
  void Compact() { mAttrsAndChildren.Compact(); }

  virtual void UpdateEditableState(bool aNotify) override;

  virtual mozilla::EventStates IntrinsicState() const override;

  // Helper for setting our editable flag and notifying
  void DoSetEditableFlag(bool aEditable, bool aNotify) {
    SetEditableFlag(aEditable);
    UpdateState(aNotify);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;

  bool ParseBackgroundAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

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
  static bool ParseAlignValue(const nsAString& aString,
                                nsAttrValue& aResult);

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
  static bool ParseImageAttribute(nsIAtom* aAttribute,
                                    const nsAString& aString,
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
                                      nsRuleData* aRuleData);
  /**
   * Same as MapCommonAttributesInto except that it does not handle hidden.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapCommonAttributesIntoExceptHidden(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aRuleData);

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
                                         nsRuleData* aData);

  /**
   * Helper to map the align attribute into a style struct for things
   * like <div>, <h1>, etc.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapDivAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                       nsRuleData* aData);

  /**
   * Helper to map the image border attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageBorderAttributeInto(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the image margin attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageMarginAttributeInto(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the image position attribute into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapImageSizeAttributesInto(const nsMappedAttributes* aAttributes,
                                         nsRuleData* aData);
  /**
   * Helper to map the background attribute
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundInto(const nsMappedAttributes* aAttributes,
                                nsRuleData* aData);
  /**
   * Helper to map the bgcolor attribute
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBGColorInto(const nsMappedAttributes* aAttributes,
                             nsRuleData* aData);
  /**
   * Helper to map the background attributes (currently background and bgcolor)
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapBackgroundAttributesInto(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData);
  /**
   * Helper to map the scrolling attribute on FRAME and IFRAME
   * into a style struct.
   *
   * @param aAttributes the list of attributes to map
   * @param aData the returned rule data [INOUT]
   * @see GetAttributeMappingFunction
   */
  static void MapScrollingAttributeInto(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData);
  /**
   * Get the presentation context for this content node.
   * @return the presentation context
   */
  enum PresContextFor
  {
    eForComposedDoc,
    eForUncomposedDoc
  };
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
  mozilla::dom::HTMLFormElement*
  FindAncestorForm(mozilla::dom::HTMLFormElement* aCurrentForm = nullptr);

  virtual void RecompileScriptEventListeners() override;

  /**
   * See if the document being tested has nav-quirks mode enabled.
   * @param doc the document
   */
  static bool InNavQuirksMode(nsIDocument* aDoc);

  /**
   * Locate an nsIEditor rooted at this content node, if there is one.
   */
  nsresult GetEditor(nsIEditor** aEditor);

  /**
   * Helper method for NS_IMPL_URI_ATTR macro.
   * Gets the absolute URI value of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If the attribute
   * isn't a relative URI the value of the attribute is returned as is. Only
   * works for attributes in null namespace.
   *
   * @param aAttr      name of attribute.
   * @param aBaseAttr  name of base attribute.
   * @param aResult    result value [out]
   */
  void GetURIAttr(nsIAtom* aAttr, nsIAtom* aBaseAttr, nsAString& aResult) const;

  /**
   * Gets the absolute URI values of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If a substring
   * isn't a relative URI, the substring is returned as is. Only works for
   * attributes in null namespace.
   */
  bool GetURIAttr(nsIAtom* aAttr, nsIAtom* aBaseAttr, nsIURI** aURI) const;

  /**
   * Returns the current disabled state of the element.
   */
  virtual bool IsDisabled() const {
    return false;
  }

  bool IsHidden() const
  {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::hidden);
  }

  virtual bool IsLabelable() const override;
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  static bool TouchEventsEnabled(JSContext* /* unused */, JSObject* /* unused */);

  static inline bool
  CanHaveName(nsIAtom* aTag)
  {
    return aTag == nsGkAtoms::img ||
           aTag == nsGkAtoms::form ||
           aTag == nsGkAtoms::applet ||
           aTag == nsGkAtoms::embed ||
           aTag == nsGkAtoms::object;
  }
  static inline bool
  ShouldExposeNameAsHTMLDocumentProperty(Element* aElement)
  {
    return aElement->IsHTMLElement() &&
           CanHaveName(aElement->NodeInfo()->NameAtom());
  }
  static inline bool
  ShouldExposeIdAsHTMLDocumentProperty(Element* aElement)
  {
    if (aElement->IsAnyOfHTMLElements(nsGkAtoms::applet,
                                      nsGkAtoms::embed,
                                      nsGkAtoms::object)) {
      return true;
    }

    // Per spec, <img> is exposed by id only if it also has a nonempty
    // name (which doesn't have to match the id or anything).
    // HasName() is true precisely when name is nonempty.
    return aElement->IsHTMLElement(nsGkAtoms::img) && aElement->HasName();
  }

  static bool
  IsScrollGrabAllowed(JSContext*, JSObject*);

protected:
  /**
   * Add/remove this element to the documents name cache
   */
  void AddToNameTable(nsIAtom* aName) {
    NS_ASSERTION(HasName(), "Node doesn't have name?");
    nsIDocument* doc = GetUncomposedDoc();
    if (doc && !IsInAnonymousSubtree()) {
      doc->AddToNameTable(this, aName);
    }
  }
  void RemoveFromNameTable() {
    if (HasName()) {
      nsIDocument* doc = GetUncomposedDoc();
      if (doc) {
        doc->RemoveFromNameTable(this, GetParsedAttr(nsGkAtoms::name)->
                                         GetAtomValue());
      }
    }
  }

  /**
   * Register or unregister an access key to this element based on the
   * accesskey attribute.
   */
  void RegAccessKey()
  {
    if (HasFlag(NODE_HAS_ACCESSKEY)) {
      RegUnRegAccessKey(true);
    }
  }

  void UnregAccessKey()
  {
    if (HasFlag(NODE_HAS_ACCESSKEY)) {
      RegUnRegAccessKey(false);
    }
  }

private:
  void RegUnRegAccessKey(bool aDoReg);

protected:
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;

  virtual mozilla::EventListenerManager*
    GetEventListenerManagerForAttr(nsIAtom* aAttrName,
                                   bool* aDefer) override;

  /**
   * Dispatch a simulated mouse click by keyboard to the given element.
   */
  nsresult DispatchSimulatedClick(nsGenericHTMLElement* aElement,
                                  bool aIsTrusted,
                                  nsPresContext* aPresContext);

  /**
   * Create a URI for the given aURISpec string.
   * Returns INVALID_STATE_ERR and nulls *aURI if aURISpec is empty
   * and the document's URI matches the element's base URI.
   */
  nsresult NewURIFromString(const nsAString& aURISpec, nsIURI** aURI);

  void GetHTMLAttr(nsIAtom* aName, nsAString& aResult) const
  {
    GetAttr(kNameSpaceID_None, aName, aResult);
  }
  void GetHTMLAttr(nsIAtom* aName, mozilla::dom::DOMString& aResult) const
  {
    GetAttr(kNameSpaceID_None, aName, aResult);
  }
  void GetHTMLEnumAttr(nsIAtom* aName, nsAString& aResult) const
  {
    GetEnumAttr(aName, nullptr, aResult);
  }
  void GetHTMLURIAttr(nsIAtom* aName, nsAString& aResult) const
  {
    GetURIAttr(aName, nullptr, aResult);
  }

  void SetHTMLAttr(nsIAtom* aName, const nsAString& aValue)
  {
    SetAttr(kNameSpaceID_None, aName, aValue, true);
  }
  void SetHTMLAttr(nsIAtom* aName, const nsAString& aValue, mozilla::ErrorResult& aError)
  {
    mozilla::dom::Element::SetAttr(aName, aValue, aError);
  }
  void UnsetHTMLAttr(nsIAtom* aName, mozilla::ErrorResult& aError)
  {
    mozilla::dom::Element::UnsetAttr(aName, aError);
  }
  void SetHTMLBoolAttr(nsIAtom* aName, bool aValue, mozilla::ErrorResult& aError)
  {
    if (aValue) {
      SetHTMLAttr(aName, EmptyString(), aError);
    } else {
      UnsetHTMLAttr(aName, aError);
    }
  }
  void SetHTMLIntAttr(nsIAtom* aName, int32_t aValue, mozilla::ErrorResult& aError)
  {
    nsAutoString value;
    value.AppendInt(aValue);

    SetHTMLAttr(aName, value, aError);
  }

  /**
   * Helper method for NS_IMPL_STRING_ATTR macro.
   * Sets the value of an attribute, returns specified default value if the
   * attribute isn't set. Only works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   * @param aResult  result value [out]
   */
  nsresult SetAttrHelper(nsIAtom* aAttr, const nsAString& aValue);

  /**
   * Helper method for NS_IMPL_INT_ATTR macro.
   * Gets the integer-value of an attribute, returns specified default value
   * if the attribute isn't set or isn't set to an integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   */
  int32_t GetIntAttr(nsIAtom* aAttr, int32_t aDefault) const;

  /**
   * Helper method for NS_IMPL_INT_ATTR macro.
   * Sets value of attribute to specified integer. Only works for attributes
   * in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Integer value of attribute.
   */
  nsresult SetIntAttr(nsIAtom* aAttr, int32_t aValue);

  /**
   * Helper method for NS_IMPL_UINT_ATTR macro.
   * Gets the unsigned integer-value of an attribute, returns specified default
   * value if the attribute isn't set or isn't set to an integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aDefault default-value to return if attribute isn't set.
   */
  uint32_t GetUnsignedIntAttr(nsIAtom* aAttr, uint32_t aDefault) const;

  /**
   * Helper method for NS_IMPL_UINT_ATTR macro.
   * Sets value of attribute to specified unsigned integer. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Integer value of attribute.
   * @param aDefault Default value (in case value is out of range).  If the spec
   *                 doesn't provide one, should be 1 if the value is limited to
   *                 nonzero values, and 0 otherwise.
   */
  void SetUnsignedIntAttr(nsIAtom* aName, uint32_t aValue, uint32_t aDefault,
                          mozilla::ErrorResult& aError)
  {
    nsAutoString value;
    if (aValue > INT32_MAX) {
      value.AppendInt(aDefault);
    } else {
      value.AppendInt(aValue);
    }

    SetHTMLAttr(aName, value, aError);
  }

  /**
   * Sets value of attribute to specified double. Only works for attributes
   * in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Double value of attribute.
   */
  void SetDoubleAttr(nsIAtom* aAttr, double aValue, mozilla::ErrorResult& aRv)
  {
    nsAutoString value;
    value.AppendFloat(aValue);

    SetHTMLAttr(aAttr, value, aRv);
  }

  /**
   * This method works like GetURIAttr, except that it supports multiple
   * URIs separated by whitespace (one or more U+0020 SPACE characters).
   *
   * Gets the absolute URI values of an attribute, by resolving any relative
   * URIs in the attribute against the baseuri of the element. If a substring
   * isn't a relative URI, the substring is returned as is. Only works for
   * attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aResult  result value [out]
   */
  nsresult GetURIListAttr(nsIAtom* aAttr, nsAString& aResult);

  /**
   * Locates the nsIEditor associated with this node.  In general this is
   * equivalent to GetEditorInternal(), but for designmode or contenteditable,
   * this may need to get an editor that's not actually on this element's
   * associated TextControlFrame.  This is used by the spellchecking routines
   * to get the editor affected by changing the spellcheck attribute on this
   * node.
   */
  virtual already_AddRefed<nsIEditor> GetAssociatedEditor();

  /**
   * Get the frame's offset information for offsetTop/Left/Width/Height.
   * Returns the parent the offset is relative to.
   * @note This method flushes pending notifications (Flush_Layout).
   * @param aRect the offset information [OUT]
   */
  mozilla::dom::Element* GetOffsetRect(mozilla::CSSIntRect& aRect);

  /**
   * Returns true if this is the current document's body element
   */
  bool IsCurrentBodyElement();

  /**
   * Ensures all editors associated with a subtree are synced, for purposes of
   * spellchecking.
   */
  static void SyncEditorsOnSubtree(nsIContent* content);

  enum ContentEditableTristate {
    eInherit = -1,
    eFalse = 0,
    eTrue = 1
  };

  /**
   * Returns eTrue if the element has a contentEditable attribute and its value
   * is "true" or an empty string. Returns eFalse if the element has a
   * contentEditable attribute and its value is "false". Otherwise returns
   * eInherit.
   */
  ContentEditableTristate GetContentEditableValue() const
  {
    static const nsIContent::AttrValuesArray values[] =
      { &nsGkAtoms::_false, &nsGkAtoms::_true, &nsGkAtoms::_empty, nullptr };

    if (!MayHaveContentEditableAttr())
      return eInherit;

    int32_t value = FindAttrValueIn(kNameSpaceID_None,
                                    nsGkAtoms::contenteditable, values,
                                    eIgnoreCase);

    return value > 0 ? eTrue : (value == 0 ? eFalse : eInherit);
  }

  // Used by A, AREA, LINK, and STYLE.
  already_AddRefed<nsIURI> GetHrefURIForAnchors() const;

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

  nsresult SetUndoScopeInternal(bool aUndoScope);

private:
  void ChangeEditableState(int32_t aChange);
};

namespace mozilla {
namespace dom {
class HTMLFieldSetElement;
} // namespace dom
} // namespace mozilla

#define FORM_ELEMENT_FLAG_BIT(n_) NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Form element specific bits
enum {
  // If this flag is set on an nsGenericHTMLFormElement or an HTMLImageElement,
  // that means that we have added ourselves to our mForm.  It's possible to
  // have a non-null mForm, but not have this flag set.  That happens when the
  // form is set via the content sink.
  ADDED_TO_FORM =                         FORM_ELEMENT_FLAG_BIT(0),

  // If this flag is set on an nsGenericHTMLFormElement or an HTMLImageElement,
  // that means that its form is in the process of being unbound from the tree,
  // and this form element hasn't re-found its form in
  // nsGenericHTMLFormElement::UnbindFromTree yet.
  MAYBE_ORPHAN_FORM_ELEMENT =             FORM_ELEMENT_FLAG_BIT(1)
};

// NOTE: I don't think it's possible to have the above two flags set at the
// same time, so if it becomes an issue we can probably merge them into the
// same bit.  --bz

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 2);

#undef FORM_ELEMENT_FLAG_BIT

/**
 * A helper class for form elements that can contain children
 */
class nsGenericHTMLFormElement : public nsGenericHTMLElement,
                                 public nsIFormControl
{
public:
  explicit nsGenericHTMLFormElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  NS_DECL_ISUPPORTS_INHERITED

  nsINode* GetScopeChainParent() const override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual void SaveSubtreeState() override;

  // nsIFormControl
  virtual mozilla::dom::HTMLFieldSetElement* GetFieldSet() override;
  virtual mozilla::dom::Element* GetFormElement() override;
  mozilla::dom::HTMLFormElement* GetForm() const
  {
    return mForm;
  }
  virtual void SetForm(nsIDOMHTMLFormElement* aForm) override;
  virtual void ClearForm(bool aRemoveFromForm) override;

  nsresult GetForm(nsIDOMHTMLFormElement** aForm);

  NS_IMETHOD SaveState() override
  {
    return NS_OK;
  }

  virtual bool RestoreState(nsPresState* aState) override
  {
    return false;
  }
  virtual bool AllowDrop() override
  {
    return true;
  }

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual IMEState GetDesiredIMEState() override;
  virtual mozilla::EventStates IntrinsicState() const override;

  virtual nsresult PreHandleEvent(
                     mozilla::EventChainPreVisitor& aVisitor) override;

  virtual bool IsDisabled() const override;

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

  void FieldSetFirstLegendChanged(bool aNotify) {
    UpdateFieldSet(aNotify);
  }

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

  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                 int32_t* aTabIndex) override;

  virtual bool IsLabelable() const override;

protected:
  virtual ~nsGenericHTMLFormElement();

  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 nsAttrValueOrString* aValue,
                                 bool aNotify) override;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;

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
   * This method is a a callback for IDTargetObserver (from nsIDocument).
   * It will be called each time the element associated with the id in @form
   * changes.
   */
  static bool FormIdUpdated(Element* aOldElement, Element* aNewElement,
                              void* aData);

  // Returns true if the event should not be handled from PreHandleEvent
  bool IsElementDisabledForEvents(mozilla::EventMessage aMessage,
                                  nsIFrame* aFrame);

  // The focusability state of this form control.  eUnfocusable means that it
  // shouldn't be focused at all, eInactiveWindow means it's in an inactive
  // window, eActiveWindow means it's in an active window.
  enum FocusTristate {
    eUnfocusable,
    eInactiveWindow,
    eActiveWindow
  };

  // Get our focus state.  If this returns eInactiveWindow, it will set this
  // element as the focused element for that window.
  FocusTristate FocusState();

  /** The form that contains this control */
  mozilla::dom::HTMLFormElement* mForm;

  /* This is a pointer to our closest fieldset parent if any */
  mozilla::dom::HTMLFieldSetElement* mFieldSet;
};

class nsGenericHTMLFormElementWithState : public nsGenericHTMLFormElement
{
public:
  explicit nsGenericHTMLFormElementWithState(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  /**
   * Get the presentation state for a piece of content, or create it if it does
   * not exist.  Generally used by SaveState().
   */
  nsPresState* GetPrimaryPresState();

  /**
   * Get the layout history object for a particular piece of content.
   *
   * @param aRead if true, won't return a layout history state if the
   *              layout history state is empty.
   * @return the history state object
   */
  already_AddRefed<nsILayoutHistoryState>
    GetLayoutHistory(bool aRead);

  /**
   * Restore the state for a form control.  Ends up calling
   * nsIFormControl::RestoreState().
   *
   * @return false if RestoreState() was not called, the return
   *         value of RestoreState() otherwise.
   */
  bool RestoreFormControlState();

  /**
   * Called when we have been cloned and adopted, and the information of the
   * node has been changed.
   */
  virtual void NodeInfoChanged() override;

protected:
  /* Generates the state key for saving the form state in the session if not
     computed already. The result is stored in mStateKey on success */
  nsresult GenerateStateKey();

  /* Used to store the key to that element in the session. Is void until
     GenerateStateKey has been used */
  nsCString mStateKey;
};

//----------------------------------------------------------------------

/**
 * This macro is similar to NS_IMPL_STRING_ATTR except that the getter method
 * falls back to an alternative method if the content attribute isn't set.
 */
#define NS_IMPL_STRING_ATTR_WITH_FALLBACK(_class, _method, _atom, _fallback) \
  NS_IMETHODIMP                                                              \
  _class::Get##_method(nsAString& aValue)                                    \
  {                                                                          \
    if (!GetAttr(kNameSpaceID_None, nsGkAtoms::_atom, aValue)) {             \
      _fallback(aValue);                                                     \
    }                                                                        \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP                                                              \
  _class::Set##_method(const nsAString& aValue)                              \
  {                                                                          \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                          \
  }

/**
 * A macro to implement the getter and setter for a given integer
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.
 */
#define NS_IMPL_INT_ATTR(_class, _method, _atom)                    \
  NS_IMPL_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, 0)

#define NS_IMPL_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default)  \
  NS_IMETHODIMP                                                           \
  _class::Get##_method(int32_t* aValue)                                   \
  {                                                                       \
    *aValue = GetIntAttr(nsGkAtoms::_atom, _default);                     \
    return NS_OK;                                                         \
  }                                                                       \
  NS_IMETHODIMP                                                           \
  _class::Set##_method(int32_t aValue)                                    \
  {                                                                       \
    return SetIntAttr(nsGkAtoms::_atom, aValue);                          \
  }

/**
 * A macro to implement the getter and setter for a given unsigned integer
 * valued content property. The method uses GetUnsignedIntAttr and
 * SetUnsignedIntAttr methods.
 */
#define NS_IMPL_UINT_ATTR(_class, _method, _atom)                         \
  NS_IMPL_UINT_ATTR_DEFAULT_VALUE(_class, _method, _atom, 0)

#define NS_IMPL_UINT_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                           \
  _class::Get##_method(uint32_t* aValue)                                  \
  {                                                                       \
    *aValue = GetUnsignedIntAttr(nsGkAtoms::_atom, _default);             \
    return NS_OK;                                                         \
  }                                                                       \
  NS_IMETHODIMP                                                           \
  _class::Set##_method(uint32_t aValue)                                   \
  {                                                                       \
    mozilla::ErrorResult rv;                                              \
    SetUnsignedIntAttr(nsGkAtoms::_atom, aValue, _default, rv);           \
    return rv.StealNSResult();                                            \
  }

/**
 * A macro to implement the getter and setter for a given unsigned integer
 * valued content property. The method uses GetUnsignedIntAttr and
 * SetUnsignedIntAttr methods. This macro is similar to NS_IMPL_UINT_ATTR except
 * that it throws an exception if the set value is null.
 */
#define NS_IMPL_UINT_ATTR_NON_ZERO(_class, _method, _atom)                \
  NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(_class, _method, _atom, 1)

#define NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                           \
  _class::Get##_method(uint32_t* aValue)                                  \
  {                                                                       \
    *aValue = GetUnsignedIntAttr(nsGkAtoms::_atom, _default);             \
    return NS_OK;                                                         \
  }                                                                       \
  NS_IMETHODIMP                                                           \
  _class::Set##_method(uint32_t aValue)                                   \
  {                                                                       \
    if (aValue == 0) {                                                    \
      return NS_ERROR_DOM_INDEX_SIZE_ERR;                                 \
    }                                                                     \
    mozilla::ErrorResult rv;                                              \
    SetUnsignedIntAttr(nsGkAtoms::_atom, aValue, _default, rv);           \
    return rv.StealNSResult();                                            \
  }

/**
 * A macro to implement the getter and setter for a given content
 * property that needs to return a URI in string form.  The method
 * uses the generic GetAttr and SetAttr methods.  This macro is much
 * like the NS_IMPL_STRING_ATTR macro, except we make sure the URI is
 * absolute.
 */
#define NS_IMPL_URI_ATTR(_class, _method, _atom)                    \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(nsAString& aValue)                           \
  {                                                                 \
    GetURIAttr(nsGkAtoms::_atom, nullptr, aValue);                  \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  _class::Set##_method(const nsAString& aValue)                     \
  {                                                                 \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);               \
  }

#define NS_IMPL_URI_ATTR_WITH_BASE(_class, _method, _atom, _base_atom)       \
  NS_IMETHODIMP                                                              \
  _class::Get##_method(nsAString& aValue)                                    \
  {                                                                          \
    GetURIAttr(nsGkAtoms::_atom, nsGkAtoms::_base_atom, aValue);             \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP                                                              \
  _class::Set##_method(const nsAString& aValue)                              \
  {                                                                          \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                        \
  }

/**
 * A macro to implement getter and setter for action and form action content
 * attributes. It's very similar to NS_IMPL_URI_ATTR excepted that if the
 * content attribute is the empty string, the empty string is returned.
 */
#define NS_IMPL_ACTION_ATTR(_class, _method, _atom)                 \
  NS_IMETHODIMP                                                     \
  _class::Get##_method(nsAString& aValue)                           \
  {                                                                 \
    GetAttr(kNameSpaceID_None, nsGkAtoms::_atom, aValue);           \
    if (!aValue.IsEmpty()) {                                        \
      GetURIAttr(nsGkAtoms::_atom, nullptr, aValue);                 \
    }                                                               \
    return NS_OK;                                                   \
  }                                                                 \
  NS_IMETHODIMP                                                     \
  _class::Set##_method(const nsAString& aValue)                     \
  {                                                                 \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                 \
  }

/**
 * A macro to implement the getter and setter for a given content
 * property that needs to set a non-negative integer. The method
 * uses the generic GetAttr and SetAttr methods. This macro is much
 * like the NS_IMPL_INT_ATTR macro except we throw an exception if
 * the set value is negative.
 */
#define NS_IMPL_NON_NEGATIVE_INT_ATTR(_class, _method, _atom)             \
  NS_IMPL_NON_NEGATIVE_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, -1)

#define NS_IMPL_NON_NEGATIVE_INT_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default)  \
  NS_IMETHODIMP                                                           \
  _class::Get##_method(int32_t* aValue)                                   \
  {                                                                       \
    *aValue = GetIntAttr(nsGkAtoms::_atom, _default);                     \
    return NS_OK;                                                         \
  }                                                                       \
  NS_IMETHODIMP                                                           \
  _class::Set##_method(int32_t aValue)                                    \
  {                                                                       \
    if (aValue < 0) {                                                     \
      return NS_ERROR_DOM_INDEX_SIZE_ERR;                                 \
    }                                                                     \
    return SetIntAttr(nsGkAtoms::_atom, aValue);                          \
  }

/**
 * A macro to implement the getter and setter for a given content
 * property that needs to set an enumerated string. The method
 * uses a specific GetEnumAttr and the generic SetAttrHelper methods.
 */
#define NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(_class, _method, _atom, _default) \
  NS_IMETHODIMP                                                           \
  _class::Get##_method(nsAString& aValue)                                 \
  {                                                                       \
    GetEnumAttr(nsGkAtoms::_atom, _default, aValue);                      \
    return NS_OK;                                                         \
  }                                                                       \
  NS_IMETHODIMP                                                           \
  _class::Set##_method(const nsAString& aValue)                           \
  {                                                                       \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                       \
  }

/**
 * A macro to implement the getter and setter for a given content
 * property that needs to set an enumerated string that has different
 * default values for missing and invalid values. The method uses a
 * specific GetEnumAttr and the generic SetAttrHelper methods.
 */
#define NS_IMPL_ENUM_ATTR_DEFAULT_MISSING_INVALID_VALUES(_class, _method, _atom, _defaultMissing, _defaultInvalid) \
  NS_IMETHODIMP                                                                                   \
  _class::Get##_method(nsAString& aValue)                                                         \
  {                                                                                               \
    GetEnumAttr(nsGkAtoms::_atom, _defaultMissing, _defaultInvalid, aValue);                      \
    return NS_OK;                                                                                 \
  }                                                                                               \
  NS_IMETHODIMP                                                                                   \
  _class::Set##_method(const nsAString& aValue)                                                   \
  {                                                                                               \
    return SetAttrHelper(nsGkAtoms::_atom, aValue);                                               \
  }

#define NS_INTERFACE_MAP_ENTRY_IF_TAG(_interface, _tag)                       \
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(_interface,                              \
                                     mNodeInfo->Equals(nsGkAtoms::_tag))


/**
 * A macro to declare the NS_NewHTMLXXXElement() functions.
 */
#define NS_DECLARE_NS_NEW_HTML_ELEMENT(_elementName)                       \
namespace mozilla {                                                        \
namespace dom {                                                            \
class HTML##_elementName##Element;                                         \
}                                                                          \
}                                                                          \
nsGenericHTMLElement*                                                      \
NS_NewHTML##_elementName##Element(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo, \
                                  mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

#define NS_DECLARE_NS_NEW_HTML_ELEMENT_AS_SHARED(_elementName)             \
inline nsGenericHTMLElement*                                               \
NS_NewHTML##_elementName##Element(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo, \
                                  mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER) \
{                                                                          \
  return NS_NewHTMLSharedElement(mozilla::Move(aNodeInfo), aFromParser);   \
}

/**
 * A macro to implement the NS_NewHTMLXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_HTML_ELEMENT(_elementName)                            \
nsGenericHTMLElement*                                                        \
NS_NewHTML##_elementName##Element(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo, \
                                  mozilla::dom::FromParser aFromParser)      \
{                                                                            \
  return new mozilla::dom::HTML##_elementName##Element(aNodeInfo);           \
}

#define NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(_elementName)               \
nsGenericHTMLElement*                                                        \
NS_NewHTML##_elementName##Element(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo, \
                                  mozilla::dom::FromParser aFromParser)      \
{                                                                            \
  return new mozilla::dom::HTML##_elementName##Element(aNodeInfo,            \
                                                       aFromParser);         \
}

// Here, we expand 'NS_DECLARE_NS_NEW_HTML_ELEMENT()' by hand.
// (Calling the macro directly (with no args) produces compiler warnings.)
nsGenericHTMLElement*
NS_NewHTMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                  mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

NS_DECLARE_NS_NEW_HTML_ELEMENT(Shared)
NS_DECLARE_NS_NEW_HTML_ELEMENT(SharedList)
NS_DECLARE_NS_NEW_HTML_ELEMENT(SharedObject)

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
NS_DECLARE_NS_NEW_HTML_ELEMENT(Div)
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
NS_DECLARE_NS_NEW_HTML_ELEMENT(Shadow)
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

inline nsISupports*
ToSupports(nsGenericHTMLElement* aHTMLElement)
{
  return static_cast<nsIContent*>(aHTMLElement);
}

inline nsISupports*
ToCanonicalSupports(nsGenericHTMLElement* aHTMLElement)
{
  return static_cast<nsIContent*>(aHTMLElement);
}

#endif /* nsGenericHTMLElement_h___ */
