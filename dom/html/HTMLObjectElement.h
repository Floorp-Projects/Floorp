/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLObjectElement_h
#define mozilla_dom_HTMLObjectElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class HTMLObjectElement final : public nsGenericHTMLFormElement
                              , public nsObjectLoadingContent
                              , public nsIDOMHTMLObjectElement
                              , public nsIConstraintValidation
{
public:
  explicit HTMLObjectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                             FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t TabIndexDefault() override;

#ifdef XP_MACOSX
  // nsIDOMEventTarget
  NS_IMETHOD PostHandleEvent(EventChainPostVisitor& aVisitor) override;
  // Helper methods
  static void OnFocusBlurPlugin(Element* aElement, bool aFocus);
  static void HandleFocusBlurPlugin(Element* aElement, WidgetEvent* aEvent);
  static void HandlePluginCrashed(Element* aElement);
  static void HandlePluginInstantiated(Element* aElement);
  // Weak pointer. Null if last action was blur.
  static Element* sLastFocused;
#endif

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) override;
  virtual IMEState GetDesiredIMEState() override;

  // Overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const override
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(nsFormSubmission *aFormSubmission) override;

  virtual bool IsDisabled() const override { return false; }

  virtual void DoneAddingChildren(bool aHaveNotified) override;
  virtual bool IsDoneAddingChildren() override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom *aAttribute,
                                const nsAString &aValue,
                                nsAttrValue &aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom *aAttribute) const override;
  virtual EventStates IntrinsicState() const override;
  virtual void DestroyContent() override;

  // nsObjectLoadingContent
  virtual uint32_t GetCapabilities() const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  nsresult CopyInnerTo(Element* aDest);

  void StartObjectLoad() { StartObjectLoad(true); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLObjectElement,
                                           nsGenericHTMLFormElement)

  // Web IDL binding methods
  // XPCOM GetData is ok; note that it's a URI attribute with a weird base URI
  void SetData(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::data, aValue, aRv);
  }
  void GetType(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::type, aValue);
  }
  void SetType(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::type, aValue, aRv);
  }
  bool TypeMustMatch()
  {
    return GetBoolAttr(nsGkAtoms::typemustmatch);
  }
  void SetTypeMustMatch(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::typemustmatch, aValue, aRv);
  }
  void GetName(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }
  void GetUseMap(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::usemap, aValue);
  }
  void SetUseMap(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::usemap, aValue, aRv);
  }
  using nsGenericHTMLFormElement::GetForm;
  void GetWidth(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::width, aValue);
  }
  void SetWidth(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::width, aValue, aRv);
  }
  void GetHeight(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::height, aValue);
  }
  void SetHeight(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::height, aValue, aRv);
  }
  using nsObjectLoadingContent::GetContentDocument;
  nsIDOMWindow* GetContentWindow();
  using nsIConstraintValidation::CheckValidity;
  using nsIConstraintValidation::GetValidationMessage;
  void GetAlign(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::align, aValue);
  }
  void SetAlign(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::align, aValue, aRv);
  }
  void GetArchive(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::archive, aValue);
  }
  void SetArchive(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::archive, aValue, aRv);
  }
  // XPCOM GetCode is ok; note that it's a URI attribute with a weird base URI
  void SetCode(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::code, aValue, aRv);
  }
  bool Declare()
  {
    return GetBoolAttr(nsGkAtoms::declare);
  }
  void SetDeclare(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::declare, aValue, aRv);
  }
  uint32_t Hspace()
  {
    return GetUnsignedIntAttr(nsGkAtoms::hspace, 0);
  }
  void SetHspace(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::hspace, aValue, aRv);
  }
  void GetStandby(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::standby, aValue);
  }
  void SetStandby(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::standby, aValue, aRv);
  }
  uint32_t Vspace()
  {
    return GetUnsignedIntAttr(nsGkAtoms::vspace, 0);
  }
  void SetVspace(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::vspace, aValue, aRv);
  }
  // XPCOM GetCodebase is ok; note that it's a URI attribute
  void SetCodeBase(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::codebase, aValue, aRv);
  }
  void GetCodeType(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::codetype, aValue);
  }
  void SetCodeType(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::codetype, aValue, aRv);
  }
  void GetBorder(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::border, aValue);
  }
  void SetBorder(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::border, aValue, aRv);
  }
  nsIDocument* GetSVGDocument()
  {
    return GetContentDocument();
  }

private:
  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  void StartObjectLoad(bool aNotify);

  /**
   * Returns if the element is currently focusable regardless of it's tabindex
   * value. This is used to know the default tabindex value.
   */
  bool IsFocusableForTabIndex();
  
  virtual void GetItemValueText(DOMString& text) override;
  virtual void SetItemValueText(const nsAString& text) override;

  virtual ~HTMLObjectElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);

  bool mIsDoneAddingChildren;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLObjectElement_h
