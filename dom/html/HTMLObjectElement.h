/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
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

class HTMLObjectElement MOZ_FINAL : public nsGenericHTMLFormElement
                                  , public nsObjectLoadingContent
                                  , public nsIDOMHTMLObjectElement
                                  , public nsIConstraintValidation
{
public:
  explicit HTMLObjectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                             FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

#ifdef XP_MACOSX
  // nsIDOMEventTarget
  NS_IMETHOD PostHandleEvent(EventChainPostVisitor& aVisitor) MOZ_OVERRIDE;
  static void HandleFocusBlurPlugin(Element* aElement, WidgetEvent* aEvent);
#endif

  // Element
  virtual bool IsInteractiveHTMLContent() const MOZ_OVERRIDE;

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) MOZ_OVERRIDE;
  virtual IMEState GetDesiredIMEState() MOZ_OVERRIDE;

  // Overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const MOZ_OVERRIDE
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset() MOZ_OVERRIDE;
  NS_IMETHOD SubmitNamesValues(nsFormSubmission *aFormSubmission) MOZ_OVERRIDE;

  virtual bool IsDisabled() const MOZ_OVERRIDE { return false; }

  virtual void DoneAddingChildren(bool aHaveNotified) MOZ_OVERRIDE;
  virtual bool IsDoneAddingChildren() MOZ_OVERRIDE;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom *aAttribute,
                                const nsAString &aValue,
                                nsAttrValue &aResult) MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom *aAttribute) const MOZ_OVERRIDE;
  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;
  virtual void DestroyContent() MOZ_OVERRIDE;

  // nsObjectLoadingContent
  virtual uint32_t GetCapabilities() const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

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
  
  virtual void GetItemValueText(DOMString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;

  virtual ~HTMLObjectElement();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);

  bool mIsDoneAddingChildren;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLObjectElement_h
